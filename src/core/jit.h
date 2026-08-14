/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * JIT (mips32): Just-In-Time native code generation
 */

#ifndef NOCT_JIT_H
#define NOCT_JIT_H

#include <noct/c89compat.h>
#include "bytecode.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#define ex_safepoint_helper noct_ex_safepoint_helper
#define ex_pbase_helper noct_ex_pbase_helper
#define ex_pcheck_helper noct_ex_pcheck_helper
#define ex_typeis_helper noct_ex_typeis_helper
#define ex_plen_helper noct_ex_plen_helper
#define ex_pload8u_helper noct_ex_pload8u_helper
#define ex_pstore8_helper noct_ex_pstore8_helper
#define ex_checktype_helper noct_ex_checktype_helper
#define ex_condition_helper noct_ex_condition_helper
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
#define ex_vori32x4i_helper noct_ex_vori32x4i_helper
#define ex_vfmaf32x4_helper noct_ex_vfmaf32x4_helper
#define ex_vcmpi32x4_helper noct_ex_vcmpi32x4_helper
#define ex_vcmpf32x4_helper noct_ex_vcmpf32x4_helper
#define ex_vselect128_helper noct_ex_vselect128_helper
#define ex_vmaskstorei32x4_helper noct_ex_vmaskstorei32x4_helper
#define ex_vinductf32x4_helper noct_ex_vinductf32x4_helper
#define ex_vgatheri32x4_checked_helper noct_ex_vgatheri32x4_checked_helper
#define ex_ploadf32_helper noct_ex_ploadf32_helper
#define ex_pstoref32_helper noct_ex_pstoref32_helper

/* Generate a JIT-compiled code for a function. */
bool
jit_build(
	struct rt_env *env,
	struct rt_func *func);

/* Commit written code. */
void
jit_commit(
	struct rt_env *env);

/* Free all JIT-compiled code. */
void
jit_free(
	struct rt_env *env);

/*
 * If JIT is enabled.
 */
#if defined(NOCT_USE_JIT)

/* Error message. */
#define BROKEN_BYTECODE		N_TR("Broken bytecode.")

/* Code size. */
#if defined(NOCT_JIT_CODE_MAX)
#define JIT_CODE_MAX		NOCT_JIT_CODE_MAX
#elif !defined(NOCT_TARGET_DOS4G) && !defined(NOCT_TARGET_PC98BE)
#define JIT_CODE_MAX		(16 * 1024 * 1024)
#else
#define JIT_CODE_MAX		(1 * 1024 * 1024)
#endif

/*
 * Return the per-VM JIT reservation, clamped to the target's compiled limit.
 * A zero value keeps source compatibility with callers that zero-initialize
 * NoctConfig instead of starting with noct_set_default_config().
 */
static INLINE size_t
jit_get_code_size(
	struct rt_env *env)
{
	size_t size = env->vm->config.jit_code_size;

	if (size == 0 || size > JIT_CODE_MAX)
		size = JIT_CODE_MAX;
	return size;
}

/*
 * One VM-local JIT allocation.  [base, committed) is RX and immutable;
 * [current, end) remains RW.  Commit rounds current up to a page boundary,
 * deliberately wasting the tail so a page is never switched back to RW.
 */
struct jit_slab {
	uint8_t *base;
	uint8_t *current;
	uint8_t *committed;
	uint8_t *end;
	size_t size;
	struct jit_slab *next;
};

bool jit_slab_acquire(struct rt_env *env, struct jit_slab **slab,
		      void **code_top, void **code_end);
bool jit_slab_reserve(struct rt_env *env, size_t estimated_size);
void jit_slab_finish(struct rt_env *env, struct jit_slab *slab,
		     void *code_end);
void jit_slab_abandon(struct rt_env *env, struct jit_slab *slab);
void jit_slab_clear_overflow(struct rt_env *env);
void jit_slab_commit_all(struct rt_env *env);
void jit_slab_free_all(struct rt_env *env);

/* Per-function dynamic JIT table bounds used by architecture backends. */
#define PC_ENTRY_MAX		(ctx->pc_entry_capacity)
#define BRANCH_PATCH_MAX	(ctx->branch_patch_capacity)

/* Runtime SIMD capabilities, detected independently by each JIT backend. */
#define JIT_SIMD_CAP_SSE2	(1u << 0)
#define JIT_SIMD_CAP_SSE3	(1u << 1)
#define JIT_SIMD_CAP_SSE41	(1u << 2)
#define JIT_SIMD_CAP_NEON	(1u << 3)
#define JIT_SIMD_CAP_ALTIVEC	(1u << 4)
#define JIT_SIMD_CAP_FMAF32X4	(1u << 5)
#define JIT_SIMD_CAP_AVX		(1u << 6)

/* Test/user ceiling.  It can remove detected features, never add them. */
static INLINE uint32_t
jit_apply_simd_max(uint32_t detected)
{
	const char *max = getenv("NOCT_JIT_SIMD_MAX");

	if (max == NULL || max[0] == '\0')
		return detected;
	if (strcmp(max, "scalar") == 0)
		return 0;
	if (strcmp(max, "sse2") == 0)
		return detected & JIT_SIMD_CAP_SSE2;
	if (strcmp(max, "sse3") == 0)
		return detected & (JIT_SIMD_CAP_SSE2 | JIT_SIMD_CAP_SSE3);
	if (strcmp(max, "sse41") == 0)
		return detected & (JIT_SIMD_CAP_SSE2 | JIT_SIMD_CAP_SSE3 |
				   JIT_SIMD_CAP_SSE41);
	if (strcmp(max, "avx") == 0)
		return detected & (JIT_SIMD_CAP_SSE2 | JIT_SIMD_CAP_SSE3 |
				   JIT_SIMD_CAP_SSE41 | JIT_SIMD_CAP_AVX);
	if (strcmp(max, "neon") == 0)
		return detected & (JIT_SIMD_CAP_NEON |
				   JIT_SIMD_CAP_FMAF32X4);
	if (strcmp(max, "altivec") == 0)
		return detected & JIT_SIMD_CAP_ALTIVEC;
	if (strcmp(max, "fma") == 0)
		return detected;
	return detected;
}

/*
 * JIT codegen context
 */
struct jit_context {
	struct rt_env *env;
	struct rt_func *func;
	uint32_t simd_caps;
	bool has_vector_ops;
	int vector_kind;	/* 0 unknown, 1 integer, 2 float region */
	bool vector_hint_active;
	int vector_hint_index_tmp;
	int vector_hint_stop_tmp;
	int vector_hint_remaining_tmp;
	int vector_hint_lanes;
	int vector_hint_flags;
	int vector_vreg_limit;
	int vector_base_tmp[2];
	uint32_t vector_base_last_lpc[2];
	int vector_imm_value;
	int vector_imm_shift;
	int vector_imm_reg;
	bool packed_loop_hint_active;
	int packed_loop_index_tmp;
	int packed_loop_stop_tmp;
	int packed_loop_remaining_tmp;
	int packed_loop_lanes;
	int packed_loop_flags;
	int packed_loop_base_tmp[3];
	int packed_loop_base_scale[3];
	uint16_t packed_loop_index_alias[32];
	int32_t packed_loop_index_alias_disp[32];
	int packed_loop_index_alias_count;
	uint16_t packed_loop_base_alias_tmp[64];
	uint16_t packed_loop_base_alias_root[64];
	int packed_loop_base_alias_count;
	int *gpr_tmp_reg;
	uint8_t *gpr_tmp_dirty;
	uint8_t *gpr_remat_valid;
	int32_t *gpr_remat_value;
	int8_t *tmp_fixed_type;
	uint8_t *tmp_frame_tag_known;
	uint8_t *tmp_compiler_temp;
	int32_t *gpr_range_min;
	int32_t *gpr_range_max;
	uint8_t *gpr_range_valid;
	int32_t *packed_index_disp;
	int32_t *packed_const_value;
	int32_t *packed_access_disp;
	uint8_t *packed_index_valid;
	uint8_t *packed_const_valid;
	uint8_t *packed_access_valid;
	uint8_t *packed_elide_lpc;
	uint32_t *packed_def_lpc;
	uint32_t *packed_lpc_use_count;
	uint32_t *packed_lpc_address_use_count;
	int gpr_reg_tmp[6];
	int gpr_next_victim;
	int gpr_reg_limit;
	bool gpr_cache_active;
	int gpr_load_tmp[3];
	int gpr_load_opcode[3];
	int gpr_load_disp[3];
	unsigned gpr_hits;
	unsigned gpr_misses;
	unsigned gpr_spills;
	unsigned gpr_dead_drops;
	unsigned gpr_proven_divisions;
	const char *packed_loop_reject_reason;

