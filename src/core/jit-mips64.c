/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: nil; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * JIT (mips64): Just-In-Time native code generation
 */

#include <noct/noct.h>

#if defined(NOCT_ARCH_MIPS64) && defined(NOCT_USE_JIT)

#include "runtime.h"
#include "jit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* False asseretion */
#define JIT_OP_NOT_IMPLEMENTED  0
#define NEVER_COME_HERE         0

/* Branch patch type */
#define PATCH_BAL               0
#define PATCH_BEQ               1
#define PATCH_BNE               2

/* Generated code. */
static uint32_t *jit_code_region;
static uint32_t *jit_code_region_cur;
static uint32_t *jit_code_region_tail;

/* Write mapped? */
static bool is_writable;

/* Forward declaration */
static bool jit_visit_bytecode(struct jit_context *ctx);
static bool jit_patch_branch(struct jit_context *ctx, int patch_index);

/*
 * Generate a JIT-compiled code for a function.
 */
bool
jit_build(
          struct rt_env *env,
          struct rt_func *func)
{
        struct jit_context ctx;
        int i;

        /* If the first call, map a memory region for the generated code. */
        if (jit_code_region == NULL) {
                if (!jit_map_memory_region((void **)&jit_code_region, jit_get_code_size(env))) {
                        rt_error(env, "Memory mapping failed.");
                        return false;
                }
                jit_code_region_cur = jit_code_region;
                jit_code_region_tail = jit_code_region + jit_get_code_size(env) / 4;
                is_writable = true;
        }

        /* Make a context. */
        memset(&ctx, 0, sizeof(struct jit_context));
        ctx.code_top = jit_code_region_cur;
        ctx.code_end = jit_code_region_tail;
        ctx.code = ctx.code_top;
        ctx.env = env;
        ctx.func = func;
	/* This backend has no vector ISA tier in the current design. */
	jit_configure_simd(&ctx, 0, "mips64");

        /* Make code writable and non-executable. */
        if (!is_writable) {
                jit_map_writable(jit_code_region, jit_get_code_size(env));
                is_writable = true;
        }

        /* Visit over the bytecode. */
        if (!jit_visit_bytecode(&ctx))
                return false;

        jit_code_region_cur = ctx.code;

        /* Patch branches. */
        for (i = 0; i < ctx.branch_patch_count; i++) {
                if (!jit_patch_branch(&ctx, i))
                        return false;
        }

        func->jit_code = (bool (*)(struct rt_env *))ctx.code_top;

        return true;
}

/*
 * Free all JIT-compiled code.
 */
void
jit_free(
         struct rt_env *env)
{
        UNUSED_PARAMETER(env);

        if (jit_code_region != NULL) {
                jit_unmap_memory_region(jit_code_region, jit_get_code_size(env));

                jit_code_region = NULL;
                jit_code_region_cur = NULL;
                jit_code_region_tail = NULL;
        }
}

/*
 * Commit written code.
 */
void
jit_commit(
        struct rt_env *env)
{
        /* Make code executable and non-writable. */
        jit_map_executable(jit_code_region, jit_get_code_size(env));

        is_writable = false;
}

/*
 * Assembler output functions
 */

/* Decoration */
#define ASM

/* Registers */
#define REG_ZERO        0
#define REG_AT          1
#define REG_V0          2
#define REG_V1          3
#define REG_A0          4
#define REG_A1          5
#define REG_A2          6
#define REG_A3          7
#define REG_T0          8
#define REG_T1          9
#define REG_T2          10
#define REG_T3          11
#define REG_T4          12
#define REG_T5          13
#define REG_T6          14
#define REG_T7          15
#define REG_S0          16
#define REG_S1          17
#define REG_S2          18
#define REG_S3          19
#define REG_S4          20
#define REG_S5          21
#define REG_S6          22
#define REG_S7          23
#define REG_T8          24
#define REG_T9          25
#define REG_K0          26
#define REG_K1          27
#define REG_GP          28
#define REG_SP          29
#define REG_FP          30
#define REG_RA          31

/* Put a instruction word. */
#define IW(w)                           if (!jit_put_word(ctx, w)) return false
static INLINE bool
jit_put_word(
        struct jit_context *ctx,
        uint32_t word)
{
        if (ctx->code >= ctx->code_end) {
                rt_error(ctx->env, "Code too big.");
                return false;
        }

        *(uint32_t *)ctx->code = word;
        ctx->code = (uint32_t *)ctx->code + 1;

        return true;
}

/*
 * Templates
 */

static INLINE uint32_t hihi16(uint64_t d)
{
        return (uint32_t)((d >> 48) & 0xffff);
}

static INLINE uint32_t hilo16(uint64_t d)
{
        return (uint32_t)((d >> 32) & 0xffff);
}

static INLINE uint32_t lohi16(uint64_t d)
{
        return (uint32_t)((d >> 16) & 0xffff);
}

static INLINE uint32_t lolo16(uint64_t d)
{
        return (uint32_t)(d & 0xffff);
}

static INLINE uint32_t hi16(uint32_t d)
{
        return (d >> 16) & 0xffff;
}

static INLINE uint32_t lo16(uint32_t d)
{
        return d & 0xffff;
}

static INLINE uint32_t tvar16(int d)
{
        return (uint32_t)d & 0xffff;
}

/* Absolute 64-bit jump through $t9 (eight slots with delay slot). */
static INLINE bool
jit_put_abs_jump(
        struct jit_context *ctx,
        uint64_t target)
{
        if (!jit_put_word(ctx, 0x3c190000 | hihi16(target))) return false;
        if (!jit_put_word(ctx, 0x37390000 | hilo16(target))) return false;
        if (!jit_put_word(ctx, 0x0019cc38)) return false;
        if (!jit_put_word(ctx, 0x37390000 | lohi16(target))) return false;
        if (!jit_put_word(ctx, 0x0019cc38)) return false;
        if (!jit_put_word(ctx, 0x37390000 | lolo16(target))) return false;
        if (!jit_put_word(ctx, 0x03200008)) return false;
        if (!jit_put_word(ctx, 0x00000000)) return false;
        return true;
}

#define EXCEPTION_IF_EQUAL(rs, rt) do {                                 \
        if (!jit_put_word(ctx, 0x14000009 |                              \
                          ((uint32_t)(rs) << 21) |                       \
                          ((uint32_t)(rt) << 16))) return false;         \
        if (!jit_put_word(ctx, 0)) return false;                         \
        if (!jit_put_abs_jump(ctx,                                      \
                (uint64_t)(uintptr_t)ctx->exception_code)) return false; \
} while (0)

#define EXCEPTION_IF_ZERO(rs) EXCEPTION_IF_EQUAL((rs), REG_ZERO)

#define ASM_BINARY_OP(f)                                                                                                \
        ASM {                                                                                                           \
                /* $s0: env */                                                                                           \
                /* $s1: &env->frame->tmpvar[0] */                                                                        \
                                                                                                                        \
                /* Arg1 $a0 = env */                                                                                     \
                /* move $a0, $s0 */             IW(0x02002025);                                                         \
                                                                                                                        \
                /* Arg2 $a1 = dst */                                                                                    \
                /* li $a1, dst */               IW(0x24050000 | lo16((uint32_t)dst));                                   \
                                                                                                                        \
                /* Arg3 $a2 = src1 */                                                                                   \
                /* li $a2, src1 */              IW(0x24060000 | tvar16(src1));                                          \
                                                                                                                        \
                /* Arg4 $a3: src2 */                                                                                    \
                /* li $a3, src2 */              IW(0x24070000 | tvar16(src2));                                          \
                                                                                                                        \
                /* Call f(). */                                                                                         \
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16((uint64_t)f));                                   \
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16((uint64_t)f));                                   \
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);                                                         \
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16((uint64_t)f));                                   \
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);                                                         \
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16((uint64_t)f));                                   \
                /* move $s2, $ra */             IW(0x03e09025);                                                         \
                /* jalr $t9 */                  IW(0x0320f809);                                                         \
                /* nop */                       IW(0x00000000);                                                         \
                /* move $ra, $s2 */             IW(0x0240f825);                                                         \
                                                                                                                        \
                /* If failed: */                                                                                        \
                EXCEPTION_IF_ZERO(REG_V0);                                                                               \
        }

