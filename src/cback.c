/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * cback: C translation backend
 */

#include "cback.h"
#include "lir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/*
 * False assertion
 */

#define NEVER_COME_HERE		0

/*
 * Constant
 */

#define ARG_MAX			32

/*
 * Message
 */

static const char BROKEN_BYTECODE[] = "Broken bytecode.";

/*
 * Translated function names.
 */

#define FUNC_MAX	(4096)

struct c_func {
	char *name;
	int param_count;
	char *param_name[ARG_MAX];
};

static struct c_func func_table[FUNC_MAX];
static int func_count;

/*
 * Translation context.
 */

static FILE *fp;

/*
 * Forward declaration
 */
static bool cback_visit_bytecode(struct lir_func *func);
static bool cback_visit_op(struct lir_func *func, int *pc);
static bool cback_write_dll_init(void);

/*
 * Clear translator states.
 */
bool
cback_init(
	const char *fname)
{
	fp = fopen(fname, "w");
	if (fp == NULL) {
		printf("Failed to open file \"%s\".\n", fname);
		return false;
	}

	return true;
}

/*
 * Translate LIR to C.
 */
bool
cback_translate_func(
	struct lir_func *func)
{
	int i;

	/* Save a function name. */
	func_table[func_count].name = strdup(func->func_name);
	if (func_table[func_count].name == NULL) {
		printf("Out of memory.\n");
		return false;
	}
	func_table[func_count].param_count = func->param_count;
	for (i = 0; i < func->param_count; i++) {
		func_table[func_count].param_name[i] = strdup(func->param_name[i]);
		if (func_table[func_count].param_name[i] == NULL) {
			printf("Out of memory.\n");
			return false;
		}
	}
	func_count++;

	/* Put a prologue code. */
	fprintf(fp, "#include <noctlang/runtime.h>\n");
	fprintf(fp, "\n");
	fprintf(fp, "#include <stdio.h>\n");
	fprintf(fp, "#include <string.h>\n");
	fprintf(fp, "\n");
	fprintf(fp, "bool L_%s(struct rt_env *rt)\n", func->func_name);
	fprintf(fp, "{\n");
	fprintf(fp, "    struct rt_value tmpvar[%d];\n", func->tmpvar_size);
	fprintf(fp, "    rt->frame->tmpvar = &tmpvar[0];\n");

	/* Visit a bytecode array. */
	if (!cback_visit_bytecode(func))
		return false;

	/* Put an epilogue code. */
	fprintf(fp, "    rt->frame->tmpvar = NULL;\n");
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n\n");

	return true;
}

/* Visit a bytecode array. */
static bool
cback_visit_bytecode(
	struct lir_func *func)
{
	int pc;

	pc = 0;
	while (pc < func->bytecode_size) {
		if (!cback_visit_op(func, &pc))
			return false;
	}

	return true;
}

#define LABEL(pc) \
	fprintf(fp, "L_pc_%d:\n", (pc));

