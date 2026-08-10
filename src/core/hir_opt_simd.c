/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR Optimizer: 128-bit SIMD auto-vectorization
 * (docs/design/06-simd.md).
 *
 * Runs right after ABCE.  For every ABCE-versioned fast loop whose
 * body fits the vector grammar (single basic block; homogeneous
 * int32 or float32 packed accesses; integer +,-,*,&,|,^/constant
 * shifts or float +,-,*,/; no counter-as-
 * value; no loop-carried temps; alias discipline), the fast loop is
 * split into a 4-lane strip loop plus a scalar remainder:
 *
 *   B1:  $baseK = PBASE(pK); ...            (existing)
 *        $simdN_mid = $hi - (($hi - $lo) & 3);
 *        $simdN_sbS = $baseK (+|-) 4L * u;  (per non-trivial offset)
 *        $simdN_vg  = (0 <= $lo) && ($lo < $simdN_mid) && <disjoint>;
 *   GV:  if ($simdN_vg)  { VFOR (i in $lo..$mid)  vector body
 *                          RFOR (i in $mid..$hi)  scalar clone }
 *   GS:  if (!$simdN_vg) { SFOR (i in $lo..$hi)   scalar clone }
 *
 * The strip loop touches exactly a prefix of the scalar iteration
 * sequence: (mid - lo) is divisible by 4 by construction (a two's-
 * complement identity that survives wraparound), and the entry
 * condition 0 <= lo confines the strip range to [0, 2^31) where the
 * 32-bit counter cannot wrap, so lanes i..i+3 are exactly the
 * elements the scalar iterations would touch (design 06, D-SIMD10).
 * Packed payloads CAN partially overlap through the preallocated-
 * buffer C API, so cross-packed stores take a runtime disjointness
 * guard (D-SIMD9) instead of relying on object identity.
 *
 * The vector body is the same HIR expression tree, lowered by
 * lir_visit_vfor_block() through a parallel visitor into the vector
 * opcodes; site indexes are rewritten to the bare counter with the
 * affine offset folded into a per-site adjusted base, so the strip
 * body needs no scalar arithmetic at all (the no-helper-call
 * invariant, design 06 5.6).
 */

#include "hir_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Program-visible vregs (must match the LIR planner's budget). */
#define SIMD_VREG_MAX		8

#define SIMD_MAX_BASES		4	/* = ABCE_MAX_PACKED           */
#define SIMD_MAX_OFFS		4	/* distinct offsets per base    */
#define SIMD_MAX_CONSTS		8	/* distinct int consts in body  */
#define SIMD_MAX_LOCALS		8	/* INV + TEMP locals in body    */
#define SIMD_MAX_LOOPS		16

/* Minimum estimated scalar work before entering a vector strip. */
#define SIMD_MIN_WORK		32

/* Index shapes (mirrors the ABCE affine shapes). */
enum simd_shape {
	SIMD_SHAPE_I,
	SIMD_SHAPE_I_PLUS_U,
	SIMD_SHAPE_U_PLUS_I,
	SIMD_SHAPE_I_MINUS_U
};

struct simd_off {
	int shape;
	bool u_is_const;
	int u_const;
	const char *u_name;
	char sb_name[32];	/* adjusted-base local ("" = use base) */
};

struct simd_base {
	const char *base_sym;	/* $abceN_baseK local        */
	const char *packed_sym;	/* the packed local (for PLEN) */
	bool restricted;	/* rpacked* function parameter */
	bool has_store;
	int off_count;
	struct simd_off off[SIMD_MAX_OFFS];
};

struct simd_ctx {
	struct hir_block *func;
	struct hir_block *loop;		/* the abce_fast FOR (becomes VFOR) */
	struct hir_block *b1;		/* the PBASE hoist block            */
	const char *counter;
	const char *lo_name;		/* $abceN_lo   */
	const char *hi_name;		/* $abceN_hi   */

	struct simd_base bases[SIMD_MAX_BASES];
	int base_count;

	/* Planner sets (must stay in sync with lir.c's planner). */
	uint32_t consts[SIMD_MAX_CONSTS];	/* int value or float bits */
	uint8_t const_type[SIMD_MAX_CONSTS];
	int const_count;
	const char *inv[SIMD_MAX_LOCALS];
	int inv_count;
	const char *temp[SIMD_MAX_LOCALS];
	int temp_count;
	int max_depth;
	int body_cost;
	int min_trip;
	bool kind_set;
	bool is_float;

	/* New local names. */
	char mid_name[32];
	char vg_name[32];
};

static int simd_loop_seq;

/* Reject-reason breadcrumb for NOCT_SIMD_DEBUG. */
static const char *simd_reject_reason;
#define SIMD_REJECT(why) do { simd_reject_reason = (why); return false; } while (0)
#define SIMD_REJECT_I(why) do { simd_reject_reason = (why); return -1; } while (0)

/*
 * Small constructors (arena-allocated; failures return NULL and the
 * caller propagates OOM via hir_out_of_memory()).
 */

static struct hir_term *
simd_mk_term_int(int v)
{
	struct hir_term *t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(*t));
	t->type = HIR_TERM_INT;
	t->val.i = v;
	return t;
}

static struct hir_term *
simd_mk_term_long(int64_t v)
{
	struct hir_term *t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(*t));
	t->type = HIR_TERM_LONG;
	t->val.l = v;
	return t;
}

static struct hir_term *
simd_mk_term_float(float v)
{
	struct hir_term *t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(*t));
	t->type = HIR_TERM_FLOAT;
	t->val.f = v;
	return t;
}

static struct hir_term *
simd_mk_term_sym(const char *sym)
{
	struct hir_term *t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(*t));
	t->type = HIR_TERM_SYMBOL;
	t->val.symbol = hir_strdup(sym);
	if (t->val.symbol == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	return t;
}

static struct hir_expr *
simd_mk_expr_term(struct hir_term *t)
{
	struct hir_expr *e;
	if (t == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(*e));
	e->type = HIR_EXPR_TERM;
	e->val.term.term = t;
	return e;
}

static struct hir_expr *
simd_mk_sym(const char *sym)
{
	return simd_mk_expr_term(simd_mk_term_sym(sym));
}

static struct hir_expr *
simd_mk_int(int v)
{
	return simd_mk_expr_term(simd_mk_term_int(v));
}

static struct hir_expr *
simd_mk_long(int64_t v)
{
	return simd_mk_expr_term(simd_mk_term_long(v));
}

static struct hir_expr *
simd_mk_binary(int type, struct hir_expr *l, struct hir_expr *r)
{
	struct hir_expr *e;
	if (l == NULL || r == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(*e));
	e->type = type;
	e->val.binary.expr[0] = l;
	e->val.binary.expr[1] = r;
	return e;
}

static struct hir_expr *
simd_mk_unary(int type, struct hir_expr *x)
{
	struct hir_expr *e;
	if (x == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(*e));
	e->type = type;
	e->val.unary.expr = x;
	return e;
}

