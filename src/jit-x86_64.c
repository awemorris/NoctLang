/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * JIT (x86_64): Just-In-Time native code generation
 */

#include "c89compat.h"        /* ARCH_X86_64 */

#if defined(ARCH_X86_64) && defined(USE_JIT)

#include "runtime.h"
#include "jit.h"
#include "execution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * ABI Check (MS ABI or SYSV ABI)
 */
#define IS_MSABI                (sizeof(long) == 4)

/* False asseretion */
#define JIT_OP_NOT_IMPLEMENTED  0
#define NEVER_COME_HERE         0

/* PC entry size. */
#define PC_ENTRY_MAX            2048

/* Branch pathch size. */
#define BRANCH_PATCH_MAX        2048

/* Branch patch type */
#define PATCH_JMP               0
#define PATCH_JE                1
#define PATCH_JNE               2

/* Generated code. */
static uint8_t *jit_code_region;
static uint8_t *jit_code_region_cur;
static uint8_t *jit_code_region_tail;

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
                if (!jit_map_memory_region((void **)&jit_code_region, JIT_CODE_MAX)) {
                        rt_error(env, "Memory mapping failed.");
                        return false;
                }
                jit_code_region_cur = jit_code_region;
                jit_code_region_tail = jit_code_region + JIT_CODE_MAX;
                is_writable = true;
        }

        /* Make a context. */
        memset(&ctx, 0, sizeof(struct jit_context));
        ctx.code_top = jit_code_region_cur;
        ctx.code_end = jit_code_region_tail;
        ctx.code = ctx.code_top;
        ctx.env = env;
        ctx.func = func;

        /* Make code writable and non-executable. */
        if (!is_writable)
                jit_map_writable(jit_code_region, JIT_CODE_MAX);

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
                jit_unmap_memory_region(jit_code_region, JIT_CODE_MAX);

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
        jit_map_executable(jit_code_region, JIT_CODE_MAX);

        is_writable = false;
}

/*
 * Assembler output functions
 */

/* Serif */
#define ASM

/* Put a instruction byte. */
#define IB(b)                        if (!jit_put_byte(ctx, b)) return false
static NOCT_INLINE bool
jit_put_byte(
        struct jit_context *ctx,
        uint8_t b)
{
        if ((uint8_t *)ctx->code + 1 > (uint8_t *)ctx->code_end) {
                rt_error(ctx->env, "Code too big.");
                return false;
        }

        *(uint8_t *)ctx->code = b;
        ctx->code = (uint8_t *)ctx->code + 1;

        return true;
}

/* Put a instruction double word. */
#define ID(d)                        if (!jit_put_dword(ctx, d)) return false
static NOCT_INLINE bool
jit_put_dword(
        struct jit_context *ctx,
        uint32_t dw)
{
        if ((uint8_t *)ctx->code + 4 > (uint8_t *)ctx->code_end) {
                rt_error(ctx->env, "Code too big.");
                return false;
        }

        *(uint8_t *)ctx->code = (uint8_t)(dw & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((dw >> 8) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((dw >> 16) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((dw >> 24) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;

        return true;
}

/* Put a instruction word. */
#define IQ(q)                        if (!jit_put_qword(ctx, q)) return false
static NOCT_INLINE bool
jit_put_qword(
        struct jit_context *ctx,
        uint64_t qw)
{
        if ((uint8_t *)ctx->code + 8 > (uint8_t *)ctx->code_end) {
                rt_error(ctx->env, "Code too big.");
                return false;
        }

        *(uint8_t *)ctx->code = (uint8_t)(qw & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 8) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 16) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 24) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 32) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 40) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 48) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;
        *(uint8_t *)ctx->code = (uint8_t)((qw >> 56) & 0xff);
        ctx->code = (uint8_t *)ctx->code + 1;

        return true;
}

/*
 * Templates
 */

#define ASM_BINARY_OP(f)                                                                                            \
    if (IS_MSABI) {                                                                                                 \
        /* if (!f(rt, dst, src1, src2)) return false; */                                                            \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: rt */                                                                                           \
                                                                                                                    \
            /* mov r14 -> rcx (1st) */       IB(0x4C); IB(0x89); IB(0xF1);                                          \
            /* mov dst -> rdx (2nd) */       IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);                       \
            /* mov src1 -> r8 (3rd) */       IB(0x49); IB(0xB8); IQ((uint64_t)src1); IB(0x4D); IB(0x89); IB(0xC0);  \
            /* mov src2 -> r9 (4th) */       IB(0x49); IB(0xB9); IQ((uint64_t)src2); IB(0x4D); IB(0x89); IB(0xC9);  \
            /* movabs f -> rax */            IB(0x48); IB(0xB8); IQ((uint64_t)f);                                   \
            /* sub rsp, 32 (shadow space) */ IB(0x48); IB(0x83); IB(0xEC); IB(0x20);                                \
            /* call rax */                   IB(0xFF); IB(0xD0);                                                    \
            /* add rsp, 32 */                IB(0x48); IB(0x83); IB(0xC4); IB(0x20);                                \
                                                                                                                    \
            /* test eax, eax */              IB(0x83); IB(0xF8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xFF); IB(0xE5);                                          \
        /* next: */                                                                                                 \
        }                                                                                                           \
    } else {                                                                                                        \
        /* if (!f(rt, dst, src1, src2)) return false; */                                                            \
        ASM {                                                                                                       \
                /* r13: exception_handler */                                                                        \
                /* r14: rt */                                                                                       \
                /* r14: &rt->frame->tmpvar[0] */                                                                    \
                                                                                                                    \
                /* movq %r14, %rdi */        IB(0x4c); IB(0x89); IB(0xf7);                                          \
                /* movq dst, %rsi */         IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);                       \
                /* movq src1, %rdx */        IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src1);                      \
                /* movq src2, %rcx */        IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src2);                      \
                /* movabs f, %r8 */          IB(0x49); IB(0xb8); IQ((uint64_t)f);                                   \
                /* call *%r8 */              IB(0x41); IB(0xff); IB(0xd0);                                          \
                                                                                                                    \
                /* cmpl $0, %eax */          IB(0x83); IB(0xf8); IB(0x00);                                          \
                /* jne 8 <next> */           IB(0x75); IB(0x03);                                                    \
                /* jmp *%r13 */              IB(0x41); IB(0xff); IB(0xe5);                                          \
                /* next:*/                                                                                          \
        }                                                                                                           \
    }