#define UNARY_OP(helper)							\
	uint32_t dst;								\
	uint32_t src;								\
										\
	if (*pc + 1 + 2 + 2 > func->bytecode_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |			\
		(uint32_t)func->bytecode[*pc + 2];				\
	if (dst >= (uint32_t)func->tmpvar_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	src = ((uint32_t)func->bytecode[*pc + 3] << 8) | 			\
		(uint32_t)func->bytecode[*pc + 4];				\
	if (src >= (uint32_t)func->tmpvar_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	*pc += 1 + 2 + 2;							\
	fprintf(fp, "if (!" #helper "(rt, (int)dst, (int)src))");		\
	fprintf(fp, "    return false;\n");

#define BINARY_OP(helper)							\
	uint32_t dst;								\
	uint32_t src1;								\
	uint32_t src2;								\
										\
	if (*pc + 1 + 2 + 2 + 2 > func->bytecode_size) {			\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | 			\
		(uint32_t)func->bytecode[*pc + 2];				\
	if (dst >= (uint32_t)func->tmpvar_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	src1 = ((uint32_t)func->bytecode[*pc + 3] << 8) |			\
		(uint32_t)func->bytecode[*pc + 4];				\
	if (src1 >= (uint32_t)func->tmpvar_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	src2 = ((uint32_t)func->bytecode[*pc + 5] << 8) | 			\
		(uint32_t)func->bytecode[*pc + 6];				\
	if (src2 >= (uint32_t)func->tmpvar_size) {				\
		printf(BROKEN_BYTECODE);					\
		return false;							\
	}									\
	*pc += 1 + 2 + 2 + 2;							\
	fprintf(fp, "if (!" #helper "(rt, (int)dst, (int)src1, (int)src2))");	\
	fprintf(fp, "    return false;\n");

/* Visit a LOP_LINEINFO instruction. */
static INLINE bool
cback_visit_lineinfo_op(
	struct lir_func *func,
	int *pc)
{
	int line;

	if (*pc + 1 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	line = (func->bytecode[*pc + 1] << 24) |
	       (func->bytecode[*pc + 2] << 16) |
	       (func->bytecode[*pc + 3] << 8) |
		func->bytecode[*pc + 4];
	*pc += 5;

	fprintf(fp, "/* line: %d */\n", line);

	return true;
}

/* Visit a LOP_ASSIGN instruction. */
static INLINE bool
cback_visit_assign_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t src;

	LABEL(*pc);

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (src >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 5;

	fprintf(fp, "rt->frame->tmpvar[dst] = tmpvar[src];\n");

	return true;
}

/* Visit a LOP_ICONST instruction. */
static INLINE bool
cback_visit_iconst_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t val;

	LABEL(*pc);

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	val = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		(uint32_t)(func->bytecode[*pc + 4] << 16) |
		(uint32_t)(func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];

	*pc += 1 + 2 + 4;

	fprintf(fp, "    rt->frame->tmpvar[%d].type = RT_VALUE_INT;\n", dst);
	fprintf(fp, "    rt->frame->tmpvar[%d].val.i = %d;\n", dst, val);

	return true;
}

/* Visit a LOP_FCONST instruction. */
static INLINE bool
cback_visit_fconst_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t raw;
	float val;

	LABEL(*pc);

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	raw = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		((uint32_t)func->bytecode[*pc + 4] << 16) |
		((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];

	val = *(float *)&raw;

	*pc += 1 + 2 + 4;

	fprintf(fp, "    rt->frame->tmpvar[%d].type = RT_VALUE_FLOAT;\n", dst);
	fprintf(fp, "    rt->frame->tmpvar[%d].val.f = %f;\n", dst, val);

	return true;
}

/* Visit a LOP_SCONST instruction. */
static INLINE bool
cback_visit_sconst_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;
	const char *s;
	int len;

	LABEL(*pc);

	if (*pc + 1 + 2  > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) | (uint32_t)
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	s = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(s);
	if (*pc + 1 + 2 + len + 1 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + len + 1;

	fprintf(fp, "    if (!rt_make_string(rt, &rt->frame->tmpvar[%d], \"%s\"))\n", dst, s);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_ACONST instruction. */
static INLINE bool
cback_visit_aconst_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;

	LABEL(*pc);

	if (*pc + 1 + 2  > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2;

	fprintf(fp, "    if (!rt_make_empty_array(rt, &rt->frame->tmpvar[%d]))\n", dst);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_DCONST instruction. */
static INLINE bool
cback_visit_dconst_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;

	LABEL(*pc);

	if (*pc + 1 + 2  > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2;

	fprintf(fp, "    if (!rt_make_empty_dict(rt, &rt->frame->tmpvar[%d]))\n", dst);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_INC instruction. */
static INLINE bool
cback_visit_inc_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;

	LABEL(*pc);

	if (*pc + 1 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2;

	fprintf(fp, "    rt->frame->tmpvar[%d].val.i++;\n", dst);

	return true;
}

/* Visit a LOP_ADD instruction. */
static INLINE bool
cback_visit_add_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_add_heler);
	return true;
}

/* Visit a LOP_SUB instruction. */
static INLINE bool
cback_visit_sub_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_sub_helper);
	return true;
}

/* Visit a LOP_MUL instruction. */
static INLINE bool
cback_visit_mul_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_mul_helper);
	return true;
}

/* Visit a LOP_DIV instruction. */
static INLINE bool
cback_visit_div_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_div_helper);
	return true;
}

/* Visit a LOP_MOD instruction. */
static INLINE bool
cback_visit_mod_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_mod_helper);
	return true;
}

/* Visit a LOP_AND instruction. */
static INLINE bool
cback_visit_and_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_and_helper);
	return true;
}

