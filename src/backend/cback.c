/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * cback: C translation backend
 */

#include <noct/noct.h>
#include "ast.h"
#include "hir.h"
#include "lir.h"
#include "bytecode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/*
 * False assertion
 */

#define NEVER_COME_HERE	0

/*
 * Constant
 */

#define CBACK_ARG_MAX		32

/*
 * Message
 */

static const char BROKEN_BYTECODE[] = "Broken bytecode.";

/*
 * Translated function names.
 */

#define FUNC_MAX	4096

struct c_func {
	char *name;
	uint32_t param_count;
	char *param_name[CBACK_ARG_MAX];
};

static struct c_func func_table[FUNC_MAX];
static uint32_t func_count;

/*
 * Translation context.
 */

static FILE *fp;

/* Optimization level for translated output. */
static int cback_optimize_level = 1;
static bool cback_lineinfo = true;
static bool cback_simd_info;

/*
 * Set the optimization level for subsequent translations.
 */
void
noct_cback_set_optimize_level(int level)
{
	cback_optimize_level = level;
	cback_lineinfo = level == 0;
}

void
noct_cback_set_lineinfo(bool enable)
{
	cback_lineinfo = enable;
}

void
noct_cback_set_simd_info(bool enable)
{
	cback_simd_info = enable;
}

/*
 * Forward declaration
 */
static bool cback_translate_func(struct lir_func *func);
static bool cback_visit_bytecode(struct lir_func *func);

static bool cback_visit_op(struct lir_func *func, uint32_t *pc);
static bool cback_write_aot_init(void);

/*
 * Start the C backend.
 */
bool
noct_cback_start(
	const char *fname)
{
	fp = fopen(fname, "wb");
	if (fp == NULL) {
		printf("Failed to open file \"%s\".\n", fname);
		return false;
	}

	/* NOCT_AOT_INTERNAL exposes the head layout of rt_env/rt_frame. */
	fprintf(fp, "#define NOCT_AOT_INTERNAL\n");
	fprintf(fp, "#include <noct/aot.h>\n");
	fprintf(fp, "\n");

	return true;
}

/*
 * Add file to the C backend.
 */
bool
noct_cback_translate(
	const char *fname,
	const char *data)
{
	uint32_t func_count, i;

	/* Propagate the optimization level to the compiler. */
	lir_set_optimize_level(cback_optimize_level);
	lir_set_lineinfo(cback_lineinfo);

	/* Do parse, build AST. */
	if (!ast_build(fname, data)) {
		printf(N_TR("Error: %s:%d: %s\n"),
		       ast_get_file_name(),
		       ast_get_error_line(),
		       ast_get_error_message());
		return false;
	}
	if (ast_get_require_count() != 0) {
		printf("%s", N_TR("Error: require is not supported by the C transpiler; use --compile --app.\n"));
		ast_cleanup();
		return false;
	}

	/* Transform AST to HIR. */
	if (!hir_build()) {
		printf(N_TR("Error: %s:%d: %s\n"),
		       hir_get_file_name(),
		       hir_get_error_line(),
		       hir_get_error_message());
		return false;
	}

	/* For each HIR function. */
	func_count = hir_get_function_count();
	for (i = 0; i < func_count; i++) {
		struct hir_block *hfunc;
		struct lir_func *lfunc;

		/* Run the HIR optimizer (ABCE; no-op below level 2). */
		hfunc = hir_get_function(i);
		if (!hir_optimize_func(hfunc, cback_optimize_level,
				       cback_simd_info)) {
			printf(N_TR("Error: %s\n"), hir_get_error_message());
			return false;
		}

		/* Transform HIR to LIR (bytecode). */
		if (!lir_build(hfunc, &lfunc)) {
			printf(N_TR("Error: %s:%d: %s\n"),
			       lir_get_file_name(),
			       lir_get_error_line(),
			       lir_get_error_message());
			return false;;
		}

		/* Put a C function. */
		if (!cback_translate_func(lfunc))
			return false;

		/* Free a single LIR. */
		lir_cleanup(lfunc);
	}

	/* Free intermediated. */
	hir_cleanup();
	ast_cleanup();

	return true;
}

/*
 * Put a finalization code for a plugin.
 */
bool
noct_cback_finalize(void)
{
	if (!cback_write_aot_init())
		return false;

	fclose(fp);
	fp = NULL;
	
	return true;
}

/*
 * Translate LIR to C.
 */
static bool
cback_translate_func(
	struct lir_func *func)
{
	uint32_t i;

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
	fprintf(fp, "bool L_%s(struct rt_env *env)\n", func->func_name);
	fprintf(fp, "{\n");

	/* Visit a bytecode array. */
	if (!cback_visit_bytecode(func))
		return false;

	/* Put an epilogue code. */
	fprintf(fp, "/* epilogue */\n");
	fprintf(fp, "  L_pc_%d:\n", func->bytecode_size);
	fprintf(fp, "    return true;\n");
	fprintf(fp, "}\n\n");

	return true;
}

/* Visit a bytecode array. */
static bool
cback_visit_bytecode(
	struct lir_func *func)
{
	uint32_t pc;

	pc = 0;
	while (pc < func->bytecode_size) {
		if (!cback_visit_op(func, &pc))
			return false;
	}

	return true;
}

