/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Bytecode Interpreter
 */

#include "runtime.h"
#include "interpreter.h"
#include "execution.h"
#include "bytecode.h"

#include <stdio.h>
#include <string.h>

/* Debug trace */
#if 0
#define DEBUG_TRACE(pc, op)	printf("[TRACE] pc=%d, opcode=%s\n", pc, op)
#else
#define DEBUG_TRACE(pc, op)
#endif

/* False assertion */
#define NOT_IMPLEMENTED		0
#define NEVER_COME_HERE		0

/* Message. */
#define BROKEN_BYTECODE		"Broken bytecode."

/* Unary OP macro */
#define UNARY_OP(helper)									\
	uint32_t dst;										\
	uint32_t src;										\
												\
	if (*pc + 1 + 2 + 2 > func->bytecode_size) {						\
		rt_error(env, BROKEN_BYTECODE);						\
		return false;									\
	}											\
	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | (uint32_t)func->bytecode[*pc + 2]; 	\
	if (dst >= (uint32_t)func->tmpvar_size) {						\
		rt_error(env, BROKEN_BYTECODE);						\
		return false;									\
	}											\
	src = ((uint32_t)func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];		\
	if (src >= (uint32_t)func->tmpvar_size) {						\
		rt_error(env, BROKEN_BYTECODE);						\
		return false;									\
	}											\
	if (!helper(env, (int)dst, (int)src))							\
		return false;									\
	*pc += 1 + 2 + 2;									\
	return true

/* Binary OP macro */
#define BINARY_OP(helper)								\
	uint32_t dst;									\
	uint32_t src1;									\
	uint32_t src2;									\
											\
	if (*pc + 1 + 2 + 2 + 2 > func->bytecode_size) {				\
		rt_error(env, BROKEN_BYTECODE);					\
		return false;								\
	}										\
	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];	\
	if (dst >= (uint32_t)func->tmpvar_size) {					\
		rt_error(env, BROKEN_BYTECODE);					\
		return false;								\
	}										\
	src1 = ((uint32_t)func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4]; 	\
	if (src1 >= (uint32_t)func->tmpvar_size) {					\
		rt_error(env, BROKEN_BYTECODE);					\
		return false;								\
	}										\
	src2 = ((uint32_t)func->bytecode[*pc + 5] << 8) | func->bytecode[*pc + 6]; 	\
	if (src2 >= (uint32_t)func->tmpvar_size) {					\
		rt_error(env, BROKEN_BYTECODE);					\
		return false;								\
	}										\
	if (!helper(env, (int)dst, (int)src1, (int)src2))				\
		return false;								\
	*pc += 1 + 2 + 2 + 2;								\
	return true

static bool rt_visit_op(struct rt_env *env, struct rt_func *func, int *pc);

/*
 * Visit a bytecode array.
 */
bool
rt_visit_bytecode(
	struct rt_env *env,
	struct rt_func *func)
{
	int pc;

	pc = 0;
	while (pc < func->bytecode_size) {
		if (!rt_visit_op(env, func, &pc))
			return false;
	}

	return true;
}

/* Visit a OP_LINEINFO instruction. */
static inline bool
rt_visit_lineinfo_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t line;

	DEBUG_TRACE(*pc, "LINEINFO");

	if (*pc + 1 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	line = ((uint32_t)func->bytecode[*pc + 1] << 24) |
	       ((uint32_t)func->bytecode[*pc + 2] << 16) |
	       ((uint32_t)func->bytecode[*pc + 3] << 8) |
		(uint32_t)func->bytecode[*pc + 4];

	env->line = (int)line;

	*pc += 5;

	return true;
}

/* Visit a OP_ASSIGN instruction. */
static inline bool
rt_visit_assign_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t src;

	DEBUG_TRACE(*pc, "ASSIGN");

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | (uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];
	if (src >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	env->frame->tmpvar[dst] = env->frame->tmpvar[src];

	*pc += 1 + 2 + 2;

	return true;
}

/* Visit a OP_ICONST instruction. */
static inline bool
rt_visit_iconst_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t val;

	DEBUG_TRACE(*pc, "ICONST");

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	val = ((uint32_t)func->bytecode[*pc + 3] << 24) |
	       ((uint32_t)func->bytecode[*pc + 4] << 16) |
	       ((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];

	env->frame->tmpvar[dst].type = NOCT_VALUE_INT;
	env->frame->tmpvar[dst].val.i = (int)val;

	*pc += 1 + 2 + 4;

	return true;
}

