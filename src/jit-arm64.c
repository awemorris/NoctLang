/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * JIT (arm64): Just-In-Time native code generation
 */

#include "c89compat.h"	/* ARCH_ARM64 */

#if defined(ARCH_ARM64) && defined(USE_JIT)

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

/* Branch patch type */
#define PATCH_BAL		0
#define PATCH_BEQ		1
#define PATCH_BNE		2

/* Generated code. */
static uint32_t *jit_code_region;
static uint32_t *jit_code_region_cur;
static uint32_t *jit_code_region_tail;

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
			rt_error(env, _("Memory mapping failed."));
			return false;
		}
		jit_code_region_cur = jit_code_region;
		jit_code_region_tail = jit_code_region + JIT_CODE_MAX / 4;
	}

	/* Make a context. */
	memset(&ctx, 0, sizeof(struct jit_context));
	ctx.code_top = jit_code_region_cur;
	ctx.code_end = jit_code_region_tail;
	ctx.code = ctx.code_top;
	ctx.env = env;
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

/* Decoration */
#define ASM

/* Registers */
#define REG_X0		0
#define REG_X1		1
#define REG_X2		2
#define REG_X3		3
#define REG_X4		4
#define REG_X5		5
#define REG_X6		6
#define REG_X7		7
#define REG_X8		8
#define REG_X9		9
#define REG_X10		10
#define REG_X11		11
#define REG_X12		12
#define REG_X13		13
#define REG_X14		14
#define REG_X15		15
#define REG_X16		16
#define REG_X17		17
#define REG_X18		18
#define REG_X19		19
#define REG_X20		20
#define REG_X21		21
#define REG_X22		22
#define REG_X23		23
#define REG_X24		24
#define REG_X25		25
#define REG_X26		26
#define REG_X27		27
#define REG_X28		28
#define REG_X29		29
#define REG_X30		30
#define REG_XZR		31
#define REG_SP		31

/* Immediate */
#define IMM8(v)		(uint32_t)(v)
#define IMM9(v)		(uint32_t)(v)
#define IMM12(v)	(uint32_t)(v)
#define IMM16(v)	(uint32_t)(v)
#define IMM19(v)	(uint32_t)(v)

/* Shift */
#define LSL_0		0
#define LSL_16		16
#define LSL_32		32
#define LSL_48		48

/* Put a instruction word. */
static NOCT_INLINE bool
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

/* movz */
#define MOVZ(rd, imm, lsl)		if (!jit_put_movz(ctx, rd, imm, lsl)) return false
static NOCT_INLINE bool
jit_put_movz(
	struct jit_context *ctx,
	uint32_t n,
	uint32_t imm,
	uint32_t lsl)
{
	if (!jit_put_word(ctx,
			  0xd2800000 | 		/* movz */
			  ((lsl / 16) << 21) |	/* shift */
			  (imm << 5) |		/* imm16 */
			  n))			/* reg */
		return false;
	return true;
}

/* movk */
#define MOVK(rd, imm, lsl)		if (!jit_put_movk(ctx, rd, imm, lsl)) return false
static NOCT_INLINE bool
jit_put_movk(
	struct jit_context *ctx,
	uint32_t n,
	uint32_t imm,
	uint32_t lsl)
{
	if (!jit_put_word(ctx,
			  0xf2800000 |		/* movk */
			  ((lsl / 16) << 21) |	/* shift */
			  (imm << 5) |		/* imm16 */
			  n))			/* reg */
		return false;
	return true;
}

/* ldr imm */
#define LDR(rd, rs)			if (!jit_put_ldr_imm(ctx, rd, rs, 0)) return false
#define LDR_IMM(rd, rs, imm)		if (!jit_put_ldr_imm(ctx, rd, rs, imm)) return false
static bool
jit_put_ldr_imm(
	struct jit_context *ctx,
	uint32_t rd,
	uint32_t rs,
	uint32_t imm)
{
	if (!jit_put_word(ctx,
			  0xf9400000 |			/* ldr */
			  (rs << 5) |			/* rd */
			  (rd) |			/* rs */
			  (((imm / 8) & 0x1ff) << 10)))	/* imm */
		return false;
	return true;
}