static struct hir_stmt *
simd_mk_assign(int line, struct hir_expr *lhs, struct hir_expr *rhs)
{
	struct hir_stmt *s;
	if (lhs == NULL || rhs == NULL)
		return NULL;
	s = hir_malloc(sizeof(struct hir_stmt));
	if (s == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(s, 0, sizeof(*s));
	s->line = line;
	s->lhs = lhs;
	s->rhs = rhs;
	return s;
}

static struct hir_block *
simd_mk_block(int type, int line, struct hir_block *parent)
{
	struct hir_block *b = hir_malloc(sizeof(struct hir_block));
	if (b == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(b, 0, sizeof(*b));
	b->type = type;
	b->line = line;
	b->parent = parent;
	b->id = hir_next_block_id();
	return b;
}

/*
 * Deep copy of an eligible body expression (the vector grammar only:
 * int/symbol terms, PAR, arithmetic binaries, PLOAD32/PSTORE32).
 */
static struct hir_expr *
simd_clone_expr(struct hir_expr *e)
{
	struct hir_expr *n;

	if (e == NULL)
		return NULL;
	n = hir_malloc(sizeof(struct hir_expr));
	if (n == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(n, 0, sizeof(*n));
	n->type = e->type;
	switch (e->type) {
	case HIR_EXPR_TERM:
	{
		struct hir_term *t = e->val.term.term;
		if (t->type == HIR_TERM_INT)
			n->val.term.term = simd_mk_term_int(t->val.i);
		else if (t->type == HIR_TERM_FLOAT)
			n->val.term.term = simd_mk_term_float(t->val.f);
		else if (t->type == HIR_TERM_SYMBOL)
			n->val.term.term = simd_mk_term_sym(t->val.symbol);
		else
			return NULL;	/* outside the grammar */
		if (n->val.term.term == NULL)
			return NULL;
		return n;
	}
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
		n->val.unary.expr = simd_clone_expr(e->val.unary.expr);
		if (n->val.unary.expr == NULL)
			return NULL;
		return n;
	case HIR_EXPR_CALL:
	{
		uint32_t i;
		n->val.call.func = simd_clone_expr(e->val.call.func);
		if (n->val.call.func == NULL)
			return NULL;
		n->val.call.arg_count = e->val.call.arg_count;
		for (i = 0; i < e->val.call.arg_count; i++) {
			n->val.call.arg[i] = simd_clone_expr(e->val.call.arg[i]);
			if (n->val.call.arg[i] == NULL)
				return NULL;
		}
		return n;
	}
	case HIR_EXPR_DOT:
		n->val.dot.obj = simd_clone_expr(e->val.dot.obj);
		n->val.dot.symbol = hir_strdup(e->val.dot.symbol);
		if (n->val.dot.obj == NULL || n->val.dot.symbol == NULL)
			return NULL;
		return n;
	default:
		/* Binary shapes (arith, shifts, PLOAD32/PSTORE32). */
		n->val.binary.expr[0] = simd_clone_expr(e->val.binary.expr[0]);
		n->val.binary.expr[1] = simd_clone_expr(e->val.binary.expr[1]);
		if (n->val.binary.expr[0] == NULL ||
		    n->val.binary.expr[1] == NULL)
			return NULL;
		return n;
	}
}

static struct hir_stmt *
simd_clone_stmt_list(struct hir_stmt *head)
{
	struct hir_stmt *nh = NULL;
	struct hir_stmt *tail = NULL;
	struct hir_stmt *s;

	for (s = head; s != NULL; s = s->next) {
		struct hir_stmt *n = hir_malloc(sizeof(struct hir_stmt));
		if (n == NULL) {
			hir_out_of_memory();
			return NULL;
		}
		memset(n, 0, sizeof(*n));
		n->line = s->line;
		if (s->lhs != NULL) {
			n->lhs = simd_clone_expr(s->lhs);
			if (n->lhs == NULL)
				return NULL;
		}
		n->rhs = simd_clone_expr(s->rhs);
		if (n->rhs == NULL)
			return NULL;
		if (tail == NULL)
			nh = n;
		else
			tail->next = n;
		tail = n;
	}
	return nh;
}

#define SIMD_INLINE_MAX 32

static bool
simd_inline_pure(struct hir_expr *e)
{
	uint32_t i;
	switch (e->type) {
	case HIR_EXPR_TERM:
		return e->val.term.term->type == HIR_TERM_INT ||
		       e->val.term.term->type == HIR_TERM_FLOAT ||
		       e->val.term.term->type == HIR_TERM_SYMBOL;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
		return simd_inline_pure(e->val.unary.expr);
	case HIR_EXPR_CALL:
		if (hir_get_intrinsic_call(e) == HIR_INTRINSIC_NONE ||
		    e->val.call.arg_count != 1)
			return false;
		for (i = 0; i < e->val.call.arg_count; i++) {
			if (!simd_inline_pure(e->val.call.arg[i]))
				return false;
		}
		return true;
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		return simd_inline_pure(e->val.binary.expr[0]) &&
		       simd_inline_pure(e->val.binary.expr[1]);
	default:
		return false;
	}
}

static struct hir_expr *
simd_expand_expr(struct hir_expr *e, const char **names,
		 struct hir_expr **defs, int count)
{
	struct hir_expr *n;
	uint32_t i;

	if (e->type == HIR_EXPR_TERM &&
	    e->val.term.term->type == HIR_TERM_SYMBOL) {
		for (i = 0; i < (uint32_t)count; i++) {
			if (strcmp(names[i], e->val.term.term->val.symbol) == 0)
				return simd_clone_expr(defs[i]);
		}
	}
	n = simd_clone_expr(e);
	if (n == NULL)
		return NULL;
	switch (e->type) {
	case HIR_EXPR_TERM:
		return n;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
		n->val.unary.expr = simd_expand_expr(e->val.unary.expr,
						names, defs, count);
		return n->val.unary.expr != NULL ? n : NULL;
	case HIR_EXPR_CALL:
		for (i = 0; i < e->val.call.arg_count; i++) {
			n->val.call.arg[i] = simd_expand_expr(e->val.call.arg[i],
							 names, defs, count);
			if (n->val.call.arg[i] == NULL)
				return NULL;
		}
		return n;
	default:
		n->val.binary.expr[0] = simd_expand_expr(e->val.binary.expr[0],
							names, defs, count);
		n->val.binary.expr[1] = simd_expand_expr(e->val.binary.expr[1],
							names, defs, count);
		return n->val.binary.expr[0] != NULL &&
		       n->val.binary.expr[1] != NULL ? n : NULL;
	}
}

static bool simd_live_chain(struct hir_block *head, const char *sym);

static bool
simd_live_expr(struct hir_expr *e, const char *sym)
{
	uint32_t i;
	if (e == NULL)
		return false;
	switch (e->type) {
	case HIR_EXPR_TERM:
		return e->val.term.term->type == HIR_TERM_SYMBOL &&
		       strcmp(e->val.term.term->val.symbol, sym) == 0;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
	case HIR_EXPR_PLEN:
	case HIR_EXPR_PBASE:
		return simd_live_expr(e->val.unary.expr, sym);
	case HIR_EXPR_CAPTURE:
		return strcmp(e->val.capture.symbol, sym) == 0 ||
		       simd_live_expr(e->val.capture.expr, sym);
	case HIR_EXPR_DOT:
		return simd_live_expr(e->val.dot.obj, sym);
	case HIR_EXPR_CALL:
		if (simd_live_expr(e->val.call.func, sym))
			return true;
		for (i = 0; i < e->val.call.arg_count; i++)
			if (simd_live_expr(e->val.call.arg[i], sym))
				return true;
		return false;
	case HIR_EXPR_THISCALL:
		if (simd_live_expr(e->val.thiscall.obj, sym))
			return true;
		for (i = 0; i < e->val.thiscall.arg_count; i++)
			if (simd_live_expr(e->val.thiscall.arg[i], sym))
				return true;
		return false;
	case HIR_EXPR_ARRAY:
		for (i = 0; i < e->val.array.elem_count; i++)
			if (simd_live_expr(e->val.array.elem[i], sym))
				return true;
		return false;
	case HIR_EXPR_DICT:
		for (i = 0; i < e->val.dict.kv_count; i++)
			if (simd_live_expr(e->val.dict.value[i], sym))
				return true;
		return false;
	case HIR_EXPR_NEW:
		return simd_live_expr(e->val.new_.init, sym);
	default:
		return simd_live_expr(e->val.binary.expr[0], sym) ||
		       simd_live_expr(e->val.binary.expr[1], sym);
	}
}

static bool
simd_live_chain(struct hir_block *head, const char *sym)
{
	struct hir_block *b;
	struct hir_block *c;
	struct hir_stmt *s;
	for (b = head; b != NULL; b = b->succ) {
		switch (b->type) {
		case HIR_BLOCK_BASIC:
			for (s = b->val.basic.stmt_list; s != NULL; s = s->next)
				if (simd_live_expr(s->lhs, sym) ||
				    simd_live_expr(s->rhs, sym))
					return true;
			break;
		case HIR_BLOCK_IF:
			for (c = b; c != NULL; c = c->val.if_.chain_next)
				if (simd_live_expr(c->val.if_.cond, sym) ||
				    simd_live_chain(c->val.if_.inner, sym))
					return true;
			break;
		case HIR_BLOCK_FOR:
			if (simd_live_expr(b->val.for_.start, sym) ||
			    simd_live_expr(b->val.for_.stop, sym) ||
			    simd_live_expr(b->val.for_.collection, sym) ||
			    simd_live_chain(b->val.for_.inner, sym))
				return true;
			break;
		case HIR_BLOCK_WHILE:
			if (simd_live_expr(b->val.while_.cond, sym) ||
			    simd_live_chain(b->val.while_.inner, sym))
				return true;
			break;
		default:
			break;
		}
		if (b->stop)
			break;
	}
	return false;
}

/*
 * Collapse a straight-line, pure, single-store body into the final store.
 * The caller keeps the original list and restores it on SIMD rejection.
 */
static struct hir_stmt *
simd_inline_temps(struct hir_block *loop)
{
	struct hir_block *body = loop->val.for_.inner;
	struct hir_block *post;
	const char *names[SIMD_INLINE_MAX];
	struct hir_expr *defs[SIMD_INLINE_MAX];
	struct hir_stmt *s;
	struct hir_stmt *last = NULL;
	struct hir_stmt *out;
	int count = 0;
	int i;

	if (body == NULL || body->type != HIR_BLOCK_BASIC || !body->stop)
		return NULL;
	for (s = body->val.basic.stmt_list; s != NULL; s = s->next) {
		last = s;
		if (s->next == NULL)
			break;
		if (s->lhs == NULL || s->lhs->type != HIR_EXPR_TERM ||
		    s->lhs->val.term.term->type != HIR_TERM_SYMBOL ||
		    !simd_inline_pure(s->rhs) || count >= SIMD_INLINE_MAX)
			return NULL;
		for (i = 0; i < count; i++) {
			if (strcmp(names[i],
				   s->lhs->val.term.term->val.symbol) == 0)
				return NULL;
		}
		names[count] = s->lhs->val.term.term->val.symbol;
		defs[count] = simd_expand_expr(s->rhs, names, defs, count);
		if (defs[count] == NULL)
			return NULL;
		count++;
	}
	if (count == 0 || last == NULL || last->lhs == NULL ||
	    (last->lhs->type != HIR_EXPR_PSTORE32 &&
	     last->lhs->type != HIR_EXPR_PSTOREF32) ||
	    !simd_inline_pure(last->rhs))
		return NULL;
	/* FAST -> FEXIT -> X1 -> G2 -> X2 -> original successor. */
	post = loop->succ;
	if (post != NULL) post = post->succ;
	if (post != NULL) post = post->succ;
	if (post != NULL) post = post->succ;
	if (post != NULL) post = post->succ;
	for (i = 0; i < count; i++) {
		if (simd_live_chain(post, names[i]))
			return NULL;
	}
	out = hir_malloc(sizeof(*out));
	if (out == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(out, 0, sizeof(*out));
	out->line = last->line;
	out->lhs = simd_clone_expr(last->lhs);
	out->rhs = simd_expand_expr(last->rhs, names, defs, count);
	if (out->lhs == NULL || out->rhs == NULL)
		return NULL;
	return out;
}

/*
 * ------------------------------------------------------------------
 * Eligibility (design 06, E1..E9).
 * ------------------------------------------------------------------
 */

static bool
simd_is_local(struct simd_ctx *ctx, const char *name)
{
	struct hir_local *local = ctx->func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, name) == 0)
			return true;
		local = local->next;
	}
	return false;
}

static int
simd_local_type(struct simd_ctx *ctx, const char *name)
{
	struct hir_local *local = ctx->func->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, name) == 0)
			return local->proven_type;
		local = local->next;
	}
	return -1;
}

