/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: nil; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * JIT (x86_64): Just-In-Time native code generation
 */

#include <noct/noct.h>

#if defined(NOCT_ARCH_X86_64) && defined(NOCT_USE_JIT)

#include <noct/noct.h>
#include "runtime.h"
#include "jit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

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

/* Forward declaration */
static bool jit_visit_bytecode(struct jit_context *ctx);
static bool jit_patch_branch(struct jit_context *ctx, int patch_index);
static uint32_t jit_detect_simd_caps(void);

/*
 * Generate a JIT-compiled code for a function.
 */
bool
jit_build(
          struct rt_env *env,
          struct rt_func *func)
{
        struct jit_context ctx;
	struct jit_slab *slab;
	void *code_top;
	void *code_end;
	void *generated_end;
	int attempt;
        int i;

	for (attempt = 0; attempt < 2; attempt++) {
		if (!jit_slab_acquire(env, &slab, &code_top, &code_end))
			return false;

		/* A failed attempt never advances slab->current, so the whole
		 * function can be regenerated without invalid branch patches. */
		memset(&ctx, 0, sizeof(struct jit_context));
		ctx.code_top = code_top;
		ctx.code_end = code_end;
		ctx.code = ctx.code_top;
		ctx.env = env;
		ctx.func = func;
		jit_configure_simd(&ctx, jit_detect_simd_caps(), "x86_64");

		if (!jit_visit_bytecode(&ctx)) {
			if (ctx.code_overflow && attempt == 0 &&
			    ((uint8_t *)code_top != slab->base ||
			     slab->size < jit_get_code_size(env))) {
				jit_slab_abandon(env, slab);
				jit_slab_clear_overflow(env);
				continue;
			}
			return false;
		}

		generated_end = ctx.code;
		for (i = 0; i < ctx.branch_patch_count; i++) {
			if (!jit_patch_branch(&ctx, i))
				return false;
		}
		jit_slab_finish(env, slab, generated_end);
		if (getenv("NOCT_JIT_CODEGEN_DEBUG") != NULL) {
			fprintf(stderr,
				"noct-jit-codegen: x86_64: func=%s bytes=%lu\n",
				func->name != NULL ? func->name : "?",
				(unsigned long)((uint8_t *)generated_end -
						(uint8_t *)ctx.code_top));
		}

		func->jit_code = (bool (*)(struct rt_env *))ctx.code_top;
		return true;
	}
	return false;
}

/*
 * Free all JIT-compiled code.
 */
void
jit_free(
         struct rt_env *env)
{
	jit_slab_free_all(env);
}

/*
 * Commit written code.
 */
void
jit_commit(
        struct rt_env *env)
{
	jit_slab_commit_all(env);
}

/*
 * Assembler output functions
 */

/* Serif */
#define ASM

/* Put a instruction byte. */
#define IB(b)                        if (!jit_put_byte(ctx, b)) return false
static INLINE bool
jit_put_byte(
        struct jit_context *ctx,
        uint8_t b)
{
        if ((uint8_t *)ctx->code + 1 > (uint8_t *)ctx->code_end) {
		ctx->code_overflow = true;
                rt_error(ctx->env, "Code too big.");
                return false;
        }

        *(uint8_t *)ctx->code = b;
        ctx->code = (uint8_t *)ctx->code + 1;

        return true;
}

/* Put a instruction double word. */
#define ID(d)                        if (!jit_put_dword(ctx, d)) return false
static INLINE bool
jit_put_dword(
        struct jit_context *ctx,
        uint32_t dw)
{
        if ((uint8_t *)ctx->code + 4 > (uint8_t *)ctx->code_end) {
		ctx->code_overflow = true;
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
static INLINE bool
jit_put_qword(
        struct jit_context *ctx,
        uint64_t qw)
{
        if ((uint8_t *)ctx->code + 8 > (uint8_t *)ctx->code_end) {
		ctx->code_overflow = true;
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
        /* if (!f(env, dst, src1, src2)) return false; */                                                           \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: env */                                                                                          \
            /* r15: &env->frame->tmpvar[0] */                                                                       \
                                                                                                                    \
            /* subq %rsp, 32 */              IB(0x48); IB(0x83); IB(0xec); IB(0x20);                                \
            /* (1st) movq %r14 -> %rcx */    IB(0x4c); IB(0x89); IB(0xf1);                                          \
            /* (2nd) movq dst -> %rdx */     IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);                       \
            /* (3rd) movq src1 -> %r8 */     IB(0x49); IB(0xb8); IQ((uint64_t)src1);                                \
            /* (4th) movq src2 -> %r9 */     IB(0x49); IB(0xb9); IQ((uint64_t)src2);                                \
            /* movabs f -> %rax */           IB(0x48); IB(0xb8); IQ((uint64_t)f);                                   \
            /* call *%rax */                 IB(0xff); IB(0xd0);                                                    \
            /* addq %rsp, 32 */              IB(0x48); IB(0x83); IB(0xc4); IB(0x20);                                \
                                                                                                                    \
            /* testl %eax, %eax */           IB(0x83); IB(0xF8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xFF); IB(0xE5);                                          \
        /* next: */                                                                                                 \
        }                                                                                                           \
    } else {                                                                                                        \
        /* if (!f(env, dst, src1, src2)) return false; */                                                           \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: env */                                                                                          \
            /* r15: &env->frame->tmpvar[0] */                                                                       \
                                                                                                                    \
            /* (1st) movq %r14 -> %rdi */    IB(0x4c); IB(0x89); IB(0xf7);                                          \
            /* (2st) movq dst -> %rsi */     IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);                       \
            /* (3rd) movq src1 -> %rdx */    IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src1);                      \
            /* (4th) movq src2 -> %rcx */    IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src2);                      \
            /* movabs f -> %rax */           IB(0x48); IB(0xb8); IQ((uint64_t)f);                                   \
            /* call *%rax */                 IB(0xff); IB(0xd0);                                                    \
                                                                                                                    \
            /* testl %eax, %eax */           IB(0x83); IB(0xF8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xff); IB(0xe5);                                          \
            /* next: */                                                                                             \
        }                                                                                                           \
    }

#define ASM_UNARY_OP(f)                                                                                             \
    if (IS_MSABI) {                                                                                                 \
        /* if (!f(env, dst, src)) return false; */                                                                  \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: env */                                                                                          \
            /* r15: &env->frame->tmpvar[0] */                                                                       \
                                                                                                                    \
            /* subq %rsp, 32 */              IB(0x48); IB(0x83); IB(0xec); IB(0x20);                                \
            /* (1st) mov %r14 -> %rcx */     IB(0x4c); IB(0x89); IB(0xf1);                                          \
            /* (2nd) mov dst -> %rdx */      IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);                       \
            /* (3rd) mov src -> %r8 */       IB(0x49); IB(0xb8); IQ((uint64_t)src);                                 \
            /* movabs f -> %rax */           IB(0x48); IB(0xb8); IQ((uint64_t)f);                                   \
            /* call *%rax */                 IB(0xff); IB(0xd0);                                                    \
            /* addq %rsp, 32 */              IB(0x48); IB(0x83); IB(0xc4); IB(0x20);                                \
                                                                                                                    \
            /* testl %eax, %eax */           IB(0x83); IB(0xf8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xff); IB(0xe5);                                          \
            /* next:*/                                                                                              \
        /* next: */                                                                                                 \
        }                                                                                                           \
    } else {                                                                                                        \
        /* if (!f(env, dst, src)) return false; */                                                                  \
        ASM {                                                                                                       \
            /* r13: exception_handler */                                                                            \
            /* r14: env */                                                                                          \
            /* r15: &env->frame->tmpvar[0] */                                                                       \
                                                                                                                    \
            /* (1st) movq %r14 -> %rdi */    IB(0x4c); IB(0x89); IB(0xf7);                                          \
            /* (2nd) movq dst -> %rsi */     IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);                       \
            /* (3rd) movq src -> %rdx */     IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);                       \
            /* movabs f -> %r8 */            IB(0x49); IB(0xb8); IQ((uint64_t)f);                                   \
            /* call *%r8 */                  IB(0x41); IB(0xff); IB(0xd0);                                          \
                                                                                                                    \
            /* testl %eax, %eax */           IB(0x83); IB(0xf8); IB(0x00);                                          \
            /* jne 8 <next> */               IB(0x75); IB(0x03);                                                    \
            /* jmp *%r13 */                  IB(0x41); IB(0xff); IB(0xe5);                                          \
        /* next:*/                                                                                                  \
        }                                                                                                           \
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

        /* env->line = line; */
        ASM {
                /* r13: exception_handler */
                /* r14: evn */
                /* r15: &env->frame->tmpvar[0] */

                /* movl line -> %rax */              IB(0x48); IB(0xc7); IB(0xc0); ID(line);
                /* movq %rax -> [%r14 + 8] */        IB(0x49); IB(0x89); IB(0x46); IB(0x08);
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
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* movq $dst -> %rax */           IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* movq $src -> %rbx */           IB(0x48); IB(0xc7); IB(0xc3); ID((uint32_t)src);
                /* addq %rax -> %r15  */          IB(0x4c); IB(0x01); IB(0xf8);
                /* addq %rbx -> %r15  */          IB(0x4c); IB(0x01); IB(0xfb);
                /* movq (%rbx) -> %rcx */         IB(0x48); IB(0x8b); IB(0x0b);
                /* movq 8(%rbx) -> %rdx */        IB(0x48); IB(0x8b); IB(0x53); IB(0x08);
                /* movq %rcx -> (%rax) */         IB(0x48); IB(0x89); IB(0x08);
                /* movq %rdx -> 8(%rax) */        IB(0x48); IB(0x89); IB(0x50); IB(0x08);
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

        /* env->frame->tmpvar[dst].type = NOCT_VALUE_INT; */
        /* env->frame->tmpvar[dst].val.i = val; */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* %rax = &env->frame->tmpvar[dst] */
                /* movq $dst -> %rax */            IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */            IB(0x4c); IB(0x01); IB(0xf8);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_INT */
                /* movl $0 -> (%rax) */            IB(0xc7); IB(0x00); ID(0);

                /* env->frame->tmpvar[dst].val.i = val */
                /* movl $val ->8 (%rax) */         IB(0xc7); IB(0x40); IB(0x08); ID((uint32_t)val);
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

        /* env->frame->tmpvar[dst].type = RT_VALUE_LONG; */
        /* env->frame->tmpvar[dst].val.l = val; */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* %rax = &env->frame->tmpvar[dst] */
                /* movq $dst -> %rax */            IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */            IB(0x4c); IB(0x01); IB(0xf8);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_LONG */
                /* movl $5 -> 0(%rax) */           IB(0xc7); IB(0x00); ID(5);

                /* env->frame->tmpvar[dst].val.l = val */
                /* movabs $val -> %rcx */          IB(0x48); IB(0xb9); IQ(val);
                /* movl %rcx -> 8(%rax) */         IB(0x48); IB(0x89); IB(0x48); IB(0x08);
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

        /* &env->frame->tmpvar[dst].type = RT_VALUE_INT; */
        /* &env->frame->tmpvar[dst].val.i = val; */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* movq $dst -> %rax */             IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */             IB(0x4c); IB(0x01); IB(0xf8);
                /* movl $1 -> (%rax) */             IB(0xc7); IB(0x00); ID(1);
                /* movl $val -> 8(%rax) */          IB(0xc7); IB(0x40); IB(0x08); ID(val);
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

        /* env->frame->tmpvar[dst].type = RT_VALUE_DOUBLE; */
        /* env->frame->tmpvar[dst].val.lf = val; */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* %rax = &env->frame->tmpvar[dst] */
                /* movq $dst -> %rax */            IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */            IB(0x4c); IB(0x01); IB(0xf8);

                /* env->frame->tmpvar[dst].type = NOCT_VALUE_DOUBLE */
                /* movl $5 -> 0(%rax) */           IB(0xc7); IB(0x00); ID(6);

                /* env->frame->tmpvar[dst].val.l = val */
                /* movabs $val -> %rcx */          IB(0x48); IB(0xb9); IQ(val);
                /* movl %rcx -> 8(%rax) */         IB(0x48); IB(0x89); IB(0x48); IB(0x08);
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

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(val, len, hash);

        dst *= (int)sizeof(struct rt_value);

        if (IS_MSABI) {
                /* rt_make_string_with_hash(env, &env->frame->tmpvar[dst], val, len, hash); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r15: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                           IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */                  IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */                  IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /*       addq %r15 -> %rdx */                  IB(0x4c); IB(0x01); IB(0xfa);
                        /* (3rd) movabs $val -> %r8 */                 IB(0x49); IB(0xb8); IQ((uint64_t)val);
                        /* (4th) movq $len -> %r9 */                   IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)len);
                        /* (5th) movq $hash -> 32(%rsp) */             IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)hash);
                        /* movabs rt_make_string_with_hash -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)rt_make_string_with_hash);
                        /* call *%rax */                               IB(0xff); IB(0xd0);
                        /* addq %rsp, $64 */                           IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                         IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                             IB(0x75); IB(0x03);
                        /* jmp *%r13 */                                IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* rt_make_string_with_hash(env, &env->frame->tmpvar[dst], val, len, hash); */
                ASM {
                        /* r13 = exception_handler */
                        /* r14 = env */
                        /* r15 = &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */                    IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst, %rsi */                    IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /*       addq %r15, %rsi */                    IB(0x4c); IB(0x01); IB(0xfe);
                        /* (3rd) movabs $val, %rdx */                  IB(0x48); IB(0xba); IQ((uint64_t)val);
                        /* (4th) movq $len -> %rcx */                  IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)len);
                        /* (5th) movq $hash -> %r8 */                  IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)hash);
                        /* movabs rt_make_string_with_hash -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)rt_make_string_with_hash);
                        /* call *%rax */                               IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                         IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                             IB(0x75); IB(0x03);
                        /* jmp *%r13 */                                IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        }

        return true;
}