/* Get a u8 from bytecode. */
#define GET_U8(v) if (!cback_get_u8(func, pc, v)) return false
static INLINE bool cback_get_u8(
        struct lir_func *func,
        uint32_t *pc,
        int *val)
{
        if (*pc + 1 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *val = func->bytecode[*pc];     

        *pc = *pc + 1;

        return true;
}

#if 0
/* Get a u16 from bytecode. */
#define GET_U16(v) if (!cback_get_u16(func, pc, v)) return false
static INLINE bool cback_get_u16(
        struct lir_func *func,
        uint32_t *pc,
        int *val)
{
        if (*pc + 2 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *val = (int)(((uint32_t)func->bytecode[*pc] << 8) |
		     (uint32_t)func->bytecode[*pc + 1]);

        *pc = *pc + 2;

        return true;
}
#endif

/* Get a u16 tmpvar index from bytecode. */
#define GET_TMPVAR(v) if (!cback_get_tmpvar(func, pc, v)) return false
static INLINE bool cback_get_tmpvar(
        struct lir_func *func,
        uint32_t *pc,
        int *val)
{
        if (*pc + 2 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *val = (int)(((uint32_t)func->bytecode[*pc] << 8) |
		     (uint32_t)func->bytecode[*pc + 1]);
        if ((uint32_t)*val >= (uint32_t)func->tmpvar_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *pc = *pc + 2;

        return true;
}

/* Get a u32 from bytecode. */
#define GET_U32(v) if (!cback_get_u32(func, pc, v)) return false
static INLINE bool cback_get_u32(
        struct lir_func *func,
        uint32_t *pc,
        uint32_t *val)
{
        if (*pc + 4 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *val = ((uint32_t)func->bytecode[*pc + 0] << 24) |
               ((uint32_t)func->bytecode[*pc + 1] << 16) |
               ((uint32_t)func->bytecode[*pc + 2] << 8) |
                (uint32_t)func->bytecode[*pc + 3];

        *pc = *pc + 4;

        return true;
}

/* Get a u32 address from bytecode. */
#define GET_U64(v) if (!cback_get_u64(func, pc, v)) return false
static INLINE bool
cback_get_u64(
	struct lir_func *func,
	uint32_t *pc,
	uint64_t *val)
{
	if (*pc + 8 > func->bytecode_size) {
		puts(BROKEN_BYTECODE);
		return false;
	}

	*val = (((uint64_t)func->bytecode[*pc]) << 56) |
	       (((uint64_t)func->bytecode[*pc + 1]) << 48) |
	       (((uint64_t)func->bytecode[*pc + 2]) << 40) |
	       (((uint64_t)func->bytecode[*pc + 3]) << 32) |
	       (((uint64_t)func->bytecode[*pc + 4]) << 24) |
	       (((uint64_t)func->bytecode[*pc + 5]) << 16) |
	       (((uint64_t)func->bytecode[*pc + 6]) << 8) |
	       ((uint64_t)func->bytecode[*pc + 7]);
	*pc += 8;

	return true;
}

#define GET_ADDR(v) if (!cback_get_addr(func, pc, v)) return false
static INLINE bool cback_get_addr(
        struct lir_func *func,
        uint32_t *pc,
        uint32_t *val)
{
        if (*pc + 4 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *val = ((uint32_t)func->bytecode[*pc + 0] << 24) |
               ((uint32_t)func->bytecode[*pc + 1] << 16) |
               ((uint32_t)func->bytecode[*pc + 2] << 8) |
                (uint32_t)func->bytecode[*pc + 3];

        if (*val > (uint32_t)func->bytecode_size + 1) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *pc = *pc + 4;

        return true;
}

/* Get a string from bytecode. */
#define GET_STRING(s, l, h) if (!cback_get_string(func, pc, s, l, h)) return false
static INLINE bool cback_get_string(
        struct lir_func *func,
        uint32_t *pc,
        const char **s,
        uint32_t *len,
        uint32_t *hash)
{
        if (*pc + 8 > func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *len = ((uint32_t)func->bytecode[*pc + 0] << 24) |
                ((uint32_t)func->bytecode[*pc + 1] << 16) |
                ((uint32_t)func->bytecode[*pc + 2] << 8) |
                (uint32_t)func->bytecode[*pc + 3];

        *hash = ((uint32_t)func->bytecode[*pc + 4] << 24) |
                ((uint32_t)func->bytecode[*pc + 5] << 16) |
                ((uint32_t)func->bytecode[*pc + 6] << 8) |
                (uint32_t)func->bytecode[*pc + 7];

        if ((uint32_t)*pc + 8 + *len > (uint32_t)func->bytecode_size) {
                puts(BROKEN_BYTECODE);
                return false;
        }

        *s = (const char *)&func->bytecode[*pc + 8];

        *pc = *pc + 8 + *len;

        return true;
}

/* Put a label. */
#define LABEL(pc) \
	fprintf(fp, "  L_pc_%d:\n", (pc));

/* Unary OP macro. */
#define UNARY_OP(helper)						\
	int dst, src;							\
	GET_TMPVAR(&dst);						\
	GET_TMPVAR(&src);						\
	fprintf(fp, "    if (!" #helper "(env, %d, %d))", dst, src);	\
	fprintf(fp, "        return false;\n");

/* Binary OP macro. */
#define BINARY_OP(helper)							\
	int dst, src1, src2;							\
	GET_TMPVAR(&dst);							\
	GET_TMPVAR(&src1);							\
	GET_TMPVAR(&src2);							\
	fprintf(fp, "    if (!" #helper "(env, %d, %d, %d))", dst, src1, src2);	\
	fprintf(fp, "        return false;\n");

/* Visit a LOP_LINEINFO instruction. */
static INLINE bool
cback_visit_lineinfo_op(
	struct lir_func *func,
	uint32_t *pc)
{
	uint32_t line;

	GET_U32(&line);

	fprintf(fp, "/* line: %d */\n", line);

	return true;
}

/* Visit a LOP_ASSIGN instruction. */
static INLINE bool
cback_visit_assign_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, src;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src);

	fprintf(fp, "    env->frame->tmpvar[%d] = env->frame->tmpvar[%d];\n", dst, src);

	return true;
}

/* Visit a LOP_ICONST instruction. */
static INLINE bool
cback_visit_iconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	uint32_t val;

	GET_TMPVAR(&dst);
	GET_U32(&val);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = %d;\n", dst, val);

	return true;
}

/* Visit a LOP_FCONST instruction. */
static INLINE bool
cback_visit_fconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	uint32_t raw;
	float val;

	GET_TMPVAR(&dst);
	GET_U32(&raw);

	val = *(float *)&raw;

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_FLOAT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.f = %ff;\n", dst, val);

	return true;
}

/* Visit a LOP_SCONST instruction. */
static INLINE bool
cback_visit_sconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	const char *s;
	uint32_t len, hash;

	GET_TMPVAR(&dst);
	GET_STRING(&s, &len, &hash);

	/* len includes the tail NUL, same as the interpreter passes. */
	fprintf(fp, "    if (!noct_ex_make_string_with_hash(env, &env->frame->tmpvar[%d], \"%s\", %uU, 0x%08x))\n", dst, s, len, hash);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_ACONST instruction. */
static INLINE bool
cback_visit_aconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;

	GET_TMPVAR(&dst);

	fprintf(fp, "    if (!noct_ex_make_empty_array(env, &env->frame->tmpvar[%d]))\n", dst);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_DCONST instruction. */
static INLINE bool
cback_visit_dconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;

	GET_TMPVAR(&dst);

	fprintf(fp, "    if (!noct_ex_make_empty_dict(env, &env->frame->tmpvar[%d]))\n", dst);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_INC instruction. */
static INLINE bool
cback_visit_inc_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	int step;

	GET_TMPVAR(&dst);
	GET_U8(&step);

	/* In-place integer increment (same semantics as the interpreter). */
	fprintf(fp, "    if (env->frame->tmpvar[%d].type != NOCT_VALUE_INT) return false;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i += %d;\n", dst, step);

	return true;
}

/* Visit a LOP_ADD instruction. */
static INLINE bool
cback_visit_add_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_add_helper);
	return true;
}

/* Visit a LOP_SUB instruction. */
static INLINE bool
cback_visit_sub_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_sub_helper);
	return true;
}