#define ASM_UNARY_OP(f)                                                                                             \
    if (IS_MSABI) {                                                                                                 \
        /* if (!f(rt, dst, src)) return false; */                                                                   \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: rt */                                                                                           \
                                                                                                                    \
            /* mov r14 -> rcx (1st) */       IB(0x4C); IB(0x89); IB(0xF1);                                          \
            /* mov dst -> rdx (2nd) */       IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);                       \
            /* mov src -> r8 (3rd) */        IB(0x49); IB(0xB8); IQ((uint64_t)src); IB(0x4D); IB(0x89); IB(0xC0);   \
            /* movabs f -> rax */            IB(0x48); IB(0xB8); IQ((uint64_t)f);                                   \
            /* sub rsp, 32 (shadow space) */ IB(0x48); IB(0x83); IB(0xEC); IB(0x20);                                \
            /* call rax */                   IB(0xFF); IB(0xD0);                                                    \
            /* add rsp, 32 */                IB(0x48); IB(0x83); IB(0xC4); IB(0x20);                                \
                                                                                                                    \
            /* cmpl $0, %eax */              IB(0x83); IB(0xf8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xff); IB(0xe5);                                          \
            /* next:*/                                                                                              \
        /* next: */                                                                                                 \
        }                                                                                                           \
    } else {                                                                                                        \
        /* if (!f(rt, dst, src)) return false; */                                                                   \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: rt */                                                                                           \
            /* r14: &rt->frame->tmpvar[0] */                                                                        \
                                                                                                                    \
            /* movq %r14, %rdi */        IB(0x4c); IB(0x89); IB(0xf7);                                              \
            /* movq dst, %rsi */         IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);                           \
            /* movq src, %rdx */         IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);                           \
            /* movabs f, %r8 */          IB(0x49); IB(0xb8); IQ((uint64_t)f);                                       \
            /* call *%r8 */              IB(0x41); IB(0xff); IB(0xd0);                                              \
                                                                                                                    \
            /* cmpl $0, %eax */          IB(0x83); IB(0xf8); IB(0x00);                                              \
            /* jne 8 <next> */           IB(0x75); IB(0x03);                                                        \
            /* jmp *%r13 */              IB(0x41); IB(0xff); IB(0xe5);                                              \
        /* next:*/                                                                                                  \
        }                                                                                                           \
    }

/*
 * Bytecode visitors
 */

/* Visit a ROP_LINEINFO instruction. */
static NOCT_INLINE bool
jit_visit_lineinfo_op(
        struct jit_context *ctx)
{
        uint32_t line;

        CONSUME_IMM32(line);

        /* rt->line = line; */
        ASM {
                /* r14 = rt */

                /* movl line, %rax */                IB(0x48); IB(0xc7); IB(0xc0); ID(line);
                /* movq %rax, 0x8(%r14) */        IB(0x49); IB(0x89); IB(0x46); IB(0x08);
        }

        return true;
}

/* Visit a ROP_ASSIGN instruction. */
static NOCT_INLINE bool
jit_visit_assign_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        dst *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);

        /* rt->frame->tmpvar[dst] = rt->frame->tmpvar[src]; */
        ASM {
                /* r15 = &rt->frame->tmpvar[0] */

                /* movq dst, %rax */                IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* movq src, %rbx */                IB(0x48); IB(0xc7); IB(0xc3); ID((uint32_t)src);
                /* addq %r15, %rax */                IB(0x4c); IB(0x01); IB(0xf8);
                /* addq %r15, %rbx */                IB(0x4c); IB(0x01); IB(0xfb);
                /* movq (%rbx), %rcx */                IB(0x48); IB(0x8b); IB(0x0b);
                /* movq 8(%rbx), %rdx */        IB(0x48); IB(0x8b); IB(0x53); IB(0x08);
                /* movq %rcx, (%rax) */                IB(0x48); IB(0x89); IB(0x08);
                /* movq %rdx, 8(%rax) */        IB(0x48); IB(0x89); IB(0x50); IB(0x08);
        }

        return true;
}

/* Visit a ROP_ICONST instruction. */
static NOCT_INLINE bool
jit_visit_iconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint32_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM32(val);

        dst *= (int)sizeof(struct rt_value);

        /* &rt->frame->tmpvar[dst].type = RT_VALUE_INT; */
        /* &rt->frame->tmpvar[dst].val.i = val; */
        ASM {
                /* r15 = &rt->frame->tmpvar[0] */

                /* movq dst, %rax */                IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15, %rax */                IB(0x4c); IB(0x01); IB(0xf8);
                /* movl $0, (%rax) */                IB(0xc7); IB(0x00); ID(0);
                /* movl val, 8(%rax) */                IB(0xc7); IB(0x40); IB(0x08); ID((uint32_t)val);
        }

        return true;
}

/* Visit a ROP_FCONST instruction. */
static NOCT_INLINE bool
jit_visit_fconst_op(
        struct jit_context *ctx)
{
        int dst;
        uint32_t val;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM32(val);

        dst *= (int)sizeof(struct rt_value);

        /* &rt->frame->tmpvar[dst].type = RT_VALUE_INT; */
        /* &rt->frame->tmpvar[dst].val.i = val; */
        ASM {
                /* r15 = &rt->frame->tmpvar[0] */

                /* movq dst, %rax */                IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15, %rax */                IB(0x4c); IB(0x01); IB(0xf8);
                /* movl $1, (%rax) */                IB(0xc7); IB(0x00); ID(1);
                /* movl val, 8(%rax) */                IB(0xc7); IB(0x40); IB(0x08); ID(val);
        }

        return true;
}

/* Visit a ROP_SCONST instruction. */
static NOCT_INLINE bool
jit_visit_sconst_op(
        struct jit_context *ctx)
{
        int dst;
        const char *val;

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(val);

        dst *= (int)sizeof(struct rt_value);

        if (IS_MSABI) {
                /* rt_make_string(rt, &rt->frame->tmpvar[dst], val); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r15: &rt->frame->tmpvar[0] */

                        /* mov r14 -> rcx (1st arg: rt) */     IB(0x4C); IB(0x89); IB(0xF1);
                        /* mov dst -> rdx (2nd arg) */         IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
                        /* add r15 -> rdx (rdx = r15 + dst) */ IB(0x4C); IB(0x01); IB(0xFA);
                        /* movabs val -> r8 (3rd arg: val) */  IB(0x49); IB(0xB8); IQ((uint64_t)val);
                        /* movabs r_make_string -> rax */      IB(0x48); IB(0xB8); IQ((uint64_t)rt_make_string);
                        /* sub rsp, 32 (shadow space) */       IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
                        /* call rax */                         IB(0xFF); IB(0xD0);
                        /* add rsp, 32 */                      IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

                        /* cmpl $0, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                     IB(0x75); IB(0x03);
                        /* jmp *%r13 */                        IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
	} else {
	        /* rt_make_string(rt, &rt->frame->tmpvar[dst], val); */
	        ASM {
	                /* r13 = exception_handler */
	                /* r14 = rt */
	                /* r15 = &rt->frame->tmpvar[0] */

	                /* movq %r14, %rdi */                  IB(0x4c); IB(0x89); IB(0xf7);
	                /* movq dst, %rsi */                   IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
	                /* addq %r15, %rsi */                  IB(0x4c); IB(0x01); IB(0xfe);
	                /* movabs val, %rdx */                 IB(0x48); IB(0xba); IQ((uint64_t)val);
	                /* movabs rt_make_string, %r8 */       IB(0x49); IB(0xb8); IQ((uint64_t)rt_make_string);
	                /* call *%r8 */                        IB(0x41); IB(0xff); IB(0xd0);

	                /* cmpl $0, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                     IB(0x75); IB(0x03);
	                /* jmp *%r13 */                        IB(0x41); IB(0xff); IB(0xe5);
	        /* next: */
	        }
	}