	/* Top of the mapped code area. */
	void *code_top;

	/* End of the mapped code area. */
	void *code_end;

	/* Current code position in the mapped code area. */
	void *code;

	/* The current function did not fit and may be retried on a fresh slab. */
	bool code_overflow;

	/* Exception handler address of the current function. */
	void *exception_code;

	/* Current PC of LIR. */
	uint32_t lpc;

	/* Mapping table from LIR-PC to Native-PC. */
	struct pc_entry {
		/* LIR-PC */
		uint32_t lpc;

		/* Native-PC */
		uint32_t *code;
	} *pc_entry;
	uint32_t pc_entry_count;
	uint32_t pc_entry_capacity;

	/* Delayed branch patching table. */
	struct branch_patch {
		/* Native code address. */
		uint32_t *code;

		/* LIR-PC */
		uint32_t lpc;

		/* Branch type. */
		int type;
	} *branch_patch;
	int branch_patch_count;
	uint32_t branch_patch_capacity;
};

#if defined(NOCT_JIT_IMPLEMENTATION) && \
	(defined(NOCT_ARCH_X86_64) || defined(NOCT_ARCH_ARM64))
/*
 * Target-neutral scalar Packed-loop scanner.
 *
 * A backend may reserve registers only after this scanner has proved that the
 * region is call-free, has the canonical one-step latch, and addresses no
 * more than three Packed roots through aliases of the induction variable.
 * The hint remains optional: a rejected region is emitted by the ordinary
 * memory-canonical visitors.
 */
static INLINE uint16_t
jit_ploop_read_u16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static INLINE uint32_t
jit_ploop_read_u32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static INLINE bool
jit_ploop_reject(struct jit_context *ctx, const char *reason)
{
	ctx->packed_loop_reject_reason = reason;
	return false;
}

static INLINE bool
jit_ploop_add_base(struct jit_context *ctx, uint16_t base, int scale)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (ctx->packed_loop_base_tmp[i] == (int)base)
			return ctx->packed_loop_base_scale[i] == scale ? true :
				jit_ploop_reject(ctx, "mixed-base-scale");
		if (ctx->packed_loop_base_tmp[i] < 0) {
			ctx->packed_loop_base_tmp[i] = (int)base;
			ctx->packed_loop_base_scale[i] = scale;
			return true;
		}
	}
	return jit_ploop_reject(ctx, "too-many-bases");
}

static INLINE bool
jit_ploop_is_index_alias(struct jit_context *ctx, int tmp)
{
	int i;

	for (i = 0; i < ctx->packed_loop_index_alias_count; i++) {
		if (ctx->packed_loop_index_alias[i] == (uint16_t)tmp)
			return true;
	}
	return false;
}

static INLINE bool
jit_ploop_index_alias_disp(struct jit_context *ctx, int tmp, int32_t *disp)
{
	int i;

	for (i = 0; i < ctx->packed_loop_index_alias_count; i++) {
		if (ctx->packed_loop_index_alias[i] == (uint16_t)tmp) {
			*disp = ctx->packed_loop_index_alias_disp[i];
			return true;
		}
	}
	return false;
}

static INLINE void
jit_ploop_remove_index_alias(struct jit_context *ctx, int tmp)
{
	int i;

	for (i = 1; i < ctx->packed_loop_index_alias_count; i++) {
		if (ctx->packed_loop_index_alias[i] == (uint16_t)tmp) {
			ctx->packed_loop_index_alias[i] =
				ctx->packed_loop_index_alias[
					--ctx->packed_loop_index_alias_count];
			ctx->packed_loop_index_alias_disp[i] =
				ctx->packed_loop_index_alias_disp[
					ctx->packed_loop_index_alias_count];
			return;
		}
	}
}

static INLINE bool
jit_ploop_add_index_alias_disp(struct jit_context *ctx, int tmp,
			       int32_t disp)
{
	int i;

	for (i = 0; i < ctx->packed_loop_index_alias_count; i++) {
		if (ctx->packed_loop_index_alias[i] == (uint16_t)tmp) {
			ctx->packed_loop_index_alias_disp[i] = disp;
			return true;
		}
	}
	if (ctx->packed_loop_index_alias_count >=
	    (int)(sizeof(ctx->packed_loop_index_alias) /
		  sizeof(ctx->packed_loop_index_alias[0])))
		return jit_ploop_reject(ctx, "index-alias-overflow");
	ctx->packed_loop_index_alias[ctx->packed_loop_index_alias_count] =
		(uint16_t)tmp;
	ctx->packed_loop_index_alias_disp[ctx->packed_loop_index_alias_count] =
		disp;
	ctx->packed_loop_index_alias_count++;
	return true;
}

static INLINE bool
jit_ploop_add_index_alias(struct jit_context *ctx, int tmp)
{
	return jit_ploop_add_index_alias_disp(ctx, tmp, 0);
}

static INLINE int
jit_ploop_resolve_base(struct jit_context *ctx, int tmp)
{
	int i;

	for (i = ctx->packed_loop_base_alias_count - 1; i >= 0; i--) {
		if (ctx->packed_loop_base_alias_tmp[i] == (uint16_t)tmp)
			return ctx->packed_loop_base_alias_root[i];
	}
	return tmp;
}

static INLINE void
jit_ploop_remove_base_alias(struct jit_context *ctx, int tmp)
{
	int i;

	for (i = 0; i < ctx->packed_loop_base_alias_count; i++) {
		if (ctx->packed_loop_base_alias_tmp[i] == (uint16_t)tmp) {
			ctx->packed_loop_base_alias_tmp[i] =
				ctx->packed_loop_base_alias_tmp[
					--ctx->packed_loop_base_alias_count];
			ctx->packed_loop_base_alias_root[i] =
				ctx->packed_loop_base_alias_root[
					ctx->packed_loop_base_alias_count];
			return;
		}
	}
}

static INLINE bool
jit_ploop_set_base_alias(struct jit_context *ctx, int dst, int src)
{
	int root;

	root = jit_ploop_resolve_base(ctx, src);
	jit_ploop_remove_base_alias(ctx, dst);
	if (ctx->packed_loop_base_alias_count >=
	    (int)(sizeof(ctx->packed_loop_base_alias_tmp) /
		  sizeof(ctx->packed_loop_base_alias_tmp[0])))
		return jit_ploop_reject(ctx, "base-alias-overflow");
	ctx->packed_loop_base_alias_tmp[ctx->packed_loop_base_alias_count] =
		(uint16_t)dst;
	ctx->packed_loop_base_alias_root[ctx->packed_loop_base_alias_count] =
		(uint16_t)root;
	ctx->packed_loop_base_alias_count++;
	return true;
}