/* Visit a LOP_MUL instruction. */
static INLINE bool
cback_visit_mul_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_mul_helper);
	return true;
}

/* Visit a LOP_DIV instruction. */
static INLINE bool
cback_visit_div_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_div_helper);
	return true;
}

/* Visit a LOP_MOD instruction. */
static INLINE bool
cback_visit_mod_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_mod_helper);
	return true;
}

/* Visit a LOP_AND instruction. */
static INLINE bool
cback_visit_and_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_and_helper);
	return true;
}

/* Visit a LOP_OR instruction. */
static INLINE bool
cback_visit_or_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_or_helper);
	return true;
}

/* Visit a LOP_XOR instruction. */
static INLINE bool
cback_visit_xor_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_xor_helper);
	return true;
}

/* Visit a LOP_XOR instruction. */
static INLINE bool
cback_visit_shl_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_shl_helper);
	return true;
}

/* Visit a LOP_XOR instruction. */
static INLINE bool
cback_visit_shr_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_shr_helper);
	return true;
}

/* Visit a LOP_NEG instruction. */
static INLINE bool
cback_visit_neg_op(
	struct lir_func *func,
	uint32_t *pc)
{
	UNARY_OP(noct_ex_neg_helper);
	return true;
}

/* Visit a LOP_NOT instruction. */
static INLINE bool
cback_visit_not_op(
	struct lir_func *func,
	uint32_t *pc)
{
	UNARY_OP(noct_ex_not_helper);
	return true;
}

/* Visit a LOP_LT instruction. */
static INLINE bool
cback_visit_lt_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_lt_helper);
	return true;
}

/* Visit a LOP_LTE instruction. */
static INLINE bool
cback_visit_lte_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_lte_helper);
	return true;
}

/* Visit a LOP_GT instruction. */
static INLINE bool
cback_visit_gt_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_gt_helper);
	return true;
}

/* Visit a LOP_GTE instruction. */
static INLINE bool
cback_visit_gte_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_gte_helper);
	return true;
}

/* Visit a LOP_EQ instruction. */
static INLINE bool
cback_visit_eq_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_eq_helper);
	return true;
}

/* Visit a LOP_NEQ instruction. */
static INLINE bool
cback_visit_neq_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_neq_helper);
	return true;
}

/* Visit a LOP_STOREARRAY instruction. */
static INLINE bool
cback_visit_storearray_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_storearray_helper);
	return true;
}

/* Visit a LOP_LOADARRAY instruction. */
static INLINE bool
cback_visit_loadarray_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_loadarray_helper);
	return true;
}

/* Visit a LOP_LEN instruction. */
static INLINE bool
cback_visit_len_op(
	struct lir_func *func,
	uint32_t *pc)
{
	UNARY_OP(noct_ex_len_helper);
	return true;
}

/* Visit a LOP_GETDICTKEYBYINDEX instruction. */
static INLINE bool
cback_visit_getdictkeybyindex_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_getdictkeybyindex_helper);
	return true;
}

/* Visit a LOP_GETDICTVALBYINDEX instruction. */
static INLINE bool
cback_visit_getdictvalbyindex_op(
	struct lir_func *func,
	uint32_t *pc)
{
	BINARY_OP(noct_ex_getdictvalbyindex_helper);
	return true;
}

/* Visit a LOP_LOADYMBOL instruction. */
static INLINE bool
cback_visit_loadsymbol_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	const char *symbol;
	uint32_t len, hash;


	GET_TMPVAR(&dst);
	GET_STRING(&symbol, &len, &hash);

	fprintf(fp, "    if (!noct_ex_loadsymbol_helper(env, %d, \"%s\", %uu, %uu))\n", dst, symbol, len, hash);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_STORESYMBOL instruction. */