#define ASM_UNARY_OP(f)                                                                                                 \
        ASM {                                                                                                           \
                /* $s0: env */                                                                                           \
                /* $s1: &env->frame->tmpvar[0] */                                                                        \
                                                                                                                        \
                /* Arg1 $a0 = env */                                                                                     \
                /* move $a0, $s0 */             IW(0x02002025);                                                         \
                                                                                                                        \
                /* Arg2 $a1 = dst */                                                                                    \
                /* li $a1, dst */               IW(0x24050000 | lo16((uint32_t)dst));                                   \
                                                                                                                        \
                /* Arg3 $a2 = src */                                                                                    \
                /* li $a2, src */               IW(0x24060000 | tvar16(src));                                           \
                                                                                                                        \
                /* Call f(). */                                                                                         \
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16((uint64_t)f));                                   \
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16((uint64_t)f));                                   \
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);                                                         \
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16((uint64_t)f));                                   \
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);                                                         \
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16((uint64_t)f));                                   \
                /* move $s2, $ra */             IW(0x03e09025);                                                         \
                /* jalr $t9 */                  IW(0x0320f809);                                                         \
                /* nop */                       IW(0x00000000);                                                         \
                /* move $ra, $s2 */             IW(0x0240f825);                                                         \
                                                                                                                        \
                /* If failed: */                                                                                        \
                EXCEPTION_IF_ZERO(REG_V0);                                                                               \
        }

/*
 * Bytecode visitors
 */

/* Visit a OP_LINEINFO instruction. */
static INLINE bool
jit_visit_lineinfo_op(
        struct jit_context *ctx)
{
        uint32_t line;

        CONSUME_IMM32(line);

        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* env->line = line; */
                /* li $t0, line */      IW(0x24080000 | lo16(line));
                /* sw $t0, 8($s0) */    IW(0xae080008);
        }

        return true;
}

/* Visit a OP_ASSIGN instruction. */
static INLINE bool
jit_visit_assign_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        dst *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);

        /* env->frame->tmpvar[dst] = env->frame->tmpvar[src]; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* $t1 = src_addr = &env->frame->tmpvar[src] */
                /* li   $t1, src */             IW(0x240d0000 | lo16((uint32_t)src));
                /* daddu $t1, $t1, $s1 */       IW(0x01b1682d);

                /* *dst_addr = *src_addr */
                /* ld $t2, 0($t1) */            IW(0xddae0000);
                /* ld $t3, 8($t1) */            IW(0xddaf0008);
                /* sd $t2, 0($t0) */            IW(0xfd8e0000);
                /* sd $t3, 8($t0) */            IW(0xfd8f0008);
        }

        return true;
}

/* Visit a OP_ICONST instruction. */
static INLINE bool
jit_visit_iconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint32_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM32(val);

        dst *= (int)sizeof(struct rt_value);

        /* Set an integer constant. */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_INT */
                /* li $t1, 0 */                 IW(0x240d0000);
                /* sw $t1, 0($t0) */            IW(0xad8d0000);

                /* env->frame->tmpvar[dst].val.i = val */
                /* lui $t1, val@h */            IW(0x3c0d0000 | hi16(val));
                /* ori $t1, $t1, val@l */       IW(0x35ad0000 | lo16(val));
                /* sw  $t1, 8($t0) */           IW(0xad8d0008);
        }

        return true;
}

/* Visit a OP_LICONST instruction. */
static INLINE bool
jit_visit_liconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint64_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM64(val);

        dst *= (int)sizeof(struct rt_value);

        /* Set an integer constant. */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_LONG */
                /* li $t1, 5 */                 IW(0x240d0005);
                /* sw $t1, 0($t0) */            IW(0xad8d0000);

                /* env->frame->tmpvar[dst].val.i = val */
                /* lui  $t1, f@hh */            IW(0x3c0d0000 | hihi16(val));
                /* ori  $t1, f@hl */            IW(0x35ad0000 | hilo16(val));
                /* dsll $t1, $t1, 16 */         IW(0x000d6c38);
                /* ori  $t1, f@lh */            IW(0x35ad0000 | lohi16(val));
                /* dsll $t1, $t1, 16 */         IW(0x000d6c38);
                /* ori  $t1, f@ll */            IW(0x35ad0000 | lolo16(val));
                /* sd  $t1, 8($t0) */           IW(0xfd8d0008);
        }

        return true;
}

/* Visit a OP_FCONST instruction. */
static INLINE bool
jit_visit_fconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint32_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM32(val);

        dst *= (int)sizeof(struct rt_value);

        /* Set a floating-point constant. */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_FLOAT */
                /* li $t1, 1 */                 IW(0x240d0001);
                /* sw $t1, 0($t0) */            IW(0xad8d0000);

                /* env->frame->tmpvar[dst].val.i = val */
                /* lui $t1, val@h */            IW(0x3c0d0000 | hi16(val));
                /* ori $t1, $t1, val@l */       IW(0x35ad0000 | lo16(val));
                /* sw  $t1, 8($t0) */           IW(0xad8d0008);
        }

        return true;
}

/* Visit a OP_LFCONST instruction. */
static INLINE bool
jit_visit_lfconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint64_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM64(val);

        dst *= (int)sizeof(struct rt_value);

        /* Set an integer constant. */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_DOUBLE */
                /* li $t1, 6 */                 IW(0x240d0006);
                /* sw $t1, 0($t0) */            IW(0xad8d0000);

                /* env->frame->tmpvar[dst].val.i = val */
                /* lui  $t1, f@hh */            IW(0x3c0d0000 | hihi16(val));
                /* ori  $t1, f@hl */            IW(0x35ad0000 | hilo16(val));
                /* dsll $t1, $t1, 16 */         IW(0x000d6c38);
                /* ori  $t1, f@lh */            IW(0x35ad0000 | lohi16(val));
                /* dsll $t1, $t1, 16 */         IW(0x000d6c38);
                /* ori  $t1, f@ll */            IW(0x35ad0000 | lolo16(val));
                /* sd  $t1, 8($t0) */           IW(0xfd8d0008);
        }

        return true;
}

/* Visit a OP_SCONST instruction. */
static INLINE bool
jit_visit_sconst_op(
        struct jit_context *ctx)
{
        int dst;
        const char *val;
        uint32_t len, hash;
        uint64_t f;

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(val, len, hash);

        f = (uint64_t)ex_make_string_with_hash;
        dst *= (int)sizeof(struct rt_value);

        /* ex_make_string(env, &env->frame->tmpvar[dst], val, len, hash); */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $a1, dst */            IW(0x24050000 | tvar16(dst));
                /* daddu $a1, $a1, $s1 */       IW(0x00b1282d);

                /* Arg3 $a2 = val */
                /* lui  $a2, val@hh */          IW(0x3c060000 | hihi16((uint64_t)val));
                /* ori  $a2, val@hl */          IW(0x34c60000 | hilo16((uint64_t)val));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, val@lh */          IW(0x34c60000 | lohi16((uint64_t)val));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, val@ll */          IW(0x34c60000 | lolo16((uint64_t)val));

                /* Arg4 $a3 = len */
                /* lui  $a3, len@h */           IW(0x3c070000 | hi16(len));
                /* ori  $a3, len@l */           IW(0x34e70000 | lo16(len));

                /* Arg5 $a4 = hash */
                /* lui  $a4, hash@h */          IW(0x3c080000 | hi16(hash));
                /* ori  $a4, hash@l */          IW(0x35080000 | lo16(hash));

                /* Call ex_make_string_with_hash(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_ACONST instruction. */
static INLINE bool
jit_visit_aconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint64_t f;

        CONSUME_TMPVAR(dst);

        f = (uint64_t)ex_make_empty_array;
        dst *= (int)sizeof(struct rt_value);

        /* ex_make_empty_array(env, &env->frame->tmpvar[dst]); */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $a1, dst */            IW(0x24050000 | lo16((uint32_t)dst));
                /* daddu $a1, $a1, $s1 */       IW(0x00b1282d);

                /* Call ex_make_empty_array(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_DCONST instruction. */
static INLINE bool
jit_visit_dconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint64_t f;

        CONSUME_TMPVAR(dst);

        f = (uint64_t)ex_make_empty_dict;
        dst *= (int)sizeof(struct rt_value);

        /* ex_make_empty_dict(env, &env->frame->tmpvar[dst]); */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $a1, dst */            IW(0x24050000 | lo16((uint32_t)dst));
                /* daddu $a1, $a1, $s1 */       IW(0x00b1282d);

                /* Call ex_make_empty_dict(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_INC instruction. */
static INLINE bool
jit_visit_inc_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

        /* Increment an integer. */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = dst_addr = &env->frame->tmpvar[dst] */
                /* li    $t0, dst */            IW(0x240c0000 | lo16((uint32_t)dst));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);

                /* env->frame->tmpvar[dst].val.i++ */
                /* lw    $t1, 8($t0) */         IW(0x8d8d0008);
                /* addiu $t1, $t1, 1 */         IW(0x25ad0001);
                /* sw    $t1, 8($t0) */         IW(0xad8d0008);
        }

        return true;
}

/* Visit a OP_ADD instruction. */
static INLINE bool
jit_visit_add_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_add_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_add_helper);

        return true;
}

