/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * LIR: Low-level Intermediate Representation
 */

#include <noct/noct.h>
#include "lir.h"
#include "hir.h"
#include "bytecode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* False assertion */
#define NEVER_COME_HERE		0
#define INVALID_OPCODE		0

/* Debug print */
#undef DEBUG_BLOCK_ORDER
#undef DEBUG_DUMP_LIR

/*
 * Target LIR.
 */

#define BYTECODE_BUF_SIZE	65536

/* Bytecode array. */
/*static uint8_t bytecode[BYTECODE_BUF_SIZE];*/
static uint8_t *bytecode;

/* Cuurent bytecode length. */
static uint32_t bytecode_top;

/*
 * Variable table.
 */

static uint32_t tmpvar_top;
static uint32_t tmpvar_count;

/* ABI/prologue metadata for the function currently being built. */
static bool has_vector_ops;

/*
 * Typed-op emission state (docs/design/07-typed-ops.md, D-TOP11).
 * Reset per lir_build().
 */
static int typed_emit_int_count;
static int typed_emit_float_count;
static int typed_generic_count;
static int typed_disabled;

/*
 * Relocation table.
 */

/* Maximum relocation count */
#define LOC_MAX	1024

/* Relocation type */
#define LOC_BLOCK_TOP		0
#define LOC_BLOCK_CONTINUE	1

struct loc_entry {
	/* Type. */
	int type;

	/* Location offset. */
	uint32_t offset;

	/* Branch target. */
	struct hir_block *block;
};

static struct loc_entry loc_tbl[LOC_MAX];
static int loc_count;

/*
 * Error position and message.
 */

static char *lir_file_name;
static int lir_error_line;
static char lir_error_message[1024];

/*
 * Optimize lelve.
 */
int lir_optimize_level = 0;

/*
 * Set the optimization level. (Propagated from NoctConfig; see
 * docs/design/01-abce.md section 3.7.)
 */
void
lir_set_optimize_level(int level)
{
	lir_optimize_level = level;
}

/*
 * Forward declaration.
 */
static uint32_t lir_count_local(struct hir_block *func);
static bool lir_visit_block(struct hir_block *block);
static bool lir_visit_basic_block(struct hir_block *block);
static bool lir_check_succ_loop_head(struct hir_block *block, struct hir_block **loop);
static bool lir_visit_if_block(struct hir_block *block);
static bool lir_visit_for_block(struct hir_block *block);
static bool lir_visit_vfor_block(struct hir_block *block);
static bool lir_visit_for_range_block(struct hir_block *block);
static bool lir_visit_for_kv_block(struct hir_block *block);
static bool lir_visit_for_v_block(struct hir_block *block);
static int lir_get_local_index(struct hir_block *block, const char *symbol);
static bool lir_visit_while_block(struct hir_block *block);
static bool lir_visit_stmt(struct hir_block *block, struct hir_stmt *stmt);
static bool lir_check_lhs_local(struct hir_block *block, struct hir_expr *lhs, int *rhs_tmpvar);
static bool lir_visit_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_abce_unary_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_abce_typetest_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_unary_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_binary_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_logical_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_dot_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_capture_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_call_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_thiscall_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_array_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_dict_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_new_expr(int dst_tmpvar, struct hir_expr *expr, struct hir_block *block);
static bool lir_visit_term(int dst_tmpvar, struct hir_term *term, struct hir_block *block);
static bool lir_visit_symbol_term(int dst_tmpvar, struct hir_term *term, struct hir_block *block);
static bool lir_visit_int_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_long_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_float_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_double_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_string_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_empty_array_term(int dst_tmpvar, struct hir_term *term);
static bool lir_visit_empty_dict_term(int dst_tmpvar, struct hir_term *term);
static bool lir_increment_tmpvar(int *tmpvar_index);
static bool lir_decrement_tmpvar(int tmpvar_index);
static bool lir_put_opcode(uint8_t op);
static bool lir_put_tmpvar(uint16_t index);
static bool lir_put_imm8(uint8_t imm);
static bool lir_put_imm32(uint32_t imm);
static bool lir_put_imm64(uint64_t imm);
static bool lir_put_string(const char *data);
static bool lir_put_branch_addr(struct hir_block *block);
static bool lir_put_continue_addr(struct hir_block *block);
static bool lir_put_u8(uint8_t b);
static bool lir_put_u16(uint16_t b);
static bool lir_put_u32(uint32_t b);
static bool lir_put_u64(uint64_t b);
static void patch_block_address(void);
static void lir_fatal(const char *msg, ...);
static void lir_out_of_memory(void);

/*
 * Build a LIR function from a HIR function.
 */
bool
lir_build(
	struct hir_block *hir_func,
	struct lir_func **lir_func)
{
	struct hir_block *cur_block;
	uint32_t i;

	assert(hir_func != NULL);
	assert(hir_func->type == HIR_BLOCK_FUNC);

	/* Copy the file name. */
	lir_file_name = noct_strdup(hir_func->val.func.file_name);
	if (lir_file_name == NULL) {
		lir_out_of_memory();
		return false;
	}

	/* Initialize the bytecode buffer. */
	if (bytecode != NULL) {
		noct_free(bytecode);
		bytecode = NULL;
	}
	bytecode = noct_calloc(BYTECODE_BUF_SIZE, 1);
	if (bytecode == NULL) {
		lir_out_of_memory();
		noct_free(bytecode);
		bytecode = NULL;
		return false;
	}
	bytecode_top = 0;
	has_vector_ops = false;

	/* Typed-op emission state (design 07). */
	typed_emit_int_count = 0;
	typed_emit_float_count = 0;
	typed_generic_count = 0;
	typed_disabled = (getenv("NOCT_TYPED_DISABLE") != NULL);

	/* Initialize the tmpvars. */
	tmpvar_top = lir_count_local(hir_func);
	if (tmpvar_top == 0) {
		/* For the return value. */
		tmpvar_top = 1;
	}
	tmpvar_count = tmpvar_top;

	/* Initialize the relocation table. */
	loc_count = 0;

	/* Typed entry checks (docs/design/02-typing.md; level >= 2). */
	if (lir_optimize_level >= 2) {
		uint32_t k;
		for (k = 0; k < hir_func->val.func.param_count; k++) {
			int check_type;

			if (hir_func->val.func.param_type[k] < 0)
				continue;
			check_type = hir_func->val.func.param_type[k];
			if (check_type == NOCT_VALUE_PACKED &&
			    hir_func->val.func.param_packed_type[k] >= 0 &&
			    hir_func->val.func.param_packed_type[k] != NOCT_PACKED_ANY)
				check_type =
					(hir_func->val.func.param_restricted[k] ?
					 TYPECHECK_RPACKED_BASE : TYPECHECK_PACKED_BASE) +
					hir_func->val.func.param_packed_type[k];
			if (!lir_put_opcode(OP_CHECKTYPE))
				return false;
			if (!lir_put_tmpvar((uint16_t)k))
				return false;
			if (!lir_put_imm8((uint8_t)check_type))
				return false;
		}
	}

	/* Visit blocks. */
	cur_block = hir_func->val.func.inner;
	while (cur_block != NULL) {
		/* Visit a block. */
		lir_visit_block(cur_block);

		/* Move to a next. */
		if (cur_block->stop) {
			assert(cur_block->succ->type == HIR_BLOCK_END);
			cur_block->succ->addr = (uint32_t)bytecode_top;
			break;
		}
		cur_block = cur_block->succ;
	}

	/* Patch block address. */
	patch_block_address();

	/* Make an lir_func. */
	*lir_func = noct_malloc(sizeof(struct lir_func));
	if (*lir_func == NULL) {
		lir_out_of_memory();
		return false;
	}

	/* Copy the function name. */
	(*lir_func)->func_name = noct_strdup(hir_func->val.func.name);
	if ((*lir_func)->func_name == NULL) {
		lir_out_of_memory();
		return false;
	}

	/* Copy the parameter names.  */
	(*lir_func)->param_count = hir_func->val.func.param_count;
	for (i = 0; i < LIR_PARAM_SIZE; i++) {
		(*lir_func)->param_type[i] = -1;
		(*lir_func)->param_packed_type[i] = -1;
		(*lir_func)->param_restricted[i] = false;
	}
	for (i = 0; i < hir_func->val.func.param_count; i++) {
		(*lir_func)->param_type[i] = hir_func->val.func.param_type[i];
		(*lir_func)->param_packed_type[i] =
			hir_func->val.func.param_packed_type[i];
		(*lir_func)->param_restricted[i] =
			hir_func->val.func.param_restricted[i];
	}
	for (i = 0; i < hir_func->val.func.param_count; i++) {
		(*lir_func)->param_name[i] = noct_strdup(hir_func->val.func.param_name[i]);
		if ((*lir_func)->param_name[i] == NULL) {
			lir_out_of_memory();
			return false;
		}
	}

	/* Copy the bytecode. */
	if (bytecode_top != 0) {
		(*lir_func)->bytecode = noct_malloc((size_t)bytecode_top);
		if ((*lir_func)->bytecode == NULL) {
			lir_out_of_memory();
			return false;
		}
		memcpy((*lir_func)->bytecode, bytecode, (size_t)bytecode_top);
	} else {
		(*lir_func)->bytecode = NULL;
	}
	(*lir_func)->bytecode_size = bytecode_top;
	noct_free(bytecode);
	bytecode = NULL;

	/* Copy the file name. */
	(*lir_func)->file_name = noct_strdup(hir_func->val.func.file_name);
	if ((*lir_func)->file_name == NULL) {
		lir_out_of_memory();
		return false;
	}

	(*lir_func)->tmpvar_size = tmpvar_count + 1;
	(*lir_func)->has_vector_ops = has_vector_ops;

#ifdef DEBUG_DUMP_LIR
	lir_dump(*lir_func);
#endif

	/* Typed-op observability (design 07 D-TOP11). */
	if (getenv("NOCT_TYPED_DEBUG") != NULL) {
		fprintf(stderr,
			"TYPED: %s: emitted=%d (int=%d float=%d) sites_generic=%d\n",
			hir_func->val.func.name != NULL ?
			hir_func->val.func.name : "?",
			typed_emit_int_count + typed_emit_float_count,
			typed_emit_int_count, typed_emit_float_count,
			typed_generic_count);
	}

	return true;
}

/* Count the number of the local variables in a func. */
static uint32_t
lir_count_local(
	struct hir_block *func)
{
	struct hir_local *local;
	uint32_t count;

	count = 0;
	local = func->val.func.local;
	while (local != NULL) {
		count++;
		local = local->next;
	}

	return count;
}

static bool
lir_visit_block(
	struct hir_block *block)
{
	assert(block != NULL);

#ifdef DEBUG_BLOCK_ORDER
	printf("LIR-pass: BLOCK %d\n", block->id);
#endif

	lir_error_line = block->line;

	switch (block->type) {
	case HIR_BLOCK_BASIC:
		if (!lir_visit_basic_block(block))
			return false;
		break;
	case HIR_BLOCK_IF:
		if (!lir_visit_if_block(block))
			return false;
		break;
	case HIR_BLOCK_FOR:
		if (!lir_visit_for_block(block))
			return false;
		break;
	case HIR_BLOCK_WHILE:
		if (!lir_visit_while_block(block))
			return false;
		break;
	case HIR_BLOCK_END:
		return true;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	return true;
}

static bool
lir_visit_basic_block(
	struct hir_block *block)
{
	struct hir_stmt *stmt;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_BASIC);
	assert(block->parent != NULL);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Visit statements. */
	stmt = block->val.basic.stmt_list;
	while (stmt != NULL) {
		/* Visit a statement. */
		if (!lir_visit_stmt(block, stmt))
			return false;
		stmt = stmt->next;
	}

	/*
	 * If the block is the tail of the siblings, it needs an explicit
	 * jump unless its successor is the block control would reach by
	 * falling through anyway.
	 *
	 * Falling through is correct only in two cases:
	 *
	 *  - The block ends a loop body and continues that same loop:
	 *    the loop emitter appends the incrementer and the back edge
	 *    right after this block.
	 *
	 *  - The block ends the function body and goes to the end block,
	 *    which is emitted next.
	 *
	 * Everything else (a break, a return from inside a loop, or a
	 * continue targeting an outer loop) has to jump. Treating those
	 * as fall-through is what used to make a "return" at the tail of
	 * a loop body run the rest of the loop instead of returning.
	 */
	if (block->stop) {
		struct hir_block *loop;
		struct hir_block *parent;
		bool falls_through;

		parent = block->parent;
		falls_through = false;
		if (parent->type == HIR_BLOCK_FOR) {
			falls_through = block->succ == parent->val.for_.inner;
		} else if (parent->type == HIR_BLOCK_WHILE) {
			falls_through = block->succ == parent->val.while_.inner;
		} else if (parent->type == HIR_BLOCK_FUNC) {
			falls_through = block->succ != NULL &&
					block->succ->type == HIR_BLOCK_END;
		}

		if (!falls_through) {
			/* Check if succ is a loop head. */
			if (lir_check_succ_loop_head(block, &loop)) {
				/* Put a safepoint. */
				if (!lir_put_opcode(OP_SAFEPOINT))
					return false;

				/* Continue edge. */
				if (!lir_put_opcode(OP_JMP))
					return false;
				if (!lir_put_continue_addr(loop))
					return false;
			} else {
				/* Break or return edge. */
				if (!lir_put_opcode(OP_JMP))
					return false;
				if (!lir_put_branch_addr(block->succ))
					return false;
			}
		}
	}

	return true;
}

/* Check if succ is a loop head. (Detects a continue edge) */
static bool
lir_check_succ_loop_head(
	struct hir_block *block,
	struct hir_block **loop)
{
	struct hir_block *b;

	b = block;
	while (b != NULL) {
		if (b->type == HIR_BLOCK_FOR) {
			if (b->val.for_.inner == block->succ) {
				*loop = b;
				return true;
			}
		}
		if (b->type == HIR_BLOCK_WHILE) {
			if (b->val.while_.inner == block->succ) {
				*loop = b;
				return true;
			}
		}
		b = b->parent;
	}
	return false;
}