static int
simd_expr_type(struct simd_ctx *ctx, struct hir_expr *e)
{
	int a, b;
	switch (e->type) {
	case HIR_EXPR_TERM:
		if (e->val.term.term->type == HIR_TERM_INT)
			return NOCT_VALUE_INT;
		if (e->val.term.term->type == HIR_TERM_FLOAT)
			return NOCT_VALUE_FLOAT;
		if (e->val.term.term->type == HIR_TERM_SYMBOL)
			return simd_local_type(ctx, e->val.term.term->val.symbol);
		return -1;
	case HIR_EXPR_PAR:
		return simd_expr_type(ctx, e->val.unary.expr);
	case HIR_EXPR_PLOAD32:
		return NOCT_VALUE_INT;
	case HIR_EXPR_PLOADF32:
		return NOCT_VALUE_FLOAT;
	case HIR_EXPR_CALL:
		switch (hir_get_intrinsic_call(e)) {
		case HIR_INTRINSIC_INT_FROM: return NOCT_VALUE_INT;
		case HIR_INTRINSIC_FLOAT_FROM: return NOCT_VALUE_FLOAT;
		default: return -1;
		}
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
		a = simd_expr_type(ctx, e->val.binary.expr[0]);
		b = simd_expr_type(ctx, e->val.binary.expr[1]);
		if (a == NOCT_VALUE_FLOAT || b == NOCT_VALUE_FLOAT)
			return (a == NOCT_VALUE_INT || a == NOCT_VALUE_FLOAT) &&
			       (b == NOCT_VALUE_INT || b == NOCT_VALUE_FLOAT) ?
			       NOCT_VALUE_FLOAT : -1;
		return a == NOCT_VALUE_INT && b == NOCT_VALUE_INT ?
			NOCT_VALUE_INT : -1;
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		a = simd_expr_type(ctx, e->val.binary.expr[0]);
		b = simd_expr_type(ctx, e->val.binary.expr[1]);
		return a == NOCT_VALUE_INT && b == NOCT_VALUE_INT ?
			NOCT_VALUE_INT : -1;
	default:
		return -1;
	}
}

