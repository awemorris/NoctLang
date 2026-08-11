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

/* PC entry size. */
#define PC_ENTRY_MAX		2048

/* Branch pathch size. */
#define BRANCH_PATCH_MAX	2048

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
	int vector_base_tmp[2];
	uint32_t vector_base_last_lpc[2];
	int vector_imm_value;
	int vector_imm_shift;

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
	} pc_entry[PC_ENTRY_MAX];
	uint32_t pc_entry_count;

	/* Delayed branch patching table. */
	struct branch_patch {
		/* Native code address. */
		uint32_t *code;

		/* LIR-PC */
		uint32_t lpc;

		/* Branch type. */
		int type;
	} branch_patch[BRANCH_PATCH_MAX];
	int branch_patch_count;
};

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
		jit_configure_simd(&jit_ctx_, (caps_), (backend_));		\
		if (!jit_visit_bytecode(&jit_ctx_)) {			\
			if (jit_ctx_.code_overflow && jit_attempt_ == 0 &&	\
			    ((uint8_t *)jit_top_ != jit_slab_->base ||		\
			     jit_slab_->size < jit_get_code_size((env_)))) {	\
				jit_slab_abandon((env_), jit_slab_);		\
				jit_slab_clear_overflow((env_));		\
				continue;					\
			}							\
			return false;					\
		}							\
		jit_generated_end_ = jit_ctx_.code;			\
		for (jit_i_ = 0; jit_i_ < jit_ctx_.branch_patch_count;	\
		     jit_i_++) {						\
			if (!jit_patch_branch(&jit_ctx_, jit_i_))		\
				return false;				\
		}							\
		jit_slab_finish((env_), jit_slab_, jit_generated_end_);	\
		(func_)->jit_code =						\
			(bool (CDECL *)(struct rt_env *))jit_ctx_.code_top;	\
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