static bool
lir_visit_if_block(
	struct hir_block *block)
{
	int cond_tmpvar;
	bool is_else;
	struct hir_block *b;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_IF);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Is an else-block? */
	if (block->val.if_.cond == NULL) {
		is_else = true;
	} else {
		is_else = false;
	}

	/* If this is not an else-block. */
	if (!is_else) {
		/* Skip this block if the condition is not met. */
		if (!lir_increment_tmpvar(&cond_tmpvar))
			return false;
		if (!lir_visit_expr(cond_tmpvar, block->val.if_.cond, block))
			return false;
		if (!lir_put_opcode(OP_JMPIFFALSE))
			return false;
		if (!lir_put_tmpvar((uint16_t)cond_tmpvar))
			return false;
		if (block->val.if_.chain_next != NULL) {
			/* Jump to a chaining else-block. */
			if (!lir_put_branch_addr(block->val.if_.chain_next))
				return false;
		} else {
			/* Jump to a first non-if block. */
			if (block->succ != NULL) {
				/* if-block */
				if (!lir_put_branch_addr(block->succ))
					return false;
			} else {
				/* elif-block */
				if (!lir_put_branch_addr(block->parent->succ))
					return false;
			}
		}
		lir_decrement_tmpvar(cond_tmpvar);
	}

	/* Visit an inner block. */
	b = block->val.if_.inner;
	while (b != NULL) {
		if (!lir_visit_block(b))
			return false;
		if (b->stop)
			break;
		b = b->succ;
	}

	/* If this is an if-block or an else-if block. */
	if (!is_else) {
		/* Jump to a first non-if block. */
		if (!lir_put_opcode(OP_JMP))
			return false;
		if (block->succ != NULL) {
			/* if-block */
			if (!lir_put_branch_addr(block->succ))
				return false;
		} else {
			/* elif-block */
			if (!lir_put_branch_addr(block->parent->succ))
				return false;
		}
	}

	/* Visit a chaining block if exists. */
	if (block->val.if_.chain_next != NULL) {
		if (!lir_visit_block(block->val.if_.chain_next))
			return false;
	}

	return true;
}

static bool
lir_visit_for_block(
	struct hir_block *block)
{
	assert(block != NULL);
	assert(block->type == HIR_BLOCK_FOR);

	/* Dispatch by type. */
	if (block->val.for_.is_vector) {
		/* This is a vectorized strip loop (design 06). */
		if (!lir_visit_vfor_block(block))
			return false;
	} else if (block->val.for_.is_ranged) {
		/* This is a ranged-for loop. */
		if (!lir_visit_for_range_block(block))
			return false;
	} else if (block->val.for_.key_symbol != NULL) {
		/* This is a for-each-key-and-value loop. */
		if (!lir_visit_for_kv_block(block))
			return false;
	} else {
		/* This is a for-each-value loop. */
		if (!lir_visit_for_v_block(block))
			return false;
	}

	return true;
}

/*
 * Vectorized strip-loop lowering (docs/design/06-simd.md).
 *
 * The body is the eligible vector grammar (checked by
 * hir_opt_simd.c): a single basic block of "temp = expr" and
 * "PSTORE32/PSTOREF32(sb, i) = expr" statements over homogeneous
 * int32 or float32 constants, invariant locals and temp locals.
 *
 * vreg plan (MUST mirror hir_opt_simd.c's budget computation):
 *   [0 .. nconst)               one per distinct int constant
 *   [nconst .. +ninv)           one per invariant local
 *   [.. +ntemp)                 one per temp local
 *   [.. 8)                      LIFO expression stack
 * TERM operands are consumed directly from their home vregs; only
 * non-term subtree results occupy stack slots.
 */

#define VFOR_VREG_MAX		8
#define VFOR_MAX_CONSTS		8
#define VFOR_MAX_LOCALS		8

struct vfor_plan {
	struct hir_block *loop;
	const char *counter;
	int counter_tmpvar;

	uint32_t consts[VFOR_MAX_CONSTS];	/* int value or float bits */
	int const_count;
	const char *inv[VFOR_MAX_LOCALS];
	int inv_count;
	const char *temp[VFOR_MAX_LOCALS];
	int temp_count;
	int stack_base;
	bool is_float;
};

static struct hir_expr *
lir_vfor_strip_par(struct hir_expr *e)
{
	while (e->type == HIR_EXPR_PAR)
		e = e->val.unary.expr;
	return e;
}

/* Map a TERM to its home vreg (const or local). */
static int
lir_vfor_term_vreg(struct vfor_plan *plan, struct hir_expr *e)
{
	struct hir_term *t = e->val.term.term;
	int i;

	if (t->type == HIR_TERM_INT || t->type == HIR_TERM_FLOAT) {
		uint32_t bits;
		if (t->type == HIR_TERM_INT)
			bits = (uint32_t)t->val.i;
		else
			memcpy(&bits, &t->val.f, sizeof(bits));
		for (i = 0; i < plan->const_count; i++) {
			if (plan->consts[i] == bits)
				return i;
		}
		return -1;
	}
	if (t->type == HIR_TERM_SYMBOL) {
		for (i = 0; i < plan->inv_count; i++) {
			if (strcmp(plan->inv[i], t->val.symbol) == 0)
				return plan->const_count + i;
		}
		for (i = 0; i < plan->temp_count; i++) {
			if (strcmp(plan->temp[i], t->val.symbol) == 0)
				return plan->const_count + plan->inv_count + i;
		}
	}
	return -1;
}

/* Plan collection walk (mirror of hir_opt_simd.c's collection). */
static bool
lir_vfor_collect(struct vfor_plan *plan, struct hir_expr *e)
{
	int i;

	switch (e->type) {
	case HIR_EXPR_TERM:
		if (e->val.term.term->type == HIR_TERM_INT ||
		    e->val.term.term->type == HIR_TERM_FLOAT) {
			uint32_t v;
			if (plan->is_float !=
			    (e->val.term.term->type == HIR_TERM_FLOAT))
				return false;
			if (plan->is_float)
				memcpy(&v, &e->val.term.term->val.f, sizeof(v));
			else
				v = (uint32_t)e->val.term.term->val.i;
			for (i = 0; i < plan->const_count; i++) {
				if (plan->consts[i] == v)
					return true;
			}
			if (plan->const_count >= VFOR_MAX_CONSTS)
				return false;
			plan->consts[plan->const_count++] = v;
			return true;
		}
		if (e->val.term.term->type == HIR_TERM_SYMBOL) {
			const char *sym = e->val.term.term->val.symbol;
			for (i = 0; i < plan->temp_count; i++) {
				if (strcmp(plan->temp[i], sym) == 0)
					return true;
			}
			for (i = 0; i < plan->inv_count; i++) {
				if (strcmp(plan->inv[i], sym) == 0)
					return true;
			}
			if (plan->inv_count >= VFOR_MAX_LOCALS)
				return false;
			plan->inv[plan->inv_count++] = sym;
			return true;
		}
		return false;
	case HIR_EXPR_PAR:
		return lir_vfor_collect(plan, e->val.unary.expr);
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
		/* Base local + bare-counter index: no vreg operands. */
		return true;
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		/* The count is an immediate, not a vector operand. */
		return lir_vfor_collect(plan, e->val.binary.expr[0]);
	default:
		if (!lir_vfor_collect(plan, e->val.binary.expr[0]))
			return false;
		return lir_vfor_collect(plan, e->val.binary.expr[1]);
	}
}

/* Emit one vector opcode with mixed u16/imm8 operands. */
static bool
lir_vfor_put3(int op, int a_is_imm8, int a, int b_is_imm8, int b,
	      int c_is_imm8, int c, int c_present)
{
	if (!lir_put_opcode((uint8_t)op))
		return false;
	if (a_is_imm8) {
		if (!lir_put_imm8((uint8_t)a))
			return false;
	} else {
		if (!lir_put_tmpvar((uint16_t)a))
			return false;
	}
	if (b_is_imm8) {
		if (!lir_put_imm8((uint8_t)b))
			return false;
	} else {
		if (!lir_put_tmpvar((uint16_t)b))
			return false;
	}
	if (!c_present)
		return true;
	if (c_is_imm8) {
		if (!lir_put_imm8((uint8_t)c))
			return false;
	} else {
		if (!lir_put_tmpvar((uint16_t)c))
			return false;
	}
	return true;
}

/* Does the expression read the given symbol anywhere? */
static bool
lir_vfor_expr_reads(struct hir_expr *e, const char *sym)
{
	switch (e->type) {
	case HIR_EXPR_TERM:
		return e->val.term.term->type == HIR_TERM_SYMBOL &&
			strcmp(e->val.term.term->val.symbol, sym) == 0;
	case HIR_EXPR_PAR:
		return lir_vfor_expr_reads(e->val.unary.expr, sym);
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
		return false;
	default:
		return lir_vfor_expr_reads(e->val.binary.expr[0], sym) ||
			lir_vfor_expr_reads(e->val.binary.expr[1], sym);
	}
}

/*
 * Evaluate a vector expression into dst (always a stack slot or a
 * destination the expression provably does not read; the statement
 * lowering guarantees this).  sp = first free stack vreg.  The
 * strategy and its slot-need formula MUST mirror hir_opt_simd.c's
 * simd_check_expr(): non-term left operands (and, for commutative
 * ops, lone non-term right operands) are built in the destination
 * itself; only a second concurrent subtree takes a stack slot.
 */
static bool
lir_vfor_expr(struct vfor_plan *plan, int dst, int sp, struct hir_expr *e)
{
	int op;

	e = lir_vfor_strip_par(e);

	switch (e->type) {
	case HIR_EXPR_TERM:
	{
		int src = lir_vfor_term_vreg(plan, e);
		if (src < 0) {
			lir_fatal("SIMD: unplanned term.");
			return false;
		}
		if (src == dst)
			return true;
		return lir_vfor_put3(OP_VMOV128, 1, dst, 1, src, 0, 0, 0);
	}
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
	{
		int base_tmpvar = lir_get_local_index(plan->loop,
			e->val.binary.expr[0]->val.term.term->val.symbol);
		return lir_vfor_put3(plan->is_float ? OP_VLOADF32X4 : OP_VLOADI32X4,
				     1, dst,
				     0, base_tmpvar,
				     0, plan->counter_tmpvar, 1);
	}
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
	{
		struct hir_expr *x = lir_vfor_strip_par(e->val.binary.expr[0]);
		int count = e->val.binary.expr[1]->val.term.term->val.i;
		int src;
		if (x->type == HIR_EXPR_TERM) {
			src = lir_vfor_term_vreg(plan, x);
			if (src < 0) {
				lir_fatal("SIMD: unplanned term.");
				return false;
			}
		} else {
			if (!lir_vfor_expr(plan, dst, sp, x))
				return false;
			src = dst;
		}
		if (count == 0) {
			if (src == dst)
				return true;
			return lir_vfor_put3(OP_VMOV128, 1, dst, 1, src, 0, 0, 0);
		}
		op = (e->type == HIR_EXPR_SHL) ? OP_VSHLI32X4 : OP_VSHRI32X4;
		return lir_vfor_put3(op, 1, dst, 1, src, 1, count, 1);
	}
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	{
		struct hir_expr *l = lir_vfor_strip_par(e->val.binary.expr[0]);
		struct hir_expr *r = lir_vfor_strip_par(e->val.binary.expr[1]);
		bool commutative = (e->type != HIR_EXPR_MINUS &&
				    e->type != HIR_EXPR_DIV);
		int va, vb;

		switch (e->type) {
		case HIR_EXPR_PLUS:  op = plan->is_float ? OP_VADDF32X4 : OP_VADDI32X4; break;
		case HIR_EXPR_MINUS: op = plan->is_float ? OP_VSUBF32X4 : OP_VSUBI32X4; break;
		case HIR_EXPR_MUL:   op = plan->is_float ? OP_VMULF32X4 : OP_VMULI32X4; break;
		case HIR_EXPR_DIV:   op = OP_VDIVF32X4; break;
		case HIR_EXPR_AND:   op = OP_VAND128;   break;
		case HIR_EXPR_OR:    op = OP_VOR128;    break;
		default:             op = OP_VXOR128;   break;
		}

		if (l->type == HIR_EXPR_TERM && r->type == HIR_EXPR_TERM) {
			va = lir_vfor_term_vreg(plan, l);
			vb = lir_vfor_term_vreg(plan, r);
			if (va < 0 || vb < 0) {
				lir_fatal("SIMD: unplanned term.");
				return false;
			}
			/* dst is stack or an unread home: never == vb. */
			return lir_vfor_put3(op, 1, dst, 1, va, 1, vb, 1);
		}
		if (l->type != HIR_EXPR_TERM && r->type == HIR_EXPR_TERM) {
			/* Build the left side in dst, combine in place. */
			if (!lir_vfor_expr(plan, dst, sp, l))
				return false;
			vb = lir_vfor_term_vreg(plan, r);
			if (vb < 0) {
				lir_fatal("SIMD: unplanned term.");
				return false;
			}
			return lir_vfor_put3(op, 1, dst, 1, dst, 1, vb, 1);
		}
		if (l->type == HIR_EXPR_TERM && r->type != HIR_EXPR_TERM) {
			va = lir_vfor_term_vreg(plan, l);
			if (va < 0) {
				lir_fatal("SIMD: unplanned term.");
				return false;
			}
			if (commutative) {
				/* Build the right side in dst. */
				if (!lir_vfor_expr(plan, dst, sp, r))
					return false;
				return lir_vfor_put3(op, 1, dst, 1, dst,
						     1, va, 1);
			}
			/* SUB needs operand order: rhs into a slot. */
			if (sp >= VFOR_VREG_MAX) {
				lir_fatal("SIMD: vreg stack overflow.");
				return false;
			}
			if (!lir_vfor_expr(plan, sp, sp + 1, r))
				return false;
			return lir_vfor_put3(op, 1, dst, 1, va, 1, sp, 1);
		}
		/* Both non-term: left in dst, right in a slot. */
		if (!lir_vfor_expr(plan, dst, sp, l))
			return false;
		if (sp >= VFOR_VREG_MAX) {
			lir_fatal("SIMD: vreg stack overflow.");
			return false;
		}
		if (!lir_vfor_expr(plan, sp, sp + 1, r))
			return false;
		return lir_vfor_put3(op, 1, dst, 1, dst, 1, sp, 1);
	}
	default:
		lir_fatal("SIMD: unexpected vector expression.");
		return false;
	}
}

static bool
lir_visit_vfor_block(
	struct hir_block *block)
{
	struct vfor_plan plan;
	uint32_t loop_addr;
	uint32_t exit_patch_pos;
	int start_tmpvar, stop_tmpvar, cmp_tmpvar, guard_tmpvar;
	int scratch_tmpvar;
	struct hir_stmt *stmt;
	int i;

