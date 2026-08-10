/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR Optimizer: ABCE pass.
 *
 * This file is compiled only when NOCT_ENABLE_OPTIMIZER is ON
 * (which defines NOCT_USE_OPTIMIZER).  It was split out of hir.c;
 * see docs/design/01-abce.md for the design and docs/design/05-cse.md
 * for the split.
 */

#include <noct/noct.h>
#include "hir.h"
#include "hir_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * ========================================================================
 * ABCE: Array Boundary Check Elimination for Packed.
 *
 * Versions eligible ranged-for loops into a guarded fast copy that
 * accesses a Packed.uint8 buffer base-relative without per-access
 * bounds checks.  Created HIR node kinds: PBASE/PLEN/PCHECK/TYPEIS/
 * PLOAD8U/PSTORE8.  See docs/design/01-abce.md for the full design,
 * eligibility rules (E1..E7), and the safepoint-freedom argument.
 *
 * This is a loop-versioning bounds-check elimination in the lineage
 * of Midkiff et al.; unlike SSA-based approaches such as ABCD, it
 * needs no SSA because the structured ranged-for hands the compiler
 * the induction variable and its bounds, and monotone affine indices
 * are proven safe by testing the two interval endpoints in a guard.
 *
 * References:
 *  [1] S. P. Midkiff, J. E. Moreira, M. Snir, "Optimizing Array
 *      Reference Checking in Java Programs," IBM Systems Journal,
 *      37(3), 1998.  (Loop versioning with guards; the direct
 *      ancestor of this design.)
 *  [2] R. Bodik, R. Gupta, V. Sarkar, "ABCD: Eliminating Array
 *      Bounds Checks on Demand," PLDI 2000.  (The classic SSA-based
 *      demand-driven approach, contrasted above.)
 *  [3] T. Wuerthinger, C. Wimmer, H. Moessenboeck, "Array Bounds
 *      Check Elimination for the Java HotSpot Client Compiler,"
 *      PPPJ 2007.  (Loop versioning in a production JIT.)
 * ========================================================================
 */

/* Limits. */
#define ABCE_MAX_BLOCKS		64	/* cloneable blocks in a body    */
#define ABCE_MAX_SITES		16	/* guarded subscript sites      */
#define ABCE_MAX_GUARDS		32	/* TYPEIS-guarded local reads   */
#define ABCE_GUARD_INT		1	/* affine-index local            */
#define ABCE_GUARD_BODY		2	/* scalar value local            */
#define ABCE_BODY_UNKNOWN	0
#define ABCE_BODY_INT		1
#define ABCE_BODY_F32		2
#define ABCE_MAX_ASSIGNED	32	/* assigned locals in a body     */
#define ABCE_MAX_LOOPS		16	/* candidate loops per function  */
#define ABCE_MAX_PACKED		4	/* packed locals per loop        */

/* A subscript site shape: p[i], p[i+u], p[u+i], p[i-u]. */
enum abce_shape {
	ABCE_SHAPE_I,		/* p[i]     */
	ABCE_SHAPE_I_PLUS_U,	/* p[i + u] */
	ABCE_SHAPE_U_PLUS_I,	/* p[u + i] */
	ABCE_SHAPE_I_MINUS_U	/* p[i - u] */
};

struct abce_site {
	int shape;
	bool u_is_const;
	int u_const;		/* if u_is_const */
	const char *u_name;	/* if !u_is_const (invariant local) */
	struct hir_expr *u_expr;	/* non-trivial loop-invariant expr */
	char hoist_name[32];	/* canonical local for u_expr */
	int packed_index;	/* owner packed (ctx->packed[]) */
};

/*
 * Packed element-type constant propagation (speculation seeding).
 *
 * A one-pass, flow-insensitive scan collecting, per local, the packed
 * element type it was created with (Packed.uint16(...) etc.), with a
 * one-level copy propagation run to a small fixpoint.  Soundness does
 * NOT depend on this analysis: the versioning guard re-checks the
 * element type at runtime (PCHECK), so a wrong fact only routes the
 * loop to the slow path.  This is deliberately NOT type inference.
 */

#define ABCE_MAX_FACTS	64
#define ABCE_FACT_TOP	(-2)	/* conflicting / opaque */

struct abce_facts {
	const char *name[ABCE_MAX_FACTS];
	int type[ABCE_MAX_FACTS];	/* NOCT_PACKED_* or ABCE_FACT_TOP */
	int count;
};

static int
abce_fact_find(struct abce_facts *f, const char *name)
{
	int i;

	for (i = 0; i < f->count; i++) {
		if (strcmp(f->name[i], name) == 0)
			return i;
	}
	return -1;
}

static void
abce_fact_meet(struct abce_facts *f, const char *name, int type)
{
	int i;

	i = abce_fact_find(f, name);
	if (i < 0) {
		if (f->count >= ABCE_MAX_FACTS)
			return;
		f->name[f->count] = name;
		f->type[f->count] = type;
		f->count++;
		return;
	}
	if (f->type[i] != type)
		f->type[i] = ABCE_FACT_TOP;
}

/* Recognize Packed.<ctor>(...) and yield its element type. */
static int
abce_packed_ctor_type(struct hir_expr *rhs)
{
	static const struct {
		const char *name;
		int type;
	} tbl[] = {
		{ "int8",    NOCT_PACKED_INT8 },
		{ "uint8",   NOCT_PACKED_UINT8 },
		{ "int16",   NOCT_PACKED_INT16 },
		{ "uint16",  NOCT_PACKED_UINT16 },
		{ "int32",   NOCT_PACKED_INT32 },
		{ "uint32",  NOCT_PACKED_UINT32 },
		{ "int64",   NOCT_PACKED_INT64 },
		{ "uint64",  NOCT_PACKED_UINT64 },
		{ "float32", NOCT_PACKED_FLOAT32 },
		{ "float64", NOCT_PACKED_FLOAT64 }
	};
	struct hir_expr *fn;
	const char *ctor;
	size_t i;

	if (rhs == NULL)
		return -1;
	if (rhs->type == HIR_EXPR_THISCALL) {
		fn = rhs->val.thiscall.obj;
		ctor = rhs->val.thiscall.func;
	} else if (rhs->type == HIR_EXPR_CALL) {
		struct hir_expr *dot = rhs->val.call.func;
		if (dot == NULL || dot->type != HIR_EXPR_DOT)
			return -1;
		fn = dot->val.dot.obj;
		ctor = dot->val.dot.symbol;
	} else {
		return -1;
	}
	if (fn == NULL || fn->type != HIR_EXPR_TERM ||
	    fn->val.term.term->type != HIR_TERM_SYMBOL ||
	    strcmp(fn->val.term.term->val.symbol, "Packed") != 0)
		return -1;
	for (i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
		if (strcmp(tbl[i].name, ctor) == 0)
			return tbl[i].type;
	}
	return -1;
}

/* Collect creation facts over a block chain (pass 1 and 2). */
static void
abce_facts_scan_chain(struct abce_facts *f, struct hir_block *head, int pass)
{
	struct hir_block *b;
	struct hir_block *c;
	struct hir_stmt *stmt;

	b = head;
	while (b != NULL) {
		switch (b->type) {
		case HIR_BLOCK_BASIC:
			stmt = b->val.basic.stmt_list;
			while (stmt != NULL) {
				if (stmt->lhs != NULL &&
				    stmt->lhs->type == HIR_EXPR_TERM &&
				    stmt->lhs->val.term.term->type == HIR_TERM_SYMBOL) {
					const char *x = stmt->lhs->val.term.term->val.symbol;
					int t = abce_packed_ctor_type(stmt->rhs);
					if (t >= 0) {
						abce_fact_meet(f, x, t);
					} else if (stmt->rhs != NULL &&
						   stmt->rhs->type == HIR_EXPR_TERM &&
						   stmt->rhs->val.term.term->type == HIR_TERM_SYMBOL) {
						/* Copy propagation: x = y.
						   Pass 0 leaves copies
						   unresolved; pass 1 pulls
						   the source's fact. */
						if (pass == 1) {
							int j = abce_fact_find(f,
								stmt->rhs->val.term.term->val.symbol);
							if (j >= 0 && f->type[j] >= 0)
								abce_fact_meet(f, x, f->type[j]);
						}
					} else {
						abce_fact_meet(f, x, ABCE_FACT_TOP);
					}
				}
				stmt = stmt->next;
			}
			break;
		case HIR_BLOCK_IF:
			c = b;
			while (c != NULL) {
				if (c->val.if_.inner != NULL)
					abce_facts_scan_chain(f, c->val.if_.inner, pass);
				c = c->val.if_.chain_next;
			}
			break;
		case HIR_BLOCK_FOR:
			if (b->val.for_.inner != NULL)
				abce_facts_scan_chain(f, b->val.for_.inner, pass);
			break;
		case HIR_BLOCK_WHILE:
			if (b->val.while_.inner != NULL)
				abce_facts_scan_chain(f, b->val.while_.inner, pass);
			break;
		default:
			break;
		}
		if (b->stop)
			break;
		b = b->succ;
	}
}