/* str imm */
#define STR(rs, rd)			if (!jit_put_str_imm(ctx, rs, rd, 0)) return false
#define STR_IMM(rs, rd, imm)		if (!jit_put_str_imm(ctx, rs, rd, imm)) return false
static bool
jit_put_str_imm(
	struct jit_context *ctx,
	uint32_t rs,
	uint32_t rd,
	uint32_t imm)
{
	if (!jit_put_word(ctx,
			  0xf9000000 |			/* movk */
			  (rs)	|			/* rs */
			  (rd << 5) |			/* rd */
			  (((imm / 8) & 0x1ff) << 10)))	/* imm */
		return false;
	return true;
}

/* ldp xN, xM, [sp], #16 */
#define LDP_POP(ra, rb)			if (!jit_put_ldp_pop(ctx, ra, rb)) return false
static NOCT_INLINE bool
jit_put_ldp_pop(
	struct jit_context *ctx,
	uint32_t ra,
	uint32_t rb)
{
	if (!jit_put_word(ctx,
			  0xa8c103e0 | 		/* ldp */
			  ra |			/* ra */
			  (rb << 10)))		/* rb */
		return false;
	return true;
}

/* stp xN, xM, [sp, #-16]! */
#define STP_PUSH(ra, rb)		if (!jit_put_stp_push(ctx, ra, rb)) return false
static NOCT_INLINE bool
jit_put_stp_push(
	struct jit_context *ctx,
	uint32_t ra,
	uint32_t rb)
{
	if (!jit_put_word(ctx,
			  0xa9bf03e0 | 		/* stp */
			  ra |			/* ra */
			  (rb << 10)))		/* rb */
		return false;
	return true;
}

/* add */
#define ADD(rd, ra, rb)			if (!jit_put_add(ctx, rd, ra, rb)) return false
static bool
jit_put_add(
	struct jit_context *ctx,
	uint32_t rd,
	uint32_t ra,
	uint32_t rb)
{
	if (!jit_put_word(ctx,
			  0x8b000000 |		/* add */
			  rd |			/* rd */
			  (ra << 5) |		/* ra */
			  (rb << 16)))		/* rb */
		return false;
	return true;
}

/* add_imm */
#define ADD_IMM(rd, rs, imm)		if (!jit_put_add_imm(ctx, rd, rs, imm)) return false
static bool
jit_put_add_imm(
	struct jit_context *ctx,
	uint32_t rd,
	uint32_t rs,
	uint32_t imm)
{
	if (!jit_put_word(ctx,
			  0x91000000 |			/* add */
			  rd |				/* rd */
			  (rs << 5) |			/* rs */
			  ((imm & 0xfff) << 10)))	/* imm */
		return false;
	return true;
}

/* lsl #4 */
#define LSL_4(rd, rs)			if (!jit_put_lsl4(ctx, rd, rs)) return false
static bool
jit_put_lsl4(
	struct jit_context *ctx,
	uint32_t rd,
	uint32_t rs)
{
	if (!jit_put_word(ctx,
			  0xd37cec00 |		/* madd */
			  rd |			/* rd */
			  (rs << 5)))		/* ra */
		return false;
	return true;
}

/* cmp_imm */
#define CMP_IMM(rs, imm)		if (!jit_put_cmp_imm(ctx, rs, imm)) return false
static bool
jit_put_cmp_imm(
	struct jit_context *ctx,
	uint32_t rs,
	uint32_t imm)
{
	if (!jit_put_word(ctx,
			  0xf100001f |			/* cmp */
			  (rs << 5) |			/* rs */
			  ((imm & 0xfff) << 10)))	/* imm */
		return false;
	return true;
}

/* cmp_w3_imm */
#define CMP_W3_IMM(imm)		if (!jit_put_cmp_w3_imm(ctx, imm)) return false
static bool
jit_put_cmp_w3_imm(
	struct jit_context *ctx,
	uint32_t imm)
{
	if (!jit_put_word(ctx,
			  0x7100007f |			/* cmp */
			  ((imm & 0xfff) << 10)))	/* imm */
		return false;
	return true;
}