	assert(block->type == HIR_BLOCK_FOR);
	assert(block->val.for_.is_vector);
	assert(block->val.for_.inner != NULL);
	assert(block->val.for_.inner->type == HIR_BLOCK_BASIC);

	block->addr = (uint32_t)bytecode_top;

	memset(&plan, 0, sizeof(plan));
	plan.loop = block;
	plan.is_float = !block->val.for_.typed_int_region;
	plan.counter = block->val.for_.counter_symbol;
	plan.counter_tmpvar = lir_get_local_index(block, plan.counter);

	/* Collect the plan (mirror of the HIR-side budget check). */
	for (stmt = block->val.for_.inner->val.basic.stmt_list;
	     stmt != NULL; stmt = stmt->next) {
		if (stmt->lhs->type == HIR_EXPR_TERM) {
			const char *sym = stmt->lhs->val.term.term->val.symbol;
			bool have = false;
			for (i = 0; i < plan.temp_count; i++) {
				if (strcmp(plan.temp[i], sym) == 0) {
					have = true;
					break;
				}
			}
			if (!have) {
				if (plan.temp_count >= VFOR_MAX_LOCALS) {
					lir_fatal("SIMD: too many temps.");
					return false;
				}
				plan.temp[plan.temp_count++] = sym;
			}
			if (!lir_vfor_collect(&plan, stmt->rhs))
				return false;
		} else {
			/* PSTORE32/PSTOREF32: value expr only. */
			if (!lir_vfor_collect(&plan, stmt->rhs))
				return false;
		}
	}
	plan.stack_base = plan.const_count + plan.inv_count + plan.temp_count;
	if (plan.stack_base > VFOR_VREG_MAX) {
		lir_fatal("SIMD: vreg budget exceeded.");
		return false;
	}

	/* Evaluate start/stop once. */
	if (!lir_increment_tmpvar(&start_tmpvar))
		return false;
	if (!lir_visit_expr(start_tmpvar, block->val.for_.start, block))
		return false;
	if (!lir_increment_tmpvar(&stop_tmpvar))
		return false;
	if (!lir_visit_expr(stop_tmpvar, block->val.for_.stop, block))
		return false;

	/* Empty-range skip (mirrors the scalar for-range; dead in
	   practice because the strip guard proved lo < mid). */
	if (!lir_increment_tmpvar(&guard_tmpvar))
		return false;
	if (!lir_put_opcode(OP_GTE))
		return false;
	if (!lir_put_tmpvar((uint16_t)guard_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)start_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)stop_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFTRUE))
		return false;
	if (!lir_put_tmpvar((uint16_t)guard_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;
	lir_decrement_tmpvar(guard_tmpvar);

	/*
	 * Preheader: splat constants and invariant locals.  From here
	 * to the lane extraction there must be no helper call on the
	 * register-mapping backends (design 06, 5.6): only ICONST,
	 * ASSIGN, EQI/JMPIFEQ, INC, JMP and vector ops are emitted.
	 */
	if (plan.const_count > 0) {
		if (!lir_increment_tmpvar(&scratch_tmpvar))
			return false;
		for (i = 0; i < plan.const_count; i++) {
			if (!lir_put_opcode(plan.is_float ? OP_FCONST : OP_ICONST))
				return false;
			if (!lir_put_tmpvar((uint16_t)scratch_tmpvar))
				return false;
			if (!lir_put_imm32((uint32_t)plan.consts[i]))
				return false;
			if (!lir_vfor_put3(plan.is_float ? OP_VSPLATF32 : OP_VSPLATI32,
					   1, i,
					   0, scratch_tmpvar, 0, 0, 0))
				return false;
		}
		lir_decrement_tmpvar(scratch_tmpvar);
	}
	for (i = 0; i < plan.inv_count; i++) {
		int idx = lir_get_local_index(block, plan.inv[i]);
		if (!lir_vfor_put3(plan.is_float ? OP_VSPLATF32 : OP_VSPLATI32,
				   1, plan.const_count + i,
				   0, idx, 0, 0, 0))
			return false;
	}

	/* counter = start */
	if (!lir_put_opcode(OP_ASSIGN))
		return false;
	if (!lir_put_tmpvar((uint16_t)plan.counter_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)start_tmpvar))
		return false;

	/* Loop head: exit when counter == stop. */
	loop_addr = (uint32_t)bytecode_top;
	if (!lir_increment_tmpvar(&cmp_tmpvar))
		return false;
	if (!lir_put_opcode(OP_EQI))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)plan.counter_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)stop_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFEQ))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	/* Local forward label: patch after the back edge. */
	exit_patch_pos = (uint32_t)bytecode_top;
	if (!lir_put_u32(0xffffffff))
		return false;

	/* The vector body. */
	for (stmt = block->val.for_.inner->val.basic.stmt_list;
	     stmt != NULL; stmt = stmt->next) {
		if (stmt->lhs->type == HIR_EXPR_TERM) {
			int home = -1;
			const char *sym = stmt->lhs->val.term.term->val.symbol;
			for (i = 0; i < plan.temp_count; i++) {
				if (strcmp(plan.temp[i], sym) == 0) {
					home = plan.const_count +
						plan.inv_count + i;
					break;
				}
			}
			assert(home >= 0);
			if (lir_vfor_expr_reads(stmt->rhs, sym)) {
				/* Build in a stack slot: the home must
				   stay readable during evaluation. */
				if (!lir_vfor_expr(&plan, plan.stack_base,
						   plan.stack_base + 1,
						   stmt->rhs))
					return false;
				if (!lir_vfor_put3(OP_VMOV128, 1, home,
						   1, plan.stack_base,
						   0, 0, 0))
					return false;
			} else {
				if (!lir_vfor_expr(&plan, home,
						   plan.stack_base,
						   stmt->rhs))
					return false;
			}
		} else {
			/* PSTORE32/PSTOREF32(sb, counter) = expr */
			struct hir_expr *v = lir_vfor_strip_par(stmt->rhs);
			int vs;
			int base_tmpvar = lir_get_local_index(block,
				stmt->lhs->val.binary.expr[0]->val.term.term->val.symbol);
			if (v->type == HIR_EXPR_TERM) {
				vs = lir_vfor_term_vreg(&plan, v);
				if (vs < 0) {
					lir_fatal("SIMD: unplanned term.");
					return false;
				}
			} else {
				if (!lir_vfor_expr(&plan, plan.stack_base,
						   plan.stack_base + 1,
						   stmt->rhs))
					return false;
				vs = plan.stack_base;
			}
			if (!lir_vfor_put3(plan.is_float ? OP_VSTOREF32X4 : OP_VSTOREI32X4,
					   0, base_tmpvar,
					   0, plan.counter_tmpvar,
					   1, vs, 1))
				return false;
		}
	}

	/* i += 4 (OP_INC is inline on every backend that matters). */
	block->val.for_.inc_addr = (uint32_t)bytecode_top;
	block->cont_addr = (uint32_t)bytecode_top;
	for (i = 0; i < 4; i++) {
		if (!lir_put_opcode(OP_INC))
			return false;
		if (!lir_put_tmpvar((uint16_t)plan.counter_tmpvar))
			return false;
	}
	if (!lir_put_opcode(OP_JMP))
		return false;
	if (!lir_put_imm32(loop_addr))
		return false;

	/* exit label: extract each temp's lane 3 (= iteration mid-1,
	   the last executed strip iteration; the remainder loop
	   overwrites these when it runs at all). */
	{
		uint32_t addr = (uint32_t)bytecode_top;
		bytecode[exit_patch_pos] = (uint8_t)((addr >> 24) & 0xff);
		bytecode[exit_patch_pos + 1] = (uint8_t)((addr >> 16) & 0xff);
		bytecode[exit_patch_pos + 2] = (uint8_t)((addr >> 8) & 0xff);
		bytecode[exit_patch_pos + 3] = (uint8_t)(addr & 0xff);
	}
	for (i = 0; i < plan.temp_count; i++) {
		int idx = lir_get_local_index(block, plan.temp[i]);
		if (!lir_vfor_put3(plan.is_float ? OP_VGETLANEF32 : OP_VGETLANEI32,
				   0, idx,
				   1, plan.const_count + plan.inv_count + i,
				   1, 3, 1))
			return false;
	}

	lir_decrement_tmpvar(cmp_tmpvar);
	lir_decrement_tmpvar(stop_tmpvar);
	lir_decrement_tmpvar(start_tmpvar);

	return true;
}

static bool
lir_visit_for_range_block(
	struct hir_block *block)
{
	uint32_t loop_addr;
	int start_tmpvar, stop_tmpvar, loop_tmpvar, cmp_tmpvar, guard_tmpvar;
	struct hir_block *b;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_FOR);
	assert(block->val.for_.is_ranged);
	assert(block->val.for_.counter_symbol);
	assert(block->val.for_.start);
	assert(block->val.for_.stop);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Visit the start expr. */
	if (!lir_increment_tmpvar(&start_tmpvar))
		return false;
	if (!lir_visit_expr(start_tmpvar, block->val.for_.start, block))
		return false;

	/* Visit the stop expr. */
	if (!lir_increment_tmpvar(&stop_tmpvar))
		return false;
	if (!lir_visit_expr(stop_tmpvar, block->val.for_.stop, block))
		return false;

	/*
	 * Skip the whole loop when the range is empty.
	 *
	 * The per-iteration test below is an equality (OP_EQI/OP_JMPIFEQ,
	 * a pair the JIT backends fuse): the loop ends when the counter
	 * *reaches* the stop value. A range whose start is already past its
	 * stop never satisfies that, and the loop runs away. This is not an
	 * exotic case -- "for (i in 1..n)" is the ordinary way to write an
	 * insertion sort, and it becomes "1..0" every time the collection
	 * is empty.
	 *
	 * One comparison before the loop settles it, and the fused test in
	 * the body stays as it was.
	 */
	if (!lir_increment_tmpvar(&guard_tmpvar))
		return false;
	if (!lir_put_opcode(OP_GTE))
		return false;
	if (!lir_put_tmpvar((uint16_t)guard_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)start_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)stop_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFTRUE))
		return false;
	if (!lir_put_tmpvar((uint16_t)guard_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;
	lir_decrement_tmpvar(guard_tmpvar);

	/* Put the start value to a loop variable. */
	loop_tmpvar = lir_get_local_index(block, block->val.for_.counter_symbol);
	if (!lir_put_opcode(OP_ASSIGN))
		return false;
	if (!lir_put_tmpvar((uint16_t)loop_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)start_tmpvar))
		return false;

	/* Put a loop header. */
	loop_addr = (uint32_t)bytecode_top;
	if (!lir_increment_tmpvar(&cmp_tmpvar))
		return false;
	if (!lir_put_opcode(OP_EQI))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)loop_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)stop_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFEQ))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;

	/* Visit an inner block. */
	b = block->val.for_.inner;
	while (b != NULL) {
		if (!lir_visit_block(b))
			return false;
		if (b->stop)
			break;
		b = b->succ;
	}

	/* Store the incrementer address. A "continue" jumps here. */
	block->val.for_.inc_addr = (uint32_t)bytecode_top;
	block->cont_addr = (uint32_t)bytecode_top;

	/* Increment the loop variable. */
	if (!lir_put_opcode(OP_INC))
		return false;
	if (!lir_put_tmpvar((uint16_t)loop_tmpvar))
		return false;

	/* Put a back-edge jump. */
	if (!lir_put_opcode(OP_JMP))
		return false;
	if (!lir_put_imm32(loop_addr))
		return false;

	lir_decrement_tmpvar(cmp_tmpvar);
	lir_decrement_tmpvar(stop_tmpvar);
	lir_decrement_tmpvar(start_tmpvar);

	return true;
}

static bool
lir_visit_for_kv_block(
	struct hir_block *block)
{
	uint32_t loop_addr;
	int col_tmpvar, size_tmpvar, i_tmpvar, key_tmpvar, val_tmpvar, cmp_tmpvar;
	struct hir_block *b;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_FOR);
	assert(!block->val.for_.is_ranged);
	assert(block->val.for_.key_symbol != NULL);
	assert(block->val.for_.value_symbol != NULL);
	assert(block->val.for_.collection != NULL);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Visit a collection expr. */
	if (!lir_increment_tmpvar(&col_tmpvar))
		return false;
	if (!lir_visit_expr(col_tmpvar, block->val.for_.collection, block))
		return false;

	/* Get a collection size. */
	if (!lir_increment_tmpvar(&size_tmpvar))
		return false;
	if (!lir_put_opcode(OP_LEN))
		return false;
	if (!lir_put_tmpvar((uint16_t)size_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)col_tmpvar))
		return false;

	/* Assign 0 to `i`. */
	if (!lir_increment_tmpvar(&i_tmpvar))
		return false;
	if (!lir_put_opcode(OP_ICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_imm32(0))
		return false;

	/* Prepare a key and a value. */
	key_tmpvar = lir_get_local_index(block, block->val.for_.key_symbol);
	val_tmpvar = lir_get_local_index(block, block->val.for_.value_symbol);
	if (!lir_increment_tmpvar(&cmp_tmpvar))
		return false;

	/* Put a loop header. */
	loop_addr = (uint32_t)bytecode_top;		/* LOOP: */
	if (!lir_put_opcode(OP_EQI)) 			/*  if i == size then break */
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)size_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFEQ)) 		/*  if i == size then break */
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;
	if (!lir_put_opcode(OP_GETDICTKEYBYINDEX))	/* key = dict.getKeyByIndex(i) */
		return false;
	if (!lir_put_tmpvar((uint16_t)key_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)col_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_opcode(OP_GETDICTVALBYINDEX)) 	/* val = dict.getValByIndex(i) */
		return false;
	if (!lir_put_tmpvar((uint16_t)val_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)col_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_opcode(OP_INC)) 		/* i++ */
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;

	/*
	 * A "continue" jumps to the loop head: the cursor has already
	 * been advanced above, before the body runs.
	 */
	block->cont_addr = loop_addr;

	/* Visit an inner block. */
	b = block->val.for_.inner;
	while (b != NULL) {
		if (!lir_visit_block(b))
			return false;
		if (b->stop)
			break;
		b = b->succ;
	}

	/* Put a back-edge jump. */
	if (!lir_put_opcode(OP_JMP))
		return false;
	if (!lir_put_imm32(loop_addr))
		return false;

	lir_decrement_tmpvar(cmp_tmpvar);
	lir_decrement_tmpvar(i_tmpvar);
	lir_decrement_tmpvar(size_tmpvar);
	lir_decrement_tmpvar(col_tmpvar);

	return true;
}