/* Visit a OP_SUB instruction. */
static INLINE bool
jit_visit_sub_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_sub_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_sub_helper);

        return true;
}

/* Visit a OP_MUL instruction. */
static INLINE bool
jit_visit_mul_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_mul_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_mul_helper);

        return true;
}

/* Visit a OP_DIV instruction. */
static INLINE bool
jit_visit_div_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_div_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_div_helper);

        return true;
}

/* Visit a OP_MOD instruction. */
static INLINE bool
jit_visit_mod_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_mod_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_mod_helper);

        return true;
}

/* Visit a OP_AND instruction. */
static INLINE bool
jit_visit_and_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_and_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_and_helper);

        return true;
}

/* Visit a OP_OR instruction. */
static INLINE bool
jit_visit_or_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_or_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_or_helper);

        return true;
}

/* Visit a OP_XOR instruction. */
static INLINE bool
jit_visit_xor_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_xor_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_xor_helper);

        return true;
}

/* Visit a OP_SHL instruction. */
static INLINE bool
jit_visit_shl_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_shl_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_shl_helper);

        return true;
}

/* Visit a OP_SHR instruction. */
static INLINE bool
jit_visit_shr_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_shr_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_shr_helper);

        return true;
}

/* Visit a OP_NEG instruction. */
static INLINE bool
jit_visit_neg_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!ex_neg_helper(env, dst, src)) return false; */
        ASM_UNARY_OP(ex_neg_helper);

        return true;
}

/* Visit a OP_NOT instruction. */
static INLINE bool
jit_visit_not_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!ex_not_helper(env, dst, src)) return false; */
        ASM_UNARY_OP(ex_not_helper);

        return true;
}

/* Visit a OP_LT instruction. */
static INLINE bool
jit_visit_lt_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_lt_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_lt_helper);

        return true;
}

/* Visit a OP_LTE instruction. */
static INLINE bool
jit_visit_lte_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_lte_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_lte_helper);

        return true;
}

/* Visit a OP_EQ instruction. */
static INLINE bool
jit_visit_eq_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_eq_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_eq_helper);

        return true;
}

/* Visit a OP_NEQ instruction. */
static INLINE bool
jit_visit_neq_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_neq_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_neq_helper);

        return true;
}

/* Visit a OP_GTE instruction. */
static INLINE bool
jit_visit_gte_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_gte_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_gte_helper);

        return true;
}

/* Visit a OP_GT instruction. */
static INLINE bool
jit_visit_gt_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_gt_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_gt_helper);

        return true;
}

/* Visit a OP_EQI instruction. */
static INLINE bool
jit_visit_eqi_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        dst *= (int)sizeof(struct rt_value);
        src1 *= (int)sizeof(struct rt_value);
        src2 *= (int)sizeof(struct rt_value);

        /* src1 == src2 */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* $t0 = env->frame->tmpvar[src1].val.i */
                /* li    $t0, src1 */           IW(0x240c0000 | lo16((uint32_t)src1));
                /* daddu $t0, $t0, $s1 */       IW(0x0191602d);
                /* lw    $t0, 8($t0) */         IW(0x8d8c0008);

                /* $t1 = env->frame->tmpvar[src2].val.i */
                /* li    $t1, src2 */           IW(0x240d0000 | lo16((uint32_t)src2));
                /* daddu $t1, $t1, $s1 */       IW(0x01b1682d);
                /* lw    $t1, 8($t1) */         IW(0x8dad0008);

                /* src1 == src2 */
                /* dsubu $at, $t0, $t1 */       IW(0x018d082f);
        }

        return true;
}

/* Visit a OP_LOADARRAY instruction. */
static INLINE bool
jit_visit_loadarray_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_loadarray_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_loadarray_helper);

        return true;
}

/* Visit a OP_STOREARRAY instruction. */
static INLINE bool
jit_visit_storearray_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_storearray_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_storearray_helper);

        return true;
}

/* Visit a OP_LEN instruction. */
static INLINE bool
jit_visit_len_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!jit_len_helper(env, dst, src)) return false; */
        ASM_UNARY_OP(ex_len_helper);

        return true;
}

/* Visit a OP_GETDICTKEYBYINDEX instruction. */
static INLINE bool
jit_visit_getdictkeybyindex_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_getdictkeybyindex_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_getdictkeybyindex_helper);

        return true;
}

/* Visit a OP_GETDICTVALBYINDEX instruction. */
static INLINE bool
jit_visit_getdictvalbyindex_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_getdictvalbyindex_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_getdictvalbyindex_helper);

        return true;
}

/* Visit a OP_LOADSYMBOL instruction. */
static INLINE bool
jit_visit_loadsymbol_op(
        struct jit_context *ctx)
{
        int dst;
        const char *src_s;
        uint32_t len, hash;
        uint64_t src;
        uint64_t f;

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(src_s, len, hash);

        src = (uint64_t)(intptr_t)src_s;
        f = (uint64_t)ex_loadsymbol_helper;

        /* if (!jit_loadsymbol_helper(env, dst, src, len, hash)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst */
                /* li $a1, dst */               IW(0x24050000 | tvar16(dst));

                /* Arg3 $a2 = src */
                /* lui  $a2, src@hh */          IW(0x3c060000 | hihi16(src));
                /* ori  $a2, src@hl */          IW(0x34c60000 | hilo16(src));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, src@lh */          IW(0x34c60000 | lohi16(src));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, src@ll */          IW(0x34c60000 | lolo16(src));

                /* Arg4 $a3 = len */
                /* lui  $a3, len@h */           IW(0x3c070000 | hi16(len));
                /* ori  $a3, len@l */           IW(0x34e70000 | lo16(len));

                /* Arg5 $a4 = hash */
                /* lui  $a4, hash@h */          IW(0x3c080000 | hi16(hash));
                /* ori  $a4, hash@l */          IW(0x35080000 | lo16(hash));

                /* Call ex_loadsymbol_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_STORESYMBOL instruction. */
static INLINE bool
jit_visit_storesymbol_op(
        struct jit_context *ctx)
{
        const char *dst_s;
        uint64_t dst;
        uint32_t len, hash;
        int src;
        uint64_t f;

        CONSUME_STRING(dst_s, len, hash);
        CONSUME_TMPVAR(src);

        dst = (uint64_t)(intptr_t)dst_s;
        f = (uint64_t)ex_storesymbol_helper;

        /* if (!ex_storesymbol_helper(env, dst, len, hash, src)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst */
                /* lui  $a1, dst@hh */          IW(0x3c050000 | hihi16(dst));
                /* ori  $a1, dst@hl */          IW(0x34a50000 | hilo16(dst));
                /* dsll $a1, $a1, 16 */         IW(0x00052c38);
                /* ori  $a1, dst@lh */          IW(0x34a50000 | lohi16(dst));
                /* dsll $a1, $a1, 16 */         IW(0x00052c38);
                /* ori  $a1, dst@ll */          IW(0x34a50000 | lolo16(dst));

                /* Arg3 $a2 = len */
                /* lui  $a2, len@h */           IW(0x3c060000 | hi16(len));
                /* ori  $a2, len@l */           IW(0x34c60000 | lo16(len));

                /* Arg4 $a3 = hash */
                /* lui  $a3, hash@h */          IW(0x3c070000 | hi16(hash));
                /* ori  $a3, hash@l */          IW(0x34e70000 | lo16(hash));

                /* Arg5 $a4 = src */
                /* li   $a4, src */             IW(0x24080000 | tvar16(src));

                /* Call ex_storesymbol_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_LOADDOT instruction. */
static INLINE bool
jit_visit_loaddot_op(
        struct jit_context *ctx)
{
        int dst;
        int dict;
        const char *field_s;
        uint32_t len, hash;
        uint64_t field;
        uint64_t f;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s, len, hash);

        field = (uint64_t)(intptr_t)field_s;
        f = (uint64_t)ex_loaddot_helper;

        /* if (!ex_loaddot_helper(env, dst, dict, field, len, hash)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst */
                /* li $a1, dst */               IW(0x24050000 | tvar16(dst));

                /* Arg3 $a2 = dict */
                /* li $a2, dict */              IW(0x24060000 | tvar16(dict));

                /* Arg4 $a3 = field */
                /* lui  $a3, field@hh */        IW(0x3c070000 | hihi16(field));
                /* ori  $a3, field@hl */        IW(0x34e70000 | hilo16(field));
                /* dsll $a3, $a3, 16 */         IW(0x00073c38);
                /* ori  $a3, field@lh */        IW(0x34e70000 | lohi16(field));
                /* dsll $a3, $a3, 16 */         IW(0x00073c38);
                /* ori  $a3, field@ll */        IW(0x34e70000 | lolo16(field));

