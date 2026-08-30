/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Private interfaces for HIR construction and optimizer passes.
 */

#ifndef NOCT_HIR_OPT_H
#define NOCT_HIR_OPT_H

#include <noct/noct.h>
#include "hir.h"

/*
 * Optimizer pass entry points.
 *  - All are compiled only when NOCT_USE_OPTIMIZER is defined.
 *  - hir_optimize_func() in hir.c is the only caller.
 */

bool
hir_opt_abce_func(
	struct hir_block *func_block);

bool
hir_opt_inline_func(
	struct hir_block *func_block);

bool
hir_opt_simd_func(
	struct hir_block *func_block,
	bool simd_info);

bool
hir_opt_unroll_func(
	struct hir_block *func_block);

bool
hir_opt_cse_func(
	struct hir_block *func_block);

bool
hir_opt_typed_func(
	struct hir_block *func_block);

/*
 * HIR arena and construction services shared by scope handling and passes.
 */
void *hir_malloc(size_t size);
char *hir_strdup(const char *s);
void hir_out_of_memory(void);
bool hir_add_local(struct hir_block *cur_block, const char *symbol);
int hir_next_block_id(void);
extern char *hir_file_name;

#endif