static bool
lir_visit_for_v_block(
	struct hir_block *block)
{
	uint32_t loop_addr;
	int arr_tmpvar, size_tmpvar, i_tmpvar, val_tmpvar, cmp_tmpvar;
	struct hir_block *b;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_FOR);
	assert(!block->val.for_.is_ranged);
	assert(block->val.for_.value_symbol != NULL);
	assert(block->val.for_.collection != NULL);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Visit an array expr. */
	if (!lir_increment_tmpvar(&arr_tmpvar))
		return false;
	if (!lir_visit_expr(arr_tmpvar, block->val.for_.collection, block))
		return false;

	/* Get a collection size. */
	if (!lir_increment_tmpvar(&size_tmpvar))
		return false;
	if (!lir_put_opcode(OP_LEN))
		return false;
	if (!lir_put_tmpvar((uint16_t)size_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)arr_tmpvar))
		return false;

	/* Assign 0 to `i`. */
	if (!lir_increment_tmpvar(&i_tmpvar))
		return false;
	if (!lir_put_opcode(OP_ICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_imm32(0))
		return false;

	/* Prepare a value. */
	val_tmpvar = lir_get_local_index(block, block->val.for_.value_symbol);
	if (!lir_increment_tmpvar(&cmp_tmpvar))
		return false;

	/* Put a loop header. */
	loop_addr = (uint32_t)bytecode_top;		/* LOOP: */
	if (!lir_put_opcode(OP_EQI)) 			/*  if i == size then break */
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)size_tmpvar))
		return false;
	if (!lir_put_opcode(OP_JMPIFEQ))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;
	if (!lir_put_opcode(OP_LOADARRAY)) 	/* val = array[i] */
		return false;
	if (!lir_put_tmpvar((uint16_t)val_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)arr_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;
	if (!lir_put_opcode(OP_INC)) 		/* i++ */
		return false;
	if (!lir_put_tmpvar((uint16_t)i_tmpvar))
		return false;

	/*
	 * A "continue" jumps to the loop head: the cursor has already
	 * been advanced above, before the body runs.
	 */
	block->cont_addr = loop_addr;

	/* Visit an inner block. */
	b = block->val.for_.inner;
	while (b != NULL) {
		if (!lir_visit_block(b))
			return false;
		if (b->stop)
			break;
		b = b->succ;
	}

	/* Put a back-edge jump. */
	if (!lir_put_opcode(OP_JMP))
		return false;
	if (!lir_put_imm32(loop_addr))
		return false;

	lir_decrement_tmpvar(cmp_tmpvar);
	lir_decrement_tmpvar(i_tmpvar);
	lir_decrement_tmpvar(size_tmpvar);
	lir_decrement_tmpvar(arr_tmpvar);

	return true;
}

/* Check whether LHS is local. */
static int
lir_get_local_index(
	struct hir_block *block,
	const char *symbol)
{
	struct hir_block *func;
	struct hir_local *local;

	/* Get a root func block. */
	func = block;
	while (func->type != HIR_BLOCK_FUNC)
		func = func->parent;

	/* Search in an explicit local variable list. */
	local = func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, symbol) == 0)
			break;
		local = local->next;
	}
	assert(local != NULL);

	return local->index;
}

static bool
lir_visit_while_block(
	struct hir_block *block)
{
	uint32_t loop_addr;
	int cmp_tmpvar;
	struct hir_block *b;

	assert(block != NULL);
	assert(block->type == HIR_BLOCK_WHILE);

	/* Store the block address. */
	block->addr = (uint32_t)bytecode_top;

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)block->line))
			return false;
	}

	/* Put a loop header. */
	loop_addr = (uint32_t)bytecode_top;
	if (!lir_increment_tmpvar(&cmp_tmpvar))
		return false;
	if (!lir_visit_expr(cmp_tmpvar, block->val.while_.cond, block))
		return false;
	if (!lir_put_opcode(OP_JMPIFFALSE))
		return false;
	if (!lir_put_tmpvar((uint16_t)cmp_tmpvar))
		return false;
	if (!lir_put_branch_addr(block->succ))
		return false;
	lir_decrement_tmpvar(cmp_tmpvar);

	/* A "continue" jumps to the loop head, re-testing the condition. */
	block->cont_addr = loop_addr;

	/* Visit an inner block. */
	b = block->val.while_.inner;
	while (b != NULL) {
		if (!lir_visit_block(b))
			return false;
		if (b->stop)
			break;
		b = b->succ;
	}

	/* Put a back-edge jump. */
	if (!lir_put_opcode(OP_JMP))
		return false;
	if (!lir_put_imm32(loop_addr))
		return false;

	return true;
}

static bool
lir_visit_stmt(
	struct hir_block *parent,
	struct hir_stmt *stmt)
{
	int rhs_tmpvar, obj_tmpvar, access_tmpvar;
	bool is_lhs_local;

	assert(stmt != NULL);
	assert(stmt->rhs != NULL);

	/* Put a line number. */
	if (lir_optimize_level == 0) {
		if (!lir_put_opcode(OP_LINEINFO))
			return false;
		if (!lir_put_imm32((uint32_t)stmt->line))
			return false;
	}

	/* Check whether LHS is a local variable. */
	is_lhs_local = lir_check_lhs_local(parent, stmt->lhs, &rhs_tmpvar);

	/* Prepare a tmpvar for RHS if LHS is not an explicit local variable. */
	if (!is_lhs_local) {
		if (!lir_increment_tmpvar(&rhs_tmpvar))
			return false;
	}

	/* Visit RHS. */
	if (!lir_visit_expr(rhs_tmpvar, stmt->rhs, parent))
		return false;

	/* Visit LHS if LHS is not an explicit local variable. */
	if (stmt->lhs != NULL && !is_lhs_local) {
		if (stmt->lhs->type == HIR_EXPR_TERM) {
			assert(stmt->lhs->val.term.term->type == HIR_TERM_SYMBOL);

			/* Put a storesymbol. */
			if (!lir_put_opcode(OP_STORESYMBOL))
				return false;
			if (!lir_put_string(stmt->lhs->val.term.term->val.symbol))
				return false;
			if (!lir_put_tmpvar((uint16_t)rhs_tmpvar))
				return false;
		} else if (stmt->lhs->type == HIR_EXPR_SUBSCR) {
			assert(stmt->lhs->val.binary.expr[0] != NULL);
			assert(stmt->lhs->val.binary.expr[1] != NULL);

			/* Visit an array. */
			if (!lir_increment_tmpvar(&obj_tmpvar))
				return false;
			if (!lir_visit_expr(obj_tmpvar, stmt->lhs->val.binary.expr[0], parent))
				return false;

			/* Visit a subscript. */
			if (!lir_increment_tmpvar(&access_tmpvar))
				return false;
			if (!lir_visit_expr(access_tmpvar, stmt->lhs->val.binary.expr[1], parent))
				return false;

			/* Put a store. */
			if (!lir_put_opcode(OP_STOREARRAY))
				return false;
			if (!lir_put_tmpvar((uint16_t)obj_tmpvar))
				return false;
			if (!lir_put_tmpvar((uint16_t)access_tmpvar))
				return false;
			if (!lir_put_tmpvar((uint16_t)rhs_tmpvar))
				return false;

			lir_decrement_tmpvar(access_tmpvar);
			lir_decrement_tmpvar(obj_tmpvar);
		} else if (stmt->lhs->type == HIR_EXPR_PSTORE8 ||
			   stmt->lhs->type == HIR_EXPR_PSTORE16 ||
			   stmt->lhs->type == HIR_EXPR_PSTORE32 ||
			   stmt->lhs->type == HIR_EXPR_PSTORE64 ||
			   stmt->lhs->type == HIR_EXPR_PSTOREF32) {
			assert(stmt->lhs->val.binary.expr[0] != NULL);
			assert(stmt->lhs->val.binary.expr[1] != NULL);

			/* Visit the base address. */
			if (!lir_increment_tmpvar(&obj_tmpvar))
				return false;
			if (!lir_visit_expr(obj_tmpvar, stmt->lhs->val.binary.expr[0], parent))
				return false;

			/* Visit the offset. */
			if (!lir_increment_tmpvar(&access_tmpvar))
				return false;
			if (!lir_visit_expr(access_tmpvar, stmt->lhs->val.binary.expr[1], parent))
				return false;

			/* Put a raw store. */
			{
				int pst;
				switch (stmt->lhs->type) {
				case HIR_EXPR_PSTORE16: pst = OP_PSTORE16; break;
				case HIR_EXPR_PSTORE32: pst = OP_PSTORE32; break;
				case HIR_EXPR_PSTORE64: pst = OP_PSTORE64; break;
				case HIR_EXPR_PSTOREF32: pst = OP_PSTOREF32; break;
				default:                pst = OP_PSTORE8;  break;
				}
				if (!lir_put_opcode((uint8_t)pst))
					return false;
			}
			if (!lir_put_tmpvar((uint16_t)obj_tmpvar))
				return false;
			if (!lir_put_tmpvar((uint16_t)access_tmpvar))
				return false;
			if (!lir_put_tmpvar((uint16_t)rhs_tmpvar))
				return false;

			lir_decrement_tmpvar(access_tmpvar);
			lir_decrement_tmpvar(obj_tmpvar);
		} else if (stmt->lhs->type == HIR_EXPR_DOT) {
			assert(stmt->lhs->val.dot.obj != NULL);
			assert(stmt->lhs->val.dot.symbol != NULL);

			/* Visit an object. */
			if (!lir_increment_tmpvar(&obj_tmpvar))
				return false;
			if (!lir_visit_expr(obj_tmpvar, stmt->lhs->val.dot.obj, parent))
				return false;

			/* Put a store. */
			if (!lir_put_opcode(OP_STOREDOT))
				return false;
			if (!lir_put_tmpvar((uint16_t)obj_tmpvar))
				return false;
			if (!lir_put_string(stmt->lhs->val.dot.symbol))
				return false;
			if (!lir_put_tmpvar((uint16_t)rhs_tmpvar))
				return false;

			lir_decrement_tmpvar(obj_tmpvar);
		} else {
			lir_fatal(N_TR("LHS is not a term or an array element."));
			return false;
		}
	}

	if (!is_lhs_local)
		lir_decrement_tmpvar(rhs_tmpvar);

	return true;
}

/* Check whether LHS is local variable. */
static bool
lir_check_lhs_local(
	struct hir_block *block,
	struct hir_expr *lhs,
	int *rhs_tmpvar)
{
	struct hir_block *func;
	struct hir_local *local;
	const char * symbol;

	/* Exclude non symbol term LHS. */
	if (lhs == NULL)
		return false;
	if (lhs->type != HIR_EXPR_TERM)
		return false;
	if (lhs->val.term.term->type != HIR_TERM_SYMBOL)
		return false;

	/* Get a symbol. */
	symbol = lhs->val.term.term->val.symbol;

	/* Check for a return value. */
	if (strcmp(symbol, "$return") == 0) {
		*rhs_tmpvar = 0;
		return true;
	}

	/* Get a root func block. */
	func = block->parent;
	while (func->type != HIR_BLOCK_FUNC)
		func = func->parent;

	/* Search in the local variable list. */
	local = func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, symbol) == 0)
			break;
		local = local->next;
	}
	if (local == NULL)
		return false;

	/* Use a tmpvar index for the local variable. */
	*rhs_tmpvar = local->index;

	return true;
}