/* Visit a OP_ACONST instruction. */
static INLINE bool
jit_visit_aconst_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

        if (IS_MSABI) {
                /* rt_make_empty_array(env, &env->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r15: &env->frame->tmpvar[0] */

                        /* subq %rsp, 32 */                       IB(0x48); IB(0x83); IB(0xec); IB(0x20);
                        /* (1st) movq %r14 -> %rcx */             IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */             IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /*       addq %r15 -> %rdx */             IB(0x4c); IB(0x01); IB(0xfa);
                        /* movabs rt_make_empty_array -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)rt_make_empty_array);
                        /* call *%rax */                          IB(0xff); IB(0xd0);
                        /* add %rsp, 32 */                        IB(0x48); IB(0x83); IB(0xc4); IB(0x20);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* rt_make_empty_array(env, &env->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r15: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14 -> %rdi */             IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst -> %rsi */             IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /*       addq %r15 -> %rsi */             IB(0x4c); IB(0x01); IB(0xfe);
                        /* movabs rt_make_empty_array -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)rt_make_empty_array);
                        /* call *%rax */                          IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        }

        return true;
}

/* Visit a OP_DCONST instruction. */
static INLINE bool
jit_visit_dconst_op(
        struct jit_context *ctx)
{
        int dst;

        CONSUME_TMPVAR(dst);

        dst *= (int)sizeof(struct rt_value);

        if (IS_MSABI) {
                /* rt_make_empty_dict(env, &env->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r15: &env->frame->tmpvar[0] */

                        /* subq %rsp, 32 */                     IB(0x48); IB(0x83); IB(0xec); IB(0x20);
                        /* (1st) movq %r14 -> rb%cx */          IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */           IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /*       add %r15 -> %rdx */            IB(0x4c); IB(0x01); IB(0xfa);
                        /* movabs rt_make_empty_dict -> %rax */ IB(0x48); IB(0xb8); IQ((uint64_t)rt_make_empty_dict);
                        /* call *%rax */                        IB(0xff); IB(0xd0);
                        /* addq %rsp, 32 */                     IB(0x48); IB(0x83); IB(0xc4); IB(0x20);

                        /* testl %eax, %eax */                  IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *%r13 */                         IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* rt_make_empty_dict(env, &env->frame->tmpvar[dst]); */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r15: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14 -> %rdi */           IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst, %rsi */             IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /*       addq %r15, %rsi */             IB(0x4c); IB(0x01); IB(0xfe);
                        /* movabs rt_make_empty_dict, %r8 */    IB(0x49); IB(0xb8); IQ((uint64_t)rt_make_empty_dict);
                        /* call *%r8 */                         IB(0x41); IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                  IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                      IB(0x75); IB(0x03);
                        /* jmp *%r13 */                         IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        }

        return true;
}

/* Visit a OP_INC instruction. */
static INLINE bool
jit_visit_inc_op(
        struct jit_context *ctx)
{
        int dst;
        int step;

        CONSUME_TMPVAR(dst);
        CONSUME_IMM8(step);
	if (ctx->vector_hint_active &&
	    dst == ctx->vector_hint_index_tmp &&
	    step == ctx->vector_hint_lanes)
		return true;

        dst *= (int)sizeof(struct rt_value);

        /* &env->frame->tmpvar[dst].val.i++ */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* movq $dst -> %rax */      IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */      IB(0x4c); IB(0x01); IB(0xf8);
                /* addq $step, 8(%rax) */    IB(0x48); IB(0x83); IB(0x40); IB(0x08); IB((uint8_t)step);
        }

	return true;
}

/* x86-64 vector-register encoders.  Keep the logical SIMD operations
 * three-address; legacy SSE lowering uses the same helpers and inserts a
 * copy only when the ISA requires destructive two-address execution.
 * GNU as/objdump high-register oracles used while reviewing these fields:
 *   vpsrld $24,xmm11,xmm10       c4 c1 29 72 d3 18
 *   vmulps xmm12,xmm11,xmm10    c4 41 20 59 d4
 *   vfmadd231ps xmm12,xmm11,xmm10 c4 42 21 b8 d4
 *   paddd xmm12,xmm10           66 45 0f fe d4
 * (operands above are written in AT&T source order). */
static INLINE bool
jit_x86_64_valid_xmm(int reg)
{
	return reg >= 0 && reg < 16;
}

static INLINE bool
jit_x86_64_put_rex_rr(struct jit_context *ctx, int reg, int rm)
{
	uint8_t rex;

	if (!jit_x86_64_valid_xmm(reg) || !jit_x86_64_valid_xmm(rm))
		return false;
	rex = (uint8_t)(0x40 | ((reg & 8) != 0 ? 4 : 0) |
			      ((rm & 8) != 0 ? 1 : 0));
	return rex == 0x40 || jit_put_byte(ctx, rex);
}

/* Legacy SSE register/register instruction.  map is 1 for 0f, 2 for
 * 0f38, and 3 for 0f3a. */
static INLINE bool
jit_x86_64_put_sse_rr(struct jit_context *ctx, uint8_t prefix, int map,
			 uint8_t opcode, int dst, int src)
{
	if (!jit_x86_64_valid_xmm(dst) || !jit_x86_64_valid_xmm(src))
		return false;
	if (prefix != 0 && !jit_put_byte(ctx, prefix))
		return false;
	if (!jit_x86_64_put_rex_rr(ctx, dst, src) ||
	    !jit_put_byte(ctx, 0x0f))
		return false;
	if (map == 2 && !jit_put_byte(ctx, 0x38))
		return false;
	if (map == 3 && !jit_put_byte(ctx, 0x3a))
		return false;
	return jit_put_byte(ctx, opcode) &&
		jit_put_byte(ctx, (uint8_t)(0xc0 | ((dst & 7) << 3) |
					       (src & 7)));
}

static INLINE bool
jit_x86_64_put_vex3(struct jit_context *ctx, int map, int pp, int dst,
			int rm, int src1, bool has_src1)
{
	uint8_t b2, b3;

	if (!jit_x86_64_valid_xmm(dst) || !jit_x86_64_valid_xmm(rm) ||
	    (has_src1 && !jit_x86_64_valid_xmm(src1)))
		return false;
	b2 = (uint8_t)(((dst & 8) == 0 ? 0x80 : 0) | 0x40 |
		       ((rm & 8) == 0 ? 0x20 : 0) | (map & 0x1f));
	b3 = (uint8_t)((has_src1 ? ((~src1 & 15) << 3) : 0x78) |
		       (pp & 3));
	return jit_put_byte(ctx, 0xc4) && jit_put_byte(ctx, b2) &&
		jit_put_byte(ctx, b3);
}

/* VEX.NDS.128 register binary operation: dst = src1 op src2. */
static INLINE bool
jit_x86_64_put_vex_rrr(struct jit_context *ctx, int map, int pp,
			  uint8_t opcode, int dst, int src1, int src2)
{
	if (!jit_x86_64_put_vex3(ctx, map, pp, dst, src2, src1, true))
		return false;
	return jit_put_byte(ctx, opcode) &&
		jit_put_byte(ctx, (uint8_t)(0xc0 | ((dst & 7) << 3) |
					       (src2 & 7)));
}

/* VEX.128 two-operand operation whose vvvv field is reserved. */
static INLINE bool
jit_x86_64_put_vex_rr(struct jit_context *ctx, int map, int pp,
			 uint8_t opcode, int dst, int src)
{
	if (!jit_x86_64_put_vex3(ctx, map, pp, dst, src, 0, false))
		return false;
	return jit_put_byte(ctx, opcode) &&
		jit_put_byte(ctx, (uint8_t)(0xc0 | ((dst & 7) << 3) |
					       (src & 7)));
}

/* VEX immediate packed shift: vvvv encodes dst, ModRM.rm encodes src,
 * and ModRM.reg remains the opcode extension. */
static INLINE bool
jit_x86_64_put_vex_shift(struct jit_context *ctx, int ext, int dst,
			    int src, uint8_t imm)
{
	if (!jit_x86_64_put_vex3(ctx, 1, 1, 0, src, dst, true))
		return false;
	return jit_put_byte(ctx, 0x72) &&
		jit_put_byte(ctx, (uint8_t)(0xc0 | ((ext & 7) << 3) |
					       (src & 7))) &&
		jit_put_byte(ctx, imm);
}