/* Visit a OP_FCONST instruction. */
static inline bool
rt_visit_fconst_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t raw;
	float val;

	DEBUG_TRACE(*pc, "FCONST");

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | (uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	raw = ((uint32_t)func->bytecode[*pc + 3] << 24) |
	       ((uint32_t)func->bytecode[*pc + 4] << 16) |
	       ((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];

	val = *(float *)&raw;

	env->frame->tmpvar[dst].type = NOCT_VALUE_FLOAT;
	env->frame->tmpvar[dst].val.f = val;

	*pc += 1 + 2 + 4;

	return true;
}

/* Visit a OP_SCONST instruction. */
static inline bool
rt_visit_sconst_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;
	const char *s;
	int len;

	DEBUG_TRACE(*pc, "SCONST");

	if (*pc + 1 + 2  > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	s = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(s);
	if (*pc + 1 + 2 + len + 1 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!rt_make_string(env, &env->frame->tmpvar[dst], s))
		return false;

	*pc += 1 + 2 + len + 1;

	return true;
}

/* Visit a OP_ACONST instruction. */
static inline bool
rt_visit_aconst_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;

	DEBUG_TRACE(*pc, "ACONST");

	if (*pc + 1 + 2  > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!noct_make_empty_array(env, &env->frame->tmpvar[dst]))
		return false;

	*pc += 1 + 2;

	return true;
}

/* Visit a OP_DCONST instruction. */
static inline bool
rt_visit_dconst_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;

	DEBUG_TRACE(*pc, "DCONST");

	if (*pc + 1 + 2  > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!noct_make_empty_dict(env, &env->frame->tmpvar[dst]))
		return false;

	*pc += 1 + 2;

	return true;
}

/* Visit a OP_INC instruction. */
static inline bool
rt_visit_inc_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	struct rt_value *val;
	uint32_t dst;

	DEBUG_TRACE(*pc, "INC");

	if (*pc + 1 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	val = &env->frame->tmpvar[dst];
	if (val->type != NOCT_VALUE_INT) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}
	val->val.i++;

	*pc += 1 + 2;

	return true;
}

/* Visit a OP_ADD instruction. */
static inline bool
rt_visit_add_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "ADD");

	BINARY_OP(rt_add_helper);
}

/* Visit a OP_SUB instruction. */
static inline bool
rt_visit_sub_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "SUB");

	BINARY_OP(rt_sub_helper);
}

/* Visit a OP_MUL instruction. */
static inline bool
rt_visit_mul_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "MUL");

	BINARY_OP(rt_mul_helper);
}

/* Visit a OP_DIV instruction. */
static inline bool
rt_visit_div_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "DIV");

	BINARY_OP(rt_div_helper);
}

/* Visit a OP_MOD instruction. */
static inline bool
rt_visit_mod_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "MOD");

	BINARY_OP(rt_mod_helper);
}

/* Visit a OP_AND instruction. */
static inline bool
rt_visit_and_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "AND");

	BINARY_OP(rt_and_helper);
}

/* Visit a OP_OR instruction. */
static inline bool
rt_visit_or_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "OR");

	BINARY_OP(rt_or_helper);
}

/* Visit a OP_XOR instruction. */
static inline bool
rt_visit_xor_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "XOR");

	BINARY_OP(rt_xor_helper);
}

/* Visit a OP_NEG instruction. */
static inline bool
rt_visit_neg_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "NEG");

	UNARY_OP(rt_neg_helper);
}

/* Visit a OP_NOT instruction. */
static inline bool
rt_visit_not_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "NOT");

	UNARY_OP(rt_not_helper);
}

/* Visit a OP_LT instruction. */
static inline bool
rt_visit_lt_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "LT");

	BINARY_OP(rt_lt_helper);
}

/* Visit a OP_LTE instruction. */
static inline bool
rt_visit_lte_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "LTE");

	BINARY_OP(rt_lte_helper);
}

/* Visit a OP_GT instruction. */
static inline bool
rt_visit_gt_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "GT");

	BINARY_OP(rt_gt_helper);
}

/* Visit a OP_GTE instruction. */
static inline bool
rt_visit_gte_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "GTE");

	BINARY_OP(rt_gte_helper);
}