static bool
lir_visit_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	assert(expr != NULL);

	switch (expr->type) {
	case HIR_EXPR_TERM:
		/* Visit a term inside the expr. */
		if (!lir_visit_term(dst_tmpvar, expr->val.term.term, block))
			return false;
		break;
	case HIR_EXPR_PAR:
		/* Visit an expr inside the expr. */
		if (!lir_visit_expr(dst_tmpvar, expr->val.unary.expr, block))
			return false;
		break;
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
		/* For the unary operators. */
		if (!lir_visit_unary_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_LT:
	case HIR_EXPR_LTE:
	case HIR_EXPR_GT:
	case HIR_EXPR_GTE:
	case HIR_EXPR_EQ:
	case HIR_EXPR_NEQ:
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_MOD:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
	case HIR_EXPR_SUBSCR:
	case HIR_EXPR_PLOAD8U:
	case HIR_EXPR_PLOAD8S:
	case HIR_EXPR_PLOAD16U:
	case HIR_EXPR_PLOAD16S:
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOAD64:
	case HIR_EXPR_PLOADF32:
		/* For the binary operators. */
		if (!lir_visit_binary_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_PBASE:
	case HIR_EXPR_PLEN:
		/* ABCE unary ops. */
		if (!lir_visit_abce_unary_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_PCHECK:
	case HIR_EXPR_TYPEIS:
		/* ABCE type-test ops. (Operand 2 is an int-constant imm8.) */
		if (!lir_visit_abce_typetest_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_LAND:
	case HIR_EXPR_LOR:
		/* For the short-circuiting logical operators. */
		if (!lir_visit_logical_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_DOT:
		/* For the dot operator. */
		if (!lir_visit_dot_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_CAPTURE:
		/* For the CSE capture operator. */
		if (!lir_visit_capture_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_CALL:
		/* For a function call. */
		if (!lir_visit_call_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_THISCALL:
		/* For a method call. */
		if (!lir_visit_thiscall_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_ARRAY:
		/* For an array expression. */
		if (!lir_visit_array_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_DICT:
		/* For a dictionary expression. */
		if (!lir_visit_dict_expr(dst_tmpvar, expr, block))
			return false;
		break;
	case HIR_EXPR_NEW:
		/* For a new expression. */
		if (!lir_visit_new_expr(dst_tmpvar, expr, block))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		abort();
		break;
	}

	return true;
}

static bool
lir_visit_unary_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int opr_tmpvar;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_NEG || expr->type == HIR_EXPR_NOT);

	/* Visit the operand expr. */
	if (!lir_increment_tmpvar(&opr_tmpvar))
		return false;
	if (!lir_visit_expr(opr_tmpvar, expr->val.unary.expr, block))
		return false;

	/* Put an opcode. */
	switch (expr->type) {
	case HIR_EXPR_NEG:
		if (!lir_put_opcode(OP_NEG))
			return false;
		if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)opr_tmpvar))
			return false;
		break;
	case HIR_EXPR_NOT:
		if (!lir_put_opcode(OP_NOT))
			return false;
		if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)opr_tmpvar))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	lir_decrement_tmpvar(opr_tmpvar);

	return true;
}

/*
 * Lower a short-circuiting logical operator (&& or ||).
 *
 * The result is a boolean 1 or 0, and the second operand is evaluated
 * only when the first has not already decided the result. Jump targets
 * inside the expression are backpatched here, because the block-level
 * patch table only handles jumps to whole HIR blocks.
 *
 *   &&:  eval a; if false -> Lzero
 *        eval b; if false -> Lzero
 *        dst = 1; jmp Lend
 *   Lzero: dst = 0
 *   Lend:
 *
 *   ||:  eval a; if true -> Lone
 *        eval b; if true -> Lone
 *        dst = 0; jmp Lend
 *   Lone: dst = 1
 *   Lend:
 */
static void
lir_patch_u32(uint32_t at, uint32_t value)
{
	bytecode[at]     = (uint8_t)((value >> 24) & 0xff);
	bytecode[at + 1] = (uint8_t)((value >> 16) & 0xff);
	bytecode[at + 2] = (uint8_t)((value >> 8) & 0xff);
	bytecode[at + 3] = (uint8_t)(value & 0xff);
}

static bool
lir_visit_logical_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int cond_tmpvar;
	bool is_and;
	uint8_t short_op;
	uint32_t patch0, patch1, patch_skip, decided_addr, end_addr;

	is_and = (expr->type == HIR_EXPR_LAND);
	/* && short-circuits on a false operand, || on a true one. */
	short_op = is_and ? OP_JMPIFFALSE : OP_JMPIFTRUE;

	if (!lir_increment_tmpvar(&cond_tmpvar))
		return false;

	/* Operand 0. */
	if (!lir_visit_expr(cond_tmpvar, expr->val.binary.expr[0], block))
		return false;
	if (!lir_put_opcode(short_op))
		return false;
	if (!lir_put_tmpvar((uint16_t)cond_tmpvar))
		return false;
	patch0 = bytecode_top;
	if (!lir_put_u32(0))
		return false;

	/* Operand 1. */
	if (!lir_visit_expr(cond_tmpvar, expr->val.binary.expr[1], block))
		return false;
	if (!lir_put_opcode(short_op))
		return false;
	if (!lir_put_tmpvar((uint16_t)cond_tmpvar))
		return false;
	patch1 = bytecode_top;
	if (!lir_put_u32(0))
		return false;

	/* Neither operand triggered the short circuit: the "not decided"
	 * result (0 for &&, 1 for ||). */
	if (!lir_put_opcode(OP_ICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm32(is_and ? 1u : 0u))
		return false;
	if (!lir_put_opcode(OP_JMP))
		return false;
	patch_skip = bytecode_top;
	if (!lir_put_u32(0))
		return false;

	/* The short-circuit target: the decided result. */
	decided_addr = bytecode_top;
	if (!lir_put_opcode(OP_ICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm32(is_and ? 0u : 1u))
		return false;

	end_addr = bytecode_top;

	lir_patch_u32(patch0, decided_addr);
	lir_patch_u32(patch1, decided_addr);
	lir_patch_u32(patch_skip, end_addr);

	lir_decrement_tmpvar(cond_tmpvar);

	return true;
}

/* Visit an ABCE unary expr (PBASE / PLEN). */
static bool
lir_visit_abce_unary_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int opr_tmpvar;
	int opcode;

	assert(expr != NULL);

	/* Visit the operand expr. */
	if (!lir_increment_tmpvar(&opr_tmpvar))
		return false;
	if (!lir_visit_expr(opr_tmpvar, expr->val.unary.expr, block))
		return false;

	opcode = (expr->type == HIR_EXPR_PBASE) ? OP_PBASE : OP_PLEN;
	if (!lir_put_opcode((uint8_t)opcode))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)opr_tmpvar))
		return false;

	lir_decrement_tmpvar(opr_tmpvar);

	return true;
}

/* Visit an ABCE type-test expr (PCHECK / TYPEIS). */
static bool
lir_visit_abce_typetest_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int opr_tmpvar;
	int opcode;
	int imm;

	assert(expr != NULL);
	assert(expr->val.binary.expr[1]->type == HIR_EXPR_TERM);
	assert(expr->val.binary.expr[1]->val.term.term->type == HIR_TERM_INT);

	/* Visit the value expr. */
	if (!lir_increment_tmpvar(&opr_tmpvar))
		return false;
	if (!lir_visit_expr(opr_tmpvar, expr->val.binary.expr[0], block))
		return false;

	/* The type constant. */
	imm = expr->val.binary.expr[1]->val.term.term->val.i;

	opcode = (expr->type == HIR_EXPR_PCHECK) ? OP_PCHECK : OP_TYPEIS;
	if (!lir_put_opcode((uint8_t)opcode))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)opr_tmpvar))
		return false;
	if (!lir_put_imm8((uint8_t)imm))
		return false;

	lir_decrement_tmpvar(opr_tmpvar);

	return true;
}

/*
 * Typed-op emission (docs/design/07-typed-ops.md).
 *
 * lir_expr_proven_type() answers "what tag does this expression
 * provably carry at runtime?" using only local, already-computed
 * facts: literals, annotated parameters (sound because OP_CHECKTYPE
 * runs at level >= 2), ABCE typed-int regions (Stage A), and the
 * closure over arithmetic.  TYPED_UNKNOWN is always sound.
 */

#define TYPED_UNKNOWN	(-1)
#define TYPED_INT	NOCT_VALUE_INT
#define TYPED_FLOAT	NOCT_VALUE_FLOAT

static int
lir_symbol_proven_type(
	const char *symbol,
	struct hir_block *block)
{
	struct hir_block *func;
	struct hir_block *b;
	struct hir_local *local;
	bool in_region;

	/* Stage A: an enclosing ABCE fast loop proves int for every
	   local/param read under it (globals cannot appear there, but
	   the local-list check below keeps this safe regardless). */
	in_region = false;
	b = block;
	while (b != NULL && b->type != HIR_BLOCK_FUNC) {
		if (b->type == HIR_BLOCK_FOR && b->val.for_.typed_int_region)
			in_region = true;
		b = b->parent;
	}
	if (b == NULL)
		return TYPED_UNKNOWN;
	func = b;

	/* Globals prove nothing. */
	local = func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, symbol) == 0)
			break;
		local = local->next;
	}
	if (local == NULL)
		return TYPED_UNKNOWN;

	if (in_region)
		return TYPED_INT;

	/*
	 * Stage B lattice proof (rule 5b), computed by
	 * hir_opt_typed_func().  This subsumes the plain "annotated
	 * parameter" rule 5a: the lattice seeds parameters from their
	 * (CHECKTYPE-backed) annotations and then meets every body
	 * assignment in, so a reassigned parameter correctly loses
	 * its proof -- consulting the annotation directly here would
	 * be unsound (found in review: "func f(a: int) { a = 1.5;
	 * ... }" changes a's runtime tag after the entry check).
	 * Without the optimizer, proven_type stays -1 and no typed op
	 * is ever emitted.
	 */
	if (local->proven_type == NOCT_VALUE_INT)
		return TYPED_INT;
	if (local->proven_type == NOCT_VALUE_FLOAT)
		return TYPED_FLOAT;

	return TYPED_UNKNOWN;
}

static int
lir_expr_proven_type(
	struct hir_expr *expr,
	struct hir_block *block)
{
	int a, b;

	switch (expr->type) {
	case HIR_EXPR_TERM:
		switch (expr->val.term.term->type) {
		case HIR_TERM_INT:
			return TYPED_INT;
		case HIR_TERM_FLOAT:
			return TYPED_FLOAT;
		case HIR_TERM_SYMBOL:
			return lir_symbol_proven_type(
				expr->val.term.term->val.symbol, block);
		default:
			return TYPED_UNKNOWN;
		}
	case HIR_EXPR_PAR:
		return lir_expr_proven_type(expr->val.unary.expr, block);
	case HIR_EXPR_CAPTURE:
		/* Yields the inner expression's value. */
		return lir_expr_proven_type(expr->val.capture.expr, block);
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
		a = lir_expr_proven_type(expr->val.binary.expr[0], block);
		b = lir_expr_proven_type(expr->val.binary.expr[1], block);
		if (a == TYPED_INT && b == TYPED_INT)
			return TYPED_INT;
		if (a == TYPED_FLOAT && b == TYPED_FLOAT)
			return TYPED_FLOAT;
		return TYPED_UNKNOWN;
	case HIR_EXPR_MOD:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		a = lir_expr_proven_type(expr->val.binary.expr[0], block);
		b = lir_expr_proven_type(expr->val.binary.expr[1], block);
		if (a == TYPED_INT && b == TYPED_INT)
			return TYPED_INT;
		return TYPED_UNKNOWN;
	case HIR_EXPR_LT:
	case HIR_EXPR_LTE:
	case HIR_EXPR_GT:
	case HIR_EXPR_GTE:
	case HIR_EXPR_EQ:
	case HIR_EXPR_NEQ:
		/* Comparisons yield an int 0/1 when they yield at all;
		   conservatively require proven operands (rule 3). */
		a = lir_expr_proven_type(expr->val.binary.expr[0], block);
		b = lir_expr_proven_type(expr->val.binary.expr[1], block);
		if (a != TYPED_UNKNOWN && b != TYPED_UNKNOWN)
			return TYPED_INT;
		return TYPED_UNKNOWN;
	case HIR_EXPR_PLOAD8U:
	case HIR_EXPR_PLOAD8S:
	case HIR_EXPR_PLOAD16U:
	case HIR_EXPR_PLOAD16S:
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLEN:
	case HIR_EXPR_PCHECK:
	case HIR_EXPR_TYPEIS:
		return TYPED_INT;
	case HIR_EXPR_PLOADF32:
		return TYPED_FLOAT;
	default:
		/* PLOAD64/PBASE (long), NEG, NOT, LAND, LOR, DOT,
		   SUBSCR, CALL, ... */
		return TYPED_UNKNOWN;
	}
}

/*
 * Pick a typed opcode for a binary expression, or -1 for the generic
 * path.  Never called for SHL/SHR (they have a different operand
 * shape and are handled inline in lir_visit_binary_expr).
 */
static int
lir_typed_binary_opcode(
	struct hir_expr *expr,
	struct hir_block *block)
{
	int a, b;
	struct hir_expr *rhs;

	if (lir_optimize_level < 2 || typed_disabled)
		return -1;

	switch (expr->type) {
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_MOD:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_LT:
	case HIR_EXPR_LTE:
	case HIR_EXPR_GT:
	case HIR_EXPR_GTE:
		break;
	default:
		return -1;
	}

	a = lir_expr_proven_type(expr->val.binary.expr[0], block);
	b = lir_expr_proven_type(expr->val.binary.expr[1], block);

	if (a == TYPED_INT && b == TYPED_INT) {
		switch (expr->type) {
		case HIR_EXPR_PLUS:	return OP_IADD;
		case HIR_EXPR_MINUS:	return OP_ISUB;
		case HIR_EXPR_MUL:	return OP_IMUL;
		case HIR_EXPR_DIV:
		case HIR_EXPR_MOD:
			/* Only statically error-free divisions: a
			   literal divisor outside {0, -1} (D-TOP5). */
			rhs = expr->val.binary.expr[1];
			if (rhs->type != HIR_EXPR_TERM ||
			    rhs->val.term.term->type != HIR_TERM_INT)
				return -1;
			if (rhs->val.term.term->val.i == 0 ||
			    rhs->val.term.term->val.i == -1)
				return -1;
			return expr->type == HIR_EXPR_DIV ? OP_IDIV : OP_IMOD;
		case HIR_EXPR_AND:	return OP_IAND;
		case HIR_EXPR_OR:	return OP_IOR;
		case HIR_EXPR_XOR:	return OP_IXOR;
		case HIR_EXPR_LT:	return OP_ILT;
		case HIR_EXPR_LTE:	return OP_ILTE;
		case HIR_EXPR_GT:	return OP_IGT;
		case HIR_EXPR_GTE:	return OP_IGTE;
		default:		return -1;
		}
	}
	if (a == TYPED_FLOAT && b == TYPED_FLOAT) {
		switch (expr->type) {
		case HIR_EXPR_PLUS:	return OP_FADD;
		case HIR_EXPR_MINUS:	return OP_FSUB;
		case HIR_EXPR_MUL:	return OP_FMUL;
		case HIR_EXPR_DIV:
			/* IEEE-total after 07 Part 0: no divisor rule. */
			return OP_FDIV;
		case HIR_EXPR_LT:	return OP_FLT;
		case HIR_EXPR_LTE:	return OP_FLTE;
		case HIR_EXPR_GT:	return OP_FGT;
		case HIR_EXPR_GTE:	return OP_FGTE;
		default:		return -1;
		}
	}

	return -1;
}