static uint16_t
jit_x86_64_read_u16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t
jit_x86_64_read_u32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Strictly prove the call-free vector region before assigning rbx/rsi/rdi. */
static bool
jit_x86_64_scan_vector_loop(struct jit_context *ctx, int index_tmp,
			    int remaining_tmp, int lanes)
{
	uint32_t p, body_lpc, size;
	uint16_t base, ofs, value;
	int i, inc_count;
	uint8_t op, imm, shift;

	ctx->vector_base_tmp[0] = -1;
	ctx->vector_base_tmp[1] = -1;
	ctx->vector_imm_value = -1;
	ctx->vector_imm_shift = -1;
	body_lpc = ctx->lpc;
	p = body_lpc;
	inc_count = 0;
	while (p < ctx->func->bytecode_size) {
		op = ctx->func->bytecode[p];
		size = 0;
		base = 0xffffu;
		switch (op) {
		case OP_VLOADI32X4:
		case OP_VLOADF32X4:
			if (p + 6 > ctx->func->bytecode_size) return false;
			base = jit_x86_64_read_u16(&ctx->func->bytecode[p + 2]);
			ofs = jit_x86_64_read_u16(&ctx->func->bytecode[p + 4]);
			if (ofs != (uint16_t)index_tmp) return false;
			size = 6; break;
		case OP_VSTOREI32X4:
		case OP_VSTOREF32X4:
			if (p + 6 > ctx->func->bytecode_size) return false;
			base = jit_x86_64_read_u16(&ctx->func->bytecode[p + 1]);
			ofs = jit_x86_64_read_u16(&ctx->func->bytecode[p + 3]);
			if (ofs != (uint16_t)index_tmp) return false;
			size = 6; break;
		case OP_VSPLATI32: case OP_VSPLATF32:
			size = 4; break;
		case OP_VGETLANEI32: case OP_VGETLANEF32:
			size = 5; break;
		case OP_VMOV128: case OP_VCVTI32F32X4:
		case OP_VCVTF32I32X4:
			size = 3; break;
		case OP_VADDI32X4: case OP_VSUBI32X4:
		case OP_VMULI32X4: case OP_VAND128: case OP_VOR128:
		case OP_VXOR128: case OP_VSHLI32X4: case OP_VSHRI32X4:
		case OP_VADDF32X4: case OP_VSUBF32X4:
		case OP_VMULF32X4: case OP_VDIVF32X4:
			size = 4; break;
		case OP_VFMAF32X4:
			if ((ctx->simd_caps & JIT_SIMD_CAP_FMAF32X4) == 0)
				return false;
			size = 5; break;
		case OP_VCMPI32X4:
		case OP_VCMPF32X4:
		case OP_VSELECT128:
			size = 5; break;
		case OP_VMASKSTOREI32X4:
			size = 7; break;
		case OP_VINDUCTF32X4:
			size = 6; break;
		case OP_VGATHERI32X4_CHECKED:
			size = 7; break;
		case OP_VORI32X4I:
			if (p + 5 > ctx->func->bytecode_size) return false;
			imm = ctx->func->bytecode[p + 3];
			shift = ctx->func->bytecode[p + 4];
			if (ctx->vector_imm_value >= 0 &&
			    (ctx->vector_imm_value != imm ||
			     ctx->vector_imm_shift != shift))
				return false;
			ctx->vector_imm_value = imm;
			ctx->vector_imm_shift = shift;
			size = 5; break;
		case OP_INC:
			if (p + 4 > ctx->func->bytecode_size ||
			    jit_x86_64_read_u16(&ctx->func->bytecode[p + 1]) !=
				(uint16_t)index_tmp ||
			    ctx->func->bytecode[p + 3] != (uint8_t)lanes)
				return false;
			inc_count++;
			size = 4; break;
		case OP_SUBJNZ:
			if (p + 8 > ctx->func->bytecode_size) return false;
			value = jit_x86_64_read_u16(&ctx->func->bytecode[p + 1]);
			if (value != (uint16_t)remaining_tmp ||
			    ctx->func->bytecode[p + 3] != (uint8_t)lanes ||
			    jit_x86_64_read_u32(&ctx->func->bytecode[p + 4]) !=
				body_lpc)
				return false;
			return ctx->vector_base_tmp[0] >= 0 && inc_count == 1;
		default:
			return false;
		}
		if (base != 0xffffu) {
			for (i = 0; i < 2; i++) {
				if (ctx->vector_base_tmp[i] == (int)base)
					break;
				if (ctx->vector_base_tmp[i] < 0) {
					ctx->vector_base_tmp[i] = (int)base;
					break;
				}
			}
			if (i == 2) return false;
		}
		p += size;
	}
	return false;
}

static INLINE bool
jit_visit_vindex_hint_op(struct jit_context *ctx)
{
	int a, b, c, required_vregs, lanes, flags;
	int ofs, base_ofs;
	uint32_t imm_value;
	CONSUME_TMPVAR(a); CONSUME_TMPVAR(b); CONSUME_TMPVAR(c);
	CONSUME_IMM8(required_vregs); CONSUME_IMM8(lanes); CONSUME_IMM8(flags);
	if (required_vregs > 13 || (flags & VINDEX_FORCE_SCALAR) != 0)
		ctx->simd_caps = 0;
	ctx->vector_hint_active = !IS_MSABI && lanes > 0 &&
		(ctx->simd_caps & JIT_SIMD_CAP_SSE2) != 0 &&
		(flags & VINDEX_CURSOR_ONLY) != 0 &&
		jit_x86_64_scan_vector_loop(ctx, a, c, lanes);
	ctx->vector_hint_index_tmp = a;
	ctx->vector_hint_stop_tmp = b;
	ctx->vector_hint_remaining_tmp = c;
	ctx->vector_hint_lanes = lanes;
	ctx->vector_hint_flags = flags;
	if (!ctx->vector_hint_active)
		return true;
	/* rax=stop; adjusted bases are raw_base + stop*4. */
	ofs = b * (int)sizeof(struct rt_value);
	ASM { IB(0x49); IB(0x63); IB(0x87); ID((uint32_t)(ofs + 8)); }
	base_ofs = ctx->vector_base_tmp[0] * (int)sizeof(struct rt_value);
	ASM {
		IB(0x49); IB(0x8b); IB(0x9f); ID((uint32_t)(base_ofs + 8));
		IB(0x48); IB(0x8d); IB(0x1c); IB(0x83);
	}
	if (ctx->vector_base_tmp[1] >= 0) {
		base_ofs = ctx->vector_base_tmp[1] *
			(int)sizeof(struct rt_value);
		ASM {
			IB(0x49); IB(0x8b); IB(0xb7); ID((uint32_t)(base_ofs + 8));
			IB(0x48); IB(0x8d); IB(0x34); IB(0x86);
		}
	}
	/* rdi = -(stop-start), sign extended to 64 bits. */
	ofs = c * (int)sizeof(struct rt_value);
	ASM {
		IB(0x49); IB(0x63); IB(0xbf); ID((uint32_t)(ofs + 8));
		IB(0x48); IB(0xf7); IB(0xdf);
	}
	if (ctx->vector_imm_value >= 0) {
		imm_value = (uint32_t)ctx->vector_imm_value <<
			    ((uint32_t)ctx->vector_imm_shift & 31u);
		/* mov imm,eax; movd eax,xmm15; pshufd $0,xmm15,xmm15. */
		ASM {
			IB(0xb8); ID(imm_value);
			IB(0x66); IB(0x44); IB(0x0f); IB(0x6e); IB(0xf8);
			IB(0x66); IB(0x45); IB(0x0f); IB(0x70); IB(0xff); IB(0x00);
		}
	}
	return true;
}

static INLINE bool
jit_visit_subjnz_op(struct jit_context *ctx)
{
	int value, decrement;
	uint32_t target_lpc;
	bool hinted;
	int ofs;

	CONSUME_TMPVAR(value);
	CONSUME_IMM8(decrement);
	CONSUME_IMM32(target_lpc);
	if (target_lpc >= (uint32_t)(ctx->func->bytecode_size + 1)) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	hinted = ctx->vector_hint_active &&
		 value == ctx->vector_hint_remaining_tmp &&
		 decrement == ctx->vector_hint_lanes;
	if (hinted) {
		/* rdi is the negative remaining count: addq lanes; jne body. */
		ASM { IB(0x48); IB(0x83); IB(0xc7); IB((uint8_t)decrement); }
		ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
		ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
		ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JNE;
		ctx->branch_patch_count++;
		ASM { IB(0x0f); IB(0x85); ID(0); }
		/* Preserve bytecode-visible state at loop exit. */
		ofs = value * (int)sizeof(struct rt_value);
		ASM {
			IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)(ofs + 8)); ID(0);
		}
		if ((ctx->vector_hint_flags & VINDEX_WRITEBACK_STOP) != 0) {
			int index_ofs = ctx->vector_hint_index_tmp *
				(int)sizeof(struct rt_value);
			int stop_ofs = ctx->vector_hint_stop_tmp *
				(int)sizeof(struct rt_value);
			ASM {
				IB(0x41); IB(0x8b); IB(0x87); ID((uint32_t)(stop_ofs + 8));
				IB(0x41); IB(0x89); IB(0x87); ID((uint32_t)(index_ofs + 8));
			}
		}
		ctx->vector_hint_active = false;
		return true;
	}
	value *= (int)sizeof(struct rt_value);
	ASM {
		/* rax = &tmpvar[value] */
		IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)value);
		IB(0x4c); IB(0x01); IB(0xf8);
		/* subl $decrement, 8(%rax) */
		IB(0x83); IB(0x68); IB(0x08); IB((uint8_t)decrement);
	}
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JNE;
	ctx->branch_patch_count++;
	ASM { IB(0x0f); IB(0x85); ID(0); }
	return true;
}

static INLINE bool
jit_visit_vori32x4i_op(struct jit_context *ctx)
{
	int dst, src, imm, shift;
	uint32_t value;
	int k;
	int src1, src2;

	CONSUME_IMM8(dst); CONSUME_IMM8(src);
	CONSUME_IMM8(imm); CONSUME_IMM8(shift);
	if (dst < 0 || dst >= 16 || src < 0 || src >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (IS_MSABI || (ctx->simd_caps & JIT_SIMD_CAP_SSE2) == 0) {
		src1 = src;
		src2 = (imm << 8) | shift;
		ASM_BINARY_OP(ex_vori32x4i_helper);
		return true;
	}
	if (dst >= 13 || src >= 13) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (ctx->vector_hint_active &&
	    ctx->vector_imm_value == imm &&
	    ctx->vector_imm_shift == shift) {
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			return jit_x86_64_put_vex_rrr(ctx, 1, 1, 0xeb,
						      dst, src, 15);
		}
		if (dst != src && !jit_x86_64_put_sse_rr(ctx, 0x66, 1,
							  0x6f, dst, src))
			return false;
		/* por xmm15, xmmDst (xmm15 selected by REX.B). */
		return jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xeb, dst, 15);
	}
	if (dst != src && !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f,
							  dst, src))
		return false;
	value = (uint32_t)imm << ((uint32_t)shift & 31);
	/* Use a temporary 16-byte stack constant; no vector register is
	   clobbered and no helper call crosses the live SIMD region. */
	ASM { IB(0x48); IB(0x83); IB(0xec); IB(0x10); }
	for (k = 0; k < 4; k++) {
		if (k == 0) {
			ASM { IB(0xc7); IB(0x04); IB(0x24); ID(value); }
		} else {
			ASM { IB(0xc7); IB(0x44); IB(0x24); IB((uint8_t)(k * 4)); ID(value); }
		}
	}
	ASM { IB(0x66); }
	if ((dst & 8) != 0) { ASM { IB(0x44); } }
	ASM { IB(0x0f); IB(0xeb);
	      IB((uint8_t)(0x04 | ((dst & 7) << 3))); IB(0x24);
	      IB(0x48); IB(0x83); IB(0xc4); IB(0x10); }
	return true;
}

static INLINE bool
jit_visit_vfmaf32x4_op(struct jit_context *ctx)
{
	int dst, src1, src2, addend;
	uint8_t opcode;

	CONSUME_IMM8(dst);
	CONSUME_IMM8(src1);
	CONSUME_IMM8(src2);
	CONSUME_IMM8(addend);
	if (dst < 0 || dst >= 16 || src1 < 0 || src1 >= 16 ||
	    src2 < 0 || src2 >= 16 || addend < 0 || addend >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (IS_MSABI ||
	    (ctx->simd_caps & JIT_SIMD_CAP_FMAF32X4) == 0) {
		int packed_src2_addend = (src2 << 8) | addend;
		src2 = packed_src2_addend;
		ASM_BINARY_OP(ex_vfmaf32x4_helper);
		return true;
	}
	if (dst < 0 || dst >= 13 || src1 < 0 || src1 >= 13 ||
	    src2 < 0 || src2 >= 13 || addend < 0 || addend >= 13) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (dst == addend) {
		/* vfmadd231ps dst, src1, src2 */
		opcode = 0xb8;
		return jit_x86_64_put_vex_rrr(ctx, 2, 1, opcode,
					      dst, src1, src2);
	} else if (dst == src1) {
		/* vfmadd213ps dst, src2, addend */
		opcode = 0xa8;
		return jit_x86_64_put_vex_rrr(ctx, 2, 1, opcode,
					      dst, src2, addend);
	} else if (dst == src2) {
		/* vfmadd213ps dst, src1, addend */
		opcode = 0xa8;
		return jit_x86_64_put_vex_rrr(ctx, 2, 1, opcode,
					      dst, src1, addend);
	} else {
		/* movdqa xmmAddend, xmmDst; vfmadd231ps dst,src1,src2 */
		if (!jit_x86_64_put_vex_rr(ctx, 1, 1, 0x6f, dst, addend))
			return false;
		opcode = 0xb8;
		return jit_x86_64_put_vex_rrr(ctx, 2, 1, opcode,
					      dst, src1, src2);
	}
}

static INLINE bool
jit_visit_vcmp_op(struct jit_context *ctx, bool is_float)
{
	int dst, src1, src2, pred;
	int left, right, imm;
	bool (CDECL *helper)(NoctEnv *, int, int, int);

	CONSUME_IMM8(dst); CONSUME_IMM8(src1);
	CONSUME_IMM8(src2); CONSUME_IMM8(pred);
	if (dst < 0 || dst >= 16 || src1 < 0 || src1 >= 16 ||
	    src2 < 0 || src2 >= 16 || pred < 0 ||
	    pred >= VCMP_PREDICATE_COUNT) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (IS_MSABI || (ctx->simd_caps & JIT_SIMD_CAP_SSE2) == 0) {
		src2 = (src2 << 8) | pred;
		helper = is_float ? ex_vcmpf32x4_helper :
				    ex_vcmpi32x4_helper;
		ASM_BINARY_OP(helper);
		return true;
	}
	if (dst >= 13 || src1 >= 13 || src2 >= 13) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (is_float) {
		left = src1;
		right = src2;
		switch (pred) {
		case VCMP_EQ: imm = 0; break;
		case VCMP_NE: imm = 4; break; /* unordered-or-not-equal */
		case VCMP_LT: imm = 1; break;
		case VCMP_LE: imm = 2; break;
		case VCMP_GT: imm = 1; left = src2; right = src1; break;
		case VCMP_GE: imm = 2; left = src2; right = src1; break;
		default: return false;
		}
		if (!jit_x86_64_put_sse_rr(ctx, 0, 1, 0x28, 13, left) ||
		    !jit_x86_64_put_sse_rr(ctx, 0, 1, 0xc2, 13, right) ||
		    !jit_put_byte(ctx, (uint8_t)imm))
			return false;
	} else {
		left = src1;
		right = src2;
		if (pred == VCMP_LT || pred == VCMP_GE) {
			left = src2;
			right = src1;
		}
		if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f, 13, left))
			return false;
		if (pred == VCMP_EQ || pred == VCMP_NE) {
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x76,
						  13, right))
				return false;
		} else if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x66,
						       13, right)) {
			return false;
		}
		if (pred == VCMP_NE || pred == VCMP_LE || pred == VCMP_GE) {
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x76, 14, 14) ||
			    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xef, 13, 14))
				return false;
		}
	}
	if (dst != 13 && !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f,
						    dst, 13))
		return false;
	return true;
}