struct abce_ctx {
	struct hir_block *func_block;
	struct hir_block *loop;
	const char *counter;

	/*
	 * Packed locals accessed by the loop (multi-packed since design
	 * 06 Part A; the SIMD strip loop needs src+dst in one loop).
	 * Each packed gets its own element-type bet, PCHECK guard term,
	 * and $abceN_baseK hoisted base local.
	 */
	const char *packed[ABCE_MAX_PACKED];
	int packed_bet[ABCE_MAX_PACKED];	/* NOCT_PACKED_* per packed */
	int packed_count;
	const char *assigned[ABCE_MAX_ASSIGNED];
	int assigned_type[ABCE_MAX_ASSIGNED];
	int assigned_count;
	const char *guards[ABCE_MAX_GUARDS];	/* locals protected by TYPEIS */
	uint8_t guard_req[ABCE_MAX_GUARDS];	/* ABCE_GUARD_* bit mask */
	uint8_t guard_type[ABCE_MAX_GUARDS];	/* NOCT_VALUE_INT/FLOAT */
	int guard_count;
	int body_kind;			/* ABCE_BODY_* */
	bool saw_int_term;
	bool saw_float_term;
	bool saw_int_only_op;
	bool mixed_numeric;
	struct abce_site sites[ABCE_MAX_SITES];
	int site_count;
	/* Clone map. */
	struct hir_block *map_old[ABCE_MAX_BLOCKS];
	struct hir_block *map_new[ABCE_MAX_BLOCKS];
	int map_count;
	/* Hoisted local names. */
	char lo_name[32];
	char hi_name[32];
	char base_name[ABCE_MAX_PACKED][32];
	char g_name[32];

	/* Element-type bet (NOCT_PACKED_*) from constant propagation. */
	int bet;
	struct abce_facts *facts;
};

/* Per-function loop sequence number for unique $abceN names. */
static int abce_loop_seq;

/*
 * Small constructors.
 */

static struct hir_term *
abce_mk_term_int(int v)
{
	struct hir_term *t;

	t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(struct hir_term));
	t->type = HIR_TERM_INT;
	t->val.i = v;
	return t;
}