static INLINE bool
cback_visit_storesymbol_op(
	struct lir_func *func,
	uint32_t *pc)
{
	const char *symbol;
	uint32_t len, hash;
	int src;


	GET_STRING(&symbol, &len, &hash);
	GET_TMPVAR(&src);

	fprintf(fp, "    if (!noct_ex_storesymbol_helper(env, \"%s\", %uu, %uu, %d))\n", symbol, len, hash, src);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_LOADDOT instruction. */
static INLINE bool
cback_visit_loaddot_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, dict;
	const char *field;
	uint32_t len, hash;


	GET_TMPVAR(&dst);
	GET_TMPVAR(&dict);
	GET_STRING(&field, &len, &hash);

	fprintf(fp, "    if (!noct_ex_loaddot_helper(env, %d, %d, \"%s\", %uu, %uu))\n", dst, dict, field, len, hash);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_STOREDOT instruction. */
static INLINE bool
cback_visit_storedot_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int src, dict;
	const char *field;
	uint32_t len, hash;


	GET_TMPVAR(&dict);
	GET_STRING(&field, &len, &hash);
	GET_TMPVAR(&src);

	fprintf(fp, "    if (!noct_ex_storedot_helper(env, %d, \"%s\", %uu, %uu, %d))\n", dict, field, len, hash, src);
	fprintf(fp, "        return false;\n");

	return true;
}

/* Visit a LOP_CALL instruction. */
static INLINE bool
cback_visit_call_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst_tmpvar;
	int func_tmpvar;
	int arg_count;
	int arg_tmpvar;
	int arg[CBACK_ARG_MAX];
	int i;


	GET_TMPVAR(&dst_tmpvar);
	GET_TMPVAR(&func_tmpvar);
	GET_U8(&arg_count);
	for (i = 0; i < arg_count; i++) {
		GET_TMPVAR(&arg_tmpvar);
		arg[i] = arg_tmpvar;
	}

	fprintf(fp, "    {\n");
	fprintf(fp, "        int arg[%d] = {", arg_count);
	for (i = 0; i < arg_count; i++)
		fprintf(fp, "%d,", arg[i]);
	fprintf(fp, "};\n");
	fprintf(fp, "        if (!noct_ex_call_helper(env, %d, %d, %d, arg))\n", dst_tmpvar, func_tmpvar, arg_count);
	fprintf(fp, "            return false;\n");
	fprintf(fp, "    }\n");

	return true;
}

/* Visit a LOP_THISCALL instruction. */
static INLINE bool
cback_visit_thiscall_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst_tmpvar;
	int obj_tmpvar;
	int func_tmpvar;
	int arg_count;
	int arg_tmpvar;
	int arg[CBACK_ARG_MAX];
	int i;

	GET_TMPVAR(&dst_tmpvar);
	GET_TMPVAR(&obj_tmpvar);
	GET_TMPVAR(&func_tmpvar);
	GET_U8(&arg_count);
	for (i = 0; i < arg_count; i++) {
		GET_TMPVAR(&arg_tmpvar);
		arg[i] = arg_tmpvar;
	}

	fprintf(fp, "    {\n");
	fprintf(fp, "        int arg[%d] = {", arg_count);
	for (i = 0; i < arg_count; i++)
		fprintf(fp, "%d,", arg[i]);
	fprintf(fp, "};\n");
	fprintf(fp, "        if (!noct_ex_thiscall_helper(env, %d, %d, NULL, 0, %uu, %d, arg))\n", dst_tmpvar, obj_tmpvar, (uint32_t)func_tmpvar, arg_count);
	fprintf(fp, "            return false;\n");
	fprintf(fp, "    }\n");

	return true;
}

/* Visit a LOP_JMP instruction. */
static inline bool
cback_visit_jmp_op(
	struct lir_func *func,
	uint32_t *pc)
{
	uint32_t target;

	GET_ADDR(&target);

	fprintf(fp, "    goto L_pc_%d;\n", target);

	return true;
}

/* Visit a LOP_JMPIFTRUE instruction. */
static bool
cback_visit_jmpiftrue_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int src;
	uint32_t target;

	GET_TMPVAR(&src);
	GET_ADDR(&target);

	fprintf(fp, "    if (env->frame->tmpvar[%d].val.i != 0)\n", src);
	fprintf(fp, "        goto L_pc_%d;\n", target);

	return true;
}

/* Visit a LOP_JMPIFFALSE instruction. */
static INLINE bool
cback_visit_jmpiffalse_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int src;
	uint32_t target;

	GET_TMPVAR(&src);
	GET_ADDR(&target);

	fprintf(fp, "    if (env->frame->tmpvar[%d].val.i == 0)\n", src);
	fprintf(fp, "        goto L_pc_%d;\n", target);

	return true;
}

/* Visit an instruction. */
/* Visit a LOP_LICONST instruction. */
static INLINE bool
cback_visit_liconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	uint64_t raw;

	GET_TMPVAR(&dst);
	GET_U64(&raw);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_LONG;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.l = (int64_t)%lldLL;\n", dst, (long long)(int64_t)raw);

	return true;
}

/* Visit a LOP_LFCONST instruction. */
static INLINE bool
cback_visit_lfconst_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst;
	uint64_t raw;
	double val;

	GET_TMPVAR(&dst);
	GET_U64(&raw);

	memcpy(&val, &raw, sizeof(double));

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_DOUBLE;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.lf = %.17g;\n", dst, val);

	return true;
}