static bool
lir_visit_binary_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int opr1_tmpvar, opr2_tmpvar;
	int opcode;

	assert(expr != NULL);

	/*
	 * Typed shifts (design 07): different operand shape -- the
	 * shift count is a compile-time immediate in operand 3, so the
	 * count operand is never materialized.  Requirements: proven
	 * int lhs and an int literal count in [0, 31] (an out-of-range
	 * literal stays generic and errors at runtime, unchanged).
	 */
	if ((expr->type == HIR_EXPR_SHL || expr->type == HIR_EXPR_SHR) &&
	    lir_optimize_level >= 2 && !typed_disabled) {
		struct hir_expr *rhs = expr->val.binary.expr[1];
		if (rhs->type == HIR_EXPR_TERM &&
		    rhs->val.term.term->type == HIR_TERM_INT &&
		    rhs->val.term.term->val.i >= 0 &&
		    rhs->val.term.term->val.i <= 31 &&
		    lir_expr_proven_type(expr->val.binary.expr[0], block) == TYPED_INT) {
			if (!lir_increment_tmpvar(&opr1_tmpvar))
				return false;
			if (!lir_visit_expr(opr1_tmpvar, expr->val.binary.expr[0], block))
				return false;
			if (!lir_put_opcode((uint8_t)(expr->type == HIR_EXPR_SHL ?
						      OP_ISHL : OP_ISHR)))
				return false;
			if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
				return false;
			if (!lir_put_tmpvar((uint16_t)opr1_tmpvar))
				return false;
			/* The count is an imm8, NOT a tmpvar: the
			   operand validators reject tmpvar indices
			   beyond the frame size. */
			if (!lir_put_imm8((uint8_t)rhs->val.term.term->val.i))
				return false;
			lir_decrement_tmpvar(opr1_tmpvar);
			typed_emit_int_count++;
			return true;
		}
	}

	/* Visit the operand1 expr. */
	if (!lir_increment_tmpvar(&opr1_tmpvar))
		return false;
	if (!lir_visit_expr(opr1_tmpvar, expr->val.binary.expr[0], block))
		return false;

	/* Visit the operand2 expr. */
	if (!lir_increment_tmpvar(&opr2_tmpvar))
		return false;
	if (!lir_visit_expr(opr2_tmpvar, expr->val.binary.expr[1], block))
		return false;

	/* Typed arithmetic (design 07): same operand shape as the
	   generic ops, so only the opcode differs. */
	opcode = lir_typed_binary_opcode(expr, block);
	if (opcode >= 0) {
		if (opcode >= OP_FADD)
			typed_emit_float_count++;
		else
			typed_emit_int_count++;
		goto put;
	}

	switch (expr->type) {
	case HIR_EXPR_LT:
	case HIR_EXPR_LTE:
	case HIR_EXPR_GT:
	case HIR_EXPR_GTE:
	case HIR_EXPR_EQ:
	case HIR_EXPR_NEQ:
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_MOD:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		if (lir_optimize_level >= 2 && !typed_disabled)
			typed_generic_count++;
		break;
	default:
		break;
	}

	/* Put an opcode. */
	switch (expr->type) {
	case HIR_EXPR_LT:
		opcode = OP_LT;
		break;
	case HIR_EXPR_LTE:
		opcode = OP_LTE;
		break;
	case HIR_EXPR_EQ:
		opcode = OP_EQ;
		break;
	case HIR_EXPR_NEQ:
		opcode = OP_NEQ;
		break;
	case HIR_EXPR_GTE:
		opcode = OP_GTE;
		break;
	case HIR_EXPR_GT:
		opcode = OP_GT;
		break;
	case HIR_EXPR_PLUS:
		opcode = OP_ADD;
		break;
	case HIR_EXPR_MINUS:
		opcode = OP_SUB;
		break;
	case HIR_EXPR_MUL:
		opcode = OP_MUL;
		break;
	case HIR_EXPR_DIV:
		opcode = OP_DIV;
		break;
	case HIR_EXPR_MOD:
		opcode = OP_MOD;
		break;
	case HIR_EXPR_AND:
		opcode = OP_AND;
		break;
	case HIR_EXPR_OR:
		opcode = OP_OR;
		break;
	case HIR_EXPR_XOR:
		opcode = OP_XOR;
		break;
	case HIR_EXPR_SHL:
		opcode = OP_SHL;
		break;
	case HIR_EXPR_SHR:
		opcode = OP_SHR;
		break;
	case HIR_EXPR_SUBSCR:
		opcode = OP_LOADARRAY;
		break;
	case HIR_EXPR_PLOAD8U:
		opcode = OP_PLOAD8U;
		break;
	case HIR_EXPR_PLOAD8S:
		opcode = OP_PLOAD8S;
		break;
	case HIR_EXPR_PLOAD16U:
		opcode = OP_PLOAD16U;
		break;
	case HIR_EXPR_PLOAD16S:
		opcode = OP_PLOAD16S;
		break;
	case HIR_EXPR_PLOAD32:
		opcode = OP_PLOAD32;
		break;
	case HIR_EXPR_PLOAD64:
		opcode = OP_PLOAD64;
		break;
	case HIR_EXPR_PLOADF32:
		opcode = OP_PLOADF32;
		break;
	default:
		opcode = -1;
		assert(NEVER_COME_HERE);
		break;
	}

put:
	if (!lir_put_opcode((uint8_t)opcode))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)opr1_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)opr2_tmpvar))
		return false;

	lir_decrement_tmpvar(opr2_tmpvar);
	lir_decrement_tmpvar(opr1_tmpvar);

	return true;
}

static bool
lir_visit_dot_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int opr_tmpvar;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_DOT);
	assert(expr->val.dot.obj != NULL);
	assert(expr->val.dot.symbol != NULL);

	/* Visit the operand expr. */
	if (!lir_increment_tmpvar(&opr_tmpvar))
		return false;
	if (!lir_visit_expr(opr_tmpvar, expr->val.dot.obj, block))
		return false;

	/* Put a bytecode sequence. */
	if (!lir_put_opcode(OP_LOADDOT))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)opr_tmpvar))
		return false;
	if (!lir_put_string(expr->val.dot.symbol))
		return false;

	lir_decrement_tmpvar(opr_tmpvar);

	return true;
}

/*
 * Visit a CSE capture expr (docs/design/05-cse.md): evaluate the
 * inner expression into dst, then copy the value into the home
 * local's slot.  The value semantics is that of the inner expression.
 */
static bool
lir_visit_capture_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int local_index;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_CAPTURE);
	assert(expr->val.capture.expr != NULL);
	assert(expr->val.capture.symbol != NULL);

	/* Visit the inner expr. */
	if (!lir_visit_expr(dst_tmpvar, expr->val.capture.expr, block))
		return false;

	/* Copy the value into the home local variable. */
	local_index = lir_get_local_index(block, expr->val.capture.symbol);
	assert(local_index >= 0);
	if (!lir_put_opcode(OP_ASSIGN))
		return false;
	if (!lir_put_tmpvar((uint16_t)local_index))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;

	return true;
}

static bool
lir_visit_call_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int arg_tmpvar[HIR_PARAM_SIZE];
	uint32_t arg_count;
	int func_tmpvar;
	int i;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_CALL);
	assert(expr->val.call.func != NULL);
	assert(expr->val.call.arg_count < HIR_PARAM_SIZE);

	arg_count = expr->val.call.arg_count;
	
	/* Visit the func expr. */
	if (!lir_increment_tmpvar(&func_tmpvar))
		return false;
	if (!lir_visit_expr(func_tmpvar, expr->val.call.func, block))
		return false;

	/* Visit the arg exprs. */
	for (i = 0; i < (int)arg_count; i++) {
		if (!lir_increment_tmpvar(&arg_tmpvar[i]))
			return false;
		if (!lir_visit_expr(arg_tmpvar[i], expr->val.call.arg[i], block))
			return false;
	}

	/* Put a bytecode sequence. */
	if (!lir_put_opcode(OP_CALL))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)func_tmpvar))
		return false;
	if (!lir_put_imm8((uint8_t)arg_count))
		return false;
	for (i = 0; i < (int)arg_count; i++) {
		if (!lir_put_tmpvar((uint16_t)arg_tmpvar[i]))
			return false;
	}

	for (i = (int)arg_count - 1; i >= 0; i--)
		lir_decrement_tmpvar(arg_tmpvar[i]);
	lir_decrement_tmpvar(func_tmpvar);

	return true;
}

static bool
lir_visit_thiscall_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int arg_tmpvar[HIR_PARAM_SIZE];
	int arg_count;
	int obj_tmpvar;
	int i;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_THISCALL);
	assert(expr->val.thiscall.func != NULL);
	assert(expr->val.thiscall.arg_count < HIR_PARAM_SIZE);

	arg_count = (int)expr->val.thiscall.arg_count;
	
	/* Visit the object expr. */
	if (!lir_increment_tmpvar(&obj_tmpvar))
		return false;
	if (!lir_visit_expr(obj_tmpvar, expr->val.thiscall.obj, block))
		return false;

	/* Visit the arg exprs. */
	for (i = 0; i < arg_count; i++) {
		if (!lir_increment_tmpvar(&arg_tmpvar[i]))
			return false;
		if (!lir_visit_expr(arg_tmpvar[i], expr->val.thiscall.arg[i], block))
			return false;
	}

	/* Put a bytecode sequence. */
	if (!lir_put_opcode(OP_THISCALL))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)obj_tmpvar))
		return false;
	if (!lir_put_string(expr->val.thiscall.func))
		return false;
	if (!lir_put_imm8((uint8_t)arg_count))
		return false;
	for (i = 0; i < arg_count; i++) {
		if (!lir_put_tmpvar((uint16_t)arg_tmpvar[i]))
			return false;
	}

	for (i = arg_count - 1; i >= 0; i--)
		lir_decrement_tmpvar(arg_tmpvar[i]);
	lir_decrement_tmpvar(obj_tmpvar);

	return true;
}

static bool
lir_visit_array_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	size_t elem_count, i;
	int build_tmpvar;
	int elem_tmpvar;
	int index_tmpvar;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_ARRAY);
	assert(expr->val.array.elem_count > 0);

	elem_count = expr->val.array.elem_count;

	/* Build in a scratch tmpvar; see lir_visit_dict_expr. */
	if (!lir_increment_tmpvar(&build_tmpvar))
		return false;

	/* Create an array. */
	if (!lir_put_opcode(OP_ACONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)build_tmpvar))
		return false;

	/* Push the elements. */
	if (!lir_increment_tmpvar(&elem_tmpvar))
		return false;
	if (!lir_increment_tmpvar(&index_tmpvar))
		return false;
	for (i = 0; i < elem_count; i++) {
		/* Visit the element. */
		if (!lir_visit_expr(elem_tmpvar, expr->val.array.elem[i], block))
			return false;

		/* Add to the array. */
		if (!lir_put_opcode(OP_ICONST))
			return false;
		if (!lir_put_tmpvar((uint16_t)index_tmpvar))
			return false;
		if (!lir_put_imm32((uint32_t)i))
			return false;
		if (!lir_put_opcode(OP_STOREARRAY))
			return false;
		if (!lir_put_tmpvar((uint16_t)build_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)index_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)elem_tmpvar))
			return false;
	}

	/* Move the finished array into dst. */
	if (!lir_put_opcode(OP_ASSIGN))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)build_tmpvar))
		return false;

	lir_decrement_tmpvar(index_tmpvar);
	lir_decrement_tmpvar(elem_tmpvar);
	lir_decrement_tmpvar(build_tmpvar);

	return true;
}

static bool
lir_visit_dict_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	size_t kv_count, i;
	int build_tmpvar;
	int key_tmpvar;
	int value_tmpvar;
	int index_tmpvar;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_DICT);

	kv_count = expr->val.dict.kv_count;

	/*
	 * Build the dictionary in a scratch tmpvar, not in dst: dst may
	 * alias a slot the value expressions still read (the $return
	 * slot is parameter 0's slot, so "return {k: param};" would
	 * otherwise store the dictionary into itself).
	 */
	if (!lir_increment_tmpvar(&build_tmpvar))
		return false;
	if (!lir_put_opcode(OP_DCONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)build_tmpvar))
		return false;

	/* Push the elements. */
	if (!lir_increment_tmpvar(&key_tmpvar))
		return false;
	if (!lir_increment_tmpvar(&value_tmpvar))
		return false;
	if (!lir_increment_tmpvar(&index_tmpvar))
		return false;
	for (i = 0; i < kv_count; i++) {
		/* Visit the element. */
		if (!lir_visit_expr(value_tmpvar, expr->val.dict.value[i], block))
			return false;

		/* Add to the dict. */
		if (!lir_put_opcode(OP_SCONST))
			return false;
		if (!lir_put_tmpvar((uint16_t)key_tmpvar))
			return false;
		if (!lir_put_string(expr->val.dict.key[i]))
			return false;
		if (!lir_put_opcode(OP_STOREARRAY))
			return false;
		if (!lir_put_tmpvar((uint16_t)build_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)key_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)value_tmpvar))
			return false;
	}

	/* Move the finished dictionary into dst. */
	if (!lir_put_opcode(OP_ASSIGN))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)build_tmpvar))
		return false;

	lir_decrement_tmpvar(index_tmpvar);
	lir_decrement_tmpvar(value_tmpvar);
	lir_decrement_tmpvar(key_tmpvar);
	lir_decrement_tmpvar(build_tmpvar);

	return true;
}

static bool
lir_visit_new_expr(
	int dst_tmpvar,
	struct hir_expr *expr,
	struct hir_block *block)
{
	int new_tmpvar, cls_tmpvar, init_tmpvar;

	assert(expr != NULL);
	assert(expr->type == HIR_EXPR_NEW);
	assert(expr->val.new_.cls != NULL);

	/* Load the "new" function. */
	if (!lir_increment_tmpvar(&new_tmpvar))
		return false;
	if (!lir_put_opcode(OP_LOADSYMBOL))
		return false;
	if (!lir_put_tmpvar((uint16_t)new_tmpvar))
		return false;
	if (!lir_put_string("Dict.merge"))
		return false;

	/* Load the class name. */
	if (!lir_increment_tmpvar(&cls_tmpvar))
		return false;
	if (!lir_put_opcode(OP_LOADSYMBOL))
		return false;
	if (!lir_put_tmpvar((uint16_t)cls_tmpvar))
		return false;
	if (!lir_put_string(expr->val.new_.cls))
		return false;

	/* Visit the initializer. */
	if (!lir_increment_tmpvar(&init_tmpvar))
		return false;
	if (expr->val.new_.init != NULL) {
		if (!lir_visit_expr(init_tmpvar, expr->val.new_.init, block))
			return false;
	} else {
		if (!lir_put_opcode(OP_DCONST))
			return false;
		if (!lir_put_tmpvar((uint16_t)init_tmpvar))
			return false;
	}

	/* Put a bytecode sequence. */
	if (!lir_put_opcode(OP_CALL))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)new_tmpvar))
		return false;
	if (!lir_put_imm8((uint8_t)2))
		return false;
	if (!lir_put_tmpvar((uint16_t)cls_tmpvar))
		return false;
	if (!lir_put_tmpvar((uint16_t)init_tmpvar))
		return false;

	lir_decrement_tmpvar(init_tmpvar);
	lir_decrement_tmpvar(cls_tmpvar);
	lir_decrement_tmpvar(new_tmpvar);

	return true;
}

static bool
lir_visit_term(
	int dst_tmpvar,
	struct hir_term *term,
	struct hir_block *block)
{
	assert(term != NULL);