/* gpr_tmp_dirty/range_valid are scratch bitsets during the grammar scan.
 * They are reset by the backend before register allocation starts. */
static INLINE void
jit_ploop_note_use(struct jit_context *ctx, int tmp)
{
	if (tmp >= 0 && (uint32_t)tmp < ctx->func->tmpvar_size &&
	    ctx->gpr_tmp_dirty[tmp] == 0)
		ctx->gpr_range_valid[tmp] = 1;
}

static INLINE void
jit_ploop_note_def(struct jit_context *ctx, int tmp)
{
	if (tmp >= 0 && (uint32_t)tmp < ctx->func->tmpvar_size)
		ctx->gpr_tmp_dirty[tmp] = 1;
}

static INLINE bool
jit_ploop_has_loop_carried_scalar(struct jit_context *ctx)
{
	uint32_t i;

	for (i = 0; i < ctx->func->tmpvar_size; i++) {
		if (ctx->gpr_tmp_dirty[i] != 0 &&
		    ctx->gpr_range_valid[i] != 0)
			return true;
	}
	return false;
}

static INLINE void
jit_ploop_count_use(struct jit_context *ctx, int tmp, bool address_only)
{
	uint32_t def;

	if (tmp < 0 || (uint32_t)tmp >= ctx->func->tmpvar_size)
		return;
	def = ctx->packed_def_lpc[tmp];
	if (def == UINT32_MAX || def >= ctx->func->bytecode_size)
		return;
	ctx->packed_lpc_use_count[def]++;
	if (address_only)
		ctx->packed_lpc_address_use_count[def]++;
}