/* Visit a LOP_SAFEPOINT instruction. */
static INLINE bool
cback_visit_safepoint_op(
	struct lir_func *func,
	uint32_t *pc)
{
	UNUSED_PARAMETER(func);
	UNUSED_PARAMETER(pc);

	fprintf(fp, "    if (!noct_ex_safepoint_helper(env)) return false;\n");

	return true;
}

/* Visit a LOP_PBASE instruction. (ABCE) */
static INLINE bool
cback_visit_pbase_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, src;
	int base_id;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src);
	GET_U8(&base_id);
	UNUSED_PARAMETER(base_id);

	fprintf(fp, "    if (!noct_ex_pbase_helper(env, %d, %d)) return false;\n", dst, src);

	return true;
}

/* Visit a LOP_PLEN instruction. (ABCE) */
static INLINE bool
cback_visit_plen_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, src;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src);

	fprintf(fp, "    if (!noct_ex_plen_helper(env, %d, %d)) return false;\n", dst, src);

	return true;
}

/* Visit a LOP_PCHECK instruction. (ABCE) */
static INLINE bool
cback_visit_pcheck_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, src, type;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src);
	GET_U8(&type);

	fprintf(fp, "    if (!noct_ex_pcheck_helper(env, %d, %d, %d)) return false;\n", dst, src, type);

	return true;
}

/* Visit a LOP_TYPEIS instruction. (ABCE) */
static INLINE bool
cback_visit_typeis_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, src, type;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src);
	GET_U8(&type);

	fprintf(fp, "    if (!noct_ex_typeis_helper(env, %d, %d, %d)) return false;\n", dst, src, type);

	return true;
}

/* Visit a LOP_PLOAD8U instruction. (ABCE: raw bounds-check-free load.) */
static INLINE bool
cback_visit_pload8u_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int)*((const unsigned char *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PSTORE8 instruction. (ABCE: raw bounds-check-free store.) */
static INLINE bool
cback_visit_pstore8_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int base, ofs, src;

	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	GET_TMPVAR(&src);

	fprintf(fp, "    *((unsigned char *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i) = (unsigned char)env->frame->tmpvar[%d].val.i;\n",
		base, ofs, src);

	return true;
}

/* Visit a LOP_PLOAD8S instruction. (ABCE raw load.) */
static INLINE bool
cback_visit_pload8s_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int)*((const int8_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PLOAD16U instruction. (ABCE raw load.) */
static INLINE bool
cback_visit_pload16u_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int)*((const uint16_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PLOAD16S instruction. (ABCE raw load.) */
static INLINE bool
cback_visit_pload16s_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int)*((const int16_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PLOAD32 instruction. (ABCE raw load.) */
static INLINE bool
cback_visit_pload32_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int)*((const int32_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PLOAD64 instruction. (ABCE raw load.) */
static INLINE bool
cback_visit_pload64_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);

	fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_LONG;\n", dst);
	fprintf(fp, "    env->frame->tmpvar[%d].val.l = *((const int64_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i);\n",
		dst, base, ofs);

	return true;
}

/* Visit a LOP_PSTORE16 instruction. (ABCE raw store; int source.) */
static INLINE bool
cback_visit_pstore16_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int base, ofs, src;

	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	GET_TMPVAR(&src);

	fprintf(fp, "    *((uint16_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i) = (uint16_t)env->frame->tmpvar[%d].val.i;\n",
		base, ofs, src);

	return true;
}

/* Visit a LOP_PSTORE32 instruction. (ABCE raw store; int source.) */
static INLINE bool
cback_visit_pstore32_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int base, ofs, src;

	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	GET_TMPVAR(&src);

	fprintf(fp, "    *((uint32_t *)(intptr_t)env->frame->tmpvar[%d].val.l + env->frame->tmpvar[%d].val.i) = (uint32_t)env->frame->tmpvar[%d].val.i;\n",
		base, ofs, src);

	return true;
}

/* Visit a LOP_PSTORE64 instruction. (ABCE raw store; int/long source.) */
static INLINE bool
cback_visit_pstore64_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int base, ofs, src;

	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	GET_TMPVAR(&src);

	fprintf(fp, "    if (!noct_ex_pstore64_helper(env, %d, %d, %d)) return false;\n", base, ofs, src);

	return true;
}

static INLINE bool
cback_visit_ploadf32_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int dst, base, ofs;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	fprintf(fp, "    if (!noct_ex_ploadf32_helper(env, %d, %d, %d)) return false;\n",
		dst, base, ofs);
	return true;
}

static INLINE bool
cback_visit_pstoref32_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int base, ofs, src;

	GET_TMPVAR(&base);
	GET_TMPVAR(&ofs);
	GET_TMPVAR(&src);
	fprintf(fp, "    if (!noct_ex_pstoref32_helper(env, %d, %d, %d)) return false;\n",
		base, ofs, src);
	return true;
}

/* Visit a LOP_CHECKTYPE instruction. */
static INLINE bool
cback_visit_checktype_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int slot, type;

	GET_TMPVAR(&slot);
	GET_U8(&type);

	fprintf(fp, "    if (!noct_ex_checktype_helper(env, %d, %d)) return false;\n", slot, type);

	return true;
}

/*
 * Visit an OP_IADD..OP_FGTE instruction.  (Typed arithmetic, design
 * 07.)  Open-coded into the generated C so the C compiler can fold
 * and inline; the two ops with an error path (IDIV/IMOD) call their
 * runtime helper instead, which carries the division-by-zero error
 * plumbing.  Integer arithmetic is emitted in uint32_t (defined
 * wraparound, identical to the helpers and the JITs).
 */