	switch (term->type) {
	case HIR_TERM_SYMBOL:
		if (!lir_visit_symbol_term(dst_tmpvar, term, block))
			return false;
		break;
	case HIR_TERM_INT:
		if (!lir_visit_int_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_LONG:
		if (!lir_visit_long_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_FLOAT:
		if (!lir_visit_float_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_DOUBLE:
		if (!lir_visit_double_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_STRING:
		if (!lir_visit_string_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_EMPTY_ARRAY:
		if (!lir_visit_empty_array_term(dst_tmpvar, term))
			return false;
		break;
	case HIR_TERM_EMPTY_DICT:
		if (!lir_visit_empty_dict_term(dst_tmpvar, term))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	return true;
}

static bool
lir_visit_symbol_term(
	int dst_tmpvar,
	struct hir_term *term,
	struct hir_block *block)
{
	struct hir_block *func;
	struct hir_local *local;

	assert(term != NULL);
	assert(term->type == HIR_TERM_SYMBOL);

	/* Get a root func block. */
	func = block->parent;
	while (func->type != HIR_BLOCK_FUNC)
		func = func->parent;

	/* Search in a local variable list. */
	local = func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, term->val.symbol) == 0)
			break;
		local = local->next;
	}

	/* Put an instruction. */
	if (local != NULL) {
		/* The term is an explicit local variable. */
		if (!lir_put_opcode(OP_ASSIGN))
			return false;
		if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
			return false;
		if (!lir_put_tmpvar((uint16_t)local->index))
			return false;
	} else {
		/* The term is not an explicit local variable. */
		if (!lir_put_opcode(OP_LOADSYMBOL))
			return false;
		if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
			return false;
		if (!lir_put_string(term->val.symbol))
			return false;
	}

	return true;
}

static bool
lir_visit_int_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	assert(term != NULL);
	assert(term->type == HIR_TERM_INT);

	if (!lir_put_opcode(OP_ICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm32((uint32_t)term->val.i))
		return false;

	return true;
}

static bool
lir_visit_long_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	assert(term != NULL);
	assert(term->type == HIR_TERM_LONG);

	if (!lir_put_opcode(OP_LICONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm64((uint64_t)term->val.l))
		return false;

	return true;
}

static bool
lir_visit_float_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	uint32_t data;

	assert(term != NULL);
	assert(term->type == HIR_TERM_FLOAT);

	data = *(uint32_t *)&term->val.f;

	if (!lir_put_opcode(OP_FCONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm32(data))
		return false;

	return true;
}

static bool
lir_visit_double_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	uint64_t data;

	assert(term != NULL);
	assert(term->type == HIR_TERM_DOUBLE);

	data = *(uint64_t *)&term->val.lf;

	if (!lir_put_opcode(OP_LFCONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_imm64(data))
		return false;

	return true;
}

static bool
lir_visit_string_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	assert(term != NULL);
	assert(term->type == HIR_TERM_STRING);

	if (!lir_put_opcode(OP_SCONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;
	if (!lir_put_string(term->val.s))
		return false;

	return true;
}

static bool
lir_visit_empty_array_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	UNUSED_PARAMETER(term);

	assert(term != NULL);
	assert(term->type == HIR_TERM_EMPTY_ARRAY);

	if (!lir_put_opcode(OP_ACONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;

	return true;
}

static bool
lir_visit_empty_dict_term(
	int dst_tmpvar,
	struct hir_term *term)
{
	UNUSED_PARAMETER(term);

	assert(term != NULL);
	assert(term->type == HIR_TERM_EMPTY_DICT);

	if (!lir_put_opcode(OP_DCONST))
		return false;
	if (!lir_put_tmpvar((uint16_t)dst_tmpvar))
		return false;

	return true;
}

static bool
lir_increment_tmpvar(
	int *tmpvar_index)
{
	if (tmpvar_top >= LIR_TMPVAR_MAX) {
		lir_fatal(N_TR("Too many local variables."));
		return false;
	}

	*tmpvar_index = (int)tmpvar_top;

	tmpvar_top++;
	if (tmpvar_top > tmpvar_count)
		tmpvar_count = tmpvar_top;

	return true;
}

static bool
lir_decrement_tmpvar(
	int tmpvar_index)
{
	UNUSED_PARAMETER(tmpvar_index);

	assert(tmpvar_index == (int)tmpvar_top - 1);
	assert(tmpvar_top > 0);

	tmpvar_top--;

	return true;
}

static bool
lir_put_opcode(
	uint8_t opcode)
{
	if (opcode >= OP_VLOADI32X4 && opcode <= OP_VDIVF32X4)
		has_vector_ops = true;
	if (!lir_put_u8(opcode))
		return false;

	return true;
}

static bool
lir_put_tmpvar(
	uint16_t index)
{
	if (!lir_put_u16(index))
		return false;

	return true;
}

static bool
lir_put_imm8(
	uint8_t imm)
{
	if (!lir_put_u8(imm))
		return false;

	return true;
}


static bool
lir_put_imm32(
	uint32_t imm)
{
	if (!lir_put_u32(imm))
		return false;

	return true;
}

static bool
lir_put_imm64(
	uint64_t imm)
{
	if (!lir_put_u64(imm))
		return false;

	return true;
}

static bool lir_put_branch_addr(
	struct hir_block *block)
{
	assert(block != NULL);

	if (loc_count >= LOC_MAX) {
		lir_fatal(N_TR("Too many jumps."));
		return false;
	}

	loc_tbl[loc_count].type = LOC_BLOCK_TOP;
	loc_tbl[loc_count].offset = (uint32_t)bytecode_top;
	loc_tbl[loc_count].block = block;
	loc_count++;

	bytecode[bytecode_top] = 0xff;
	bytecode[bytecode_top + 1] = 0xff;
	bytecode[bytecode_top + 2] = 0xff;
	bytecode[bytecode_top + 3] = 0xff;
	bytecode_top += 4;

	return true;
}

static bool lir_put_continue_addr(
	struct hir_block *block)
{
	assert(block != NULL);
	assert(block->type == HIR_BLOCK_FOR || block->type == HIR_BLOCK_WHILE);

	if (loc_count >= LOC_MAX) {
		lir_fatal(N_TR("Too many jumps."));
		return false;
	}

	loc_tbl[loc_count].type = LOC_BLOCK_CONTINUE;
	loc_tbl[loc_count].offset = (uint32_t)bytecode_top;
	loc_tbl[loc_count].block = block;
	loc_count++;

	bytecode[bytecode_top] = 0xff;
	bytecode[bytecode_top + 1] = 0xff;
	bytecode[bytecode_top + 2] = 0xff;
	bytecode[bytecode_top + 3] = 0xff;
	bytecode_top += 4;

	return true;
}

static bool
lir_put_string(
	const char *s)
{
	uint32_t len, hash, i;

	/* Put the length. (including NUL)*/
	len = (uint32_t)strlen(s) + 1;
	if (!lir_put_u32(len))
		return false;

	/* Put the hash. */
	hash = noct_string_hash(s);
	if (!lir_put_u32(hash))
		return false;

	/* Put the string. (including NUL terminator) */
	for (i = 0; i < len; i++) {
		if (!lir_put_u8((uint8_t)*s++))
			return false;
	}

	return true;
}

static bool
lir_put_u8(
	uint8_t b)
{
	if (bytecode_top + 1 > BYTECODE_BUF_SIZE)
		return false;

	bytecode[bytecode_top] = b;

	bytecode_top++;

	return true;
}

static bool
lir_put_u16(
	uint16_t b)
{
	if (bytecode_top + 2 > BYTECODE_BUF_SIZE)
		return false;

	/* MSB-first. */
	bytecode[bytecode_top] = (uint8_t)((b >> 8) & 0xff);
	bytecode[bytecode_top + 1] = (uint8_t)(b & 0xff);

	bytecode_top += 2;

	return true;
}

static bool
lir_put_u32(
	uint32_t b)
{
	if (bytecode_top + 4 > BYTECODE_BUF_SIZE)
		return false;

	/* MSB-first. */
	bytecode[bytecode_top] = (uint8_t)((b >> 24) & 0xff);
	bytecode[bytecode_top + 1] = (uint8_t)((b >> 16) & 0xff);
	bytecode[bytecode_top + 2] = (uint8_t)((b >> 8) & 0xff);
	bytecode[bytecode_top + 3] = (uint8_t)(b & 0xff);

	bytecode_top += 4;

	return true;
}

static bool
lir_put_u64(
	uint64_t b)
{
	if (bytecode_top + 8 > BYTECODE_BUF_SIZE)
		return false;

	/* MSB-first. */
	bytecode[bytecode_top] = (uint8_t)((b >> 56) & 0xff);
	bytecode[bytecode_top + 1] = (uint8_t)((b >> 48) & 0xff);
	bytecode[bytecode_top + 2] = (uint8_t)((b >> 40) & 0xff);
	bytecode[bytecode_top + 3] = (uint8_t)((b >> 32) & 0xff);
	bytecode[bytecode_top + 4] = (uint8_t)((b >> 24) & 0xff);
	bytecode[bytecode_top + 5] = (uint8_t)((b >> 16) & 0xff);
	bytecode[bytecode_top + 6] = (uint8_t)((b >> 8) & 0xff);
	bytecode[bytecode_top + 7] = (uint8_t)(b & 0xff);

	bytecode_top += 8;

	return true;
}

static void
patch_block_address(void)
{
	uint32_t offset, addr;
	int i;

	for (i = 0; i < loc_count; i++) {
		switch (loc_tbl[i].type) {
		case LOC_BLOCK_TOP:
			offset = loc_tbl[i].offset;
			addr = loc_tbl[i].block->addr;
			bytecode[offset] = (uint8_t)((addr >> 24) & 0xff);
			bytecode[offset + 1] = (uint8_t)((addr >> 16) & 0xff);
			bytecode[offset + 2] = (uint8_t)((addr >> 8) & 0xff);
			bytecode[offset + 3] = (uint8_t)(addr & 0xff);
			break;
		case LOC_BLOCK_CONTINUE:
			offset = loc_tbl[i].offset;
			addr = loc_tbl[i].block->cont_addr;
			bytecode[offset] = (uint8_t)((addr >> 24) & 0xff);
			bytecode[offset + 1] = (uint8_t)((addr >> 16) & 0xff);
			bytecode[offset + 2] = (uint8_t)((addr >> 8) & 0xff);
			bytecode[offset + 3] = (uint8_t)(addr & 0xff);
			break;
		default:
			assert(NEVER_COME_HERE);
			break;
		}
	}
}

/*
 * Free a constructed LIR.
 */
void
lir_cleanup(struct lir_func *func)
{
	uint32_t i;

	assert(func != NULL);

	noct_free(func->func_name);
	for (i = 0; i < func->param_count; i++)
		noct_free(func->param_name[i]);
	noct_free(func->bytecode);
	memset(func, 0, sizeof(struct lir_func));
	noct_free(lir_file_name);
	lir_file_name = NULL;
}

/*
 * Get a file name.
 */
const char *
lir_get_file_name(void)
{
	return lir_file_name;
}

/*
 * Get an error line.
 */
int
lir_get_error_line(void)
{
	return lir_error_line;
}

/*
 * Get an error message.
 */
const char *
lir_get_error_message(void)
{
	return lir_error_message;
}

/* Set an error message. */
static void
lir_fatal(
	const char *msg,
	...)
{
	va_list ap;

	va_start(ap, msg);
	vsnprintf(lir_error_message,
		  sizeof(lir_error_message),
		  msg,
		  ap);
	va_end(ap);
}

/* Set an out-of-memory error message. */
static void
lir_out_of_memory(void)
{
	snprintf(lir_error_message,
		 sizeof(lir_error_message),
		 "%s",
		 N_TR("LIR: Out of memory error."));
}

/*
 * Dump
 */

/* IMM 1-byte */
#define IMM1(d) imm1(&pc, &d)
static INLINE void imm1(uint8_t **pc, uint8_t *ret)
{
	*ret = **pc;
	(*pc) += 1;
}

/* IMM 2-byte */
#define IMM2(d) imm2(&pc, &d)
static INLINE void imm2(uint8_t **pc, uint16_t *ret)
{
	uint32_t b0;
	uint32_t b1;

	b0 = **pc;
	b1 = *((*pc) + 1);
	
	*ret = (uint16_t)((b0 << 8) | (b1));

	(*pc) += 2;
}

/* IMM 4-byte */
#define IMM4(d) imm4(&pc, &d)
static INLINE void imm4(uint8_t **pc, uint32_t *ret)
{
	uint32_t b0;
	uint32_t b1;
	uint32_t b2;
	uint32_t b3;

	b0 = **pc;
	b1 = *((*pc) + 1);
	b2 = *((*pc) + 2);
	b3 = *((*pc) + 3);

	*ret = (uint32_t)((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);

	(*pc) += 4;
}

/* IMM 8-byte */
#define IMM8(d) imm8(&pc, &d)
static INLINE void imm8(uint8_t **pc, uint64_t *ret)
{
	uint32_t b0;
	uint32_t b1;
	uint32_t b2;
	uint32_t b3;
	uint32_t b4;
	uint32_t b5;
	uint32_t b6;
	uint32_t b7;

	b0 = **pc;
	b1 = *((*pc) + 1);
	b2 = *((*pc) + 2);
	b3 = *((*pc) + 3);
	b4 = *((*pc) + 4);
	b5 = *((*pc) + 5);
	b6 = *((*pc) + 6);
	b7 = *((*pc) + 7);

	*ret = ((uint64_t)b0 << 56) |
	       ((uint64_t)b1 << 48) |
               ((uint64_t)b2 << 40) |
               ((uint64_t)b3 << 32) |
               ((uint64_t)b4 << 24) |
               ((uint64_t)b5 << 16) |
               ((uint64_t)b6 << 8) |
               ((uint64_t)b7);

	(*pc) += 8;
}

/* IMM string */
#define IMMS(d) imms(&pc, &d)
static INLINE void imms(uint8_t **pc, const char **ret)
{
	(*pc) += 8;
	*ret = (const char *)*pc;
	(*pc) += strlen((const char *)*pc) + 1;
}

void
lir_dump(
	struct lir_func *func)
{
	uint8_t *pc;
	uint8_t *end;

	pc = func->bytecode;
	end = func->bytecode + func->bytecode_size;

	while (pc < end) {
		int opcode;
		int ofs;
		ofs = (int)(ptrdiff_t)(pc - func->bytecode);
		opcode = *pc++;
		switch (opcode) {
		case OP_LINEINFO:
		{
			uint32_t line;
			IMM4(line);
			printf("%04d: LINEINFO(line:%d)\n", ofs, line);
			break;
		}
		case OP_NOP:
			pc++;
			break;
		case OP_ASSIGN:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: ASSIGN(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_ICONST:
		{
			uint16_t dst;
			uint32_t val;
			IMM2(dst);
			IMM4(val);
			printf("%04d: ICONST(dst:%d, val:%d)\n", ofs, dst, val);
			break;
		}
		case OP_LICONST:
		{
			uint16_t dst;
			uint64_t val;
			IMM2(dst);
			IMM8(val);
			printf("%04d: LICONST(dst:%d, val:%" PRId64 ")\n", ofs, dst, val);
			break;
		}
		case OP_FCONST:
		{
			uint16_t dst;
			uint32_t val = 0;
			float val_f;
			IMM2(dst);
			IMM4(val);
			val_f = *(float *)&val;
			printf("%04d: FCONST(dst:%d, val:%f)\n", ofs, dst, val_f);
			break;
		}
		case OP_LFCONST:
		{
			uint16_t dst;
			uint64_t val = 0;
			double val_f;
			IMM2(dst);
			IMM8(val);
			val_f = *(double *)&val;
			printf("%04d: LFCONST(dst:%d, val:%f)\n", ofs, dst, val_f);
			break;
		}
		case OP_SCONST:
		{
			uint16_t dst;
			const char *val;
			IMM2(dst);
			IMMS(val);
			printf("%04d: SCONST(dst:%d, val:%s)\n", ofs, dst, val);
			break;
		}
		case OP_ACONST:
		{
			uint16_t dst;
			IMM2(dst);
			printf("%04d: ACONST(dst:%d)\n", ofs, dst);
			break;
		}
		case OP_DCONST:
		{
			uint16_t dst;
			IMM2(dst);
			printf("%04d: DCONST(dst:%d)\n", ofs, dst);
			break;
		}
		case OP_INC:
		{
			uint16_t dst;
			IMM2(dst);
			printf("%04d: INC(dst:%d)\n", ofs, dst);
			break;
		}
		case OP_NOT:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: NOT(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_NEG:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: NEG(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_ADD:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: ADD(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_SUB:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: SUB(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_MUL:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: MUL(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_DIV:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: DIV(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_MOD:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: MOD(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_AND:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: AND(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_OR:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: OR(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_XOR:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: XOR(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_SHL:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: SHL(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_SHR:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: SHR(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_PBASE:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: PBASE(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_PLEN:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: PLEN(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_PCHECK:
		{
			uint16_t dst;
			uint16_t src;
			uint8_t type;
			IMM2(dst);
			IMM2(src);
			IMM1(type);
			printf("%04d: PCHECK(dst:%d, src:%d, type:%d)\n", ofs, dst, src, type);
			break;
		}
		case OP_TYPEIS:
		{
			uint16_t dst;
			uint16_t src;
			uint8_t type;
			IMM2(dst);
			IMM2(src);
			IMM1(type);
			printf("%04d: TYPEIS(dst:%d, src:%d, type:%d)\n", ofs, dst, src, type);
			break;
		}
		case OP_PLOAD8U:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD8U(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PSTORE8:
		{
			uint16_t base;
			uint16_t o;
			uint16_t src;
			IMM2(base);
			IMM2(o);
			IMM2(src);
			printf("%04d: PSTORE8(base:%d, ofs:%d, src:%d)\n", ofs, base, o, src);
			break;
		}
		case OP_PLOAD8S:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD8S(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PLOAD16U:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD16U(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PLOAD16S:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD16S(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PLOAD32:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD32(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PLOAD64:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOAD64(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PLOADF32:
		{
			uint16_t dst;
			uint16_t base;
			uint16_t o;
			IMM2(dst);
			IMM2(base);
			IMM2(o);
			printf("%04d: PLOADF32(dst:%d, base:%d, ofs:%d)\n", ofs, dst, base, o);
			break;
		}
		case OP_PSTORE16:
		{
			uint16_t base;
			uint16_t o;
			uint16_t src;
			IMM2(base);
			IMM2(o);
			IMM2(src);
			printf("%04d: PSTORE16(base:%d, ofs:%d, src:%d)\n", ofs, base, o, src);
			break;
		}
		case OP_PSTORE32:
		{
			uint16_t base;
			uint16_t o;
			uint16_t src;
			IMM2(base);
			IMM2(o);
			IMM2(src);
			printf("%04d: PSTORE32(base:%d, ofs:%d, src:%d)\n", ofs, base, o, src);
			break;
		}
		case OP_PSTORE64:
		{
			uint16_t base;
			uint16_t o;
			uint16_t src;
			IMM2(base);
			IMM2(o);
			IMM2(src);
			printf("%04d: PSTORE64(base:%d, ofs:%d, src:%d)\n", ofs, base, o, src);
			break;
		}
		case OP_PSTOREF32:
		{
			uint16_t base;
			uint16_t o;
			uint16_t src;
			IMM2(base);
			IMM2(o);
			IMM2(src);
			printf("%04d: PSTOREF32(base:%d, ofs:%d, src:%d)\n", ofs, base, o, src);
			break;
		}
		case OP_CHECKTYPE:
		{
			uint16_t slot;
			uint8_t type;
			IMM2(slot);
			IMM1(type);
			printf("%04d: CHECKTYPE(slot:%d, type:%d)\n", ofs, slot, type);
			break;
		}
		case OP_VLOADI32X4:
		case OP_VSTOREI32X4:
		case OP_VSPLATI32:
		case OP_VGETLANEI32:
		case OP_VMOV128:
		case OP_VADDI32X4:
		case OP_VSUBI32X4:
		case OP_VMULI32X4:
		case OP_VAND128:
		case OP_VOR128:
		case OP_VXOR128:
		case OP_VSHLI32X4:
		case OP_VSHRI32X4:
		case OP_VLOADF32X4:
		case OP_VSTOREF32X4:
		case OP_VSPLATF32:
		case OP_VGETLANEF32:
		case OP_VADDF32X4:
		case OP_VSUBF32X4:
		case OP_VMULF32X4:
		case OP_VDIVF32X4:
		{
			/* 128-bit SIMD (design 06); operand shapes vary. */
			static const char *vec_name[] = {
				"VLOADI32X4", "VSTOREI32X4", "VSPLATI32",
				"VGETLANEI32", "VMOV128",
				"VADDI32X4", "VSUBI32X4", "VMULI32X4",
				"VAND128", "VOR128", "VXOR128",
				"VSHLI32X4", "VSHRI32X4",
				"VLOADF32X4", "VSTOREF32X4", "VSPLATF32",
				"VGETLANEF32", "VADDF32X4", "VSUBF32X4",
				"VMULF32X4", "VDIVF32X4"
			};
			const char *nm = vec_name[opcode - OP_VLOADI32X4];
			uint16_t t1;
			uint16_t t2;
			uint8_t i1;
			uint8_t i2;
			uint8_t i3;
			switch (opcode) {
			case OP_VLOADI32X4:
			case OP_VLOADF32X4:
				IMM1(i1); IMM2(t1); IMM2(t2);
				printf("%04d: %s(vd:%d, base:%d, ofs:%d)\n", ofs, nm, i1, t1, t2);
				break;
			case OP_VSTOREI32X4:
			case OP_VSTOREF32X4:
				IMM2(t1); IMM2(t2); IMM1(i1);
				printf("%04d: %s(base:%d, ofs:%d, vs:%d)\n", ofs, nm, t1, t2, i1);
				break;
			case OP_VSPLATI32:
			case OP_VSPLATF32:
				IMM1(i1); IMM2(t1);
				printf("%04d: %s(vd:%d, src:%d)\n", ofs, nm, i1, t1);
				break;
			case OP_VGETLANEI32:
			case OP_VGETLANEF32:
				IMM2(t1); IMM1(i1); IMM1(i2);
				printf("%04d: %s(dst:%d, vs:%d, lane:%d)\n", ofs, nm, t1, i1, i2);
				break;
			case OP_VMOV128:
				IMM1(i1); IMM1(i2);
				printf("%04d: %s(vd:%d, vs:%d)\n", ofs, nm, i1, i2);
				break;
			default:
				IMM1(i1); IMM1(i2); IMM1(i3);
				printf("%04d: %s(vd:%d, va:%d, vb:%d)\n", ofs, nm, i1, i2, i3);
				break;
			}
			break;
		}
		case OP_ISHL:
		case OP_ISHR:
		{
			/* Typed shifts (design 07): imm8 count. */
			uint16_t dst;
			uint16_t s1;
			uint8_t imm;
			IMM2(dst);
			IMM2(s1);
			IMM1(imm);
			printf("%04d: %s(dst:%d, src1:%d, imm:%d)\n", ofs,
			       opcode == OP_ISHL ? "ISHL" : "ISHR",
			       dst, s1, imm);
			break;
		}
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
		case OP_FADD:
		case OP_FSUB:
		case OP_FMUL:
		case OP_FDIV:
		case OP_FLT:
		case OP_FLTE:
		case OP_FGT:
		case OP_FGTE:
		{
			/* Typed arithmetic (design 07). */
			static const char *typed_name[] = {
				"IADD", "ISUB", "IMUL", "IDIV", "IMOD",
				"IAND", "IOR", "IXOR", "ISHL", "ISHR",
				"ILT", "ILTE", "IGT", "IGTE",
				"FADD", "FSUB", "FMUL", "FDIV",
				"FLT", "FLTE", "FGT", "FGTE"
			};
			uint16_t dst;
			uint16_t s1;
			uint16_t s2;
			IMM2(dst);
			IMM2(s1);
			IMM2(s2);
			printf("%04d: %s(dst:%d, src1:%d, src2:%d)\n", ofs,
			       typed_name[opcode - OP_IADD], dst, s1, s2);
			break;
		}
		case OP_LT:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: LT(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_LTE:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: LTE(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_GT:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: GT(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_GTE:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: GTE(dst:%d, src1:%d, src2:%d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_EQ:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: EQ(dst:%d, src1:%d, src2:%d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_EQI:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: EQI(dst:%d, src1:%d, src2:%d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_NEQ:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: NEQ(dst:%d, src1:%d, src2: %d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_LOADARRAY:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: LOADARRAY(dst:%d, arr:%d, subsc:%d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_STOREARRAY:
		{
			uint16_t dst;
			uint16_t src1;
			uint16_t src2;
			IMM2(dst);
			IMM2(src1);
			IMM2(src2);
			printf("%04d: STOREARRAY(arr:%d, subsc:%d, val:%d)\n", ofs, dst, src1, src2);
			break;
		}
		case OP_LEN:
		{
			uint16_t dst;
			uint16_t src;
			IMM2(dst);
			IMM2(src);
			printf("%04d: LEN(dst:%d, src:%d)\n", ofs, dst, src);
			break;
		}
		case OP_GETDICTKEYBYINDEX:
		{
			uint16_t dst;
			uint16_t dict;
			uint16_t index;
			IMM2(dst);
			IMM2(dict);
			IMM2(index);
			printf("%04d: GETDICTKEYBYINDEX(dst:%d, dict:%d, index:%d)\n", ofs, dst, dict, index);
			break;
		}
		case OP_GETDICTVALBYINDEX:
		{
			uint16_t dst;
			uint16_t dict;
			uint16_t index;
			IMM2(dst);
			IMM2(dict);
			IMM2(index);
			printf("%04d: GETDICTKEYBYINDEX(dst:%d, dict:%d, index:%d)\n", ofs, dst, dict, index);
			break;
		}
		case OP_STOREDOT:
		{
			const char *symbol;
			uint16_t obj, src;
			IMM2(obj);
			IMMS(symbol);
			IMM2(src);
			printf("%04d: STOREDOT(obj:%d, symbol:%s, src:%d)\n", ofs, obj, symbol, src);
			break;
		}
		case OP_LOADDOT:
		{
			const char *symbol;
			uint16_t dst, obj;
			IMM2(dst);
			IMM2(obj);
			IMMS(symbol);
			printf("%04d: LOADDOT(dst: %d, obj:%d, symbol:%s)\n", ofs, dst, obj, symbol);
			break;
		}
		case OP_STORESYMBOL:
		{
			const char *symbol;
			uint16_t src;
			IMMS(symbol);
			IMM2(src);
			printf("%04d: STORESYMBOL(symbol:%s, src:%d)\n", ofs, symbol, src);
			break;
		}
		case OP_LOADSYMBOL:
		{
			uint16_t dst;
			const char *symbol;
			IMM2(dst);
			IMMS(symbol);
			printf("%04d: LOADSYMBOL(src: %d, symbol:%s)\n", ofs, dst, symbol);
			break;
		}
		case OP_CALL:
		{
			uint16_t dst;
			uint16_t func;
			uint8_t arg_count;
			uint16_t arg;
			int i;
			IMM2(dst);
			IMM2(func);
			IMM1(arg_count);
			printf("%04d: CALL(dst: %d, arg_count:%d", ofs, dst, arg_count);
			for (i = 0; i < arg_count; i++) {
				IMM2(arg);
				printf(", %d", arg);
			}
			printf(")\n");
			break;
		}
		case OP_THISCALL:
		{
			uint16_t dst;
			uint16_t obj;
			uint16_t func;
			uint8_t arg_count;
			uint16_t arg;
			int i;
			IMM2(dst);
			IMM2(obj);
			IMM2(func);
			IMM1(arg_count);
			printf("%04d: THISCALL(dst: %d, obj: %d,arg_count:%d", ofs, dst, obj, arg_count);
			for (i = 0; i < arg_count; i++) {
				IMM2(arg);
				printf(", %d", arg);
			}
			printf(")\n");
			break;
		}
		case OP_JMP:
		{
			uint32_t target;
			IMM4(target);
			printf("%04d: JMP(target:%d)\n", ofs, target);
			break;
		}
		case OP_JMPIFTRUE:
		{
			uint16_t src;
			uint32_t target;
			IMM2(src);
			IMM4(target);
			printf("%04d: JMPIFTRUE(src:%d, target:%d)\n", ofs, src, target);
			break;
		}
		case OP_JMPIFFALSE:
		{
			uint16_t src;
			uint32_t target;
			IMM2(src);
			IMM4(target);
			printf("%04d: JMPIFFALSE(src:%d, target:%d)\n", ofs, src, target);
			break;
		}
		case OP_JMPIFEQ:
		{
			uint16_t src;
			uint32_t target;
			IMM2(src);
			IMM4(target);
			printf("%04d: JMPIFEQ(src:%d, target:%d)\n", ofs, src, target);
			break;
		}
		case OP_SAFEPOINT:
		{
			printf("%04d: SAFEPOINT()\n", ofs);
			break;
		}
		default:
		{
			printf("Unknown Opcode: 0x%x\n", opcode);
			assert(INVALID_OPCODE);
			break;
		}
		}
	}
}