static INLINE bool
jit_visit_vselect128_op(struct jit_context *ctx)
{
	int dst, mask, src1, src2;

	CONSUME_IMM8(dst); CONSUME_IMM8(mask);
	CONSUME_IMM8(src1); CONSUME_IMM8(src2);
	if (dst < 0 || dst >= 16 || mask < 0 || mask >= 16 ||
	    src1 < 0 || src1 >= 16 || src2 < 0 || src2 >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (IS_MSABI || (ctx->simd_caps & JIT_SIMD_CAP_SSE2) == 0) {
		src2 = (src1 << 8) | src2;
		src1 = mask;
		ASM_BINARY_OP(ex_vselect128_helper);
		return true;
	}
	if (dst >= 13 || mask >= 13 || src1 >= 13 || src2 >= 13) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	/* xmm13 = mask & true; xmm14 = ~mask & false. */
	if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f, 13, mask) ||
	    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xdb, 13, src1) ||
	    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f, 14, mask) ||
	    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xdf, 14, src2) ||
	    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xeb, 13, 14))
		return false;
	if (dst != 13 && !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f,
						    dst, 13))
		return false;
	return true;
}

static INLINE bool
jit_visit_vmaskstorei32x4_op(struct jit_context *ctx)
{
	int dst, src1, src2;
	int mask;
	CONSUME_TMPVAR(dst); CONSUME_TMPVAR(src1);
	CONSUME_IMM8(src2); CONSUME_IMM8(mask);
	if (src2 < 0 || src2 >= 16 || mask < 0 || mask >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	/* LIR requests 16 logical vregs for this region, so x86_64 reaches
	   this handler only in the memory-canonical direct-scalar tier. */
	if ((ctx->simd_caps & JIT_SIMD_CAP_SSE2) != 0) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	src2 = (src2 << 8) | mask;
	ASM_BINARY_OP(ex_vmaskstorei32x4_helper);
	return true;
}

static INLINE bool
jit_visit_vinductf32x4_op(struct jit_context *ctx)
{
	int dst, src1, src2;
	CONSUME_IMM8(dst); CONSUME_TMPVAR(src1); CONSUME_TMPVAR(src2);
	if (dst < 0 || dst >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	ASM_BINARY_OP(ex_vinductf32x4_helper);
	return true;
}

static INLINE bool
jit_visit_vgatheri32x4_checked_op(struct jit_context *ctx)
{
	int dst, src1, plen, vi, src2;
	CONSUME_IMM8(dst); CONSUME_TMPVAR(src1);
	CONSUME_TMPVAR(plen); CONSUME_IMM8(vi);
	if (dst < 0 || dst >= 16 || vi < 0 || vi >= 16) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	src2 = (plen << 8) | vi;
	ASM_BINARY_OP(ex_vgatheri32x4_checked_helper);
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

        /* if (!rt_add_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_sub_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_mul_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_div_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_mod_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_and_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_or_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_xor_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_shl_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_shr_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_neg_helper(env, dst, src)) return false; */
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

        /* if (!rt_not_helper(env, dst, src)) return false; */
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

        /* if (!rt_lt_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_lte_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_eq_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_neq_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_gte_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_gte_helper);

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

        /* src1 - src2 */
        ASM {
                /* r13: exception_handler */
                /* r14: env */
                /* r15: &env->frame->tmpvar[0] */

                /* movq $dst -> %rax */          IB(0x48); IB(0xc7); IB(0xc0); ID((uint32_t)dst);
                /* addq %r15 -> %rax */          IB(0x4c); IB(0x01); IB(0xf8);

                /* movq $src1 -> %rbx */         IB(0x48); IB(0xc7); IB(0xc3); ID((uint32_t)src1);
                /* addq %r15 -> %rbx */          IB(0x4c); IB(0x01); IB(0xfb);
                
                /* movq $src2 -> %rcx */         IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src2);
                /* addq %r15 -> %rcx */          IB(0x4c); IB(0x01); IB(0xf9);

                /* movq 8(%rbx) -> %rax */       IB(0x48); IB(0x8b); IB(0x43); IB(0x08);
                /* movq 8(%rcx) -> %rdx */       IB(0x48); IB(0x8b); IB(0x51); IB(0x08);
                /* cmpl %eax, %edx */            IB(0x39); IB(0xc2);
        }

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

        /* if (!rt_gt_helper(env, dst, src1, src2)) return false; */
        ASM_BINARY_OP(ex_gt_helper);

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

        /* if (!rt_loadarray_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_storearray_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_len_helper(env, dst, src)) return false; */
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

        /* if (!rt_getdictkeybyindex_helper(env, dst, src1, src2)) return false; */
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

        /* if (!rt_getdictvalbyindex_helper(env, dst, src1, src2)) return false; */
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
        uint32_t src_len, src_hash;
        uint64_t src;

        CONSUME_TMPVAR(dst);
        CONSUME_STRING(src_s, src_len, src_hash);
        src = (uint64_t)(intptr_t)src_s;

        if (IS_MSABI) {
                /* if (!rt_loadsymbol_helper(env, dst, src, src_len, src_hash)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, 64 */                       IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */             IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */             IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /* (3rd) movabs $src -> %r8 */            IB(0x49); IB(0xb8); IQ((uint64_t)src);
                        /* (4th) movq $src_len -> %r9 */          IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)src_len);
                        /* (5th) movq $src_hash -> 32(%rsp) */    IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)src_hash);
                        /* movabs rt_loadsymbol_helper -> %rax */ IB(0x48); IB(0xb8); IQ((uint64_t)ex_loadsymbol_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);
                        /* addq %rsp, 64 */                       IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_loadsymbol_helper(env, dst, src, src_len, src_hash)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */               IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst, %rsi */               IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* (3rd) movabs %src, %rdx */             IB(0x48); IB(0xba); IQ(src);
                        /* (4th) movq $src_len, %rcx */           IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)src_len);
                        /* (5th) movq $src_hash, %r8 */           IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)src_hash);
                        /* movabs rt_loadsymbol_helper -> %rax */ IB(0x48); IB(0xb8); IQ((uint64_t)ex_loadsymbol_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
        }

        return true;
}

/* Visit a OP_STORESYMBOL instruction. */
static INLINE bool
jit_visit_storesymbol_op(
        struct jit_context *ctx)
{
        const char *dst_s;
        uint32_t dst_len, dst_hash;
        uint64_t dst;
        int src;

        CONSUME_STRING(dst_s, dst_len, dst_hash);
        CONSUME_TMPVAR(src);
        dst = (uint64_t)(intptr_t)dst_s;