static struct simd_base *
simd_find_base(struct simd_ctx *ctx, const char *sym)
{
	int i;
	for (i = 0; i < ctx->base_count; i++) {
		if (strcmp(ctx->bases[i].base_sym, sym) == 0)
			return &ctx->bases[i];
	}
	return NULL;
}

static bool
simd_note_const(struct simd_ctx *ctx, uint32_t v, int type)
{
	int i;
	for (i = 0; i < ctx->const_count; i++) {
		if (ctx->consts[i] == v && ctx->const_type[i] == type)
			return true;
	}
	if (ctx->const_count >= SIMD_MAX_CONSTS)
		return false;
	ctx->consts[ctx->const_count] = v;
	ctx->const_type[ctx->const_count++] = (uint8_t)type;
	return true;
}

static bool
simd_in_list(const char **list, int count, const char *name)
{
	int i;
	for (i = 0; i < count; i++) {
		if (strcmp(list[i], name) == 0)
			return true;
	}
	return false;
}

/* Parse a site index expr into an offset shape (counter-affine). */
static bool
simd_parse_index(struct simd_ctx *ctx, struct hir_expr *f,
		 struct simd_off *out)
{
	struct hir_expr *a;
	struct hir_expr *b;

	memset(out, 0, sizeof(*out));
	if (f->type == HIR_EXPR_TERM &&
	    f->val.term.term->type == HIR_TERM_SYMBOL &&
	    strcmp(f->val.term.term->val.symbol, ctx->counter) == 0) {
		out->shape = SIMD_SHAPE_I;
		return true;
	}
	if (f->type != HIR_EXPR_PLUS && f->type != HIR_EXPR_MINUS)
		return false;
	a = f->val.binary.expr[0];
	b = f->val.binary.expr[1];
	if (a->type != HIR_EXPR_TERM || b->type != HIR_EXPR_TERM)
		return false;
	if (a->val.term.term->type == HIR_TERM_SYMBOL &&
	    strcmp(a->val.term.term->val.symbol, ctx->counter) == 0) {
		out->shape = (f->type == HIR_EXPR_PLUS) ?
			SIMD_SHAPE_I_PLUS_U : SIMD_SHAPE_I_MINUS_U;
		if (b->val.term.term->type == HIR_TERM_INT) {
			out->u_is_const = true;
			out->u_const = b->val.term.term->val.i;
		} else if (b->val.term.term->type == HIR_TERM_SYMBOL) {
			out->u_name = b->val.term.term->val.symbol;
		} else {
			return false;
		}
		return true;
	}
	if (f->type == HIR_EXPR_PLUS &&
	    b->val.term.term->type == HIR_TERM_SYMBOL &&
	    strcmp(b->val.term.term->val.symbol, ctx->counter) == 0) {
		out->shape = SIMD_SHAPE_U_PLUS_I;
		if (a->val.term.term->type == HIR_TERM_INT) {
			out->u_is_const = true;
			out->u_const = a->val.term.term->val.i;
		} else if (a->val.term.term->type == HIR_TERM_SYMBOL) {
			out->u_name = a->val.term.term->val.symbol;
		} else {
			return false;
		}
		return true;
	}
	return false;
}

static bool
simd_off_equal(const struct simd_off *a, const struct simd_off *b)
{
	if (a->shape != b->shape)
		return false;
	if (a->u_is_const != b->u_is_const)
		return false;
	if (a->u_is_const)
		return a->u_const == b->u_const;
	if (a->u_name == NULL || b->u_name == NULL)
		return a->u_name == b->u_name;
	return strcmp(a->u_name, b->u_name) == 0;
}

/* Register a homogeneous PLOAD/PSTORE i32 or f32 site. */
static bool
simd_note_site(struct simd_ctx *ctx, struct hir_expr *site, bool is_store)
{
	struct simd_base *base;
	struct simd_off off;
	const char *base_sym;
	int i;
	bool is_float = site->type == HIR_EXPR_PLOADF32 ||
			 site->type == HIR_EXPR_PSTOREF32;

	if (!ctx->kind_set) {
		ctx->kind_set = true;
		ctx->is_float = is_float;
	} else if (is_float) {
		ctx->is_float = true;
	}

	if (site->val.binary.expr[0]->type != HIR_EXPR_TERM ||
	    site->val.binary.expr[0]->val.term.term->type != HIR_TERM_SYMBOL)
		return false;
	base_sym = site->val.binary.expr[0]->val.term.term->val.symbol;

	if (!simd_parse_index(ctx, site->val.binary.expr[1], &off))
		return false;

	base = simd_find_base(ctx, base_sym);
	if (base == NULL) {
		if (ctx->base_count >= SIMD_MAX_BASES)
			return false;
		base = &ctx->bases[ctx->base_count++];
		memset(base, 0, sizeof(*base));
		base->base_sym = base_sym;
	}
	if (is_store)
		base->has_store = true;

	for (i = 0; i < base->off_count; i++) {
		if (simd_off_equal(&base->off[i], &off))
			return true;
	}
	if (base->off_count >= SIMD_MAX_OFFS)
		return false;
	base->off[base->off_count++] = off;
	return true;
}

/* Does the expression read the given symbol anywhere? */
static bool
simd_expr_reads(struct hir_expr *e, const char *sym)
{
	switch (e->type) {
	case HIR_EXPR_TERM:
		return e->val.term.term->type == HIR_TERM_SYMBOL &&
			strcmp(e->val.term.term->val.symbol, sym) == 0;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
		return simd_expr_reads(e->val.unary.expr, sym);
	case HIR_EXPR_CALL:
	{
		uint32_t i;
		for (i = 0; i < e->val.call.arg_count; i++) {
			if (simd_expr_reads(e->val.call.arg[i], sym))
				return true;
		}
		return false;
	}
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
		return false;	/* base/index are not vector operands */
	default:
		return simd_expr_reads(e->val.binary.expr[0], sym) ||
			simd_expr_reads(e->val.binary.expr[1], sym);
	}
}

/* PARs are transparent for operand-position decisions. */
static struct hir_expr *
simd_strip_par(struct hir_expr *e)
{
	while (e->type == HIR_EXPR_PAR)
		e = e->val.unary.expr;
	return e;
}

/*
 * Expression walk: grammar check (E4), counter-position check (E5),
 * local read/const collection, and the extra-stack-slot need f(e)
 * for evaluating e into a given destination vreg (the destination
 * itself is not counted; TERM operands are consumed directly from
 * their home vregs).  Must stay in lockstep with the LIR planner
 * (lir_vfor_expr_need).  Returns -1 on rejection.
 */