        return true;
}

/* Visit a ROP_ACONST instruction. */
static NOCT_INLINE bool
jit_visit_aconst_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

        if (IS_MSABI) {
                /* rt_make_empty_array(rt, &rt->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r15: &rt->frame->tmpvar[0] */

                        /* mov r14 -> rcx (1st arg: rt) */        IB(0x4C); IB(0x89); IB(0xF1);
                        /* mov dst -> rdx (2nd arg) */            IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
                        /* add r15 -> rdx (rdx = r15 + dst) */    IB(0x4C); IB(0x01); IB(0xFA);
                        /* movabs rt_make_empty_array -> rax */   IB(0x48); IB(0xB8); IQ((uint64_t)rt_make_empty_array);
                        /* sub rsp, 32 (shadow space) */          IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
                        /* call rax */                            IB(0xFF); IB(0xD0);
                        /* add rsp, 32 */                         IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

                        /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* rt_make_empty_array(rt, &rt->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r15: &rt->frame->tmpvar[0] */

                        /* movq %r14, %rdi */                     IB(0x4c); IB(0x89); IB(0xf7);
                        /* movq dst, %rsi */                      IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* addq %r15, %rsi */                     IB(0x4c); IB(0x01); IB(0xfe);
                        /* movabs rt_make_empty_array, %r8 */     IB(0x49); IB(0xb8); IQ((uint64_t)rt_make_empty_array);
                        /* call *%r8 */                           IB(0x41); IB(0xff); IB(0xd0);

                        /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        }

        return true;
}

/* Visit a ROP_DCONST instruction. */
static NOCT_INLINE bool
jit_visit_dconst_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

	if (IS_MSABI) {
                /* rt_make_empty_dict(rt, &rt->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r15: &rt->frame->tmpvar[0] */

                        /* mov r14 -> rcx (1st arg: rt) */      IB(0x4C); IB(0x89); IB(0xF1);
                        /* mov dst -> rdx (2nd arg) */          IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
                        /* add r15 -> rdx (rdx = r15 + dst) */  IB(0x4C); IB(0x01); IB(0xFA);
                        /* movabs rt_make_empty_dict -> rax */  IB(0x48); IB(0xB8); IQ((uint64_t)rt_make_empty_dict);
                        /* sub rsp, 32 (shadow space) */        IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
                        /* call rax */                          IB(0xFF); IB(0xD0);
                        /* add rsp, 32 */                       IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

                        /* cmpl $0, %eax */                     IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *%r13 */                         IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* rt_make_empty_dict(rt, &rt->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r15: &rt->frame->tmpvar[0] */

                        /* movq %r14, %rdi */                   IB(0x4c); IB(0x89); IB(0xf7);
                        /* movq dst, %rsi */                    IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* addq %r15, %rsi */                   IB(0x4c); IB(0x01); IB(0xfe);
                        /* movabs rt_make_empty_dict, %r8 */    IB(0x49); IB(0xb8); IQ((uint64_t)rt_make_empty_dict);
                        /* call *%r8 */                         IB(0x41); IB(0xff); IB(0xd0);

                        /* cmpl $0, %eax */                     IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *%r13 */                         IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
	}

        return true;
}

/* Visit a ROP_INC instruction. */
static NOCT_INLINE bool
jit_visit_inc_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

        /* &rt->frame->tmpvar[dst].val.i++ */
        ASM {
                /* r15 = &rt->frame->tmpvar[0] */

                /* movq dst, %rax */                        IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15, %rax */                        IB(0x4c); IB(0x01); IB(0xf8);
                /* incq 8(%rax) */                        IB(0x48); IB(0xff); IB(0x40); IB(0x08);
        }

        return true;
}

/* Visit a ROP_ADD instruction. */
static NOCT_INLINE bool
jit_visit_add_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_add_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_add_helper);

        return true;
}

/* Visit a ROP_SUB instruction. */
static NOCT_INLINE bool
jit_visit_sub_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_sub_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_sub_helper);

        return true;
}

/* Visit a ROP_MUL instruction. */
static NOCT_INLINE bool
jit_visit_mul_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_mul_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_mul_helper);

        return true;
}

/* Visit a ROP_DIV instruction. */
static NOCT_INLINE bool
jit_visit_div_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_div_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_div_helper);

        return true;
}

/* Visit a ROP_MOD instruction. */
static NOCT_INLINE bool
jit_visit_mod_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_mod_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_mod_helper);

        return true;
}

/* Visit a ROP_AND instruction. */
static NOCT_INLINE bool
jit_visit_and_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_and_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_and_helper);

        return true;
}

/* Visit a ROP_OR instruction. */
static NOCT_INLINE bool
jit_visit_or_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_or_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_or_helper);

        return true;
}

/* Visit a ROP_XOR instruction. */
static NOCT_INLINE bool
jit_visit_xor_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_xor_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_xor_helper);

        return true;
}

/* Visit a ROP_NEG instruction. */
static NOCT_INLINE bool
jit_visit_neg_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!jit_neg_helper(rt, dst, src)) return false; */
        ASM_UNARY_OP(rt_not_helper);

        return true;
}

/* Visit a ROP_NOT instruction. */
static NOCT_INLINE bool
jit_visit_not_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!jit_not_helper(rt, dst, src)) return false; */
        ASM_UNARY_OP(rt_not_helper);

        return true;
}

/* Visit a ROP_LT instruction. */
static NOCT_INLINE bool
jit_visit_lt_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_lt_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_lt_helper);

        return true;
}

/* Visit a ROP_LTE instruction. */
static NOCT_INLINE bool
jit_visit_lte_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_lte_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_lte_helper);

        return true;
}

/* Visit a ROP_EQ instruction. */
static NOCT_INLINE bool
jit_visit_eq_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_eq_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_eq_helper);

        return true;
}