        if (IS_MSABI) {
                /* if (!rt_storesymbol_helper(env, dst, dst_len, dst_hash, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                       IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */              IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movabs $dst -> %rdx */            IB(0x48); IB(0xba); IQ((uint64_t)dst);
                        /* (3rd) movq $dst_len -> %r8 */           IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)dst_len);
                        /* (4th) movq $dst_hash -> %r9 */          IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)dst_hash);
                        /* (5th) movq $src -> 32(%rsp) */          IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)src);
                        /* movabs rt_storesymbol_helper -> %rax */ IB(0x48); IB(0xb8); IQ((uint64_t)ex_storesymbol_helper);
                        /* call *%rax */                           IB(0xff); IB(0xd0);
                        /* addq %rsp, $64 */                       IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                     IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                         IB(0x75); IB(0x03);
                        /* jmp *%r13 */                            IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_storesymbol_helper(env, dst, dst_len, dst_hash, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */                IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movabs $dst, %rsi */              IB(0x48); IB(0xbe); IQ(dst);
                        /* (3rd) movq $dst_len, %rdx */            IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst_len);
                        /* (4th) movq $dst_hash, %rcx */           IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)dst_hash);
                        /* (5th) movq $src, %r8 */                 IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)src);
                        /* movabs rt_storesymbol_helper -> %rax */ IB(0x48); IB(0xb8); IQ((uint64_t)ex_storesymbol_helper);
                        /* call *%rax */                           IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                     IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                         IB(0x75); IB(0x03);
                        /* jmp *%r13 */                            IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
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
        uint32_t field_len;
        uint32_t field_hash;
        uint64_t field;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s, field_len, field_hash);
        field = (uint64_t)(intptr_t)field_s;

        if (IS_MSABI) {
                /* if (!rt_loaddot_helper(env, dst, dict, field, field_len, field_hash)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                      IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */             IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */             IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /* (3rd) movq $dict -> %r8 */             IB(0x49); IB(0xb8); IQ((uint64_t)dict);
                        /* (4th) movabs $field -> %r9 */          IB(0x49); IB(0xb9); IQ((uint64_t)field);
                        /* (5th) movq $field_len -> 32(%rsp) */   IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)field_len);
                        /* (6th) movq $field_hash -> 40(%rsp) */  IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x28); ID((uint32_t)field_hash);
                        /* movabs rt_loaddot_helper -> %rax */    IB(0x48); IB(0xb8); IQ((uint64_t)ex_loaddot_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);
                        /* addq %rsp, $64 */                      IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_loaddot_helper(env, dst, dict, field, field_len, field_hash)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */               IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst, %rsi */               IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* (3rd) movq $dict, %rdx */              IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dict);
                        /* (4th) movabs $field, %rcx */           IB(0x48); IB(0xb9); IQ(field);
                        /* (5th) movq $field_len, %r8 */          IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)field_len);
                        /* (6th) movq $field_hash, %r9 */         IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)field_hash);
                        /* movabs rt_loaddot_helper -> %rax */    IB(0x48); IB(0xb8); IQ((uint64_t)ex_loaddot_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
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
        uint32_t field_len;
        uint32_t field_hash;
        uint64_t field;
        int src;

        CONSUME_TMPVAR(dict);
        CONSUME_STRING(field_s, field_len, field_hash);
        CONSUME_TMPVAR(src);
        field = (uint64_t)(intptr_t)field_s;

        if (IS_MSABI) {
                /* if (!rt_storedot_helper(env, dict, field, field_len, field_hash, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                     IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */            IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movl $dict -> %edx */           IB(0xba); ID((uint32_t)dict);
                        /* (3rd) movabs $field -> %r8 */         IB(0x49); IB(0xb8); IQ((uint64_t)field);
                        /* (4th) movq $field_len -> %r9 */       IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)field_len);
                        /* (5th) movq $field_hash -> 32(%rsp) */ IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)field_hash);
                        /* (6th) movq $src -> 40(%rsp) */        IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x28); ID((uint32_t)src);
                        /* movabs rt_storedot_helper -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)ex_storedot_helper);
                        /* call *%rax */                         IB(0xff); IB(0xd0);
                        /* addq %rsp, $64 */                     IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                   IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                       IB(0x75); IB(0x03);
                        /* jmp *r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_storedot_helper(env, dict, field, field_len, field_hash, src)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14 -> %rdi */            IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dict -> %rsi */           IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dict);
                        /* (3rd) movabs $field -> %rdx */        IB(0x48); IB(0xba); IQ(field);
                        /* (4th) movq $field_len -> %rcx */      IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)field_len);
                        /* (5th) movq $field_hash -> %r8 */      IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)field_hash);
                        /* (6th) movq $src -> %r9 */             IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)src);
                        /* movabs ex_storedot_helper -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)ex_storedot_helper);
                        /* call *%rax */                         IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                   IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                       IB(0x75); IB(0x03);
                        /* jmp *%r13 */                          IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
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
                /* if (!rt_call_helper(env, dst, func, arg_count, arg)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                     IB(0x48); IB(0x83); IB(0xEC); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */            IB(0x4C); IB(0x89); IB(0xF1);
                        /* (2nd) movq $dst -> %rdx */            IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /* (3rd) movq $func -> %r8 */            IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)func);
                        /* (4th) movq $arg_count -> %r9 */       IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)arg_count);
                        /* (5th) movabs $arg_addr -> %rax */     IB(0x48); IB(0xB8); IQ((uint64_t)arg_addr);
                        /*       movq %rax -> 32(%rsp) */        IB(0x48); IB(0x89); IB(0x44); IB(0x24); IB(0x20);
                        /* movabs ex_call_helper -> rax */       IB(0x48); IB(0xB8); IQ((uint64_t)ex_call_helper);
                        /* call *%rax */                         IB(0xFF); IB(0xD0);
                        /* addq %rsp, $64 */                     IB(0x48); IB(0x83); IB(0xC4); IB(0x40);

                        /* test eax, eax */                      IB(0x83); IB(0xF8); IB(0x00);
                        /* jne 8 <next> */                       IB(0x75); IB(0x03);
                        /* jmp *r13 */                           IB(0x41); IB(0xFF); IB(0xE5);
                /* next: */
                }
        } else {
                /* if (!rt_call_helper(env, dst, func, arg_count, arg)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */              IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst, %rsi */              IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* (3rd) movq $func, %rdx */             IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)func);
                        /* (4th) movq $arg_count, %rcx */        IB(0x48); IB(0xc7); IB(0xc1); ID((uint32_t)arg_count);
                        /* (5th) movabs $arg_addr, %r8 */        IB(0x49); IB(0xb8); IQ(arg_addr);
                        /* movabs ex_call_helper -> rax */       IB(0x48); IB(0xB8); IQ((uint64_t)ex_call_helper);
                        /* call *%rax */                         IB(0xFF); IB(0xD0);

                        /* cmpl $0, %eax */                      IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                       IB(0x75); IB(0x03);
                        /* jmp *%r13 */                          IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
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
        uint32_t symbol_len;
        uint32_t symbol_hash;
        int arg_count;
        int arg_tmp;
        int arg[NOCT_ARG_MAX];
        uint64_t arg_addr;
        int i;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(obj);
        CONSUME_TMPVAR(arg_tmp);
        symbol = NULL;
        symbol_len = 0;
        symbol_hash = (uint32_t)arg_tmp;
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
                /* if (!rt_thiscall_helper(env, dst, obj, symbol, symbol_len, symbol_hash, arg_count, arg)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $64 */                           IB(0x48); IB(0x83); IB(0xec); IB(0x40);
                        /* (1st) movq %r14 -> %rcx */                  IB(0x4c); IB(0x89); IB(0xf1);
                        /* (2nd) movq $dst -> %rdx */                  IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)dst);
                        /* (3rd) movq $obj -> %r8 */                   IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)obj);
                        /* (4th) movabs $symbol -> %r9 */              IB(0x49); IB(0xb9); IQ((uint64_t)symbol);
                        /* (5th) movq $symbol_len -> 32(%rsp) */       IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x20); ID((uint32_t)symbol_len);
                        /* (6th) movq $symbol_hash -> 40(%rsp) */      IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x28); ID((uint32_t)symbol_hash);
                        /* (7th) movq $arg_count -> 48(%rsp) */        IB(0x48); IB(0xc7); IB(0x44); IB(0x24); IB(0x30); ID((uint32_t)arg_count);
                        /* (8th) movabs $arg_addr -> %rax */           IB(0x48); IB(0xb8); IQ((uint64_t)arg_addr);
                        /*       movq %rax -> 56(%rsp) */              IB(0x48); IB(0x89); IB(0x44); IB(0x24); IB(0x38);
                        /* movabs ex_thiscall_helper -> %rax */        IB(0x48); IB(0xb8); IQ((uint64_t)ex_thiscall_helper);
                        /* call *%rax */                               IB(0xff); IB(0xd0);
                        /* addq %rsp, $64 */                           IB(0x48); IB(0x83); IB(0xc4); IB(0x40);

                        /* testl %eax, %eax */                         IB(0x83); IB(0xF8); IB(0x00);
                        /* jne 8 <next> */                             IB(0x75); IB(0x03);
                        /* jmp *r13 */                                 IB(0x41); IB(0xFF); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_thiscall_helper(env, dst, obj, symbol, symbol_len, symbol_hash, arg_count, arg)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $32 */                          IB(0x48); IB(0x83); IB(0xEC); IB(0x20);
                        /* (1st) movq %r14 -> %rdi */                 IB(0x4c); IB(0x89); IB(0xf7);
                        /* (2nd) movq $dst -> %rsi */                 IB(0x48); IB(0xc7); IB(0xc6); ID((uint32_t)dst);
                        /* (3rd) movq $obj -> %rdx */                 IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)obj);
                        /* (4th) movabs $symbol -> %rcx */            IB(0x48); IB(0xb9); IQ((uint64_t)symbol);
                        /* (5th) movq $symbol_len -> %r8 */           IB(0x49); IB(0xc7); IB(0xc0); ID((uint32_t)symbol_len);
                        /* (6th) movq $symbol_hash -> %r9 */          IB(0x49); IB(0xc7); IB(0xc1); ID((uint32_t)symbol_hash);
                        /* (7th) movq $arg_count -> 0(%rsp) */        IB(0x48); IB(0xc7); IB(0x04); IB(0x24); ID((uint32_t)arg_count);
                        /* (8th) movabs $arg -> %rax */               IB(0x48); IB(0xB8); IQ((uint64_t)arg_addr);
                        /*       movq %rax -> 8(%rsp) */              IB(0x48); IB(0x89); IB(0x44); IB(0x24); IB(0x08);
                        /* movabs ex_thiscall_helper -> %r10 */       IB(0x49); IB(0xba); IQ((uint64_t)ex_thiscall_helper);
                        /* call *%r10 */                              IB(0x41); IB(0xff); IB(0xd2);
                        /* add %rsp, 32 */                            IB(0x48); IB(0x83); IB(0xc4); IB(0x20);

                        /* testl %eax, %eax */                         IB(0x83); IB(0xF8); IB(0x00);
                        /* jne 8 <next> */                            IB(0x75); IB(0x03);
                        /* jmp *%r13 */                               IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
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
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JMP;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* jmp 5 */        IB(0xe9); ID(0);
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

        src *= (int)sizeof(struct rt_value);

        ASM {
                /* r13: exception_handler */
                /* r14: rt */
                /* r15: &env->frame->tmpvar[0] */

                /* rdx = &env->frame->tmpvar[src] */
                /* movq src, %rdx */               IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);
                /* addq %r15, %rdx */              IB(0x4c); IB(0x01); IB(0xfa);
                /* movl 8(%rdx), %eax */           IB(0x8b); IB(0x42); IB(0x08);

                /* Compare: env->frame->tmpvar[dst].val.i == 0 */
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

        src *= (int)sizeof(struct rt_value);

        ASM {
                /* rdx = &env->frame->tmpvar[src] */
                /* movq src, %rdx */               IB(0x48); IB(0xc7); IB(0xc2); ID((uint32_t)src);
                /* addq %r15, %rdx */              IB(0x4c); IB(0x01); IB(0xfa);
                /* movl 8(%rdx), %eax */           IB(0x8b); IB(0x42); IB(0x08);

                /* Compare: env->frame->tmpvar[dst].val.i == 0 */
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
        ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JE;
        ctx->branch_patch_count++;

        ASM {
                /* Patched later. */
                /* je 6 */                                IB(0x0f); IB(0x84); ID(0);
        }

        return true;
}

/* Visit a OP_SAFEPOINT instruction. */
static INLINE bool
jit_visit_safepoint_op(
        struct jit_context *ctx)
{
        if (IS_MSABI) {
                /* if (!rt_safepoint_helper(env)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* subq %rsp, $32 */                      IB(0x48); IB(0x83); IB(0xec); IB(0x20);
                        /* (1st) movq %r14 -> %rcx */             IB(0x4c); IB(0x89); IB(0xf1);
                        /* movabs rt_safepoint_helper -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)ex_safepoint_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);
                        /* addq %rsp, $32 */                      IB(0x48); IB(0x83); IB(0xc4); IB(0x20);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next: */
                }
        } else {
                /* if (!rt_safepoint_helper(env)) return false; */
                ASM {
                        /* r13: exception_handler */
                        /* r14: env */
                        /* r14: &env->frame->tmpvar[0] */

                        /* (1st) movq %r14, %rdi */               IB(0x4c); IB(0x89); IB(0xf7);
                        /* movabs rt_safepoint_helper -> %rax */  IB(0x48); IB(0xb8); IQ((uint64_t)ex_safepoint_helper);
                        /* call *%rax */                          IB(0xff); IB(0xd0);

                        /* testl %eax, %eax */                    IB(0x83); IB(0xf8); IB(0x00);
                        /* jne 8 <next> */                        IB(0x75); IB(0x03);
                        /* jmp *%r13 */                           IB(0x41); IB(0xff); IB(0xe5);
                /* next:*/
                }
        }

        return true;
}

/* Visit a OP_PBASE instruction. (ABCE; inline machine code.)
 *
 * The ABCE guard has already proven the operand is a packed, so this
 * trusts the value and loads the payload pointer directly.
 */
static INLINE bool
jit_visit_pbase_op(
        struct jit_context *ctx)
{
        int dst;
        int src;
        int base_id;
        uint32_t buf_ofs;

        CONSUME_TMPVAR(dst);
        CONSUME_TMPVAR(src);
        CONSUME_IMM8(base_id);
        UNUSED_PARAMETER(base_id);

        dst *= (int)sizeof(struct rt_value);
        src *= (int)sizeof(struct rt_value);
        buf_ofs = (uint32_t)offsetof(struct rt_packed, packed_buffer);

        /* env->frame->tmpvar[dst].type = NOCT_VALUE_LONG; */
        /* env->frame->tmpvar[dst].val.l = (int64_t)packed->packed_buffer; */
        ASM {
                /* r15: &env->frame->tmpvar[0] */

                /* movq src+8(%r15) -> %rax */   IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(src + 8));
                /* movq buf_ofs(%rax) -> %rax */ IB(0x48); IB(0x8b); IB(0x80); ID(buf_ofs);
                /* movl $LONG -> dst(%r15) */    IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_LONG);
                /* movq %rax -> dst+8(%r15) */   IB(0x49); IB(0x89); IB(0x87); ID((uint32_t)(dst + 8));
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

