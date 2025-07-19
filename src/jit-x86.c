/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * JIT (x86): Just-In-Time native code generation
 */

#include "c89compat.h"	/* ARCH_X86 */

#if defined(ARCH_X86) && defined(USE_JIT)

#include "runtime.h"
#include "jit.h"
#include "execution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* False asseretion */
#define JIT_OP_NOT_IMPLEMENTED	0
#define NEVER_COME_HERE		0

/* PC entry size. */
#define PC_ENTRY_MAX		2048

/* Branch pathch size. */
#define BRANCH_PATCH_MAX	2048

/* Branch patch type */
#define PATCH_JMP		0
#define PATCH_JE		1
#define PATCH_JNE		2

/* Generated code. */
static uint8_t *jit_code_region;
static uint8_t *jit_code_region_cur;
static uint8_t *jit_code_region_tail;

/* Forward declaration */
static bool jit_visit_bytecode(struct jit_context *ctx);
static bool jit_patch_branch(struct jit_context *ctx, int patch_index);

/*
 * Generate a JIT-compiled code for a function.
 */
bool
jit_build(
	  struct rt_env *rt,
	  struct rt_func *func)
{
	struct jit_context ctx;
	int i;

	/* If the first call, map a memory region for the generated code. */
	if (jit_code_region == NULL) {
		if (!jit_map_memory_region((void **)&jit_code_region, JIT_CODE_MAX)) {
			noct_error(rt, "Memory mapping failed.");
			return false;
		}
		jit_code_region_cur = jit_code_region;
		jit_code_region_tail = jit_code_region + JIT_CODE_MAX;
	}

	/* Make a context. */
	memset(&ctx, 0, sizeof(struct jit_context));
	ctx.code_top = jit_code_region_cur;
	ctx.code_end = jit_code_region_tail;
	ctx.code = ctx.code_top;
	ctx.rt = rt;
	ctx.func = func;

	/* Make code writable and non-executable. */
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

	/* Make code executable and non-writable. */
	jit_map_executable(jit_code_region, JIT_CODE_MAX);

	func->jit_code = (bool (*)(struct rt_env *))ctx.code_top;

	return true;
}

/*
 * Free a JIT-compiled code for a function.
 */
void
jit_free(
	 struct rt_env *rt,
	 struct rt_func *func)
{
	UNUSED_PARAMETER(rt);
	UNUSED_PARAMETER(func);

	/* XXX: */
}

/*
 * Assembler output functions
 */

/* Serif */
#define ASM

/* Put a instruction byte. */
#define IB(b)			if (!jit_put_byte(ctx, b)) return false
static NOCT_INLINE bool
jit_put_byte(
	struct jit_context *ctx,
	uint8_t b)
{
	if ((uint8_t *)ctx->code + 1 > (uint8_t *)ctx->code_end) {
		noct_error(ctx->rt, "Code too big.");
		return false;
	}

	*(uint8_t *)ctx->code = b;
	ctx->code = (uint8_t *)ctx->code + 1;
	return true;
}

/* Put a instruction word. */
#define IW(b)			if (!jit_put_word(ctx, w)) return false
static NOCT_INLINE bool
jit_put_word(
	struct jit_context *ctx,
	uint16_t w)
{
	if ((uint8_t *)ctx->code + 2 > (uint8_t *)ctx->code_end) {
		noct_error(ctx->rt, "Code too big.");
		return false;
	}

	*(uint8_t *)ctx->code = (uint8_t)(w & 0xff);
	ctx->code = (uint8_t *)ctx->code + 1;

	*(uint8_t *)ctx->code = (uint8_t)((w >> 8) & 0xff);
	ctx->code = (uint8_t *)ctx->code + 1;

	return true;
}