static bool
jit_scan_packed_loop(struct jit_context *ctx, bool reject_loop_carried)
{
	uint32_t p;
	uint32_t body_lpc;
	uint32_t size;
	uint16_t base;
	uint16_t ofs;
	uint16_t dst;
	uint16_t src1;
	uint16_t src2;
	uint16_t value;
	uint8_t op;
	int scale;
	int inc_count;
	uint32_t i;
	bool address_expr;

	ctx->packed_loop_reject_reason = "none";
	if (ctx->gpr_tmp_dirty == NULL || ctx->gpr_range_valid == NULL)
		return jit_ploop_reject(ctx, "analysis-storage");
	for (i = 0; i < ctx->func->tmpvar_size; i++) {
		ctx->gpr_tmp_dirty[i] = 0;
		ctx->gpr_range_valid[i] = 0;
	}
	ctx->packed_loop_base_tmp[0] = -1;
	ctx->packed_loop_base_tmp[1] = -1;
	ctx->packed_loop_base_tmp[2] = -1;
	ctx->packed_loop_base_scale[0] = 0;
	ctx->packed_loop_base_scale[1] = 0;
	ctx->packed_loop_base_scale[2] = 0;
	ctx->packed_loop_index_alias_count = 1;
	ctx->packed_loop_index_alias[0] =
		(uint16_t)ctx->packed_loop_index_tmp;
	ctx->packed_loop_index_alias_disp[0] = 0;
	ctx->packed_loop_base_alias_count = 0;
	memset(ctx->packed_index_valid, 0, ctx->func->tmpvar_size);
	memset(ctx->packed_const_valid, 0, ctx->func->tmpvar_size);
	memset(ctx->packed_access_valid, 0, ctx->func->bytecode_size);
	memset(ctx->packed_elide_lpc, 0, ctx->func->bytecode_size);
	memset(ctx->packed_lpc_use_count, 0,
	       ctx->func->bytecode_size *
	       sizeof(*ctx->packed_lpc_use_count));
	memset(ctx->packed_lpc_address_use_count, 0,
	       ctx->func->bytecode_size *
	       sizeof(*ctx->packed_lpc_address_use_count));
	for (i = 0; i < ctx->func->tmpvar_size; i++) {
		ctx->packed_def_lpc[i] = UINT32_MAX;
	}
	ctx->packed_index_valid[ctx->packed_loop_index_tmp] = 1;
	ctx->packed_index_disp[ctx->packed_loop_index_tmp] = 0;
	body_lpc = ctx->lpc;
	p = body_lpc;
	inc_count = 0;
	while (p < ctx->func->bytecode_size) {
		op = ctx->func->bytecode[p];
		if (getenv("NOCT_JIT_REGCACHE_SCAN_DEBUG") != NULL)
			fprintf(stderr,
				"noct-jit-regcache-scan: lpc=%u op=%u\n",
				(unsigned)p, (unsigned)op);
		size = 0;
		base = 0xffffu;
		scale = 0;
		switch (op) {
		case OP_LINEINFO:
			if (p + 5 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 5;
			break;
		case OP_ASSIGN:
			if (p + 5 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 5;
			dst = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			src1 = jit_ploop_read_u16(&ctx->func->bytecode[p + 3]);
			jit_ploop_note_use(ctx, src1);
			if (dst == (uint16_t)ctx->packed_loop_index_tmp ||
			    dst == (uint16_t)ctx->packed_loop_remaining_tmp)
				return jit_ploop_reject(ctx, "index-escape");
			if (ctx->packed_index_valid[src1]) {
				ctx->packed_index_valid[dst] = 1;
				ctx->packed_index_disp[dst] =
					ctx->packed_index_disp[src1];
				if (!jit_ploop_add_index_alias_disp(ctx, dst,
						ctx->packed_index_disp[dst]))
					return false;
				ctx->packed_elide_lpc[p] = 1;
				jit_ploop_count_use(ctx, src1, true);
			} else {
				jit_ploop_count_use(ctx, src1, false);
				ctx->packed_index_valid[dst] = 0;
				jit_ploop_remove_index_alias(ctx, dst);
			}
			ctx->packed_const_valid[dst] =
				ctx->packed_const_valid[src1];
			if (ctx->packed_const_valid[src1])
				ctx->packed_const_value[dst] =
					ctx->packed_const_value[src1];
			if (!jit_ploop_set_base_alias(ctx, dst, src1))
				return false;
			ctx->packed_def_lpc[dst] = p;
			jit_ploop_note_def(ctx, dst);
			break;
		case OP_ICONST:
			if (p + 7 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 7;
			dst = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			ctx->packed_def_lpc[dst] = p;
			if (dst == (uint16_t)ctx->packed_loop_index_tmp ||
			    dst == (uint16_t)ctx->packed_loop_remaining_tmp)
				return jit_ploop_reject(ctx, "index-escape");
			jit_ploop_remove_index_alias(ctx, dst);
			ctx->packed_index_valid[dst] = 0;
			ctx->packed_const_valid[dst] = 1;
			ctx->packed_const_value[dst] = (int32_t)
				jit_ploop_read_u32(&ctx->func->bytecode[p + 3]);
			jit_ploop_remove_base_alias(ctx, dst);
			jit_ploop_note_def(ctx, dst);
			break;
		case OP_PLOAD8U:
		case OP_PLOAD8S:
		case OP_PLOAD16U:
		case OP_PLOAD16S:
		case OP_PLOAD32:
			if (p + 7 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 7;
			base = (uint16_t)jit_ploop_resolve_base(ctx,
				jit_ploop_read_u16(&ctx->func->bytecode[p + 3]));
			ofs = jit_ploop_read_u16(&ctx->func->bytecode[p + 5]);
			if (!ctx->packed_index_valid[ofs])
				return jit_ploop_reject(ctx, "index-escape");
			ctx->packed_access_valid[p] = 1;
			ctx->packed_access_disp[p] = ctx->packed_index_disp[ofs];
			jit_ploop_count_use(ctx, ofs, true);
			dst = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			jit_ploop_remove_index_alias(ctx, dst);
			ctx->packed_index_valid[dst] = 0;
			ctx->packed_const_valid[dst] = 0;
			jit_ploop_remove_base_alias(ctx, dst);
			ctx->packed_def_lpc[dst] = p;
			jit_ploop_note_def(ctx, dst);
			scale = op == OP_PLOAD32 ? 4 :
				op == OP_PLOAD16U || op == OP_PLOAD16S ? 2 : 1;
			break;
		case OP_PSTORE8:
		case OP_PSTORE16:
		case OP_PSTORE32:
			if (p + 7 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 7;
			base = (uint16_t)jit_ploop_resolve_base(ctx,
				jit_ploop_read_u16(&ctx->func->bytecode[p + 1]));
			ofs = jit_ploop_read_u16(&ctx->func->bytecode[p + 3]);
			if (!ctx->packed_index_valid[ofs])
				return jit_ploop_reject(ctx, "index-escape");
			ctx->packed_access_valid[p] = 1;
			ctx->packed_access_disp[p] = ctx->packed_index_disp[ofs];
			jit_ploop_count_use(ctx, ofs, true);
			src1 = jit_ploop_read_u16(&ctx->func->bytecode[p + 5]);
			if (jit_ploop_is_index_alias(ctx, src1))
				return jit_ploop_reject(ctx, "index-escape");
			jit_ploop_note_use(ctx, src1);
			jit_ploop_count_use(ctx, src1, false);
			scale = op == OP_PSTORE32 ? 4 :
				op == OP_PSTORE16 ? 2 : 1;
			break;
		case OP_IADD:
		case OP_ISUB:
		case OP_IMUL:
		case OP_IDIV:
		case OP_IMOD:
		case OP_IAND:
		case OP_IOR:
		case OP_IXOR:
		case OP_ILT:
		case OP_ILTE:
		case OP_IGT:
		case OP_IGTE:
		case OP_IDIV_CHECKED:
		case OP_IMOD_CHECKED:
			if (p + 7 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 7;
			dst = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			src1 = jit_ploop_read_u16(&ctx->func->bytecode[p + 3]);
			src2 = jit_ploop_read_u16(&ctx->func->bytecode[p + 5]);
			address_expr = false;
			jit_ploop_note_use(ctx, src1);
			jit_ploop_note_use(ctx, src2);
			if (dst == (uint16_t)ctx->packed_loop_index_tmp ||
			    dst == (uint16_t)ctx->packed_loop_remaining_tmp)
				return jit_ploop_reject(ctx, "index-escape");
			ctx->packed_index_valid[dst] = 0;
			ctx->packed_const_valid[dst] = 0;
			if ((op == OP_IADD || op == OP_ISUB) &&
			    ctx->packed_index_valid[src1] &&
			    ctx->packed_const_valid[src2]) {
				int64_t d = ctx->packed_index_disp[src1];
				d += op == OP_IADD ?
					ctx->packed_const_value[src2] :
					-ctx->packed_const_value[src2];
				if (d < INT32_MIN || d > INT32_MAX)
					return jit_ploop_reject(ctx,
						"index-displacement-overflow");
				ctx->packed_index_valid[dst] = 1;
				ctx->packed_index_disp[dst] = (int32_t)d;
				if (!jit_ploop_add_index_alias_disp(ctx, dst,
						(int32_t)d))
					return false;
				ctx->packed_elide_lpc[p] = 1;
				address_expr = true;
			} else if (op == OP_IADD &&
				   ctx->packed_const_valid[src1] &&
				   ctx->packed_index_valid[src2]) {
				int64_t d = (int64_t)ctx->packed_const_value[src1] +
					ctx->packed_index_disp[src2];
				if (d < INT32_MIN || d > INT32_MAX)
					return jit_ploop_reject(ctx,
						"index-displacement-overflow");
				ctx->packed_index_valid[dst] = 1;
				ctx->packed_index_disp[dst] = (int32_t)d;
				if (!jit_ploop_add_index_alias_disp(ctx, dst,
						(int32_t)d))
					return false;
				ctx->packed_elide_lpc[p] = 1;
				address_expr = true;
			} else if (ctx->packed_index_valid[src1] ||
				   ctx->packed_index_valid[src2]) {
				return jit_ploop_reject(ctx, "index-escape");
			}
			jit_ploop_remove_index_alias(ctx, dst);
			if (ctx->packed_index_valid[dst] &&
			    !jit_ploop_add_index_alias_disp(ctx, dst,
					ctx->packed_index_disp[dst]))
				return false;
			jit_ploop_remove_base_alias(ctx, dst);
			jit_ploop_count_use(ctx, src1, address_expr);
			jit_ploop_count_use(ctx, src2, address_expr);
			ctx->packed_def_lpc[dst] = p;
			jit_ploop_note_def(ctx, dst);
			break;
		case OP_ISHL:
		case OP_ISHR:
			if (p + 6 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			size = 6;
			dst = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			src1 = jit_ploop_read_u16(&ctx->func->bytecode[p + 3]);
			jit_ploop_note_use(ctx, src1);
			if (dst == (uint16_t)ctx->packed_loop_index_tmp ||
			    dst == (uint16_t)ctx->packed_loop_remaining_tmp ||
			    jit_ploop_is_index_alias(ctx, src1))
				return jit_ploop_reject(ctx, "index-escape");
			jit_ploop_remove_index_alias(ctx, dst);
			jit_ploop_remove_base_alias(ctx, dst);
			jit_ploop_count_use(ctx, src1, false);
			ctx->packed_def_lpc[dst] = p;
			jit_ploop_note_def(ctx, dst);
			break;
		case OP_INC:
			{
				int factor = (ctx->packed_loop_flags &
					PLOOP_UNROLL4) != 0 ? 4 : 1;
			if (p + 4 > ctx->func->bytecode_size ||
			    jit_ploop_read_u16(&ctx->func->bytecode[p + 1]) !=
				(uint16_t)ctx->packed_loop_index_tmp ||
			    ctx->func->bytecode[p + 3] != factor)
				return jit_ploop_reject(ctx, "wrong-latch");
			inc_count++;
			size = 4;
			break;
			}
		case OP_SUBJNZ:
			if (p + 8 > ctx->func->bytecode_size)
				return jit_ploop_reject(ctx, "malformed-region");
			value = jit_ploop_read_u16(&ctx->func->bytecode[p + 1]);
			if (value != (uint16_t)ctx->packed_loop_remaining_tmp ||
			    ctx->func->bytecode[p + 3] !=
				((ctx->packed_loop_flags & PLOOP_UNROLL4) != 0 ?
				 4 : 1) ||
			    jit_ploop_read_u32(&ctx->func->bytecode[p + 4]) !=
				body_lpc || inc_count != 1 ||
			    ctx->packed_loop_base_tmp[0] < 0)
				return jit_ploop_reject(ctx, "wrong-latch");
			if (reject_loop_carried &&
			    jit_ploop_has_loop_carried_scalar(ctx))
				return jit_ploop_reject(ctx,
					"loop-carried-scalar");
			for (i = body_lpc; i <= p; i++) {
				if (ctx->packed_lpc_use_count[i] != 0 &&
				    ctx->packed_lpc_use_count[i] ==
					ctx->packed_lpc_address_use_count[i] &&
				    ctx->func->bytecode[i] == OP_ICONST)
					ctx->packed_elide_lpc[i] = 1;
			}
			if (getenv("NOCT_JIT_REGCACHE_SCAN_DEBUG") != NULL) {
				unsigned accesses = 0;
				unsigned elided = 0;
				uint32_t q;
				for (q = body_lpc; q <= p; q++) {
					if (ctx->packed_access_valid[q]) accesses++;
					if (ctx->packed_elide_lpc[q]) elided++;
				}
				fprintf(stderr,
					"noct-jit-regcache-scan: factor=%d accesses=%u elided=%u\n",
					(ctx->packed_loop_flags & PLOOP_UNROLL4) != 0 ? 4 : 1,
					accesses, elided);
			}
			return true;
		default:
			return jit_ploop_reject(ctx, "unsupported-opcode");
		}
		if (p + size > ctx->func->bytecode_size)
			return jit_ploop_reject(ctx, "malformed-region");
		if (base != 0xffffu &&
		    !jit_ploop_add_base(ctx, base, scale))
			return false;
		p += size;
	}
	return jit_ploop_reject(ctx, "malformed-region");
}

static INLINE bool
jit_ploop_current_access_disp(struct jit_context *ctx, int32_t *disp)
{
	uint32_t p;

	if (!ctx->packed_loop_hint_active || ctx->lpc < 7 ||
	    ctx->packed_access_valid == NULL)
		return false;
	p = ctx->lpc - 7;
	if (p >= ctx->func->bytecode_size || !ctx->packed_access_valid[p])
		return false;
	*disp = ctx->packed_access_disp[p];
	return true;
}

static INLINE bool
jit_ploop_current_elided(struct jit_context *ctx, uint32_t size)
{
	uint32_t p;

	if (!ctx->packed_loop_hint_active || ctx->lpc < size ||
	    ctx->packed_elide_lpc == NULL)
		return false;
	p = ctx->lpc - size;
	return p < ctx->func->bytecode_size && ctx->packed_elide_lpc[p] != 0;
}

/* Return the next bytecode position that reads tmp, stopping at its next
 * definition.  Accepted PLOOP regions have a small, closed opcode set, so a
 * linear walk is simpler and deterministic; with four/six physical GPRs it
 * is also cheaper than maintaining a general CFG liveness structure. */
static INLINE uint32_t
jit_ploop_next_use_lpc(struct jit_context *ctx, int tmp, uint32_t from)
{
	const uint8_t *bc = ctx->func->bytecode;
	uint32_t p = from;
	uint16_t a, b, c;
	uint8_t op;

	while (p < ctx->func->bytecode_size) {
		op = bc[p];
		switch (op) {
		case OP_LINEINFO: p += 5; break;
		case OP_ASSIGN:
			a = jit_ploop_read_u16(bc + p + 1);
			b = jit_ploop_read_u16(bc + p + 3);
			if (b == (uint16_t)tmp) return p;
			if (a == (uint16_t)tmp) return UINT32_MAX;
			p += 5; break;
		case OP_ICONST:
			a = jit_ploop_read_u16(bc + p + 1);
			if (a == (uint16_t)tmp) return UINT32_MAX;
			p += 7; break;
		case OP_PLOAD8U: case OP_PLOAD8S:
		case OP_PLOAD16U: case OP_PLOAD16S: case OP_PLOAD32:
			a = jit_ploop_read_u16(bc + p + 1);
			b = jit_ploop_read_u16(bc + p + 3);
			c = jit_ploop_read_u16(bc + p + 5);
			if (b == (uint16_t)tmp || c == (uint16_t)tmp) return p;
			if (a == (uint16_t)tmp) return UINT32_MAX;
			p += 7; break;
		case OP_PSTORE8: case OP_PSTORE16: case OP_PSTORE32:
			a = jit_ploop_read_u16(bc + p + 1);
			b = jit_ploop_read_u16(bc + p + 3);
			c = jit_ploop_read_u16(bc + p + 5);
			if (a == (uint16_t)tmp || b == (uint16_t)tmp ||
			    c == (uint16_t)tmp) return p;
			p += 7; break;
		case OP_IADD: case OP_ISUB: case OP_IMUL:
		case OP_IDIV: case OP_IMOD: case OP_IAND: case OP_IOR:
		case OP_IXOR: case OP_ILT: case OP_ILTE: case OP_IGT:
		case OP_IGTE: case OP_IDIV_CHECKED: case OP_IMOD_CHECKED:
			a = jit_ploop_read_u16(bc + p + 1);
			b = jit_ploop_read_u16(bc + p + 3);
			c = jit_ploop_read_u16(bc + p + 5);
			if (b == (uint16_t)tmp || c == (uint16_t)tmp) return p;
			if (a == (uint16_t)tmp) return UINT32_MAX;
			p += 7; break;
		case OP_ISHL: case OP_ISHR:
			a = jit_ploop_read_u16(bc + p + 1);
			b = jit_ploop_read_u16(bc + p + 3);
			if (b == (uint16_t)tmp) return p;
			if (a == (uint16_t)tmp) return UINT32_MAX;
			p += 6; break;
		case OP_INC:
			a = jit_ploop_read_u16(bc + p + 1);
			if (a == (uint16_t)tmp) return p;
			p += 4; break;
		case OP_SUBJNZ:
			a = jit_ploop_read_u16(bc + p + 1);
			return a == (uint16_t)tmp ? p : UINT32_MAX;
		default:
			return UINT32_MAX;
		}
	}
	return UINT32_MAX;
}
#endif /* PLOOP scanner backends */

/*
 * One bytecode byte is the smallest possible instruction, so bytecode_size
 * entries cover every instruction and delayed branch.  The extra PC entry
 * maps the end of the bytecode.  Tables are per function and impose no fixed
 * 2048-instruction ceiling.
 */
static INLINE bool
jit_context_init_tables(struct jit_context *ctx)
{
	size_t pc_capacity;
	size_t branch_capacity;

	if (ctx->func->bytecode_size == UINT32_MAX) {
		rt_error(ctx->env, "JIT bytecode is too large.");
		return false;
	}
	pc_capacity = (size_t)ctx->func->bytecode_size + 1;
	branch_capacity = (size_t)ctx->func->bytecode_size;
	if (pc_capacity > SIZE_MAX / sizeof(*ctx->pc_entry) ||
	    branch_capacity > SIZE_MAX / sizeof(*ctx->branch_patch)) {
		rt_error(ctx->env, "JIT bytecode is too large.");
		return false;
	}
	ctx->pc_entry = noct_malloc(pc_capacity * sizeof(*ctx->pc_entry));
	if (ctx->pc_entry == NULL) {
		rt_out_of_memory(ctx->env);
		return false;
	}
	ctx->pc_entry_capacity = (uint32_t)pc_capacity;
	if (branch_capacity == 0)
		branch_capacity = 1;
	ctx->branch_patch =
		noct_malloc(branch_capacity * sizeof(*ctx->branch_patch));
	if (ctx->branch_patch == NULL) {
		noct_free(ctx->pc_entry);
		ctx->pc_entry = NULL;
		ctx->pc_entry_capacity = 0;
		rt_out_of_memory(ctx->env);
		return false;
	}
	ctx->branch_patch_capacity = (uint32_t)branch_capacity;
	return true;
}

/* Allocate scalar register-cache analysis storage only for a function that
 * actually contains an eligible PLOOP region. */
static INLINE bool
jit_context_init_regcache(struct jit_context *ctx)
{
	size_t tmp_capacity;

	if (ctx->gpr_tmp_reg != NULL)
		return true;
	tmp_capacity = ctx->func->tmpvar_size != 0 ?
		(size_t)ctx->func->tmpvar_size : 1;
	if (tmp_capacity > SIZE_MAX / sizeof(*ctx->gpr_tmp_reg) ||
	    tmp_capacity > SIZE_MAX / sizeof(*ctx->gpr_range_min) ||
	    tmp_capacity > SIZE_MAX / sizeof(*ctx->gpr_range_max)) {
		rt_error(ctx->env, "JIT temporary-variable table is too large.");
		return false;
	}
#if SIZE_MAX <= UINT32_MAX
	if ((size_t)ctx->func->bytecode_size >
	    SIZE_MAX / sizeof(*ctx->packed_access_disp)) {
		rt_error(ctx->env, "JIT bytecode analysis table is too large.");
		return false;
	}
#endif
	ctx->gpr_tmp_reg = noct_malloc(tmp_capacity *
				       sizeof(*ctx->gpr_tmp_reg));
	ctx->gpr_tmp_dirty = noct_malloc(tmp_capacity *
					 sizeof(*ctx->gpr_tmp_dirty));
	ctx->gpr_remat_valid = noct_malloc(tmp_capacity *
					 sizeof(*ctx->gpr_remat_valid));
	ctx->gpr_remat_value = noct_malloc(tmp_capacity *
					 sizeof(*ctx->gpr_remat_value));
	ctx->gpr_range_min = noct_malloc(tmp_capacity *
					 sizeof(*ctx->gpr_range_min));
	ctx->gpr_range_max = noct_malloc(tmp_capacity *
					 sizeof(*ctx->gpr_range_max));
	ctx->gpr_range_valid = noct_malloc(tmp_capacity *
					   sizeof(*ctx->gpr_range_valid));
	ctx->packed_index_disp = noct_malloc(tmp_capacity *
					    sizeof(*ctx->packed_index_disp));
	ctx->packed_const_value = noct_malloc(tmp_capacity *
					     sizeof(*ctx->packed_const_value));
	ctx->packed_index_valid = noct_malloc(tmp_capacity *
					     sizeof(*ctx->packed_index_valid));
	ctx->packed_const_valid = noct_malloc(tmp_capacity *
					     sizeof(*ctx->packed_const_valid));
	ctx->packed_access_disp = noct_malloc(ctx->func->bytecode_size != 0 ?
		ctx->func->bytecode_size * sizeof(*ctx->packed_access_disp) :
		sizeof(*ctx->packed_access_disp));
	ctx->packed_access_valid = noct_malloc(ctx->func->bytecode_size != 0 ?
		ctx->func->bytecode_size : 1);
	ctx->packed_elide_lpc = noct_malloc(ctx->func->bytecode_size != 0 ?
		ctx->func->bytecode_size : 1);
	ctx->packed_def_lpc = noct_malloc(tmp_capacity *
					 sizeof(*ctx->packed_def_lpc));
	ctx->packed_lpc_use_count = noct_malloc(
		ctx->func->bytecode_size != 0 ?
		ctx->func->bytecode_size * sizeof(*ctx->packed_lpc_use_count) :
		sizeof(*ctx->packed_lpc_use_count));
	ctx->packed_lpc_address_use_count = noct_malloc(
		ctx->func->bytecode_size != 0 ?
		ctx->func->bytecode_size *
			sizeof(*ctx->packed_lpc_address_use_count) :
		sizeof(*ctx->packed_lpc_address_use_count));
	if (ctx->gpr_tmp_reg == NULL || ctx->gpr_tmp_dirty == NULL ||
	    ctx->gpr_remat_valid == NULL || ctx->gpr_remat_value == NULL ||
	    ctx->gpr_range_min == NULL || ctx->gpr_range_max == NULL ||
	    ctx->gpr_range_valid == NULL || ctx->packed_index_disp == NULL ||
	    ctx->packed_const_value == NULL ||
	    ctx->packed_index_valid == NULL ||
	    ctx->packed_const_valid == NULL ||
	    ctx->packed_access_disp == NULL ||
	    ctx->packed_access_valid == NULL ||
	    ctx->packed_elide_lpc == NULL ||
	    ctx->packed_def_lpc == NULL ||
	    ctx->packed_lpc_use_count == NULL ||
	    ctx->packed_lpc_address_use_count == NULL) {
		noct_free(ctx->packed_lpc_address_use_count);
		noct_free(ctx->packed_lpc_use_count);
		noct_free(ctx->packed_def_lpc);
		noct_free(ctx->packed_elide_lpc);
		noct_free(ctx->packed_access_valid);
		noct_free(ctx->packed_access_disp);
		noct_free(ctx->packed_const_valid);
		noct_free(ctx->packed_index_valid);
		noct_free(ctx->packed_const_value);
		noct_free(ctx->packed_index_disp);
		noct_free(ctx->gpr_range_valid);
		noct_free(ctx->gpr_range_max);
		noct_free(ctx->gpr_range_min);
		noct_free(ctx->gpr_tmp_dirty);
		noct_free(ctx->gpr_remat_value);
		noct_free(ctx->gpr_remat_valid);
		noct_free(ctx->gpr_tmp_reg);
		ctx->gpr_tmp_dirty = NULL;
		ctx->gpr_remat_value = NULL;
		ctx->gpr_remat_valid = NULL;
		ctx->gpr_tmp_reg = NULL;
		ctx->gpr_range_valid = NULL;
		ctx->gpr_range_max = NULL;
		ctx->gpr_range_min = NULL;
		ctx->packed_elide_lpc = NULL;
		ctx->packed_access_valid = NULL;
		ctx->packed_access_disp = NULL;
		ctx->packed_const_valid = NULL;
		ctx->packed_index_valid = NULL;
		ctx->packed_const_value = NULL;
		ctx->packed_index_disp = NULL;
		ctx->packed_lpc_address_use_count = NULL;
		ctx->packed_lpc_use_count = NULL;
		ctx->packed_def_lpc = NULL;
		rt_out_of_memory(ctx->env);
		return false;
	}
	return true;
}

static INLINE void
jit_context_dispose_tables(struct jit_context *ctx)
{
	noct_free(ctx->tmp_frame_tag_known);
	noct_free(ctx->tmp_fixed_type);
	noct_free(ctx->tmp_compiler_temp);
	noct_free(ctx->branch_patch);
	noct_free(ctx->pc_entry);
	noct_free(ctx->gpr_tmp_dirty);
	noct_free(ctx->gpr_remat_value);
	noct_free(ctx->gpr_remat_valid);
	noct_free(ctx->gpr_tmp_reg);
	noct_free(ctx->gpr_range_valid);
	noct_free(ctx->gpr_range_max);
	noct_free(ctx->gpr_range_min);
	noct_free(ctx->packed_elide_lpc);
	noct_free(ctx->packed_access_valid);
	noct_free(ctx->packed_access_disp);
	noct_free(ctx->packed_const_valid);
	noct_free(ctx->packed_index_valid);
	noct_free(ctx->packed_const_value);
	noct_free(ctx->packed_index_disp);
	noct_free(ctx->packed_lpc_address_use_count);
	noct_free(ctx->packed_lpc_use_count);
	noct_free(ctx->packed_def_lpc);
	ctx->branch_patch = NULL;
	ctx->tmp_frame_tag_known = NULL;
	ctx->tmp_fixed_type = NULL;
	ctx->tmp_compiler_temp = NULL;
	ctx->pc_entry = NULL;
	ctx->gpr_tmp_dirty = NULL;
	ctx->gpr_remat_value = NULL;
	ctx->gpr_remat_valid = NULL;
	ctx->gpr_tmp_reg = NULL;
	ctx->gpr_range_valid = NULL;
	ctx->gpr_range_max = NULL;
	ctx->gpr_range_min = NULL;
	ctx->packed_elide_lpc = NULL;
	ctx->packed_access_valid = NULL;
	ctx->packed_access_disp = NULL;
	ctx->packed_const_valid = NULL;
	ctx->packed_index_valid = NULL;
	ctx->packed_const_value = NULL;
	ctx->packed_index_disp = NULL;
	ctx->packed_lpc_address_use_count = NULL;
	ctx->packed_lpc_use_count = NULL;
	ctx->packed_def_lpc = NULL;
	ctx->branch_patch_capacity = 0;
	ctx->pc_entry_capacity = 0;
}

static INLINE void
jit_configure_simd(struct jit_context *ctx, uint32_t detected,
		   const char *backend)
{
	ctx->simd_caps = jit_apply_simd_max(detected);
	ctx->has_vector_ops = ctx->func->has_vector_ops;
	if (ctx->func->has_fma_ops &&
	    (ctx->simd_caps & JIT_SIMD_CAP_FMAF32X4) == 0)
		ctx->simd_caps = 0;
	if (getenv("NOCT_JIT_SIMD_DEBUG") != NULL) {
		fprintf(stderr,
			"noct-jit-simd: %s: caps=0x%x vector=%d fma=%d mode=%s\n",
			backend, (unsigned)ctx->simd_caps,
			ctx->has_vector_ops ? 1 : 0,
			ctx->func->has_fma_ops ? 1 : 0,
			ctx->func->has_fma_ops &&
			(ctx->simd_caps & JIT_SIMD_CAP_FMAF32X4) == 0 ?
				"portable" : "native");
	}
}

static INLINE void
jit_dump_standard_code(struct jit_context *ctx, void *generated_end,
		       const char *backend)
{
	const char *dir;
	const char *src;
	char name[96];
	char path[512];
	size_t i;
	size_t n;
	FILE *fp;

	dir = getenv("NOCT_JIT_DUMP_DIR");
	if (dir == NULL || dir[0] == '\0')
		return;
	src = ctx->func->name != NULL ? ctx->func->name : "anonymous";
	for (i = 0; src[i] != '\0' && i + 1 < sizeof(name); i++) {
		char c = src[i];
		name[i] = (c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '-' ?
			c : '_';
	}
	name[i] = '\0';
	if (snprintf(path, sizeof(path), "%s/%s-%p.%s.bin", dir, name,
		     (void *)ctx->func->bytecode, backend) >= (int)sizeof(path))
		return;
	fp = fopen(path, "wb");
	if (fp == NULL)
		return;
	n = (size_t)((uint8_t *)generated_end - (uint8_t *)ctx->code_top);
	(void)fwrite(ctx->code_top, 1, n, fp);
	(void)fclose(fp);
}

/* Map a region. */
bool jit_map_memory_region(void **region, size_t size);

/* Unmap a region. */
void jit_unmap_memory_region(void *region, size_t size);

/* Make a region executable. */
void jit_map_executable(void * region, size_t size);

/* Standard backend body.  The backend supplies its SIMD capability probe;
 * branch patching remains local so architecture-specific ranges are kept. */
#define JIT_BUILD_STANDARD(env_, func_, caps_, backend_) do {		\
	struct jit_context jit_ctx_;					\
	struct jit_slab *jit_slab_;					\
	void *jit_top_;							\
	void *jit_end_;							\
	void *jit_generated_end_;					\
	int jit_attempt_;						\
	int jit_i_;							\
	for (jit_attempt_ = 0; jit_attempt_ < 2; jit_attempt_++) {	\
		if (!jit_slab_acquire((env_), &jit_slab_,			\
				      &jit_top_, &jit_end_))			\
			return false;					\
		memset(&jit_ctx_, 0, sizeof(jit_ctx_));			\
		jit_ctx_.code_top = jit_top_;				\
		jit_ctx_.code_end = jit_end_;				\
		jit_ctx_.code = jit_top_;					\
		jit_ctx_.env = (env_);					\
		jit_ctx_.func = (func_);					\
		if (!jit_context_init_tables(&jit_ctx_))			\
			return false;					\
		jit_configure_simd(&jit_ctx_, (caps_), (backend_));		\
		if (!jit_visit_bytecode(&jit_ctx_)) {			\
			if (jit_ctx_.code_overflow && jit_attempt_ == 0 &&	\
			    ((uint8_t *)jit_top_ != jit_slab_->base ||		\
			     jit_slab_->size < jit_get_code_size((env_)))) {	\
				jit_slab_abandon((env_), jit_slab_);		\
				jit_slab_clear_overflow((env_));		\
				jit_context_dispose_tables(&jit_ctx_);		\
				continue;					\
			}							\
			jit_context_dispose_tables(&jit_ctx_);			\
			return false;					\
		}							\
		jit_generated_end_ = jit_ctx_.code;			\
		for (jit_i_ = 0; jit_i_ < jit_ctx_.branch_patch_count;	\
		     jit_i_++) {						\
			if (!jit_patch_branch(&jit_ctx_, jit_i_)) {		\
				jit_context_dispose_tables(&jit_ctx_);		\
				return false;				\
			}						\
		}							\
		jit_dump_standard_code(&jit_ctx_, jit_generated_end_,		\
				       (backend_));				\
		jit_slab_finish((env_), jit_slab_, jit_generated_end_);	\
		if (getenv("NOCT_JIT_CODEGEN_DEBUG") != NULL)		\
			fprintf(stderr,					\
				"noct-jit-codegen: %s: func=%s bytes=%lu pc_entries=%u branches=%d\n", \
				(backend_), (func_)->name != NULL ?		\
				(func_)->name : "?",				\
				(unsigned long)((uint8_t *)jit_generated_end_ - \
					(uint8_t *)jit_ctx_.code_top),		\
				jit_ctx_.pc_entry_count,			\
				jit_ctx_.branch_patch_count);			\
		(func_)->jit_code =						\
			(bool (CDECL *)(struct rt_env *))jit_ctx_.code_top;	\
		jit_context_dispose_tables(&jit_ctx_);				\
		return true;						\
	}								\
	return false;							\
} while (0)

/*
 * Get an opcode.
 */
#define CONSUME_OPCODE(d)	if (!jit_get_opcode(ctx, &d)) return false
static INLINE bool
jit_get_opcode(
	struct jit_context *ctx,
	uint8_t *opcode)
{
	if (ctx->lpc + 1 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*opcode = 0;
		return false;
	}

	*opcode = ctx->func->bytecode[ctx->lpc];

	ctx->lpc++;

	return true;
}

/*
 * Get an imm32 operand.
 */
#define CONSUME_IMM32(d)	if (!jit_get_opr_imm32(ctx, &d)) return false
static INLINE bool
jit_get_opr_imm32(
	struct jit_context *ctx,
	uint32_t *d)
{
	if (ctx->lpc + 4 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*d = 0;
		return false;
	}

	*d = ((uint32_t)ctx->func->bytecode[ctx->lpc] << 24) |
	     (uint32_t)(ctx->func->bytecode[ctx->lpc + 1] << 16) |
	     (uint32_t)(ctx->func->bytecode[ctx->lpc + 2] << 8) |
	     (uint32_t)ctx->func->bytecode[ctx->lpc + 3];

	ctx->lpc += 4;

	return true;
}

/*
 * Get an imm64 operand.
 */
#define CONSUME_IMM64(d)	if (!jit_get_opr_imm64(ctx, &d)) return false
static INLINE bool
jit_get_opr_imm64(
	struct jit_context *ctx,
	uint64_t *d)
{
	if (ctx->lpc + 8 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*d = 0;
		return false;
	}

	*d = ((uint64_t)ctx->func->bytecode[ctx->lpc + 0] << 56) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 1] << 48) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 2] << 40) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 3] << 32) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 4] << 24) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 5] << 16) |
	     ((uint64_t)ctx->func->bytecode[ctx->lpc + 6] << 8) |
             ((uint64_t)ctx->func->bytecode[ctx->lpc + 7]);

	ctx->lpc += 8;

	return true;
}