/* Visit a ROP_NEQ instruction. */
static NOCT_INLINE bool
jit_visit_neq_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_neq_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_neq_helper);

        return true;
}

/* Visit a ROP_GTE instruction. */
static NOCT_INLINE bool
jit_visit_gte_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_gte_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_gte_helper);

        return true;
}

/* Visit a ROP_EQI instruction. */
static NOCT_INLINE bool
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

        /* src1 - src2 */
        ASM {
                /* movq dst, %rax */                IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15, %rax */                IB(0x4c); IB(0x01); IB(0xf8);

                /* movq src1, %rbx */                IB(0x48); IB(0xc7); IB(0xc3); ID((uint32_t)src1);
                /* addq %r15, %rbx */                IB(0x4c); IB(0x01); IB(0xfb);
                
                /* movq src2, %rcx */                IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src2);
                /* addq %r15, %rcx */                IB(0x4c); IB(0x01); IB(0xf9);

                /* movq 8(%rbx), %rax */        IB(0x48); IB(0x8b); IB(0x43); IB(0x08);
                /* movq 8(%rcx), %rdx */        IB(0x48); IB(0x8b); IB(0x51); IB(0x08);
                /* cmpl %eax, %edx */                IB(0x39); IB(0xc2);
        }

        return true;
}

/* Visit a ROP_GT instruction. */
static NOCT_INLINE bool
jit_visit_gt_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_gt_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_gt_helper);

        return true;
}

/* Visit a ROP_LOADARRAY instruction. */
static NOCT_INLINE bool
jit_visit_loadarray_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_loadarray_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_loadarray_helper);

        return true;
}

/* Visit a ROP_STOREARRAY instruction. */
static NOCT_INLINE bool
jit_visit_storearray_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_storearray_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_storearray_helper);

        return true;
}

/* Visit a ROP_LEN instruction. */
static NOCT_INLINE bool
jit_visit_len_op(
        struct jit_context *ctx)
{
        int dst;
        int src;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);

        /* if (!jit_len_helper(rt, dst, src)) return false; */
        ASM_UNARY_OP(rt_len_helper);

        return true;
}

/* Visit a ROP_GETDICTKEYBYINDEX instruction. */
static NOCT_INLINE bool
jit_visit_getdictkeybyindex_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_getdictkeybyindex_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_getdictkeybyindex_helper);

        return true;
}

/* Visit a ROP_GETDICTVALBYINDEX instruction. */
static NOCT_INLINE bool
jit_visit_getdictvalbyindex_op(
        struct jit_context *ctx)
{
        int dst;
        int src1;
        int src2;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src1);
        CONSUME_TMPVAR(src2);

        /* if (!jit_getdictvalbyindex_helper(rt, dst, src1, src2)) return false; */
        ASM_BINARY_OP(rt_getdictvalbyindex_helper);

        return true;
}

/* Visit a ROP_LOADSYMBOL instruction. */
static NOCT_INLINE bool
jit_visit_loadsymbol_op(
        struct jit_context *ctx)
{
        int dst;
        const char *src_s;
        uint64_t src;

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(src_s);
        src = (uint64_t)(intptr_t)src_s;

	if (IS_MSABI) {
	        /* if (!rt_loadsymbol_helper(rt, dst, src)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* mov r14 -> rcx (1st arg: rt) */       IB(0x4C); IB(0x89); IB(0xF1);
	                /* mov dst -> rdx (2nd arg) */           IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
	                /* movabs src -> r8 (3rd arg) */         IB(0x49); IB(0xB8); IQ((uint64_t)src);
	                /* movabs rt_loadsymbol_helper -> rax */ IB(0x48); IB(0xB8); IQ((uint64_t)rt_loadsymbol_helper);

	                /* sub rsp, 32 (shadow space) */         IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
	                /* call rax */                           IB(0xFF); IB(0xD0);
	                /* add rsp, 32 */                        IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

	                /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                        IB(0x75); IB(0x03);
	                /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
	        /* next: */
	        }
	} else {
	        /* if (!rt_loadsymbol_helper(rt, dst, src)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* movq %r14, %rdi */                    IB(0x4c); IB(0x89); IB(0xf7);
	                /* movq dst, %rsi */                     IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
	                /* movabs src, %rdx */                   IB(0x48); IB(0xba); IQ(src);
	                /* movabs rt_loadsymbol_helper, %r8 */   IB(0x49); IB(0xb8); IQ((uint64_t)rt_loadsymbol_helper);
	                /* call *%r8 */                          IB(0x41); IB(0xff); IB(0xd0);

	                /* cmpl $0, %eax */                      IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                       IB(0x75); IB(0x03);
	                /* jmp *%r13 */                          IB(0x41); IB(0xff); IB(0xe5);
	        /* next:*/
	        }
	}

        return true;
}

/* Visit a ROP_STORESYMBOL instruction. */
static NOCT_INLINE bool
jit_visit_storesymbol_op(
        struct jit_context *ctx)
{
        const char *dst_s;
        uint64_t dst;
        int src;

        CONSUME_STRING(dst_s);
        CONSUME_TMPVAR(src);
        dst = (uint64_t)(intptr_t)dst_s;

	if (IS_MSABI) {
	        /* if (!rt_storesymbol_helper(rt, dst, src)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* mov r14 -> rcx (1st arg: rt) */        IB(0x4C); IB(0x89); IB(0xF1);
	                /* movabs dst -> rdx (2nd arg) */         IB(0x48); IB(0xBA); IQ((uint64_t)dst);
	                /* mov src -> r8 (3rd arg) */             IB(0x49); IB(0xB8); IQ((uint64_t)src);
	                /* movabs rt_storesymbol_helper -> rax */ IB(0x48); IB(0xB8); IQ((uint64_t)rt_storesymbol_helper);
	                /* sub rsp, 32 (shadow space) */          IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
	                /* call rax */                            IB(0xFF); IB(0xD0);
	                /* add rsp, 32 */                         IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

	                /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                        IB(0x75); IB(0x03);
	                /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
	        }
	} else {
	        /* if (!rt_storesymbol_helper(rt, dst, src)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* movq %r14, %rdi */                     IB(0x4c); IB(0x89); IB(0xf7);
	                /* movabs dst, %rsi */                    IB(0x48); IB(0xbe); IQ(dst);
	                /* movq src, %rdx */                      IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);
	                /* movabs rt_storesymbol_helper, %r8 */   IB(0x49); IB(0xb8); IQ((uint64_t)rt_storesymbol_helper);
	                /* call *%r8 */                           IB(0x41); IB(0xff); IB(0xd0);

	                /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                        IB(0x75); IB(0x03);
	                /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
        	}
	}

        return true;
}