/* Put a instruction double word. */
#define ID(d)			if (!jit_put_dword(ctx, d)) return false
static NOCT_INLINE bool
jit_put_dword(
	struct jit_context *ctx,
	uint32_t dw)
{
	if ((uint8_t *)ctx->code + 4 > (uint8_t *)ctx->code_end) {
		noct_error(ctx->rt, "Code too big.");
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

/*
 * Templates
 */

#define ASM_BINARY_OP(f)											\
	/* if (!f(rt, dst, src1, src2)) return false; */							\
	ASM {													\
		/* ebp-4: &rt->frame->tmpvar[0] */								\
		/* ebp-8: rt */											\
		/* ebp-12: exception_handler */									\
														\
		/* movl $src2, %eax */		IB(0xb8); ID((uint32_t)src2); 					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl $src1, %eax */		IB(0xb8); ID((uint32_t)src1); 					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl $dst, %eax */		IB(0xb8); ID((uint32_t)dst); 					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl -8(%ebp), %eax */	IB(0x8b); IB(0x45); IB(0xf8);					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl $f, %eax */		IB(0xb8); ID((uint32_t)f);					\
		/* call *%eax */		IB(0xff); IB(0xd0);						\
		/* addl $16, %esp */		IB(0x83); IB(0xc4); IB(16);					\
														\
		/* cmpl $0, %eax */		IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */			IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */		IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
	}

#define ASM_UNARY_OP(f)												\
	/* if (!f(rt, dst, src)) return false; */								\
	ASM {													\
		/* ebp-4: &rt->frame->tmpvar[0] */								\
		/* ebp-8: rt */											\
		/* ebp-12: exception_handler */									\
														\
		/* movl $src, %eax */		IB(0xb8); ID((uint32_t)src); 					\
		/* push %eax */			IB(0x50);							\
														\
		/* movl $dst, %eax */		IB(0xb8); ID((uint32_t)dst); 					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl -8(%ebp), %eax */	IB(0x8b); IB(0x45); IB(0xf8);					\
		/* pushl %eax */		IB(0x50);							\
														\
		/* movl $f, %eax */		IB(0xb8); ID((uint32_t)f);					\
		/* call *%eax */		IB(0xff); IB(0xd0);						\
		/* addl $12, %esp */		IB(0x83); IB(0xc4); IB(12);					\
														\
		/* cmpl $0, %eax */		IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */			IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */		IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
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
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $line, %eax */		IB(0xb8); ID(line);
		/* movl -8(%ebp), %ebx */	IB(0x8b); IB(0x5d); IB(0xf8);
		/* movl %eax, 4(%ebx) */	IB(0x89); IB(0x43); IB(0x04);
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
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $dst, %eax */		IB(0xb8); ID((uint32_t)dst);
		/* movl $src, %ebx */		IB(0xbb); ID((uint32_t)src);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* addl -4(%ebp), %ebx */	IB(0x03); IB(0x5d); IB(0xfc);
		/* movl (%ebx), %ecx */		IB(0x8b); IB(0x0b);
		/* movl 4(%ebx), %edx */	IB(0x8b); IB(0x53); IB(0x04);
		/* movl %ecx, (%eax) */		IB(0x89); IB(0x08);
		/* movl %edx, 4(%eax) */	IB(0x89); IB(0x50); IB(0x04);
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
		/* ebp-4: &rt->frame->tmpvar[0] */								\
		/* ebp-8: rt */											\
		/* ebp-12: exception_handler */									\

		/* movl $dst, %eax */		IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* movl $0, (%eax) */		IB(0xc7); IB(0x00); ID(0);
		/* movl $val, 4(%eax) */	IB(0xc7); IB(0x40); IB(0x04); ID(val);
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
		/* ebp-4: &rt->frame->tmpvar[0] */								\
		/* ebp-8: rt */											\
		/* ebp-12: exception_handler */									\

		/* movl $dst, %eax */		IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* movl $1, (%eax) */		IB(0xc7); IB(0x00); ID(1);
		/* movl $val, 4(%eax) */	IB(0xc7); IB(0x40); IB(0x04); ID(val);
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

	/* rt_make_string(rt, &rt->frame->tmpvar[dst], val); */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $val, %eax */			IB(0xb8); ID((uint32_t)val);
		/* pushl %eax */			IB(0x50);
		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */		IB(0x03); IB(0x45); IB(0xfc);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $noct_make_string, %eax */	IB(0xb8); ID((uint32_t)noct_make_string);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $12, %esp */			IB(0x83); IB(0xc4); IB(12);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);
		/* jne next */				IB(0x75); IB(0x03);
		/* jmp -12(%ebp) */			IB(0xff); IB(0x65); IB(0xf4);
		/* next:*/
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

	/* rt_make_empty_array(rt, &rt->frame->tmpvar[dst]); */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */		IB(0x03); IB(0x45); IB(0xfc);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $noct_make_empty_array, %eax */	IB(0xb8); ID((uint32_t)noct_make_empty_array);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $8, %esp */			IB(0x83); IB(0xc4); IB(8);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);
		/* jne next */				IB(0x75); IB(0x03);
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);
		/* next:*/
	/* next: */
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

	/* rt_make_empty_dict(rt, &rt->frame->tmpvar[dst]); */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $noct_make_empty_dict, %eax */	IB(0xb8); ID((uint32_t)noct_make_empty_dict);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $8, %esp */			IB(0x83); IB(0xc4); IB(8);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);
		/* jne next */				IB(0x75); IB(0x03);
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);
		/* next:*/
	/* next: */
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
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* addl -4(%ebp), %eax */		IB(0x03); IB(0x45); IB(0xfc);
		/* incl 4(%eax) */			IB(0xff); IB(0x40); IB(0x04);
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
	ASM_UNARY_OP(rt_neg_helper);

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

	src1 *= (int)sizeof(struct rt_value);
	src2 *= (int)sizeof(struct rt_value);

	/* src1 - src2 */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src1, %eax */		IB(0xb8); ID((uint32_t)src1);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);

		/* movl $src2, %ebx */		IB(0xbb); ID((uint32_t)src2);
		/* addl -4(%ebp), %ebx */	IB(0x03); IB(0x5d); IB(0xfc);

		/* movl 4(%eax), %ecx */	IB(0x8b); IB(0x48); IB(0x04);
		/* movl 4(%ebx), %edx */	IB(0x8b); IB(0x53); IB(0x04);
		/* cmpl %ecx, %edx */		IB(0x39); IB(0xca);
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
	const char *src;

	CONSUME_TMPVAR(dst);
	CONSUME_STRING(src);

	/* if (!rt_loadsymbol_helper(rt, dst, src)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src, %eax */			IB(0xb8); ID((uint32_t)src);
		/* push %eax */				IB(0x50);
		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_loadsymbol_helper, %eax */	IB(0xb8); ID((uint32_t)rt_loadsymbol_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $12, %esp */			IB(0x83); IB(0xc4); IB(12);

		/* cmpl $0, %eax */		IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */			IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */		IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
	}

	return true;
}