/*
 * Get an imm16 operand that represents tmpvar index.
 */
#define CONSUME_TMPVAR(d)	if (!jit_get_opr_tmpvar(ctx, &d)) return false
static INLINE bool
jit_get_opr_tmpvar(
	struct jit_context *ctx,
	int *d)
{
	if (ctx->lpc + 2 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*d = 0;
		return false;
	}

	*d = (ctx->func->bytecode[ctx->lpc] << 8) |
	      ctx->func->bytecode[ctx->lpc + 1];
	if ((uint32_t)*d >= ctx->func->tmpvar_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}

	ctx->lpc += 2;

	return true;
}

/*
 * Get an imm8 operand.
 */
#define CONSUME_IMM8(d)		if (!jit_get_imm8(ctx, &d)) return false
static INLINE bool
jit_get_imm8(
	struct jit_context *ctx,
	int *imm8)
{
	if (ctx->lpc + 1 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*imm8 = 0;
		return false;
	}

	*imm8 = ctx->func->bytecode[ctx->lpc];

	ctx->lpc++;

	return true;
}

/* Consume the architecture-neutral scalar Packed-loop hint. */
static INLINE bool
jit_visit_ploop_hint_op(struct jit_context *ctx)
{
	int index_tmp;
	int stop_tmp;
	int remaining_tmp;
	int lanes;
	int flags;

	if (!jit_get_opr_tmpvar(ctx, &index_tmp) ||
	    !jit_get_opr_tmpvar(ctx, &stop_tmp) ||
	    !jit_get_opr_tmpvar(ctx, &remaining_tmp) ||
	    !jit_get_imm8(ctx, &lanes) ||
	    !jit_get_imm8(ctx, &flags))
		return false;
	if (lanes != 1 ||
	    ((flags & PLOOP_TYPED_INT) != 0 &&
	     (flags & PLOOP_TYPED_FLOAT) != 0)) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	ctx->packed_loop_hint_active = true;
	ctx->packed_loop_index_tmp = index_tmp;
	ctx->packed_loop_stop_tmp = stop_tmp;
	ctx->packed_loop_remaining_tmp = remaining_tmp;
	ctx->packed_loop_lanes = lanes;
	ctx->packed_loop_flags = flags;
	return true;
}