static int
simd_check_expr(struct simd_ctx *ctx, struct hir_expr *e)
{
	int l, r;

	/* Terms and parentheses are free; every other node is one unit. */
	if (e->type != HIR_EXPR_TERM && e->type != HIR_EXPR_PAR)
		ctx->body_cost++;

	switch (e->type) {
	case HIR_EXPR_TERM:
		switch (e->val.term.term->type) {
		case HIR_TERM_INT:
			if (!simd_note_const(ctx,
					     (uint32_t)e->val.term.term->val.i,
					     NOCT_VALUE_INT))
				SIMD_REJECT_I("E8 const cap");
			return 0;
		case HIR_TERM_FLOAT:
		{
			uint32_t bits;
			memcpy(&bits, &e->val.term.term->val.f, sizeof(bits));
			if (!simd_note_const(ctx, bits, NOCT_VALUE_FLOAT))
				SIMD_REJECT_I("E8 const cap");
			return 0;
		}
		case HIR_TERM_SYMBOL:
		{
			const char *sym = e->val.term.term->val.symbol;
			if (strcmp(sym, ctx->counter) == 0)
				SIMD_REJECT_I("E5 counter value");
			if (simd_find_base(ctx, sym) != NULL)
				SIMD_REJECT_I("E4 base ref");
			if (!simd_is_local(ctx, sym))
				SIMD_REJECT_I("E4 global");
			/* Record the read. */
			if (!simd_in_list(ctx->temp, ctx->temp_count, sym) &&
			    !simd_in_list(ctx->inv, ctx->inv_count, sym)) {
				if (ctx->inv_count >= SIMD_MAX_LOCALS)
					SIMD_REJECT_I("E8 local cap");
				ctx->inv[ctx->inv_count++] = sym;
			}
			return 0;
		}
		default:
			return -1;
		}
	case HIR_EXPR_PAR:
		return simd_check_expr(ctx, e->val.unary.expr);
	case HIR_EXPR_CALL:
		if (e->val.call.arg_count != 1 ||
		    hir_get_intrinsic_call(e) == HIR_INTRINSIC_NONE)
			SIMD_REJECT_I("E4 call");
		return simd_check_expr(ctx, e->val.call.arg[0]);
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOADF32:
		if (!simd_note_site(ctx, e, false))
			SIMD_REJECT_I("E7 load site");
		return 0;	/* loads go straight to the destination */
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	{
		/*
		 * f(e) = extra stack slots to evaluate e into a given
		 * destination, with destination reuse: a non-term left
		 * operand is built in the destination itself, and for
		 * commutative ops a lone non-term right operand is too.
		 * Must mirror lir_vfor_expr() exactly.
		 */
		bool lterm, rterm;
		bool commutative = (e->type != HIR_EXPR_MINUS &&
				    e->type != HIR_EXPR_DIV);
		l = simd_check_expr(ctx, e->val.binary.expr[0]);
		if (l < 0)
			return -1;
		r = simd_check_expr(ctx, e->val.binary.expr[1]);
		if (r < 0)
			return -1;
		if (simd_expr_type(ctx, e) < 0 && !ctx->kind_set)
			SIMD_REJECT_I("E4 mixed types");
		if (e->type == HIR_EXPR_DIV &&
		    (simd_expr_type(ctx, e) == NOCT_VALUE_INT ||
		     (simd_expr_type(ctx, e) < 0 && !ctx->is_float))) {
			SIMD_REJECT_I("E4 i32 divide");
		}
		lterm = simd_strip_par(e->val.binary.expr[0])->type == HIR_EXPR_TERM;
		rterm = simd_strip_par(e->val.binary.expr[1])->type == HIR_EXPR_TERM;
		if (lterm && rterm)
			return 0;
		if (lterm)
			return commutative ? r : 1 + r;
		if (rterm)
			return l;
		return l > (1 + r) ? l : (1 + r);
	}
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
	{
		struct hir_expr *c = e->val.binary.expr[1];
		if (c->type != HIR_EXPR_TERM ||
		    c->val.term.term->type != HIR_TERM_INT)
			SIMD_REJECT_I("E9 shift count");
		if (c->val.term.term->val.i < 0 ||
		    c->val.term.term->val.i > 31)
			SIMD_REJECT_I("E9 shift range");
		l = simd_check_expr(ctx, e->val.binary.expr[0]);
		if (l < 0)
			return -1;
		if (simd_expr_type(ctx, e) != NOCT_VALUE_INT &&
		    !(simd_expr_type(ctx, e) < 0 && ctx->kind_set &&
		      !ctx->is_float))
			SIMD_REJECT_I("E4 f32 shift");
		/* Shift is applied in place on the destination. */
		return l;
	}
	default:
		/* DIV, MOD, comparisons, LAND/LOR, NEG, NOT, CAPTURE,
		   other PLOAD/PSTORE widths, DOT, CALL, ... */
		SIMD_REJECT_I("E4 grammar");
	}
}

/* Scan the (single) body block; returns false if ineligible. */
static bool
simd_check_body(struct simd_ctx *ctx, struct hir_block *body)
{
	struct hir_stmt *stmt;
	int d;

	if (body == NULL || body->type != HIR_BLOCK_BASIC || !body->stop)
		SIMD_REJECT("E2 body shape");

	for (stmt = body->val.basic.stmt_list; stmt != NULL; stmt = stmt->next) {
		if (stmt->lhs == NULL)
			SIMD_REJECT("E3 bare stmt");
		if (stmt->lhs->type == HIR_EXPR_TERM &&
		    stmt->lhs->val.term.term->type == HIR_TERM_SYMBOL) {
			const char *sym = stmt->lhs->val.term.term->val.symbol;
			if (strcmp(sym, "$return") == 0)
				SIMD_REJECT("E3 return");
			if (simd_find_base(ctx, sym) != NULL)
				SIMD_REJECT("E3 base assign");
			d = simd_check_expr(ctx, stmt->rhs);
			ctx->body_cost++; /* vector temporary assignment */
			if (d < 0)
				return false;
			/* If the RHS reads the assigned temp, the value
			   is built in a stack slot and moved (the home
			   vreg must stay intact during evaluation). */
			if (simd_expr_reads(stmt->rhs, sym))
				d = d + 1;
			if (d > ctx->max_depth)
				ctx->max_depth = d;
			/* E6: a TEMP read before its first assignment
			   is loop-carried.  simd_check_expr registered
			   any prior read into inv[]; if this symbol is
			   there, it was read first. */
			if (simd_in_list(ctx->inv, ctx->inv_count, sym))
				SIMD_REJECT("E6 loop-carried temp");
			if (!simd_in_list(ctx->temp, ctx->temp_count, sym)) {
				if (ctx->temp_count >= SIMD_MAX_LOCALS)
					SIMD_REJECT("E6 temp cap");
				ctx->temp[ctx->temp_count++] = sym;
			}
		} else if (stmt->lhs->type == HIR_EXPR_PSTORE32 ||
			   stmt->lhs->type == HIR_EXPR_PSTOREF32) {
			if (!simd_note_site(ctx, stmt->lhs, true))
				SIMD_REJECT("E7 store site");
			d = simd_check_expr(ctx, stmt->rhs);
			ctx->body_cost++; /* packed store */
			if (d < 0)
				return false;
			/* A non-term store value is built in one slot. */
			if (simd_strip_par(stmt->rhs)->type != HIR_EXPR_TERM)
				d = d + 1;
			if (d > ctx->max_depth)
				ctx->max_depth = d;
		} else {
			SIMD_REJECT("E1 store width");
		}
	}

	/* E7 (same-base): a stored base with mixed offsets. */
	{
		int i;
		for (i = 0; i < ctx->base_count; i++) {
			if (ctx->bases[i].has_store &&
			    ctx->bases[i].off_count > 1)
				SIMD_REJECT("E7 mixed offsets");
		}
	}

	/* E8: the vreg budget (mirror of the LIR planner). */
	if (ctx->const_count + ctx->inv_count + ctx->temp_count +
	    ctx->max_depth > SIMD_VREG_MAX)
		SIMD_REJECT("E8 vreg budget");

	/* Round ceil(min-work/body-cost) up to a complete four-lane group. */
	ctx->min_trip = (SIMD_MIN_WORK + ctx->body_cost - 1) /
		ctx->body_cost;
	if (ctx->min_trip < 4)
		ctx->min_trip = 4;
	ctx->min_trip = (ctx->min_trip + 3) & ~3;

	return true;
}