/* cmp w3, w4 */
#define CMP_W3_W4()			if (!jit_put_cmp_w3_w4(ctx)) return false
static bool
jit_put_cmp_w3_w4(
	struct jit_context *ctx)
{
	if (!jit_put_word(ctx, 0x6b04007f))
		return false;
	return true;
}

/* BAL */
#define BAL(rel)		if (!jit_put_bal(ctx, rel)) return false
static NOCT_INLINE bool
jit_put_bal(
	struct jit_context *ctx,
	uint32_t rel)    
{
	if (!jit_put_word(ctx,
			  0x54000000 |		       			/* b.cond */
			  (0xe) |					/* always */
			  ((((uint32_t)(rel / 4)) & 0x7ffff) << 5)))	/* rel */
		return false;
	return true;
}

/* BEQ */
#define BEQ(rel)		if (!jit_put_beq(ctx, rel)) return false
static NOCT_INLINE bool
jit_put_beq(
	struct jit_context *ctx,
	uint32_t rel)    
{
	if (!jit_put_word(ctx,
			  0x54000000 |		       			/* b.cond */
			  (0x0) |					/* eq */
			  ((((uint32_t)(rel / 4)) & 0x7ffff) << 5)))	/* rel */
		return false;
	return true;
}

/* BNE */
#define BNE(rel)		if (!jit_put_bne(ctx, rel)) return false
static NOCT_INLINE bool
jit_put_bne(
	struct jit_context *ctx,
	uint32_t rel)    
{
	if (!jit_put_word(ctx,
			  0x54000000 |		       			/* b.cond */
			  (0x1) |					/* ne */
			  ((((uint32_t)(rel / 4)) & 0x7ffff) << 5)))	/* rel */
		return false;
	return true;
}

/* BLR */
#define BLR(rd)			if (!jit_put_blr(ctx, rd)) return false
static NOCT_INLINE bool
jit_put_blr(
	struct jit_context *ctx,
	uint32_t rd)
{
	if (!jit_put_word(ctx,
			  0xd63f0000 |		       	/* blr */
			  (rd << 5)))			/* rel */
		return false;
	return true;
}

/* ret */
#define RET()			if (!jit_put_ret(ctx)) return false
static NOCT_INLINE bool
jit_put_ret(
	struct jit_context *ctx)
{
	if (!jit_put_word(ctx,
			  0xd65f03c0)) 		/* ret */
		return false;
	return true;
}

/*
 * Templates
 */

#define ASM_BINARY_OP(f)											\
	ASM {													\
		STP_PUSH	(REG_X0, REG_X1);								\
		STP_PUSH	(REG_X30, REG_XZR);								\
														\
		/* Arg1 x0: rt */										\
														\
		/* Arg2 x1: dst */										\
		MOVZ		(REG_X1, IMM16(dst), LSL_0);							\
														\
		/* Arg3 x2: src1 */										\
		MOVZ		(REG_X2, IMM16(src1), LSL_0);							\
														\
		/* Arg4 x3: src2 */										\
		MOVZ		(REG_X3, IMM16(src2), LSL_0);							\
														\
		/* Call f(). */											\
		MOVZ		(REG_X4, IMM16(((uint64_t)(f)) & 0xffff), LSL_0); \
		MOVK		(REG_X4, IMM16((((uint64_t)(f)) >> 16) & 0xffff), LSL_16); \
		MOVK		(REG_X4, IMM16((((uint64_t)(f)) >> 32) & 0xffff), LSL_32); \
		MOVK		(REG_X4, IMM16((((uint64_t)(f)) >> 48) & 0xffff), LSL_48); \
		BLR		(REG_X4);									\
														\
		/* If failed: */										\
		CMP_IMM		(REG_X0, IMM12(0));								\
		LDP_POP		(REG_X30, REG_X1);								\
		LDP_POP		(REG_X0, REG_X1);								\
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));			\
	}

