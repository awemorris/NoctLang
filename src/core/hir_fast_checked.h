/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Checked HIR support for __fast functions.
 */

#ifndef NOCT_HIR_FAST_CHECKED_H
#define NOCT_HIR_FAST_CHECKED_H

#include <noct/noct.h>

struct hir_block;

bool
hir_fast_checked_module(
	struct hir_block *const *func_table,
	uint32_t func_count);

#endif