                /* Arg5 $a4 = len */
                /* lui  $a4, len@h */           IW(0x3c080000 | hi16(len));
                /* ori  $a4, $a4, len@l */      IW(0x35080000 | lo16(len));

                /* Arg6 $a5 = hash */
                /* lui  $a5, hash@h */          IW(0x3c090000 | hi16(hash));
                /* ori  $a5, $a5, hash@l */     IW(0x35290000 | lo16(hash));

                /* Call ex_loaddot_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_STOREDOT instruction. */
static INLINE bool
jit_visit_storedot_op(
        struct jit_context *ctx)
{
        int dict;
        const char *field_s;
        uint32_t len, hash;
        uint64_t field;
        int src;
        uint64_t f;

        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s, len, hash);
        CONSUME_TMPVAR(src);

        field = (uint64_t)(intptr_t)field_s;
        f = (uint64_t)ex_storedot_helper;

        /* if (!jit_storedot_helper(env, dict, field, len, hash, src)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dict */
                /* li   $a1, dict */            IW(0x24050000 | tvar16(dict));

                /* Arg3 $a2 = field */
                /* lui  $a2, field@hh */        IW(0x3c060000 | hihi16(field));
                /* ori  $a2, field@hl */        IW(0x34c60000 | hilo16(field));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, field@lh */        IW(0x34c60000 | lohi16(field));
                /* dsll $a2, $a2, 16 */         IW(0x00063438);
                /* ori  $a2, field@ll */        IW(0x34c60000 | lolo16(field));

                /* Arg4 $a3 = len */
                /* lui  $a3, len@h */           IW(0x3c070000 | hi16(len));
                /* ori  $a3, len@l */           IW(0x34e70000 | lo16(len));

                /* Arg5 $a4 = hash */
                /* lui  $a4, hash@h */          IW(0x3c080000 | hi16(hash));
                /* ori  $a4, hash@l */          IW(0x35080000 | lo16(hash));

                /* Arg6 $a5 = src */
                /* li   $a5, src */             IW(0x24090000 | tvar16(src));

                /* Call ex_storedot_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_CALL instruction. */
static inline bool
jit_visit_call_op(
        struct jit_context *ctx)
{
        int dst;
        int func;
        int arg_count;
        int arg_tmp;
        int arg[NOCT_ARG_MAX];
        uint32_t tmp;
        uint64_t arg_addr;
        int i;
        uint64_t f;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(func);
        CONSUME_IMM8(arg_count);
        for (i = 0; i < arg_count; i++) {
                CONSUME_TMPVAR(arg_tmp);
                arg[i] = arg_tmp;
        }

        if (arg_count > 0) {
                /* Embed arguments to the code. */
                tmp = (uint32_t)((8 + 4 * arg_count - 4) / 4);
                ASM {
                        /* b */         IW(0x10000000 | tmp);
                        /* nop */       IW(0x00000000);
                }
                arg_addr = (uint64_t)(intptr_t)ctx->code;
                for (i = 0; i < arg_count; i++) {
                        *(uint32_t *)ctx->code = (uint32_t)arg[i];
                        ctx->code = (uint32_t *)ctx->code + 1;
                }
        } else {
                arg_addr = 0;
        }

        f = (uint64_t)ex_call_helper;

        /* if (!ex_call_helper(env, dst, func, arg_count, arg)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst */
                /* li $a1, dst */               IW(0x24050000 | tvar16(dst));

                /* Arg3 $a2 = func */
                /* li $a2, func */              IW(0x24060000 | tvar16(func));

                /* Arg4 $a3 = arg_count */
                /* li $a3, arg_count */         IW(0x24070000 | lo16((uint32_t)arg_count));

                /* Arg5 $a4 = arg */
                /* lui  $a4, arg@hh */          IW(0x3c080000 | hihi16(arg_addr));
                /* ori  $a4, arg@hl */          IW(0x35080000 | hilo16(arg_addr));
                /* dsll $a4, $a4, 16 */         IW(0x00084438);
                /* ori  $a4, arg@lh */          IW(0x35080000 | lohi16(arg_addr));
                /* dsll $a4, $a4, 16 */         IW(0x00084438);
                /* ori  $a4, arg@ll */          IW(0x35080000 | lolo16(arg_addr));

                /* Call ex_call_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_THISCALL instruction. */
static inline bool
jit_visit_thiscall_op(
        struct jit_context *ctx)
{
        int dst;
        int obj;
        const char *symbol;
        uint32_t len, hash;
        int arg_count;
        int arg_tmp;
        int arg[NOCT_ARG_MAX];
        uint32_t tmp;
        uint64_t arg_addr;
        int i;
        uint64_t f;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(obj);
        CONSUME_STRING(symbol, len, hash);
        CONSUME_IMM8(arg_count);
        for (i = 0; i < arg_count; i++) {
                CONSUME_TMPVAR(arg_tmp);
                arg[i] = arg_tmp;
        }

        if (arg_count > 0) {
                /* Embed arguments to the code. */
                tmp = (uint32_t)((8 + 4 * arg_count - 4) / 4);
                ASM {
                        /* b */         IW(0x10000000 | tmp);
                        /* nop */       IW(0x00000000);
                }
                arg_addr = (uint64_t)(intptr_t)ctx->code;
                for (i = 0; i < arg_count; i++) {
                        *(uint32_t *)ctx->code = (uint32_t)arg[i];
                        ctx->code = (uint32_t *)ctx->code + 1;
                }
        } else {
                arg_addr = 0;
        }

        f = (uint64_t)ex_thiscall_helper;

        /* if (!ex_thiscall_helper(env, dst, obj, symbol, len, hash, arg_count, arg)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Arg2 $a1 = dst */
                /* li $a1, dst */               IW(0x24050000 | tvar16(dst));

                /* Arg3 $a2 = obj */
                /* li $a2, obj */               IW(0x24060000 | tvar16(obj));

                /* Arg4 $a3 symbol */
                /* lui  $a3, symbol@hh */       IW(0x3c070000 | hihi16((uint64_t)symbol));
                /* ori  $a3, $a3, symbol@hl */  IW(0x34e70000 | hilo16((uint64_t)symbol));
                /* dsll $a3, $a3, 16 */         IW(0x00073c38);
                /* ori  $a3, $a3, symbol@lh */  IW(0x34e70000 | lohi16((uint64_t)symbol));
                /* dsll $a3, $a3, 16 */         IW(0x00073c38);
                /* ori  $a3, $a3, symbol@ll */  IW(0x34e70000 | lolo16((uint64_t)symbol));

                /* Arg5 $a4 = len */
                /* lui  $a4, len@h */           IW(0x3c080000 | hi16(len));
                /* ori  $a4, $a4, len@l */      IW(0x35080000 | lo16(len));

                /* Arg6 $a5 = hash */
                /* lui  $a5, hash@h */          IW(0x3c090000 | hi16(hash));
                /* ori  $a5, $a5, hash@l */     IW(0x35290000 | lo16(hash));

                /* Arg7 $a6 = arg_count */
                /* li   $a6, arg_count */       IW(0x240a0000 | lo16(arg_count));

                /* Arg8 $a7 = arg */
                /* lui  $a7, arg@hh */          IW(0x3c0b0000 | hihi16(arg_addr));
                /* ori  $a7, $a7, arg@hl */     IW(0x356b0000 | hilo16(arg_addr));
                /* dsll $a7, $a7, 16 */         IW(0x000b5c38);
                /* ori  $a7, $a7, arg@lh */     IW(0x356b0000 | lohi16(arg_addr));
                /* dsll $a7, $a7, 16 */         IW(0x000b5c38);
                /* ori  $a7, $a7, arg@ll */     IW(0x356b0000 | lolo16(arg_addr));

                /* Call ex_thiscall_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_JMP instruction. */
static inline bool
jit_visit_jmp_op(
        struct jit_context *ctx)
{
        uint32_t target_lpc;

        CONSUME_IMM32(target_lpc);
        if (target_lpc >= (uint32_t)(ctx->func->bytecode_size + 1)) {
                rt_error(ctx->env, BROKEN_BYTECODE);
                return false;
        }

        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BAL;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* b 0 */       IW(0x10000000);
                /* nop */       IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000);
        }

        return true;
}

/* Visit a OP_JMPIFTRUE instruction. */
static inline bool
jit_visit_jmpiftrue_op(
        struct jit_context *ctx)
{
        int src;
        uint32_t target_lpc;

        CONSUME_TMPVAR(src);
        CONSUME_IMM32(target_lpc);
        if (target_lpc >= (uint32_t)(ctx->func->bytecode_size + 1)) {
                rt_error(ctx->env, BROKEN_BYTECODE);
                return false;
        }