/* Visit a ROP_LOADDOT instruction. */
static NOCT_INLINE bool
jit_visit_loaddot_op(
        struct jit_context *ctx)
{
        int dst;
        int dict;
        const char *field_s;
        uint64_t field;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s);
        field = (uint64_t)(intptr_t)field_s;

	if (IS_MSABI) {
	        /* if (!rt_loaddot_helper(rt, dst, dict, field)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* mov r14 -> rcx (1st arg: rt) */        IB(0x4C); IB(0x89); IB(0xF1);
	                /* mov dst -> rdx (2nd arg) */            IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
	                /* mov dict -> r8 (3rd arg) */            IB(0x49); IB(0xB8); IQ((uint64_t)dict);
	                /* movabs field -> r9 (4th arg) */        IB(0x49); IB(0xB9); IQ((uint64_t)field);
	                /* movabs rt_loaddot_helper -> rax */     IB(0x48); IB(0xB8); IQ((uint64_t)rt_loaddot_helper);
	                /* sub rsp, 32 (shadow space) */          IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
	                /* call rax */                            IB(0xFF); IB(0xD0);
	                /* add rsp, 32 */                         IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

                        /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_loaddot_helper(rt, dst, dict, field)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r14: &rt->frame->tmpvar[0] */

                        /* movq %r14, %rdi */                     IB(0x4c); IB(0x89); IB(0xf7);
                        /* movq dst, %rsi */                      IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* movq dict, %rdx */                     IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dict);
                        /* movabs field, %rcx */                  IB(0x48); IB(0xb9); IQ(field);
                        /* movabs rt_loaddot_helper, %r8 */       IB(0x49); IB(0xb8); IQ((uint64_t)rt_loaddot_helper);
                        /* call *%r8 */                           IB(0x41); IB(0xff); IB(0xd0);

                        /* cmpl $0, %eax */                       IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
	}

        return true;
}

/* Visit a ROP_STOREDOT instruction. */
static NOCT_INLINE bool
jit_visit_storedot_op(
        struct jit_context *ctx)
{
        int dict;
        const char *field_s;
        uint64_t field;
        int src;

        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s);
        CONSUME_TMPVAR(src);
        field = (uint64_t)(intptr_t)field_s;

        if (IS_MSABI) {
                /* if (!jit_storedot_helper(rt, dict, field, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r14: &rt->frame->tmpvar[0] */

                        /* mov r14 -> rcx (1st arg: rt) */      IB(0x4C); IB(0x89); IB(0xF1);
                        /* mov dict -> rdx (2nd arg) */         IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dict);
                        /* movabs field -> r8 (3rd arg) */      IB(0x49); IB(0xB8); IQ((uint64_t)field);
                        /* mov src -> r9 (4th arg) */           IB(0x49); IB(0xB9); IQ((uint64_t)src);
                        /* movabs rt_storedot_helper -> rax */  IB(0x48); IB(0xB8); IQ((uint64_t)rt_storedot_helper);
                        /* sub rsp, 32 (shadow space) */        IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
                        /* call rax */                          IB(0xFF); IB(0xD0);
                        /* add rsp, 32 */                       IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

                        /* test eax, eax */                     IB(0x83); IB(0xF8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *r13 */                          IB(0x41); IB(0xFF); IB(0xE5);
                /* next: */
                }
        } else {
                /* if (!jit_storedot_helper(rt, dict, field, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r14: &rt->frame->tmpvar[0] */

                        /* movq %r14, %rdi */                   IB(0x4c); IB(0x89); IB(0xf7);
                        /* movq dict, %rsi */                   IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dict);
                        /* movabs field, %rdx */                IB(0x48); IB(0xba); IQ(field);
                        /* movq src, %rcx */                    IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src);
                        /* movabs rt_storedot_helper, %r8 */    IB(0x49); IB(0xb8); IQ((uint64_t)rt_storedot_helper);
                        /* call *%r8 */                         IB(0x41); IB(0xff); IB(0xd0);

                        /* cmpl $0, %eax */                     IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *%r13 */                         IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
        }

        return true;
}

/* Visit a ROP_CALL instruction. */
static inline bool
jit_visit_call_op(
        struct jit_context *ctx)
{
        int dst;
        int func;
        int arg_count;
        int arg_tmp;
        int arg[NOCT_ARG_MAX];
        uint64_t arg_addr;
        int i;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(func);
        CONSUME_IMM8(arg_count);
        for (i = 0; i < arg_count; i++) {
                CONSUME_TMPVAR(arg_tmp);
                arg[i] = arg_tmp;
        }

        /* Embed arguments to the code. */
        if (arg_count > 0) {
                ASM {
                        /* jmp (5 + arg_count * 4) */
                        IB(0xe9);
                        ID((uint32_t)(4 * arg_count));
                }
                arg_addr = (uint64_t)(intptr_t)ctx->code;
                for (i = 0; i < arg_count; i++) {
                        *(int *)ctx->code = arg[i];
                        ctx->code = (uint8_t *)ctx->code + 4;
                }
        } else {
                arg_addr = 0;
        }

	if (IS_MSABI) {
	        /* if (!rt_call_helper(rt, dst, func, arg_count, arg)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* mov r14 -> rcx (1st arg: rt) */       IB(0x4C); IB(0x89); IB(0xF1);
	                /* mov dst -> rdx (2nd arg) */           IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
	                /* mov func -> r8 (3rd arg) */           IB(0x49); IB(0xC7); IB(0xC0); ID((uint32_t)func);
	                /* mov arg_count -> r9 (4th arg) */      IB(0x49); IB(0xC7); IB(0xC1); ID((uint32_t)arg_count);
	                /* shadow space */                       IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
	                /* movabs rax, arg_addr */               IB(0x48); IB(0xB8); IQ((uint64_t)arg_addr);
	                /* mov [rsp+32], rax (5th arg) */        IB(0x48); IB(0x89); IB(0x44); IB(0x24); IB(0x20);
	                /* movabs rt_call_helper -> rax */       IB(0x48); IB(0xB8); IQ((uint64_t)rt_call_helper);
	                /* call rax */                           IB(0xFF); IB(0xD0);
	                /* add rsp, 32 (shadow) */               IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

	                /* test eax, eax */                      IB(0x83); IB(0xF8); IB(0x00);
	                /* jne 8 <next> */                       IB(0x75); IB(0x03);
	                /* jmp *r13 */                           IB(0x41); IB(0xFF); IB(0xE5);
	        /* next: */
	        }
	} else {
                /* if (!rt_call_helper(rt, dst, func, arg_count, arg)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: rt */
                        /* r14: &rt->frame->tmpvar[0] */

                        /* movq %r14, %rdi */                    IB(0x4c); IB(0x89); IB(0xf7);
                        /* movq dst, %rsi */                     IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* movq func, %rdx */                    IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)func);
                        /* movq arg_count, %rcx */               IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)arg_count);
                        /* movabs arg_addr, %r8 */               IB(0x49); IB(0xb8); IQ(arg_addr);
                        /* movabs rt_call_helper, %r9 */         IB(0x49); IB(0xb9); IQ((uint64_t)rt_call_helper);
                        /* call *%r9 */                          IB(0x41); IB(0xff); IB(0xd1);

                        /* cmpl $0, %eax */                      IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                       IB(0x75); IB(0x03);
                        /* jmp *%r13 */                          IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
        }

        return true;
}