/* Function-head metadata emitted after whole-function type aggregation. */
static INLINE bool
jit_visit_tmpvar_type_op(struct jit_context *ctx)
{
	int tmp;
	int type;
	size_t count;

	if (!jit_get_opr_tmpvar(ctx, &tmp) || !jit_get_imm8(ctx, &type))
		return false;
	bool compiler_temp;

	compiler_temp = (type & TMPVAR_TYPE_COMPILER_TEMP) != 0;
	type &= ~TMPVAR_TYPE_COMPILER_TEMP;
	if (type != TMPVAR_TYPE_DYNAMIC &&
	    type != NOCT_VALUE_INT && type != NOCT_VALUE_LONG &&
	    type != NOCT_VALUE_FLOAT && type != NOCT_VALUE_DOUBLE) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	if (ctx->tmp_fixed_type == NULL) {
		count = ctx->func->tmpvar_size != 0 ?
			(size_t)ctx->func->tmpvar_size : 1;
		ctx->tmp_fixed_type = noct_malloc(count);
		ctx->tmp_frame_tag_known = noct_calloc(count, 1);
		ctx->tmp_compiler_temp = noct_calloc(count, 1);
		if (ctx->tmp_fixed_type == NULL ||
		    ctx->tmp_frame_tag_known == NULL ||
		    ctx->tmp_compiler_temp == NULL) {
			noct_free(ctx->tmp_compiler_temp);
			noct_free(ctx->tmp_frame_tag_known);
			noct_free(ctx->tmp_fixed_type);
			ctx->tmp_frame_tag_known = NULL;
			ctx->tmp_fixed_type = NULL;
			ctx->tmp_compiler_temp = NULL;
			rt_out_of_memory(ctx->env);
			return false;
		}
		memset(ctx->tmp_fixed_type, -1, count);
	}
	ctx->tmp_fixed_type[tmp] = type == TMPVAR_TYPE_DYNAMIC ? -1 :
		(int8_t)type;
	ctx->tmp_compiler_temp[tmp] = compiler_temp ? 1 : 0;
	/* A fresh non-parameter slot has zero tag, which is INT. */
	if (!compiler_temp && (uint32_t)tmp >= ctx->func->param_count &&
	    type == NOCT_VALUE_INT)
		ctx->tmp_frame_tag_known[tmp] = 1;
	return true;
}

