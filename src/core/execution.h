/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Execution Helpers
 */

#ifndef NOCT_EXECUTION_H
#define NOCT_EXECUTION_H

#include <noct/c89compat.h>

/*
 * Some exotic compilers for x86 including Watcom utilize registers to
 * pass function arguments. However, our JIT-generated code for x86
 * uses the stack for function arguments. To bridge this gap, we use
 * the CDECL keyword in this module.
 */

NOCT_DLL
bool
CDECL
ex_make_string_with_hash(
	struct rt_env *env,
	struct rt_value *val,
	const char *data,
	size_t len,
	uint32_t hash);

NOCT_DLL
bool
CDECL
ex_make_empty_array(
	struct rt_env *env,
	struct rt_value *val);


NOCT_DLL
bool
CDECL
ex_make_empty_dict(
	struct rt_env *env,
	struct rt_value *val);

NOCT_DLL
bool
CDECL
ex_add_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_sub_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_mul_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_div_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_mod_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_and_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_or_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_xor_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_shl_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_shr_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_neg_helper(
	struct rt_env *rt,
	int dst,
	int src);

NOCT_DLL
bool
CDECL
ex_not_helper(
	struct rt_env *rt,
	int dst,
	int src);

NOCT_DLL
bool
CDECL
ex_lt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_lte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_eq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_neq_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_gte_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_gt_helper(
	struct rt_env *rt,
	int dst,
	int src1,
	int src2);

NOCT_DLL
bool
CDECL
ex_storearray_helper(
	struct rt_env *rt,
	int arr,
	int subscr,
	int val);

NOCT_DLL
bool
CDECL
ex_loadarray_helper(
	struct rt_env *rt,
	int dst,
	int arr,
	int subscr);

NOCT_DLL
bool
CDECL
ex_len_helper(
	struct rt_env *rt,
	int dst,
	int src);

NOCT_DLL
bool
CDECL
ex_getdictkeybyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr);

NOCT_DLL
bool
CDECL
ex_getdictvalbyindex_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	int subscr);

NOCT_DLL
bool
CDECL
ex_loadsymbol_helper(
	struct rt_env *rt,
	int dst,
	const char *symbol,
	uint32_t symbol_len,
	uint32_t symbol_hash);

NOCT_DLL
bool
CDECL
ex_storesymbol_helper(
	struct rt_env *rt,
	const char *symbol,
	uint32_t symbol_len,
	uint32_t symbol_hash,
	int src);

NOCT_DLL
bool
CDECL
ex_loaddot_helper(
	struct rt_env *rt,
	int dst,
	int dict,
	const char *field,
	uint32_t field_len,
	uint32_t field_hash);

NOCT_DLL
bool
CDECL
ex_storedot_helper(
	struct rt_env *rt,
	int dict,
	const char *field,
	uint32_t field_len,
	uint32_t field_hash,
	int src);

NOCT_DLL
bool
CDECL
ex_call_helper(
	struct rt_env *rt,
	int dst,
	int func,
	int arg_count,
	int *arg);

NOCT_DLL
bool
CDECL
ex_thiscall_helper(
	struct rt_env *rt,
	int dst,
	int obj,
	const char *name,
	uint32_t name_len,
	uint32_t name_hash,
	int arg_count,
	int *arg);

#endif