/*
 * Find the loop's surroundings: parent guard IF (G1), the PBASE
 * hoist block B1, and map base symbols to packed symbols.
 */
static bool
simd_find_environment(struct simd_ctx *ctx)
{
	struct hir_block *g1;
	struct hir_block *b1;
	struct hir_stmt *s;
	int i;

	g1 = ctx->loop->parent;
	if (g1 == NULL || g1->type != HIR_BLOCK_IF)
		SIMD_REJECT("env G1");
	b1 = g1->val.if_.inner;
	if (b1 == NULL || b1->type != HIR_BLOCK_BASIC)
		SIMD_REJECT("env B1");
	if (b1->succ != ctx->loop)
		SIMD_REJECT("env B1 succ");
	ctx->b1 = b1;

	/* $baseK = PBASE(pK) statements give the packed mapping. */
	for (s = b1->val.basic.stmt_list; s != NULL; s = s->next) {
		struct simd_base *base;
		if (s->lhs == NULL || s->rhs == NULL)
			continue;
		if (s->lhs->type != HIR_EXPR_TERM ||
		    s->lhs->val.term.term->type != HIR_TERM_SYMBOL)
			continue;
		if (s->rhs->type != HIR_EXPR_PBASE)
			continue;
		if (s->rhs->val.unary.expr->type != HIR_EXPR_TERM ||
		    s->rhs->val.unary.expr->val.term.term->type != HIR_TERM_SYMBOL)
			continue;
		base = simd_find_base(ctx, s->lhs->val.term.term->val.symbol);
		if (base != NULL)
			base->packed_sym =
				s->rhs->val.unary.expr->val.term.term->val.symbol;
	}
	for (i = 0; i < ctx->base_count; i++) {
		uint32_t k;
		if (ctx->bases[i].packed_sym == NULL)
			SIMD_REJECT("env packed map");
		for (k = 0; k < ctx->func->val.func.param_count; k++) {
			if (ctx->func->val.func.param_restricted[k] &&
			    strcmp(ctx->func->val.func.param_name[k],
				   ctx->bases[i].packed_sym) == 0) {
				ctx->bases[i].restricted = true;
				break;
			}
		}
	}

	/* $lo/$hi from the loop bounds (symbols by construction). */
	if (ctx->loop->val.for_.start->type != HIR_EXPR_TERM ||
	    ctx->loop->val.for_.start->val.term.term->type != HIR_TERM_SYMBOL)
		SIMD_REJECT("env lo");
	if (ctx->loop->val.for_.stop->type != HIR_EXPR_TERM ||
	    ctx->loop->val.for_.stop->val.term.term->type != HIR_TERM_SYMBOL)
		SIMD_REJECT("env hi");
	ctx->lo_name = ctx->loop->val.for_.start->val.term.term->val.symbol;
	ctx->hi_name = ctx->loop->val.for_.stop->val.term.term->val.symbol;

	return true;
}

/* Count the function's locals (frame-budget check). */
static int
simd_count_locals(struct simd_ctx *ctx)
{
	struct hir_local *local = ctx->func->val.func.local;
	int n = 0;
	while (local != NULL) {
		n++;
		local = local->next;
	}
	return n;
}

/* Build the site's index offset as an int expression (u or const). */
static struct hir_expr *
simd_mk_u_expr(const struct simd_off *off)
{
	if (off->u_is_const)
		return simd_mk_int(off->u_const);
	return simd_mk_sym(off->u_name);
}

/* Rewrite VFOR body sites to (adjusted base, bare counter). */
static bool
simd_rewrite_expr(struct simd_ctx *ctx, struct hir_expr *e)
{
	if (e == NULL)
		return true;
	switch (e->type) {
	case HIR_EXPR_TERM:
		return true;
	case HIR_EXPR_PAR:
		return simd_rewrite_expr(ctx, e->val.unary.expr);
	case HIR_EXPR_CALL:
	{
		uint32_t i;
		for (i = 0; i < e->val.call.arg_count; i++) {
			if (!simd_rewrite_expr(ctx, e->val.call.arg[i]))
				return false;
		}
		return true;
	}
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PSTORE32:
	case HIR_EXPR_PLOADF32:
	case HIR_EXPR_PSTOREF32:
	{
		struct simd_base *base;
		struct simd_off off;
		struct hir_expr *idx;
		int i;

		base = simd_find_base(ctx,
			e->val.binary.expr[0]->val.term.term->val.symbol);
		assert(base != NULL);
		if (!simd_parse_index(ctx, e->val.binary.expr[1], &off))
			return false;
		for (i = 0; i < base->off_count; i++) {
			if (simd_off_equal(&base->off[i], &off))
				break;
		}
		assert(i < base->off_count);
		if (base->off[i].sb_name[0] != '\0') {
			struct hir_expr *nb = simd_mk_sym(base->off[i].sb_name);
			if (nb == NULL)
				return false;
			e->val.binary.expr[0] = nb;
		}
		idx = simd_mk_sym(ctx->counter);
		if (idx == NULL)
			return false;
		e->val.binary.expr[1] = idx;
		return true;
	}
	default:
		if (!simd_rewrite_expr(ctx, e->val.binary.expr[0]))
			return false;
		return simd_rewrite_expr(ctx, e->val.binary.expr[1]);
	}
}

/*
 * The transform.  Returns false only on OOM.
 */