/* Visit a ROP_THISCALL instruction. */
static inline bool
jit_visit_thiscall_op(
        struct jit_context *ctx)
{
        int dst;
        int obj;
        const char *symbol;
        int arg_count;
        int arg_tmp;
        int arg[NOCT_ARG_MAX];
        uint64_t arg_addr;
        int i;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(obj);
        CONSUME_STRING(symbol);
        CONSUME_IMM8(arg_count);
        for (i = 0; i < arg_count; i++) {
                CONSUME_TMPVAR(arg_tmp);
                arg[i] = arg_tmp;
        }

        /* Embed arguments to the code. */
        ASM {
                /* jmp (5 + arg_count * 4) */
                IB(0xe9);
                ID((uint32_t)(4 * arg_count));
        }
        arg_addr = (uint64_t)(intptr_t)ctx->code;
        for (i = 0; i < arg_count; i++) {
                *(int *)ctx->code = arg[i];
                ctx->code = (uint8_t *)ctx->code + 4;
        }

	if (IS_MSABI) {
	        /* if (!rt_thiscall_helper(rt, dst, obj, symbol, arg_count, arg)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* mov r14 -> rcx (1st arg: rt) */             IB(0x4C); IB(0x89); IB(0xF1);
	                /* mov dst -> rdx (2nd arg) */                 IB(0x48); IB(0xC7); IB(0xC2); ID((uint32_t)dst);
	                /* mov obj -> r8 (3rd arg) */                  IB(0x49); IB(0xC7); IB(0xC0); ID((uint32_t)obj);
	                /* mov symbol -> r9 (4th arg) */               IB(0x49); IB(0xB9); IQ((uint64_t)symbol);
	                /* shadow space */                             IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
	                /* mov arg_count -> [rsp+0x20] (5th arg) */    IB(0xC7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)arg_count);
	                /* movabs arg_addr -> rax */                   IB(0x48); IB(0xB8); IQ((uint64_t)arg_addr);
	                /* mov  rax -> [rsp+0x28] (6th arg) */         IB(0x48); IB(0x89); IB(0x44); IB(0x24); IB(0x28);
	                /* movabs rt_thiscall_helper -> rax */         IB(0x48); IB(0xB8); IQ((uint64_t)rt_thiscall_helper);
	                /* call rax */                                 IB(0xFF); IB(0xD0);
	                /* add rsp, 32 (shadow) */                     IB(0x48); IB(0x83); IB(0xC4); IB(0x20);

	                /* test eax, eax */                            IB(0x83); IB(0xF8); IB(0x00);
	                /* jne 8 <next> */                             IB(0x75); IB(0x03);
	                /* jmp *r13 */                                 IB(0x41); IB(0xFF); IB(0xE5);
                /* next: */
	        }
	} else {
	        /* if (!rt_thiscall_helper(rt, dst, obj, symbol, arg_count, arg)) return false; */
	        ASM {
	                /* r13: exception_handler */
	                /* r14: rt */
	                /* r14: &rt->frame->tmpvar[0] */

	                /* movq %r14, %rdi */                         IB(0x4c); IB(0x89); IB(0xf7);
	                /* movq dst, %rsi */                          IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
	                /* movq obj, %rdx */                          IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)obj);
	                /* movabs symbol, %rcx */                     IB(0x48); IB(0xb9); IQ((uint64_t)symbol);
	                /* movq arg_count, %r8 */                     IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)arg_count);
	                /* movabs arg_addr, %r9 */                    IB(0x49); IB(0xb9); IQ(arg_addr);
	                /* movabs rt_thiscall_helper, %r10 */         IB(0x49); IB(0xba); IQ((uint64_t)rt_thiscall_helper);
	                /* call *%r10 */                              IB(0x41); IB(0xff); IB(0xd2);

	                /* cmpl $0, %eax */                           IB(0x83); IB(0xf8); IB(0x00);
	                /* jne 8 <next> */                            IB(0x75); IB(0x03);
	                /* jmp *%r13 */                               IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
	        }
	}

        return true;
}

/* Visit a ROP_JMP instruction. */
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
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JMP;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* jmp 5 */        IB(0xe9); ID(0);
        }

        return true;
}

/* Visit a ROP_JMPIFTRUE instruction. */
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
                /* rdx = &rt->frame->tmpvar[src] */
                /* movq %r14, %rcx */              IB(0x4c); IB(0x89); IB(0xf1);
                /* movq src, %rdx */               IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);
                /* shlq $4, %rdx */                IB(0x48); IB(0xc1); IB(0xe2); IB(0x04);
                /* addq %r15, %rdx */              IB(0x4c); IB(0x01); IB(0xfa);
                /* movl 8(%rdx), %eax */           IB(0x8b); IB(0x42); IB(0x08);

                /* Compare: rt->frame->tmpvar[dst].val.i == 0 */
                /* test %eax, %eax */                        IB(0x85); IB(0xc0);
        }

        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JNE;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* jne 6 */                                IB(0x0f); IB(0x85); ID(0);
        }

        return true;
}

/* Visit a ROP_JMPIFFALSE instruction. */
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
                /* rdx = &rt->frame->tmpvar[src] */
                /* movq %r14, %rcx */              IB(0x4c); IB(0x89); IB(0xf1);
                /* movq src, %rdx */               IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);
                /* shlq $4, %rdx */                IB(0x48); IB(0xc1); IB(0xe2); IB(0x04);
                /* addq %r15, %rdx */              IB(0x4c); IB(0x01); IB(0xfa);
                /* movl 8(%rdx), %eax */           IB(0x8b); IB(0x42); IB(0x08);

                /* Compare: rt->frame->tmpvar[dst].val.i == 0 */
                /* test %eax, %eax */                        IB(0x85); IB(0xc0);
        }

        /* Patch later. */
        ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
        ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JE;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* je 6 */                                IB(0x0f); IB(0x84); ID(0);
        }

        return true;
}

/* Visit a ROP_JMPIFEQ instruction. */
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
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JE;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* je 6 */                                IB(0x0f); IB(0x84); ID(0);
        }

        return true;
}

/* Visit a bytecode of a function. */
bool
jit_visit_bytecode(
        struct jit_context *ctx)
{
        uint8_t opcode;