/* Visit a OP_PLOAD8U instruction. (ABCE; inline machine code.) */
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

        /* dst.val.i = *(uint8_t *)(base.val.l + ofs.val.i); dst.type = INT; */
        ASM {
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* movzbl (%rax,%rcx) -> %edx */   IB(0x0f); IB(0xb6); IB(0x14); IB(0x08);
                /* movl $INT -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_INT);
                /* movl %edx -> dst+8(%r15) */     IB(0x41); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PSTORE8 instruction. (ABCE; inline machine code.
 * Operand order: base, ofs, src.) */
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

        /* *(uint8_t *)(base.val.l + ofs.val.i) = (uint8_t)src.val.i; */
        ASM {
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* movl src+8(%r15) -> %edx */     IB(0x41); IB(0x8b); IB(0x97); ID((uint32_t)(src + 8));
                /* movb %dl -> (%rax,%rcx) */      IB(0x88); IB(0x14); IB(0x08);
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

/* Visit a OP_PLOAD8S instruction. (ABCE; inline machine code.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* load scaled -> %(e|r)dx */      IB(0x0f); IB(0xbe); IB(0x14); IB(0x08);
                /* movl $tag -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_INT);
                /* movl %edx -> dst+8(%r15) */   IB(0x41); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PLOAD16U instruction. (ABCE; inline machine code.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* load scaled -> %(e|r)dx */      IB(0x0f); IB(0xb7); IB(0x14); IB(0x48);
                /* movl $tag -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_INT);
                /* movl %edx -> dst+8(%r15) */   IB(0x41); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PLOAD16S instruction. (ABCE; inline machine code.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* load scaled -> %(e|r)dx */      IB(0x0f); IB(0xbf); IB(0x14); IB(0x48);
                /* movl $tag -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_INT);
                /* movl %edx -> dst+8(%r15) */   IB(0x41); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PLOAD32 instruction. (ABCE; inline machine code.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* load scaled -> %(e|r)dx */      IB(0x8b); IB(0x14); IB(0x88);
                /* movl $tag -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_INT);
                /* movl %edx -> dst+8(%r15) */   IB(0x41); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PLOAD64 instruction. (ABCE; inline machine code.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* load scaled -> %(e|r)dx */      IB(0x48); IB(0x8b); IB(0x14); IB(0xc8);
                /* movl $tag -> dst(%r15) */       IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_LONG);
                /* movq %rdx -> dst+8(%r15) */   IB(0x49); IB(0x89); IB(0x97); ID((uint32_t)(dst + 8));
        }

        return true;
}

/* Visit a OP_PSTORE16 instruction. (ABCE; inline. Int source per ABCE rules.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* movl src+8(%r15) -> %edx */     IB(0x41); IB(0x8b); IB(0x97); ID((uint32_t)(src + 8));
                /* store scaled */                 IB(0x66); IB(0x89); IB(0x14); IB(0x48);
        }

        return true;
}

/* Visit a OP_PSTORE32 instruction. (ABCE; inline. Int source per ABCE rules.) */
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
                /* r15: &env->frame->tmpvar[0] */

                /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                /* movl src+8(%r15) -> %edx */     IB(0x41); IB(0x8b); IB(0x97); ID((uint32_t)(src + 8));
                /* store scaled */                 IB(0x89); IB(0x14); IB(0x88);
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
 * Typed arithmetic ops (docs/design/07-typed-ops.md): inline machine
 * code.  Every op trusts the operand tags (proven by the LIR layer);
 * rax/rcx/rdx and xmm0 are per-op scratch, like the other emitters.
 *
 * The float comparisons are NaN-safe by construction: ucomiss sets
 * CF=ZF=PF=1 on an unordered compare, so seta/setae (which read
 * CF/ZF) yield 0 for NaN, matching the C semantics of the scalar
 * helpers.  "a < b" is emitted as "b > a" (load b, compare against
 * a, seta) so that the unordered case falls on the false side; never
 * replace seta/setae with setl/setle here.
 */

/* Emit "movl ofs+8(%r15), %eax". */
#define TYPED_LOAD_EAX(ofs)     IB(0x41); IB(0x8b); IB(0x87); ID((uint32_t)((ofs) + 8))
/* Emit "movss ofs+8(%r15), %xmm0". */
#define TYPED_LOAD_XMM0(ofs)    IB(0xf3); IB(0x41); IB(0x0f); IB(0x10); IB(0x87); ID((uint32_t)((ofs) + 8))
/* Emit "movl $tag, dst(%r15); movl %eax, dst+8(%r15)". */
#define TYPED_STORE_EAX(dst, tag)                                                               \
        IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)(dst)); ID((uint32_t)(tag));                 \
        IB(0x41); IB(0x89); IB(0x87); ID((uint32_t)((dst) + 8))
/* Emit "setcc %al; movzbl %al, %eax" (cc = setcc second opcode byte). */
#define TYPED_SETCC_EAX(cc)                                                                     \
        IB(0x0f); IB(cc); IB(0xc0);                                                             \
        IB(0x0f); IB(0xb6); IB(0xc0)