/* Visit a ROP_STORESYMBOL instruction. */
static NOCT_INLINE bool
jit_visit_storesymbol_op(
	struct jit_context *ctx)
{
	const char *dst;
	int src;

	CONSUME_STRING(dst);
	CONSUME_TMPVAR(src);

	/* if (!rt_storesymbol_helper(rt, dst, src)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src, %eax */			IB(0xb8); ID((uint32_t)src);
		/* push %eax */				IB(0x50);
		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_storesymbol_helper, %eax */	IB(0xb8); ID((uint32_t)rt_storesymbol_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $12, %esp */			IB(0x83); IB(0xc4); IB(12);

		/* cmpl $0, %eax */		IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */			IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */		IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
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
	const char *field;

	CONSUME_TMPVAR(dst);
	CONSUME_TMPVAR(dict);
	CONSUME_STRING(field);

	/* if (!rt_loaddot_helper(rt, dst, dict, field)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl field, %eax */			IB(0xb8); ID((uint32_t)field);
		/* push %eax */				IB(0x50);
		/* movl dict, %eax */			IB(0xb8); ID((uint32_t)dict);
		/* push %eax */				IB(0x50);
		/* movl $dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_loaddot_helper, %eax */	IB(0xb8); ID((uint32_t)rt_loaddot_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $16, %esp */			IB(0x83); IB(0xc4); IB(16);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */				IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
	}

	return true;
}

/* Visit a ROP_STOREDOT instruction. */
static NOCT_INLINE bool
jit_visit_storedot_op(
	struct jit_context *ctx)
{
	int dict;
	const char *field;
	int src;

	CONSUME_TMPVAR(dict);
	CONSUME_STRING(field);
	CONSUME_TMPVAR(src);