	if (IS_MSABI) {
	        /* Put a prologue. */
	        ASM {
	        /* prologue: */
	                /* pushq %rax */                        IB(0x50);
	                /* pushq %rbx */                        IB(0x53);
	                /* pushq %rcx */                        IB(0x51);
	                /* pushq %rdx */                        IB(0x52);
	                /* pushq %rdi */                        IB(0x57);
	                /* pushq %rsi */                        IB(0x56);
	                /* pushq %r13 */                        IB(0x41); IB(0x55);
	                /* pushq %r14 */                        IB(0x41); IB(0x56);
	                /* pushq %r15 */                        IB(0x41); IB(0x57);

	                /* align stack to 16 bytes (push count=9 -> misaligned) */
	                /* sub rsp, 8 */                        IB(0x48); IB(0x83); IB(0xEC); IB(0x08);

	                /* r14 = env */
	                /* movq %rcx, %r14 */                   IB(0x49); IB(0x89); IB(0xCE);

	                /* r15 = *&env->frame->tmpvar[0] */
	                /* movq (%r14), %rax */                 IB(0x49); IB(0x8b); IB(0x06);
	                /* movq (%rax), %r15 */                 IB(0x4c); IB(0x8b); IB(0x38);

	                /* r13 = exception_handler */
	                /* movabs (ctx->code + 10), %r13 */     IB(0x49); IB(0xbd); IQ((uint64_t)(intptr_t)((uint8_t*)ctx->code + 10));

	                /* Skip an exception handler. */
	                /* jmp exception_handler_end */         IB(0xeb); IB(0x18);
	        }

	        /* Put an exception handler. */
	        ctx->exception_code = ctx->code;
	        ASM {
	        /* exception_handler: */
	                /* addq $8, %rsp (align back) */        IB(0x48); IB(0x83); IB(0xC4); IB(0x08);
	                /* popq %r15 */                         IB(0x41); IB(0x5f);
	                /* popq %r14 */                         IB(0x41); IB(0x5e);
	                /* popq %r13 */                         IB(0x41); IB(0x5d);
	                /* popq %rsi */                         IB(0x5e);
	                /* popq %rdi */                         IB(0x5f);
	                /* popq %rdx */                         IB(0x5a);
	                /* popq %rcx */                         IB(0x59);
	                /* popq %rbx */                         IB(0x5b);
	                /* popq %rax */                         IB(0x58);
	                /* movq $0, %rax */                     IB(0x48); IB(0xc7); IB(0xc0); ID(0);
	                /* ret */                               IB(0xc3);
	        /* exception_handler_end: */
	        }
	} else {
	        /* Put a prologue. */
	        ASM {
	        /* prologue: */
	                /* pushq %rax */                        IB(0x50);
	                /* pushq %rbx */                        IB(0x53);
	                /* pushq %rcx */                        IB(0x51);
	                /* pushq %rdx */                        IB(0x52);
	                /* pushq %rdi */                        IB(0x57);
	                /* pushq %rsi */                        IB(0x56);
	                /* pushq %r13 */                        IB(0x41); IB(0x55);
	                /* pushq %r14 */                        IB(0x41); IB(0x56);
	                /* pushq %r15 */                        IB(0x41); IB(0x57);

	                /* r14 = rt */
	                /* movq %rdi, %r14 */                   IB(0x49); IB(0x89); IB(0xfe);

	                /* r15 = *&rt->frame->tmpvar[0] */
	                /* movq (%r14), %rax */                 IB(0x49); IB(0x8b); IB(0x06);
	                /* movq (%rax), %r15 */                 IB(0x4c); IB(0x8b); IB(0x38);

	                /* r13 = exception_handler */
	                /* movabs (ctx->code + 10), %r13 */     IB(0x49); IB(0xbd); IQ((uint64_t)(intptr_t)((uint8_t*)ctx->code + 10));

	                /* Skip an exception handler. */
	                /* jmp exception_handler_end */         IB(0xeb); IB(0x14);
	        }

	        /* Put an exception handler. */
	        ctx->exception_code = ctx->code;
	        ASM {
	        /* exception_handler: */
	                /* popq %r15 */                         IB(0x41); IB(0x5f);
	                /* popq %r14 */                         IB(0x41); IB(0x5e);
	                /* popq %r13 */                         IB(0x41); IB(0x5d);
	                /* popq %rsi */                         IB(0x5e);
	                /* popq %rdi */                         IB(0x5f);
	                /* popq %rdx */                         IB(0x5a);
	                /* popq %rcx */                         IB(0x59);
	                /* popq %rbx */                         IB(0x5b);
	                /* popq %rax */                         IB(0x58);
	                /* movq $0, %rax */                     IB(0x48); IB(0xc7); IB(0xc0); ID(0);
	                /* ret */                               IB(0xc3);
	        /* exception_handler_end: */
	        }
	}