static INLINE bool
cback_visit_typed_op(
	struct lir_func *func,
	uint32_t *pc,
	int op)
{
	/* Indexed by (op - OP_IADD); NULL = special-cased below. */
	static const char *iexpr[] = {
		"(a + b)",	/* IADD */
		"(a - b)",	/* ISUB */
		"(a * b)",	/* IMUL */
		NULL,		/* IDIV */
		NULL,		/* IMOD */
		"(a & b)",	/* IAND */
		"(a | b)",	/* IOR  */
		"(a ^ b)"	/* IXOR */
	};
	int dst, src1, src2;

	GET_TMPVAR(&dst);
	GET_TMPVAR(&src1);
	if (op == OP_ISHL || op == OP_ISHR) {
		/* The shift count is an imm8, not a tmpvar. */
		GET_U8(&src2);
	} else {
		GET_TMPVAR(&src2);
	}

	switch (op) {
	case OP_IADD:
	case OP_ISUB:
	case OP_IMUL:
	case OP_IAND:
	case OP_IOR:
	case OP_IXOR:
		fprintf(fp, "    {\n");
		fprintf(fp, "        uint32_t a = (uint32_t)env->frame->tmpvar[%d].val.i;\n", src1);
		fprintf(fp, "        uint32_t b = (uint32_t)env->frame->tmpvar[%d].val.i;\n", src2);
		fprintf(fp, "        env->frame->tmpvar[%d].val.i = (int32_t)%s;\n",
			dst, iexpr[op - OP_IADD]);
		fprintf(fp, "        env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
		fprintf(fp, "    }\n");
		break;
	case OP_IDIV:
		fprintf(fp, "    if (!noct_ex_idiv_helper(env, %d, %d, %d)) return false;\n",
			dst, src1, src2);
		break;
	case OP_IMOD:
		fprintf(fp, "    if (!noct_ex_imod_helper(env, %d, %d, %d)) return false;\n",
			dst, src1, src2);
		break;
	case OP_ISHL:
	case OP_ISHR:
		/* src2 is the shift-count immediate (0..31). */
		fprintf(fp, "    env->frame->tmpvar[%d].val.i = (int32_t)((uint32_t)env->frame->tmpvar[%d].val.i %s %d);\n",
			dst, src1, op == OP_ISHL ? "<<" : ">>", src2 & 31);
		fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
		break;
	case OP_ILT:
	case OP_ILTE:
	case OP_IGT:
	case OP_IGTE:
		fprintf(fp, "    env->frame->tmpvar[%d].val.i = (env->frame->tmpvar[%d].val.i %s env->frame->tmpvar[%d].val.i) ? 1 : 0;\n",
			dst, src1,
			op == OP_ILT ? "<" : op == OP_ILTE ? "<=" :
			op == OP_IGT ? ">" : ">=", src2);
		fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
		break;
	case OP_FADD:
	case OP_FSUB:
	case OP_FMUL:
	case OP_FDIV:
		fprintf(fp, "    env->frame->tmpvar[%d].val.f = env->frame->tmpvar[%d].val.f %s env->frame->tmpvar[%d].val.f;\n",
			dst, src1,
			op == OP_FADD ? "+" : op == OP_FSUB ? "-" :
			op == OP_FMUL ? "*" : "/", src2);
		fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_FLOAT;\n", dst);
		break;
	case OP_FLT:
	case OP_FLTE:
	case OP_FGT:
	case OP_FGTE:
		fprintf(fp, "    env->frame->tmpvar[%d].val.i = (env->frame->tmpvar[%d].val.f %s env->frame->tmpvar[%d].val.f) ? 1 : 0;\n",
			dst, src1,
			op == OP_FLT ? "<" : op == OP_FLTE ? "<=" :
			op == OP_FGT ? ">" : ">=", src2);
		fprintf(fp, "    env->frame->tmpvar[%d].type = NOCT_VALUE_INT;\n", dst);
		break;
	default:
		return false;
	}

	return true;
}

/*
 * Visit an OP_VLOADI32X4..OP_VSHRI32X4 instruction.  (128-bit SIMD,
 * design 06.)  The generated C calls the portable emulation helpers,
 * which the runtime always compiles (aot.h declares them).
 */
static INLINE bool
cback_visit_vector_op(
	struct lir_func *func,
	uint32_t *pc,
	int op)
{
	static const char *helper_name[] = {
		"noct_ex_vloadi32x4_helper",
		"noct_ex_vstorei32x4_helper",
		"noct_ex_vsplati32_helper",
		"noct_ex_vgetlanei32_helper",
		"noct_ex_vmov128_helper",
		"noct_ex_vaddi32x4_helper",
		"noct_ex_vsubi32x4_helper",
		"noct_ex_vmuli32x4_helper",
		"noct_ex_vand128_helper",
		"noct_ex_vor128_helper",
		"noct_ex_vxor128_helper",
		"noct_ex_vshli32x4_helper",
		"noct_ex_vshri32x4_helper",
		"noct_ex_vloadf32x4_helper",
		"noct_ex_vstoref32x4_helper",
		"noct_ex_vsplatf32_helper",
		"noct_ex_vgetlanef32_helper",
		"noct_ex_vaddf32x4_helper",
		"noct_ex_vsubf32x4_helper",
		"noct_ex_vmulf32x4_helper",
		"noct_ex_vdivf32x4_helper",
		"noct_ex_vcvti32f32x4_helper",
		"noct_ex_vcvtf32i32x4_helper"
	};
	int a, b, c;

	switch (op) {
	case OP_VLOADI32X4:
	case OP_VLOADF32X4:
		GET_U8(&a);
		GET_TMPVAR(&b);
		GET_TMPVAR(&c);
		break;
	case OP_VSTOREI32X4:
	case OP_VSTOREF32X4:
		GET_TMPVAR(&a);
		GET_TMPVAR(&b);
		GET_U8(&c);
		break;
	case OP_VSPLATI32:
	case OP_VSPLATF32:
		GET_U8(&a);
		GET_TMPVAR(&b);
		c = 0;
		break;
	case OP_VGETLANEI32:
	case OP_VGETLANEF32:
		GET_TMPVAR(&a);
		GET_U8(&b);
		GET_U8(&c);
		break;
	case OP_VMOV128:
	case OP_VCVTI32F32X4:
	case OP_VCVTF32I32X4:
		GET_U8(&a);
		GET_U8(&b);
		c = 0;
		break;
	default:
		GET_U8(&a);
		GET_U8(&b);
		GET_U8(&c);
		break;
	}

	fprintf(fp, "    if (!%s(env, %d, %d, %d)) return false;\n",
		helper_name[op - OP_VLOADI32X4], a, b, c);

	return true;
}

static INLINE bool
cback_visit_vindex_hint_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int index_tmp, stop_tmp, remaining_tmp;
	int index_id, lanes, flags;

	GET_TMPVAR(&index_tmp);
	GET_TMPVAR(&stop_tmp);
	GET_TMPVAR(&remaining_tmp);
	GET_U8(&index_id);
	GET_U8(&lanes);
	GET_U8(&flags);
	UNUSED_PARAMETER(index_tmp);
	UNUSED_PARAMETER(stop_tmp);
	UNUSED_PARAMETER(remaining_tmp);
	UNUSED_PARAMETER(index_id);
	UNUSED_PARAMETER(lanes);
	UNUSED_PARAMETER(flags);
	return true;
}