/* Visit an OP_IADD..OP_FGTE instruction.  (Typed arithmetic; inline.) */
static INLINE bool
jit_visit_typed_op(
        struct jit_context *ctx,
        int op)
{
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

        dst *= (int)sizeof(struct rt_value);
        if (op != OP_ISHL && op != OP_ISHR)
                src2 *= (int)sizeof(struct rt_value);
        src1 *= (int)sizeof(struct rt_value);

        switch (op) {
        case OP_IADD:
        case OP_ISUB:
        case OP_IMUL:
        case OP_IAND:
        case OP_IOR:
        case OP_IXOR:
                ASM {
                        /* r15: &env->frame->tmpvar[0] */
                        TYPED_LOAD_EAX(src1);
                }
                switch (op) {
                /* op src2+8(%r15) -> %eax */
                case OP_IADD: ASM { IB(0x41); IB(0x03); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_ISUB: ASM { IB(0x41); IB(0x2b); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_IAND: ASM { IB(0x41); IB(0x23); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_IOR:  ASM { IB(0x41); IB(0x0b); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_IXOR: ASM { IB(0x41); IB(0x33); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                default:      ASM { IB(0x41); IB(0x0f); IB(0xaf); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        case OP_ISHL:
        case OP_ISHR:
                /* src2 is the shift-count immediate (0..31). */
                ASM {
                        TYPED_LOAD_EAX(src1);
                }
                if ((src2 & 31) != 0) {
                        if (op == OP_ISHL) {
                                /* shll $imm, %eax */
                                ASM { IB(0xc1); IB(0xe0); IB((uint8_t)(src2 & 31)); }
                        } else {
                                /* shrl $imm, %eax (LOGICAL) */
                                ASM { IB(0xc1); IB(0xe8); IB((uint8_t)(src2 & 31)); }
                        }
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        case OP_IDIV:
        case OP_IMOD:
                /* The LIR layer only emits these with a literal
                   divisor outside {0, -1}: no trap is reachable from
                   compiled code.  (Crafted bytecode is outside the
                   JIT trust model, as with the PLOAD family.) */
                ASM {
                        TYPED_LOAD_EAX(src1);
                        /* cltd */              IB(0x99);
                        /* idivl src2+8(%r15) */ IB(0x41); IB(0xf7); IB(0xbf); ID((uint32_t)(src2 + 8));
                }
                if (op == OP_IMOD) {
                        /* movl %edx, %eax */
                        ASM { IB(0x89); IB(0xd0); }
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        case OP_ILT:
        case OP_ILTE:
        case OP_IGT:
        case OP_IGTE:
                ASM {
                        TYPED_LOAD_EAX(src1);
                        /* cmpl src2+8(%r15), %eax */
                        IB(0x41); IB(0x3b); IB(0x87); ID((uint32_t)(src2 + 8));
                }
                switch (op) {
                case OP_ILT:  ASM { TYPED_SETCC_EAX(0x9c); } break;     /* setl  */
                case OP_ILTE: ASM { TYPED_SETCC_EAX(0x9e); } break;     /* setle */
                case OP_IGT:  ASM { TYPED_SETCC_EAX(0x9f); } break;     /* setg  */
                default:      ASM { TYPED_SETCC_EAX(0x9d); } break;     /* setge */
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        case OP_FADD:
        case OP_FSUB:
        case OP_FMUL:
        case OP_FDIV:
                ASM {
                        TYPED_LOAD_XMM0(src1);
                }
                switch (op) {
                /* opss src2+8(%r15) -> %xmm0 */
                case OP_FADD: ASM { IB(0xf3); IB(0x41); IB(0x0f); IB(0x58); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_FSUB: ASM { IB(0xf3); IB(0x41); IB(0x0f); IB(0x5c); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                case OP_FMUL: ASM { IB(0xf3); IB(0x41); IB(0x0f); IB(0x59); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                default:      ASM { IB(0xf3); IB(0x41); IB(0x0f); IB(0x5e); IB(0x87); ID((uint32_t)(src2 + 8)); } break;
                }
                ASM {
                        /* movl $tag, dst(%r15) */
                        IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst); ID((uint32_t)NOCT_VALUE_FLOAT);
                        /* movss %xmm0, dst+8(%r15) */
                        IB(0xf3); IB(0x41); IB(0x0f); IB(0x11); IB(0x87); ID((uint32_t)(dst + 8));
                }
                break;
        case OP_FLT:
        case OP_FLTE:
                /* a < b  ==  b > a: load b, compare against a. */
                ASM {
                        TYPED_LOAD_XMM0(src2);
                        /* ucomiss src1+8(%r15), %xmm0 */
                        IB(0x41); IB(0x0f); IB(0x2e); IB(0x87); ID((uint32_t)(src1 + 8));
                }
                if (op == OP_FLT) {
                        ASM { TYPED_SETCC_EAX(0x97); }  /* seta  */
                } else {
                        ASM { TYPED_SETCC_EAX(0x93); }  /* setae */
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        case OP_FGT:
        case OP_FGTE:
                ASM {
                        TYPED_LOAD_XMM0(src1);
                        /* ucomiss src2+8(%r15), %xmm0 */
                        IB(0x41); IB(0x0f); IB(0x2e); IB(0x87); ID((uint32_t)(src2 + 8));
                }
                if (op == OP_FGT) {
                        ASM { TYPED_SETCC_EAX(0x97); }  /* seta  */
                } else {
                        ASM { TYPED_SETCC_EAX(0x93); }  /* setae */
                }
                ASM {
                        TYPED_STORE_EAX(dst, NOCT_VALUE_INT);
                }
                break;
        default:
                assert(NEVER_COME_HERE);
                return false;
        }

        return true;
}

/*
 * 128-bit SIMD ops (docs/design/06-simd.md).
 *
 * SysV x86_64 with SSE2/SSE4.1 (runtime CPUID gate): inline vector code,
 * vreg k -> xmm k (xmm0..xmm12 are available to call-free vector
 * regions; xmm13..xmm15 are backend scratch/invariant registers). Win64 uses
 * direct scalar lowering over env->vreg so it never owns nonvolatile
 * xmm6/xmm7; scalar FP uses only volatile xmm0.
 */

/* Per-build CPUID: no unsynchronised process-global feature cache. */
static uint32_t
jit_detect_simd_caps(void)
{
#if defined(__GNUC__)
        uint32_t a, b, c, d;
	uint32_t xcr0_lo, xcr0_hi;
	uint32_t caps = 0;
        __asm__ __volatile__("cpuid"
                             : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                             : "a"(1), "c"(0));
	if ((d & (1u << 26)) != 0)
		caps |= JIT_SIMD_CAP_SSE2;
	if ((c & (1u << 0)) != 0)
		caps |= JIT_SIMD_CAP_SSE3;
	if ((c & (1u << 19)) != 0)
		caps |= JIT_SIMD_CAP_SSE41;
	if ((c & (1u << 27)) != 0 && (c & (1u << 28)) != 0) {
		__asm__ __volatile__("xgetbv"
				     : "=a"(xcr0_lo), "=d"(xcr0_hi)
				     : "c"(0));
		UNUSED_PARAMETER(xcr0_hi);
		if ((xcr0_lo & 6u) == 6u) {
			caps |= JIT_SIMD_CAP_AVX;
			if ((c & (1u << 12)) != 0)
				caps |= JIT_SIMD_CAP_FMAF32X4;
		}
	}
	return caps;
#elif defined(_MSC_VER)
	int regs[4];
	unsigned __int64 xcr0;
	uint32_t caps = 0;
	__cpuidex(regs, 1, 0);
	if (((uint32_t)regs[3] & (1u << 26)) != 0)
		caps |= JIT_SIMD_CAP_SSE2;
	if (((uint32_t)regs[2] & (1u << 0)) != 0)
		caps |= JIT_SIMD_CAP_SSE3;
	if (((uint32_t)regs[2] & (1u << 19)) != 0)
		caps |= JIT_SIMD_CAP_SSE41;
	if (((uint32_t)regs[2] & (1u << 27)) != 0 &&
	    ((uint32_t)regs[2] & (1u << 28)) != 0) {
		xcr0 = _xgetbv(0);
		if ((xcr0 & 6u) == 6u) {
			caps |= JIT_SIMD_CAP_AVX;
			if (((uint32_t)regs[2] & (1u << 12)) != 0)
				caps |= JIT_SIMD_CAP_FMAF32X4;
		}
	}
	return caps;
#else
	return 0;
#endif
}

/* Direct scalar lowering for the forced-scalar and Win64 tiers. */
static INLINE bool
jit_visit_vector_scalar_op(
        struct jit_context *ctx,
        int op,
        int dst,
        int src1,
        int src2)
{
        uint32_t vbase = (uint32_t)offsetof(struct rt_env, vreg);
        int lane;

        switch (op) {
        case OP_VLOADI32X4:
        case OP_VLOADF32X4:
        {
                int base = src1 * (int)sizeof(struct rt_value);
                int ofs = src2 * (int)sizeof(struct rt_value);
                ASM {
                        IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                        IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                }
                for (lane = 0; lane < 4; lane++) {
                        IB(0x8b); IB(0x54); IB(0x88); IB((uint8_t)(lane * 4));
                        IB(0x41); IB(0x89); IB(0x96);
                        ID(vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4);
                }
                return true;
        }
        case OP_VSTOREI32X4:
        case OP_VSTOREF32X4:
        {
                int base = dst * (int)sizeof(struct rt_value);
                int ofs = src1 * (int)sizeof(struct rt_value);
                ASM {
                        IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                        IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                }
                for (lane = 0; lane < 4; lane++) {
                        IB(0x41); IB(0x8b); IB(0x96);
                        ID(vbase + (uint32_t)src2 * 16 + (uint32_t)lane * 4);
                        IB(0x89); IB(0x54); IB(0x88); IB((uint8_t)(lane * 4));
                }
                return true;
        }
        case OP_VSPLATI32:
        case OP_VSPLATF32:
        {
                int src = src1 * (int)sizeof(struct rt_value);
                ASM { IB(0x41); IB(0x8b); IB(0x87); ID((uint32_t)(src + 8)); }
                for (lane = 0; lane < 4; lane++) {
                        IB(0x41); IB(0x89); IB(0x86);
                        ID(vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4);
                }
                return true;
        }
        case OP_VGETLANEI32:
        case OP_VGETLANEF32:
        {
                int d = dst * (int)sizeof(struct rt_value);
                ASM {
                        IB(0x41); IB(0x8b); IB(0x86);
                        ID(vbase + (uint32_t)src1 * 16 + (uint32_t)src2 * 4);
                        IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)d);
                        ID((uint32_t)(op == OP_VGETLANEF32 ?
                                      NOCT_VALUE_FLOAT : NOCT_VALUE_INT));
                        IB(0x41); IB(0x89); IB(0x87); ID((uint32_t)(d + 8));
                }
                return true;
        }
        case OP_VMOV128:
                for (lane = 0; lane < 4; lane++) {
                        IB(0x41); IB(0x8b); IB(0x86);
                        ID(vbase + (uint32_t)src1 * 16 + (uint32_t)lane * 4);
                        IB(0x41); IB(0x89); IB(0x86);
                        ID(vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4);
                }
                return true;
	case OP_VCVTI32F32X4:
	case OP_VCVTF32I32X4:
		for (lane = 0; lane < 4; lane++) {
			uint32_t s = vbase + (uint32_t)src1 * 16 +
				(uint32_t)lane * 4;
			uint32_t d = vbase + (uint32_t)dst * 16 +
				(uint32_t)lane * 4;
			if (op == OP_VCVTI32F32X4) {
				/* cvtsi2ssl s(%r14), xmm0; movss xmm0,d(%r14) */
				IB(0xf3); IB(0x41); IB(0x0f); IB(0x2a); IB(0x86); ID(s);
				IB(0xf3); IB(0x41); IB(0x0f); IB(0x11); IB(0x86); ID(d);
			} else {
				/* cvttss2si s(%r14),eax; mov eax,d(%r14) */
				IB(0xf3); IB(0x41); IB(0x0f); IB(0x2c); IB(0x86); ID(s);
				IB(0x41); IB(0x89); IB(0x86); ID(d);
			}
		}
		return true;
        case OP_VADDI32X4:
        case OP_VSUBI32X4:
        case OP_VMULI32X4:
        case OP_VAND128:
        case OP_VOR128:
        case OP_VXOR128:
                for (lane = 0; lane < 4; lane++) {
                        uint32_t a = vbase + (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t b = vbase + (uint32_t)src2 * 16 + (uint32_t)lane * 4;
                        uint32_t d = vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        IB(0x41); IB(0x8b); IB(0x86); ID(a);
                        IB(0x41);
                        switch (op) {
                        case OP_VADDI32X4: IB(0x03); IB(0x86); ID(b); break;
                        case OP_VSUBI32X4: IB(0x2b); IB(0x86); ID(b); break;
                        case OP_VMULI32X4: IB(0x0f); IB(0xaf); IB(0x86); ID(b); break;
                        case OP_VAND128:   IB(0x23); IB(0x86); ID(b); break;
                        case OP_VOR128:    IB(0x0b); IB(0x86); ID(b); break;
                        default:           IB(0x33); IB(0x86); ID(b); break;
                        }
                        IB(0x41); IB(0x89); IB(0x86); ID(d);
                }
                return true;
        case OP_VSHLI32X4:
        case OP_VSHRI32X4:
                for (lane = 0; lane < 4; lane++) {
                        uint32_t s = vbase + (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t d = vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        IB(0x41); IB(0x8b); IB(0x86); ID(s);
                        IB(0xc1); IB((uint8_t)(op == OP_VSHLI32X4 ? 0xe0 : 0xe8));
                        IB((uint8_t)src2);
                        IB(0x41); IB(0x89); IB(0x86); ID(d);
                }
                return true;
        case OP_VADDF32X4:
        case OP_VSUBF32X4:
        case OP_VMULF32X4:
        case OP_VDIVF32X4:
                for (lane = 0; lane < 4; lane++) {
                        uint32_t a = vbase + (uint32_t)src1 * 16 + (uint32_t)lane * 4;
                        uint32_t b = vbase + (uint32_t)src2 * 16 + (uint32_t)lane * 4;
                        uint32_t d = vbase + (uint32_t)dst * 16 + (uint32_t)lane * 4;
                        IB(0xf3); IB(0x41); IB(0x0f); IB(0x10); IB(0x86); ID(a);
                        IB(0xf3); IB(0x41); IB(0x0f);
                        switch (op) {
                        case OP_VADDF32X4: IB(0x58); break;
                        case OP_VSUBF32X4: IB(0x5c); break;
                        case OP_VMULF32X4: IB(0x59); break;
                        default:           IB(0x5e); break;
                        }
                        IB(0x86); ID(b);
                        IB(0xf3); IB(0x41); IB(0x0f); IB(0x11); IB(0x86); ID(d);
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
        int a;
        int b;
        int c;
        int inline_ok;

	/* Win64 keeps the memory-canonical direct scalar tier until xmm6/xmm7
	   receive an explicitly tested save area.  SysV needs only SSE2;
	   SSE4.1 selects shorter multiply/extract sequences below. */
	inline_ok = !IS_MSABI &&
		    (ctx->simd_caps & JIT_SIMD_CAP_SSE2) != 0;

        /* Decode (shapes vary per op; see bytecode.h). */
        switch (op) {
        case OP_VLOADI32X4:
        case OP_VLOADF32X4:
                CONSUME_IMM8(a);
                CONSUME_TMPVAR(b);
                CONSUME_TMPVAR(c);
                break;
        case OP_VSTOREI32X4:
        case OP_VSTOREF32X4:
                CONSUME_TMPVAR(a);
                CONSUME_TMPVAR(b);
                CONSUME_IMM8(c);
                break;
        case OP_VSPLATI32:
        case OP_VSPLATF32:
                CONSUME_IMM8(a);
                CONSUME_TMPVAR(b);
                c = 0;
                break;
        case OP_VGETLANEI32:
        case OP_VGETLANEF32:
                CONSUME_TMPVAR(a);
                CONSUME_IMM8(b);
                CONSUME_IMM8(c);
                break;
        case OP_VMOV128:
	case OP_VCVTI32F32X4:
	case OP_VCVTF32I32X4:
                CONSUME_IMM8(a);
                CONSUME_IMM8(b);
                c = 0;
                break;
        default:
                CONSUME_IMM8(a);
                CONSUME_IMM8(b);
                CONSUME_IMM8(c);
                break;
        }

        if (!inline_ok)
                return jit_visit_vector_scalar_op(ctx, op, a, b, c);

	/* Native SysV mapping reserves xmm13/xmm14 for SSE2 multiply
	 * scratch and xmm15 for the opaque-alpha invariant. */
	switch (op) {
	case OP_VLOADI32X4:
	case OP_VLOADF32X4:
	case OP_VSPLATI32:
	case OP_VSPLATF32:
		if (a < 0 || a >= 13) goto broken_vreg;
		break;
	case OP_VSTOREI32X4:
	case OP_VSTOREF32X4:
		if (c < 0 || c >= 13) goto broken_vreg;
		break;
	case OP_VGETLANEI32:
	case OP_VGETLANEF32:
		if (b < 0 || b >= 13) goto broken_vreg;
		break;
	case OP_VMOV128:
	case OP_VCVTI32F32X4:
	case OP_VCVTF32I32X4:
		if (a < 0 || a >= 13 || b < 0 || b >= 13)
			goto broken_vreg;
		break;
	case OP_VSHLI32X4:
	case OP_VSHRI32X4:
		if (a < 0 || a >= 13 || b < 0 || b >= 13)
			goto broken_vreg;
		break;
	default:
		if (a < 0 || a >= 13 || b < 0 || b >= 13 ||
		    c < 0 || c >= 13)
			goto broken_vreg;
		break;
	}

        switch (op) {
        case OP_VLOADI32X4:
        case OP_VLOADF32X4:
        {
                int base = b * (int)sizeof(struct rt_value);
                int ofs = c * (int)sizeof(struct rt_value);
		int cursor = -1;
		if (ctx->vector_hint_active) {
			if (ctx->vector_base_tmp[0] == b) cursor = 0;
			else if (ctx->vector_base_tmp[1] == b) cursor = 1;
		}
		if (cursor >= 0) {
			/* movdqu (rbx|rsi,rdi,4), xmmA */
			ASM { IB(0xf3); }
			if ((a & 8) != 0) { ASM { IB(0x44); } }
			ASM { IB(0x0f); IB(0x6f);
			      IB((uint8_t)(0x04 | ((a & 7) << 3)));
			      IB((uint8_t)(cursor == 0 ? 0xbb : 0xbe)); }
			break;
		}
                ASM {
                        /* r15: &env->frame->tmpvar[0] */
                        /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                        /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                        /* movdqu (%rax,%rcx,4) -> %xmmA */ IB(0xf3);
                }
		if ((a & 8) != 0) { ASM { IB(0x44); } }
		ASM { IB(0x0f); IB(0x6f);
		      IB((uint8_t)(0x04 | ((a & 7) << 3))); IB(0x88); }
                break;
        }
        case OP_VSTOREI32X4:
        case OP_VSTOREF32X4:
        {
                int base = a * (int)sizeof(struct rt_value);
                int ofs = b * (int)sizeof(struct rt_value);
		int cursor = -1;
		if (ctx->vector_hint_active) {
			if (ctx->vector_base_tmp[0] == a) cursor = 0;
			else if (ctx->vector_base_tmp[1] == a) cursor = 1;
		}
		if (cursor >= 0) {
			/* movdqu xmmC, (rbx|rsi,rdi,4) */
			ASM { IB(0xf3); }
			if ((c & 8) != 0) { ASM { IB(0x44); } }
			ASM { IB(0x0f); IB(0x7f);
			      IB((uint8_t)(0x04 | ((c & 7) << 3)));
			      IB((uint8_t)(cursor == 0 ? 0xbb : 0xbe)); }
			break;
		}
                ASM {
                        /* movq base+8(%r15) -> %rax */    IB(0x49); IB(0x8b); IB(0x87); ID((uint32_t)(base + 8));
                        /* movslq ofs+8(%r15) -> %rcx */   IB(0x49); IB(0x63); IB(0x8f); ID((uint32_t)(ofs + 8));
                        /* movdqu %xmmC -> (%rax,%rcx,4) */ IB(0xf3);
                }
		if ((c & 8) != 0) { ASM { IB(0x44); } }
		ASM { IB(0x0f); IB(0x7f);
		      IB((uint8_t)(0x04 | ((c & 7) << 3))); IB(0x88); }
                break;
        }
        case OP_VSPLATI32:
        case OP_VSPLATF32:
        {
                int src = b * (int)sizeof(struct rt_value);
		ASM { IB(0x66); IB((uint8_t)((a & 8) != 0 ? 0x45 : 0x41));
		      IB(0x0f); IB(0x6e);
		      IB((uint8_t)(0x87 | ((a & 7) << 3)));
		      ID((uint32_t)(src + 8)); }
		if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x70, a, a))
			return false;
		ASM { IB(0x00); }
                break;
        }
        case OP_VGETLANEI32:
        case OP_VGETLANEF32:
        {
                int dst = a * (int)sizeof(struct rt_value);
		if ((ctx->simd_caps & JIT_SIMD_CAP_SSE41) != 0) {
			ASM { IB(0x66);
			      IB((uint8_t)((b & 8) != 0 ? 0x45 : 0x41));
			      IB(0x0f); IB(0x3a); IB(0x16);
			      IB((uint8_t)(0x87 | ((b & 7) << 3)));
			      ID((uint32_t)(dst + 8)); IB((uint8_t)c); }
		} else {
			ASM { IB(0x66); }
			if ((b & 8) != 0) { ASM { IB(0x41); } }
			ASM {
				/* SSE2: combine two pextrw results without changing xmmB. */
				IB(0x0f); IB(0xc5); IB((uint8_t)(0xc0 | (b & 7))); IB((uint8_t)(c * 2));
				IB(0x66);
			}
			if ((b & 8) != 0) { ASM { IB(0x41); } }
			ASM {
				IB(0x0f); IB(0xc5); IB((uint8_t)(0xc8 | (b & 7))); IB((uint8_t)(c * 2 + 1));
				IB(0xc1); IB(0xe1); IB(0x10);
				IB(0x09); IB(0xc8);
				IB(0x41); IB(0x89); IB(0x87); ID((uint32_t)(dst + 8));
			}
		}
		ASM {
			/* Both i32 and f32 lanes are raw 32-bit payloads. */
			IB(0x41); IB(0xc7); IB(0x87); ID((uint32_t)dst);
			ID((uint32_t)(op == OP_VGETLANEF32 ?
				      NOCT_VALUE_FLOAT : NOCT_VALUE_INT));
		}
                break;
        }
        case OP_VMOV128:
                if (a != b) {
			if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
				if (!jit_x86_64_put_vex_rr(ctx, 1, 1, 0x6f,
							 a, b))
					return false;
			} else if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1,
							  0x6f, a, b)) {
				return false;
			}
                }
                break;
	case OP_VCVTI32F32X4:
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			if (!jit_x86_64_put_vex_rr(ctx, 1, 0, 0x5b, a, b))
				return false;
		} else if (!jit_x86_64_put_sse_rr(ctx, 0, 1, 0x5b, a, b)) {
			return false;
		}
		break;
	case OP_VCVTF32I32X4:
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			if (!jit_x86_64_put_vex_rr(ctx, 1, 2, 0x5b, a, b))
				return false;
		} else if (!jit_x86_64_put_sse_rr(ctx, 0xf3, 1, 0x5b,
							  a, b)) {
			return false;
		}
		break;
        case OP_VADDI32X4:
        case OP_VSUBI32X4:
        case OP_VMULI32X4:
        case OP_VAND128:
        case OP_VOR128:
        case OP_VXOR128:
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			int map = op == OP_VMULI32X4 ? 2 : 1;
			uint8_t opcode;
			switch (op) {
			case OP_VADDI32X4: opcode = 0xfe; break;
			case OP_VSUBI32X4: opcode = 0xfa; break;
			case OP_VMULI32X4: opcode = 0x40; break;
			case OP_VAND128: opcode = 0xdb; break;
			case OP_VOR128: opcode = 0xeb; break;
			default: opcode = 0xef; break;
			}
			if (!jit_x86_64_put_vex_rrr(ctx, map, 1, opcode,
						      a, b, c))
				return false;
			break;
		}
		/* Legacy two-address lowering. */
		if (op == OP_VMULI32X4 &&
		    (ctx->simd_caps & JIT_SIMD_CAP_SSE41) == 0) {
			/* xmm13/xmm14 are reserved outside the logical map. */
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f, 13, b) ||
			    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x70, 13, 13))
				return false;
			ASM { IB(0xf5); }
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f, 14, c) ||
			    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x70, 14, 14))
				return false;
			ASM { IB(0xf5); }
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xf4, 13, 14))
				return false;
		}
		if (a != b && !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f,
							 a, b))
			return false;
		switch (op) {
		case OP_VADDI32X4:
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xfe, a, c))
				return false;
			break;
		case OP_VSUBI32X4:
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xfa, a, c))
				return false;
			break;
		case OP_VMULI32X4:
			if ((ctx->simd_caps & JIT_SIMD_CAP_SSE41) != 0) {
				if (!jit_x86_64_put_sse_rr(ctx, 0x66, 2, 0x40,
							  a, c))
					return false;
			} else {
				if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xf4,
							  a, c) ||
				    !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x70,
							  a, a))
					return false;
				ASM { IB(0x88); }
				if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x70,
							  13, 13))
					return false;
				ASM { IB(0x88); }
				if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x62,
							  a, 13))
					return false;
			}
			break;
		case OP_VAND128:
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xdb, a, c))
				return false;
			break;
		case OP_VOR128:
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xeb, a, c))
				return false;
			break;
		default:
			if (!jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0xef, a, c))
				return false;
			break;
		}
                break;
	case OP_VADDF32X4:
	case OP_VSUBF32X4:
	case OP_VMULF32X4:
	case OP_VDIVF32X4:
	{
		uint8_t opcode = op == OP_VADDF32X4 ? 0x58 :
			op == OP_VSUBF32X4 ? 0x5c :
			op == OP_VMULF32X4 ? 0x59 : 0x5e;
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			if (!jit_x86_64_put_vex_rrr(ctx, 1, 0, opcode,
						      a, b, c))
				return false;
		} else {
			if (a != b && !jit_x86_64_put_sse_rr(ctx, 0x66, 1,
							     0x6f, a, b))
				return false;
			if (!jit_x86_64_put_sse_rr(ctx, 0, 1, opcode, a, c))
				return false;
		}
		break;
	}
        case OP_VSHLI32X4:
        case OP_VSHRI32X4:
		if ((ctx->simd_caps & JIT_SIMD_CAP_AVX) != 0) {
			if (!jit_x86_64_put_vex_shift(ctx,
					op == OP_VSHLI32X4 ? 6 : 2,
					a, b, (uint8_t)c))
				return false;
			break;
		}
		if (a != b && !jit_x86_64_put_sse_rr(ctx, 0x66, 1, 0x6f,
							 a, b))
			return false;
		ASM { IB(0x66); }
		if ((a & 8) != 0) { ASM { IB(0x41); } }
		ASM { IB(0x0f); IB(0x72);
		      IB((uint8_t)((op == OP_VSHLI32X4 ? 0xf0 : 0xd0) |
				   (a & 7))); IB((uint8_t)c); }
                break;
        default:
                assert(NEVER_COME_HERE);
                return false;
        }

	return true;