        ASM {
                /* $v0 = ex_condition_helper(env, src): 1/0, -1 on error. */
                IW(0x02002025);
                IW(0x24050000 | tvar16(src));
                IW(0x3c190000 | hihi16((uint64_t)ex_condition_helper));
                IW(0x37390000 | hilo16((uint64_t)ex_condition_helper));
                IW(0x0019cc38);
                IW(0x37390000 | lohi16((uint64_t)ex_condition_helper));
                IW(0x0019cc38);
                IW(0x37390000 | lolo16((uint64_t)ex_condition_helper));
                IW(0x03e09025);
                IW(0x0320f809);
                IW(0x00000000);
                IW(0x0240f825);
                IW(0x240cffff);
                EXCEPTION_IF_EQUAL(REG_V0, REG_T4);
                /* Branch patching expects the condition in $at. */
                IW(0x00400825);
        }

        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BNE;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* bne $at, 0, target */        IW(0x14200000);
                /* nop */                       IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
        }

        return true;
}

/* Visit a OP_JMPIFFALSE instruction. */
static inline bool
jit_visit_jmpiffalse_op(
        struct jit_context *ctx)
{
        int src;
        uint32_t target_lpc;

        CONSUME_TMPVAR(src);
        CONSUME_IMM32(target_lpc);
        if (target_lpc >= (uint32_t)(ctx->func->bytecode_size + 1)) {
                rt_error(ctx->env, BROKEN_BYTECODE);
                return false;
        }

        ASM {
                /* $v0 = ex_condition_helper(env, src): 1/0, -1 on error. */
                IW(0x02002025);
                IW(0x24050000 | tvar16(src));
                IW(0x3c190000 | hihi16((uint64_t)ex_condition_helper));
                IW(0x37390000 | hilo16((uint64_t)ex_condition_helper));
                IW(0x0019cc38);
                IW(0x37390000 | lohi16((uint64_t)ex_condition_helper));
                IW(0x0019cc38);
                IW(0x37390000 | lolo16((uint64_t)ex_condition_helper));
                IW(0x03e09025);
                IW(0x0320f809);
                IW(0x00000000);
                IW(0x0240f825);
                IW(0x240cffff);
                EXCEPTION_IF_EQUAL(REG_V0, REG_T4);
                IW(0x00400825);
        }
        
        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BEQ;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* beq $at, 0, target */        IW(0x10200000);
                /* nop */                       IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
        }

        return true;
}

/* Visit a OP_JMPIFEQ instruction. */
static inline bool
jit_visit_jmpifeq_op(
        struct jit_context *ctx)
{
        int src;
        uint32_t target_lpc;

        CONSUME_TMPVAR(src);
        CONSUME_IMM32(target_lpc);
        if (target_lpc >= (uint32_t)(ctx->func->bytecode_size + 1)) {
                rt_error(ctx->env, BROKEN_BYTECODE);
                return false;
        }

        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BEQ;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* beq $at, 0, taget */         IW(0x10200000);
                /* nop */                       IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
                IW(0x00000000); IW(0x00000000); IW(0x00000000); IW(0x00000000);
        }

        return true;
}

/* Visit a OP_SAFEPOINT instruction. */
static INLINE bool
jit_visit_safepoint_op(
        struct jit_context *ctx)
{
        uint64_t f;

        f = (uint64_t)ex_safepoint_helper;

        /* if (!ex_safepoint_helper(env)) return false; */
        ASM {
                /* $s0: env */
                /* $s1: &env->frame->tmpvar[0] */

                /* Arg1 $a0 = env */
                /* move $a0, $s0 */             IW(0x02002025);

                /* Call ex_safepoint_helper(). */
                /* lui  $t9, f@hh */            IW(0x3c190000 | hihi16(f));
                /* ori  $t9, f@hl */            IW(0x37390000 | hilo16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@lh */            IW(0x37390000 | lohi16(f));
                /* dsll $t9, $t9, 16 */         IW(0x0019cc38);
                /* ori  $t9, f@ll */            IW(0x37390000 | lolo16(f));
                /* move $s2, $ra */             IW(0x03e09025);
                /* jalr $t9 */                  IW(0x0320f809);
                /* nop */                       IW(0x00000000);
                /* move $ra, $s2 */             IW(0x0240f825);

                /* If failed: */
                EXCEPTION_IF_ZERO(REG_V0);
        }

        return true;
}

/* Visit a OP_PBASE instruction. (ABCE; inline machine code, mips64.)
 * The guard has proven the operand is a packed. */
static INLINE bool
jit_visit_pbase_op(
        struct jit_context *ctx)
{
        int dst;
        int src;
        uint32_t buf_ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        dst *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);
        buf_ofs = (uint32_t)offsetof(struct rt_packed, packed_buffer);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, src+8($s1) */        IW(0xdE2C0000 | lo16((uint32_t)(src + 8)));
                /* ld $t0, buf_ofs($t0) */      IW(0xdD8C0000 | lo16(buf_ofs));
                /* li $t2, LONG */              IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_LONG));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* sd $t0, dst+8($s1) */        IW(0xFE2C0000 | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PLEN instruction. (ABCE; helper-call implementation.) */
static INLINE bool
jit_visit_plen_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!ex_plen_helper(env, dst, src)) return false; */
        ASM_UNARY_OP(ex_plen_helper);

        return true;
}

/* Visit a OP_PCHECK instruction. (ABCE; helper-call implementation.) */
static INLINE bool
jit_visit_pcheck_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_IMM8(src2);

        /* if (!ex_pcheck_helper(env, dst, src, type)) return false; */
        ASM_BINARY_OP(ex_pcheck_helper);

        return true;
}

/* Visit a OP_TYPEIS instruction. (ABCE; helper-call implementation.) */
static INLINE bool
jit_visit_typeis_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_IMM8(src2);

        /* if (!ex_typeis_helper(env, dst, src, type)) return false; */
        ASM_BINARY_OP(ex_typeis_helper);

        return true;
}

/* Visit a OP_PLOAD8U instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload8u_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0x90000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_INT));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xac000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PSTORE8 instruction. (ABCE; inline, mips64 BE. Int source.) */
static INLINE bool
jit_visit_pstore8_op(
        struct jit_context *ctx)
{
        int base;
        int ofs;
        int src;

        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);
        CONSUME_TMPVAR(src);

        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* lw $t2, src+8($s1) */        IW(0x8E2E0000 | lo16((uint32_t)(src + 8)));
                /* store elem */                IW(0xa0000000 | (12 << 21) | (14 << 16));
        }

        return true;
}

/* Visit a OP_CHECKTYPE instruction. (Typed entry check.) */
static INLINE bool
jit_visit_checktype_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM8(src);

        /* if (!ex_checktype_helper(env, slot, type)) return false; */
        ASM_UNARY_OP(ex_checktype_helper);

        return true;
}

/* Visit a OP_PLOAD8S instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload8s_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0x80000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_INT));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xac000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PLOAD16U instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload16u_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,1 */      IW(0x000d6838 | (1 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0x94000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_INT));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xac000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PLOAD16S instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload16s_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,1 */      IW(0x000d6838 | (1 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0x84000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_INT));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xac000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PLOAD32 instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload32_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,2 */      IW(0x000d6838 | (2 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0x8c000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_INT));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xac000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PLOAD64 instruction. (ABCE; inline machine code, mips64 BE.) */
static INLINE bool
jit_visit_pload64_op(
        struct jit_context *ctx)
{
        int dst;
        int base;
        int ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);

        dst *= (int)sizeof(struct rt_value);
        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,3 */      IW(0x000d6838 | (3 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* load elem -> $t1 */          IW(0xdc000000 | (12 << 21) | (13 << 16));
                /* li $t2, tag */               IW(0x240e0000 | lo16((uint32_t)NOCT_VALUE_LONG));
                /* sw $t2, dst($s1) */          IW(0xAE2E0000 | lo16((uint32_t)dst));
                /* s[wd] $t1, dst+8($s1) */     IW(0xfc000000 | (17 << 21) | (13 << 16) | lo16((uint32_t)(dst + 8)));
        }

        return true;
}

/* Visit a OP_PSTORE16 instruction. (ABCE; inline, mips64 BE. Int source.) */
static INLINE bool
jit_visit_pstore16_op(
        struct jit_context *ctx)
{
        int base;
        int ofs;
        int src;

        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);
        CONSUME_TMPVAR(src);

        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,1 */      IW(0x000d6838 | (1 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* lw $t2, src+8($s1) */        IW(0x8E2E0000 | lo16((uint32_t)(src + 8)));
                /* store elem */                IW(0xa4000000 | (12 << 21) | (14 << 16));
        }

        return true;
}

