/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Interpreter
 */

#ifndef NOCT_INTERPRETER_H
#define NOCT_INTERPRETER_H

#include <noct/c89compat.h>

#define ex_make_string_with_hash noct_ex_make_string_with_hash
#define ex_make_empty_array noct_ex_make_empty_array
#define ex_make_empty_dict noct_ex_make_empty_dict
#define ex_add_helper noct_ex_add_helper
#define ex_sub_helper noct_ex_sub_helper
#define ex_mul_helper noct_ex_mul_helper
#define ex_div_helper noct_ex_div_helper
#define ex_mod_helper noct_ex_mod_helper
#define ex_and_helper noct_ex_and_helper
#define ex_or_helper noct_ex_or_helper
#define ex_xor_helper noct_ex_xor_helper
#define ex_shl_helper noct_ex_shl_helper
#define ex_shr_helper noct_ex_shr_helper
#define ex_neg_helper noct_ex_neg_helper
#define ex_not_helper noct_ex_not_helper
#define ex_lt_helper noct_ex_lt_helper
#define ex_lte_helper noct_ex_lte_helper
#define ex_eq_helper noct_ex_eq_helper
#define ex_neq_helper noct_ex_neq_helper
#define ex_gte_helper noct_ex_gte_helper
#define ex_gt_helper noct_ex_gt_helper
#define ex_storearray_helper noct_ex_storearray_helper
#define ex_loadarray_helper noct_ex_loadarray_helper
#define ex_len_helper noct_ex_len_helper
#define ex_getdictkeybyindex_helper noct_ex_getdictkeybyindex_helper
#define ex_getdictvalbyindex_helper noct_ex_getdictvalbyindex_helper
#define ex_loadsymbol_helper noct_ex_loadsymbol_helper
#define ex_storesymbol_helper noct_ex_storesymbol_helper
#define ex_loaddot_helper noct_ex_loaddot_helper
#define ex_storedot_helper noct_ex_storedot_helper
#define ex_call_helper noct_ex_call_helper
#define ex_thiscall_helper noct_ex_thiscall_helper
#define ex_pbase_helper noct_ex_pbase_helper
#define ex_pcheck_helper noct_ex_pcheck_helper
#define ex_typeis_helper noct_ex_typeis_helper
#define ex_plen_helper noct_ex_plen_helper
#define ex_pload8u_helper noct_ex_pload8u_helper
#define ex_pstore8_helper noct_ex_pstore8_helper
#define ex_checktype_helper noct_ex_checktype_helper
#define ex_pload8s_helper noct_ex_pload8s_helper
#define ex_pload16u_helper noct_ex_pload16u_helper
#define ex_pload16s_helper noct_ex_pload16s_helper
#define ex_pload32_helper noct_ex_pload32_helper
#define ex_pload64_helper noct_ex_pload64_helper
#define ex_pstore16_helper noct_ex_pstore16_helper
#define ex_pstore32_helper noct_ex_pstore32_helper
#define ex_pstore64_helper noct_ex_pstore64_helper
#define ex_iadd_helper noct_ex_iadd_helper
#define ex_isub_helper noct_ex_isub_helper
#define ex_imul_helper noct_ex_imul_helper
#define ex_idiv_helper noct_ex_idiv_helper
#define ex_imod_helper noct_ex_imod_helper
#define ex_iand_helper noct_ex_iand_helper
#define ex_ior_helper noct_ex_ior_helper
#define ex_ixor_helper noct_ex_ixor_helper
#define ex_ishl_helper noct_ex_ishl_helper
#define ex_ishr_helper noct_ex_ishr_helper
#define ex_ilt_helper noct_ex_ilt_helper
#define ex_ilte_helper noct_ex_ilte_helper
#define ex_igt_helper noct_ex_igt_helper
#define ex_igte_helper noct_ex_igte_helper
#define ex_fadd_helper noct_ex_fadd_helper
#define ex_fsub_helper noct_ex_fsub_helper
#define ex_fmul_helper noct_ex_fmul_helper
#define ex_fdiv_helper noct_ex_fdiv_helper
#define ex_flt_helper noct_ex_flt_helper
#define ex_flte_helper noct_ex_flte_helper
#define ex_fgt_helper noct_ex_fgt_helper
#define ex_fgte_helper noct_ex_fgte_helper
#define ex_vloadi32x4_helper noct_ex_vloadi32x4_helper
#define ex_vstorei32x4_helper noct_ex_vstorei32x4_helper
#define ex_vsplati32_helper noct_ex_vsplati32_helper
#define ex_vgetlanei32_helper noct_ex_vgetlanei32_helper
#define ex_vmov128_helper noct_ex_vmov128_helper
#define ex_vaddi32x4_helper noct_ex_vaddi32x4_helper
#define ex_vsubi32x4_helper noct_ex_vsubi32x4_helper
#define ex_vmuli32x4_helper noct_ex_vmuli32x4_helper
#define ex_vand128_helper noct_ex_vand128_helper
#define ex_vor128_helper noct_ex_vor128_helper
#define ex_vxor128_helper noct_ex_vxor128_helper
#define ex_vshli32x4_helper noct_ex_vshli32x4_helper
#define ex_vshri32x4_helper noct_ex_vshri32x4_helper
#define ex_vloadf32x4_helper noct_ex_vloadf32x4_helper
#define ex_vstoref32x4_helper noct_ex_vstoref32x4_helper
#define ex_vsplatf32_helper noct_ex_vsplatf32_helper
#define ex_vgetlanef32_helper noct_ex_vgetlanef32_helper
#define ex_vaddf32x4_helper noct_ex_vaddf32x4_helper
#define ex_vsubf32x4_helper noct_ex_vsubf32x4_helper
#define ex_vmulf32x4_helper noct_ex_vmulf32x4_helper
#define ex_vdivf32x4_helper noct_ex_vdivf32x4_helper
#define ex_ploadf32_helper noct_ex_ploadf32_helper
#define ex_pstoref32_helper noct_ex_pstoref32_helper

/* Visit bytecode. */
bool
rt_visit_bytecode(struct rt_env *rt, struct rt_func *func);

#endif