broken_vreg:
	rt_error(ctx->env, BROKEN_BYTECODE);
	return false;
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
                        /* %rsp = 16n + 8 */

                        /* pushq %rax */                        IB(0x50);
                        /* pushq %rbx */                        IB(0x53);
                        /* pushq %rcx */                        IB(0x51);
                        /* pushq %rdx */                        IB(0x52);
                        /* pushq %rdi */                        IB(0x57);
                        /* pushq %rsi */                        IB(0x56);
                        /* pushq %r12 */                        IB(0x41); IB(0x54);
                        /* pushq %r13 */                        IB(0x41); IB(0x55);
                        /* pushq %r14 */                        IB(0x41); IB(0x56);
                        /* pushq %r15 */                        IB(0x41); IB(0x57);

                        /* align stack to 16 bytes */
                        /* sub rsp, 8 */                        IB(0x48); IB(0x83); IB(0xEC); IB(0x08);

                        /* r14 = env */
                        /* movq %rcx, %r14 */                   IB(0x49); IB(0x89); IB(0xCE);

                        /* r15 = *&env->frame->tmpvar[0] */
                        /* movq (%r14), %rax */                 IB(0x49); IB(0x8b); IB(0x06);
                        /* movq (%rax), %r15 */                 IB(0x4c); IB(0x8b); IB(0x38);

                        /* r13 = exception_handler */
                        /* movabs (ctx->code + 10), %r13 */     IB(0x49); IB(0xbd); IQ((uint64_t)(intptr_t)((uint8_t*)ctx->code + 10));

                        /* Skip an exception handler. */
                        /* jmp exception_handler_end */         IB(0xeb); IB(0x1a);
                }

               /* Put an exception handler. */
                ctx->exception_code = ctx->code;
                ASM {
                /* exception_handler: */
                        /* addq $8, %rsp (align back) */        IB(0x48); IB(0x83); IB(0xC4); IB(0x08);
                        /* popq %r15 */                         IB(0x41); IB(0x5f);
                        /* popq %r14 */                         IB(0x41); IB(0x5e);
                        /* popq %r13 */                         IB(0x41); IB(0x5d);
                        /* popq %r12 */                         IB(0x41); IB(0x5c);
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

                        /* r14 = env */
                        /* movq %rdi, %r14 */                   IB(0x49); IB(0x89); IB(0xfe);

                        /* r15 = *&env->frame->tmpvar[0] */
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
		case OP_VINDEX_HINT:
			if (!jit_visit_vindex_hint_op(ctx)) return false;
			break;
		case OP_SUBJNZ:
			if (!jit_visit_subjnz_op(ctx)) return false;
			break;
		case OP_VORI32X4I:
			if (!jit_visit_vori32x4i_op(ctx)) return false;
			break;
		case OP_VFMAF32X4:
			if (!jit_visit_vfmaf32x4_op(ctx)) return false;
			break;
		case OP_VCMPI32X4:
			if (!jit_visit_vcmp_op(ctx, false)) return false;
			break;
		case OP_VCMPF32X4:
			if (!jit_visit_vcmp_op(ctx, true)) return false;
			break;
		case OP_VSELECT128:
			if (!jit_visit_vselect128_op(ctx)) return false;
			break;
		case OP_VMASKSTOREI32X4:
			if (!jit_visit_vmaskstorei32x4_op(ctx)) return false;
			break;
		case OP_VINDUCTF32X4:
			if (!jit_visit_vinductf32x4_op(ctx)) return false;
			break;
		case OP_VGATHERI32X4_CHECKED:
			if (!jit_visit_vgatheri32x4_checked_op(ctx)) return false;
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
		case OP_VCVTI32F32X4:
		case OP_VCVTF32I32X4:
                        if (!jit_visit_vector_op(ctx, opcode))
                                return false;
                        break;
		default:
			return false; /* interpreter fallback for newer bytecode */
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
                        /* popq %r12 */                  IB(0x41); IB(0x5c);
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
        intptr_t wide_offset;
        uint32_t i;

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
        wide_offset = (intptr_t)target_code -
                      (intptr_t)ctx->branch_patch[patch_index].code;
        if (wide_offset < (-2147483647L - 1L) || wide_offset > 2147483647L) {
                rt_error(ctx->env, "Branch target too far.");
                return false;
        }
        offset = (int)wide_offset;

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

#endif /* defined(NOCT_ARCH_X86_64) && defined(NOCT_USE_JIT) */