static bool
simd_vectorize(struct simd_ctx *ctx)
{
	struct hir_block *F = ctx->loop;
	struct hir_block *G1 = F->parent;
	struct hir_block *FEXIT = F->succ;
	struct hir_block *GV, *GS, *XV, *XS, *RFOR, *SFOR, *EV, *ES;
	struct hir_stmt *body1;
	struct hir_stmt *body2;
	struct hir_stmt *tail;
	struct hir_expr *vg;
	int line = F->line;
	int i, j;

	/* Names and locals. */
	snprintf(ctx->mid_name, sizeof(ctx->mid_name), "$simd%d_mid",
		 simd_loop_seq);
	snprintf(ctx->vg_name, sizeof(ctx->vg_name), "$simd%d_vg",
		 simd_loop_seq);
	if (!hir_add_local(ctx->func, ctx->mid_name))
		return false;
	if (!hir_add_local(ctx->func, ctx->vg_name))
		return false;
	for (i = 0; i < ctx->base_count; i++) {
		struct simd_base *base = &ctx->bases[i];
		for (j = 0; j < base->off_count; j++) {
			if (base->off[j].shape == SIMD_SHAPE_I) {
				base->off[j].sb_name[0] = '\0';
				continue;
			}
			snprintf(base->off[j].sb_name,
				 sizeof(base->off[j].sb_name),
				 "$simd%d_sb%d_%d", simd_loop_seq, i, j);
			if (!hir_add_local(ctx->func, base->off[j].sb_name))
				return false;
		}
	}
	simd_loop_seq++;

	/* Clone the body twice BEFORE the vector rewrite. */
	body1 = simd_clone_stmt_list(F->val.for_.inner->val.basic.stmt_list);
	body2 = simd_clone_stmt_list(F->val.for_.inner->val.basic.stmt_list);
	if (body1 == NULL || body2 == NULL)
		return false;

	/* Rewrite the vector body in place. */
	{
		struct hir_stmt *s;
		for (s = F->val.for_.inner->val.basic.stmt_list;
		     s != NULL; s = s->next) {
			if (!simd_rewrite_expr(ctx, s->lhs))
				return false;
			if (!simd_rewrite_expr(ctx, s->rhs))
				return false;
		}
	}

	/* Append to B1: $mid, adjusted bases, $vg. */
	tail = ctx->b1->val.basic.stmt_list;
	assert(tail != NULL);
	while (tail->next != NULL)
		tail = tail->next;

#define SIMD_APPEND(stmt_expr)						\
	do {								\
		struct hir_stmt *ns_ = (stmt_expr);			\
		if (ns_ == NULL)					\
			return false;					\
		tail->next = ns_;					\
		tail = ns_;						\
	} while (0)

	/* $mid = $hi - (($hi - $lo) & 3) */
	SIMD_APPEND(simd_mk_assign(line, simd_mk_sym(ctx->mid_name),
		simd_mk_binary(HIR_EXPR_MINUS, simd_mk_sym(ctx->hi_name),
			simd_mk_binary(HIR_EXPR_AND,
				simd_mk_binary(HIR_EXPR_MINUS,
					simd_mk_sym(ctx->hi_name),
					simd_mk_sym(ctx->lo_name)),
				simd_mk_int(3)))));

	/* $sbS = $baseK (+|-) 4L * u */
	for (i = 0; i < ctx->base_count; i++) {
		struct simd_base *base = &ctx->bases[i];
		for (j = 0; j < base->off_count; j++) {
			struct simd_off *off = &base->off[j];
			int op;
			if (off->sb_name[0] == '\0')
				continue;
			op = (off->shape == SIMD_SHAPE_I_MINUS_U) ?
				HIR_EXPR_MINUS : HIR_EXPR_PLUS;
			SIMD_APPEND(simd_mk_assign(line,
				simd_mk_sym(off->sb_name),
				simd_mk_binary(op,
					simd_mk_sym(base->base_sym),
					simd_mk_binary(HIR_EXPR_MUL,
						simd_mk_long(4),
						simd_mk_u_expr(off)))));
		}
	}

	/* $vg = (0 <= $lo) && ($lo < $mid) &&
	 *       ($mid - $lo >= min_trip) && <disjointness terms> */
	vg = simd_mk_binary(HIR_EXPR_LAND,
		simd_mk_binary(HIR_EXPR_LTE, simd_mk_int(0),
			       simd_mk_sym(ctx->lo_name)),
		simd_mk_binary(HIR_EXPR_LT, simd_mk_sym(ctx->lo_name),
			       simd_mk_sym(ctx->mid_name)));
	if (vg == NULL)
		return false;
	vg = simd_mk_binary(HIR_EXPR_LAND, vg,
		simd_mk_binary(HIR_EXPR_GTE,
			simd_mk_binary(HIR_EXPR_MINUS,
				simd_mk_sym(ctx->mid_name),
				simd_mk_sym(ctx->lo_name)),
			simd_mk_int(ctx->min_trip)));
	if (vg == NULL)
		return false;
	for (i = 0; i < ctx->base_count; i++) {
		for (j = i + 1; j < ctx->base_count; j++) {
			struct simd_base *P = &ctx->bases[i];
			struct simd_base *Q = &ctx->bases[j];
			struct hir_expr *p_end, *q_end, *disj;
			bool same_offs;
			if (!P->has_store && !Q->has_store)
				continue;
			/* end = base + 4L * PLEN(packed) */
			p_end = simd_mk_binary(HIR_EXPR_PLUS,
				simd_mk_sym(P->base_sym),
				simd_mk_binary(HIR_EXPR_MUL, simd_mk_long(4),
					simd_mk_unary(HIR_EXPR_PLEN,
						simd_mk_sym(P->packed_sym))));
			q_end = simd_mk_binary(HIR_EXPR_PLUS,
				simd_mk_sym(Q->base_sym),
				simd_mk_binary(HIR_EXPR_MUL, simd_mk_long(4),
					simd_mk_unary(HIR_EXPR_PLEN,
						simd_mk_sym(Q->packed_sym))));
			disj = simd_mk_binary(HIR_EXPR_LOR,
				simd_mk_binary(HIR_EXPR_LTE, p_end,
					       simd_mk_sym(Q->base_sym)),
				simd_mk_binary(HIR_EXPR_LTE, q_end,
					       simd_mk_sym(P->base_sym)));
			/* Identical single offsets: same object is
			   element-aligned and safe -> allow equality. */
			same_offs = (!P->restricted && !Q->restricted &&
				     P->off_count == 1 && Q->off_count == 1 &&
				     simd_off_equal(&P->off[0], &Q->off[0]));
			if (same_offs) {
				disj = simd_mk_binary(HIR_EXPR_LOR, disj,
					simd_mk_binary(HIR_EXPR_EQ,
						simd_mk_sym(P->base_sym),
						simd_mk_sym(Q->base_sym)));
			}
			vg = simd_mk_binary(HIR_EXPR_LAND, vg, disj);
			if (vg == NULL)
				return false;
		}
	}
	SIMD_APPEND(simd_mk_assign(line, simd_mk_sym(ctx->vg_name), vg));

#undef SIMD_APPEND

	/* Build the sibling structure. */
	GV = simd_mk_block(HIR_BLOCK_IF, line, G1);
	XV = simd_mk_block(HIR_BLOCK_BASIC, line, G1);
	GS = simd_mk_block(HIR_BLOCK_IF, line, G1);
	XS = simd_mk_block(HIR_BLOCK_BASIC, line, G1);
	RFOR = simd_mk_block(HIR_BLOCK_FOR, line, GV);
	EV = simd_mk_block(HIR_BLOCK_BASIC, line, GV);
	SFOR = simd_mk_block(HIR_BLOCK_FOR, line, GS);
	ES = simd_mk_block(HIR_BLOCK_BASIC, line, GS);
	if (GV == NULL || XV == NULL || GS == NULL || XS == NULL ||
	    RFOR == NULL || EV == NULL || SFOR == NULL || ES == NULL)
		return false;

	/* RFOR (remainder): $mid..$hi, scalar clone 1. */
	RFOR->val.for_.is_ranged = true;
	RFOR->val.for_.counter_symbol = F->val.for_.counter_symbol;
	RFOR->val.for_.start = simd_mk_sym(ctx->mid_name);
	RFOR->val.for_.stop = simd_mk_sym(ctx->hi_name);
	if (RFOR->val.for_.start == NULL || RFOR->val.for_.stop == NULL)
		return false;
	RFOR->val.for_.typed_int_region = F->val.for_.typed_int_region;
	RFOR->val.for_.inner = simd_mk_block(HIR_BLOCK_BASIC, line, RFOR);
	if (RFOR->val.for_.inner == NULL)
		return false;
	RFOR->val.for_.inner->val.basic.stmt_list = body1;
	RFOR->val.for_.inner->stop = true;
	/* Loop-body tail convention: succ = the loop's inner (the
	   natural back edge; lir falls through to the incrementer). */
	RFOR->val.for_.inner->succ = RFOR->val.for_.inner;

	/* SFOR (unvectorized fallback): $lo..$hi, scalar clone 2. */
	SFOR->val.for_.is_ranged = true;
	SFOR->val.for_.counter_symbol = F->val.for_.counter_symbol;
	SFOR->val.for_.start = simd_mk_sym(ctx->lo_name);
	SFOR->val.for_.stop = simd_mk_sym(ctx->hi_name);
	if (SFOR->val.for_.start == NULL || SFOR->val.for_.stop == NULL)
		return false;
	SFOR->val.for_.typed_int_region = F->val.for_.typed_int_region;
	SFOR->val.for_.inner = simd_mk_block(HIR_BLOCK_BASIC, line, SFOR);
	if (SFOR->val.for_.inner == NULL)
		return false;
	SFOR->val.for_.inner->val.basic.stmt_list = body2;
	SFOR->val.for_.inner->stop = true;
	SFOR->val.for_.inner->succ = SFOR->val.for_.inner;

	/* F becomes the strip loop: $lo..$mid, vector body. */
	F->val.for_.stop = simd_mk_sym(ctx->mid_name);
	if (F->val.for_.stop == NULL)
		return false;
	F->val.for_.is_vector = true;
	F->val.for_.abce_fast = false;
	F->parent = GV;

	/* Wire the region: B1 -> GV{F -> RFOR -> EV} -> XV ->
	   GS{SFOR -> ES} -> XS -> FEXIT. */
	ctx->b1->succ = GV;
	GV->val.if_.cond = simd_mk_sym(ctx->vg_name);
	if (GV->val.if_.cond == NULL)
		return false;
	GV->val.if_.inner = F;
	GV->succ = XV;
	F->succ = RFOR;
	F->stop = false;
	RFOR->succ = EV;
	EV->stop = true;
	EV->succ = XV;
	XV->succ = GS;
	GS->val.if_.cond = simd_mk_unary(HIR_EXPR_NOT,
					 simd_mk_sym(ctx->vg_name));
	if (GS->val.if_.cond == NULL)
		return false;
	GS->val.if_.inner = SFOR;
	GS->succ = XS;
	SFOR->succ = ES;
	ES->stop = true;
	ES->succ = XS;
	XS->succ = FEXIT;

	return true;
}