static INLINE bool
cback_visit_subjnz_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int value, decrement;
	uint32_t target;

	GET_TMPVAR(&value);
	GET_U8(&decrement);
	GET_U32(&target);
	fprintf(fp, "    env->frame->tmpvar[%d].val.i -= %d;\n",
		value, decrement);
	fprintf(fp, "    if (env->frame->tmpvar[%d].val.i != 0) goto L_pc_%u;\n",
		value, target);
	return true;
}

static INLINE bool
cback_visit_vori32x4i_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int vd, vs, imm, shift;

	GET_U8(&vd);
	GET_U8(&vs);
	GET_U8(&imm);
	GET_U8(&shift);
	fprintf(fp,
		"    if (!noct_ex_vori32x4i_helper(env, %d, %d, %d)) return false;\n",
		vd, vs, (imm << 8) | shift);
	return true;
}

static INLINE bool
cback_visit_vfmaf32x4_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int vd, va, vb, vc;

	GET_U8(&vd);
	GET_U8(&va);
	GET_U8(&vb);
	GET_U8(&vc);
	fprintf(fp,
		"    if (!noct_ex_vfmaf32x4_helper(env, %d, %d, %d)) return false;\n",
		vd, va, (vb << 8) | vc);
	return true;
}

static bool
cback_visit_op(
	struct lir_func *func,
	uint32_t *pc)
{
	int op;

	LABEL(*pc);

	GET_U8(&op);

	switch (op) {
	case OP_NOP:
		/* NOP */
		(*pc)++;
		break;
	case OP_LINEINFO:
		if (!cback_visit_lineinfo_op(func, pc))
			return false;
		break;
	case OP_ASSIGN:
		if (!cback_visit_assign_op(func, pc))
			return false;
		break;
	case OP_ICONST:
		if (!cback_visit_iconst_op(func, pc))
			return false;
		break;
	case OP_FCONST:
		if (!cback_visit_fconst_op(func, pc))
			return false;
		break;
	case OP_SCONST:
		if (!cback_visit_sconst_op(func, pc))
			return false;
		break;
	case OP_ACONST:
		if (!cback_visit_aconst_op(func, pc))
			return false;
		break;
	case OP_DCONST:
		if (!cback_visit_dconst_op(func, pc))
			return false;
		break;
	case OP_INC:
		if (!cback_visit_inc_op(func, pc))
			return false;
		break;
	case OP_ADD:
		if (!cback_visit_add_op(func, pc))
			return false;
		break;
	case OP_SUB:
		if (!cback_visit_sub_op(func, pc))
			return false;
		break;
	case OP_MUL:
		if (!cback_visit_mul_op(func, pc))
			return false;
		break;
	case OP_DIV:
		if (!cback_visit_div_op(func, pc))
			return false;
		break;
	case OP_MOD:
		if (!cback_visit_mod_op(func, pc))
			return false;
		break;
	case OP_AND:
		if (!cback_visit_and_op(func, pc))
			return false;
		break;
	case OP_OR:
		if (!cback_visit_or_op(func, pc))
			return false;
		break;
	case OP_XOR:
		if (!cback_visit_xor_op(func, pc))
			return false;
		break;
	case OP_NEG:
		if (!cback_visit_neg_op(func, pc))
			return false;
		break;
	case OP_NOT:
		if (!cback_visit_not_op(func, pc))
			return false;
		break;
	case OP_LT:
		if (!cback_visit_lt_op(func, pc))
			return false;
		break;
	case OP_LTE:
		if (!cback_visit_lte_op(func, pc))
			return false;
		break;
	case OP_GT:
		if (!cback_visit_gt_op(func, pc))
			return false;
		break;
	case OP_GTE:
		if (!cback_visit_gte_op(func, pc))
			return false;
		break;
	case OP_EQ:
		if (!cback_visit_eq_op(func, pc))
			return false;
		break;
	case OP_EQI:
		/* Same as EQ. EQI is an optimization hint for JIT-compiler. */
		if (!cback_visit_eq_op(func, pc))
			return false;
		break;
	case OP_NEQ:
		if (!cback_visit_neq_op(func, pc))
			return false;
		break;
	case OP_SHL:
		if (!cback_visit_shl_op(func, pc))
			return false;
		break;
	case OP_SHR:
		if (!cback_visit_shr_op(func, pc))
			return false;
		break;
	case OP_STOREARRAY:
		if (!cback_visit_storearray_op(func, pc))
			return false;
		break;
	case OP_LOADARRAY:
		if (!cback_visit_loadarray_op(func, pc))
			return false;
		break;
	case OP_LEN:
		if (!cback_visit_len_op(func, pc))
			return false;
		break;
	case OP_GETDICTKEYBYINDEX:
		if (!cback_visit_getdictkeybyindex_op(func, pc))
			return false;
		break;
	case OP_GETDICTVALBYINDEX:
		if (!cback_visit_getdictvalbyindex_op(func, pc))
			return false;
		break;
	case OP_LOADSYMBOL:
		if (!cback_visit_loadsymbol_op(func, pc))
			return false;
		break;
	case OP_STORESYMBOL:
		if (!cback_visit_storesymbol_op(func, pc))
			return false;
		break;
	case OP_LOADDOT:
		if (!cback_visit_loaddot_op(func, pc))
			return false;
		break;
	case OP_STOREDOT:
		if (!cback_visit_storedot_op(func, pc))
			return false;
		break;
	case OP_CALL:
		if (!cback_visit_call_op(func, pc))
			return false;
		break;
	case OP_THISCALL:
		if (!cback_visit_thiscall_op(func, pc))
			return false;
		break;
	case OP_JMP:
		if (!cback_visit_jmp_op(func, pc))
			return false;
		break;
	case OP_JMPIFTRUE:
		if (!cback_visit_jmpiftrue_op(func, pc))
			return false;
		break;
	case OP_JMPIFFALSE:
		if (!cback_visit_jmpiffalse_op(func, pc))
			return false;
		break;
	case OP_JMPIFEQ:
		/* Same as JMPIFTRUE. (JMPIFEQ is an optimization hint for JIT-compiler.) */
		if (!cback_visit_jmpiftrue_op(func, pc))
			return false;
		break;
	case OP_LICONST:
		if (!cback_visit_liconst_op(func, pc))
			return false;
		break;
	case OP_LFCONST:
		if (!cback_visit_lfconst_op(func, pc))
			return false;
		break;
	case OP_SAFEPOINT:
		if (!cback_visit_safepoint_op(func, pc))
			return false;
		break;
	case OP_PBASE:
		if (!cback_visit_pbase_op(func, pc))
			return false;
		break;
	case OP_PLEN:
		if (!cback_visit_plen_op(func, pc))
			return false;
		break;
	case OP_PCHECK:
		if (!cback_visit_pcheck_op(func, pc))
			return false;
		break;
	case OP_TYPEIS:
		if (!cback_visit_typeis_op(func, pc))
			return false;
		break;
	case OP_PLOAD8U:
		if (!cback_visit_pload8u_op(func, pc))
			return false;
		break;
	case OP_PSTORE8:
		if (!cback_visit_pstore8_op(func, pc))
			return false;
		break;
	case OP_CHECKTYPE:
		if (!cback_visit_checktype_op(func, pc))
			return false;
		break;
	case OP_PLOAD8S:
		if (!cback_visit_pload8s_op(func, pc))
			return false;
		break;
	case OP_PLOAD16U:
		if (!cback_visit_pload16u_op(func, pc))
			return false;
		break;
	case OP_PLOAD16S:
		if (!cback_visit_pload16s_op(func, pc))
			return false;
		break;
	case OP_PLOAD32:
		if (!cback_visit_pload32_op(func, pc))
			return false;
		break;
	case OP_PLOAD64:
		if (!cback_visit_pload64_op(func, pc))
			return false;
		break;
	case OP_PSTORE16:
		if (!cback_visit_pstore16_op(func, pc))
			return false;
		break;
	case OP_PSTORE32:
		if (!cback_visit_pstore32_op(func, pc))
			return false;
		break;
	case OP_PSTORE64:
		if (!cback_visit_pstore64_op(func, pc))
			return false;
		break;
	case OP_PLOADF32:
		if (!cback_visit_ploadf32_op(func, pc))
			return false;
		break;
	case OP_PSTOREF32:
		if (!cback_visit_pstoref32_op(func, pc))
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
		if (!cback_visit_typed_op(func, pc, op))
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
		if (!cback_visit_vector_op(func, pc, op))
			return false;
		break;
	case OP_VINDEX_HINT:
		if (!cback_visit_vindex_hint_op(func, pc))
			return false;
		break;
	case OP_SUBJNZ:
		if (!cback_visit_subjnz_op(func, pc))
			return false;
		break;
	case OP_VORI32X4I:
		if (!cback_visit_vori32x4i_op(func, pc))
			return false;
		break;
	case OP_VFMAF32X4:
		if (!cback_visit_vfmaf32x4_op(func, pc))
			return false;
		break;
	default:
		printf("Unknown opcode.");
		return false;
	}

	return true;
}

static bool
cback_write_aot_init(void)
{
	uint32_t i, j;

	fprintf(fp, "bool init_aot_code(NoctEnv *env)\n");
	fprintf(fp, "{\n");
	for (i = 0; i < func_count; i++) {
		fprintf(fp, "    {\n");
		if (func_table[i].param_count > 0) {
			fprintf(fp, "        const char *params[] = {");
			for (j = 0; j < func_table[i].param_count; j++)
				fprintf(fp, "\"%s\",", func_table[i].param_name[j]);
			fprintf(fp, "};\n");
			fprintf(fp, "        if (!noct_register_cfunc(env, \"%s\", %d, params, L_%s, NULL))\n",
				func_table[i].name, func_table[i].param_count, func_table[i].name);
			fprintf(fp, "            return false;\n");
		} else {
			fprintf(fp, "        if (!noct_register_cfunc(env, \"%s\", 0, NULL, L_%s, NULL))\n",
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