/* Visit a OP_EQ instruction. */
static inline bool
rt_visit_eq_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "EQ");

	BINARY_OP(rt_eq_helper);
}

/* Visit a OP_NEQ instruction. */
static inline bool
rt_visit_neq_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "NEQ");

	BINARY_OP(rt_neq_helper);
}

/* Visit a OP_STOREARRAY instruction. */
static inline bool
rt_visit_storearray_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "STOREARRAY");

	BINARY_OP(rt_storearray_helper);
}

/* Visit a OP_LOADARRAY instruction. */
static inline bool
rt_visit_loadarray_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "LOADARRAY");

	BINARY_OP(rt_loadarray_helper);
}

/* Visit a OP_LEN instruction. */
static inline bool
rt_visit_len_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "LEN");

	UNARY_OP(rt_len_helper);
}

/* Visit a OP_GETDICTKEYBYINDEX instruction. */
static inline bool
rt_visit_getdictkeybyindex_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "GETDICTKEYBYINDEX");

	BINARY_OP(rt_getdictkeybyindex_helper);
}

/* Visit a OP_GETDICTVALBYINDEX instruction. */
static inline bool
rt_visit_getdictvalbyindex_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	DEBUG_TRACE(*pc, "GETDICTVALBYINDEX");

	BINARY_OP(rt_getdictvalbyindex_helper);
}

/* Visit a OP_LOADYMBOL instruction. */
static inline bool
rt_visit_loadsymbol_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	int dst;
	const char *symbol;
	int len;

	DEBUG_TRACE(*pc, "LOADSYMBOL");

	if (*pc + 1 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = (func->bytecode[*pc + 1] << 8) | (func->bytecode[*pc + 2]);

	symbol = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(symbol);
	if (*pc + 2 + len + 1 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!rt_loadsymbol_helper(env, dst, symbol))
		return false;

	*pc += 1 + 2 + len + 1;

	return true;
}

/* Visit a OP_STORESYMBOL instruction. */
static inline bool
rt_visit_storesymbol_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	const char *symbol;
	uint32_t src;
	int len;

	DEBUG_TRACE(*pc, "STORESYMBOL");

	symbol = (const char *)&func->bytecode[*pc + 1];
	len = (int)strlen(symbol);
	if (*pc + 1 + len + 1 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1 + len + 1] << 8) |
	      ((uint32_t)func->bytecode[*pc + 1 + len + 1 + 1]);

	if (!rt_storesymbol_helper(env, symbol, (int)src))
		return false;

	*pc += 1 + len + 1 + 2;

	return true;
}