/* Collect abce_fast loops (snapshot; the transform rewires blocks). */
static void
simd_collect_loops(struct hir_block *head, struct hir_block **loops,
		   int *count)
{
	struct hir_block *b;
	struct hir_block *c;

	b = head;
	while (b != NULL) {
		switch (b->type) {
		case HIR_BLOCK_IF:
			c = b;
			while (c != NULL) {
				if (c->val.if_.inner != NULL)
					simd_collect_loops(c->val.if_.inner,
							   loops, count);
				c = c->val.if_.chain_next;
			}
			break;
		case HIR_BLOCK_FOR:
			if (b->val.for_.abce_fast && *count < SIMD_MAX_LOOPS)
				loops[(*count)++] = b;
			if (b->val.for_.inner != NULL)
				simd_collect_loops(b->val.for_.inner,
						   loops, count);
			break;
		case HIR_BLOCK_WHILE:
			if (b->val.while_.inner != NULL)
				simd_collect_loops(b->val.while_.inner,
						   loops, count);
			break;
		default:
			break;
		}
		if (b->stop)
			break;
		b = b->succ;
	}
}

bool
hir_opt_simd_func(struct hir_block *func_block, bool simd_info)
{
	struct hir_block *loops[SIMD_MAX_LOOPS];
	int loop_count;
	int i;

	assert(func_block != NULL);
	assert(func_block->type == HIR_BLOCK_FUNC);

	if (getenv("NOCT_SIMD_DISABLE") != NULL)
		return true;
	if (func_block->val.func.inner == NULL)
		return true;

	loop_count = 0;
	simd_collect_loops(func_block->val.func.inner, loops, &loop_count);

	for (i = 0; i < loop_count; i++) {
		struct simd_ctx *ctx;
		struct hir_stmt *original_body;
		struct hir_stmt *inlined_body;

		ctx = hir_malloc(sizeof(struct simd_ctx));
		if (ctx == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(ctx, 0, sizeof(*ctx));
		ctx->func = func_block;
		ctx->loop = loops[i];
		ctx->counter = loops[i]->val.for_.counter_symbol;
		original_body = loops[i]->val.for_.inner->val.basic.stmt_list;
		inlined_body = simd_inline_temps(loops[i]);
		if (inlined_body != NULL)
			loops[i]->val.for_.inner->val.basic.stmt_list = inlined_body;

		simd_reject_reason = "?";
		if (!simd_check_body(ctx, loops[i]->val.for_.inner) ||
		    !simd_find_environment(ctx)) {
			loops[i]->val.for_.inner->val.basic.stmt_list = original_body;
			if (getenv("NOCT_SIMD_DEBUG") != NULL)
				fprintf(stderr,
					"SIMD: %s:%d: rejected (%s)\n",
					hir_file_name, loops[i]->line,
					simd_reject_reason);
			continue;
		}

		/* Frame budget: locals we would add. */
		{
			int adds = 2;	/* mid + vg */
			int k, j;
			for (k = 0; k < ctx->base_count; k++) {
				for (j = 0; j < ctx->bases[k].off_count; j++) {
					if (ctx->bases[k].off[j].shape !=
					    SIMD_SHAPE_I)
						adds++;
				}
			}
			if (simd_count_locals(ctx) + adds > 112) {
				loops[i]->val.for_.inner->val.basic.stmt_list =
					original_body;
				if (getenv("NOCT_SIMD_DEBUG") != NULL)
					fprintf(stderr,
						"SIMD: %s:%d: rejected (frame)\n",
						hir_file_name, loops[i]->line);
				continue;
			}
		}

		if (!simd_vectorize(ctx))
			return false;

		if (simd_info)
			fprintf(stderr,
				"SIMD: %s:%d: vectorized (%s)\n",
				hir_file_name, loops[i]->line,
				ctx->is_float ? "f32x4" : "i32x4");

		if (getenv("NOCT_SIMD_DEBUG") != NULL)
			fprintf(stderr,
				"SIMD: %s:%d: vectorized (bases=%d consts=%d inv=%d temps=%d depth=%d cost=%d mintrip=%d)\n",
				hir_file_name, loops[i]->line,
				ctx->base_count, ctx->const_count,
				ctx->inv_count, ctx->temp_count,
				ctx->max_depth, ctx->body_cost,
				ctx->min_trip);
	}

	return true;
}