	/* if (!jit_storedot_helper(rt, dict, field, src)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src, %eax */			IB(0xb8); ID((uint32_t)src);
		/* push %eax */				IB(0x50);
		/* movl field, %eax */			IB(0xb8); ID((uint32_t)field);
		/* push %eax */				IB(0x50);
		/* movl dict, %eax */			IB(0xb8); ID((uint32_t)dict);
		/* push %eax */				IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_storedot_helper, %eax */	IB(0xb8); ID((uint32_t)rt_storedot_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $16, %esp */			IB(0x83); IB(0xc4); IB(16);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */				IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
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
	uint32_t arg_addr;
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
		arg_addr = (uint32_t)(intptr_t)ctx->code;
		for (i = 0; i < arg_count; i++) {
			*(int *)ctx->code = arg[i];
			ctx->code = (uint8_t *)ctx->code + 4;
		}
	} else {
		arg_addr = 0;
	}

	/* if (!rt_call_helper(rt, dst, func, arg_count, arg)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $arg_addr, %eax */		IB(0xb8); ID(arg_addr);
		/* pushl %eax */			IB(0x50);
		/* movl $arg_count, %eax */		IB(0xb8); ID((uint32_t)arg_count);
		/* pushl %eax */			IB(0x50);
		/* movl func, %eax */			IB(0xb8); ID((uint32_t)func);
		/* pushl %eax */			IB(0x50);
		/* movl dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_call_helper, %eax */	IB(0xb8); ID((uint32_t)rt_call_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $20, %esp */			IB(0x83); IB(0xc4); IB(20);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */				IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
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
	uint32_t arg_addr;
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
	arg_addr = (uint32_t)(intptr_t)ctx->code;
	for (i = 0; i < arg_count; i++) {
		*(int *)ctx->code = arg[i];
		ctx->code = (uint8_t *)ctx->code + 4;
	}

	/* if (!rt_thiscall_helper(rt, dst, obj, symbol, arg_count, arg)) return false; */
	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $arg_addr, %eax */		IB(0xb8); ID(arg_addr);
		/* pushl %eax */			IB(0x50);
		/* movl $arg_count, %eax */		IB(0xb8); ID((uint32_t)arg_count);
		/* pushl %eax */			IB(0x50);
		/* movl symbol, %eax */			IB(0xb8); ID((uint32_t)symbol);
		/* pushl %eax */			IB(0x50);
		/* movl obj, %eax */			IB(0xb8); ID((uint32_t)obj);
		/* pushl %eax */			IB(0x50);
		/* movl dst, %eax */			IB(0xb8); ID((uint32_t)dst);
		/* pushl %eax */			IB(0x50);
		/* movl -8(%ebp), %eax */		IB(0x8b); IB(0x45); IB(0xf8);
		/* pushl %eax */			IB(0x50);
		/* movl $rt_thiscall_helper, %eax */	IB(0xb8); ID((uint32_t)rt_thiscall_helper);
		/* call *%eax */			IB(0xff); IB(0xd0);
		/* addl $24, %esp */			IB(0x83); IB(0xc4); IB(24);

		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);					\
		/* jne next */				IB(0x75); IB(0x03);						\
		/* jmp 8(%ebp) */			IB(0xff); IB(0x65); IB(0x08);					\
		/* next:*/											\
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
		noct_error(ctx->rt, BROKEN_BYTECODE);
		return false;
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JMP;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		/* jmp 5 */	IB(0xe9); ID(0);
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
		noct_error(ctx->rt, BROKEN_BYTECODE);
		return false;
	}

	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src, %eax */		IB(0xb8); ID((uint32_t)src);
		/* shll $3, %eax */		IB(0xc1); IB(0xe0); IB(0x03);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* movl 4(%eax), %eax */	IB(0x8b); IB(0x40); IB(0x04);