/* Visit a LOP_OR instruction. */
static INLINE bool
cback_visit_or_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_or_helper);
	return true;
}

/* Visit a LOP_XOR instruction. */
static INLINE bool
cback_visit_xor_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_xor_helper);
	return true;
}

/* Visit a LOP_NEG instruction. */
static INLINE bool
cback_visit_neg_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	UNARY_OP(rt_neg_helper);
	return true;
}

/* Visit a LOP_LT instruction. */
static INLINE bool
cback_visit_lt_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_lt_helper);
	return true;
}

/* Visit a LOP_LTE instruction. */
static INLINE bool
cback_visit_lte_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_lte_helper);
	return true;
}

/* Visit a LOP_GT instruction. */
static INLINE bool
cback_visit_gt_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_gt_helper);
	return true;
}

/* Visit a LOP_GTE instruction. */
static INLINE bool
cback_visit_gte_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_gte_helper);
	return true;
}

/* Visit a LOP_EQ instruction. */
static INLINE bool
cback_visit_eq_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_eq_helper);
	return true;
}

/* Visit a LOP_NEQ instruction. */
static INLINE bool
cback_visit_neq_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_neq_helper);
	return true;
}

/* Visit a LOP_STOREARRAY instruction. */
static INLINE bool
cback_visit_storearray_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_storearray_helper);
	return true;
}

/* Visit a LOP_LOADARRAY instruction. */
static INLINE bool
cback_visit_loadarray_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_loadarray_helper);
	return true;
}

/* Visit a LOP_LEN instruction. */
static INLINE bool
cback_visit_len_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	UNARY_OP(rt_len_helper);
	return true;
}

/* Visit a LOP_GETDICTKEYBYINDEX instruction. */
static INLINE bool
cback_visit_getdictkeybyindex_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_getdictkeybyindex_helper);
	return true;
}

/* Visit a LOP_GETDICTVALBYINDEX instruction. */
static INLINE bool
cback_visit_getdictvalbyindex_op(
	struct lir_func *func,
	int *pc)
{
	LABEL(*pc);
	BINARY_OP(rt_getdictvalbyindex_helper);
	return true;
}

