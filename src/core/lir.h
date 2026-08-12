/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * LIR: Low-level Intermediate Representation
 */

#ifndef NOCT_LIR_H
#define NOCT_LIR_H

#include <noct/noct.h>
#include "accel.h"
#include "fast.h"

#define LIR_PARAM_SIZE		32

/*
 * Frame size limit: locals + expression temps per function.  Shared
 * with the HIR optimizer (hir_opt_cse.c budgets its $cseN home locals
 * against this so a program that compiles at level 0 always compiles
 * at level 2).
 */
#define LIR_TMPVAR_MAX		128

struct hir_block;

struct lir_func {
	char *file_name;
	char *func_name;
	uint32_t param_count;
	char *param_name[LIR_PARAM_SIZE];

	/* NOCT_VALUE_* tag per param, or -1 = unannotated. */
	int param_type[LIR_PARAM_SIZE];
	/* NOCT_PACKED_* element kind, or -1 = not typed packed. */
	int param_packed_type[LIR_PARAM_SIZE];
	/* rpacked* source annotation. */
	bool param_restricted[LIR_PARAM_SIZE];
	int param_accel_access[LIR_PARAM_SIZE];
	int param_accel_transport[LIR_PARAM_SIZE];
	unsigned int param_accel_effect[LIR_PARAM_SIZE];
	/* Optional declared return tag and packed element kind. */
	int return_type;
	int return_packed_type;
	/* The bytecode enforces the declared return type on every edge. */
	bool return_type_checked;
	/* Bytecode contains at least one OP_V* instruction. */
	bool has_vector_ops;
	bool is_accel;
	int func_kind;
	struct fast_signature fast_signature;
	struct accel_kernel *accel_kernel;
	struct accel_program *accel_program;
	/* Bytecode contains OP_VFMAF32X4 and requires fused semantics. */
	bool has_fma_ops;
	uint32_t tmpvar_size;
	uint32_t bytecode_size;
	uint8_t *bytecode;
};

/*
 * Build a LIR function from a HIR function.
 */
bool
lir_build(
	struct hir_block *hir_func,
	struct lir_func **lir_func);

/*
 * Free a constructed LIR.
 */
void
lir_cleanup(
	struct lir_func *func);

/*
 * Get a file name.
 */
const char *
lir_get_file_name(void);

/*
 * Get an error line.
 */
int
lir_get_error_line(void);

/*
 * Get an error message.
 */
const char *
lir_get_error_message(void);

/*
 * Dump LIR.
 */
void
lir_dump(
	struct lir_func *func);

void
lir_set_optimize_level(int level);

void
lir_set_lineinfo(bool enable);

#endif