/* Visit a OP_PSTORE32 instruction. (ABCE; inline, mips64 BE. Int source.) */
static INLINE bool
jit_visit_pstore32_op(
        struct jit_context *ctx)
{
        int base;
        int ofs;
        int src;

        CONSUME_TMPVAR(base);
        CONSUME_TMPVAR(ofs);
        CONSUME_TMPVAR(src);

        base *= (int)sizeof(struct rt_value);
        ofs *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);

        ASM {
                /* $s1: &env->frame->tmpvar[0] */

                /* ld $t0, base+8($s1) */       IW(0xdE2C0000 | lo16((uint32_t)(base + 8)));
                /* lw $t1, ofs+8($s1) */        IW(0x8E2D0000 | lo16((uint32_t)(ofs + 8)));
                /* dsll $t1,$t1,2 */      IW(0x000d6838 | (2 << 6));
                /* daddu $t0,$t0,$t1 */       IW(0x018d602d);
                /* lw $t2, src+8($s1) */        IW(0x8E2E0000 | lo16((uint32_t)(src + 8)));
                /* store elem */                IW(0xac000000 | (12 << 21) | (14 << 16));
        }

        return true;
}

/* Visit a OP_PSTORE64 instruction. (ABCE width op; helper-call.) */
static INLINE bool
jit_visit_pstore64_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!ex_pstore64_helper(env, a, b, c)) return false; */
        ASM_BINARY_OP(ex_pstore64_helper);

        return true;
}

/* Visit a OP_PLOADF32 instruction. (ABCE float32 width op; helper-call.) */
static INLINE bool
jit_visit_ploadf32_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        ASM_BINARY_OP(ex_ploadf32_helper);
        return true;
}

/* Visit a OP_PSTOREF32 instruction. (ABCE float32 width op; helper-call.) */
static INLINE bool
jit_visit_pstoref32_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        ASM_BINARY_OP(ex_pstoref32_helper);
        return true;
}


/*
 * Typed arithmetic ops (docs/design/07-typed-ops.md): dispatch-free
 * helper calls, like OP_PLOAD64 above.  The table is indexed by
 * (opcode - OP_IADD); the opcode block is contiguous by contract.
 */
typedef bool (CDECL *jit_typed_helper_t)(NoctEnv *env, int dst, int src1, int src2);

static const jit_typed_helper_t jit_typed_op_helper[] = {
        ex_iadd_helper,
        ex_isub_helper,
        ex_imul_helper,
        ex_idiv_helper,
        ex_imod_helper,
        ex_iand_helper,
        ex_ior_helper,
        ex_ixor_helper,
        ex_ishl_helper,
        ex_ishr_helper,
        ex_ilt_helper,
        ex_ilte_helper,
        ex_igt_helper,
        ex_igte_helper,
        ex_fadd_helper,
        ex_fsub_helper,
        ex_fmul_helper,
        ex_fdiv_helper,
        ex_flt_helper,
        ex_flte_helper,
        ex_fgt_helper,
        ex_fgte_helper
};