/* Non-optimizing backends keep frame tags canonical and only need to consume
 * the explicit materialization boundary. */
static INLINE bool
jit_visit_materialize_type_metadata_op(struct jit_context *ctx)
{
	int tmp;
	int type;

	if (!jit_get_opr_tmpvar(ctx, &tmp) || !jit_get_imm8(ctx, &type))
		return false;
	UNUSED_PARAMETER(tmp);
	if (type != NOCT_VALUE_INT && type != NOCT_VALUE_LONG &&
	    type != NOCT_VALUE_FLOAT && type != NOCT_VALUE_DOUBLE) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		return false;
	}
	return true;
}

static INLINE bool
jit_tmp_has_fixed_primitive_type(struct jit_context *ctx, int tmp, int type)
{
	return ctx->tmp_fixed_type != NULL &&
	       (type == NOCT_VALUE_INT || type == NOCT_VALUE_LONG ||
		type == NOCT_VALUE_FLOAT || type == NOCT_VALUE_DOUBLE) &&
	       ctx->tmp_fixed_type[tmp] == type;
}

/*
 * Get a string operand.
 */
#define CONSUME_STRING(s,l,h)	if (!jit_get_opr_string(ctx, &s, &l, &h)) return false
static INLINE bool
jit_get_opr_string(
	struct jit_context *ctx,
	const char **s,
	uint32_t *len,
	uint32_t *hash)
{
	if (ctx->lpc + 8 > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*s = 0;
		return false;
	}

	*len = ((uint32_t)ctx->func->bytecode[ctx->lpc] << 24) |
		(uint32_t)(ctx->func->bytecode[ctx->lpc + 1] << 16) |
		(uint32_t)(ctx->func->bytecode[ctx->lpc + 2] << 8) |
		(uint32_t)ctx->func->bytecode[ctx->lpc + 3];

	*hash = ((uint32_t)ctx->func->bytecode[ctx->lpc + 4] << 24) |
		(uint32_t)(ctx->func->bytecode[ctx->lpc + 5] << 16) |
		(uint32_t)(ctx->func->bytecode[ctx->lpc + 6] << 8) |
		(uint32_t)ctx->func->bytecode[ctx->lpc + 7];

	if (ctx->lpc + 8 + *len > ctx->func->bytecode_size) {
		rt_error(ctx->env, BROKEN_BYTECODE);
		*s = NULL;
		return false;
	}

	*s = (const char *)&ctx->func->bytecode[ctx->lpc + 8];

	ctx->lpc += 8 + *len;

	return true;
}

#endif /* defined(NOCT_USE_JIT) */

#endif