        /* Put a body. */
        while (ctx->lpc < ctx->func->bytecode_size) {
                /* Save LPC and addr. */
                if (ctx->pc_entry_count >= PC_ENTRY_MAX) {
                        rt_error(ctx->env, "Too big code.");
                        return false;
                }
                ctx->pc_entry[ctx->pc_entry_count].lpc = (uint32_t)ctx->lpc;
                ctx->pc_entry[ctx->pc_entry_count].code = ctx->code;
                ctx->pc_entry_count++;

                /* Dispatch by opcode. */
                CONSUME_OPCODE(opcode);
                switch (opcode) {
                case ROP_LINEINFO:
                        if (!jit_visit_lineinfo_op(ctx))
                                return false;
                        break;
                case ROP_ASSIGN:
                        if (!jit_visit_assign_op(ctx))
                                return false;
                        break;
                case ROP_ICONST:
                        if (!jit_visit_iconst_op(ctx))
                                return false;
                        break;
                case ROP_FCONST:
                        if (!jit_visit_fconst_op(ctx))
                                return false;
                        break;
                case ROP_SCONST:
                        if (!jit_visit_sconst_op(ctx))
                                return false;
                        break;
                case ROP_ACONST:
                        if (!jit_visit_aconst_op(ctx))
                                return false;
                        break;
                case ROP_DCONST:
                        if (!jit_visit_dconst_op(ctx))
                                return false;
                        break;
                case ROP_INC:
                        if (!jit_visit_inc_op(ctx))
                                return false;
                        break;
                case ROP_ADD:
                        if (!jit_visit_add_op(ctx))
                                return false;
                        break;
                case ROP_SUB:
                        if (!jit_visit_sub_op(ctx))
                                return false;
                        break;
                case ROP_MUL:
                        if (!jit_visit_mul_op(ctx))
                                return false;
                        break;
                case ROP_DIV:
                        if (!jit_visit_div_op(ctx))
                                return false;
                        break;
                case ROP_MOD:
                        if (!jit_visit_mod_op(ctx))
                                return false;
                        break;
                case ROP_AND:
                        if (!jit_visit_and_op(ctx))
                                return false;
                        break;
                case ROP_OR:
                        if (!jit_visit_or_op(ctx))
                                return false;
                        break;
                case ROP_XOR:
                        if (!jit_visit_xor_op(ctx))
                                return false;
                        break;
                case ROP_NEG:
                        if (!jit_visit_neg_op(ctx))
                                return false;
                        break;
                case ROP_NOT:
                        if (!jit_visit_not_op(ctx))
                                return false;
                        break;
                case ROP_LT:
                        if (!jit_visit_lt_op(ctx))
                                return false;
                        break;
                case ROP_LTE:
                        if (!jit_visit_lte_op(ctx))
                                return false;
                        break;
                case ROP_EQ:
                        if (!jit_visit_eq_op(ctx))
                                return false;
                        break;
                case ROP_NEQ:
                        if (!jit_visit_neq_op(ctx))
                                return false;
                        break;
                case ROP_GTE:
                        if (!jit_visit_gte_op(ctx))
                                return false;
                        break;
                case ROP_GT:
                        if (!jit_visit_gt_op(ctx))
                                return false;
                        break;
                case ROP_EQI:
                        if (!jit_visit_eqi_op(ctx))
                                return false;
                        break;
                case ROP_LOADARRAY:
                        if (!jit_visit_loadarray_op(ctx))
                                return false;
                        break;
                case ROP_STOREARRAY:
                        if (!jit_visit_storearray_op(ctx))
                                return false;
                        break;
                case ROP_LEN:
                        if (!jit_visit_len_op(ctx))
                        return false;
                        break;
                case ROP_GETDICTKEYBYINDEX:
                        if (!jit_visit_getdictkeybyindex_op(ctx))
                        return false;
                        break;
                case ROP_GETDICTVALBYINDEX:
                        if (!jit_visit_getdictvalbyindex_op(ctx))
                                return false;
                        break;
                case ROP_LOADSYMBOL:
                        if (!jit_visit_loadsymbol_op(ctx))
                                return false;
                        break;
                case ROP_STORESYMBOL:
                        if (!jit_visit_storesymbol_op(ctx))
                                return false;
                        break;
                case ROP_LOADDOT:
                        if (!jit_visit_loaddot_op(ctx))
                                return false;
                        break;
                case ROP_STOREDOT:
                        if (!jit_visit_storedot_op(ctx))
                                return false;
                        break;
                case ROP_CALL:
                        if (!jit_visit_call_op(ctx))
                                return false;
                        break;
                case ROP_THISCALL:
                        if (!jit_visit_thiscall_op(ctx))
                                return false;
                        break;
                case ROP_JMP:
                        if (!jit_visit_jmp_op(ctx))
                                return false;
                        break;
                case ROP_JMPIFTRUE:
                        if (!jit_visit_jmpiftrue_op(ctx))
                                return false;
                        break;
                case ROP_JMPIFFALSE:
                        if (!jit_visit_jmpiffalse_op(ctx))
                                return false;
                        break;
                case ROP_JMPIFEQ:
                        if (!jit_visit_jmpifeq_op(ctx))
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

	if (IS_MSABI) {
                /* Put an epilogue. */
                ASM {
                /* epilogue: */
                        /* addq $8, %rsp (align back) */ IB(0x48); IB(0x83); IB(0xC4); IB(0x08);
                        /* popq %r15 */                  IB(0x41); IB(0x5f);
                        /* popq %r14 */                  IB(0x41); IB(0x5e);
                        /* popq %r13 */                  IB(0x41); IB(0x5d);
                        /* popq %rsi */                  IB(0x5e);
                        /* popq %rdi */                  IB(0x5f);
                        /* popq %rdx */                  IB(0x5a);
                        /* popq %rcx */                  IB(0x59);
                        /* popq %rbx */                  IB(0x5b);
                        /* popq %rax */                  IB(0x58);
                        /* movq $1, %rax */              IB(0x48); IB(0xc7); IB(0xc0); ID(1);
                        /* ret */                        IB(0xc3);
                }
        } else {
                /* Put an epilogue. */
                ASM {
                /* epilogue: */
                        /* popq %r15 */                  IB(0x41); IB(0x5f);
                        /* popq %r14 */                  IB(0x41); IB(0x5e);
                        /* popq %r13 */                  IB(0x41); IB(0x5d);
                        /* popq %rsi */                  IB(0x5e);
                        /* popq %rdi */                  IB(0x5f);
                        /* popq %rdx */                  IB(0x5a);
                        /* popq %rcx */                  IB(0x59);
                        /* popq %rbx */                  IB(0x5b);
                        /* popq %rax */                  IB(0x58);
                        /* movq $1, %rax */              IB(0x48); IB(0xc7); IB(0xc0); ID(1);
                        /* ret */                        IB(0xc3);
                }
	}

        return true;
}

static bool
jit_patch_branch(
    struct jit_context *ctx,
    int patch_index)
{
        uint8_t *target_code;
        int offset;
        int i;

        /* Search a code addr at lpc. */
        target_code = NULL;
        for (i = 0; i < ctx->pc_entry_count; i++) {
                if (ctx->pc_entry[i].lpc == ctx->branch_patch[patch_index].lpc) {
                        target_code = (uint8_t *)ctx->pc_entry[i].code;
                        break;
                }
                        
        }
        if (target_code == NULL) {
                rt_error(ctx->env, "Branch target not found.");
                return false;
        }

        /* Calc a branch offset. */
        offset = (int)((intptr_t)target_code - (intptr_t)ctx->branch_patch[patch_index].code);

        /* Set the assembler cursor. */
        ctx->code = ctx->branch_patch[patch_index].code;

        /* Assemble. */
        if (ctx->branch_patch[patch_index].type == PATCH_JMP) {
                offset -= 5;
                ASM {
                        /* jmp offset */
                        IB(0xe9);
                        ID((uint32_t)offset);
                }
        } else if (ctx->branch_patch[patch_index].type == PATCH_JE) {
                offset -= 6;
                ASM {
                        /* je offset */
                        IB(0x0f);
                        IB(0x84);
                        ID((uint32_t)offset);
                }
        } else if (ctx->branch_patch[patch_index].type == PATCH_JNE) {
                offset -= 6;
                ASM {
                        /* jne offset */
                        IB(0x0f);
                        IB(0x85);
                        ID((uint32_t)offset);
                }
        }

        return true;
}

#endif /* defined(ARCH_X86_64) && defined(USE_JIT) */