/* Visit an OP_IADD..OP_FGTE instruction. */
static INLINE bool
jit_visit_typed_op(
        struct jit_context *ctx,
        int op)
{
        jit_typed_helper_t f;
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        if (op == OP_ISHL || op == OP_ISHR) {
                /* The shift count is an imm8, not a tmpvar. */
                CONSUME_IMM8(src2);
        } else {
                CONSUME_TMPVAR(src2);
        }

        f = jit_typed_op_helper[op - OP_IADD];

        /* if (!f(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(f);

        return true;
}

/*
 * 128-bit vector ops use direct scalar lane operations over env->vreg.
 */
static bool
jit_put_scalar_vreg_base(struct jit_context *ctx)
{
        uint32_t ofs = (uint32_t)offsetof(struct rt_env, vreg);

        /* lui/ori $t4,offset; daddu $t4,$t4,$s0 */
        IW(0x3c0c0000 | hi16(ofs));
        IW(0x358c0000 | lo16(ofs));
        IW(0x0190602d);
        return true;
}

static bool
jit_put_vector_scalar_op(struct jit_context *ctx, int op,
                         int dst, int src1, int src2)
{
        int lane;

        if (!jit_put_scalar_vreg_base(ctx))
                return false;

        switch (op) {
        case OP_VLOADI32X4:
        case OP_VLOADF32X4:
        {
                int base = src1 * (int)sizeof(struct rt_value) + 8;
                int ofs = src2 * (int)sizeof(struct rt_value) + 8;
                IW(0xde280000 | lo16((uint32_t)base));
                IW(0x8e290000 | lo16((uint32_t)ofs));
                IW(0x00094880);
                IW(0x0109402d); /* daddu t0,t0,t1 */
                for (lane = 0; lane < 4; lane++) {
                        IW(0x8d0a0000 | (uint32_t)lane * 4);
                        IW(0xad8a0000 | (uint32_t)dst * 16 + (uint32_t)lane * 4);
                }
                return true;
        }
        case OP_VSTOREI32X4:
        case OP_VSTOREF32X4:
        {
                int base = dst * (int)sizeof(struct rt_value) + 8;
                int ofs = src1 * (int)sizeof(struct rt_value) + 8;
                IW(0xde280000 | lo16((uint32_t)base));
                IW(0x8e290000 | lo16((uint32_t)ofs));
                IW(0x00094880);
                IW(0x0109402d);
                for (lane = 0; lane < 4; lane++) {
                        IW(0x8d8a0000 | (uint32_t)src2 * 16 + (uint32_t)lane * 4);
                        IW(0xad0a0000 | (uint32_t)lane * 4);
                }
                return true;
        }
        case OP_VSPLATI32:
        case OP_VSPLATF32:
        {
                int src = src1 * (int)sizeof(struct rt_value) + 8;
                IW(0x8e2a0000 | lo16((uint32_t)src));
                for (lane = 0; lane < 4; lane++)
                        IW(0xad8a0000 | (uint32_t)dst * 16 + (uint32_t)lane * 4);
                return true;
        }
        case OP_VGETLANEI32:
        case OP_VGETLANEF32:
        {
                int d = dst * (int)sizeof(struct rt_value);
                uint32_t tag = (uint32_t)(op == OP_VGETLANEF32 ?
                                           NOCT_VALUE_FLOAT : NOCT_VALUE_INT);
                IW(0x8d8a0000 | (uint32_t)src1 * 16 + (uint32_t)src2 * 4);
                IW(0x240b0000 | tag);
                IW(0xae2b0000 | lo16((uint32_t)d));
                IW(0xae2a0000 | lo16((uint32_t)(d + 8)));
                return true;
        }
        case OP_VMOV128:
                for (lane = 0; lane < 4; lane++) {
                        IW(0x8d8a0000 | (uint32_t)src1 * 16 + (uint32_t)lane * 4);
                        IW(0xad8a0000 | (uint32_t)dst * 16 + (uint32_t)lane * 4);
                }
                return true;
        case OP_VADDI32X4:
        case OP_VSUBI32X4:
        case OP_VMULI32X4:
        case OP_VAND128:
        case OP_VOR128:
        case OP_VXOR128:
                for (lane = 0; lane < 4; lane++) {
                        uint32_t a = (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t b = (uint32_t)src2 * 16 + (uint32_t)lane * 4;
                        uint32_t d = (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        uint32_t word;
                        IW(0x8d880000 | a);
                        IW(0x8d890000 | b);
                        switch (op) {
                        case OP_VADDI32X4: word = 0x01095021; break;
                        case OP_VSUBI32X4: word = 0x01095023; break;
                        case OP_VMULI32X4: word = 0x71095002; break;
                        case OP_VAND128:   word = 0x01095024; break;
                        case OP_VOR128:    word = 0x01095025; break;
                        default:           word = 0x01095026; break;
                        }
                        IW(word);
                        IW(0xad8a0000 | d);
                }
                return true;
        case OP_VSHLI32X4:
        case OP_VSHRI32X4:
                IW(0x240b0000 | ((uint32_t)src2 & 31u));
                for (lane = 0; lane < 4; lane++) {
                        uint32_t s = (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t d = (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        IW(0x8d880000 | s);
                        IW(op == OP_VSHLI32X4 ? 0x01685004 : 0x01685006);
                        IW(0xad8a0000 | d);
                }
                return true;
        case OP_VADDF32X4:
        case OP_VSUBF32X4:
        case OP_VMULF32X4:
        case OP_VDIVF32X4:
                for (lane = 0; lane < 4; lane++) {
                        uint32_t a = (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t b = (uint32_t)src2 * 16 + (uint32_t)lane * 4;
                        uint32_t d = (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        uint32_t word;
                        IW(0xc5800000 | a);
                        IW(0xc5820000 | b);
                        switch (op) {
                        case OP_VADDF32X4: word = 0x46020100; break;
                        case OP_VSUBF32X4: word = 0x46020101; break;
                        case OP_VMULF32X4: word = 0x46020102; break;
                        default:           word = 0x46020103; break;
                        }
                        IW(word);
                        IW(0xe5840000 | d);
                }
                return true;
        default:
                assert(NEVER_COME_HERE);
                return false;
        }
}

/* Visit an OP_VLOADI32X4..OP_VSHRI32X4 instruction. */
static INLINE bool
jit_visit_vector_op(
        struct jit_context *ctx,
        int op)
{
        int dst;
        int src1;
        int src2;

        /* Decode (operand shapes vary per op; see bytecode.h). */
        switch (op) {
        case OP_VLOADI32X4:
        case OP_VLOADF32X4:
                CONSUME_IMM8(dst);
                CONSUME_TMPVAR(src1);
                CONSUME_TMPVAR(src2);
                break;
        case OP_VSTOREI32X4:
        case OP_VSTOREF32X4:
                CONSUME_TMPVAR(dst);
                CONSUME_TMPVAR(src1);
                CONSUME_IMM8(src2);
                break;
        case OP_VSPLATI32:
        case OP_VSPLATF32:
                CONSUME_IMM8(dst);
                CONSUME_TMPVAR(src1);
                src2 = 0;
                break;
        case OP_VGETLANEI32:
        case OP_VGETLANEF32:
                CONSUME_TMPVAR(dst);
                CONSUME_IMM8(src1);
                CONSUME_IMM8(src2);
                break;
        case OP_VMOV128:
                CONSUME_IMM8(dst);
                CONSUME_IMM8(src1);
                src2 = 0;
                break;
        default:
                CONSUME_IMM8(dst);
                CONSUME_IMM8(src1);
                CONSUME_IMM8(src2);
                break;
        }

        return jit_put_vector_scalar_op(ctx, op, dst, src1, src2);
}

/* Visit a bytecode of a function. */
bool
jit_visit_bytecode(
        struct jit_context *ctx)
{
        uint8_t opcode;

        /* Put a prologue. */
        ASM {
                /* s0: env */
                /* s1: &env->frame->tmpvar[0] */

                /* Push the general-purpose registers. */
                /* daddiu $sp, $sp, -64 */      IW(0x67bdffc0);
                /* sd $s0, 56($sp) */           IW(0xffb00038);
                /* sd $s1, 48($sp) */           IW(0xffb10030);
                /* sd $s2, 40($sp) */           IW(0xffb20028);
                /* sd $s3, 32($sp) */           IW(0xffb30020);
                /* sd $s4, 24($sp) */           IW(0xffb40018);
                /* sd $s5, 16($sp) */           IW(0xffb50010);
                /* sd $s6, 8($sp) */            IW(0xffb60008);
                /* sd $s7, 0($sp) */            IW(0xffb70000);

                /* s0 = env */
                /* move $s0, $a0 */             IW(0x00808025);

                /* s1 = *env->frame = &env->frame->tmpvar[0] */
                /* ld $s1, 0($a0) */            IW(0xdc910000);
                /* nop */                       IW(0x00000000);
                /* ld $s1, 0($s1) */            IW(0xde310000);
                /* nop */                       IW(0x00000000);

                /* Skip an exception handler. */
                /* b body */                    IW(0x1000000d);
                /* nop */                       IW(0x00000000);
        }

        /* Put an exception handler. */
        ctx->exception_code = ctx->code;
        ASM {
        /* EXCEPTION: */
                /* ld $s7, 0($sp) */            IW(0xdfb70000);
                /* ld $s6, 8($sp) */            IW(0xdfb60008);
                /* ld $s5, 16($sp) */           IW(0xdfb50010);
                /* ld $s4, 24($sp) */           IW(0xdfb40018);
                /* ld $s3, 32($sp) */           IW(0xdfb30020);
                /* ld $s2, 40($sp) */           IW(0xdfb20028);
                /* ld $s0, 48($sp) */           IW(0xdfb00030);
                /* ld $s1, 56($sp) */           IW(0xdfb10038);
                /* daddiu $sp, $sp, 64 */       IW(0x67bd0040);
                /* li $v0, 0 */                 IW(0x34020000);
                /* jr $ra */                    IW(0x03e00008);
                /* nop */                       IW(0x00000000);
        }

        /* Put a body. */
        while (ctx->lpc < ctx->func->bytecode_size) {
                /* Save LPC and addr. */
                if (ctx->pc_entry_count >= PC_ENTRY_MAX) {
                        rt_error(ctx->env, "Code too big.");
                        return false;
                }
                ctx->pc_entry[ctx->pc_entry_count].lpc = (uint32_t)ctx->lpc;
                ctx->pc_entry[ctx->pc_entry_count].code = ctx->code;
                ctx->pc_entry_count++;

                /* Dispatch by opcode. */
                CONSUME_OPCODE(opcode);
                switch (opcode) {
                case OP_LINEINFO:
                        if (!jit_visit_lineinfo_op(ctx))
                                return false;
                        break;
                case OP_ASSIGN:
                        if (!jit_visit_assign_op(ctx))
                                return false;
                        break;
                case OP_ICONST:
                        if (!jit_visit_iconst_op(ctx))
                                return false;
                        break;
                case OP_LICONST:
                        if (!jit_visit_liconst_op(ctx))
                                return false;
                        break;
                case OP_FCONST:
                        if (!jit_visit_fconst_op(ctx))
                                return false;
                        break;
                case OP_LFCONST:
                        if (!jit_visit_lfconst_op(ctx))
                                return false;
                        break;
                case OP_SCONST:
                        if (!jit_visit_sconst_op(ctx))
                                return false;
                        break;
                case OP_ACONST:
                        if (!jit_visit_aconst_op(ctx))
                                return false;
                        break;
                case OP_DCONST:
                        if (!jit_visit_dconst_op(ctx))
                                return false;
                        break;
                case OP_INC:
                        if (!jit_visit_inc_op(ctx))
                                return false;
                        break;
                case OP_ADD:
                        if (!jit_visit_add_op(ctx))
                                return false;
                        break;
                case OP_SUB:
                        if (!jit_visit_sub_op(ctx))
                                return false;
                        break;
                case OP_MUL:
                        if (!jit_visit_mul_op(ctx))
                                return false;
                        break;
                case OP_DIV:
                        if (!jit_visit_div_op(ctx))
                                return false;
                        break;
                case OP_MOD:
                        if (!jit_visit_mod_op(ctx))
                                return false;
                        break;
                case OP_AND:
                        if (!jit_visit_and_op(ctx))
                                return false;
                        break;
                case OP_OR:
                        if (!jit_visit_or_op(ctx))
                                return false;
                        break;
                case OP_XOR:
                        if (!jit_visit_xor_op(ctx))
                                return false;
                        break;
                case OP_SHL:
                        if (!jit_visit_shl_op(ctx))
                                return false;
                        break;
                case OP_SHR:
                        if (!jit_visit_shr_op(ctx))
                                return false;
                        break;
                case OP_NEG:
                        if (!jit_visit_neg_op(ctx))
                                return false;
                        break;
                case OP_NOT:
                        if (!jit_visit_not_op(ctx))
                                return false;
                        break;
                case OP_LT:
                        if (!jit_visit_lt_op(ctx))
                                return false;
                        break;
                case OP_LTE:
                        if (!jit_visit_lte_op(ctx))
                                return false;
                        break;
                case OP_EQ:
                        if (!jit_visit_eq_op(ctx))
                                return false;
                        break;
                case OP_NEQ:
                        if (!jit_visit_neq_op(ctx))
                                return false;
                        break;
                case OP_GTE:
                        if (!jit_visit_gte_op(ctx))
                                return false;
                        break;
                case OP_GT:
                        if (!jit_visit_gt_op(ctx))
                                return false;
                        break;
                case OP_EQI:
                        if (!jit_visit_eqi_op(ctx))
                                return false;
                        break;
                case OP_LOADARRAY:
                        if (!jit_visit_loadarray_op(ctx))
                                return false;
                        break;
                case OP_STOREARRAY:
                        if (!jit_visit_storearray_op(ctx))
                                return false;
                        break;
                case OP_LEN:
                        if (!jit_visit_len_op(ctx))
                        return false;
                        break;
                case OP_GETDICTKEYBYINDEX:
                        if (!jit_visit_getdictkeybyindex_op(ctx))
                        return false;
                        break;
                case OP_GETDICTVALBYINDEX:
                        if (!jit_visit_getdictvalbyindex_op(ctx))
                                return false;
                        break;
                case OP_LOADSYMBOL:
                        if (!jit_visit_loadsymbol_op(ctx))
                                return false;
                        break;
                case OP_STORESYMBOL:
                        if (!jit_visit_storesymbol_op(ctx))
                                return false;
                        break;
                case OP_LOADDOT:
                        if (!jit_visit_loaddot_op(ctx))
                                return false;
                        break;
                case OP_STOREDOT:
                        if (!jit_visit_storedot_op(ctx))
                                return false;
                        break;
                case OP_CALL:
                        if (!jit_visit_call_op(ctx))
                                return false;
                        break;
                case OP_THISCALL:
                        if (!jit_visit_thiscall_op(ctx))
                                return false;
                        break;
                case OP_JMP:
                        if (!jit_visit_jmp_op(ctx))
                                return false;
                        break;
                case OP_JMPIFTRUE:
                        if (!jit_visit_jmpiftrue_op(ctx))
                                return false;
                        break;
                case OP_JMPIFFALSE:
                        if (!jit_visit_jmpiffalse_op(ctx))
                                return false;
                        break;
                case OP_JMPIFEQ:
                        if (!jit_visit_jmpifeq_op(ctx))
                                return false;
                        break;
                case OP_SAFEPOINT:
#if defined(NOCT_USE_MULTITHREAD)
                        if (!jit_visit_safepoint_op(ctx))
                                return false;
#endif
                        break;
                case OP_PBASE:
                        if (!jit_visit_pbase_op(ctx))
                                return false;
                        break;
                case OP_PLEN:
                        if (!jit_visit_plen_op(ctx))
                                return false;
                        break;
                case OP_PCHECK:
                        if (!jit_visit_pcheck_op(ctx))
                                return false;
                        break;
                case OP_TYPEIS:
                        if (!jit_visit_typeis_op(ctx))
                                return false;
                        break;
                case OP_PLOAD8U:
                        if (!jit_visit_pload8u_op(ctx))
                                return false;
                        break;
                case OP_PSTORE8:
                        if (!jit_visit_pstore8_op(ctx))
                                return false;
                        break;
                case OP_CHECKTYPE:
                        if (!jit_visit_checktype_op(ctx))
                                return false;
                        break;
                case OP_PLOAD8S:
                        if (!jit_visit_pload8s_op(ctx))
                                return false;
                        break;
                case OP_PLOAD16U:
                        if (!jit_visit_pload16u_op(ctx))
                                return false;
                        break;
                case OP_PLOAD16S:
                        if (!jit_visit_pload16s_op(ctx))
                                return false;
                        break;
                case OP_PLOAD32:
                        if (!jit_visit_pload32_op(ctx))
                                return false;
                        break;
                case OP_PLOAD64:
                        if (!jit_visit_pload64_op(ctx))
                                return false;
                        break;
                case OP_PSTORE16:
                        if (!jit_visit_pstore16_op(ctx))
                                return false;
                        break;
                case OP_PSTORE32:
                        if (!jit_visit_pstore32_op(ctx))
                                return false;
                        break;
                case OP_PSTORE64:
                        if (!jit_visit_pstore64_op(ctx))
                                return false;
                        break;
                case OP_PLOADF32:
                        if (!jit_visit_ploadf32_op(ctx))
                                return false;
                        break;
                case OP_PSTOREF32:
                        if (!jit_visit_pstoref32_op(ctx))
                                return false;
                        break;
                case OP_IADD:
                case OP_ISUB:
                case OP_IMUL:
                case OP_IDIV:
                case OP_IMOD:
                case OP_IAND:
                case OP_IOR:
                case OP_IXOR:
                case OP_ISHL:
                case OP_ISHR:
                case OP_ILT:
                case OP_ILTE:
                case OP_IGT:
                case OP_IGTE:
                case OP_FADD:
                case OP_FSUB:
                case OP_FMUL:
                case OP_FDIV:
                case OP_FLT:
                case OP_FLTE:
                case OP_FGT:
                case OP_FGTE:
                        if (!jit_visit_typed_op(ctx, opcode))
                                return false;
                        break;
                case OP_VLOADI32X4:
                case OP_VSTOREI32X4:
                case OP_VSPLATI32:
                case OP_VGETLANEI32:
                case OP_VMOV128:
                case OP_VADDI32X4:
                case OP_VSUBI32X4:
                case OP_VMULI32X4:
                case OP_VAND128:
                case OP_VOR128:
                case OP_VXOR128:
        case OP_VSHLI32X4:
        case OP_VSHRI32X4:
        case OP_VLOADF32X4:
        case OP_VSTOREF32X4:
        case OP_VSPLATF32:
        case OP_VGETLANEF32:
        case OP_VADDF32X4:
        case OP_VSUBF32X4:
        case OP_VMULF32X4:
        case OP_VDIVF32X4:
                        if (!jit_visit_vector_op(ctx, opcode))
                                return false;
                        break;
                default:
                        assert(JIT_OP_NOT_IMPLEMENTED);
                        break;
                }
        }

        /* Add the tail PC to the table. */
        ctx->pc_entry[ctx->pc_entry_count].lpc = (uint32_t)ctx->lpc;
        ctx->pc_entry[ctx->pc_entry_count].code = ctx->code;
        ctx->pc_entry_count++;

        /* Put an epilogue. */
        ASM {
        /* EPILOGUE: */
                /* ld $s7, 0($sp) */            IW(0xdfb70000);
                /* ld $s6, 8($sp) */            IW(0xdfb60008);
                /* ld $s5, 16($sp) */           IW(0xdfb50010);
                /* ld $s4, 24($sp) */           IW(0xdfb40018);
                /* ld $s3, 32($sp) */           IW(0xdfb30020);
                /* ld $s2, 40($sp) */           IW(0xdfb20028);
                /* ld $s1, 48($sp) */           IW(0xdfb10030);
                /* ld $s0, 56($sp) */           IW(0xdfb00038);
                /* daddiu $sp, $sp, 64 */       IW(0x67bd0040);
                /* li $v0, 1 */                 IW(0x34020001);
                /* jr $ra */                    IW(0x03e00008);
                /* nop */                       IW(0x00000000);
        }

        return true;
}

static bool
jit_patch_branch(
    struct jit_context *ctx,
    int patch_index)
{
        uint32_t *target_code;
        int offset;
        int i;

        if (ctx->pc_entry_count == 0)
                return true;

        /* Search a code addr at lpc. */
        target_code = NULL;
        for (i = 0; i < ctx->pc_entry_count; i++) {
                if (ctx->pc_entry[i].lpc == ctx->branch_patch[patch_index].lpc) {
                        target_code = ctx->pc_entry[i].code;
                        break;
                }
                        
        }
        if (target_code == NULL) {
                rt_error(ctx->env, "Branch target not found.");
                return false;
        }

        /* Calc a branch offset. */
        offset = (int)((intptr_t)target_code - (intptr_t)ctx->branch_patch[patch_index].code - 4) / 4;
        /* Set the assembler cursor. */
        ctx->code = ctx->branch_patch[patch_index].code;

        /* Assemble. */
        if (ctx->branch_patch[patch_index].type == PATCH_BAL) {
                if (getenv("NOCT_JIT_FORCE_LONG_BRANCH") == NULL &&
                    offset >= -32768 && offset <= 32767) {
                        int i;
                        ASM { IW(0x10000000 | lo16((uint32_t)offset)); IW(0); }
                        for (i = 0; i < 6; i++) if (!jit_put_word(ctx, 0)) return false;
                } else if (!jit_put_abs_jump(ctx, (uint64_t)(uintptr_t)target_code)) {
                        return false;
                }
        } else if (ctx->branch_patch[patch_index].type == PATCH_BEQ) {
                if (getenv("NOCT_JIT_FORCE_LONG_BRANCH") == NULL &&
                    offset >= -32768 && offset <= 32767) {
                        int i;
                        ASM { IW(0x10200000 | lo16((uint32_t)offset)); IW(0); }
                        for (i = 0; i < 8; i++) if (!jit_put_word(ctx, 0)) return false;
                } else {
                        ASM { IW(0x14200009); IW(0); }
                        if (!jit_put_abs_jump(ctx, (uint64_t)(uintptr_t)target_code)) return false;
                }
        } else if (ctx->branch_patch[patch_index].type == PATCH_BNE) {
                if (getenv("NOCT_JIT_FORCE_LONG_BRANCH") == NULL &&
                    offset >= -32768 && offset <= 32767) {
                        int i;
                        ASM { IW(0x14200000 | lo16((uint32_t)offset)); IW(0); }
                        for (i = 0; i < 8; i++) if (!jit_put_word(ctx, 0)) return false;
                } else {
                        ASM { IW(0x10200009); IW(0); }
                        if (!jit_put_abs_jump(ctx, (uint64_t)(uintptr_t)target_code)) return false;
                }
        }

        return true;
}

#endif /* defined(NOCT_ARCH_PPC32) && defined(NOCT_USE_JIT) */
