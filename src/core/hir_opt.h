/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR Optimizer: private interface between hir.c and the optimizer
 * passes (hir_opt_abce.c, hir_opt_cse.c).  See docs/design/05-cse.md.
 */

#ifndef NOCT_HIR_OPT_H
#define NOCT_HIR_OPT_H

#include <noct/noct.h>
#include "hir.h"

/*
 * Pass entry points.  All are compiled only when NOCT_USE_OPTIMIZER
 * is defined; hir_optimize_func() in hir.c is the only caller.
 */
bool hir_opt_abce_func(struct hir_block *func_block);
bool hir_opt_simd_func(struct hir_block *func_block, bool simd_info);
bool hir_opt_cse_func(struct hir_block *func_block);
bool hir_opt_typed_func(struct hir_block *func_block);

/*
 * hir.c internals shared with the optimizer passes.
 */

/* Arena allocation (freed wholesale in hir_cleanup()). */
void *hir_malloc(size_t size);
char *hir_strdup(const char *s);

/* Set the out-of-memory error message. */
void hir_out_of_memory(void);

/* Register a local variable on the enclosing function block. */
bool hir_add_local(struct hir_block *cur_block, const char *symbol);

/* Allocate a fresh debug block id. */
int hir_next_block_id(void);

/* Current source file name (for debug output). */
extern char *hir_file_name;

#endif
