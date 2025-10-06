/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Execution Helpers (runtime.c)
 */

#ifndef NOCT_EXECUTION_H
#define NOCT_EXECUTION_H

#include <noct/c89compat.h>

struct rt_string_imm {
	uint32_t len;
	uint32_t hash;
	const char *s;
};

bool
rt_assign_helper(
	struct rt_env *rt,
	int dst,
	int src);

bool
rt_add_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_sub_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_mul_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_div_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_mod_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_and_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_or_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_xor_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_shl_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_shr_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_neg_helper(
	struct rt_env *rt,
	int dst,
	int src);

bool
rt_not_helper(
	struct rt_env *rt,
	int dst,
	int src);

bool
rt_lt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_lte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_eq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_neq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_gte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_gt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

bool
rt_storearray_helper(
	struct rt_env *rt,
	int arr,
	int subscr,
	int val);

bool
rt_loadarray_helper(
	struct rt_env *rt,
	int dst,
	int arr,
	int subscr);

bool
rt_len_helper(
	struct rt_env *rt,
	int dst,
	int src);

bool
rt_getdictkeybyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr);

bool
rt_getdictvalbyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr);

bool
rt_loadsymbol_helper(
	struct rt_env *rt,
	int dst,
	struct rt_string_imm *symbol);

bool
rt_storesymbol_helper(
	struct rt_env *rt,
	struct rt_string_imm *symbol,
	int src);

bool
rt_loaddot_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	struct rt_string_imm *field);

bool
rt_storedot_helper(
	struct rt_env *rt,
	int dict,
	struct rt_string_imm *field,
	int src);

bool
rt_call_helper(
	struct rt_env *rt,
	int dst,
	int func,
	int arg_count,
	int *arg);

bool
rt_thiscall_helper(
	struct rt_env *rt,
	int dst,
	int obj,
	struct rt_string_imm *name,
	int arg_count,
	int *arg);

#endif