/* Visit a OP_LOADDOT instruction. */
static inline bool
rt_visit_loaddot_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t dict;
	const char *field;
	int len;

	DEBUG_TRACE(*pc, "LOADDOT");

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)(func->bytecode[*pc + 2]);
	if (dst >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dict = ((uint32_t)func->bytecode[*pc + 3] << 8) |
		(uint32_t)(func->bytecode[*pc + 4]);
	if (dict >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	field = (const char *)&func->bytecode[*pc + 5];
	len = (int)strlen(field);
	if (*pc + 1 + 2  + 2 + len + 1 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!rt_loaddot_helper(env, (int)dst, (int)dict, field))
		return false;

	*pc += 1 + 2 + 2 + len + 1;

	return true;
}

/* Visit a OP_STOREDOT instruction. */
static inline bool
rt_visit_storedot_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t src;
	uint32_t dict;
	const char *field;
	int len;

	DEBUG_TRACE(*pc, "STOREDOT");

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dict = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dict >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	field = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(field);
	if (*pc + 1 + 2  + 2 + len + 1 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1 + 2 + len + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 1 + 2 + len + 1 + 1];
	if (src >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (!rt_storedot_helper(env, (int)dict, field, (int)src))
		return false;

	*pc += 1 + 2 + 2 + len + 1;

	return true;
}

/* Visit a OP_CALL instruction. */
static inline bool
rt_visit_call_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	int dst_tmpvar;
	int func_tmpvar;
	int arg_count;
	int arg_tmpvar;
	int arg[NOCT_ARG_MAX];
	int i;

	DEBUG_TRACE(*pc, "CALL");

	dst_tmpvar = (func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];
	if (dst_tmpvar >= func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	func_tmpvar = (func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];
	if (func_tmpvar >= func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	arg_count = func->bytecode[*pc + 5];
	for (i = 0; i < arg_count; i++) {
		arg_tmpvar = (func->bytecode[*pc + 6 + i * 2] << 8 ) | 
			     func->bytecode[*pc + 6 + i * 2 + 1];
		arg[i] = arg_tmpvar;
	}

	if (!rt_call_helper(env, dst_tmpvar, func_tmpvar, arg_count, arg))
		return false;

	*pc += 6 + arg_count * 2;

	return true;
}

/* Visit a OP_THISCALL instruction. */
static inline bool
rt_visit_thiscall_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	int dst_tmpvar;
	int obj_tmpvar;
	const char *name;
	int len;
	int arg_count;
	int arg_tmpvar;
	int arg[NOCT_ARG_MAX];
	int i;

	DEBUG_TRACE(*pc, "THISCALL");

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	dst_tmpvar = (func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];
	if (dst_tmpvar >= func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	obj_tmpvar = (func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];
	if (obj_tmpvar >= func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	name = (const char *)&func->bytecode[*pc + 5];
	len = (int)strlen(name);
	if (*pc + 1 + 2 + 2 + len + 1 + 1 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	arg_count = func->bytecode[*pc + 1 + 2 + 2 + len + 1];
	for (i = 0; i < arg_count; i++) {
		arg_tmpvar = (func->bytecode[*pc + 1 + 2 + 2 + len + 1 + 1 + i * 2] << 8 ) | 
			     func->bytecode[*pc + 1 + 2 + 2 + len + 1 + 1 + i * 2 + 1];
		arg[i] = arg_tmpvar;
	}

	if (!rt_thiscall_helper(env, dst_tmpvar, obj_tmpvar, name, arg_count, arg))
		return false;

	*pc += 1 + 2 + 2 + len + 1 + 1 + arg_count * 2;

	return true;
}

/* Visit a OP_JMP instruction. */
static inline bool
rt_visit_jmp_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t target;

	DEBUG_TRACE(*pc, "JMP");

	if (*pc + 1 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 1] << 24) |
		(uint32_t)(func->bytecode[*pc + 2] << 16) |
		(uint32_t)(func->bytecode[*pc + 3] << 8) |
		(uint32_t)func->bytecode[*pc + 4];
	if (target > (uint32_t)func->bytecode_size + 1) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	*pc = (int)target;

	return true;
}

/* Visit a OP_JMPIFTRUE instruction. */
static inline bool
rt_visit_jmpiftrue_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t target;
	uint32_t src;

	DEBUG_TRACE(*pc, "JMPIFTRUE");

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (src >= (uint32_t)func->tmpvar_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		((uint32_t)func->bytecode[*pc + 4] << 16) |
		((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];
	if (target > (uint32_t)func->bytecode_size + 1) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (env->frame->tmpvar[src].type != NOCT_VALUE_INT) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (env->frame->tmpvar[src].val.i == 1)
		*pc = (int)target;
	else
		*pc += 1 + 2 + 4;

	return true;
}

/* Visit a OP_JMPIFFALSE instruction. */
static inline bool
rt_visit_jmpiffalse_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	uint32_t target;
	uint32_t src;

	DEBUG_TRACE(*pc, "JMPIFFALSE");

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (src >= (uint32_t)func->tmpvar_size + 1) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		((uint32_t)func->bytecode[*pc + 4] << 16) |
		((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];
	if (target > (uint32_t)func->bytecode_size) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (env->frame->tmpvar[src].type != NOCT_VALUE_INT) {
		noct_error(env, BROKEN_BYTECODE);
		return false;
	}

	if (env->frame->tmpvar[src].val.i == 0)
		*pc = (int)target;
	else
		*pc += 1 + 2 + 4;

	return true;
}

/* Visit an instruction. */
static bool
rt_visit_op(
	struct rt_env *env,
	struct rt_func *func,
	int *pc)
{
	switch (func->bytecode[*pc]) {
	case OP_NOP:
		/* NOP */
		(*pc)++;
		break;
	case OP_LINEINFO:
		if (!rt_visit_lineinfo_op(env, func, pc))
			return false;
		break;
	case OP_ASSIGN:
		if (!rt_visit_assign_op(env, func, pc))
			return false;
		break;
	case OP_ICONST:
		if (!rt_visit_iconst_op(env, func, pc))
			return false;
		break;
	case OP_FCONST:
		if (!rt_visit_fconst_op(env, func, pc))
			return false;
		break;
	case OP_SCONST:
		if (!rt_visit_sconst_op(env, func, pc))
			return false;
		break;
	case OP_ACONST:
		if (!rt_visit_aconst_op(env, func, pc))
			return false;
		break;
	case OP_DCONST:
		if (!rt_visit_dconst_op(env, func, pc))
			return false;
		break;
	case OP_INC:
		if (!rt_visit_inc_op(env, func, pc))
			return false;
		break;
	case OP_ADD:
		if (!rt_visit_add_op(env, func, pc))
			return false;
		break;
	case OP_SUB:
		if (!rt_visit_sub_op(env, func, pc))
			return false;
		break;
	case OP_MUL:
		if (!rt_visit_mul_op(env, func, pc))
			return false;
		break;
	case OP_DIV:
		if (!rt_visit_div_op(env, func, pc))
			return false;
		break;
	case OP_MOD:
		if (!rt_visit_mod_op(env, func, pc))
			return false;
		break;
	case OP_AND:
		if (!rt_visit_and_op(env, func, pc))
			return false;
		break;
	case OP_OR:
		if (!rt_visit_or_op(env, func, pc))
			return false;
		break;
	case OP_XOR:
		if (!rt_visit_xor_op(env, func, pc))
			return false;
		break;
	case OP_NEG:
		if (!rt_visit_neg_op(env, func, pc))
			return false;
		break;
	case OP_NOT:
		if (!rt_visit_not_op(env, func, pc))
			return false;
		break;
	case OP_LT:
		if (!rt_visit_lt_op(env, func, pc))
			return false;
		break;
	case OP_LTE:
		if (!rt_visit_lte_op(env, func, pc))
			return false;
		break;
	case OP_GT:
		if (!rt_visit_gt_op(env, func, pc))
			return false;
		break;
	case OP_GTE:
		if (!rt_visit_gte_op(env, func, pc))
			return false;
		break;
	case OP_EQ:
		if (!rt_visit_eq_op(env, func, pc))
			return false;
		break;
	case OP_EQI:
		/* Same as EQ. EQI is an optimization hint for JIT-compiler. */
		if (!rt_visit_eq_op(env, func, pc))
			return false;
		break;
	case OP_NEQ:
		if (!rt_visit_neq_op(env, func, pc))
			return false;
		break;
	case OP_STOREARRAY:
		if (!rt_visit_storearray_op(env, func, pc))
			return false;
		break;
	case OP_LOADARRAY:
		if (!rt_visit_loadarray_op(env, func, pc))
			return false;
		break;
	case OP_LEN:
		if (!rt_visit_len_op(env, func, pc))
			return false;
		break;
	case OP_GETDICTKEYBYINDEX:
		if (!rt_visit_getdictkeybyindex_op(env, func, pc))
			return false;
		break;
	case OP_GETDICTVALBYINDEX:
		if (!rt_visit_getdictvalbyindex_op(env, func, pc))
			return false;
		break;
	case OP_LOADSYMBOL:
		if (!rt_visit_loadsymbol_op(env, func, pc))
			return false;
		break;
	case OP_STORESYMBOL:
		if (!rt_visit_storesymbol_op(env, func, pc))
			return false;
		break;
	case OP_LOADDOT:
		if (!rt_visit_loaddot_op(env, func, pc))
			return false;
		break;
	case OP_STOREDOT:
		if (!rt_visit_storedot_op(env, func, pc))
			return false;
		break;
	case OP_CALL:
		if (!rt_visit_call_op(env, func, pc))
			return false;
		break;
	case OP_THISCALL:
		if (!rt_visit_thiscall_op(env, func, pc))
			return false;
		break;
	case OP_JMP:
		if (!rt_visit_jmp_op(env, func, pc))
			return false;
		break;
	case OP_JMPIFTRUE:
		if (!rt_visit_jmpiftrue_op(env, func, pc))
			return false;
		break;
	case OP_JMPIFFALSE:
		if (!rt_visit_jmpiffalse_op(env, func, pc))
			return false;
		break;
	case OP_JMPIFEQ:
		/* Same as JMPIFTRUE. (JMPIFEQ is an optimization hint for JIT-compiler.) */
		if (!rt_visit_jmpiftrue_op(env, func, pc))
			return false;
		break;
	default:
		noct_error(env, "Unknown opcode %d at pc=%d.", func->bytecode[*pc], *pc);
		return false;
	}

	return true;
}