static struct hir_term *
abce_mk_term_symbol(const char *name)
{
	struct hir_term *t;

	t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(t, 0, sizeof(struct hir_term));
	t->type = HIR_TERM_SYMBOL;
	t->val.symbol = hir_strdup(name);
	if (t->val.symbol == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	return t;
}

static struct hir_expr *
abce_mk_expr_term(struct hir_term *t)
{
	struct hir_expr *e;

	if (t == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_TERM;
	e->val.term.term = t;
	return e;
}

static struct hir_expr *
abce_mk_expr_int(int v)
{
	return abce_mk_expr_term(abce_mk_term_int(v));
}

static struct hir_expr *
abce_mk_expr_symbol(const char *name)
{
	return abce_mk_expr_term(abce_mk_term_symbol(name));
}

static struct hir_expr *
abce_mk_binary(int type, struct hir_expr *e0, struct hir_expr *e1)
{
	struct hir_expr *e;

	if (e0 == NULL || e1 == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = type;
	e->val.binary.expr[0] = e0;
	e->val.binary.expr[1] = e1;
	return e;
}

static struct hir_expr *
abce_mk_unary(int type, struct hir_expr *e0)
{
	struct hir_expr *e;

	if (e0 == NULL)
		return NULL;
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = type;
	e->val.unary.expr = e0;
	return e;
}

static struct hir_stmt *
abce_mk_assign_stmt(int line, struct hir_expr *lhs, struct hir_expr *rhs)
{
	struct hir_stmt *s;

	if (rhs == NULL)
		return NULL;
	s = hir_malloc(sizeof(struct hir_stmt));
	if (s == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(s, 0, sizeof(struct hir_stmt));
	s->line = line;
	s->lhs = lhs;
	s->rhs = rhs;
	s->next = NULL;
	return s;
}

static struct hir_block *
abce_mk_block(int type, int line, struct hir_block *parent)
{
	struct hir_block *b;

	b = hir_malloc(sizeof(struct hir_block));
	if (b == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(b, 0, sizeof(struct hir_block));
	b->id = hir_next_block_id();
	b->type = type;
	b->line = line;
	b->parent = parent;
	return b;
}

/*
 * Eligibility scan.
 */

static bool
abce_is_local(struct abce_ctx *ctx, const char *name)
{
	struct hir_local *local;

	local = ctx->func_block->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, name) == 0)
			return true;
		local = local->next;
	}
	return false;
}

static bool
abce_add_guard(struct abce_ctx *ctx, const char *name, uint8_t req, int type)
{
	int i;

	for (i = 0; i < ctx->guard_count; i++) {
		if (strcmp(ctx->guards[i], name) == 0) {
			if (ctx->guard_type[i] != type)
				return false;
			ctx->guard_req[i] |= req;
			return true;
		}
	}
	if (ctx->guard_count >= ABCE_MAX_GUARDS)
		return false;	/* over the cap: ineligible */
	ctx->guards[ctx->guard_count] = name;
	ctx->guard_req[ctx->guard_count] = req;
	ctx->guard_type[ctx->guard_count] = (uint8_t)type;
	ctx->guard_count++;
	return true;
}

static int
abce_local_type(struct abce_ctx *ctx, const char *name)
{
	struct hir_local *local = ctx->func_block->val.func.local;
	int i;

	for (i = 0; i < ctx->assigned_count; i++) {
		if (strcmp(ctx->assigned[i], name) == 0)
			return ctx->assigned_type[i];
	}

	while (local != NULL) {
		if (strcmp(local->symbol, name) == 0)
			return local->proven_type;
		local = local->next;
	}
	return -1;
}

/* Pick the guarded element type for a packed local. */
static int
abce_bet_for_symbol(struct abce_ctx *ctx, const char *name)
{
	int k;

	if (ctx->facts == NULL)
		return NOCT_PACKED_UINT8;
	k = abce_fact_find(ctx->facts, name);
	if (k < 0 || ctx->facts->type[k] < 0)
		return NOCT_PACKED_UINT8;
	return ctx->facts->type[k];
}

static bool
abce_add_assigned(struct abce_ctx *ctx, const char *name)
{
	int i;

	for (i = 0; i < ctx->assigned_count; i++) {
		if (strcmp(ctx->assigned[i], name) == 0)
			return true;
	}
	if (ctx->assigned_count >= ABCE_MAX_ASSIGNED)
		return false;
	ctx->assigned[ctx->assigned_count] = name;
	ctx->assigned_type[ctx->assigned_count++] = -1;
	return true;
}

static void
abce_set_assigned_type(struct abce_ctx *ctx, const char *name, int type)
{
	int i;
	for (i = 0; i < ctx->assigned_count; i++) {
		if (strcmp(ctx->assigned[i], name) == 0) {
			ctx->assigned_type[i] = type;
			return;
		}
	}
}

static bool
abce_is_assigned(struct abce_ctx *ctx, const char *name)
{
	int i;

	for (i = 0; i < ctx->assigned_count; i++) {
		if (strcmp(ctx->assigned[i], name) == 0)
			return true;
	}
	return false;
}

static bool
abce_expr_equal(struct hir_expr *a, struct hir_expr *b)
{
	if (a == NULL || b == NULL || a->type != b->type)
		return false;
	if (a->type == HIR_EXPR_TERM) {
		struct hir_term *ta = a->val.term.term;
		struct hir_term *tb = b->val.term.term;
		if (ta->type != tb->type)
			return false;
		if (ta->type == HIR_TERM_INT)
			return ta->val.i == tb->val.i;
		if (ta->type == HIR_TERM_SYMBOL)
			return strcmp(ta->val.symbol, tb->val.symbol) == 0;
		return false;
	}
	if (a->type == HIR_EXPR_PAR)
		return abce_expr_equal(a->val.unary.expr, b->val.unary.expr);
	return abce_expr_equal(a->val.binary.expr[0], b->val.binary.expr[0]) &&
	       abce_expr_equal(a->val.binary.expr[1], b->val.binary.expr[1]);
}

/* Pure int expression whose local reads are invariant in this loop. */
static bool
abce_check_invariant_u(struct abce_ctx *ctx, struct hir_expr *e)
{
	if (e->type == HIR_EXPR_TERM) {
		if (e->val.term.term->type == HIR_TERM_INT)
			return true;
		if (e->val.term.term->type != HIR_TERM_SYMBOL)
			return false;
		if (strcmp(e->val.term.term->val.symbol, ctx->counter) == 0 ||
		    !abce_is_local(ctx, e->val.term.term->val.symbol) ||
		    abce_is_assigned(ctx, e->val.term.term->val.symbol))
			return false;
		return abce_add_guard(ctx, e->val.term.term->val.symbol,
				      ABCE_GUARD_INT, NOCT_VALUE_INT);
	}
	if (e->type == HIR_EXPR_PAR)
		return abce_check_invariant_u(ctx, e->val.unary.expr);
	if (e->type != HIR_EXPR_PLUS && e->type != HIR_EXPR_MINUS &&
	    e->type != HIR_EXPR_MUL && e->type != HIR_EXPR_AND &&
	    e->type != HIR_EXPR_OR && e->type != HIR_EXPR_XOR &&
	    e->type != HIR_EXPR_SHL && e->type != HIR_EXPR_SHR)
		return false;
	return abce_check_invariant_u(ctx, e->val.binary.expr[0]) &&
	       abce_check_invariant_u(ctx, e->val.binary.expr[1]);
}

/* Register a subscript site; validate the affine shape. */
static bool
abce_check_site(struct abce_ctx *ctx, struct hir_expr *subscr)
{
	struct hir_expr *base;
	struct hir_expr *f;
	struct abce_site site;
	struct hir_expr *a;
	struct hir_expr *b;
	int i;

	base = subscr->val.binary.expr[0];
	f = subscr->val.binary.expr[1];

	/* Base must be a plain local symbol term. */
	if (base->type != HIR_EXPR_TERM)
		return false;
	if (base->val.term.term->type != HIR_TERM_SYMBOL)
		return false;
	if (!abce_is_local(ctx, base->val.term.term->val.symbol))
		return false;

	/* Find or register the owner packed local. */
	{
		const char *sym = base->val.term.term->val.symbol;
		int bet = abce_bet_for_symbol(ctx, sym);
		int body_kind;
		int k;

		if (bet == NOCT_PACKED_FLOAT64)
			return false;
		body_kind = bet == NOCT_PACKED_FLOAT32 ?
			ABCE_BODY_F32 : ABCE_BODY_INT;
		if (ctx->body_kind != ABCE_BODY_UNKNOWN &&
		    ctx->body_kind != body_kind)
			return false;
		ctx->body_kind = body_kind;
		for (k = 0; k < ctx->packed_count; k++) {
			if (strcmp(ctx->packed[k], sym) == 0)
				break;
		}
		if (k == ctx->packed_count) {
			if (ctx->packed_count >= ABCE_MAX_PACKED)
				return false;
			ctx->packed[ctx->packed_count] = sym;
			ctx->packed_bet[ctx->packed_count] = bet;
			ctx->packed_count++;
		}
		memset(&site, 0, sizeof(site));
		site.packed_index = k;
	}

	/* Match the index shape. */
	if (f->type == HIR_EXPR_TERM &&
	    f->val.term.term->type == HIR_TERM_SYMBOL &&
	    strcmp(f->val.term.term->val.symbol, ctx->counter) == 0) {
		site.shape = ABCE_SHAPE_I;
	} else if (f->type == HIR_EXPR_PLUS || f->type == HIR_EXPR_MINUS) {
		a = f->val.binary.expr[0];
		b = f->val.binary.expr[1];
		if (a->type == HIR_EXPR_TERM &&
		    a->val.term.term->type == HIR_TERM_SYMBOL &&
		    strcmp(a->val.term.term->val.symbol, ctx->counter) == 0) {
			/* i + u  or  i - u */
			site.shape = (f->type == HIR_EXPR_PLUS) ?
				ABCE_SHAPE_I_PLUS_U : ABCE_SHAPE_I_MINUS_U;
			if (b->type == HIR_EXPR_TERM &&
			    b->val.term.term->type == HIR_TERM_INT) {
				site.u_is_const = true;
				site.u_const = b->val.term.term->val.i;
			} else if (b->type == HIR_EXPR_TERM &&
				   b->val.term.term->type == HIR_TERM_SYMBOL) {
				site.u_name = b->val.term.term->val.symbol;
			} else {
				site.u_expr = b;
			}
		} else if (f->type == HIR_EXPR_PLUS &&
			   b->type == HIR_EXPR_TERM &&
			   b->val.term.term->type == HIR_TERM_SYMBOL &&
			   strcmp(b->val.term.term->val.symbol, ctx->counter) == 0) {
			/* u + i */
			site.shape = ABCE_SHAPE_U_PLUS_I;
			if (a->type == HIR_EXPR_TERM &&
			    a->val.term.term->type == HIR_TERM_INT) {
				site.u_is_const = true;
				site.u_const = a->val.term.term->val.i;
			} else if (a->type == HIR_EXPR_TERM &&
				   a->val.term.term->type == HIR_TERM_SYMBOL) {
				site.u_name = a->val.term.term->val.symbol;
			} else {
				site.u_expr = a;
			}
		} else {
			return false;
		}
		if (site.u_name != NULL) {
			if (strcmp(site.u_name, ctx->counter) == 0)
				return false;	/* i+i: coefficient 2 */
			if (!abce_is_local(ctx, site.u_name))
				return false;	/* global: not invariant */
			if (!abce_add_guard(ctx, site.u_name, ABCE_GUARD_INT,
					    NOCT_VALUE_INT))
				return false;
		}
	} else {
		return false;
	}

	/* Dedup structurally-equal sites. */
	for (i = 0; i < ctx->site_count; i++) {
		struct abce_site *o = &ctx->sites[i];
		if (o->packed_index != site.packed_index)
			continue;
		if (o->shape != site.shape)
			continue;
		if (o->u_is_const != site.u_is_const)
			continue;
		if (site.u_is_const && o->u_const == site.u_const)
			return true;
		if (!site.u_is_const && site.u_name != NULL &&
		    o->u_name != NULL &&
		    strcmp(o->u_name, site.u_name) == 0)
			return true;
		if (!site.u_is_const && site.u_expr != NULL &&
		    o->u_expr != NULL && abce_expr_equal(o->u_expr, site.u_expr))
			return true;
		if (site.shape == ABCE_SHAPE_I)
			return true;
	}
	if (ctx->site_count >= ABCE_MAX_SITES)
		return false;
	ctx->sites[ctx->site_count++] = site;
	return true;
}

/* Safe-expression walk: proves the fast body cannot allocate. */
static bool
abce_check_expr(struct abce_ctx *ctx, struct hir_expr *expr)
{
	struct hir_term *t;

	switch (expr->type) {
	case HIR_EXPR_TERM:
		t = expr->val.term.term;
		switch (t->type) {
		case HIR_TERM_INT:
			ctx->saw_int_term = true;
			return true;
		case HIR_TERM_FLOAT:
			ctx->saw_float_term = true;
			ctx->mixed_numeric = true;
			return true;
		case HIR_TERM_LONG:
		case HIR_TERM_DOUBLE:
			return false;
		case HIR_TERM_SYMBOL:
			if (strcmp(t->val.symbol, ctx->counter) == 0)
				return true;
			if (!abce_is_local(ctx, t->val.symbol))
				return false;	/* global read */
			/* A value defined earlier in this straight-line walk is
			   re-established by the fast body and needs no entry guard. */
			if (abce_is_assigned(ctx, t->val.symbol))
				return true;
			{
				int type = abce_local_type(ctx, t->val.symbol);
				if (type != NOCT_VALUE_INT && type != NOCT_VALUE_FLOAT)
					return false;
				if (type == NOCT_VALUE_FLOAT)
					ctx->mixed_numeric = true;
				return abce_add_guard(ctx, t->val.symbol,
						      ABCE_GUARD_BODY, type);
			}
		default:
			/* STRING / EMPTY_ARRAY / EMPTY_DICT allocate. */
			return false;
		}
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
		return abce_check_expr(ctx, expr->val.unary.expr);
	case HIR_EXPR_NOT:
		ctx->saw_int_only_op = true;
		return abce_check_expr(ctx, expr->val.unary.expr);
	case HIR_EXPR_CALL:
		if (expr->val.call.arg_count != 1 ||
		    hir_get_intrinsic_call(expr) == HIR_INTRINSIC_NONE)
			return false;
		ctx->mixed_numeric = true;
		return abce_check_expr(ctx, expr->val.call.arg[0]);
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
	case HIR_EXPR_LAND:
	case HIR_EXPR_LOR:
		if (expr->type != HIR_EXPR_PLUS &&
		    expr->type != HIR_EXPR_MINUS &&
		    expr->type != HIR_EXPR_MUL &&
		    expr->type != HIR_EXPR_DIV) {
			ctx->saw_int_only_op = true;
		}
		if (!abce_check_expr(ctx, expr->val.binary.expr[0]))
			return false;
		if (!abce_check_expr(ctx, expr->val.binary.expr[1]))
			return false;
		return true;
	case HIR_EXPR_SUBSCR:
		return abce_check_site(ctx, expr);
	default:
		/* DOT, CALL, THISCALL, ARRAY, DICT, NEW, ... */
		return false;
	}
}

/* Exact scalar tag required by raw PSTORE lowering. */
static int
abce_expr_type(struct abce_ctx *ctx, struct hir_expr *expr)
{
	int a, b;

	switch (expr->type) {
	case HIR_EXPR_TERM:
		if (expr->val.term.term->type == HIR_TERM_INT)
			return NOCT_VALUE_INT;
		if (expr->val.term.term->type == HIR_TERM_FLOAT)
			return NOCT_VALUE_FLOAT;
		if (expr->val.term.term->type == HIR_TERM_SYMBOL)
			return abce_local_type(ctx,
				expr->val.term.term->val.symbol);
		return -1;
	case HIR_EXPR_PAR:
		return abce_expr_type(ctx, expr->val.unary.expr);
	case HIR_EXPR_NEG:
		a = abce_expr_type(ctx, expr->val.unary.expr);
		return a == NOCT_VALUE_INT || a == NOCT_VALUE_FLOAT ? a : -1;
	case HIR_EXPR_PLOAD8U:
	case HIR_EXPR_PLOAD8S:
	case HIR_EXPR_PLOAD16U:
	case HIR_EXPR_PLOAD16S:
	case HIR_EXPR_PLOAD32:
		return NOCT_VALUE_INT;
	case HIR_EXPR_PLOADF32:
		return NOCT_VALUE_FLOAT;
	case HIR_EXPR_SUBSCR:
		if (expr->val.binary.expr[0]->type == HIR_EXPR_TERM &&
		    expr->val.binary.expr[0]->val.term.term->type ==
			HIR_TERM_SYMBOL) {
			int bet = abce_bet_for_symbol(ctx,
				expr->val.binary.expr[0]->val.term.term->val.symbol);
			return bet == NOCT_PACKED_FLOAT32 ?
				NOCT_VALUE_FLOAT : NOCT_VALUE_INT;
		}
		return -1;
	case HIR_EXPR_CALL:
		switch (hir_get_intrinsic_call(expr)) {
		case HIR_INTRINSIC_INT_FROM: return NOCT_VALUE_INT;
		case HIR_INTRINSIC_FLOAT_FROM: return NOCT_VALUE_FLOAT;
		default: return -1;
		}
	case HIR_EXPR_PLUS:
	case HIR_EXPR_MINUS:
	case HIR_EXPR_MUL:
	case HIR_EXPR_DIV:
		a = abce_expr_type(ctx, expr->val.binary.expr[0]);
		b = abce_expr_type(ctx, expr->val.binary.expr[1]);
		if (a == NOCT_VALUE_FLOAT || b == NOCT_VALUE_FLOAT)
			return (a == NOCT_VALUE_INT || a == NOCT_VALUE_FLOAT) &&
			       (b == NOCT_VALUE_INT || b == NOCT_VALUE_FLOAT) ?
			       NOCT_VALUE_FLOAT : -1;
		return a == NOCT_VALUE_INT && b == NOCT_VALUE_INT ?
			NOCT_VALUE_INT : -1;
	case HIR_EXPR_MOD:
	case HIR_EXPR_AND:
	case HIR_EXPR_OR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		a = abce_expr_type(ctx, expr->val.binary.expr[0]);
		b = abce_expr_type(ctx, expr->val.binary.expr[1]);
		return a == NOCT_VALUE_INT && b == NOCT_VALUE_INT ?
			NOCT_VALUE_INT : -1;
	default:
		return -1;
	}
}

static bool
abce_check_stmt(struct abce_ctx *ctx, struct hir_stmt *stmt)
{
	const char *name;

	if (stmt->lhs == NULL) {
		/* Bare expression statement. */
		return abce_check_expr(ctx, stmt->rhs);
	}
	if (stmt->lhs->type == HIR_EXPR_TERM) {
		if (stmt->lhs->val.term.term->type != HIR_TERM_SYMBOL)
			return false;
		name = stmt->lhs->val.term.term->val.symbol;
		if (strcmp(name, ctx->counter) == 0)
			return false;
		if (strcmp(name, "$return") != 0) {
			if (!abce_is_local(ctx, name))
				return false;
			if (!abce_add_assigned(ctx, name))
				return false;
		}
		if (!abce_check_expr(ctx, stmt->rhs))
			return false;
		abce_set_assigned_type(ctx, name,
				       abce_expr_type(ctx, stmt->rhs));
		return true;
	}
	if (stmt->lhs->type == HIR_EXPR_SUBSCR) {
		if (!abce_check_site(ctx, stmt->lhs))
			return false;
		if (!abce_check_expr(ctx, stmt->rhs))
			return false;
		if (abce_expr_type(ctx, stmt->rhs) !=
		    (ctx->body_kind == ABCE_BODY_F32 ?
		     NOCT_VALUE_FLOAT : NOCT_VALUE_INT))
			return false;
		return true;
	}
	/* DOT store or anything else. */
	return false;
}

/* Scan a body block chain; reject non-BASIC/IF blocks and continues. */
static bool
abce_scan_chain(struct abce_ctx *ctx, struct hir_block *head)
{
	struct hir_block *b;
	struct hir_block *c;
	struct hir_stmt *stmt;

	b = head;
	while (b != NULL) {
		/* A continue edge targets the loop inner from a nested block. */
		if (b->stop &&
		    b->succ == ctx->loop->val.for_.inner &&
		    b->parent != ctx->loop)
			return false;

		switch (b->type) {
		case HIR_BLOCK_BASIC:
			stmt = b->val.basic.stmt_list;
			while (stmt != NULL) {
				if (!abce_check_stmt(ctx, stmt))
					return false;
				stmt = stmt->next;
			}
			break;
		case HIR_BLOCK_IF:
			c = b;
			while (c != NULL) {
				if (c->val.if_.cond != NULL) {
					if (!abce_check_expr(ctx, c->val.if_.cond))
						return false;
				}
				if (c->val.if_.inner != NULL) {
					if (!abce_scan_chain(ctx, c->val.if_.inner))
						return false;
				}
				c = c->val.if_.chain_next;
			}
			break;
		default:
			/* Nested FOR/WHILE etc. */
			return false;
		}

		if (b->stop)
			break;
		b = b->succ;
	}
	return true;
}

static bool
abce_check_eligibility(struct abce_ctx *ctx)
{
	struct hir_block *loop;
	int i;

	loop = ctx->loop;
	if (!loop->val.for_.is_ranged)
		return false;
	if (loop->val.for_.counter_symbol == NULL)
		return false;
	if (loop->val.for_.start == NULL || loop->val.for_.stop == NULL)
		return false;
	if (loop->val.for_.inner == NULL)
		return false;
	if (loop->stop)
		return false;	/* defensive: FOR is never a tail today */

	ctx->counter = loop->val.for_.counter_symbol;

	if (!abce_scan_chain(ctx, loop->val.for_.inner))
		return false;

	/* At least one packed access, otherwise nothing to gain. */
	if (ctx->packed_count == 0 || ctx->site_count == 0)
		return false;

	/* Every packed and every affine u must be loop-invariant. */
	for (i = 0; i < ctx->packed_count; i++) {
		if (abce_is_assigned(ctx, ctx->packed[i]))
			return false;
	}
	for (i = 0; i < ctx->site_count; i++) {
		if (!ctx->sites[i].u_is_const &&
		    ctx->sites[i].u_name != NULL &&
		    abce_is_assigned(ctx, ctx->sites[i].u_name))
			return false;
		if (ctx->sites[i].u_expr != NULL &&
		    !abce_check_invariant_u(ctx, ctx->sites[i].u_expr))
			return false;
	}

	if (ctx->body_kind == ABCE_BODY_UNKNOWN)
		return false;

	/*
	 * Multi-packed loops reject 64-bit bets outright: a PLOAD64
	 * yields a long, and letting it flow into another packed's
	 * PSTORE8/16/32 (which assume an int-tagged source) would write
	 * raw long bits.  Single-packed 64-bit loops keep today's
	 * behavior (loads and stores agree on the width; PSTORE64
	 * dispatches on int/long).
	 */
	if (ctx->packed_count > 1) {
		for (i = 0; i < ctx->packed_count; i++) {
			if (ctx->packed_bet[i] == NOCT_PACKED_INT64 ||
			    ctx->packed_bet[i] == NOCT_PACKED_UINT64)
				return false;
		}
	}

	return true;
}

/*
 * Cloning.
 */

static struct hir_term *
abce_clone_term(struct hir_term *t)
{
	struct hir_term *n;

	n = hir_malloc(sizeof(struct hir_term));
	if (n == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(n, 0, sizeof(struct hir_term));
	n->type = t->type;
	switch (t->type) {
	case HIR_TERM_SYMBOL:
		n->val.symbol = hir_strdup(t->val.symbol);
		if (n->val.symbol == NULL)
			return NULL;
		break;
	case HIR_TERM_STRING:
		n->val.s = hir_strdup(t->val.s);
		if (n->val.s == NULL)
			return NULL;
		break;
	default:
		n->val = t->val;
		break;
	}
	return n;
}

static struct hir_expr *
abce_clone_expr(struct hir_expr *e)
{
	struct hir_expr *n;
	uint32_t i;

	if (e == NULL)
		return NULL;
	n = hir_malloc(sizeof(struct hir_expr));
	if (n == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(n, 0, sizeof(struct hir_expr));
	n->type = e->type;
	switch (e->type) {
	case HIR_EXPR_TERM:
		n->val.term.term = abce_clone_term(e->val.term.term);
		if (n->val.term.term == NULL)
			return NULL;
		break;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
		n->val.unary.expr = abce_clone_expr(e->val.unary.expr);
		if (n->val.unary.expr == NULL)
			return NULL;
		break;
	case HIR_EXPR_DOT:
		n->val.dot.obj = abce_clone_expr(e->val.dot.obj);
		n->val.dot.symbol = hir_strdup(e->val.dot.symbol);
		if (n->val.dot.obj == NULL || n->val.dot.symbol == NULL)
			return NULL;
		break;
	case HIR_EXPR_CALL:
		n->val.call.func = abce_clone_expr(e->val.call.func);
		if (n->val.call.func == NULL)
			return NULL;
		n->val.call.arg_count = e->val.call.arg_count;
		for (i = 0; i < e->val.call.arg_count; i++) {
			n->val.call.arg[i] = abce_clone_expr(e->val.call.arg[i]);
			if (n->val.call.arg[i] == NULL)
				return NULL;
		}
		break;
	case HIR_EXPR_THISCALL:
		n->val.thiscall.obj = abce_clone_expr(e->val.thiscall.obj);
		n->val.thiscall.func = hir_strdup(e->val.thiscall.func);
		if (n->val.thiscall.obj == NULL || n->val.thiscall.func == NULL)
			return NULL;
		n->val.thiscall.arg_count = e->val.thiscall.arg_count;
		for (i = 0; i < e->val.thiscall.arg_count; i++) {
			n->val.thiscall.arg[i] = abce_clone_expr(e->val.thiscall.arg[i]);
			if (n->val.thiscall.arg[i] == NULL)
				return NULL;
		}
		break;
	default:
		/* Binary operators (incl. SUBSCR, LAND, LOR). */
		n->val.binary.expr[0] = abce_clone_expr(e->val.binary.expr[0]);
		n->val.binary.expr[1] = abce_clone_expr(e->val.binary.expr[1]);
		if (n->val.binary.expr[0] == NULL ||
		    n->val.binary.expr[1] == NULL)
			return NULL;
		break;
	}
	return n;
}

static struct hir_stmt *
abce_clone_stmt_list(struct hir_stmt *head)
{
	struct hir_stmt *n_head;
	struct hir_stmt *n_tail;
	struct hir_stmt *s;
	struct hir_stmt *n;

	n_head = NULL;
	n_tail = NULL;
	s = head;
	while (s != NULL) {
		n = hir_malloc(sizeof(struct hir_stmt));
		if (n == NULL) {
			hir_out_of_memory();
			return NULL;
		}
		memset(n, 0, sizeof(struct hir_stmt));
		n->line = s->line;
		if (s->lhs != NULL) {
			n->lhs = abce_clone_expr(s->lhs);
			if (n->lhs == NULL)
				return NULL;
		}
		n->rhs = abce_clone_expr(s->rhs);
		if (n->rhs == NULL)
			return NULL;
		if (n_head == NULL)
			n_head = n;
		else
			n_tail->next = n;
		n_tail = n;
		s = s->next;
	}
	return n_head;
}

/* Collect every block of a body subtree into the clone map. */
static bool
abce_collect_blocks(struct abce_ctx *ctx, struct hir_block *head)
{
	struct hir_block *b;
	struct hir_block *c;

	b = head;
	while (b != NULL) {
		if (ctx->map_count >= ABCE_MAX_BLOCKS)
			return false;
		ctx->map_old[ctx->map_count] = b;
		ctx->map_new[ctx->map_count] = NULL;
		ctx->map_count++;

		if (b->type == HIR_BLOCK_IF) {
			c = b;
			while (c != NULL) {
				if (c != b) {
					if (ctx->map_count >= ABCE_MAX_BLOCKS)
						return false;
					ctx->map_old[ctx->map_count] = c;
					ctx->map_new[ctx->map_count] = NULL;
					ctx->map_count++;
				}
				if (c->val.if_.inner != NULL) {
					if (!abce_collect_blocks(ctx, c->val.if_.inner))
						return false;
				}
				c = c->val.if_.chain_next;
			}
		}

		if (b->stop)
			break;
		b = b->succ;
	}
	return true;
}

static struct hir_block *
abce_map_block(struct abce_ctx *ctx, struct hir_block *old)
{
	int i;

	if (old == NULL)
		return NULL;
	for (i = 0; i < ctx->map_count; i++) {
		if (ctx->map_old[i] == old)
			return ctx->map_new[i];
	}
	return NULL;
}

/* Clone every collected block (fields; pointer fixup comes after). */
static bool
abce_clone_blocks(struct abce_ctx *ctx)
{
	struct hir_block *o;
	struct hir_block *n;
	int i;

	for (i = 0; i < ctx->map_count; i++) {
		o = ctx->map_old[i];
		n = abce_mk_block(o->type, o->line, NULL);
		if (n == NULL)
			return false;
		n->stop = o->stop;
		switch (o->type) {
		case HIR_BLOCK_BASIC:
			n->val.basic.stmt_list =
				abce_clone_stmt_list(o->val.basic.stmt_list);
			if (o->val.basic.stmt_list != NULL &&
			    n->val.basic.stmt_list == NULL)
				return false;
			break;
		case HIR_BLOCK_IF:
			if (o->val.if_.cond != NULL) {
				n->val.if_.cond = abce_clone_expr(o->val.if_.cond);
				if (n->val.if_.cond == NULL)
					return false;
			}
			break;
		default:
			return false;
		}
		ctx->map_new[i] = n;
	}

	/* Fix up graph pointers. */
	for (i = 0; i < ctx->map_count; i++) {
		struct hir_block *mapped;
		o = ctx->map_old[i];
		n = ctx->map_new[i];

		mapped = abce_map_block(ctx, o->parent);
		n->parent = mapped;	/* NULL = direct child of loop; set later */

		mapped = abce_map_block(ctx, o->succ);
		if (mapped != NULL)
			n->succ = mapped;
		else
			n->succ = o->succ;	/* exit/back/END edge; set later */

		if (o->type == HIR_BLOCK_IF) {
			n->val.if_.inner = abce_map_block(ctx, o->val.if_.inner);
			n->val.if_.chain_next = abce_map_block(ctx, o->val.if_.chain_next);
			n->val.if_.chain_prev = abce_map_block(ctx, o->val.if_.chain_prev);
		}
	}
	return true;
}

/*
 * Fast-body rewrite: SUBSCR on the packed -> PLOAD8U/PSTORE8.
 */

/* Load/store HIR kinds for the element-type bet. */
static int
abce_load_kind(int bet)
{
	switch (bet) {
	case NOCT_PACKED_INT8:   return HIR_EXPR_PLOAD8S;
	case NOCT_PACKED_UINT8:  return HIR_EXPR_PLOAD8U;
	case NOCT_PACKED_INT16:  return HIR_EXPR_PLOAD16S;
	case NOCT_PACKED_UINT16: return HIR_EXPR_PLOAD16U;
	case NOCT_PACKED_INT32:  return HIR_EXPR_PLOAD32;
	case NOCT_PACKED_UINT32: return HIR_EXPR_PLOAD32;
	case NOCT_PACKED_INT64:  return HIR_EXPR_PLOAD64;
	case NOCT_PACKED_UINT64: return HIR_EXPR_PLOAD64;
	case NOCT_PACKED_FLOAT32:return HIR_EXPR_PLOADF32;
	default:                 return HIR_EXPR_PLOAD8U;
	}
}

static int
abce_store_kind(int bet)
{
	switch (bet) {
	case NOCT_PACKED_INT8:   return HIR_EXPR_PSTORE8;
	case NOCT_PACKED_UINT8:  return HIR_EXPR_PSTORE8;
	case NOCT_PACKED_INT16:  return HIR_EXPR_PSTORE16;
	case NOCT_PACKED_UINT16: return HIR_EXPR_PSTORE16;
	case NOCT_PACKED_INT32:  return HIR_EXPR_PSTORE32;
	case NOCT_PACKED_UINT32: return HIR_EXPR_PSTORE32;
	case NOCT_PACKED_INT64:  return HIR_EXPR_PSTORE64;
	case NOCT_PACKED_UINT64: return HIR_EXPR_PSTORE64;
	case NOCT_PACKED_FLOAT32:return HIR_EXPR_PSTOREF32;
	default:                 return HIR_EXPR_PSTORE8;
	}
}

/* Return the owner packed index of a p[f] subscript, or -1. */
static int
abce_packed_subscr_index(struct abce_ctx *ctx, struct hir_expr *e)
{
	const char *sym;
	int k;

	if (e->type != HIR_EXPR_SUBSCR)
		return -1;
	if (e->val.binary.expr[0]->type != HIR_EXPR_TERM)
		return -1;
	if (e->val.binary.expr[0]->val.term.term->type != HIR_TERM_SYMBOL)
		return -1;
	sym = e->val.binary.expr[0]->val.term.term->val.symbol;
	for (k = 0; k < ctx->packed_count; k++) {
		if (strcmp(ctx->packed[k], sym) == 0)
			return k;
	}
	return -1;
}

static bool
abce_canonicalize_index(struct abce_ctx *ctx, int packed_index,
			struct hir_expr *f)
{
	int i;

	for (i = 0; i < ctx->site_count; i++) {
		struct abce_site *site = &ctx->sites[i];
		struct hir_expr *u = NULL;
		struct hir_expr *n;
		if (site->packed_index != packed_index ||
		    site->u_expr == NULL || site->hoist_name[0] == '\0')
			continue;
		if (f->type != HIR_EXPR_PLUS && f->type != HIR_EXPR_MINUS)
			continue;
		if (site->shape == ABCE_SHAPE_I_PLUS_U ||
		    site->shape == ABCE_SHAPE_I_MINUS_U)
			u = f->val.binary.expr[1];
		else if (site->shape == ABCE_SHAPE_U_PLUS_I)
			u = f->val.binary.expr[0];
		if (u == NULL || !abce_expr_equal(u, site->u_expr))
			continue;
		if (site->shape == ABCE_SHAPE_U_PLUS_I)
			n = abce_mk_binary(HIR_EXPR_PLUS,
				abce_mk_expr_symbol(site->hoist_name),
				abce_mk_expr_symbol(ctx->counter));
		else
			n = abce_mk_binary(f->type,
				abce_mk_expr_symbol(ctx->counter),
				abce_mk_expr_symbol(site->hoist_name));
		if (n == NULL)
			return false;
		*f = *n;
		return true;
	}
	return true;
}

static bool
abce_rewrite_expr(struct abce_ctx *ctx, struct hir_expr *e)
{
	uint32_t i;

	if (e == NULL)
		return true;
	{
		int k = abce_packed_subscr_index(ctx, e);
		if (k >= 0) {
			/* p[f]  ->  PLOAD*($baseK, f) */
			struct hir_expr *base_term;
			base_term = abce_mk_expr_symbol(ctx->base_name[k]);
			if (base_term == NULL)
				return false;
			e->type = abce_load_kind(ctx->packed_bet[k]);
			e->val.binary.expr[0] = base_term;
			/* expr[1] (the offset) stays. */
			if (!abce_canonicalize_index(ctx, k,
						      e->val.binary.expr[1]))
				return false;
			return abce_rewrite_expr(ctx, e->val.binary.expr[1]);
		}
	}
	switch (e->type) {
	case HIR_EXPR_TERM:
		return true;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
		return abce_rewrite_expr(ctx, e->val.unary.expr);
	case HIR_EXPR_DOT:
		return abce_rewrite_expr(ctx, e->val.dot.obj);
	case HIR_EXPR_CALL:
		if (!abce_rewrite_expr(ctx, e->val.call.func))
			return false;
		for (i = 0; i < e->val.call.arg_count; i++) {
			if (!abce_rewrite_expr(ctx, e->val.call.arg[i]))
				return false;
		}
		return true;
	case HIR_EXPR_THISCALL:
		if (!abce_rewrite_expr(ctx, e->val.thiscall.obj))
			return false;
		for (i = 0; i < e->val.thiscall.arg_count; i++) {
			if (!abce_rewrite_expr(ctx, e->val.thiscall.arg[i]))
				return false;
		}
		return true;
	default:
		if (!abce_rewrite_expr(ctx, e->val.binary.expr[0]))
			return false;
		return abce_rewrite_expr(ctx, e->val.binary.expr[1]);
	}
}

static bool
abce_rewrite_block(struct abce_ctx *ctx, struct hir_block *b)
{
	struct hir_stmt *stmt;

	switch (b->type) {
	case HIR_BLOCK_BASIC:
		stmt = b->val.basic.stmt_list;
		while (stmt != NULL) {
			int k;
			if (stmt->lhs != NULL &&
			    (k = abce_packed_subscr_index(ctx, stmt->lhs)) >= 0) {
				/* p[f] = x  ->  PSTORE*($baseK, f) = x */
				struct hir_expr *base_term;
				base_term = abce_mk_expr_symbol(ctx->base_name[k]);
				if (base_term == NULL)
					return false;
				stmt->lhs->type = abce_store_kind(ctx->packed_bet[k]);
				stmt->lhs->val.binary.expr[0] = base_term;
				if (!abce_canonicalize_index(ctx, k,
							      stmt->lhs->val.binary.expr[1]))
					return false;
				if (!abce_rewrite_expr(ctx, stmt->lhs->val.binary.expr[1]))
					return false;
			} else if (stmt->lhs != NULL) {
				if (!abce_rewrite_expr(ctx, stmt->lhs))
					return false;
			}
			if (!abce_rewrite_expr(ctx, stmt->rhs))
				return false;
			stmt = stmt->next;
		}
		break;
	case HIR_BLOCK_IF:
		if (b->val.if_.cond != NULL) {
			if (!abce_rewrite_expr(ctx, b->val.if_.cond))
				return false;
		}
		break;
	default:
		break;
	}
	return true;
}

/*
 * Guard construction.
 */

static struct hir_expr *
abce_mk_endpoint(struct abce_ctx *ctx, struct abce_site *site, bool at_hi)
{
	struct hir_expr *iexpr;
	struct hir_expr *uexpr;

	/* i at the endpoint: $lo, or $hi - 1. */
	if (at_hi) {
		iexpr = abce_mk_binary(HIR_EXPR_MINUS,
				       abce_mk_expr_symbol(ctx->hi_name),
				       abce_mk_expr_int(1));
	} else {
		iexpr = abce_mk_expr_symbol(ctx->lo_name);
	}
	if (iexpr == NULL)
		return NULL;

	if (site->shape == ABCE_SHAPE_I)
		return iexpr;

	if (site->u_is_const)
		uexpr = abce_mk_expr_int(site->u_const);
	else if (site->u_expr != NULL)
		uexpr = abce_clone_expr(site->u_expr);
	else
		uexpr = abce_mk_expr_symbol(site->u_name);
	if (uexpr == NULL)
		return NULL;

	switch (site->shape) {
	case ABCE_SHAPE_I_PLUS_U:
		return abce_mk_binary(HIR_EXPR_PLUS, iexpr, uexpr);
	case ABCE_SHAPE_U_PLUS_I:
		return abce_mk_binary(HIR_EXPR_PLUS, uexpr, iexpr);
	case ABCE_SHAPE_I_MINUS_U:
		return abce_mk_binary(HIR_EXPR_MINUS, iexpr, uexpr);
	default:
		return NULL;
	}
}

static struct hir_expr *
abce_mk_guard(struct abce_ctx *ctx)
{
	struct hir_expr *g;
	struct hir_expr *e;
	int i;

	/* TYPEIS($lo, int) && TYPEIS($hi, int) -- FIRST: never error. */
	g = abce_mk_binary(HIR_EXPR_TYPEIS,
			   abce_mk_expr_symbol(ctx->lo_name),
			   abce_mk_expr_int(NOCT_VALUE_INT));
	e = abce_mk_binary(HIR_EXPR_TYPEIS,
			   abce_mk_expr_symbol(ctx->hi_name),
			   abce_mk_expr_int(NOCT_VALUE_INT));
	g = abce_mk_binary(HIR_EXPR_LAND, g, e);

	/* TYPEIS(v, int/float) for every guarded local. */
	for (i = 0; i < ctx->guard_count; i++) {
		int type = ctx->guard_type[i];
		e = abce_mk_binary(HIR_EXPR_TYPEIS,
				   abce_mk_expr_symbol(ctx->guards[i]),
				   abce_mk_expr_int(type));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
	}

	/* $lo < $hi */
	e = abce_mk_binary(HIR_EXPR_LT,
			   abce_mk_expr_symbol(ctx->lo_name),
			   abce_mk_expr_symbol(ctx->hi_name));
	g = abce_mk_binary(HIR_EXPR_LAND, g, e);

	/* PCHECK(p_k, <bet_k>) for every packed. */
	for (i = 0; i < ctx->packed_count; i++) {
		e = abce_mk_binary(HIR_EXPR_PCHECK,
				   abce_mk_expr_symbol(ctx->packed[i]),
				   abce_mk_expr_int(ctx->packed_bet[i]));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
	}

	/*
	 * Per-site endpoint bounds.  BOTH endpoints are checked against
	 * BOTH bounds: with 32-bit wrapping arithmetic, checking only
	 * f(lo) >= 0 and f(hi-1) < len is unsound (a large invariant
	 * addend can wrap f(hi-1) negative, passing the upper check
	 * while the loop reads out of bounds).  With all four checks, a
	 * passing guard implies every iteration's wrapped index lies in
	 * [0, len): the endpoint values wrap by the same +-2^32 as the
	 * loop body's arithmetic, and a mixed-wrap range always leaves
	 * one endpoint outside [0, len).
	 */
	for (i = 0; i < ctx->site_count; i++) {
		/* f(lo) >= 0 && f(lo) < PLEN(p) */
		e = abce_mk_binary(HIR_EXPR_GTE,
				   abce_mk_endpoint(ctx, &ctx->sites[i], false),
				   abce_mk_expr_int(0));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
		e = abce_mk_binary(HIR_EXPR_LT,
				   abce_mk_endpoint(ctx, &ctx->sites[i], false),
				   abce_mk_unary(HIR_EXPR_PLEN,
						 abce_mk_expr_symbol(ctx->packed[ctx->sites[i].packed_index])));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
		/* f(hi-1) >= 0 && f(hi-1) < PLEN(p) */
		e = abce_mk_binary(HIR_EXPR_GTE,
				   abce_mk_endpoint(ctx, &ctx->sites[i], true),
				   abce_mk_expr_int(0));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
		e = abce_mk_binary(HIR_EXPR_LT,
				   abce_mk_endpoint(ctx, &ctx->sites[i], true),
				   abce_mk_unary(HIR_EXPR_PLEN,
						 abce_mk_expr_symbol(ctx->packed[ctx->sites[i].packed_index])));
		g = abce_mk_binary(HIR_EXPR_LAND, g, e);
	}

	return g;
}

/*
 * The transform.
 */

static bool
abce_version_loop(struct abce_ctx *ctx)
{
	struct hir_block *F;
	struct hir_block *P;
	struct hir_block *orig_succ;
	struct hir_block *G1;
	struct hir_block *G2;
	struct hir_block *X1;
	struct hir_block *X2;
	struct hir_block *B1;
	struct hir_block *FAST;
	struct hir_block *SLOW;
	struct hir_block *FEXIT;
	struct hir_block *SEXIT;
	struct hir_block *b;
	struct hir_stmt *s_lo;
	struct hir_stmt *s_hi;
	struct hir_stmt *s_base;
	struct hir_expr *guard;
	struct hir_expr *pbase;
	int line;
	int i;

	F = ctx->loop;
	P = F->parent;
	orig_succ = F->succ;
	line = F->line;

	/* Unique hoist-local names. */
	snprintf(ctx->lo_name, sizeof(ctx->lo_name), "$abce%d_lo", abce_loop_seq);
	snprintf(ctx->hi_name, sizeof(ctx->hi_name), "$abce%d_hi", abce_loop_seq);
	for (i = 0; i < ctx->packed_count; i++) {
		snprintf(ctx->base_name[i], sizeof(ctx->base_name[i]),
			 "$abce%d_base%d", abce_loop_seq, i);
	}
	for (i = 0; i < ctx->site_count; i++) {
		int j;
		if (ctx->sites[i].u_expr == NULL)
			continue;
		for (j = 0; j < i; j++) {
			if (ctx->sites[j].u_expr != NULL &&
			    abce_expr_equal(ctx->sites[j].u_expr,
					    ctx->sites[i].u_expr)) {
				strcpy(ctx->sites[i].hoist_name,
				       ctx->sites[j].hoist_name);
				break;
			}
		}
		if (j == i)
			snprintf(ctx->sites[i].hoist_name,
				 sizeof(ctx->sites[i].hoist_name),
				 "$abce%d_off%d", abce_loop_seq, i);
	}
	snprintf(ctx->g_name, sizeof(ctx->g_name), "$abce%d_g", abce_loop_seq);
	abce_loop_seq++;
	if (!hir_add_local(F, ctx->lo_name))
		return false;
	if (!hir_add_local(F, ctx->hi_name))
		return false;
	for (i = 0; i < ctx->packed_count; i++) {
		if (!hir_add_local(F, ctx->base_name[i]))
			return false;
	}
	for (i = 0; i < ctx->site_count; i++) {
		int j;
		if (ctx->sites[i].u_expr == NULL)
			continue;
		for (j = 0; j < i; j++) {
			if (strcmp(ctx->sites[j].hoist_name,
				   ctx->sites[i].hoist_name) == 0)
				break;
		}
		if (j == i && !hir_add_local(F, ctx->sites[i].hoist_name))
			return false;
	}
	if (!hir_add_local(F, ctx->g_name))
		return false;

	/* Clone the body for the fast version. */
	ctx->map_count = 0;
	if (!abce_collect_blocks(ctx, F->val.for_.inner))
		return false;
	if (!abce_clone_blocks(ctx))
		return false;

	/* Build the sibling chain blocks. */
	G1 = abce_mk_block(HIR_BLOCK_IF, line, P);
	X1 = abce_mk_block(HIR_BLOCK_BASIC, line, P);
	G2 = abce_mk_block(HIR_BLOCK_IF, line, P);
	X2 = abce_mk_block(HIR_BLOCK_BASIC, line, P);
	B1 = abce_mk_block(HIR_BLOCK_BASIC, line, G1);
	FAST = abce_mk_block(HIR_BLOCK_FOR, line, G1);
	FEXIT = abce_mk_block(HIR_BLOCK_BASIC, line, G1);
	SLOW = abce_mk_block(HIR_BLOCK_FOR, line, G2);
	SEXIT = abce_mk_block(HIR_BLOCK_BASIC, line, G2);
	if (G1 == NULL || X1 == NULL || G2 == NULL || X2 == NULL ||
	    B1 == NULL || FAST == NULL || FEXIT == NULL ||
	    SLOW == NULL || SEXIT == NULL)
		return false;

	/* SLOW takes over the original loop payload and body. */
	SLOW->val.for_ = F->val.for_;
	SLOW->val.for_.start = abce_mk_expr_symbol(ctx->lo_name);
	SLOW->val.for_.stop = abce_mk_expr_symbol(ctx->hi_name);
	if (SLOW->val.for_.start == NULL || SLOW->val.for_.stop == NULL)
		return false;
	/* Reparent the original body's direct children. */
	b = SLOW->val.for_.inner;
	while (b != NULL) {
		if (b->parent == F)
			b->parent = SLOW;
		if (b->stop)
			break;
		b = b->succ;
	}
	/* Also nested blocks whose parent was F (if-exit blocks). */
	for (i = 0; i < ctx->map_count; i++) {
		if (ctx->map_old[i]->parent == F)
			ctx->map_old[i]->parent = SLOW;
	}
	SLOW->succ = SEXIT;
	SEXIT->stop = true;
	SEXIT->succ = X2;

	/* FAST uses the cloned body. */
	FAST->val.for_.is_ranged = true;
	FAST->val.for_.counter_symbol = F->val.for_.counter_symbol;
	/*
	 * Typed-op region (docs/design/07-typed-ops.md 3.2): inside the
	 * guarded fast loop every local read is TYPEIS-int proven, and
	 * with loads of width <= 32 (int-yielding) plus the int-only
	 * safe-expression rules, every body assignment re-establishes
	 * int, so the proof holds inductively.  A 64-bit load would let
	 * an accumulator turn long at runtime (width64.noct), so it
	 * suppresses the flag.  The SLOW loop (guard false) must never
	 * be flagged.
	 */
	FAST->val.for_.typed_int_region =
		ctx->body_kind == ABCE_BODY_INT && !ctx->mixed_numeric;
	for (i = 0; i < ctx->packed_count; i++) {
		if (ctx->packed_bet[i] == NOCT_PACKED_INT64 ||
		    ctx->packed_bet[i] == NOCT_PACKED_UINT64)
			FAST->val.for_.typed_int_region = false;
	}
	/* Mark for the SIMD pass (design 06; it runs right after us). */
	FAST->val.for_.abce_fast = true;
	FAST->val.for_.key_symbol = NULL;
	FAST->val.for_.value_symbol = NULL;
	FAST->val.for_.collection = NULL;
	FAST->val.for_.start = abce_mk_expr_symbol(ctx->lo_name);
	FAST->val.for_.stop = abce_mk_expr_symbol(ctx->hi_name);
	if (FAST->val.for_.start == NULL || FAST->val.for_.stop == NULL)
		return false;
	FAST->val.for_.inner = abce_map_block(ctx, F->val.for_.inner);
	if (FAST->val.for_.inner == NULL)
		return false;
	/* Fix clone edges that referenced the original loop. */
	for (i = 0; i < ctx->map_count; i++) {
		struct hir_block *n = ctx->map_new[i];
		if (n->parent == NULL)
			n->parent = FAST;
		/* Back edges cloned as "old inner" were mapped already
		   (the inner head is itself in the map).  Break edges
		   pointed at the original loop's succ: retarget. */
		if (n->succ == orig_succ)
			n->succ = orig_succ;	/* keep: jumps after X2 */
	}
	FAST->succ = FEXIT;
	FEXIT->stop = true;
	FEXIT->succ = X1;

	/* Rewrite packed accesses in the fast body. */
	for (i = 0; i < ctx->map_count; i++) {
		if (!abce_rewrite_block(ctx, ctx->map_new[i]))
			return false;
	}

	/* B1: $baseK = PBASE(pK) for every packed. */
	{
		struct hir_stmt *tail = NULL;
		for (i = 0; i < ctx->packed_count; i++) {
			pbase = abce_mk_unary(HIR_EXPR_PBASE,
					      abce_mk_expr_symbol(ctx->packed[i]));
			s_base = abce_mk_assign_stmt(line,
						     abce_mk_expr_symbol(ctx->base_name[i]),
						     pbase);
			if (s_base == NULL)
				return false;
			if (tail == NULL)
				B1->val.basic.stmt_list = s_base;
			else
				tail->next = s_base;
			tail = s_base;
		}
		/* Canonicalize non-trivial invariant offsets only after the
		   TYPEIS guard has succeeded. */
		for (i = 0; i < ctx->site_count; i++) {
			struct hir_stmt *s_off;
			int j;
			if (ctx->sites[i].u_expr == NULL)
				continue;
			for (j = 0; j < i; j++) {
				if (strcmp(ctx->sites[j].hoist_name,
					   ctx->sites[i].hoist_name) == 0)
					break;
			}
			if (j != i)
				continue;
			s_off = abce_mk_assign_stmt(line,
				abce_mk_expr_symbol(ctx->sites[i].hoist_name),
				abce_clone_expr(ctx->sites[i].u_expr));
			if (s_off == NULL)
				return false;
			tail->next = s_off;
			tail = s_off;
		}
	}
	B1->succ = FAST;

	/*
	 * Guard.  Evaluated exactly ONCE into $g (in the hoist block):
	 * the fast body may legitimately change the runtime type of a
	 * TYPEIS-guarded local (e.g. an int accumulator becoming long
	 * through 64-bit element loads), so re-evaluating the guard for
	 * the else-branch could make BOTH versions run.
	 */
	guard = abce_mk_guard(ctx);
	if (guard == NULL)
		return false;

	/* G1: if ($g) { $base = PBASE(p); fast-for } */
	G1->val.if_.cond = abce_mk_expr_symbol(ctx->g_name);
	if (G1->val.if_.cond == NULL)
		return false;
	G1->val.if_.inner = B1;
	G1->succ = X1;

	/* G2: if (!$g) { slow-for } */
	G2->val.if_.cond = abce_mk_unary(HIR_EXPR_NOT,
					 abce_mk_expr_symbol(ctx->g_name));
	if (G2->val.if_.cond == NULL)
		return false;
	G2->val.if_.inner = SLOW;
	G2->succ = X2;

	X1->succ = G2;
	X2->succ = orig_succ;

	/*
	 * Convert the original FOR block into the hoist BASIC block in
	 * place, so every predecessor pointing at it stays valid:
	 *   $lo = <start>; $hi = <stop>;
	 */
	s_lo = abce_mk_assign_stmt(line, abce_mk_expr_symbol(ctx->lo_name),
				   F->val.for_.start);
	s_hi = abce_mk_assign_stmt(line, abce_mk_expr_symbol(ctx->hi_name),
				   F->val.for_.stop);
	if (s_lo == NULL || s_hi == NULL)
		return false;
	{
		struct hir_stmt *s_g;
		s_g = abce_mk_assign_stmt(line,
					  abce_mk_expr_symbol(ctx->g_name),
					  guard);
		if (s_g == NULL)
			return false;
		s_lo->next = s_hi;
		s_hi->next = s_g;
	}
	memset(&F->val, 0, sizeof(F->val));
	F->type = HIR_BLOCK_BASIC;
	F->val.basic.stmt_list = s_lo;
	F->succ = G1;
	F->stop = false;

	return true;
}

/* Debug verification: the fast subtree must be allocation-free. */
#ifndef NDEBUG
static void
abce_verify_fast_expr(struct hir_expr *e)
{
	if (e == NULL)
		return;
	switch (e->type) {
	case HIR_EXPR_TERM:
		assert(e->val.term.term->type != HIR_TERM_STRING);
		assert(e->val.term.term->type != HIR_TERM_EMPTY_ARRAY);
		assert(e->val.term.term->type != HIR_TERM_EMPTY_DICT);
		break;
	case HIR_EXPR_CALL:
		assert(hir_get_intrinsic_call(e) != HIR_INTRINSIC_NONE);
		assert(e->val.call.arg_count == 1);
		abce_verify_fast_expr(e->val.call.arg[0]);
		break;
	case HIR_EXPR_THISCALL:
	case HIR_EXPR_NEW:
	case HIR_EXPR_ARRAY:
	case HIR_EXPR_DICT:
		assert(0);	/* allocation source in fast body */
		break;
	case HIR_EXPR_PAR:
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
	case HIR_EXPR_PBASE:
	case HIR_EXPR_PLEN:
		abce_verify_fast_expr(e->val.unary.expr);
		break;
	case HIR_EXPR_CAPTURE:
		/* CSE runs after ABCE, so this cannot appear today; keep
		   the walk correct in case the pass order ever changes. */
		abce_verify_fast_expr(e->val.capture.expr);
		break;
	default:
		abce_verify_fast_expr(e->val.binary.expr[0]);
		abce_verify_fast_expr(e->val.binary.expr[1]);
		break;
	}
}

static void
abce_verify_fast(struct abce_ctx *ctx)
{
	struct hir_stmt *stmt;
	int i;

	for (i = 0; i < ctx->map_count; i++) {
		struct hir_block *b = ctx->map_new[i];
		assert(b->type == HIR_BLOCK_BASIC || b->type == HIR_BLOCK_IF);
		if (b->type == HIR_BLOCK_BASIC) {
			stmt = b->val.basic.stmt_list;
			while (stmt != NULL) {
				abce_verify_fast_expr(stmt->lhs);
				abce_verify_fast_expr(stmt->rhs);
				stmt = stmt->next;
			}
		} else if (b->val.if_.cond != NULL) {
			abce_verify_fast_expr(b->val.if_.cond);
		}
	}
}
#endif

/* Collect candidate ranged-for blocks (snapshot before transforming). */
static void
abce_collect_loops(struct hir_block *head,
		   struct hir_block **loops,
		   int *count)
{
	struct hir_block *b;
	struct hir_block *c;

	b = head;
	while (b != NULL) {
		switch (b->type) {
		case HIR_BLOCK_FOR:
			if (b->val.for_.is_ranged && *count < ABCE_MAX_LOOPS)
				loops[(*count)++] = b;
			if (b->val.for_.inner != NULL)
				abce_collect_loops(b->val.for_.inner, loops, count);
			break;
		case HIR_BLOCK_WHILE:
			if (b->val.while_.inner != NULL)
				abce_collect_loops(b->val.while_.inner, loops, count);
			break;
		case HIR_BLOCK_IF:
			c = b;
			while (c != NULL) {
				if (c->val.if_.inner != NULL)
					abce_collect_loops(c->val.if_.inner, loops, count);
				c = c->val.if_.chain_next;
			}
			break;
		default:
			break;
		}
		if (b->stop)
			break;
		b = b->succ;
	}
}

/*
 * Run the ABCE pass on one function.  (Level gating is done by the
 * driver hir_optimize_func() in hir.c.)
 */
bool
hir_opt_abce_func(
	struct hir_block *func_block)
{
	struct hir_block *loops[ABCE_MAX_LOOPS];
	struct abce_ctx *ctx;
	int loop_count;
	int i;

	assert(func_block != NULL);
	assert(func_block->type == HIR_BLOCK_FUNC);

	abce_loop_seq = 0;

	loop_count = 0;
	if (func_block->val.func.inner != NULL)
		abce_collect_loops(func_block->val.func.inner, loops, &loop_count);

	{
	/* Packed element-type facts (function-wide, two passes). */
	static struct abce_facts facts;
	memset(&facts, 0, sizeof(facts));
	/* Element-specific packed annotations are entry-checked before the
	   body at this optimization level, so they are sound seed facts. */
	for (i = 0; i < (int)func_block->val.func.param_count; i++) {
		int packed_type = func_block->val.func.param_packed_type[i];
		if (packed_type >= 0 && packed_type != NOCT_PACKED_ANY)
			abce_fact_meet(&facts,
				func_block->val.func.param_name[i], packed_type);
	}
	if (func_block->val.func.inner != NULL) {
		abce_facts_scan_chain(&facts, func_block->val.func.inner, 0);
		abce_facts_scan_chain(&facts, func_block->val.func.inner, 1);
	}

	for (i = 0; i < loop_count; i++) {
		ctx = hir_malloc(sizeof(struct abce_ctx));
		if (ctx == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(ctx, 0, sizeof(struct abce_ctx));
		ctx->func_block = func_block;
		ctx->loop = loops[i];
		ctx->facts = &facts;
		if (!abce_check_eligibility(ctx)) {
			if (getenv("NOCT_ABCE_DEBUG") != NULL)
				fprintf(stderr, "[abce] %s:%d: ineligible\n", hir_file_name, loops[i]->line);
			continue;
		}
		if (!abce_version_loop(ctx))
			return false;
		if (getenv("NOCT_ABCE_DEBUG") != NULL)
			fprintf(stderr, "[abce] %s:%d: versioned (sites=%d guards=%d packeds=%d bet0=%d)\n",
				hir_file_name, loops[i]->line, ctx->site_count, ctx->guard_count,
				ctx->packed_count, ctx->packed_bet[0]);
#ifndef NDEBUG
		abce_verify_fast(ctx);
#endif
	}
	}

	return true;
}