#define ASM_UNARY_OP(f)												\
	ASM {													\
		STP_PUSH	(REG_X0, REG_X1);								\
		STP_PUSH	(REG_X30, REG_XZR);								\
														\
		/* Arg1 x0: rt */										\
														\
		/* Arg2 x1: dst */										\
		MOVZ		(REG_X1, IMM16(dst), LSL_0);							\
														\
		/* Arg3 x2: src */										\
		MOVZ		(REG_X2, IMM16(src), LSL_0);							\
														\
		/* Call f(). */											\
		MOVZ		(REG_X3, IMM16(((uint64_t)f) & 0xffff), LSL_0);					\
		MOVK		(REG_X3, IMM16((((uint64_t)f) >> 16) & 0xffff), LSL_16);			\
		MOVK		(REG_X3, IMM16((((uint64_t)f) >> 32) & 0xffff), LSL_32);			\
		MOVK		(REG_X3, IMM16((((uint64_t)f) >> 48) & 0xffff), LSL_48);			\
		BLR		(REG_X3);									\
														\
		/* If failed: */										\
		CMP_IMM		(REG_X0, IMM12(0));								\
		LDP_POP		(REG_X30, REG_X1);								\
		LDP_POP		(REG_X0, REG_X1);								\
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));			\
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

	ASM {
		/* rt->line = line; */
		MOVZ		(REG_X2, IMM16(line & 0xffff), LSL_0);
		STR_IMM		(REG_X2, REG_X0, IMM9(8));
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
		/* x2 = dst_addr = &rt->frame->tmpvar[dst] */
		MOVZ	(REG_X2, IMM16(dst), LSL_0);
		ADD	(REG_X2, REG_X2, REG_X1);

		/* x3 = src_addr = &rt->frame->tmpvar[src] */
		MOVZ	(REG_X3, IMM16(src), LSL_0);	/* src */
		ADD	(REG_X3, REG_X3, REG_X1);

		/* *dst_addr = *src_addr */
		LDR_IMM	(REG_X4, REG_X3, 0);
		LDR_IMM	(REG_X5, REG_X3, 8);
		STR_IMM	(REG_X4, REG_X2, 0);
		STR_IMM	(REG_X5, REG_X2, 8);
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

	/* Set an integer constant. */
	ASM {
		/* x2 = &rt->frame->tmpvar[dst] */
		MOVZ	(REG_X2, IMM16(dst), LSL_0);	/* dst */
		ADD	(REG_X2, REG_X2, REG_X1);

		/* rt->frame->tmpvar[dst].type = RT_VALUE_INT */
		MOVZ	(REG_X3, IMM16(0), LSL_0);
		STR	(REG_X3, REG_X2);

		/* rt->frame->tmpvar[dst].val.i = val */
		MOVZ	(REG_X3, IMM16(val & 0xffff), LSL_0);
		MOVK	(REG_X3, IMM16((val >> 16) & 0xffff), LSL_16);
		STR_IMM	(REG_X3, REG_X2, IMM9(8));
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

	/* Set a floating-point constant. */
	ASM {
		/* x2 = &rt->frame->tmpvar[dst] */
		MOVZ	(REG_X2, IMM16(dst), LSL_0);	/* dst */
		ADD	(REG_X2, REG_X2, REG_X1);

		/* Assign rt->frame->tmpvar[dst].type = RT_VALUE_FLOAT. */
		MOVZ	(REG_X3, IMM16(NOCT_VALUE_FLOAT), LSL_0);
		STR	(REG_X3, REG_X2);

		/* Assign rt->frame->tmpvar[dst].val.f = val. */
		MOVZ	(REG_X3, IMM16(val & 0xffff), LSL_0);
		MOVK	(REG_X3, IMM16((val >> 16) & 0xffff), LSL_16);
		STR_IMM	(REG_X3, REG_X2, IMM9(8));
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
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);
		
		/* Arg1 x0: rt */

		/* Arg2 x1: &rt->frame->tmpvar[dst] */
		MOVZ		(REG_X2, IMM16(dst), LSL_0);	/* dst */
		ADD		(REG_X1, REG_X1, REG_X2);

		/* Arg3: x2: val */
		MOVZ		(REG_X2, IMM16(((uint64_t)val) & 0xffff), LSL_0);
		MOVK		(REG_X2, IMM16((((uint64_t)val >> 16)) & 0xffff), LSL_16);
		MOVK		(REG_X2, IMM16((((uint64_t)val >> 32)) & 0xffff), LSL_32);
		MOVK		(REG_X2, IMM16((((uint64_t)val >> 48)) & 0xffff), LSL_48);

		/* Call rt_make_string(). */
		MOVZ		(REG_X3, IMM16(((uint64_t)noct_make_string) & 0xffff), LSL_0);
		MOVK		(REG_X3, IMM16((((uint64_t)noct_make_string) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X3, IMM16((((uint64_t)noct_make_string) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X3, IMM16((((uint64_t)noct_make_string) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X3);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint32_t)(ptrdiff_t)((uint64_t)ctx->exception_code - (uint64_t)ctx->code)));
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
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: &rt->frame->tmpvar[dst] */
		MOVZ		(REG_X2, IMM16(dst), LSL_0);	/* dst */
		ADD		(REG_X1, REG_X1, REG_X2);

		/* Call rt_make_empty_array(). */
		MOVZ		(REG_X2, IMM16(((uint64_t)noct_make_empty_array) & 0xffff), LSL_0);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_array) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_array) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_array) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X2);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: &rt->frame->tmpvar[dst] */
		MOVZ		(REG_X2, IMM16(dst), LSL_0);	/* dst */
		ADD		(REG_X1, REG_X1, REG_X2);

		/* Call rt_make_empty_dict(). */
		MOVZ		(REG_X2, IMM16(((uint64_t)noct_make_empty_dict) & 0xffff), LSL_0);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_dict) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_dict) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X2, IMM16((((uint64_t)noct_make_empty_dict) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X2);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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

	/* Increment an integer. */
	ASM {
		/* Get &rt->frame->tmpvar[dst] at x3. */
		MOVZ	(REG_X2, IMM16(dst), LSL_0);			/* dst */
		ADD	(REG_X2, REG_X2, REG_X1);			/* x3 = &rt->frame->tmpvar[dst] = &rt->frame->tmpvar[dst].type */

		/* rt->frame->tmpvar[dst].val.i++ */
		LDR_IMM	(REG_X3, REG_X2, IMM9(8));			/* tmp = &rt->frame->tmpvar[dst].val.i */
		ADD_IMM	(REG_X3, REG_X3, IMM12(1));			/* tmp++ */
		STR_IMM	(REG_X3, REG_X2, IMM9(8));			/* rt->frame->tmpvar[dst].val.i = tmp */
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

	/* if (!rt_add_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_sub_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_mul_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_div_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_mod_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_and_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_or_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_xor_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_neg_helper(rt, dst, src)) return false; */
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

	/* if (!rt_not_helper(rt, dst, src)) return false; */
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

	/* if (!rt_lt_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_lte_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_eq_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_neq_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!rt_gte_helper(rt, dst, src1, src2)) return false; */
	ASM_BINARY_OP(rt_gte_helper);

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

	/* if (!rt_gt_helper(rt, dst, src1, src2)) return false; */
	ASM_BINARY_OP(rt_gt_helper);

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

	/* src1 == src2 */
	ASM {
		/* x3 = &rt->frame->tmpvar[src1].val.i */
		MOVZ		(REG_X3, IMM16(src1), LSL_0);	/* src1 */
		ADD		(REG_X3, REG_X3, REG_X1);
		LDR_IMM		(REG_X3, REG_X3, 8);

		/* x4 = &rt->frame->tmpvar[src2].val.i */
		MOVZ		(REG_X4, IMM16(src2), LSL_0);	/* src2 */
		ADD		(REG_X4, REG_X4, REG_X1);
		LDR_IMM		(REG_X4, REG_X4, 8);

		/* src1 == src2 */
		CMP_W3_W4	();
	}

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

	/* if (!rt_loadarray_helper(rt, dst, src1, src2)) return false; */
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

	/* if (!jit_loadsymbol_helper(rt, dst, src)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dst */
		MOVZ		(REG_X1, IMM16(dst), LSL_0);

		/* Arg3 x2: src */
		MOVZ		(REG_X2, IMM16(src & 0xffff), LSL_0);
		MOVK		(REG_X2, IMM16((src >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X2, IMM16((src >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X2, IMM16((src >> 48) & 0xffff), LSL_48);

		/* Call rt_loadsymbol_helper(). */
		MOVZ		(REG_X3, IMM16(((uint64_t)rt_loadsymbol_helper) & 0xffff), LSL_0);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_loadsymbol_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_loadsymbol_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_loadsymbol_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X3);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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

	/* if (!rt_storesymbol_helper(rt, dst, src)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dst */
		MOVZ		(REG_X1, IMM16(dst & 0xffff), LSL_0);
		MOVK		(REG_X1, IMM16((dst >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X1, IMM16((dst >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X1, IMM16((dst >> 48) & 0xffff), LSL_48);

		/* Arg3 x2: src */
		MOVZ		(REG_X2, IMM16(src), LSL_0);

		/* Call rt_storesymbol_helper(). */
		MOVZ		(REG_X3, IMM16(((uint64_t)rt_storesymbol_helper) & 0xffff), LSL_0);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_storesymbol_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_storesymbol_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X3, IMM16((((uint64_t)rt_storesymbol_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X3);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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

	/* if (!rt_loaddot_helper(rt, dst, dict, field)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dst */
		MOVZ		(REG_X1, IMM16(dst), LSL_0);

		/* Arg3 x2: dict */
		MOVZ		(REG_X2, IMM16(dict), LSL_0);

		/* Arg4 x3: field */
		MOVZ		(REG_X3, IMM16(field & 0xffff), LSL_0);
		MOVK		(REG_X3, IMM16((field >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X3, IMM16((field >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X3, IMM16((field >> 48) & 0xffff), LSL_48);

		/* Call rt_loaddot_helper(). */
		MOVZ		(REG_X4, IMM16(((uint64_t)rt_loaddot_helper) & 0xffff), LSL_0);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_loaddot_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_loaddot_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_loaddot_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X4);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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

	/* if (!jit_loaddot_helper(rt, dst, dict, field)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dict */
		MOVZ		(REG_X1, IMM16(dict), LSL_0);

		/* Arg3 x2: field */
		MOVZ		(REG_X2, IMM16(field & 0xffff), LSL_0);
		MOVK		(REG_X2, IMM16((field >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X2, IMM16((field >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X2, IMM16((field >> 48) & 0xffff), LSL_48);

		/* Arg4 x3: src */
		MOVZ		(REG_X3, IMM16(src), LSL_0);

		/* Call rt_storedot_helper(). */
		MOVZ		(REG_X4, IMM16(((uint64_t)rt_storedot_helper) & 0xffff), LSL_0);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_storedot_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_storedot_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X4, IMM16((((uint64_t)rt_storedot_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X4);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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
			BAL		(IMM12(4 + 4 * arg_count));
		}
		arg_addr = (uint64_t)(intptr_t)ctx->code;
		for (i = 0; i < arg_count; i++) {
			*(uint32_t *)ctx->code = (uint32_t)arg[i];
			ctx->code = (uint32_t *)ctx->code + 1;
		}
	} else {
		arg_addr = 0;
	}

	/* if (!rt_call_helper(rt, dst, func, arg_count, arg)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dst */
		MOVZ		(REG_X1, IMM16(dst), LSL_0);

		/* Arg3 x2: func */
		MOVZ		(REG_X2, IMM16(func), LSL_0);

		/* Arg4 x3: arg_count */
		MOVZ		(REG_X3, IMM16(arg_count), LSL_0);

		/* Arg5 x4: arg */
		MOVZ		(REG_X4, IMM16(arg_addr & 0xffff), LSL_0);
		MOVK		(REG_X4, IMM16((arg_addr >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X4, IMM16((arg_addr >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X4, IMM16((arg_addr >> 48) & 0xffff), LSL_48);

		/* Call rt_call_helper(). */
		MOVZ		(REG_X5, IMM16(((uint64_t)rt_call_helper) & 0xffff), LSL_0);
		MOVK		(REG_X5, IMM16((((uint64_t)rt_call_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X5, IMM16((((uint64_t)rt_call_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X5, IMM16((((uint64_t)rt_call_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X5);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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

	/* Embed arguments. */
	ASM {
		BAL		(IMM12(4 + 4 * arg_count));
	}
	arg_addr = (uint64_t)(intptr_t)ctx->code;
	for (i = 0; i < arg_count; i++) {
		*(uint32_t *)ctx->code = (uint32_t)arg[i];
		ctx->code = (uint32_t *)ctx->code + 1;
	}

	/* if (!rt_thiscall_helper(rt, dst, obj, symbol, arg_count, arg)) return false; */
	ASM {
		STP_PUSH	(REG_X0, REG_X1);
		STP_PUSH	(REG_X30, REG_XZR);

		/* Arg1 x0: rt */

		/* Arg2 x1: dst */
		MOVZ		(REG_X1, IMM16(dst), LSL_0);

		/* Arg3 x2: obj */
		MOVZ		(REG_X2, IMM16(obj), LSL_0);

		/* Arg4 x3: symbol */
		MOVZ		(REG_X3, IMM16((intptr_t)symbol & 0xffff), LSL_0);
		MOVK		(REG_X3, IMM16(((intptr_t)symbol >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X3, IMM16(((intptr_t)symbol >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X3, IMM16(((intptr_t)symbol >> 48) & 0xffff), LSL_48);

		/* Arg5 x4: argcount */
		MOVZ		(REG_X4, IMM16(arg_count), LSL_0);

		/* Arg6 x5: arg */
		MOVZ		(REG_X5, IMM16(arg_addr & 0xffff), LSL_0);
		MOVK		(REG_X5, IMM16((arg_addr >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X5, IMM16((arg_addr >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X5, IMM16((arg_addr >> 48) & 0xffff), LSL_48);

		/* Call rt_thiscall_helper(). */
		MOVZ		(REG_X6, IMM16(((uint64_t)rt_thiscall_helper) & 0xffff), LSL_0);
		MOVK		(REG_X6, IMM16((((uint64_t)rt_thiscall_helper) >> 16) & 0xffff), LSL_16);
		MOVK		(REG_X6, IMM16((((uint64_t)rt_thiscall_helper) >> 32) & 0xffff), LSL_32);
		MOVK		(REG_X6, IMM16((((uint64_t)rt_thiscall_helper) >> 48) & 0xffff), LSL_48);
		BLR		(REG_X6);

		/* If failed: */
		CMP_IMM		(REG_X0, IMM12(0));
		LDP_POP		(REG_X30, REG_X1);
		LDP_POP		(REG_X0, REG_X1);
		BEQ		(IMM19((uint64_t)ctx->exception_code - (uint64_t)ctx->code));
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
		noct_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BAL;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		BAL	(IMM19(0));
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
		noct_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}

	ASM {
		/* x3 = &rt->frame->tmpvar[src].val.i */
		MOVZ	(REG_X2, IMM16(src), LSL_0);
		LSL_4	(REG_X2, REG_X2);		/* src * sizeof(struct rt_value) */
		ADD	(REG_X2, REG_X2, REG_X1);
		LDR_IMM	(REG_X3, REG_X2, IMM9(8));

		/* Compare: rt->frame->tmpvar[dst].val.i == 1 */
		CMP_W3_IMM	(IMM12(0));
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BNE;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		BNE	(IMM19(0));
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
		noct_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}

	ASM {
		/* x3 = &rt->frame->tmpvar[src].val.i */
		MOVZ		(REG_X2, IMM16(src), LSL_0);
		LSL_4		(REG_X2, REG_X2);		/* src * sizeof(struct rt_value) */
		ADD		(REG_X2, REG_X2, REG_X1);
		LDR_IMM		(REG_X3, REG_X2, IMM9(8));

		/* Compare: rt->frame->tmpvar[dst].val.i == 0 */
		CMP_W3_IMM	(IMM12(0));
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BEQ;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		BEQ	(IMM19(0));
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
		noct_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}

	/* Patch later. */
	ctx->branch_patch[ctx->branch_patch_count].code = ctx->code;
	ctx->branch_patch[ctx->branch_patch_count].lpc = target_lpc;
	ctx->branch_patch[ctx->branch_patch_count].type = PATCH_BEQ;
	ctx->branch_patch_count++;

	ASM {
		/* Patched later. */
		BEQ	(IMM19(0));
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
		/* Push the general-purpose registers. */
		STP_PUSH	(REG_X29, REG_X30);
		STP_PUSH	(REG_X27, REG_X28);
		STP_PUSH	(REG_X25, REG_X26);
		STP_PUSH	(REG_X23, REG_X24);
		STP_PUSH	(REG_X21, REG_X22);
		STP_PUSH	(REG_X19, REG_X20);
		STP_PUSH	(REG_X17, REG_X18);
		STP_PUSH	(REG_X15, REG_X16);
		STP_PUSH	(REG_X13, REG_X14);
		STP_PUSH	(REG_X11, REG_X12);
		STP_PUSH	(REG_X9, REG_X10);
		STP_PUSH	(REG_X7, REG_X8);
		STP_PUSH	(REG_X5, REG_X6);
		STP_PUSH	(REG_X3, REG_X4);
		STP_PUSH	(REG_X1, REG_X2);
		STP_PUSH	(REG_XZR, REG_X0);

		/* x1 = *rt->frame = &rt->frame->tmpvar[0] */
		LDR		(REG_X1, REG_X0);
		LDR		(REG_X1, REG_X1);

		/* Skip an exception handler. */
		BAL		(IMM19(76));
	}

	/* Put an exception handler. */
	ctx->exception_code = ctx->code;
	ASM {
	/* EXCEPTION: */
		LDP_POP		(REG_X1, REG_X0);	/* x1 is dummy */
		LDP_POP		(REG_X1, REG_X2);
		LDP_POP		(REG_X3, REG_X4);
		LDP_POP		(REG_X5, REG_X6);
		LDP_POP		(REG_X7, REG_X8);
		LDP_POP		(REG_X9, REG_X10);
		LDP_POP		(REG_X11, REG_X12);
		LDP_POP		(REG_X13, REG_X14);
		LDP_POP		(REG_X15, REG_X16);
		LDP_POP		(REG_X17, REG_X18);
		LDP_POP		(REG_X19, REG_X20);
		LDP_POP		(REG_X21, REG_X22);
		LDP_POP		(REG_X23, REG_X24);
		LDP_POP		(REG_X25, REG_X26);
		LDP_POP		(REG_X27, REG_X28);
		LDP_POP		(REG_X29, REG_X30);
		MOVZ		(REG_X0, IMM16(0), LSL_0);
		RET		();
	}

	/* Put a body. */
	while (ctx->lpc < ctx->func->bytecode_size) {
		/* Save LPC and addr. */
		if (ctx->pc_entry_count >= PC_ENTRY_MAX) {
			noct_error(ctx->env, _("Code too big."));
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

	/* Put an epilogue. */
	ASM {
	/* EPILOGUE: */
		LDP_POP		(REG_X1, REG_X0);	/* x1 is dummy */
		LDP_POP		(REG_X1, REG_X2);
		LDP_POP		(REG_X3, REG_X4);
		LDP_POP		(REG_X5, REG_X6);
		LDP_POP		(REG_X7, REG_X8);
		LDP_POP		(REG_X9, REG_X10);
		LDP_POP		(REG_X11, REG_X12);
		LDP_POP		(REG_X13, REG_X14);
		LDP_POP		(REG_X15, REG_X16);
		LDP_POP		(REG_X17, REG_X18);
		LDP_POP		(REG_X19, REG_X20);
		LDP_POP		(REG_X21, REG_X22);
		LDP_POP		(REG_X23, REG_X24);
		LDP_POP		(REG_X25, REG_X26);
		LDP_POP		(REG_X27, REG_X28);
		LDP_POP		(REG_X29, REG_X30);
		MOVZ		(REG_X0, IMM16(1), LSL_0);
		RET		();
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
		noct_error(ctx->env, _("Branch target not found."));
		return false;
	}

	/* Calc a branch offset. */
	offset = (int)((intptr_t)target_code - (intptr_t)ctx->branch_patch[patch_index].code);

	/* Set the assembler cursor. */
	ctx->code = ctx->branch_patch[patch_index].code;

	/* Assemble. */
	if (ctx->branch_patch[patch_index].type == PATCH_BAL) {
		ASM {
			BAL	(IMM19(offset));
		}
	} else if (ctx->branch_patch[patch_index].type == PATCH_BEQ) {
		ASM {
			BEQ	(IMM19(offset));
		}
	} else if (ctx->branch_patch[patch_index].type == PATCH_BNE) {
		ASM {
			BNE	(IMM19(offset));
		}
	}

	return true;
}

#endif /* defined(ARCH_ARM64) && defined(USE_JIT) */
