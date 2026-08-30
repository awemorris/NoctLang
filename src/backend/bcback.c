/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Bytecode backend. */

#include <noct/noct.h>
#include "ast.h"
#include "hir.h"
#include "fast.h"
#include "lir.h"
#include "bytecode.h"

#include <stdio.h>
#include <stdint.h>

enum bcback_func_kind {
	BCBACK_FUNC_NORMAL = 0,
	BCBACK_FUNC_FAST = 1
};

static FILE *fp;
static int bcback_optimize_level = 1;
static bool bcback_lineinfo = true;
static bool bcback_simd_info;

static bool bcback_write_header(FILE *out, const char *source, uint32_t count);
static bool bcback_write_fast_signature(FILE *out, const struct lir_func *func);
static bool bcback_write_function(FILE *out, const struct lir_func *func);

static bool
bcback_write_header(FILE *out, const char *source, uint32_t count)
{
	return fprintf(out, "Noct Bytecode 1.0\nSource\n%s\n"
		       "Number Of Functions\n%u\n", source, count) >= 0;
}

/* Write one exact sparse fast-function signature. */
static bool
bcback_write_fast_signature(
	FILE *out,
	const struct lir_func *func)
{
	const struct fast_signature *signature;
	const struct fast_param_contract *param;
	const struct fast_extent *extent;
	uint32_t param_index;
	uint32_t axis;

	signature = &func->fast_signature;
	if (!signature->valid)
		return false;
	if (signature->version != NOCT_FAST_SIGNATURE_VERSION)
		return false;
	if (!fast_signature_valid(signature))
		return false;
	if (signature->param_count != func->param_count)
		return false;
	if (signature->param_count != 0 && signature->param == NULL)
		return false;

	if (fprintf(
		out,
		"Fast Signature\n%u\n%d\n%u\n%d\n",
		signature->version,
		signature->valid ? 1 : 0,
		signature->param_count,
		signature->return_type) < 0)
		return false;

	/* Write every parameter contract in declaration order. */
	for (param_index = 0;
	     param_index < signature->param_count;
	     param_index++) {
		param = &signature->param[param_index];
		if (param->rank > NOCT_FAST_RANK_MAX)
			return false;
		if (param->rank != 0 && param->extent == NULL)
			return false;

		if (fprintf(
			out,
			"%d\n%d\n%d\n%u\n",
			param->value_type,
			param->packed_type,
			param->restricted ? 1 : 0,
			param->rank) < 0)
			return false;

		/* Write the exact extent expression for every shaped axis. */
		for (axis = 0; axis < param->rank; axis++) {
			extent = &param->extent[axis];
			if (fprintf(out, "%d\n", extent->kind) < 0)
				return false;

			if (extent->kind == FAST_EXTENT_CONST) {
				if (fprintf(
					out,
					"%lld\n",
					(long long)extent->value.constant) < 0) {
					return false;
				}
			} else if (extent->kind == FAST_EXTENT_PARAM) {
				if (fprintf(
					out,
					"%u\n",
					extent->value.param_index) < 0) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	return true;
}

static bool
bcback_write_function(FILE *out, const struct lir_func *f)
{
	uint32_t j;
	int any;

	if (fprintf(out, "Begin Function\nName\n%s\nSource\n%s\nParameters\n%u\n",
		    f->func_name, f->file_name, f->param_count) < 0)
		return false;
	for (j = 0; j < f->param_count; j++)
		if (fprintf(out, "%s\n", f->param_name[j]) < 0) return false;
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_type[j] >= 0) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Types\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_type[j]) < 0) return false;
	}
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_packed_type[j] >= 0) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Packed Types\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_packed_type[j]) < 0) return false;
	}
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_restricted[j]) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Restricted\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_restricted[j] ? 1 : 0) < 0)
				return false;
	}
	if ((f->return_type >= 0 || f->is_fast) &&
	    fprintf(out, "Return Type\n%d\n%d\n%d\n", f->return_type,
		    f->return_packed_type, f->return_type_checked ? 1 : 0) < 0)
		return false;
	if (f->has_vector_ops && fprintf(out, "Vector Ops\n1\n") < 0)
		return false;
	if (f->is_fast) {
		if (fprintf(out, "Function Kind\n%d\n", BCBACK_FUNC_FAST) < 0)
			return false;
		if (!bcback_write_fast_signature(out, f))
			return false;
	}
	if (f->has_fma_ops && fprintf(out, "FMA Ops\n1\n") < 0)
		return false;
	if (fprintf(out, "Temporary Size\n%u\nBytecode Size\n%u\n",
		    f->tmpvar_size, f->bytecode_size) < 0)
		return false;
	if (f->bytecode_size != 0 &&
	    fwrite(f->bytecode, 1, f->bytecode_size, out) != f->bytecode_size)
		return false;
	return fprintf(out, "\nEnd Function\n") >= 0;
}

NOCT_DLL void
noct_bcback_set_optimize_level(int level)
{
	bcback_optimize_level = level;
	bcback_lineinfo = level == 0;
}

NOCT_DLL void
noct_bcback_set_lineinfo(bool enable)
{
	bcback_lineinfo = enable;
}

NOCT_DLL void
noct_bcback_set_simd_info(bool enable)
{
	bcback_simd_info = enable;
}

NOCT_DLL bool
noct_bcback_start(const char *out_file_name)
{
	fp = fopen(out_file_name, "wb");
	if (fp == NULL) {
		printf("Failed to open file \"%s\".\n", out_file_name);
		return false;
	}
	return true;
}

NOCT_DLL bool
noct_bcback_translate(const char *source_file_name, const char *source_data)
{
	uint32_t count;
	uint32_t i;
	bool ok = false;

	if (!ast_build(source_file_name, source_data)) {
		printf(N_TR("Error: %s:%d: %s\n"), ast_get_file_name(),
		       ast_get_error_line(), ast_get_error_message());
		ast_cleanup();
		return false;
	}
	if (ast_get_require_count() != 0) {
		printf("%s", N_TR("Error: require is not supported by the bytecode backend.\n"));
		ast_cleanup();
		return false;
	}
	if (!hir_build()) {
		printf(N_TR("Error: %s:%d: %s\n"), hir_get_file_name(),
		       hir_get_error_line(), hir_get_error_message());
		ast_cleanup();
		return false;
	}
	lir_set_optimize_level(bcback_optimize_level);
	lir_set_lineinfo(bcback_lineinfo);
	count = hir_get_function_count();
	if (!bcback_write_header(fp, source_file_name, count))
		goto cleanup;
	for (i = 0; i < count; i++) {
		struct hir_block *hfunc = hir_get_function(i);
		struct lir_func *lfunc;
		if (!hir_optimize_func(hfunc, bcback_optimize_level,
				       bcback_simd_info)) {
			printf(N_TR("Error: %s\n"), hir_get_error_message());
			goto cleanup;
		}
		if (!lir_build(hfunc, &lfunc)) {
			printf(N_TR("Error: %s:%d: %s\n"), lir_get_file_name(),
			       lir_get_error_line(), lir_get_error_message());
			goto cleanup;
		}
		if (!bcback_write_function(fp, lfunc)) {
			lir_cleanup(lfunc);
			goto cleanup;
		}
		lir_cleanup(lfunc);
	}
	ok = true;
cleanup:
	hir_cleanup();
	ast_cleanup();
	return ok;
}

NOCT_DLL bool
noct_bcback_finalize(void)
{
	bool ok;
	if (fp == NULL)
		return false;
	ok = fclose(fp) == 0;
	fp = NULL;
	return ok;
}