		/* Compare: rt->frame->tmpvar[dst].val.i == 0 */
		/* cmpl $0, %eax */			IB(0x83); IB(0xf8); IB(0x00);
	}
	
	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JNE;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		/* jne 6 */				IB(0x0f); IB(0x84); ID(0);
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
		noct_error(ctx->rt, BROKEN_BYTECODE);
		return false;
	}

	ASM {
		/* ebp-4: &rt->frame->tmpvar[0] */
		/* ebp-8: rt */
		/* ebp-12: exception_handler */

		/* movl $src, %eax */		IB(0xb8); ID((uint32_t)src);
		/* shll $3, %eax */		IB(0xc1); IB(0xe0); IB(0x03);
		/* addl -4(%ebp), %eax */	IB(0x03); IB(0x45); IB(0xfc);
		/* movl 4(%eax), %eax */	IB(0x8b); IB(0x40); IB(0x04);

		/* Compare: rt->frame->tmpvar[dst].val.i == 0 */
		/* cmpl $0, %eax */		IB(0x83); IB(0xf8); IB(0x00);
	}
	
	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JE;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		/* je 6 */			IB(0x0f); IB(0x85); ID(0);
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
		noct_error(ctx->rt, BROKEN_BYTECODE);
		return false;
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_JE;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		/* je 6 */			IB(0x0f); IB(0x84); ID(0);
	}

	return true;
}

/* Visit a bytecode of a function. */
bool
jit_visit_bytecode(
	struct jit_context *ctx)
{
	uint8_t opcode;

	/* Put a prologue. */
	ASM {
	/* prologue: */
		/* mov 4(%esp), %eax; rt */		IB(0x8b); IB(0x44); IB(0x24); IB(0x04);

		/* pushl %ebx */			IB(0x53);
		/* pushl %ecx */			IB(0x51);
		/* pushl %edx */			IB(0x52);
		/* pushl %edi */			IB(0x57);
		/* pushl %esi */			IB(0x56);
		/* pushl %ebp */			IB(0x55);

		/* movl %esp, %ebp */			IB(0x89); IB(0xe5);
		/* subl $16, %esp */			IB(0x83); IB(0xec); IB(0x0c);

		/* (ebp-8): rt */
		/* movl %eax, -8(%ebp) */		IB(0x89); IB(0x45); IB(0xf8);

		/* (ebp-4): &rt->frame->tmpvar[0] */
		/* movl (%eax), %eax */			IB(0x8b); IB(0x00);
		/* movl (%eax), %eax */			IB(0x8b); IB(0x00);
		/* movl %eax, -4(%ebp) */		IB(0x89); IB(0x45); IB(0xfc);

		/* (ebp-12): exception_handler */
		/* movl $(ctx->code + 10), -12(%ebp) */	IB(0xc7); IB(0x45); IB(0xf4); ID((uint32_t)((uint8_t *)ctx->code + 10));

		/* Skip an exception handler. */
		/* jmp exception_handler_end */		IB(0xeb); IB(0x0f);
	}

	/* Put an exception handler. */
	ctx->exception_code = ctx->code;
	ASM {
	/* exception_handler: */
		/* addl $16, %esp */	IB(0x83); IB(0xc4); IB(0x0c);
		/* popl %ebp */ 	IB(0x5d);
		/* popl %esi */ 	IB(0x5e);
		/* popl %edi */ 	IB(0x5f);
		/* popl %edx */ 	IB(0x5a);
		/* popl %ecx */		IB(0x59);
		/* popl %ebx */		IB(0x5b);
		/* movl $0, %eax */	IB(0xb8); ID(0);
		/* ret */		IB(0xc3);
	/* exception_handler_end: */
	}

	/* Put a body. */
	while (ctx->lpc < ctx->func->bytecode_size) {
		/* Save LPC and addr. */
		if (ctx->pc_entry_count >= PC_ENTRY_MAX) {
			noct_error(ctx->rt, "Too big code.");
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
			if (!jit_visit_eq_op(ctx))
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
			if (!jit_visit_jmpiftrue_op(ctx))
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
	/* epilogue: */
		/* addl $16, %esp */	IB(0x83); IB(0xc4); IB(0x0c);
		/* popl %ebp */ 	IB(0x5d);
		/* popl %esi */ 	IB(0x5e);
		/* popl %edi */ 	IB(0x5f);
		/* popl %edx */ 	IB(0x5a);
		/* popl %ecx */		IB(0x59);
		/* popl %ebx */		IB(0x5b);
		/* movl $1, %eax */	IB(0xb8); ID(1);
		/* ret */		IB(0xc3);
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
		noct_error(ctx->rt, "Branch target not found.");
		return false;
	}

	/* Calc a branch offset. */
	offset = (intptr_t)target_code - (intptr_t)ctx->branch_patch[patch_index].code;

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