/* Visit a LOP_LOADYMBOL instruction. */
static INLINE bool
cback_visit_loadsymbol_op(
	struct lir_func *func,
	int *pc)
{
	int dst;
	const char *symbol;
	int len;

	LABEL(*pc);

	if (*pc + 1 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = (func->bytecode[*pc + 1] << 8) | (func->bytecode[*pc + 2]);

	symbol = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(symbol);
	if (*pc + 2 + len + 1 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + len + 1;

	fprintf(fp, "    if (!rt_loadsymbol_helper(rt, %d, \"%s\"))\n", dst, symbol);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_STORESYMBOL instruction. */
static INLINE bool
cback_visit_storesymbol_op(
	struct lir_func *func,
	int *pc)
{
	const char *symbol;
	uint32_t src;
	int len;

	LABEL(*pc);

	symbol = (const char *)&func->bytecode[*pc + 1];
	len = (int)strlen(symbol);
	if (*pc + 1 + len + 1 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1 + len + 1] << 8) |
		(uint32_t)(func->bytecode[*pc + 1 + len + 1 + 1]);

	*pc += 1 + len + 1 + 2;

	fprintf(fp, "    if (!rt_storesymbol_helper(rt, \"%s\", %d))\n", symbol, src);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_LOADDOT instruction. */
static INLINE bool
cback_visit_loaddot_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t dst;
	uint32_t dict;
	const char *field;
	int len;

	LABEL(*pc);

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)(func->bytecode[*pc + 2]);
	if (dst >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dict = ((uint32_t)func->bytecode[*pc + 3] << 8) |
		(uint32_t)(func->bytecode[*pc + 4]);
	if (dict >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	field = (const char *)&func->bytecode[*pc + 5];
	len = (int)strlen(field);
	if (*pc + 1 + 2  + 2 + len + 1 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + 2 + len + 1;

	fprintf(fp, "    if (!rt_loaddot_helper(rt, %d, %d, \"%s\"))\n", dst, dict, field);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_STOREDOT instruction. */
static INLINE bool
cback_visit_storedot_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t src;
	uint32_t dict;
	const char *field;
	int len;

	LABEL(*pc);

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dict = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)(func->bytecode[*pc + 2]);
	if (dict >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	field = (const char *)&func->bytecode[*pc + 3];
	len = (int)strlen(field);
	if (*pc + 1 + 2  + 2 + len + 1 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1 + 2 + len + 1] << 8) |
	       ((uint32_t)func->bytecode[*pc + 1 + 2 + len + 1 + 1]);
	if (src >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + 2 + len + 1;

	fprintf(fp, "    if (!rt_storedot_helper(rt, %d, \"%s\", %d))\n", dict, field, src);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_CALL instruction. */
static INLINE bool
cback_visit_call_op(
	struct lir_func *func,
	int *pc)
{
	int dst_tmpvar;
	int func_tmpvar;
	int arg_count;
	int arg_tmpvar;
	int arg[ARG_MAX];
	int i;

	LABEL(*pc);

	dst_tmpvar = (func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];
	if (dst_tmpvar >= func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	func_tmpvar = (func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];
	if (func_tmpvar >= func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	arg_count = func->bytecode[*pc + 5];
	for (i = 0; i < arg_count; i++) {
		arg_tmpvar = (func->bytecode[*pc + 6 + i * 2] << 8 ) | 
			     func->bytecode[*pc + 6 + i * 2 + 1];
		arg[i] = arg_tmpvar;
	}

	*pc += 6 + arg_count * 2;

	fprintf(fp, "    {\n");
	fprintf(fp, "        int arg[%d] = {", arg_count);
	for (i = 0; i < arg_count; i++)
		fprintf(fp, "%d,", arg[i]);
	fprintf(fp, "};\n");
	fprintf(fp, "        if (!rt_call_helper(rt, %d, %d, %d, arg))\n", dst_tmpvar, func_tmpvar, arg_count);
	fprintf(fp, "            return false;\n");
	fprintf(fp, "    };\n");

	return true;
}

/* Visit a LOP_THISCALL instruction. */
static INLINE bool
cback_visit_thiscall_op(
	struct lir_func *func,
	int *pc)
{
	int dst_tmpvar;
	int obj_tmpvar;
	const char *name;
	int len;
	int arg_count;
	int arg_tmpvar;
	int arg[ARG_MAX];
	int i;

	LABEL(*pc);

	if (*pc + 1 + 2 + 2 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	dst_tmpvar = (func->bytecode[*pc + 1] << 8) | func->bytecode[*pc + 2];
	if (dst_tmpvar >= func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	obj_tmpvar = (func->bytecode[*pc + 3] << 8) | func->bytecode[*pc + 4];
	if (obj_tmpvar >= func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	name = (const char *)&func->bytecode[*pc + 5];
	len = (int)strlen(name);
	if (*pc + 1 + 2 + 2 + len + 1 + 1 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	arg_count = func->bytecode[*pc + 1 + 2 + 2 + len + 1];
	for (i = 0; i < arg_count; i++) {
		arg_tmpvar = (func->bytecode[*pc + 1 + 2 + 2 + len + 1 + 1 + i * 2] << 8 ) | 
			     func->bytecode[*pc + 1 + 2 + 2 + len + 1 + 1 + i * 2 + 1];
		arg[i] = arg_tmpvar;
	}

	*pc += 1 + 2 + 2 + len + 1 + 1 + arg_count * 2;

	fprintf(fp, "    {\n");
	fprintf(fp, "        int arg[%d] = {", arg_count);
	for (i = 0; i < arg_count; i++)
		fprintf(fp, "%d,", arg[i]);
	fprintf(fp, "};\n");
	fprintf(fp, "        if (!rt_thiscall_helper(rt, %d, %d, \"%s\", %d, arg))\n", dst_tmpvar, obj_tmpvar, name, arg_count);
	fprintf(fp, "            return false;\n");
	fprintf(fp, "    };\n");

	return true;
}

/* Visit a LOP_JMP instruction. */
static inline bool
cback_visit_jmp_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t target;

	LABEL(*pc);

	if (*pc + 1 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 1] << 24) |
		((uint32_t)func->bytecode[*pc + 2] << 16) |
		((uint32_t)func->bytecode[*pc + 3] << 8) |
		(uint32_t)func->bytecode[*pc + 4];
	if (target > (uint32_t)func->bytecode_size + 1) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 4;

	fprintf(fp, "    goto L_pc_%d;\n", target);

	return true;
}

/* Visit a LOP_JMPIFTRUE instruction. */
static bool
cback_visit_jmpiftrue_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t target;
	uint32_t src;

	LABEL(*pc);

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (src >= (uint32_t)func->tmpvar_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		((uint32_t)func->bytecode[*pc + 4] << 16) |
		((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];
	if (target > (uint32_t)func->bytecode_size + 1) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + 4;

	fprintf(fp, "    if (rt->frame->tmpvar[%d].val.i != 0)\n", src);
	fprintf(fp, "        goto L_pc_%d;\n", target);

	return true;
}

/* Visit a LOP_JMPIFFALSE instruction. */
static INLINE bool
cback_visit_jmpiffalse_op(
	struct lir_func *func,
	int *pc)
{
	uint32_t target;
	uint32_t src;

	LABEL(*pc);

	if (*pc + 1 + 2 + 4 > func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	src = ((uint32_t)func->bytecode[*pc + 1] << 8) |
		(uint32_t)func->bytecode[*pc + 2];
	if (src >= (uint32_t)func->tmpvar_size + 1) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	target = ((uint32_t)func->bytecode[*pc + 3] << 24) |
		((uint32_t)func->bytecode[*pc + 4] << 16) |
		((uint32_t)func->bytecode[*pc + 5] << 8) |
		(uint32_t)func->bytecode[*pc + 6];
	if (target > (uint32_t)func->bytecode_size) {
		printf(BROKEN_BYTECODE);
		return false;
	}

	*pc += 1 + 2 + 4;

	fprintf(fp, "    if (rt->frame->tmpvar[%d].val.i == 0)\n", src);
	fprintf(fp, "        goto L_pc_%d;\n", target);

	return true;
}

/* Visit an instruction. */
static bool
cback_visit_op(
	struct lir_func *func,
	int *pc)
{
	switch (func->bytecode[*pc]) {
	case LOP_NOP:
		/* NOP */
		(*pc)++;
		break;
	case LOP_LINEINFO:
		if (!cback_visit_lineinfo_op(func, pc))
			return false;
		break;
	case LOP_ASSIGN:
		if (!cback_visit_assign_op(func, pc))
			return false;
		break;
	case LOP_ICONST:
		if (!cback_visit_iconst_op(func, pc))
			return false;
		break;
	case LOP_FCONST:
		if (!cback_visit_fconst_op(func, pc))
			return false;
		break;
	case LOP_SCONST:
		if (!cback_visit_sconst_op(func, pc))
			return false;
		break;
	case LOP_ACONST:
		if (!cback_visit_aconst_op(func, pc))
			return false;
		break;
	case LOP_DCONST:
		if (!cback_visit_dconst_op(func, pc))
			return false;
		break;
	case LOP_INC:
		if (!cback_visit_inc_op(func, pc))
			return false;
		break;
	case LOP_ADD:
		if (!cback_visit_add_op(func, pc))
			return false;
		break;
	case LOP_SUB:
		if (!cback_visit_sub_op(func, pc))
			return false;
		break;
	case LOP_MUL:
		if (!cback_visit_mul_op(func, pc))
			return false;
		break;
	case LOP_DIV:
		if (!cback_visit_div_op(func, pc))
			return false;
		break;
	case LOP_MOD:
		if (!cback_visit_mod_op(func, pc))
			return false;
		break;
	case LOP_AND:
		if (!cback_visit_and_op(func, pc))
			return false;
		break;
	case LOP_OR:
		if (!cback_visit_or_op(func, pc))
			return false;
		break;
	case LOP_XOR:
		if (!cback_visit_xor_op(func, pc))
			return false;
		break;
	case LOP_NEG:
		if (!cback_visit_neg_op(func, pc))
			return false;
		break;
	case LOP_LT:
		if (!cback_visit_lt_op(func, pc))
			return false;
		break;
	case LOP_LTE:
		if (!cback_visit_lte_op(func, pc))
			return false;
		break;
	case LOP_GT:
		if (!cback_visit_gt_op(func, pc))
			return false;
		break;
	case LOP_GTE:
		if (!cback_visit_gte_op(func, pc))
			return false;
		break;
	case LOP_EQ:
		if (!cback_visit_eq_op(func, pc))
			return false;
		break;
	case LOP_EQI:
		/* Same as EQ. EQI is an optimization hint for JIT-compiler. */
		if (!cback_visit_eq_op(func, pc))
			return false;
		break;
	case LOP_NEQ:
		if (!cback_visit_neq_op(func, pc))
			return false;
		break;
	case LOP_STOREARRAY:
		if (!cback_visit_storearray_op(func, pc))
			return false;
		break;
	case LOP_LOADARRAY:
		if (!cback_visit_loadarray_op(func, pc))
			return false;
		break;
	case LOP_LEN:
		if (!cback_visit_len_op(func, pc))
			return false;
		break;
	case LOP_GETDICTKEYBYINDEX:
		if (!cback_visit_getdictkeybyindex_op(func, pc))
			return false;
		break;
	case LOP_GETDICTVALBYINDEX:
		if (!cback_visit_getdictvalbyindex_op(func, pc))
			return false;
		break;
	case LOP_LOADSYMBOL:
		if (!cback_visit_loadsymbol_op(func, pc))
			return false;
		break;
	case LOP_STORESYMBOL:
		if (!cback_visit_storesymbol_op(func, pc))
			return false;
		break;
	case LOP_LOADDOT:
		if (!cback_visit_loaddot_op(func, pc))
			return false;
		break;
	case LOP_STOREDOT:
		if (!cback_visit_storedot_op(func, pc))
			return false;
		break;
	case LOP_CALL:
		if (!cback_visit_call_op(func, pc))
			return false;
		break;
	case LOP_THISCALL:
		if (!cback_visit_thiscall_op(func, pc))
			return false;
		break;
	case LOP_JMP:
		if (!cback_visit_jmp_op(func, pc))
			return false;
		break;
	case LOP_JMPIFTRUE:
		if (!cback_visit_jmpiftrue_op(func, pc))
			return false;
		break;
	case LOP_JMPIFFALSE:
		if (!cback_visit_jmpiffalse_op(func, pc))
			return false;
		break;
	case LOP_JMPIFEQ:
		/* Same as JMPIFTRUE. (JMPIFEQ is an optimization hint for JIT-compiler.) */
		if (!cback_visit_jmpiftrue_op(func, pc))
			return false;
		break;
	default:
		printf("Unknow opcode.");
		return false;
	}

	return true;
}


/*
 * Put a finalization code for a plugin.
 */
bool cback_finalize_dll(void)
{
	if (!cback_write_dll_init())
		return false;

	fclose(fp);
	fp = NULL;
	
	return true;
}

/*
 * Put a finalization code for a standalone app.
 */
bool cback_finalize_standalone(void)
{
	if (!cback_write_dll_init())
		return false;

	fprintf(fp, "bool L_print(struct rt_env *rt)\n");
	fprintf(fp, "{\n");
	fprintf(fp, "    struct rt_value msg;\n");
	fprintf(fp, "    const char *s;\n");
	fprintf(fp, "    float f;\n");
	fprintf(fp, "    int i;\n");
	fprintf(fp, "    int type;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    if (!rt_get_local(rt, \"msg\", &msg))\n");
	fprintf(fp, "        return false;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    if (!rt_get_value_type(rt, &msg, &type))\n");
	fprintf(fp, "        return false;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    switch (type) {\n");
	fprintf(fp, "    case RT_VALUE_INT:\n");
	fprintf(fp, "        if (!rt_get_int(rt, &msg, &i))\n");
	fprintf(fp, "            return false;\n");
	fprintf(fp, "        printf(\"%%i\\n\", i);\n");
	fprintf(fp, "        break;\n");
	fprintf(fp, "    case RT_VALUE_FLOAT:\n");
	fprintf(fp, "        if (!rt_get_float(rt, &msg, &f))\n");
	fprintf(fp, "            return false;\n");
	fprintf(fp, "        printf(\"%%f\\n\", f);\n");
	fprintf(fp, "        break;\n");
	fprintf(fp, "    case RT_VALUE_STRING:\n");
	fprintf(fp, "        if (!rt_get_string(rt, &msg, &s))\n");
	fprintf(fp, "            return false;\n");
	fprintf(fp, "        printf(\"%%s\\n\", s);\n");
	fprintf(fp, "        break;\n");
	fprintf(fp, "    default:\n");
	fprintf(fp, "        printf(\"[object]\\n\");\n");
	fprintf(fp, "        break;\n");
	fprintf(fp, "    }\n");
	fprintf(fp, "\n");
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n");
	fprintf(fp, "\n");
	fprintf(fp, "static bool L_readline(struct rt_env *rt)\n");
	fprintf(fp, "{\n");
	fprintf(fp, "    struct rt_value ret;\n");
	fprintf(fp, "    char buf[1024];\n");
	fprintf(fp, "\n");
	fprintf(fp, "    memset(buf, 0, sizeof(buf));\n");
	fprintf(fp, "\n");
	fprintf(fp, "    fgets(buf, sizeof(buf) - 1, stdin);\n");
	fprintf(fp, "\n");
	fprintf(fp, "    if (!rt_make_string(rt, &ret, buf))\n");
	fprintf(fp, "        return false;\n");
	fprintf(fp, "    if (!rt_set_local(rt, \"$return\", &ret))\n");
	fprintf(fp, "        return false;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n");
	fprintf(fp, "\n");
	fprintf(fp, "static bool install_intrinsics(struct rt_env *rt)\n");
	fprintf(fp, "{\n");
	fprintf(fp, "    if (!rt_register_cfunc(rt, \"print\", 1, print_param, L_print))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "    if (!rt_register_cfunc(rt, \"readline\", 0, NULL, L_readline))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n");
	fprintf(fp, "\n");
	fprintf(fp, "int main(int argc, char *argv)");
	fprintf(fp, "{\n");
	fprintf(fp, "    struct rt_env *rt;\n");
	fprintf(fp, "    struct rt_value ret;\n");
	fprintf(fp, "    const char *print_param[] = {\"msg\"};\n");
	fprintf(fp, "\n");
	fprintf(fp, "    /* Create a runtime. */\n");
	fprintf(fp, "    if (!rt_create(&rt))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    /* Install intrinsics. */\n");
	fprintf(fp, "    if (!install_intrinsics(rt))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    /* Install app functions. */");
	fprintf(fp, "    if (!L_dll_init(rt))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    /* Call app main. */\n");
	fprintf(fp, "    if (!rt_call_with_name(rt, \"main\", NULL, 0, NULL, &ret))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    /* Destroy a runtime. */\n");
	fprintf(fp, "    if (!rt_destroy(rt))\n");
	fprintf(fp, "        return 1;\n");
	fprintf(fp, "\n");
	fprintf(fp, "    return ret.val.i;\n");
	fprintf(fp, "}\n");

	fclose(fp);
	fp = NULL;

	return true;
}

static bool cback_write_dll_init(void)
{
	int i, j;

	fprintf(fp, "bool L_dll_init(struct rt_env *rt)\n");
	fprintf(fp, "{\n");
	for (i = 0; i < func_count; i++) {
		fprintf(fp, "    {\n");
		if (func_table[i].param_count > 0) {
			fprintf(fp, "        const char *params[] = {");
			for (j = 0; j < func_table[i].param_count; j++)
				fprintf(fp, "\"%s\",", func_table[i].param_name[i]);
			fprintf(fp, "};\n");
			fprintf(fp, "        if (!rt_register_cfunc(rt, \"%s\", %d, params, L_%s))\n",
				func_table[i].name, func_table[i].param_count, func_table[i].name);
			fprintf(fp, "            return false;\n");
		} else {
			fprintf(fp, "        if (!rt_register_cfunc(rt, \"%s\", 0, NULL, L_%s))\n",
				func_table[i].name, func_table[i].name);
			fprintf(fp, "            return false;\n");
		}
		fprintf(fp, "    }\n");
	}
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n");
	fprintf(fp, "\n");

	return true;
}
