/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR: High-level Intermediate Representation
 */

#include <noct/noct.h>
#include "hir.h"
#include "hir_parallel.h"
#include "hir_opt.h"
#include "ast.h"
#include "arena.h"
#include "accel_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

/* False assertions. */
#define NEVER_COME_HERE		(0)
#define UNIMPLEMENTED		(0)

/* Debug dump */
#undef DEBUG_DUMP

/* Arena allocator size. */
#if !defined(NOCT_MEMORY_SMALL)
#define ARENA_SIZE		(64 * 1024 * 1024)
#else
#define ARENA_SIZE		(1024 * 1024)
#endif

/* List-add function. */
#define HIR_ADD_TO_LAST(type, list, p)			\
	do {						\
		if (list == NULL) {			\
			list = p;			\
		} else {				\
			type *elem = list;		\
			while (elem->next)		\
				elem = elem->next;	\
			elem->next = p;			\
		}					\
	} while (0);

/*
 * Constructed HIR.
 */

#define HIR_FUNC_MAX	1024

char *hir_file_name;
static bool hir_current_is_accel;
static int hir_current_func_kind;
static struct hir_block *hir_current_func_block;
static bool hir_fast_direct_call_target;

#define HIR_FAST_LOOP_MAX 16
struct hir_fast_loop_domain {
	const char *counter;
	bool known;
	int64_t lower;
	int64_t upper;
};
static struct hir_fast_loop_domain hir_fast_loop[HIR_FAST_LOOP_MAX];
static int hir_fast_loop_depth;
static int hir_fast_cond_depth;

#define HIR_FAST_EDGE_MAX 4096
struct hir_fast_call_edge {
	const char *caller;
	const char *callee;
	int line;
};
static struct hir_fast_call_edge hir_fast_edge[HIR_FAST_EDGE_MAX];
static uint32_t hir_fast_edge_count;

#define HIR_FAST_PROTOTYPE_MAX 1024
struct hir_fast_prototype {
	char *name;
	int func_kind;
	struct fast_signature signature;
};
static struct hir_fast_prototype hir_fast_prototype[HIR_FAST_PROTOTYPE_MAX];
static uint32_t hir_fast_prototype_count;
uint32_t hir_func_count;
struct hir_block *hir_func_tbl[HIR_FUNC_MAX];

/*
 * Error position and message.
 */

static int hir_error_line;
static char hir_error_message[1024];

/*
 * Block id top.
 */
static int block_id_top;

/* Allocate a fresh debug block id (shared with the optimizer passes). */
int
hir_next_block_id(void)
{
	return block_id_top++;
}

/*
 * Anonymous functions.
 */

#define ANON_FUNC_SIZE	256

static int hir_anon_func_count;
static char *hir_anon_func_name[ANON_FUNC_SIZE];
static struct ast_param_list *hir_anon_func_param_list[ANON_FUNC_SIZE];
static struct ast_stmt_list *hir_anon_func_stmt_list[ANON_FUNC_SIZE];

/*
 * Arena allocator.
 */
static struct arena_info hir_arena;

/*
 * Block scoping (docs/design/04-scoping.md).
 *
 * Scopes are pushed for the function body and for every if/elif/else/
 * for/while body.  Declarations are pre-scanned at push time (one
 * level, not nested blocks) so use-before-declaration is a static
 * error.  Shadowing renames the inner declaration to "name$N"
 * (alpha-renaming) so the rest of the compiler keeps its flat
 * per-function local list unchanged.
 */

struct hir_scope_entry {
	char *src_name;		/* name as written                 */
	char *int_name;		/* internal name (may be name$N)   */
	bool declared;		/* false until the decl stmt       */
	bool is_let;
	struct hir_scope_entry *next;
};

struct hir_scope {
	struct hir_scope_entry *entries;
	struct hir_scope *up;
};

static struct hir_scope *hir_scope_top;
static int hir_scope_seq;	/* per-function rename counter     */
static int hir_scope_line;	/* current stmt line for messages  */

/* Early declarations for the scope machinery (redeclared below). */
static void hir_fatal(int line, const char *msg);
static bool hir_check_type_annotation(int line, const char *type_name,
				      int *tag, int *packed_type,
				      bool *restricted);
/* hir_out_of_memory/hir_malloc/hir_strdup are declared in hir_opt.h. */

/* Reset the scope machinery at function entry. */
static void
hir_scope_begin_func(void)
{
	hir_scope_top = NULL;
	hir_scope_seq = 0;
}

static struct hir_scope_entry *
hir_scope_find_here(struct hir_scope *scope, const char *name)
{
	struct hir_scope_entry *e;

	if (scope == NULL)
		return NULL;
	e = scope->entries;
	while (e != NULL) {
		if (strcmp(e->src_name, name) == 0)
			return e;
		e = e->next;
	}
	return NULL;
}

static struct hir_scope_entry *
hir_scope_find(const char *name)
{
	struct hir_scope *scope;
	struct hir_scope_entry *e;

	scope = hir_scope_top;
	while (scope != NULL) {
		e = hir_scope_find_here(scope, name);
		if (e != NULL)
			return e;
		scope = scope->up;
	}
	return NULL;
}

/* Add an entry to the innermost scope. */
static bool
hir_scope_add_entry(
	int line,
	const char *src_name,
	bool declared,
	bool is_let,
	struct hir_scope_entry **out)
{
	struct hir_scope_entry *e;
	char msg[256];

	assert(hir_scope_top != NULL);

	if (hir_scope_find_here(hir_scope_top, src_name) != NULL) {
		snprintf(msg, sizeof(msg),
			 N_TR("Variable '%s' is already declared in this scope."),
			 src_name);
		hir_fatal(line, msg);
		return false;
	}

	e = hir_malloc(sizeof(struct hir_scope_entry));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_scope_entry));
	e->src_name = hir_strdup(src_name);
	if (e->src_name == NULL) {
		hir_out_of_memory();
		return false;
	}
	e->declared = declared;
	e->is_let = is_let;
	e->next = hir_scope_top->entries;
	hir_scope_top->entries = e;
	if (out != NULL)
		*out = e;
	return true;
}

/* Compute the internal name of a newly-declared entry. */
static bool
hir_scope_intern(struct hir_scope_entry *e)
{
	char buf[64];

	/*
	 * Function-root declarations (parameters and function-body-level
	 * vars) keep their source name: their scope covers the rest of
	 * the function, so the flat per-function local list can never
	 * leak them past their scope.
	 *
	 * Every INNER-block declaration is renamed unconditionally.
	 * The rename is what makes the binding invisible after its
	 * block: LIR resolves locals by name against the flat list, so
	 * an out-of-scope use of the bare name must not match the dead
	 * binding's slot (it must fall through to the global path).
	 */
	if (hir_scope_top->up == NULL) {
		e->int_name = e->src_name;
		return true;
	}
	hir_scope_seq++;
	snprintf(buf, sizeof(buf), "%s$%d", e->src_name, hir_scope_seq);
	e->int_name = hir_strdup(buf);
	if (e->int_name == NULL) {
		hir_out_of_memory();
		return false;
	}
	return true;
}

/* Push a scope; pre-scan the block's own statements for var/let. */
static bool
hir_scope_push(struct ast_stmt_list *stmt_list)
{
	struct hir_scope *scope;
	struct ast_stmt *stmt;
	struct ast_expr *lhs;

	scope = hir_malloc(sizeof(struct hir_scope));
	if (scope == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(scope, 0, sizeof(struct hir_scope));
	scope->up = hir_scope_top;
	hir_scope_top = scope;

	if (stmt_list == NULL)
		return true;

	stmt = stmt_list->list;
	while (stmt != NULL) {
		if (stmt->type == AST_STMT_ASSIGN &&
		    (stmt->val.assign.is_var || stmt->val.assign.is_let)) {
			lhs = stmt->val.assign.lhs;
			if (lhs != NULL &&
			    lhs->type == AST_EXPR_TERM &&
			    lhs->val.term.term->type == AST_TERM_SYMBOL) {
				if (!hir_scope_add_entry(stmt->line,
							 lhs->val.term.term->val.symbol,
							 false,
							 stmt->val.assign.is_let,
							 NULL))
					return false;
			}
		}
		stmt = stmt->next;
	}
	return true;
}

static void
hir_scope_pop(void)
{
	assert(hir_scope_top != NULL);
	hir_scope_top = hir_scope_top->up;
}

/* Declaration visit: find the pre-scanned entry, intern, mark. */
static bool
hir_scope_declare(
	int line,
	const char *src_name,
	bool is_let,
	const char **int_name)
{
	struct hir_scope_entry *e;
	char msg[256];

	e = hir_scope_find_here(hir_scope_top, src_name);
	if (e == NULL) {
		/* Not pre-scanned (unusual LHS shape); add now. */
		if (!hir_scope_add_entry(line, src_name, false, is_let, &e))
			return false;
	}
	if (e->declared) {
		snprintf(msg, sizeof(msg),
			 N_TR("Variable '%s' is already declared in this scope."),
			 src_name);
		hir_fatal(line, msg);
		return false;
	}
	if (!hir_scope_intern(e))
		return false;
	e->is_let = is_let;
	*int_name = e->int_name;
	return true;
}

/* Mark the entry declared (after its initializer was visited). */
static void
hir_scope_mark_declared(const char *src_name)
{
	struct hir_scope_entry *e;

	e = hir_scope_find_here(hir_scope_top, src_name);
	assert(e != NULL);
	e->declared = true;
}

/*
 * Resolve a symbol use.  Returns false on a TDZ error.  On success,
 * *out_name is the internal name to use, or NULL for the global path.
 */
static bool
hir_scope_resolve(
	const char *name,
	const char **out_name)
{
	struct hir_scope_entry *e;
	char msg[256];

	*out_name = NULL;
	if (name[0] == '$')
		return true;	/* compiler-internal names */
	e = hir_scope_find(name);
	if (e == NULL)
		return true;	/* global */
	if (!e->declared) {
		snprintf(msg, sizeof(msg),
			 N_TR("Variable '%s' is used before its declaration."),
			 name);
		hir_fatal(hir_scope_line, msg);
		return false;
	}
	*out_name = e->int_name;
	return true;
}

/* Reject assignment to a let binding (LHS already resolved). */
static bool
hir_scope_check_let_assign(
	int line,
	const char *int_name)
{
	struct hir_scope *scope;
	struct hir_scope_entry *e;
	char msg[256];

	scope = hir_scope_top;
	while (scope != NULL) {
		e = scope->entries;
		while (e != NULL) {
			if (e->int_name != NULL &&
			    strcmp(e->int_name, int_name) == 0) {
				if (e->is_let) {
					snprintf(msg, sizeof(msg),
						 N_TR("Cannot assign to 'let' variable '%s'."),
						 e->src_name);
					hir_fatal(line, msg);
					return false;
				}
				return true;
			}
			e = e->next;
		}
		scope = scope->up;
	}
	return true;
}

/* Wrap an expression in a Dict.freeze(...) call (class/extend). */
static bool
hir_wrap_freeze(
	struct hir_expr **hexpr,
	struct hir_expr *inner)
{
	struct hir_expr *call;
	struct hir_expr *fn;
	struct hir_term *fn_term;

	call = hir_malloc(sizeof(struct hir_expr));
	fn = hir_malloc(sizeof(struct hir_expr));
	fn_term = hir_malloc(sizeof(struct hir_term));
	if (call == NULL || fn == NULL || fn_term == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(call, 0, sizeof(struct hir_expr));
	memset(fn, 0, sizeof(struct hir_expr));
	memset(fn_term, 0, sizeof(struct hir_term));

	fn_term->type = HIR_TERM_SYMBOL;
	fn_term->val.symbol = hir_strdup("Dict.freeze");
	if (fn_term->val.symbol == NULL) {
		hir_out_of_memory();
		return false;
	}
	fn->type = HIR_EXPR_TERM;
	fn->val.term.term = fn_term;

	call->type = HIR_EXPR_CALL;
	call->val.call.func = fn;
	call->val.call.arg_count = 1;
	call->val.call.arg[0] = inner;

	*hexpr = call;
	return true;
}

/*
 * Forward Declaration
 */
static bool hir_visit_func(struct ast_func *afunc);
static bool hir_visit_stmt_list(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt_list *stmt_list);
static bool hir_visit_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_expr_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_assign_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_if_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_scope_push(struct ast_stmt_list *stmt_list);
static void hir_scope_pop(void);
static bool hir_scope_declare(int line, const char *src_name, bool is_let, const char **int_name);
static bool hir_scope_resolve(const char *name, const char **out_name);
static bool hir_scope_check_let_assign(int line, const char *int_name);
static bool hir_visit_elif_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_else_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_while_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_for_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_return_stmt(struct hir_block **cur_block, struct hir_block **prev_block, struct hir_block *parent_block, struct ast_stmt *cur_astmt);
static bool hir_visit_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_term_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_binary_expr(struct hir_expr **hexpr, struct ast_expr *aexpr, int type);
static bool hir_visit_fast_multi_subscr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static int hir_fast_infer_expr_type(const struct hir_expr *expr);
static bool hir_resolve_type_name(const char *name, int *tag,
				  int *packed_type, bool *restricted);
static bool hir_fast_check_subscript(const struct ast_expr *expr);
static bool hir_fast_ast_constant(const struct ast_expr *expr,
				  int64_t *value);
static bool hir_fast_validate_call_graph(void);
static bool hir_fast_stmt_list_returns(const struct ast_stmt_list *list);
static bool hir_is_fast_intrinsic_name(const char *name);
static struct ast_func *hir_find_fast_ast_func(const char *source_name);
static bool hir_build_ast_fast_signature(struct ast_func *func,
					 struct fast_signature *signature);
static bool hir_visit_unary_expr(struct hir_expr **hexpr, struct ast_expr *aexpr, int type);
static bool hir_visit_dot_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_call_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_thiscall_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_array_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_dict_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_func_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_new_expr(struct hir_expr **hexpr, struct ast_expr *aexpr);
static bool hir_visit_term(struct hir_term **hterm, struct ast_term *aterm);
static bool hir_visit_param_list(struct hir_block *hfunc,struct ast_func *afunc);
static bool hir_defer_anon_func(struct ast_expr *aexpr, char **symbol);
static struct hir_local *hir_find_local(struct hir_block *block,
					const char *symbol);
static void hir_set_local_declaration(struct hir_block *block,
				      const char *symbol,
				      int declaration_kind,
				      int declared_type,
				      int declared_scalar_kind,
				      int declared_packed_type,
				      int storage_class,
				      int line,
				      const struct hir_stmt *declaration_stmt,
				      const struct hir_expr *initializer);
static int hir_packed_constructor_type(const struct hir_expr *expr);
static int hir_declared_scalar_kind(const char *type_name);
static void hir_free_block(struct hir_block *b);
static void hir_free_stmt(struct hir_stmt *s);
static void hir_free_expr(struct hir_expr *e);
static void hir_free_term(struct hir_term *t);

static const struct accel_op_desc *
hir_ast_accel_math(const struct ast_expr *expr)
{
	const struct ast_expr *dot;
	const struct ast_expr *obj;
	const struct ast_term *term;
	if (expr == NULL || expr->type != AST_EXPR_CALL) return NULL;
	dot = expr->val.call.func;
	if (dot == NULL || dot->type != AST_EXPR_DOT) return NULL;
	obj = dot->val.dot.obj;
	if (obj == NULL || obj->type != AST_EXPR_TERM) return NULL;
	term = obj->val.term.term;
	if (term == NULL || term->type != AST_TERM_SYMBOL ||
	    strcmp(term->val.symbol, "Accel") != 0) return NULL;
	return accel_math_lookup_member(dot->val.dot.symbol);
}

static const struct accel_op_desc *
hir_ast_accel_math_property(const struct ast_expr *expr)
{
	const struct ast_expr *obj;
	const struct ast_term *term;
	if (expr == NULL || expr->type != AST_EXPR_DOT) return NULL;
	obj = expr->val.dot.obj;
	if (obj == NULL || obj->type != AST_EXPR_TERM) return NULL;
	term = obj->val.term.term;
	if (term == NULL || term->type != AST_TERM_SYMBOL ||
	    strcmp(term->val.symbol, "Accel") != 0) return NULL;
	return accel_math_lookup_member(expr->val.dot.symbol);
}

static bool
hir_ast_accel_float32_bits_property(const struct ast_expr *expr)
{
	const struct ast_expr *obj;
	const struct ast_term *term;
	if (expr == NULL || expr->type != AST_EXPR_DOT ||
	    strcmp(expr->val.dot.symbol, "float32FromBits") != 0) return false;
	obj = expr->val.dot.obj;
	if (obj == NULL || obj->type != AST_EXPR_TERM) return false;
	term = obj->val.term.term;
	return term != NULL && term->type == AST_TERM_SYMBOL &&
	       strcmp(term->val.symbol, "Accel") == 0;
}

static bool
hir_ast_accel_float32_bits_call(const struct ast_expr *expr)
{
	return expr != NULL && expr->type == AST_EXPR_CALL &&
	       hir_ast_accel_float32_bits_property(expr->val.call.func);
}

int
hir_get_intrinsic_call(const struct hir_expr *expr)
{
	const struct hir_expr *fn;
	const struct hir_expr *obj;
	const char *pkg;

	if (expr == NULL || expr->type != HIR_EXPR_CALL ||
	    expr->val.call.arg_count != 1)
		return HIR_INTRINSIC_NONE;
	fn = expr->val.call.func;
	if (fn == NULL || fn->type != HIR_EXPR_DOT ||
	    strcmp(fn->val.dot.symbol, "from") != 0)
		return HIR_INTRINSIC_NONE;
	obj = fn->val.dot.obj;
	if (obj == NULL || obj->type != HIR_EXPR_TERM ||
	    obj->val.term.term->type != HIR_TERM_SYMBOL)
		return HIR_INTRINSIC_NONE;
	pkg = obj->val.term.term->val.symbol;
	if (strcmp(pkg, "Int") == 0)
		return HIR_INTRINSIC_INT_FROM;
	if (strcmp(pkg, "Float") == 0)
		return HIR_INTRINSIC_FLOAT_FROM;
	return HIR_INTRINSIC_NONE;
}
static void hir_free_local(struct hir_local *local);
static void hir_fatal(int line, const char *msg);
static void hir_free(void *p);
static void hir_dump_block_at_level(struct hir_block *block, int level);

static bool
hir_fast_graph_reaches(const char *from, const char *target, bool *seen)
{
	uint32_t i;

	if (strcmp(from, target) == 0) return true;
	for (i = 0; i < hir_fast_edge_count; i++) {
		if (!seen[i] && strcmp(hir_fast_edge[i].caller, from) == 0) {
			seen[i] = true;
			if (hir_fast_graph_reaches(hir_fast_edge[i].callee,
						   target, seen))
				return true;
		}
	}
	return false;
}

static bool
hir_fast_validate_call_graph(void)
{
	uint32_t i;

	for (i = 0; i < hir_fast_edge_count; i++) {
		bool seen[HIR_FAST_EDGE_MAX];
		memset(seen, 0, sizeof(seen));
		if (hir_fast_graph_reaches(hir_fast_edge[i].callee,
					   hir_fast_edge[i].caller, seen)) {
			hir_fatal(hir_fast_edge[i].line,
				  N_TR("Recursive and mutually recursive __fast calls are not supported."));
			return false;
		}
	}
	return true;
}

/*
 * Construct an HIR from an AST.
 */
bool
hir_build(void)
{
	struct ast_func_list *func_list;
	struct ast_func *func;
	int i;

	assert(hir_file_name == NULL);
	assert(hir_func_count == 0);

	/* Initialize the arena allocator. */
	if (!arena_init(&hir_arena, ARENA_SIZE)) {
		hir_out_of_memory();
		return false;
	}

	/* Copy a file name. */
	hir_file_name = hir_strdup(ast_get_file_name());
	if (hir_file_name == NULL) {
		hir_out_of_memory();
		return false;
	}

	hir_anon_func_count = 0;
	hir_fast_edge_count = 0;

	/* Copy a file name. */
	hir_file_name = hir_strdup(ast_get_file_name());
	if (hir_file_name == NULL)
		return false;

	/* Construct a HIR func for each AST func: */
	func_list = ast_get_func_list();
	assert(func_list != NULL);
	func = func_list->list;
	while (func != NULL) {
		/* Visit an AST func. */
		if (!hir_visit_func(func))
			return false;

		func = func->next;

		/*
		 * If an anonymous func appears while a visit,
		 * it is queued to the deffered table.
		 */
	}

	/* Construct a HIR func for each deffered anonymous func: */
	for (i = 0; i < hir_anon_func_count; i++) {
		/* Visit an AST func. */
		struct ast_func afunc;
		afunc.name = hir_anon_func_name[i];
		afunc.param_list = hir_anon_func_param_list[i];
		afunc.return_type_name = NULL;
		afunc.is_static = false;
		afunc.is_inline = false;
		afunc.is_accel = false;
		afunc.func_kind = NOCT_FUNC_NORMAL;
		afunc.stmt_list = hir_anon_func_stmt_list[i];
		afunc.next = NULL;
		if (!hir_visit_func(&afunc))
			return false;

		hir_anon_func_name[i] = NULL;
		hir_anon_func_param_list[i] = NULL;
		hir_anon_func_stmt_list[i] = NULL;
	}
	if (!hir_fast_validate_call_graph())
		return false;

	return true;
}

/*
 * Free constructed HIR functions.
 */
void
hir_cleanup(void)
{
	uint32_t i;

	if (hir_file_name != NULL) {
		hir_free(hir_file_name);
		hir_file_name = NULL;
	}

	for (i = 0; i < hir_func_count; i++) {
		hir_free_block(hir_func_tbl[i]);
		hir_func_tbl[i] = NULL;
	}

	hir_func_count = 0;
	arena_cleanup(&hir_arena);
}

/*
 * Get a number of constructed functions.
 */
uint32_t
hir_get_function_count(void)
{
	return hir_func_count;
}

/*
 * Get a constructed HIR function.
 */
struct hir_block *
hir_get_function(uint32_t index)
{
	struct hir_block *func;

	assert(index < hir_func_count);

	func = hir_func_tbl[index];

	return func;
}

bool
hir_set_function_name(struct hir_block *func, const char *name)
{
	char *copy;

	assert(func != NULL);
	assert(func->type == HIR_BLOCK_FUNC);
	copy = hir_strdup(name);
	if (copy == NULL) {
		hir_out_of_memory();
		return false;
	}
	func->val.func.name = copy;
	return true;
}

/*
 * Get a file name.
 */
const char *
hir_get_file_name(void)
{
	assert(hir_file_name);

	return hir_file_name;
}

/*
 * Get an error line number.
 */
int
hir_get_error_line(void)
{
	return hir_error_line;
}

/*
 * Get an error message.
 */
const char *
hir_get_error_message(void)
{
	return hir_error_message;
}

void
hir_set_error(int line, const char *message)
{
	hir_error_line = line;
	snprintf(hir_error_message, sizeof(hir_error_message), "%s",
		 message != NULL ? message : "Accelerator compilation failed.");
}

/* Visit an AST func. */
static bool
hir_fast_stmt_list_returns(const struct ast_stmt_list *list)
{
	const struct ast_stmt *stmt;

	stmt = list != NULL ? list->list : NULL;
	while (stmt != NULL) {
		if (stmt->type == AST_STMT_RETURN)
			return true;
		if (stmt->type == AST_STMT_IF) {
			const struct ast_stmt *branch;
			bool all_return;
			bool has_else;
			all_return = hir_fast_stmt_list_returns(stmt->val.if_.stmt_list);
			has_else = false;
			branch = stmt->next;
			while (branch != NULL &&
			       (branch->type == AST_STMT_ELIF ||
				branch->type == AST_STMT_ELSE)) {
				if (branch->type == AST_STMT_ELIF)
					all_return = all_return &&
						hir_fast_stmt_list_returns(
							branch->val.elif.stmt_list);
				else {
					has_else = true;
					all_return = all_return &&
						hir_fast_stmt_list_returns(
							branch->val.else_.stmt_list);
				}
				branch = branch->next;
			}
			if (all_return && has_else) return true;
		}
		stmt = stmt->next;
	}
	return false;
}

static bool
hir_visit_func(
	struct ast_func *afunc)
{
	struct hir_block *func_block;
	struct hir_block *end_block;
	struct hir_block *cur_block;
	struct hir_block *prev_block;

	hir_current_is_accel = afunc->is_accel;
	hir_current_func_kind = afunc->func_kind;
	hir_fast_loop_depth = 0;
	hir_fast_cond_depth = 0;
	if (afunc->func_kind == NOCT_FUNC_FAST &&
	    hir_is_fast_intrinsic_name(afunc->name)) {
		hir_fatal(0,
			  N_TR("A compiler-owned __fast intrinsic name cannot be redeclared."));
		return false;
	}

	/* Check maximum functions. */
	if (hir_func_count >= HIR_FUNC_MAX) {
		hir_fatal(0, N_TR("Too many functions."));
		return false;
	}

	/* Alloc a func block. */
	func_block = hir_malloc(sizeof(struct hir_block));
	if (func_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(func_block, 0, sizeof(struct hir_block));
	func_block->val.func.fast_signature =
		hir_malloc(sizeof(*func_block->val.func.fast_signature));
	if (func_block->val.func.fast_signature == NULL) {
		hir_out_of_memory();
		return false;
	}
	func_block->id = block_id_top++;
	func_block->type = HIR_BLOCK_FUNC;
	func_block->val.func.file_name = hir_strdup(hir_file_name);
	if (func_block->val.func.file_name == NULL) {
		hir_out_of_memory();
		return false;
	}
	hir_current_func_block = func_block;

	do {
		/* Set a func name. */
		func_block->val.func.name = hir_strdup(afunc->name);
		if (func_block->val.func.name == NULL) {
			hir_out_of_memory();
			break;
		}
		func_block->val.func.is_static = afunc->is_static;
		func_block->val.func.is_inline = afunc->is_inline;
		func_block->val.func.is_accel = afunc->is_accel;
		func_block->val.func.func_kind = afunc->func_kind;

		/* Parse the parameters. */
		if (!hir_visit_param_list(func_block, afunc))
			break;

		/* Resolve the optional return type.  Restrict is an input
		   alias contract and is meaningless on a returned value. */
		{
			bool return_restricted;
			char return_base[64];
			const char *return_annotation;
			bool return_has_shape;

			return_annotation = afunc->return_type_name;
			if (return_annotation != NULL &&
			    strchr(return_annotation, '(') != NULL) {
				if (!fast_annotation_base(return_annotation, return_base,
							 sizeof(return_base),
							 &return_has_shape)) {
					hir_fatal(0, N_TR("Invalid return type shape."));
					break;
				}
				return_annotation = return_base;
			}
			if (!hir_check_type_annotation(0,
					       return_annotation,
					       &func_block->val.func.return_type,
					       &func_block->val.func.return_packed_type,
					       &return_restricted))
				break;
			if (return_restricted) {
				hir_fatal(0, N_TR("A restricted packed type is only valid for a parameter."));
				break;
			}
			if ((afunc->func_kind == NOCT_FUNC_ACCEL ||
			     afunc->func_kind == NOCT_FUNC_GPU) &&
			    func_block->val.func.return_type != HIR_TYPE_VOID) {
				hir_fatal(0, afunc->func_kind == NOCT_FUNC_ACCEL ?
					  N_TR("An accelerator function must declare a void return type.") :
					  N_TR("A GPU function must declare a void return type."));
				break;
			}
		}
		if (afunc->func_kind == NOCT_FUNC_FAST &&
		    func_block->val.func.return_type != HIR_TYPE_VOID &&
		    !hir_fast_stmt_list_returns(afunc->stmt_list)) {
			hir_fatal(0,
				  N_TR("Every reachable path of a non-void __fast func must return a value."));
			break;
		}

		/* Build the mandatory __fast signature after every parameter
		   name and scalar type is known. */
		{
			const char *annotation[HIR_PARAM_SIZE];
			const char *name[HIR_PARAM_SIZE];
			struct ast_param *param;
			uint32_t i;
			char message[256];

			for (i = 0; i < HIR_PARAM_SIZE; i++) {
				annotation[i] = NULL;
				name[i] = NULL;
			}
			param = afunc->param_list != NULL ?
				afunc->param_list->list : NULL;
			i = 0;
			while (param != NULL && i < HIR_PARAM_SIZE) {
				annotation[i] = param->type_name;
				name[i] = func_block->val.func.param_name[i];
				param = param->next;
				i++;
			}
			if (!fast_signature_build(
				    func_block->val.func.fast_signature,
				    afunc->func_kind,
				    func_block->val.func.param_count,
				    name, annotation,
				    func_block->val.func.param_type,
				    func_block->val.func.param_packed_type,
				    func_block->val.func.param_restricted,
				    afunc->return_type_name,
				    func_block->val.func.return_type,
				    message, sizeof(message))) {
				hir_fatal(0, message);
				break;
			}
		}

		/* Alloc an end block. */
		end_block = hir_malloc(sizeof(struct hir_block));
		if (end_block == NULL) {
			hir_out_of_memory();
			break;
		}
		memset(end_block, 0, sizeof(struct hir_block));
		end_block->id = block_id_top++;
		end_block->type = HIR_BLOCK_END;

		/* Set end_block to the succ of func_block. */
		func_block->succ = end_block;

		/* Visit the stmt_list. */
		/* Begin the function scope (params + top-level vars). */
		hir_scope_begin_func();
		if (!hir_scope_push(afunc->stmt_list))
			break;
		{
			/* Register parameters (added first by
			   hir_visit_param_list, so the local list holds
			   exactly the params at this point). */
			struct hir_local *plocal;
			struct hir_scope_entry *pentry;
			bool param_ok;
			param_ok = true;
			plocal = func_block->val.func.local;
			while (plocal != NULL) {
				if (!hir_scope_add_entry(0,
							 plocal->symbol,
							 true, false, &pentry)) {
					param_ok = false;
					break;
				}
				pentry->int_name = pentry->src_name;
				plocal = plocal->next;
			}
			if (!param_ok)
				break;
		}

		if (afunc->stmt_list != NULL) {
			/* Pre-allocate a first inner basic block. */
			func_block->val.func.inner = hir_malloc(sizeof(struct hir_block));
			if (func_block->val.func.inner == NULL) {
				hir_out_of_memory();
				break;
			}
			memset(func_block->val.func.inner, 0, sizeof(struct hir_block));
			func_block->val.func.inner->id = block_id_top++;
			func_block->val.func.inner->type = HIR_BLOCK_BASIC;
			func_block->val.func.inner->parent = func_block;

			/* Visit the stmt_list. */
			cur_block = func_block->val.func.inner;
			prev_block = NULL;
			if (!hir_visit_stmt_list(&cur_block,		/* cur_block */
						 &prev_block,		/* prev_block */
						 func_block,		/* parent_block*/
						 afunc->stmt_list))	/* stmt_list */
				break;

			/* If the first inner block was garbage-collected. */
			if (cur_block == NULL)
				func_block->val.func.inner = NULL;
		}

		/* End the function scope. */
		hir_scope_pop();

		if (afunc->func_kind == NOCT_FUNC_GPU) {
			char gpu_error[256];
			if (!hir_gpu_build_kernel(func_block, afunc, gpu_error,
						  sizeof(gpu_error))) {
				hir_fatal(hir_error_line, gpu_error);
				break;
			}
		}

		/* Store func_block to the table. */
		hir_func_tbl[hir_func_count] = func_block;
		hir_func_count++;

#ifdef DEBUG_DUMP
		hir_dump_block(func_block);
#endif

		/* Succeeded. */
		return true;
	} while (0);

	/* Failed. */
	if (func_block != NULL)
		hir_free_block(func_block);
	return false;
}

/* Visit an AST stmt_list. */
static bool
hir_visit_stmt_list(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt_list *stmt_list)
{
	struct hir_block *p_search;
	struct ast_stmt *cur_astmt;
	bool is_control;

	assert(cur_block != NULL);
	assert(prev_block != NULL);
	assert(parent_block != NULL);

	/* Assume we have a first block allocated. */
	assert(*cur_block != NULL);
	assert((*cur_block)->type == HIR_BLOCK_BASIC);

	/* Visit each stmt. */
	cur_astmt = NULL;
	is_control = false;
	if (stmt_list != NULL) {
		assert(*cur_block != NULL);

		cur_astmt = stmt_list->list;
		while (cur_astmt != NULL) {
			/* Break if the astmt is a loop-control statement. */
			if (cur_astmt->type == AST_STMT_CONTINUE ||
			    cur_astmt->type == AST_STMT_BREAK) {
				is_control = true;
				break;
			}

			/* Visit a stmt. */
			if (!hir_visit_stmt(cur_block, prev_block, parent_block, cur_astmt))
				return false;

			assert(*cur_block != NULL);

			/* Break if the astmt is a control statement. */
			if (cur_astmt->type == AST_STMT_RETURN) {
				is_control = true;
				break;
			}

			cur_astmt = cur_astmt->next;
		}
	}

	/* Terminate with a proper succ. */
	if (cur_astmt != NULL && is_control) {
		/* If the control stopped with... */
		assert(cur_astmt != NULL);
		switch (cur_astmt->type) {
		case AST_STMT_CONTINUE:
			/* Find the inner most loop. */
			p_search = parent_block;
			while (p_search != NULL) {
				if (p_search->type == HIR_BLOCK_FOR ||
				    p_search->type == HIR_BLOCK_WHILE)
					break;
				p_search = p_search->parent;
			}
			if (p_search == NULL) {
				hir_fatal(cur_astmt->line, N_TR("continue appeared outside loop."));
				return false;
			}

			/* Continue with the first inner block. */
			if (p_search->type == HIR_BLOCK_FOR) {
				assert(p_search->val.for_.inner != NULL);
				(*cur_block)->succ = p_search->val.for_.inner;
				(*cur_block)->stop = true;
			} else if (p_search->type == HIR_BLOCK_WHILE) {
				assert(p_search->val.while_.inner != NULL);
				(*cur_block)->succ = p_search->val.while_.inner;
				(*cur_block)->stop = true;
			}
			break;
		case AST_STMT_BREAK:
			/* Find the inner most loop. */
			p_search = parent_block;
			while (p_search != NULL) {
				if (p_search->type == HIR_BLOCK_FOR ||
				    p_search->type == HIR_BLOCK_WHILE)
					break;
				p_search = p_search->parent;
			}
			if (p_search == NULL) {
				hir_fatal(cur_astmt->line, N_TR("continue appeared outside loop."));
				return false;
			}

			/* Continue with the block after the loop. */
			(*cur_block)->succ = p_search->succ;
			(*cur_block)->stop = true;
			break;
		case AST_STMT_RETURN:
			/* Search a func block.*/
			p_search = *cur_block;
			do {
				if (p_search->parent != NULL) {
					p_search = p_search->parent;
				} else {
					if (p_search->type == HIR_BLOCK_FUNC)
						break;
					assert(p_search->type == HIR_BLOCK_IF);
					p_search = p_search->val.if_.chain_prev;
				}
			} while (1);
			assert(p_search->succ != NULL);
			assert(p_search->succ->type == HIR_BLOCK_END);

			/* Go to HIR_BLOCK_END. */
			(*cur_block)->succ = p_search->succ;
			(*cur_block)->stop = true;
			(*cur_block)->is_return_edge = true;
			break;
		default:
			assert(NEVER_COME_HERE);
			break;
		}
	} else {
		/* If the end of... */
		switch (parent_block->type) {
		case HIR_BLOCK_FUNC:
			/* Search a func block.*/
			p_search = parent_block;
			while (p_search->parent != NULL)
				p_search = p_search->parent;
			assert(p_search->type == HIR_BLOCK_FUNC);
			assert(p_search->succ != NULL);
			assert(p_search->succ->type == HIR_BLOCK_END);

			/* Go to HIR_BLOCK_END. */
			(*cur_block)->succ = p_search->succ;
			(*cur_block)->stop = true;
			break;
		case HIR_BLOCK_IF:
			/* Find the chain-top if block. */
			if (parent_block->succ != NULL) {
				/* Parent is if block */
				p_search = parent_block;
			} else {
				/* Parent is else-if or else block. Use its parent, i.e., if block. */
				p_search = parent_block->parent;
			}

			/* Go to the placeholder block after if block. */
			(*cur_block)->succ = p_search->succ;
			(*cur_block)->stop = true;
			break;
		case HIR_BLOCK_FOR:
			/* Continue to the first inner block. */
			assert(parent_block->val.for_.inner != NULL);
			(*cur_block)->succ = parent_block->val.for_.inner;
			(*cur_block)->stop = true;
			break;
		case HIR_BLOCK_WHILE:
			/* Continue to the first inner block. */
			assert(parent_block->val.while_.inner != NULL);
			(*cur_block)->succ = parent_block->val.while_.inner;
			(*cur_block)->stop = true;
			break;
		default:
			assert(NEVER_COME_HERE);
			break;
		}
	}

	return true;
}

/* Visit an AST stmt. */
static bool
hir_visit_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	bool result;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);

	/* For scope-resolution error messages. */
	hir_scope_line = cur_astmt->line;

	hir_error_line = cur_astmt->line;

	switch (cur_astmt->type) {
	case AST_STMT_EXPR:
		result = hir_visit_expr_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_ASSIGN:
		result = hir_visit_assign_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_IF:
		result = hir_visit_if_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_ELIF:
		result = hir_visit_elif_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_ELSE:
		result = hir_visit_else_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_WHILE:
		result = hir_visit_while_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_FOR:
		result = hir_visit_for_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	case AST_STMT_RETURN:
		result = hir_visit_return_stmt(cur_block, prev_block, parent_block, cur_astmt);
		break;
	default:
		result = false;
		assert(NEVER_COME_HERE);
		break;
	}

	return result;
}

/* Visit an AST expr stmt. */
static bool
hir_visit_expr_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_stmt *hstmt;

	UNUSED_PARAMETER(parent_block);
	UNUSED_PARAMETER(prev_block);

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert((*cur_block)->type == HIR_BLOCK_BASIC);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_EXPR);

	/* Assume we are on a basic block. */
	assert((*cur_block)->type == HIR_BLOCK_BASIC);

	/* Allocate an hstmt. */
	hstmt = hir_malloc(sizeof(struct hir_stmt));
	if (hstmt == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(hstmt, 0, sizeof(struct hir_stmt));
	hstmt->line = cur_astmt->line;

	/* There is no LHS for an expr stmt. */
	hstmt->lhs = NULL;

	/* Visit an expr. */
	if (!hir_visit_expr(&hstmt->rhs, cur_astmt->val.expr.expr)) {
		hir_free_stmt(hstmt);
		return false;
	}

	/* Add hstmt to the end of the block. */
	HIR_ADD_TO_LAST(struct hir_stmt, (*cur_block)->val.basic.stmt_list, hstmt);

	/* Set a block line number if this is a first stmt in the block. */
	if ((*cur_block)->val.basic.stmt_list == hstmt)
		(*cur_block)->line = cur_astmt->line;

	/* Continue on the same basic block. */

	return true;
}

/* Visit an assign stmt. */
static bool
hir_visit_assign_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_stmt *hstmt;
	bool is_lhs_ok;

	UNUSED_PARAMETER(parent_block);
	UNUSED_PARAMETER(prev_block);

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert((*cur_block)->type == HIR_BLOCK_BASIC);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_ASSIGN);

	/* Allocate an hstmt. */
	hstmt = hir_malloc(sizeof(struct hir_stmt));
	if (hstmt == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(hstmt, 0, sizeof(struct hir_stmt));
	hstmt->line = cur_astmt->line;

	/* Declarations (var/let): declare-first so the LHS never goes
	   through use resolution, and the initializer stays inside the
	   TDZ (docs/design/04-scoping.md). */
	if (cur_astmt->val.assign.is_var || cur_astmt->val.assign.is_let) {
		struct ast_expr *alhs;
		struct hir_term *lhs_term;
		struct hir_expr *lhs_expr;
		const char *src_name;
		const char *int_name;
		int anno_tag;
		int anno_packed_type;
		bool anno_restricted;
		int constructor_packed_type;
		int storage_class;

		alhs = cur_astmt->val.assign.lhs;
		if (!(alhs != NULL &&
		      alhs->type == AST_EXPR_TERM &&
		      alhs->val.term.term->type == AST_TERM_SYMBOL)) {
			hir_fatal(cur_astmt->line, N_TR("var is specified without a single symbol."));
			hir_free_stmt(hstmt);
			return false;
		}
		src_name = alhs->val.term.term->val.symbol;
		if (hir_current_func_kind == NOCT_FUNC_GPU &&
		    strcmp(src_name, "Accel") == 0) {
			hir_fatal(cur_astmt->line,
				  N_TR("'Accel' is a reserved name inside __gpu func."));
			hir_free_stmt(hstmt);
			return false;
		}

		/* Validate the optional type annotation (hint only). */
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    cur_astmt->val.assign.type_name == NULL) {
			hir_fatal(cur_astmt->line,
				  N_TR("Every explicit __fast local requires a type annotation."));
			hir_free_stmt(hstmt);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    cur_astmt->val.assign.type_name != NULL &&
		    strchr(cur_astmt->val.assign.type_name, '(') != NULL) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast local must have a primitive type."));
			hir_free_stmt(hstmt);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    cur_astmt->val.assign.type_name != NULL &&
		    strcmp(cur_astmt->val.assign.type_name, "int") != 0 &&
		    strcmp(cur_astmt->val.assign.type_name, "long") != 0 &&
		    strcmp(cur_astmt->val.assign.type_name, "float") != 0 &&
		    strcmp(cur_astmt->val.assign.type_name, "double") != 0) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast local type must be int, long, float, or double exactly."));
			hir_free_stmt(hstmt);
			return false;
		}
		if (!hir_check_type_annotation(cur_astmt->line,
					       cur_astmt->val.assign.type_name,
					       &anno_tag,
					       &anno_packed_type,
					       &anno_restricted)) {
			hir_free_stmt(hstmt);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    !(anno_tag == NOCT_VALUE_INT || anno_tag == NOCT_VALUE_LONG ||
		      anno_tag == NOCT_VALUE_FLOAT || anno_tag == NOCT_VALUE_DOUBLE)) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast local type must be int, long, float, or double."));
			hir_free_stmt(hstmt);
			return false;
		}

		if (!hir_scope_declare(cur_astmt->line, src_name,
				       cur_astmt->val.assign.is_let,
				       &int_name)) {
			hir_free_stmt(hstmt);
			return false;
		}

		/* Visit RHS while the binding is still in its TDZ. */
		if (!hir_visit_expr(&hstmt->rhs, cur_astmt->val.assign.rhs)) {
			hir_free_stmt(hstmt);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    hir_fast_infer_expr_type(hstmt->rhs) != anno_tag) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast local initializer must exactly match its declared type."));
			hir_free_stmt(hstmt);
			return false;
		}
		hir_scope_mark_declared(src_name);

		/* Build the LHS term with the internal name. */
		lhs_term = hir_malloc(sizeof(struct hir_term));
		lhs_expr = hir_malloc(sizeof(struct hir_expr));
		if (lhs_term == NULL || lhs_expr == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(lhs_term, 0, sizeof(struct hir_term));
		memset(lhs_expr, 0, sizeof(struct hir_expr));
		lhs_term->type = HIR_TERM_SYMBOL;
		lhs_term->val.symbol = hir_strdup(int_name);
		if (lhs_term->val.symbol == NULL) {
			hir_out_of_memory();
			return false;
		}
		lhs_expr->type = HIR_EXPR_TERM;
		lhs_expr->val.term.term = lhs_term;
		hstmt->lhs = lhs_expr;

		if (!hir_add_local(*cur_block, lhs_term->val.symbol))
			return false;
		constructor_packed_type = hir_packed_constructor_type(hstmt->rhs);
		if (anno_packed_type < 0 && constructor_packed_type >= 0) {
			anno_tag = NOCT_VALUE_PACKED;
			anno_packed_type = constructor_packed_type;
		}
		storage_class = anno_packed_type >= 0 ||
			constructor_packed_type >= 0 ?
			HIR_LOCAL_STORAGE_LOGICAL_BUFFER :
			HIR_LOCAL_STORAGE_SCALAR;
		hir_set_local_declaration(*cur_block, lhs_term->val.symbol,
					  cur_astmt->val.assign.is_let ?
						HIR_LOCAL_DECL_LET :
						HIR_LOCAL_DECL_VAR,
					  anno_tag,
					  hir_declared_scalar_kind(
						  cur_astmt->val.assign.type_name),
					  anno_packed_type,
					  storage_class,
					  cur_astmt->line,
					  hstmt,
					  hstmt->rhs);

		/* Add hstmt to the end of the block. */
		HIR_ADD_TO_LAST(struct hir_stmt, (*cur_block)->val.basic.stmt_list, hstmt);
		if ((*cur_block)->val.basic.stmt_list == hstmt)
			(*cur_block)->line = cur_astmt->line;
		return true;
	}

	/* Visit LHS. */
	if (!hir_visit_expr(&hstmt->lhs, cur_astmt->val.assign.lhs)) {
		hir_free_stmt(hstmt);
		return false;
	}

	/* Check LHS. */
	is_lhs_ok = false;
	if (hstmt->lhs->type == HIR_EXPR_TERM &&
	    hstmt->lhs->val.term.term->type == HIR_TERM_SYMBOL)
		is_lhs_ok = true;
	else if (hstmt->lhs->type == HIR_EXPR_SUBSCR)
		is_lhs_ok = true;
	else if (hstmt->lhs->type == HIR_EXPR_DOT)
		is_lhs_ok = true;
	if (!is_lhs_ok) {
		hir_fatal(cur_astmt->line, N_TR("LHS is not a term or an array element."));
		hir_free_stmt(hstmt);
		return false;
	}

	/* Reject assignment to a let binding. */
	if (hstmt->lhs->type == HIR_EXPR_TERM &&
	    hstmt->lhs->val.term.term->type == HIR_TERM_SYMBOL) {
		if (!hir_scope_check_let_assign(cur_astmt->line,
						hstmt->lhs->val.term.term->val.symbol)) {
			hir_free_stmt(hstmt);
			return false;
		}
	}

	/* Visit RHS. */
	if (!hir_visit_expr(&hstmt->rhs, cur_astmt->val.assign.rhs)) {
		hir_free_stmt(hstmt);
		return false;
	}
	if (hir_current_func_kind == NOCT_FUNC_FAST) {
		int lhs_type;
		int rhs_type;
		lhs_type = hir_fast_infer_expr_type(hstmt->lhs);
		rhs_type = hir_fast_infer_expr_type(hstmt->rhs);
		if (lhs_type < 0 || rhs_type < 0 || lhs_type != rhs_type) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast assignment requires exactly matching primitive types."));
			hir_free_stmt(hstmt);
			return false;
		}
	}

	/* Add hstmt to the end of the block. */
	HIR_ADD_TO_LAST(struct hir_stmt, (*cur_block)->val.basic.stmt_list, hstmt);

	/* Set a block line number if this is a first stmt in the block. */
	if ((*cur_block)->val.basic.stmt_list == hstmt)
		(*cur_block)->line = cur_astmt->line;

	/* Continue on the same basic block. */

	return true;
}

/* Add a local variable entry. */
bool
hir_add_local(
	struct hir_block *cur_block,
	const char *symbol)
{
	struct hir_block *func;
	struct hir_local *local;
	int index;

	/* Get a root func block. */
	func = cur_block;
	while (func->type != HIR_BLOCK_FUNC)
		func = func->parent;

	/* Search a symbol. */
	index = 0;
	local = func->val.func.local;
	while (local != NULL) {
		/* If already exists. */
		if (strcmp(local->symbol, symbol) == 0)
			return true;
		index++;
		local = local->next;
	}

	/* Add a local variable symbol. */
	local = hir_malloc(sizeof(struct hir_local));
	if (local == NULL) {
		hir_out_of_memory();
		return false;
	}
	local->symbol = hir_strdup(symbol);
	if (local->symbol == NULL) {
		hir_out_of_memory();
		hir_free(local);
		return false;
	}
	local->index = index;
	/* -1 = unproven; NOT zero (NOCT_VALUE_INT == 0; see hir.h). */
	local->proven_type = -1;
	local->is_parameter = false;
	local->is_let = false;
	local->declaration_kind = HIR_LOCAL_DECL_UNKNOWN;
	local->declared_type = -1;
	local->declared_scalar_kind = HIR_DECL_SCALAR_UNKNOWN;
	local->declared_packed_type = -1;
	local->storage_class = HIR_LOCAL_STORAGE_UNKNOWN;
	local->declaration_line = -1;
	local->declaration_stmt = NULL;
	local->initializer = NULL;
	local->next = func->val.func.local;
	func->val.func.local = local;

	return true;
}

static struct hir_local *
hir_find_local(
	struct hir_block *block,
	const char *symbol)
{
	struct hir_local *local;

	while (block != NULL && block->type != HIR_BLOCK_FUNC)
		block = block->parent;
	if (block == NULL)
		return NULL;
	local = block->val.func.local;
	while (local != NULL) {
		if (strcmp(local->symbol, symbol) == 0)
			return local;
		local = local->next;
	}
	return NULL;
}

static void
hir_set_local_declaration(
	struct hir_block *block,
	const char *symbol,
	int declaration_kind,
	int declared_type,
	int declared_scalar_kind,
	int declared_packed_type,
	int storage_class,
	int line,
	const struct hir_stmt *declaration_stmt,
	const struct hir_expr *initializer)
{
	struct hir_local *local;

	local = hir_find_local(block, symbol);
	assert(local != NULL);
	if (local == NULL)
		return;
	local->is_parameter = declaration_kind == HIR_LOCAL_DECL_PARAMETER;
	local->is_let = declaration_kind == HIR_LOCAL_DECL_LET;
	local->declaration_kind = declaration_kind;
	local->declared_type = declared_type;
	local->declared_scalar_kind = declared_scalar_kind;
	local->declared_packed_type = declared_packed_type;
	local->storage_class = storage_class;
	local->declaration_line = line;
	local->declaration_stmt = declaration_stmt;
	local->initializer = initializer;
}

/* Return a NOCT_PACKED_* kind for a direct Packed.* constructor. */
static int
hir_packed_constructor_type(
	const struct hir_expr *expr)
{
	const struct hir_expr *obj;
	const char *name;

	if (expr == NULL || expr->type != HIR_EXPR_THISCALL)
		return -1;
	obj = expr->val.thiscall.obj;
	if (obj == NULL || obj->type != HIR_EXPR_TERM ||
	    obj->val.term.term == NULL ||
	    obj->val.term.term->type != HIR_TERM_SYMBOL ||
	    strcmp(obj->val.term.term->val.symbol, "Packed") != 0)
		return -1;
	name = expr->val.thiscall.func;
	if (strcmp(name, "int8") == 0) return NOCT_PACKED_INT8;
	if (strcmp(name, "uint8") == 0) return NOCT_PACKED_UINT8;
	if (strcmp(name, "int16") == 0) return NOCT_PACKED_INT16;
	if (strcmp(name, "uint16") == 0) return NOCT_PACKED_UINT16;
	if (strcmp(name, "int32") == 0) return NOCT_PACKED_INT32;
	if (strcmp(name, "uint32") == 0) return NOCT_PACKED_UINT32;
	if (strcmp(name, "int64") == 0) return NOCT_PACKED_INT64;
	if (strcmp(name, "uint64") == 0) return NOCT_PACKED_UINT64;
	if (strcmp(name, "float32") == 0) return NOCT_PACKED_FLOAT32;
	if (strcmp(name, "float64") == 0) return NOCT_PACKED_FLOAT64;
	return -1;
}

static int
hir_declared_scalar_kind(
	const char *type_name)
{
	if (type_name == NULL)
		return HIR_DECL_SCALAR_UNKNOWN;
	if (strcmp(type_name, "int") == 0 ||
	    strcmp(type_name, "i32") == 0)
		return HIR_DECL_SCALAR_INT32;
	if (strcmp(type_name, "u32") == 0)
		return HIR_DECL_SCALAR_UINT32;
	if (strcmp(type_name, "float") == 0)
		return HIR_DECL_SCALAR_FLOAT32;
	return HIR_DECL_SCALAR_OTHER;
}

/* Visit an AST "if" stmt. */
static bool
hir_visit_if_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_block *if_block;
	struct hir_block *exit_block;
	struct hir_block *inner_cur_block;
	struct hir_block *inner_prev_block;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_IF);

	/* Allocate an if block. */
	if ((*cur_block)->type == HIR_BLOCK_BASIC &&
	    (*cur_block)->val.basic.stmt_list == NULL) {
		/* Reuse an empty basic block. */
		(*cur_block)->type = HIR_BLOCK_IF;
		if_block = *cur_block;
	} else {
		/* Simply allocate. */
		if_block = hir_malloc(sizeof(struct hir_block));
		if (if_block == NULL) {
			hir_out_of_memory();
			return false;
		}
		if_block->type = HIR_BLOCK_IF;
		(*cur_block)->succ = if_block;
	}
	if_block->line = cur_astmt->line;
	if_block->parent = parent_block;
	if_block->val.if_.chain_next = NULL;
	if_block->val.if_.chain_prev = NULL;

	/* Alloc an inner block. */
	if_block->val.if_.inner = hir_malloc(sizeof(struct hir_block));
	if (if_block->val.if_.inner == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(if_block->val.if_.inner, 0, sizeof(struct hir_block));
	if_block->val.if_.inner->id = block_id_top++;
	if_block->val.if_.inner->type = HIR_BLOCK_BASIC;
	if_block->val.if_.inner->line = cur_astmt->line;
	if_block->val.if_.inner->parent = if_block;

	/* Allocate an exit block. (This may be reused as a basic block.) */
	exit_block = hir_malloc(sizeof(struct hir_block));
	if (exit_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(exit_block, 0, sizeof(struct hir_block));
	exit_block->id = block_id_top++;
	exit_block->type = HIR_BLOCK_BASIC;
	exit_block->succ = parent_block->succ;
	exit_block->parent = parent_block;
	if_block->succ = exit_block;

	/* Visit a cond expr. */
	if (!hir_visit_expr(&if_block->val.if_.cond, cur_astmt->val.if_.cond)) {
		hir_free_block(if_block);
		return false;
	}

	/* Visit an inner stmt_list */
	if (!hir_scope_push(cur_astmt->val.if_.stmt_list))
		return false;
	if (cur_astmt->val.if_.stmt_list != NULL) {
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth++;
		inner_cur_block = if_block->val.if_.inner;
		inner_prev_block = NULL;
		if (!hir_visit_stmt_list(&inner_cur_block,	/* cur_block */
					 &inner_prev_block,	/* prev_block */
					 if_block,		/* parent_block */
					 cur_astmt->val.if_.stmt_list)) {
			if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
			hir_free_block(if_block);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
	}

	/* End the block scope. */
	hir_scope_pop();

	/* Move the cursor to the exit block. */
	*cur_block = exit_block;
	*prev_block = if_block;

	assert((*cur_block)->type != HIR_BLOCK_END);

	return true;
}

/* Visit an AST "else if" stmt. */
static bool
hir_visit_elif_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_block *elif_block;
	struct hir_block *inner_cur_block;
	struct hir_block *inner_prev_block;
	struct hir_block *b;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert((*prev_block)->type == HIR_BLOCK_IF);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);

	/* Check the previous block. */
	if (*prev_block == NULL || (*prev_block)->type != HIR_BLOCK_IF) {
		hir_fatal(cur_astmt->line, N_TR("else-if block appeared without if block."));
		return false;
	}
	if ((*prev_block)->val.if_.cond == NULL) {
		hir_fatal(cur_astmt->line, N_TR("else-if appeared after else."));
		return false;
	}
	assert((*prev_block)->val.if_.chain_next == NULL);

	/*
	 * The exit block is taken from the chain's first if-block below,
	 * so parent_block itself is not used here. When an if/else-if
	 * chain is nested inside another if's branch, parent_block is
	 * that enclosing if-block, which is fine.
	 */

	/* Alloc an else-if block. */
	elif_block = hir_malloc(sizeof(struct hir_block));
	if (elif_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	elif_block->id = block_id_top++;
	elif_block->type = HIR_BLOCK_IF;
	elif_block->succ = NULL;
	elif_block->parent = parent_block;
	elif_block->line = cur_astmt->line;
	elif_block->val.if_.chain_prev = (*prev_block);
	(*prev_block)->val.if_.chain_next = elif_block;

	/* Get a first if-block. */
	b = elif_block->val.if_.chain_prev;
	while (b->val.if_.chain_prev != NULL)
		b = b->val.if_.chain_prev;
	elif_block->parent = b;

	/* Alloc an inner block. */
	elif_block->val.if_.inner = hir_malloc(sizeof(struct hir_block));
	if (elif_block->val.if_.inner == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(elif_block->val.if_.inner, 0, sizeof(struct hir_block));
	elif_block->val.if_.inner->id = block_id_top++;
	elif_block->val.if_.inner->type = HIR_BLOCK_BASIC;
	elif_block->val.if_.inner->parent = elif_block;
	elif_block->val.if_.inner->line = cur_astmt->line;

	/* Visit a cond expr. */
	if (!hir_visit_expr(&elif_block->val.if_.cond, cur_astmt->val.if_.cond)) {
		hir_free_block(elif_block);
		return false;
	}

	/* Visit an inner stmt_list */
	if (!hir_scope_push(cur_astmt->val.elif.stmt_list))
		return false;
	if (cur_astmt->val.elif.stmt_list != NULL) {
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth++;
		inner_cur_block = elif_block->val.if_.inner;
		inner_prev_block = NULL;
		if (!hir_visit_stmt_list(&inner_cur_block,	/* cur_block */
					 &inner_prev_block,	/* prev_block */
					 elif_block,		/* parent_block */
					 cur_astmt->val.elif.stmt_list)) {
			if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
			hir_free_block(elif_block);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
	}

	/* End the block scope. */
	hir_scope_pop();

	/* Move the cursor to the exit block. */
	*cur_block = elif_block->parent->succ;
	*prev_block = elif_block;

	return true;
}

/* Visit an AST "else" stmt. */
static bool
hir_visit_else_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_block *else_block;
	struct hir_block *inner_cur_block;
	struct hir_block *inner_prev_block;
	struct hir_block *b;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert((*prev_block)->type == HIR_BLOCK_IF);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);

	/* Check the previous block. */
	if (*prev_block == NULL || (*prev_block)->type != HIR_BLOCK_IF) {
		hir_fatal(cur_astmt->line, N_TR("else-if block appeared without if block."));
		return false;
	}
	if ((*prev_block)->val.if_.cond == NULL) {
		hir_fatal(cur_astmt->line, N_TR("else appeared after else."));
		return false;
	}
	assert((*prev_block)->val.if_.chain_next == NULL);

	/* Alloc an else block. */
	else_block = hir_malloc(sizeof(struct hir_block));
	if (else_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(else_block, 0, sizeof(struct hir_block));
	else_block->id = block_id_top++;
	else_block->type = HIR_BLOCK_IF;
	else_block->succ = NULL;
	else_block->parent = parent_block;
	else_block->line = cur_astmt->line;
	else_block->val.if_.chain_next = NULL;
	else_block->val.if_.chain_prev = (*prev_block);
	(*prev_block)->val.if_.chain_next = else_block;

	/* Get a first if-block. */
	b = else_block->val.if_.chain_prev;
	while (b->val.if_.chain_prev != NULL)
		b = b->val.if_.chain_prev;
	else_block->parent = b;

	/* Alloc an inner block. */
	else_block->val.if_.inner = hir_malloc(sizeof(struct hir_block));
	if (else_block->val.if_.inner == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(else_block->val.if_.inner, 0, sizeof(struct hir_block));
	else_block->val.if_.inner->id = block_id_top++;
	else_block->val.if_.inner->type = HIR_BLOCK_BASIC;
	else_block->val.if_.inner->parent = else_block;
	else_block->val.if_.inner->line = cur_astmt->line;

	/* Visit an inner stmt_list */
	if (!hir_scope_push(cur_astmt->val.else_.stmt_list))
		return false;
	if (cur_astmt->val.else_.stmt_list != NULL) {
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth++;
		inner_cur_block = else_block->val.if_.inner;
		inner_prev_block = NULL;
		if (!hir_visit_stmt_list(&inner_cur_block,	/* cur_block */
					 &inner_prev_block,	/* prev_block */
					 else_block,		/* parent_block */
					 cur_astmt->val.else_.stmt_list)) {
			if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
			hir_free_block(else_block);
			return false;
		}
		if (hir_current_func_kind == NOCT_FUNC_FAST) hir_fast_cond_depth--;
	}

	/* End the block scope. */
	hir_scope_pop();

	/* Move the cursor. */
	*cur_block = else_block->parent->succ;
	*prev_block = else_block;

	return true;
}

/* Visit an AST "while" stmt. */
static bool
hir_visit_while_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_block *while_block;
	struct hir_block *exit_block;
	struct hir_block *inner_cur_block;
	struct hir_block *inner_prev_block;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_WHILE);

	/* Alloc a while block. */
	if ((*cur_block)->type == HIR_BLOCK_BASIC &&
	    (*cur_block)->val.basic.stmt_list == NULL) {
		/* Reuse an empty basic block. */
		while_block = *cur_block;
		while_block->type = HIR_BLOCK_WHILE;
		while_block->parent = parent_block;
		while_block->line = cur_astmt->line;
	} else {
		while_block = hir_malloc(sizeof(struct hir_block));
		if (while_block == NULL) {
			hir_out_of_memory();
			return false;
		}
		while_block->id = block_id_top++;
		while_block->type = HIR_BLOCK_WHILE;
		while_block->parent = parent_block;
		while_block->line = cur_astmt->line;
		(*cur_block)->succ = while_block;
	}

	/* Alloc an inner block. */
	while_block->val.while_.inner = hir_malloc(sizeof(struct hir_block));
	if (while_block->val.while_.inner == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(while_block->val.while_.inner, 0, sizeof(struct hir_block));
	while_block->id = block_id_top++;
	while_block->val.while_.inner->type = HIR_BLOCK_BASIC;
	while_block->val.while_.inner->parent = while_block;
	while_block->val.while_.inner->line = cur_astmt->line;

	/* Alloc an exit-block. */
	exit_block = hir_malloc(sizeof(struct hir_block));
	if (exit_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(exit_block, 0, sizeof(struct hir_block));
	exit_block->id = block_id_top++;
	exit_block->type = HIR_BLOCK_BASIC;
	exit_block->parent = parent_block;
	while_block->succ = exit_block;

	/* Visit a cond expr. */
	if (!hir_visit_expr(&while_block->val.while_.cond, cur_astmt->val.while_.cond)) {
		hir_free_block(while_block);
		return false;
	}

	/* Visit an inner stmt_list */
	if (!hir_scope_push(cur_astmt->val.while_.stmt_list))
		return false;
	if (cur_astmt->val.while_.stmt_list != NULL) {
		inner_cur_block = while_block->val.while_.inner;
		inner_prev_block = NULL;
		if (!hir_visit_stmt_list(&inner_cur_block,	/* cur_block */
					 &inner_prev_block,	/* prev_block */
					 while_block,		/* parent_block */
					 cur_astmt->val.while_.stmt_list)) {
			hir_free_block(while_block);
			return false;
		}
	}

	/* End the block scope. */
	hir_scope_pop();

	/* Move the cursor to the exit block. */
	*cur_block = exit_block;
	*prev_block = while_block;

	return true;
}

/* Visit an AST "for" stmt. */
static bool
hir_visit_for_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_block *for_block;
	struct hir_block *exit_block;
	struct hir_block *inner_cur_block;
	struct hir_block *inner_prev_block;

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_FOR);

	/* Alloc an for block. */
	if ((*cur_block)->type == HIR_BLOCK_BASIC &&
	    (*cur_block)->val.basic.stmt_list == NULL) {
		/* Reuse an empty basic block. */
		for_block = *cur_block;
		for_block->type = HIR_BLOCK_FOR;
		for_block->parent = parent_block;
		for_block->line = cur_astmt->line;
	} else {
		for_block = hir_malloc(sizeof(struct hir_block));
		if (for_block == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(for_block, 0, sizeof(struct hir_block));
		for_block->id = block_id_top++;
		for_block->type = HIR_BLOCK_FOR;
		for_block->parent = parent_block;
		for_block->line = cur_astmt->line;
		(*cur_block)->succ = for_block;
	}

	/* Alloc an inner block. */
	for_block->val.for_.inner = hir_malloc(sizeof(struct hir_block));
	if (for_block->val.for_.inner == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(for_block->val.for_.inner, 0, sizeof(struct hir_block));
	for_block->val.for_.inner->id = block_id_top++;
	for_block->val.for_.inner->type = HIR_BLOCK_BASIC;
	for_block->val.for_.inner->parent = for_block;
	for_block->val.for_.inner->line = cur_astmt->line;

	/* Alloc an exit-block. */
	exit_block = hir_malloc(sizeof(struct hir_block));
	if (exit_block == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(exit_block, 0, sizeof(struct hir_block));
	exit_block->id = block_id_top++;
	exit_block->type = HIR_BLOCK_BASIC;
	exit_block->parent = parent_block;
	exit_block->succ = parent_block->succ;
	for_block->succ = exit_block;

	/* Mark ranged-for. (Loop variables are registered after the
	   start/stop/collection exprs are visited in the OUTER scope;
	   see the loop-body scope section below.) */
	if (cur_astmt->val.for_.counter_symbol)
		for_block->val.for_.is_ranged = true;

	/* Visit the start and stop exprs. */
	if (cur_astmt->val.for_.start != NULL) {
		if (!hir_visit_expr(&for_block->val.for_.start, cur_astmt->val.for_.start)) {
			hir_free_block(for_block);
			return false;
		}
	}
	if (cur_astmt->val.for_.stop != NULL) {
		if (!hir_visit_expr(&for_block->val.for_.stop, cur_astmt->val.for_.stop)) {
			hir_free_block(for_block);
			return false;
		}
	}

	/* Visit the collection expr. */
	if (cur_astmt->val.for_.collection != NULL) {
		if (!hir_visit_expr(&for_block->val.for_.collection, cur_astmt->val.for_.collection)) {
			hir_free_block(for_block);
			return false;
		}
	}

	/* Enter the loop-body scope; loop variables live in it. */
	if (!hir_scope_push(cur_astmt->val.for_.stmt_list)) {
		hir_free_block(for_block);
		return false;
	}
	{
		static const char *empty = "";
		const char *names[3];
		char **fields[3];
		int k;
		names[0] = cur_astmt->val.for_.counter_symbol;
		names[1] = cur_astmt->val.for_.key_symbol;
		names[2] = cur_astmt->val.for_.value_symbol;
		fields[0] = &for_block->val.for_.counter_symbol;
		fields[1] = &for_block->val.for_.key_symbol;
		fields[2] = &for_block->val.for_.value_symbol;
		(void)empty;
		for (k = 0; k < 3; k++) {
			const char *iname;
			if (names[k] == NULL)
				continue;
			if (!hir_scope_declare(cur_astmt->line, names[k],
					       false, &iname)) {
				hir_free_block(for_block);
				return false;
			}
			hir_scope_mark_declared(names[k]);
			*fields[k] = hir_strdup(iname);
			if (*fields[k] == NULL) {
				hir_out_of_memory();
				return false;
			}
			if (!hir_add_local(*cur_block, *fields[k]))
				return false;
			hir_set_local_declaration(*cur_block, *fields[k],
						  HIR_LOCAL_DECL_LOOP_COUNTER,
						  hir_current_func_kind == NOCT_FUNC_FAST &&
						  k == 0 && for_block->val.for_.is_ranged ?
							NOCT_VALUE_INT : -1,
						  hir_current_func_kind == NOCT_FUNC_FAST &&
						  k == 0 && for_block->val.for_.is_ranged ?
							HIR_DECL_SCALAR_INT32 :
							HIR_DECL_SCALAR_UNKNOWN,
						  -1,
						  HIR_LOCAL_STORAGE_SCALAR,
						  cur_astmt->line,
						  NULL,
						  NULL);
		}
	}

	/* Make a literal ranged-for domain available to mandatory fast
	   bounds diagnostics while its body is constructed. */
	if (hir_current_func_kind == NOCT_FUNC_FAST &&
	    cur_astmt->val.for_.counter_symbol != NULL &&
	    hir_fast_loop_depth < HIR_FAST_LOOP_MAX) {
		int64_t start;
		int64_t stop;
		struct hir_fast_loop_domain *domain;
		domain = &hir_fast_loop[hir_fast_loop_depth++];
		domain->counter = cur_astmt->val.for_.counter_symbol;
		domain->known = hir_fast_ast_constant(cur_astmt->val.for_.start,
						      &start) &&
				hir_fast_ast_constant(cur_astmt->val.for_.stop,
						      &stop) && stop > start;
		if (domain->known) {
			domain->lower = start;
			domain->upper = stop - 1;
		}
	}

	/* Visit an inner stmt_list */
	inner_cur_block = for_block->val.for_.inner;
	inner_prev_block = NULL;
	if (!hir_visit_stmt_list(&inner_cur_block,	/* cur_block */
				 &inner_prev_block,	/* prev_block */
				 for_block,		/* parent_block */
				 cur_astmt->val.for_.stmt_list)) {
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    cur_astmt->val.for_.counter_symbol != NULL &&
		    hir_fast_loop_depth > 0)
			hir_fast_loop_depth--;
		hir_free_block(for_block);
		return false;
	}
	if (hir_current_func_kind == NOCT_FUNC_FAST &&
	    cur_astmt->val.for_.counter_symbol != NULL &&
	    hir_fast_loop_depth > 0)
		hir_fast_loop_depth--;

	/* End the loop-body scope. */
	hir_scope_pop();

	/* Move the cursor to the exit block. */
	*cur_block = exit_block;
	*prev_block = for_block;

	return true;
}

/* Visit an AST return stmt. */
static bool
hir_visit_return_stmt(
	struct hir_block **cur_block,
	struct hir_block **prev_block,
	struct hir_block *parent_block,
	struct ast_stmt *cur_astmt)
{
	struct hir_stmt *hstmt;

	UNUSED_PARAMETER(parent_block);
	UNUSED_PARAMETER(prev_block);

	assert(cur_block != NULL);
	assert(*cur_block != NULL);
	assert((*cur_block)->type == HIR_BLOCK_BASIC);
	assert(prev_block != NULL);
	assert(parent_block != NULL);
	assert(cur_astmt != NULL);
	assert(cur_astmt->type == AST_STMT_RETURN);

	/* Assume we are on a basic block. */
	assert((*cur_block)->type == HIR_BLOCK_BASIC);

	/* Allocate an hstmt. */
	hstmt = hir_malloc(sizeof(struct hir_stmt));
	if (hstmt == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(hstmt, 0, sizeof(struct hir_stmt));
	hstmt->line = cur_astmt->line;
	hstmt->is_bare_return = !cur_astmt->val.return_.has_value;

	/* Set LHS. */
	hstmt->lhs = hir_malloc(sizeof(struct hir_expr));
	if (hstmt->lhs == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(hstmt->lhs, 0, sizeof(struct hir_expr));
	hstmt->lhs->type = HIR_EXPR_TERM;
	hstmt->lhs->val.term.term = hir_malloc(sizeof(struct hir_term));
	if (hstmt->lhs->val.term.term == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(hstmt->lhs->val.term.term, 0, sizeof(struct hir_term));
	hstmt->lhs->val.term.term->type = HIR_TERM_SYMBOL;
	hstmt->lhs->val.term.term->val.symbol = hir_strdup("$return");
	if (hstmt->lhs->val.term.term->val.symbol == NULL) {
		hir_out_of_memory();
		return false;
	}

	/* Visit an expr. */
	if (!hir_visit_expr(&hstmt->rhs, cur_astmt->val.return_.expr)) {
		hir_free_stmt(hstmt);
		return false;
	}
	if (hir_current_func_kind == NOCT_FUNC_FAST &&
	    hir_current_func_block != NULL) {
		int declared_return;
		declared_return = hir_current_func_block->val.func.return_type;
		if ((hstmt->is_bare_return && declared_return != HIR_TYPE_VOID) ||
		    (!hstmt->is_bare_return &&
		     hir_fast_infer_expr_type(hstmt->rhs) != declared_return)) {
			hir_fatal(cur_astmt->line,
				  N_TR("A __fast return value must exactly match the declared return type."));
			hir_free_stmt(hstmt);
			return false;
		}
	}

	/* Add hstmt to the end of the block. */
	HIR_ADD_TO_LAST(struct hir_stmt, (*cur_block)->val.basic.stmt_list, hstmt);

	/* Continue on the same basic block. */

	return true;
}

static const struct fast_param_contract *
hir_fast_subscript_contract(const struct ast_expr *base)
{
	const char *symbol;
	uint32_t i;

	if (hir_current_func_kind != NOCT_FUNC_FAST ||
	    hir_current_func_block == NULL || base == NULL ||
	    base->type != AST_EXPR_TERM || base->val.term.term == NULL ||
	    base->val.term.term->type != AST_TERM_SYMBOL)
		return NULL;
	symbol = base->val.term.term->val.symbol;
	for (i = 0; i < hir_current_func_block->val.func.param_count; i++) {
		if (strcmp(symbol,
		    hir_current_func_block->val.func.param_name[i]) == 0 &&
		    hir_current_func_block->val.func.fast_signature->param[i].rank > 0)
			return &hir_current_func_block->val.func.fast_signature->param[i];
	}
	return NULL;
}

static bool
hir_fast_ast_constant(const struct ast_expr *expr, int64_t *value)
{
	if (expr == NULL) return false;
	if (expr->type == AST_EXPR_PAR)
		return hir_fast_ast_constant(expr->val.par.expr, value);
	if (expr->type == AST_EXPR_TERM && expr->val.term.term != NULL) {
		if (expr->val.term.term->type == AST_TERM_INT) {
			*value = expr->val.term.term->val.i;
			return true;
		}
		if (expr->val.term.term->type == AST_TERM_LONG) {
			*value = expr->val.term.term->val.l;
			return true;
		}
	}
	if (expr->type == AST_EXPR_NEG) {
		int64_t inner;
		if (hir_fast_ast_constant(expr->val.unary.expr, &inner) &&
		    inner != INT64_MIN) {
			*value = -inner;
			return true;
		}
	}
	return false;
}

static bool
hir_fast_index_interval(const struct ast_expr *expr, int64_t *lower,
			int64_t *upper)
{
	int i;
	int64_t constant;

	if (hir_fast_ast_constant(expr, &constant)) {
		*lower = constant;
		*upper = constant;
		return true;
	}
	if (expr != NULL && expr->type == AST_EXPR_TERM &&
	    expr->val.term.term != NULL &&
	    expr->val.term.term->type == AST_TERM_SYMBOL) {
		for (i = hir_fast_loop_depth - 1; i >= 0; i--) {
			if (hir_fast_loop[i].counter != NULL &&
			    strcmp(hir_fast_loop[i].counter,
				   expr->val.term.term->val.symbol) == 0 &&
			    hir_fast_loop[i].known) {
				*lower = hir_fast_loop[i].lower;
				*upper = hir_fast_loop[i].upper;
				return true;
			}
		}
	}
	if (expr != NULL &&
	    (expr->type == AST_EXPR_PLUS || expr->type == AST_EXPR_MINUS)) {
		int64_t lo;
		int64_t hi;
		int64_t c;
		if (hir_fast_index_interval(expr->val.binary.expr[0], &lo, &hi) &&
		    hir_fast_ast_constant(expr->val.binary.expr[1], &c)) {
			if (expr->type == AST_EXPR_MINUS) {
				if (c == INT64_MIN) return false;
				c = -c;
			}
			if ((c > 0 && hi > INT64_MAX - c) ||
			    (c < 0 && lo < INT64_MIN - c)) return false;
			*lower = lo + c;
			*upper = hi + c;
			return true;
		}
		if (expr->type == AST_EXPR_PLUS &&
		    hir_fast_ast_constant(expr->val.binary.expr[0], &c) &&
		    hir_fast_index_interval(expr->val.binary.expr[1], &lo, &hi)) {
			if ((c > 0 && hi > INT64_MAX - c) ||
			    (c < 0 && lo < INT64_MIN - c)) return false;
			*lower = lo + c;
			*upper = hi + c;
			return true;
		}
	}
	return false;
}

static bool
hir_fast_check_subscript(const struct ast_expr *expr)
{
	const struct fast_param_contract *contract;
	const struct ast_expr *index;
	uint32_t count;
	uint32_t axis;

	contract = hir_fast_subscript_contract(expr->val.binary.expr[0]);
	if (contract == NULL) {
		hir_fatal(hir_error_line,
			  N_TR("A __fast subscript base must be a shaped rpacked parameter."));
		return false;
	}
	if (expr->val.binary.expr[1]->type == AST_EXPR_ARRAY) {
		index = expr->val.binary.expr[1]->val.array.elem_list != NULL ?
			expr->val.binary.expr[1]->val.array.elem_list->list : NULL;
		count = 0;
		while (index != NULL) { count++; index = index->next; }
		index = expr->val.binary.expr[1]->val.array.elem_list->list;
	} else {
		index = expr->val.binary.expr[1];
		count = 1;
	}
	if ((int)count != contract->rank) {
		hir_fatal(hir_error_line,
			  N_TR("The number of indices does not match the __fast parameter rank."));
		return false;
	}
	for (axis = 0; axis < count; axis++) {
		int64_t lower;
		int64_t upper;
		const struct fast_extent *extent;
		extent = &contract->extent[axis];
		if (hir_fast_cond_depth == 0 &&
		    extent->kind == FAST_EXTENT_CONST &&
		    hir_fast_index_interval(index, &lower, &upper) &&
		    (lower < 0 || upper >= extent->constant)) {
			hir_fatal(hir_error_line,
				  N_TR("A __fast packed access is provably out of bounds."));
			return false;
		}
		if (count > 1) index = index->next;
	}
	return true;
}

/* Visit an AST expr. */
static bool
hir_visit_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	bool result;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);

	/* Visit by type. */
	switch (aexpr->type) {
	case AST_EXPR_TERM:
		result = hir_visit_term_expr(hexpr, aexpr);
		break;
	case AST_EXPR_LT:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_LT);
		break;
	case AST_EXPR_LTE:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_LTE);
		break;
	case AST_EXPR_GT:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_GT);
		break;
	case AST_EXPR_GTE:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_GTE);
		break;
	case AST_EXPR_EQ:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_EQ);
		break;
	case AST_EXPR_NEQ:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_NEQ);
		break;
	case AST_EXPR_PLUS:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_PLUS);
		break;
	case AST_EXPR_MINUS:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_MINUS);
		break;
	case AST_EXPR_MUL:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_MUL);
		break;
	case AST_EXPR_DIV:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_DIV);
		break;
	case AST_EXPR_MOD:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_MOD);
		break;
	case AST_EXPR_AND:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_AND);
		break;
	case AST_EXPR_OR:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_OR);
		break;
	case AST_EXPR_LAND:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_LAND);
		break;
	case AST_EXPR_LOR:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_LOR);
		break;
	case AST_EXPR_XOR:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_XOR);
		break;
	case AST_EXPR_SHL:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_SHL);
		break;
	case AST_EXPR_SHR:
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_SHR);
		break;
	case AST_EXPR_SUBSCR:
		if (hir_current_func_kind == NOCT_FUNC_FAST &&
		    !hir_fast_check_subscript(aexpr)) {
			result = false;
			break;
		}
		if (aexpr->val.binary.expr[1] != NULL &&
		    aexpr->val.binary.expr[1]->type == AST_EXPR_ARRAY) {
			result = hir_visit_fast_multi_subscr(hexpr, aexpr);
			break;
		}
		if (aexpr->val.binary.expr[0] != NULL &&
		    aexpr->val.binary.expr[0]->type == AST_EXPR_TERM &&
		    aexpr->val.binary.expr[0]->val.term.term != NULL &&
		    aexpr->val.binary.expr[0]->val.term.term->type == AST_TERM_SYMBOL &&
		    ast_is_accel_resource_symbol(
			aexpr->val.binary.expr[0]->val.term.term->val.symbol)) {
			if (hir_current_func_kind == NOCT_FUNC_NORMAL)
				hir_fatal(hir_error_line,
					  N_TR("Accelerator resources cannot be subscripted by host code."));
			else
				hir_fatal(hir_error_line,
					  N_TR("An accel var must be passed through a _ptr parameter before kernel access."));
			result = false;
			break;
		}
		result = hir_visit_binary_expr(hexpr, aexpr, HIR_EXPR_SUBSCR);
		if (result && hir_current_func_kind == NOCT_FUNC_FAST) {
			int index_type;
			index_type = hir_fast_infer_expr_type(
				(*hexpr)->val.binary.expr[1]);
			if (index_type != NOCT_VALUE_INT &&
			    index_type != NOCT_VALUE_LONG) {
				hir_fatal(hir_error_line,
					  N_TR("A __fast array index must be int or long."));
				result = false;
			}
		}
		break;
	case AST_EXPR_NEG:
		result = hir_visit_unary_expr(hexpr, aexpr, HIR_EXPR_NEG);
		break;
	case AST_EXPR_NOT:
		result = hir_visit_unary_expr(hexpr, aexpr, HIR_EXPR_NOT);
		break;
	case AST_EXPR_PAR:
		result = hir_visit_unary_expr(hexpr, aexpr, HIR_EXPR_PAR);
		break;
	case AST_EXPR_DOT:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Object and package member access is not allowed inside __fast func."));
			result = false;
			break;
		}
		result = hir_visit_dot_expr(hexpr, aexpr);
		break;
	case AST_EXPR_CALL:
	{
		const struct accel_op_desc *math_op;
		math_op = hir_ast_accel_math(aexpr);
		if (hir_ast_accel_float32_bits_call(aexpr) &&
		    hir_current_func_kind != NOCT_FUNC_GPU) {
			hir_fatal(hir_error_line,
				  N_TR("Accel.float32FromBits() is valid only inside __gpu func."));
			result = false;
			break;
		}
		if (math_op != NULL && hir_current_func_kind != NOCT_FUNC_GPU) {
			char msg[256];
			snprintf(msg, sizeof(msg),
				 N_TR("GPU math operation '%s' is valid only inside __gpu func."),
				 math_op->source_spelling);
			hir_fatal(hir_error_line, msg);
			result = false;
			break;
		}
		if (aexpr->val.call.func != NULL &&
		    aexpr->val.call.func->type == AST_EXPR_DOT &&
		    !(aexpr->val.call.func->val.dot.obj->type == AST_EXPR_TERM &&
		      aexpr->val.call.func->val.dot.obj->val.term.term->type == AST_TERM_SYMBOL &&
		      (strcmp(aexpr->val.call.func->val.dot.obj->val.term.term->val.symbol,
		              "Int") == 0 ||
		       strcmp(aexpr->val.call.func->val.dot.obj->val.term.term->val.symbol,
		              "Float") == 0) &&
		      strcmp(aexpr->val.call.func->val.dot.symbol, "from") == 0))
			result = hir_visit_thiscall_expr(hexpr, aexpr);
		else
			result = hir_visit_call_expr(hexpr, aexpr);
		break;
	}
	case AST_EXPR_ARRAY:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Array literals are not allowed inside __fast func."));
			result = false;
			break;
		}
		result = hir_visit_array_expr(hexpr, aexpr);
		break;
	case AST_EXPR_DICT:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Dictionary literals are not allowed inside __fast func."));
			result = false;
			break;
		}
		result = hir_visit_dict_expr(hexpr, aexpr);
		break;
	case AST_EXPR_FUNC:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Lambda expressions are not allowed inside __fast func."));
			result = false;
			break;
		}
		result = hir_visit_func_expr(hexpr, aexpr);
		break;
	case AST_EXPR_NEW:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Object construction is not allowed inside __fast func."));
			result = false;
			break;
		}
		result = hir_visit_new_expr(hexpr, aexpr);
		break;
	default:
		result = false;
		assert(UNIMPLEMENTED);
		break;
	}

	return result;
}

/* Visit an AST term expr. */
static bool
hir_visit_term_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_TERM);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_TERM;

	/* Visit a term. */
	if (!hir_visit_term(&e->val.term.term, aexpr->val.term.term)) {
		hir_free_expr(e);
		return false;
	}

	*hexpr = e;

	return true;
}

/* Visit an AST binary-op expr. */
static bool
hir_visit_binary_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr,
	int type)
{
	struct hir_expr *e;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = type;

	/* Visit the two expressions. */
	if (!hir_visit_expr(&e->val.binary.expr[0], aexpr->val.binary.expr[0])) {
		hir_free_expr(e);
		return false;
	}
	if (!hir_visit_expr(&e->val.binary.expr[1], aexpr->val.binary.expr[1])) {
		hir_free_expr(e);
		return false;
	}

	*hexpr = e;

	return true;
}

static struct hir_expr *
hir_fast_term_symbol(const char *symbol)
{
	struct hir_expr *expr;
	struct hir_term *term;

	expr = hir_malloc(sizeof(*expr));
	term = hir_malloc(sizeof(*term));
	if (expr == NULL || term == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(expr, 0, sizeof(*expr));
	memset(term, 0, sizeof(*term));
	expr->type = HIR_EXPR_TERM;
	expr->val.term.term = term;
	term->type = HIR_TERM_SYMBOL;
	term->val.symbol = hir_strdup(symbol);
	if (term->val.symbol == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	return expr;
}

static struct hir_expr *
hir_fast_extent_expr(const struct fast_extent *extent)
{
	struct hir_expr *expr;
	struct hir_term *term;

	if (extent->kind == FAST_EXTENT_PARAM) {
		if (hir_current_func_block == NULL || extent->param_index < 0 ||
		    (uint32_t)extent->param_index >=
			hir_current_func_block->val.func.param_count)
			return NULL;
		return hir_fast_term_symbol(
			hir_current_func_block->val.func.param_name[extent->param_index]);
	}
	expr = hir_malloc(sizeof(*expr));
	term = hir_malloc(sizeof(*term));
	if (expr == NULL || term == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(expr, 0, sizeof(*expr));
	memset(term, 0, sizeof(*term));
	expr->type = HIR_EXPR_TERM;
	expr->val.term.term = term;
	if (extent->constant <= INT_MAX) {
		term->type = HIR_TERM_INT;
		term->val.i = (int)extent->constant;
	} else {
		term->type = HIR_TERM_LONG;
		term->val.l = extent->constant;
	}
	return expr;
}

static struct hir_expr *
hir_fast_binary_expr(int type, struct hir_expr *left, struct hir_expr *right)
{
	struct hir_expr *expr;

	if (left == NULL || right == NULL)
		return NULL;
	expr = hir_malloc(sizeof(*expr));
	if (expr == NULL) {
		hir_out_of_memory();
		return NULL;
	}
	memset(expr, 0, sizeof(*expr));
	expr->type = type;
	expr->val.binary.expr[0] = left;
	expr->val.binary.expr[1] = right;
	return expr;
}

static bool
hir_fast_multi_index_proven(const struct ast_expr *array,
			    const struct fast_param_contract *contract)
{
	const struct ast_expr *index;
	int64_t elements;
	int axis;

	if (hir_fast_cond_depth != 0 || array == NULL ||
	    array->val.array.elem_list == NULL)
		return false;
	elements = 1;
	index = array->val.array.elem_list->list;
	for (axis = 0; axis < contract->rank; axis++) {
		int64_t lower;
		int64_t upper;
		const struct fast_extent *extent;
		extent = &contract->extent[axis];
		if (index == NULL || extent->kind != FAST_EXTENT_CONST ||
		    extent->constant > INT_MAX ||
		    elements > INT_MAX / extent->constant ||
		    !hir_fast_index_interval(index, &lower, &upper) ||
		    lower < 0 || upper >= extent->constant)
			return false;
		elements *= extent->constant;
		index = index->next;
	}
	return index == NULL;
}

static bool
hir_fast_lower_proven_multi_subscr(struct hir_expr **hexpr,
				   struct ast_expr *base,
				   struct ast_expr *array,
				   const struct fast_param_contract *contract)
{
	struct hir_expr *subscr;
	struct hir_expr *flat;
	struct ast_expr *index;
	int axis;

	subscr = hir_malloc(sizeof(*subscr));
	if (subscr == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(subscr, 0, sizeof(*subscr));
	subscr->type = HIR_EXPR_SUBSCR;
	if (!hir_visit_expr(&subscr->val.binary.expr[0], base))
		return false;
	index = array->val.array.elem_list->list;
	flat = NULL;
	if (!hir_visit_expr(&flat, index))
		return false;
	index = index->next;
	for (axis = 1; axis < contract->rank; axis++) {
		struct hir_expr *extent;
		struct hir_expr *next_index;
		extent = hir_fast_extent_expr(&contract->extent[axis]);
		next_index = NULL;
		if (!hir_visit_expr(&next_index, index))
			return false;
		flat = hir_fast_binary_expr(HIR_EXPR_MUL, flat, extent);
		flat = hir_fast_binary_expr(HIR_EXPR_PLUS, flat, next_index);
		if (flat == NULL)
			return false;
		index = index->next;
	}
	subscr->val.binary.expr[1] = flat;
	*hexpr = subscr;
	return true;
}

/* Lower a source multi-index to an internal checked row-major helper call. */
static bool
hir_visit_fast_multi_subscr(struct hir_expr **hexpr, struct ast_expr *aexpr)
{
	struct ast_expr *base;
	struct ast_expr *array;
	struct ast_expr *index;
	struct hir_expr *subscr;
	struct hir_expr *call;
	struct hir_expr *dot;
	const struct fast_param_contract *contract;
	const char *symbol;
	char helper[32];
	uint32_t param;
	uint32_t count;
	uint32_t axis;

	base = aexpr->val.binary.expr[0];
	array = aexpr->val.binary.expr[1];
	if (hir_current_func_kind != NOCT_FUNC_FAST) {
		hir_fatal(hir_error_line,
			  N_TR("Multi-dimensional subscripting is valid only inside __fast func."));
		return false;
	}
	if (base == NULL || base->type != AST_EXPR_TERM ||
	    base->val.term.term == NULL ||
	    base->val.term.term->type != AST_TERM_SYMBOL ||
	    hir_current_func_block == NULL) {
		hir_fatal(hir_error_line,
			  N_TR("A multi-dimensional subscript base must be a shaped __fast parameter."));
		return false;
	}
	symbol = base->val.term.term->val.symbol;
	for (param = 0; param < hir_current_func_block->val.func.param_count;
	     param++) {
		if (strcmp(symbol,
		    hir_current_func_block->val.func.param_name[param]) == 0)
			break;
	}
	if (param == hir_current_func_block->val.func.param_count) {
		hir_fatal(hir_error_line,
			  N_TR("A multi-dimensional subscript base must be a shaped __fast parameter."));
		return false;
	}
	contract = &hir_current_func_block->val.func.fast_signature->param[param];
	count = 0;
	index = array->val.array.elem_list != NULL ?
		array->val.array.elem_list->list : NULL;
	while (index != NULL) { count++; index = index->next; }
	if ((int)count != contract->rank) {
		hir_fatal(hir_error_line,
			  N_TR("The number of indices does not match the __fast parameter rank."));
		return false;
	}
	/* Exact constant shapes with statically bounded axes need no helper.
	 * Preserve the row-major expression so ABCE/SIMD can see the contiguous
	 * final axis. */
	if (hir_fast_multi_index_proven(array, contract))
		return hir_fast_lower_proven_multi_subscr(
			hexpr, base, array, contract);
	subscr = hir_malloc(sizeof(*subscr));
	call = hir_malloc(sizeof(*call));
	dot = hir_malloc(sizeof(*dot));
	if (subscr == NULL || call == NULL || dot == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(subscr, 0, sizeof(*subscr));
	memset(call, 0, sizeof(*call));
	memset(dot, 0, sizeof(*dot));
	subscr->type = HIR_EXPR_SUBSCR;
	call->type = HIR_EXPR_CALL;
	dot->type = HIR_EXPR_DOT;
	if (!hir_visit_expr(&subscr->val.binary.expr[0], base))
		return false;
	snprintf(helper, sizeof(helper), "index%u", count);
	dot->val.dot.obj = hir_fast_term_symbol("$Fast");
	dot->val.dot.symbol = hir_strdup(helper);
	if (dot->val.dot.obj == NULL || dot->val.dot.symbol == NULL)
		return false;
	call->val.call.func = dot;
	call->val.call.arg_count = count * 2;
	index = array->val.array.elem_list->list;
	for (axis = 0; axis < count; axis++) {
		if (!hir_visit_expr(&call->val.call.arg[axis * 2], index))
			return false;
		if (hir_fast_infer_expr_type(call->val.call.arg[axis * 2]) !=
			NOCT_VALUE_INT &&
		    hir_fast_infer_expr_type(call->val.call.arg[axis * 2]) !=
			NOCT_VALUE_LONG) {
			hir_fatal(hir_error_line,
				  N_TR("A __fast array index must be int or long."));
			return false;
		}
		call->val.call.arg[axis * 2 + 1] =
			hir_fast_extent_expr(&contract->extent[axis]);
		if (call->val.call.arg[axis * 2 + 1] == NULL)
			return false;
		index = index->next;
	}
	subscr->val.binary.expr[1] = call;
	*hexpr = subscr;
	return true;
}

/* Visit an AST unary-op expr. */
static bool
hir_visit_unary_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr,
	int type)
{
	struct hir_expr *e;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_NEG ||
	       aexpr->type == AST_EXPR_NOT ||
	       aexpr->type == AST_EXPR_PAR);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = type;

	/* Visit the expression. */
	if (!hir_visit_expr(&e->val.unary.expr, aexpr->val.unary.expr)) {
		hir_free_expr(e);
		return false;
	}

	*hexpr = e;

	return true;
}

/* Visit an AST dot expr. */
static bool
hir_visit_dot_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	const struct accel_op_desc *math_op;
	char msg[256];

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_DOT);

	math_op = hir_ast_accel_math_property(aexpr);
	if (hir_ast_accel_float32_bits_property(aexpr)) {
		hir_fatal(hir_error_line,
			  N_TR("Accel.float32FromBits must be called directly inside __gpu func."));
		return false;
	}
	if (math_op != NULL) {
		snprintf(msg, sizeof(msg),
			 N_TR("GPU math operation '%s' must be called directly inside __gpu func."),
			 math_op->source_spelling);
		hir_fatal(hir_error_line, msg);
		return false;
	}

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_DOT;

	/* Visit the expression. */
	if (!hir_visit_expr(&e->val.dot.obj, aexpr->val.dot.obj)) {
		hir_free_expr(e);
		return false;
	}

	/* Copy the member symbol. */
	e->val.dot.symbol = hir_strdup(aexpr->val.dot.symbol);
	if (e->val.dot.symbol == NULL) {
		hir_free_expr(e);
		return false;
	}

	*hexpr = e;

	return true;
}

static struct ast_func *
hir_find_fast_ast_func(const char *source_name)
{
	struct ast_func_list *list;
	struct ast_func *func;
	const char *resolved;

	resolved = ast_resolve_static_symbol(source_name);
	list = ast_get_func_list();
	func = list != NULL ? list->list : NULL;
	while (func != NULL) {
		if (func->func_kind == NOCT_FUNC_FAST &&
		    (strcmp(func->name, source_name) == 0 ||
		     strcmp(func->name, resolved) == 0))
			return func;
		func = func->next;
	}
	return NULL;
}

static bool
hir_build_ast_fast_signature(struct ast_func *func,
			     struct fast_signature *signature)
{
	const char *name[HIR_PARAM_SIZE];
	const char *annotation[HIR_PARAM_SIZE];
	int type[HIR_PARAM_SIZE];
	int packed[HIR_PARAM_SIZE];
	bool restricted[HIR_PARAM_SIZE];
	struct ast_param *param;
	uint32_t count;
	int return_type;
	int return_packed;
	bool return_restricted;
	char message[256];

	count = 0;
	param = func->param_list != NULL ? func->param_list->list : NULL;
	while (param != NULL && count < HIR_PARAM_SIZE) {
		char base[64];
		bool has_shape;
		name[count] = param->name;
		annotation[count] = param->type_name;
		if (param->type_name == NULL ||
		    !fast_annotation_base(param->type_name, base, sizeof(base),
					  &has_shape) ||
		    !hir_resolve_type_name(base, &type[count], &packed[count],
					   &restricted[count]))
			return false;
		count++;
		param = param->next;
	}
	if (param != NULL || func->return_type_name == NULL ||
	    !hir_resolve_type_name(func->return_type_name, &return_type,
				   &return_packed, &return_restricted))
		return false;
	return fast_signature_build(signature, NOCT_FUNC_FAST, count, name,
		annotation, type, packed, restricted, func->return_type_name,
		return_type, message, sizeof(message));
}

void
hir_fast_prototypes_reset(void)
{
	uint32_t i;

	for (i = 0; i < hir_fast_prototype_count; i++) {
		free(hir_fast_prototype[i].name);
		hir_fast_prototype[i].name = NULL;
	}
	hir_fast_prototype_count = 0;
}

bool
hir_fast_prototype_add(const char *name, int func_kind,
		       const struct fast_signature *signature)
{
	uint32_t i;
	struct hir_fast_prototype *prototype;

	for (i = 0; i < hir_fast_prototype_count; i++) {
		prototype = &hir_fast_prototype[i];
		if (strcmp(prototype->name, name) != 0)
			continue;
		if (prototype->func_kind == NOCT_FUNC_FAST ||
		    func_kind == NOCT_FUNC_FAST) {
			if (prototype->func_kind != func_kind ||
			    signature == NULL ||
			    !fast_signature_equal(&prototype->signature, signature)) {
				char message[256];
				snprintf(message, sizeof(message),
					 N_TR("Incompatible duplicate __fast prototype '%s'."),
					 name);
				hir_fatal(0, message);
				return false;
			}
		}
		return true;
	}
	if (hir_fast_prototype_count >= HIR_FAST_PROTOTYPE_MAX) {
		hir_fatal(0, N_TR("Too many function prototypes in the require graph."));
		return false;
	}
	prototype = &hir_fast_prototype[hir_fast_prototype_count];
	prototype->name = malloc(strlen(name) + 1);
	if (prototype->name == NULL) {
		hir_fatal(0, N_TR("Out of memory while collecting __fast prototypes."));
		return false;
	}
	strcpy(prototype->name, name);
	prototype->func_kind = func_kind;
	fast_signature_init(&prototype->signature);
	if (func_kind == NOCT_FUNC_FAST && signature != NULL)
		prototype->signature = *signature;
	hir_fast_prototype_count++;
	return true;
}

bool
hir_fast_prototypes_collect(void)
{
	struct ast_func_list *list;
	struct ast_func *func;

	list = ast_get_func_list();
	func = list != NULL ? list->list : NULL;
	while (func != NULL) {
		struct fast_signature signature;
		const struct fast_signature *signature_ptr;

		if (func->is_static) {
			func = func->next;
			continue;
		}
		signature_ptr = NULL;
		if (func->func_kind == NOCT_FUNC_FAST) {
			if (!hir_build_ast_fast_signature(func, &signature)) {
				hir_fatal(0, N_TR("Invalid __fast prototype in required module."));
				return false;
			}
			signature_ptr = &signature;
		}
		if (!hir_fast_prototype_add(func->name, func->func_kind,
					    signature_ptr))
			return false;
		func = func->next;
	}
	return true;
}

const struct fast_signature *
hir_fast_prototype_find(const char *name)
{
	uint32_t i;

	for (i = 0; i < hir_fast_prototype_count; i++) {
		if (hir_fast_prototype[i].func_kind == NOCT_FUNC_FAST &&
		    strcmp(hir_fast_prototype[i].name, name) == 0)
			return &hir_fast_prototype[i].signature;
	}
	return NULL;
}

static bool
hir_is_fast_intrinsic_name(const char *name)
{
	static const char *const names[] = {
		"min", "max", "abs", "sqrt", "sin", "cos", "tan",
		"asin", "acos", "atan", "atan2", "exp", "ln", "log2",
		"log10", "int", "long", "float", "double"
	};
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (strcmp(name, names[i]) == 0) return true;
	return false;
}

static int
hir_fast_packed_value_type(int packed_type)
{
	switch (packed_type) {
	case NOCT_PACKED_INT64:
	case NOCT_PACKED_UINT64:
		return NOCT_VALUE_LONG;
	case NOCT_PACKED_FLOAT32:
		return NOCT_VALUE_FLOAT;
	case NOCT_PACKED_FLOAT64:
		return NOCT_VALUE_DOUBLE;
	default:
		return NOCT_VALUE_INT;
	}
}

static int
hir_fast_infer_expr_type(const struct hir_expr *expr)
{
	const struct hir_term *term;
	struct hir_local *local;
	int left;
	int right;

	if (expr == NULL) return NOCT_FAST_RETURN_VOID;
	switch (expr->type) {
	case HIR_EXPR_TERM:
		term = expr->val.term.term;
		switch (term->type) {
		case HIR_TERM_INT: return NOCT_VALUE_INT;
		case HIR_TERM_LONG: return NOCT_VALUE_LONG;
		case HIR_TERM_FLOAT: return NOCT_VALUE_FLOAT;
		case HIR_TERM_DOUBLE: return NOCT_VALUE_DOUBLE;
		case HIR_TERM_SYMBOL:
			local = hir_current_func_block != NULL ?
				hir_current_func_block->val.func.local : NULL;
			while (local != NULL) {
				if (strcmp(local->symbol, term->val.symbol) == 0)
					return local->declared_type;
				local = local->next;
			}
			return -1;
		default: return -1;
		}
	case HIR_EXPR_SUBSCR:
		left = hir_fast_infer_expr_type(expr->val.binary.expr[0]);
		if (left != NOCT_VALUE_PACKED) return -1;
		if (expr->val.binary.expr[0]->type == HIR_EXPR_TERM) {
			const char *name;
			name = expr->val.binary.expr[0]->val.term.term->val.symbol;
			local = hir_current_func_block->val.func.local;
			while (local != NULL) {
				if (strcmp(local->symbol, name) == 0)
					return hir_fast_packed_value_type(
						local->declared_packed_type);
				local = local->next;
			}
		}
		return -1;
	case HIR_EXPR_NEG:
	case HIR_EXPR_PAR:
		return hir_fast_infer_expr_type(expr->val.unary.expr);
	case HIR_EXPR_NOT:
		return NOCT_VALUE_INT;
	case HIR_EXPR_LT: case HIR_EXPR_LTE: case HIR_EXPR_GT:
	case HIR_EXPR_GTE: case HIR_EXPR_EQ: case HIR_EXPR_NEQ:
	case HIR_EXPR_LAND: case HIR_EXPR_LOR:
		return NOCT_VALUE_INT;
	case HIR_EXPR_PLUS: case HIR_EXPR_MINUS: case HIR_EXPR_MUL:
	case HIR_EXPR_DIV: case HIR_EXPR_MOD: case HIR_EXPR_AND:
	case HIR_EXPR_OR: case HIR_EXPR_XOR: case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
		left = hir_fast_infer_expr_type(expr->val.binary.expr[0]);
		right = hir_fast_infer_expr_type(expr->val.binary.expr[1]);
		return left == right ? left : -1;
	case HIR_EXPR_CALL:
		if (expr->val.call.func != NULL &&
		    expr->val.call.func->type == HIR_EXPR_TERM &&
		    expr->val.call.func->val.term.term->type == HIR_TERM_SYMBOL) {
			struct ast_func *callee;
			const struct fast_signature *prototype;
			const char *annotation;
			callee = hir_find_fast_ast_func(
				expr->val.call.func->val.term.term->val.symbol);
			prototype = hir_fast_prototype_find(
				expr->val.call.func->val.term.term->val.symbol);
			annotation = callee != NULL ? callee->return_type_name : NULL;
			if (annotation != NULL) {
				if (strcmp(annotation, "void") == 0) return HIR_TYPE_VOID;
				if (strcmp(annotation, "int") == 0) return NOCT_VALUE_INT;
				if (strcmp(annotation, "long") == 0) return NOCT_VALUE_LONG;
				if (strcmp(annotation, "float") == 0) return NOCT_VALUE_FLOAT;
				if (strcmp(annotation, "double") == 0) return NOCT_VALUE_DOUBLE;
			}
			if (prototype != NULL)
				return prototype->return_type == NOCT_FAST_RETURN_VOID ?
					HIR_TYPE_VOID : prototype->return_type;
		}
		if (expr->val.call.func != NULL &&
		    expr->val.call.func->type == HIR_EXPR_DOT &&
		    expr->val.call.func->val.dot.obj->type == HIR_EXPR_TERM) {
			const char *pkg;
			const char *name;
			pkg = expr->val.call.func->val.dot.obj->val.term.term->val.symbol;
			name = expr->val.call.func->val.dot.symbol;
			if (strcmp(pkg, "$Fast") == 0) return NOCT_VALUE_LONG;
			if (strcmp(pkg, "$FastMath") == 0) {
				if (strcmp(name, "int") == 0) return NOCT_VALUE_INT;
				if (strcmp(name, "long") == 0) return NOCT_VALUE_LONG;
				if (strcmp(name, "float") == 0) return NOCT_VALUE_FLOAT;
				if (strcmp(name, "double") == 0) return NOCT_VALUE_DOUBLE;
				return expr->val.call.arg_count != 0 ?
					hir_fast_infer_expr_type(expr->val.call.arg[0]) : -1;
			}
		}
		return -1;
	default:
		return -1;
	}
}

static bool
hir_validate_fast_intrinsic(const char *name, struct hir_expr *call)
{
	uint32_t expected;
	int first;
	int second;
	bool primitive;
	bool floating;

	expected = strcmp(name, "min") == 0 || strcmp(name, "max") == 0 ||
		   strcmp(name, "atan2") == 0 ? 2 : 1;
	if (call->val.call.arg_count != expected) {
		hir_fatal(hir_error_line, N_TR("Wrong number of arguments for __fast intrinsic."));
		return false;
	}
	first = hir_fast_infer_expr_type(call->val.call.arg[0]);
	primitive = first == NOCT_VALUE_INT || first == NOCT_VALUE_LONG ||
		    first == NOCT_VALUE_FLOAT || first == NOCT_VALUE_DOUBLE;
	floating = first == NOCT_VALUE_FLOAT || first == NOCT_VALUE_DOUBLE;
	if (!primitive) {
		hir_fatal(hir_error_line, N_TR("A __fast intrinsic requires a statically typed numeric argument."));
		return false;
	}
	if (expected == 2) {
		second = hir_fast_infer_expr_type(call->val.call.arg[1]);
		if (second != first) {
			hir_fatal(hir_error_line, N_TR("A binary __fast intrinsic requires operands of the same type."));
			return false;
		}
	}
	if ((strcmp(name, "sqrt") == 0 || strcmp(name, "sin") == 0 ||
	     strcmp(name, "cos") == 0 || strcmp(name, "tan") == 0 ||
	     strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 ||
	     strcmp(name, "atan") == 0 || strcmp(name, "atan2") == 0 ||
	     strcmp(name, "exp") == 0 || strcmp(name, "ln") == 0 ||
	     strcmp(name, "log2") == 0 || strcmp(name, "log10") == 0) &&
	    !floating) {
		hir_fatal(hir_error_line, N_TR("A transcendental __fast intrinsic requires float or double."));
		return false;
	}
	return true;
}

/* Visit an AST call expr. */
static bool
hir_visit_call_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	struct ast_expr *arg;
	const char *fast_intrinsic_name;
	struct ast_func *fast_callee;
	const struct fast_signature *fast_prototype;
	struct fast_signature fast_local_signature;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_CALL);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_CALL;
	fast_intrinsic_name = NULL;
	fast_callee = NULL;
	fast_prototype = NULL;
	if (aexpr->val.call.func != NULL &&
	    aexpr->val.call.func->type == AST_EXPR_TERM &&
	    aexpr->val.call.func->val.term.term != NULL &&
	    aexpr->val.call.func->val.term.term->type == AST_TERM_SYMBOL)
	{
		const char *call_name;
		call_name = aexpr->val.call.func != NULL &&
			aexpr->val.call.func->type == AST_EXPR_TERM &&
			aexpr->val.call.func->val.term.term != NULL &&
			aexpr->val.call.func->val.term.term->type == AST_TERM_SYMBOL ?
			aexpr->val.call.func->val.term.term->val.symbol : NULL;
		if (call_name != NULL) {
			fast_callee = hir_find_fast_ast_func(call_name);
			if (fast_callee != NULL) {
				if (!hir_build_ast_fast_signature(fast_callee,
							  &fast_local_signature)) {
					hir_fatal(hir_error_line,
						  N_TR("Invalid direct __fast callee signature."));
					return false;
				}
				fast_prototype = &fast_local_signature;
			} else {
				fast_prototype = hir_fast_prototype_find(call_name);
			}
		}
	}
	if (hir_current_func_kind == NOCT_FUNC_FAST) {
		struct ast_expr *func_expr;
		const char *name;
		struct ast_func *callee;

		func_expr = aexpr->val.call.func;
		if (func_expr == NULL || func_expr->type != AST_EXPR_TERM ||
		    func_expr->val.term.term == NULL ||
		    func_expr->val.term.term->type != AST_TERM_SYMBOL) {
			hir_fatal(hir_error_line,
				  N_TR("A __fast func may call only a direct __fast function or intrinsic."));
			return false;
		}
		name = func_expr->val.term.term->val.symbol;
		callee = hir_find_fast_ast_func(name);
		fast_callee = callee;
		if (callee == NULL && fast_prototype == NULL &&
		    !hir_is_fast_intrinsic_name(name)) {
			char message[256];
			snprintf(message, sizeof(message),
				 N_TR("Call to non-fast function '%s' is not allowed inside __fast func."),
				 name);
			hir_fatal(hir_error_line, message);
			return false;
		}
		if (callee == NULL && fast_prototype == NULL)
			fast_intrinsic_name = name;
		if (callee != NULL && hir_current_func_block != NULL &&
		    strcmp(callee->name, hir_current_func_block->val.func.name) == 0) {
			hir_fatal(hir_error_line,
				  N_TR("Recursive __fast function calls are not supported."));
			return false;
		}
		if ((callee != NULL || fast_prototype != NULL) &&
		    hir_current_func_block != NULL) {
			if (hir_fast_edge_count >= HIR_FAST_EDGE_MAX) {
				hir_fatal(hir_error_line,
					  N_TR("Too many direct __fast call edges."));
				return false;
			}
			hir_fast_edge[hir_fast_edge_count].caller =
				hir_current_func_block->val.func.name;
			hir_fast_edge[hir_fast_edge_count].callee =
				callee != NULL ? callee->name : name;
			hir_fast_edge[hir_fast_edge_count].line = hir_error_line;
			hir_fast_edge_count++;
		}
	}

	/* Visit the func expression. */
	if (fast_intrinsic_name != NULL) {
		struct hir_expr *dot;
		dot = hir_malloc(sizeof(*dot));
		if (dot == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(dot, 0, sizeof(*dot));
		dot->type = HIR_EXPR_DOT;
		dot->val.dot.obj = hir_fast_term_symbol("$FastMath");
		dot->val.dot.symbol = hir_strdup(fast_intrinsic_name);
		if (dot->val.dot.obj == NULL || dot->val.dot.symbol == NULL)
			return false;
		e->val.call.func = dot;
	} else {
		hir_fast_direct_call_target =
			hir_current_func_kind == NOCT_FUNC_FAST ||
			fast_callee != NULL || fast_prototype != NULL;
		if (!hir_visit_expr(&e->val.call.func, aexpr->val.call.func)) {
			hir_fast_direct_call_target = false;
			hir_free_expr(e);
			return false;
		}
		hir_fast_direct_call_target = false;
	}

	/* Visit the argument expressions. */
	if (aexpr->val.call.arg_list != NULL) {
		arg = aexpr->val.call.arg_list->list;
		while (arg != NULL) {
			if (e->val.call.arg_count >= HIR_PARAM_SIZE) {
				hir_fatal(hir_error_line, N_TR("Exceeded the maximum argument count."));
				return false;
			}
			if (!hir_visit_expr(&e->val.call.arg[e->val.call.arg_count], arg)) {
				hir_free_expr(e);
				return false;
			}
			arg = arg->next;
			e->val.call.arg_count++;
		}
	}
	if (fast_intrinsic_name != NULL &&
	    !hir_validate_fast_intrinsic(fast_intrinsic_name, e)) {
		hir_free_expr(e);
		return false;
	}
	if (fast_prototype != NULL && hir_current_func_kind == NOCT_FUNC_FAST) {
		uint32_t index;
		if (fast_prototype->param_count != e->val.call.arg_count) {
			hir_fatal(hir_error_line,
				  N_TR("A direct __fast call has the wrong argument count."));
			return false;
		}
		for (index = 0; index < e->val.call.arg_count; index++) {
			const struct fast_param_contract *formal_contract;
			formal_contract = &fast_prototype->param[index];
			if (hir_fast_infer_expr_type(e->val.call.arg[index]) !=
			    formal_contract->value_type) {
				hir_fatal(hir_error_line,
					  N_TR("A direct __fast call argument does not match the callee type."));
				return false;
			}
			if (formal_contract->value_type == NOCT_VALUE_PACKED) {
				struct hir_local *actual;
				const char *actual_name;
				uint32_t actual_param;
				int axis;
				if (e->val.call.arg[index]->type != HIR_EXPR_TERM ||
				    e->val.call.arg[index]->val.term.term->type != HIR_TERM_SYMBOL) {
					hir_fatal(hir_error_line,
						  N_TR("A direct __fast call packed argument must be a parameter."));
					return false;
				}
				actual_name = e->val.call.arg[index]->val.term.term->val.symbol;
				actual = hir_current_func_block->val.func.local;
				while (actual != NULL &&
				       strcmp(actual->symbol, actual_name) != 0)
					actual = actual->next;
				if (actual == NULL ||
				    actual->declared_packed_type != formal_contract->packed_type) {
					hir_fatal(hir_error_line,
						  N_TR("A direct __fast call packed argument has the wrong element type."));
					return false;
				}
				for (actual_param = 0;
				     actual_param < hir_current_func_block->val.func.param_count;
				     actual_param++)
					if (strcmp(actual_name,
					    hir_current_func_block->val.func.param_name[actual_param]) == 0)
						break;
				if (actual_param < hir_current_func_block->val.func.param_count) {
					const struct fast_param_contract *actual_contract;
					actual_contract = &hir_current_func_block->val.func.fast_signature->param[actual_param];
					if (actual_contract->rank != formal_contract->rank) {
						hir_fatal(hir_error_line,
							  N_TR("A direct __fast call packed shape rank does not match."));
						return false;
					}
					for (axis = 0; axis < actual_contract->rank; axis++) {
						const struct fast_extent *actual_extent;
						const struct fast_extent *formal_extent;
						int mapped_param;
						actual_extent = &actual_contract->extent[axis];
						formal_extent = &formal_contract->extent[axis];
						mapped_param = -1;
						if (formal_extent->kind == FAST_EXTENT_PARAM &&
						    formal_extent->param_index >= 0 &&
						    (uint32_t)formal_extent->param_index < e->val.call.arg_count &&
						    e->val.call.arg[formal_extent->param_index]->type == HIR_EXPR_TERM) {
							const char *mapped_name;
							uint32_t m;
							mapped_name = e->val.call.arg[formal_extent->param_index]->val.term.term->val.symbol;
							for (m = 0; m < hir_current_func_block->val.func.param_count; m++)
								if (strcmp(mapped_name,
								    hir_current_func_block->val.func.param_name[m]) == 0) {
									mapped_param = (int)m;
									break;
								}
						}
						if (actual_extent->kind != formal_extent->kind ||
						    (actual_extent->kind == FAST_EXTENT_CONST &&
						     actual_extent->constant != formal_extent->constant) ||
						    (actual_extent->kind == FAST_EXTENT_PARAM &&
						     actual_extent->param_index != mapped_param)) {
							hir_fatal(hir_error_line,
								  N_TR("A direct __fast call packed shape does not match the callee view."));
							return false;
						}
					}
				}
			}
		}
	}
	if (fast_prototype != NULL) {
		uint32_t left_index;
		left_index = 0;
		while (left_index < e->val.call.arg_count) {
			if (fast_prototype->param[left_index].restricted &&
			    fast_prototype->param[left_index].value_type == NOCT_VALUE_PACKED &&
			    e->val.call.arg[left_index]->type == HIR_EXPR_TERM) {
				uint32_t right_index;
				right_index = left_index + 1;
				while (right_index < e->val.call.arg_count) {
					if (fast_prototype->param[right_index].restricted &&
					    fast_prototype->param[right_index].value_type == NOCT_VALUE_PACKED &&
					    e->val.call.arg[right_index]->type == HIR_EXPR_TERM &&
					    strcmp(e->val.call.arg[left_index]->val.term.term->val.symbol,
						   e->val.call.arg[right_index]->val.term.term->val.symbol) == 0) {
						hir_fatal(hir_error_line,
							  N_TR("A direct __fast call passes the same object to restricted parameters."));
						return false;
					}
					right_index++;
				}
			}
			left_index++;
		}
	}

	*hexpr = e;

	return true;
}

/* Visit an AST call expr. */
static bool
hir_visit_thiscall_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	struct ast_expr *dot;
	struct ast_expr *obj;
	struct ast_expr *arg;
	struct ast_arg_list *arg_list;
	const char *func;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_CALL);
	assert(aexpr->val.call.func != NULL);
	assert(aexpr->val.call.func->type == AST_EXPR_DOT);
	dot = aexpr->val.call.func;
	obj = dot->val.dot.obj;
	func = dot->val.dot.symbol;
	arg_list = aexpr->val.call.arg_list;

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_THISCALL;

	/* Visit the object expression. */
	if (!hir_visit_expr(&e->val.thiscall.obj, obj)) {
		hir_free_expr(e);
		return false;
	}

	/* Copy the function name. */
	e->val.thiscall.func = hir_strdup(func);
	if (e->val.thiscall.func == NULL) {
		hir_free_expr(e);
		return false;
	}

	/* Visit the argument expressions. */
	if (arg_list != NULL) {
		arg = arg_list->list;
		while (arg != NULL) {
			if (e->val.thiscall.arg_count >= HIR_PARAM_SIZE) {
				hir_fatal(hir_error_line, N_TR("Too many parameters."));
				hir_free_expr(e);
				return false;
			}
			if (!hir_visit_expr(&e->val.thiscall.arg[e->val.thiscall.arg_count], arg)) {
				hir_free_expr(e);
				return false;
			}
			arg = arg->next;
			e->val.thiscall.arg_count++;
		}
	}

	*hexpr = e;

	return true;
}

/* Visit an AST array expr. */
static bool
hir_visit_array_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	struct ast_expr *elem;
	size_t count, index;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_ARRAY);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_ARRAY;

	/* Count the elements and allocate a table. */
	count = 0;
	if (aexpr->val.array.elem_list != NULL) {
		elem = aexpr->val.array.elem_list->list;
		while (elem != NULL) {
			elem = elem->next;
			count++;
		}

		if (count > SIZE_MAX / sizeof(struct hir_exp *)) {
			hir_out_of_memory();
			hir_free_expr(e);
			return false;
		}

		e->val.array.elem_count = count;
		e->val.array.elem = hir_malloc((uint32_t)count * sizeof(struct hir_exp *));
		if (e->val.array.elem == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(e->val.array.elem, 0, (size_t)count * sizeof(struct hir_exp *));
	}

	/* Visit the argument expressions. */
	if (aexpr->val.array.elem_list != NULL) {
		elem = aexpr->val.array.elem_list->list;
		index = 0;
		while (elem != NULL) {
			if (!hir_visit_expr(&e->val.array.elem[index], elem)) {
				hir_free_expr(e);
				return false;
			}
			elem = elem->next;
			index++;
		}
	}

	*hexpr = e;

	return true;
}

/* Visit an AST dictionary expr. */
static bool
hir_visit_dict_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	struct ast_kv *kv;
	size_t count, index;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_DICT);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_DICT;

	/* Count the elements and allocate a table. */
	count = 0;
	if (aexpr->val.dict.kv_list != NULL) {
		kv = aexpr->val.dict.kv_list->list;
		while (kv != NULL) {
			kv = kv->next;
			count++;
		}

		if (count > SIZE_MAX / sizeof(char *)) {
			hir_out_of_memory();
			hir_free_expr(e);
			return false;
		}

		e->val.dict.kv_count = count;
		e->val.dict.key = hir_malloc(count * sizeof(char *));
		if (e->val.dict.key == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(e->val.dict.key, 0, count * sizeof(char *));

		e->val.dict.value = hir_malloc(count * sizeof(struct hir_exp *));
		if (e->val.dict.value == NULL) {
			hir_out_of_memory();
			return false;
		}
		memset(e->val.dict.value, 0, count * sizeof(struct hir_exp *));
	}

	/* Visit the argument expressions. */
	if (aexpr->val.dict.kv_list != NULL) {
		kv = aexpr->val.dict.kv_list->list;
		index = 0;
		while (kv != NULL) {
			/* Copy the key. */
			e->val.dict.key[index] = hir_strdup(kv->key);
			if (e->val.dict.key[index] == NULL) {
				hir_out_of_memory();
				return false;
			}

			/* Copy the value. */
			if (!hir_visit_expr(&e->val.dict.value[index], kv->value)) {
				hir_free_expr(e);
				return false;
			}

			kv = kv->next;
			index++;
		}
	}

	/* A class literal is frozen at creation. */
	if (aexpr->val.dict.is_class)
		return hir_wrap_freeze(hexpr, e);

	*hexpr = e;

	return true;
}

/* Visit an AST anonymous function expr. */
static bool
hir_visit_func_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;
	struct hir_term *t;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_FUNC);

	/* Here, we replace an anonymous function to a symbol. */

	/* Alocate an hterm. */
	t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(t, 0, sizeof(struct hir_term));
	t->type = HIR_TERM_SYMBOL;

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_TERM;
	e->val.term.term = t;

	/* Defer the analysis of the anonymous function. */
	if (!hir_defer_anon_func(aexpr, &t->val.symbol))
		return false;

	*hexpr = e;

	return true;
}

/* Visit an AST new expr. */
static bool
hir_visit_new_expr(
	struct hir_expr **hexpr,
	struct ast_expr *aexpr)
{
	struct hir_expr *e;

	assert(hexpr != NULL);
	assert(*hexpr == NULL);
	assert(aexpr != NULL);
	assert(aexpr->type == AST_EXPR_NEW);

	/* Allocate an hexpr. */
	e = hir_malloc(sizeof(struct hir_expr));
	if (e == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(e, 0, sizeof(struct hir_expr));
	e->type = HIR_EXPR_NEW;
	e->val.new_.cls = hir_strdup(aexpr->val.new_.cls);
	if (e->val.new_.cls == NULL) {
		hir_out_of_memory();
		return false;
	}

	/* Visit an expr. */
	if (aexpr->val.new_.init != NULL) {
		if (!hir_visit_expr(&e->val.new_.init, aexpr->val.new_.init)) {
			hir_free_expr(e);
			return false;
		}
	}

	/* An extend result is a frozen class template. */
	if (aexpr->val.new_.is_extend)
		return hir_wrap_freeze(hexpr, e);

	*hexpr = e;

	return true;
}

/* Visit an AST term. */
static bool
hir_visit_term(
	struct hir_term **hterm,
	struct ast_term *aterm)
{
	struct hir_term *t;

	/* Allocate an hterm. */
	t = hir_malloc(sizeof(struct hir_term));
	if (t == NULL) {
		hir_out_of_memory();
		return false;
	}
	memset(t, 0, sizeof(struct hir_term));

	/* Copy the value. */
	switch (aterm->type) {
	case AST_TERM_SYMBOL:
	{
		const char *resolved;

		/* Scope resolution (alpha-renaming + static TDZ). */
		if (!hir_scope_resolve(aterm->val.symbol, &resolved))
			return false;
		if (hir_current_func_kind == NOCT_FUNC_FAST && resolved == NULL &&
		    !hir_fast_direct_call_target) {
			char message[256];
			snprintf(message, sizeof(message),
				 N_TR("Global or unresolved symbol '%s' is not available inside __fast func."),
				 aterm->val.symbol);
			hir_fatal(hir_error_line, message);
			return false;
		}
		if (resolved == NULL && !hir_fast_direct_call_target &&
		    hir_find_fast_ast_func(aterm->val.symbol) != NULL) {
			hir_fatal(hir_error_line,
				  N_TR("A __fast function is not a first-class value and must be called directly."));
			return false;
		}
		t->type = HIR_TERM_SYMBOL;
		t->val.symbol = hir_strdup(resolved != NULL ? resolved :
					   ast_resolve_static_symbol(aterm->val.symbol));
		if (t->val.symbol == NULL) {
			hir_out_of_memory();
			return false;
		}
		break;
	}
	case AST_TERM_INT:
		t->type = HIR_TERM_INT;
		t->val.i = aterm->val.i;
		break;
	case AST_TERM_LONG:
		t->type = HIR_TERM_LONG;
		t->val.l = aterm->val.l;
		break;
	case AST_TERM_FLOAT:
		t->type = HIR_TERM_FLOAT;
		t->val.f = aterm->val.f;
		break;
	case AST_TERM_DOUBLE:
	{
		t->type = HIR_TERM_DOUBLE;
		t->val.lf = aterm->val.lf;
		break;
	}
	case AST_TERM_STRING:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("String values are not allowed inside __fast func."));
			return false;
		}
		t->type = HIR_TERM_STRING;
		t->val.s = hir_strdup(aterm->val.s);
		if (t->val.symbol == NULL) {
			hir_out_of_memory();
			return false;
		}
		break;
	case AST_TERM_EMPTY_ARRAY:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Array values are not allowed inside __fast func."));
			return false;
		}
		t->type = HIR_TERM_EMPTY_ARRAY;
		break;
	case AST_TERM_EMPTY_DICT:
		if (hir_current_func_kind == NOCT_FUNC_FAST) {
			hir_fatal(hir_error_line,
				  N_TR("Dictionary values are not allowed inside __fast func."));
			return false;
		}
		t->type = HIR_TERM_EMPTY_DICT;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	*hterm = t;

	return true;
}

/*
 * Type-annotation name table (docs/design/02-typing.md).  Sized
 * integer names degrade to their storage-class tag.  Returns the
 * NOCT_VALUE_* tag, or -1 for an unknown name (caller errors).
 */
static bool
hir_resolve_type_name(
	const char *name,
	int *tag,
	int *packed_type,
	bool *restricted)
{
	static const struct {
		const char *name;
		int tag;
		int packed_type;
		bool restricted;
	} tbl[] = {
		{ "void",            HIR_TYPE_VOID,       -1, false },
		{ "int",             NOCT_VALUE_INT,    -1, false },
		{ "long",            NOCT_VALUE_LONG,   -1, false },
		{ "float",           NOCT_VALUE_FLOAT,  -1, false },
		{ "double",          NOCT_VALUE_DOUBLE, -1, false },
		{ "string",          NOCT_VALUE_STRING, -1, false },
		{ "array",           NOCT_VALUE_ARRAY,  -1, false },
		{ "dict",            NOCT_VALUE_DICT,   -1, false },
		{ "packed",          NOCT_VALUE_PACKED, NOCT_PACKED_ANY, false },
		{ "func",            NOCT_VALUE_FUNC,   -1, false },
		{ "i8",              NOCT_VALUE_INT,    -1, false },
		{ "i16",             NOCT_VALUE_INT,    -1, false },
		{ "i32",             NOCT_VALUE_INT,    -1, false },
		{ "u8",              NOCT_VALUE_INT,    -1, false },
		{ "u16",             NOCT_VALUE_INT,    -1, false },
		{ "u32",             NOCT_VALUE_INT,    -1, false },
		{ "i64",             NOCT_VALUE_LONG,   -1, false },
		{ "u64",             NOCT_VALUE_LONG,   -1, false },
		{ "packedint8",      NOCT_VALUE_PACKED, NOCT_PACKED_INT8, false },
		{ "packeduint8",     NOCT_VALUE_PACKED, NOCT_PACKED_UINT8, false },
		{ "packedint16",     NOCT_VALUE_PACKED, NOCT_PACKED_INT16, false },
		{ "packeduint16",    NOCT_VALUE_PACKED, NOCT_PACKED_UINT16, false },
		{ "packedint32",     NOCT_VALUE_PACKED, NOCT_PACKED_INT32, false },
		{ "packeduint32",    NOCT_VALUE_PACKED, NOCT_PACKED_UINT32, false },
		{ "packedint64",     NOCT_VALUE_PACKED, NOCT_PACKED_INT64, false },
		{ "packeduint64",    NOCT_VALUE_PACKED, NOCT_PACKED_UINT64, false },
		{ "packedfloat",     NOCT_VALUE_PACKED, NOCT_PACKED_FLOAT32, false },
		{ "packeddouble",    NOCT_VALUE_PACKED, NOCT_PACKED_FLOAT64, false },
		{ "rpackedint8",     NOCT_VALUE_PACKED, NOCT_PACKED_INT8, true },
		{ "rpackeduint8",    NOCT_VALUE_PACKED, NOCT_PACKED_UINT8, true },
		{ "rpackedint16",    NOCT_VALUE_PACKED, NOCT_PACKED_INT16, true },
		{ "rpackeduint16",   NOCT_VALUE_PACKED, NOCT_PACKED_UINT16, true },
		{ "rpackedint32",    NOCT_VALUE_PACKED, NOCT_PACKED_INT32, true },
		{ "rpackeduint32",   NOCT_VALUE_PACKED, NOCT_PACKED_UINT32, true },
		{ "rpackedint64",    NOCT_VALUE_PACKED, NOCT_PACKED_INT64, true },
		{ "rpackeduint64",   NOCT_VALUE_PACKED, NOCT_PACKED_UINT64, true },
		{ "rpackedfloat",    NOCT_VALUE_PACKED, NOCT_PACKED_FLOAT32, true },
		{ "rpackeddouble",   NOCT_VALUE_PACKED, NOCT_PACKED_FLOAT64, true }
	};
	size_t i;

	for (i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
		if (strcmp(tbl[i].name, name) == 0) {
			*tag = tbl[i].tag;
			*packed_type = tbl[i].packed_type;
			*restricted = tbl[i].restricted;
			return true;
		}
	}
	return false;
}

/* Resolve an optional annotation; error on an unknown name. */
static bool
hir_check_type_annotation(
	int line,
	const char *type_name,
	int *tag,
	int *packed_type,
	bool *restricted)
{
	char msg[256];

	if (type_name == NULL) {
		*tag = -1;
		*packed_type = -1;
		*restricted = false;
		return true;
	}
	if (!hir_resolve_type_name(type_name, tag, packed_type, restricted)) {
		snprintf(msg, sizeof(msg),
			 N_TR("Unknown type name '%s'."), type_name);
		hir_fatal(line, msg);
		return false;
	}
	return true;
}

/* Copy parameter names and count parameters. */
static bool
hir_visit_param_list(
	struct hir_block *hfunc,
	struct ast_func *afunc)
{
	struct ast_param *param;
	uint32_t param_count;

	/* -1 = unannotated (0 would read as NOCT_VALUE_INT). */
	{
		uint32_t k;
		for (k = 0; k < HIR_PARAM_SIZE; k++) {
			hfunc->val.func.param_type[k] = -1;
			hfunc->val.func.param_packed_type[k] = -1;
			hfunc->val.func.param_restricted[k] = false;
			hfunc->val.func.param_accel_access[k] = ACCEL_ACCESS_NONE;
			hfunc->val.func.param_accel_transport[k] =
				ACCEL_TRANSPORT_SCALAR;
			hfunc->val.func.param_accel_effect[k] = ACCEL_EFFECT_NONE;
		}
	}

	/* If there is no param_list. */
	if (afunc->param_list == NULL) {
		hfunc->val.func.param_count = 0;
		return true;
	}

	/* Assume we have at lease one parameter. */
	assert(afunc->param_list->list != NULL);

	/* Do traverse. */
	param = afunc->param_list->list;
	param_count = 0;
	while (param != NULL) {
		const char *annotation;
		char base_annotation[64];
		char shaped_base[64];
		bool has_shape;
		size_t annotation_len;
		int accel_access;
		int accel_transport;
		if (param_count >= HIR_PARAM_SIZE) {
			hir_fatal(hir_error_line, N_TR("Too many parameters."));
			return false;
		}
		if (afunc->func_kind == NOCT_FUNC_GPU &&
		    strcmp(param->name, "Accel") == 0) {
			hir_fatal(0, N_TR("'Accel' is a reserved name inside __gpu func."));
			return false;
		}

		/* Copy names and count parameters. */
		hfunc->val.func.param_name[param_count] = hir_strdup(param->name);
		if (param->name == NULL) {
			hir_out_of_memory();
			return false;
		}

		/* Split accelerator direction suffixes before ordinary type
		   resolution.  These spellings are parameter-only contracts. */
		annotation = param->type_name;
		accel_access = ACCEL_ACCESS_NONE;
		accel_transport = ACCEL_TRANSPORT_SCALAR;
		if (annotation != NULL) {
			annotation_len = strlen(annotation);
			if (annotation_len > 3 &&
			    strcmp(annotation + annotation_len - 3, "_in") == 0) {
				accel_access = ACCEL_ACCESS_IN;
				accel_transport = ACCEL_TRANSPORT_COPY_IN;
				annotation_len -= 3;
			} else if (annotation_len > 4 &&
				   strcmp(annotation + annotation_len - 4, "_out") == 0) {
				accel_access = ACCEL_ACCESS_OUT;
				accel_transport = ACCEL_TRANSPORT_COPY_OUT;
				annotation_len -= 4;
			} else if (annotation_len > 4 &&
				   strcmp(annotation + annotation_len - 4, "_ptr") == 0) {
				accel_transport = ACCEL_TRANSPORT_DEVICE_PTR;
				annotation_len -= 4;
			}
			if (accel_transport != ACCEL_TRANSPORT_SCALAR) {
				if (afunc->func_kind == NOCT_FUNC_NORMAL ||
				    annotation_len >= sizeof(base_annotation)) {
					hir_fatal(0, N_TR("Accelerator direction types are valid only on accelerator function parameters."));
					return false;
				}
				memcpy(base_annotation, annotation, annotation_len);
				base_annotation[annotation_len] = '\0';
				annotation = base_annotation;
			}
		}
		if (afunc->func_kind == NOCT_FUNC_GPU &&
		    accel_transport != ACCEL_TRANSPORT_SCALAR &&
		    accel_transport != ACCEL_TRANSPORT_DEVICE_PTR) {
			hir_fatal(0, N_TR("GPU buffer parameters must use _ptr."));
			return false;
		}
		if (annotation != NULL && strchr(annotation, '(') != NULL) {
			if (!fast_annotation_base(annotation, shaped_base,
						 sizeof(shaped_base), &has_shape)) {
				hir_fatal(0, N_TR("Invalid shaped parameter type."));
				return false;
			}
			annotation = shaped_base;
		}

		/* Resolve the optional type annotation. */
		if (!hir_check_type_annotation(0,
					       annotation,
					       &hfunc->val.func.param_type[param_count],
					       &hfunc->val.func.param_packed_type[param_count],
					       &hfunc->val.func.param_restricted[param_count]))
			return false;
		hfunc->val.func.param_accel_access[param_count] = accel_access;
		hfunc->val.func.param_accel_transport[param_count] = accel_transport;
		if (accel_access == ACCEL_ACCESS_IN)
			hfunc->val.func.param_accel_effect[param_count] =
				ACCEL_EFFECT_READ;
		else if (accel_access == ACCEL_ACCESS_OUT)
			hfunc->val.func.param_accel_effect[param_count] =
				ACCEL_EFFECT_WRITE;
		else if (accel_transport == ACCEL_TRANSPORT_DEVICE_PTR)
			hfunc->val.func.param_accel_effect[param_count] =
				ACCEL_EFFECT_READ | ACCEL_EFFECT_WRITE;
		if (accel_transport != ACCEL_TRANSPORT_SCALAR &&
		    (!hfunc->val.func.param_restricted[param_count] ||
		     hfunc->val.func.param_packed_type[param_count] < 0)) {
			hir_fatal(0, N_TR("Accelerator buffer parameters must use a restricted packed element type."));
			return false;
		}
		if (accel_access != ACCEL_ACCESS_NONE &&
		    (!hfunc->val.func.param_restricted[param_count] ||
		     (hfunc->val.func.param_packed_type[param_count] != NOCT_PACKED_INT32 &&
		      hfunc->val.func.param_packed_type[param_count] != NOCT_PACKED_UINT32 &&
		      hfunc->val.func.param_packed_type[param_count] != NOCT_PACKED_FLOAT32))) {
			hir_fatal(0, N_TR("Accelerator packed parameters must be restricted int32, uint32, or float32 values."));
			return false;
		}
		/* Add to a local variable list. */
		if (!hir_add_local(hfunc, param->name))
			return false;
		hir_set_local_declaration(
			hfunc,
			param->name,
			HIR_LOCAL_DECL_PARAMETER,
			hfunc->val.func.param_type[param_count],
			hir_declared_scalar_kind(annotation),
			hfunc->val.func.param_packed_type[param_count],
			hfunc->val.func.param_packed_type[param_count] >= 0 ?
				HIR_LOCAL_STORAGE_LOGICAL_BUFFER :
				HIR_LOCAL_STORAGE_SCALAR,
			-1,
			NULL,
			NULL);
		param_count++;

		param = param->next;
	}
	hfunc->val.func.param_count = param_count;

	return true;
}

/* Defer an analysis of an anonymous function. */
static bool
hir_defer_anon_func(
	struct ast_expr *aexpr,
	char **symbol)
{
	char name[1024];

	snprintf(name, sizeof(name), "$anon.%s.%d", hir_file_name, hir_anon_func_count);
	*symbol = hir_strdup(name);
	if (*symbol == NULL) {
		hir_out_of_memory();
		return false;
	}

	if (hir_anon_func_count >= ANON_FUNC_SIZE) {
		hir_fatal(hir_error_line, N_TR("Too many anonymous functions."));
		return false;
	}

	hir_anon_func_name[hir_anon_func_count] = *symbol;
	hir_anon_func_param_list[hir_anon_func_count] = aexpr->val.func.param_list;
	hir_anon_func_stmt_list[hir_anon_func_count] = aexpr->val.func.stmt_list;
	hir_anon_func_count++;

	return true;
}

/* Free a block and its siblings. */
static void
hir_free_block(
	struct hir_block *b)
{
	uint32_t i;

	switch (b->type) {
	case HIR_BLOCK_FUNC:
		if (b->val.func.accel_kernel != NULL) {
			accel_kernel_free(b->val.func.accel_kernel);
			b->val.func.accel_kernel = NULL;
		}
		if (b->val.func.accel_program != NULL) {
			accel_program_free(b->val.func.accel_program);
			b->val.func.accel_program = NULL;
		}
		if (b->val.func.name != NULL) {
			hir_free(b->val.func.name);
			b->val.func.name = NULL;
		}
		for (i = 0; i < b->val.func.param_count; i++) {
			if (b->val.func.param_name[i] != NULL) {
				hir_free(b->val.func.param_name[i]);
				b->val.func.param_name[i] = NULL;
			}
		}
		if (b->val.func.inner != NULL) {
			hir_free_block(b->val.func.inner);
			b->val.func.inner = NULL;
		}
		if (b->val.func.local != NULL) {
			hir_free_local(b->val.func.local);
			b->val.func.local = NULL;
		}
		break;
	case HIR_BLOCK_BASIC:
		if (b->val.basic.stmt_list != NULL) {
			hir_free_stmt(b->val.basic.stmt_list);
			b->val.basic.stmt_list = NULL;
		}
		break;
	case HIR_BLOCK_IF:
		if (b->val.if_.cond != NULL) {
			hir_free_expr(b->val.if_.cond);
			b->val.if_.cond = NULL;
		}
		if (b->val.if_.inner != NULL) {
			hir_free_block(b->val.if_.inner);
			b->val.if_.inner = NULL;
		}
		if (b->val.if_.chain_next != NULL) {
			hir_free_block(b->val.if_.chain_next);
			b->val.if_.chain_next = NULL;
		}
		break;
	case HIR_BLOCK_FOR:
		if (b->val.for_.counter_symbol != NULL) {
			hir_free(b->val.for_.counter_symbol);
			b->val.for_.counter_symbol = NULL;
		}
		if (b->val.for_.key_symbol != NULL) {
			hir_free(b->val.for_.key_symbol);
			b->val.for_.key_symbol = NULL;
		}
		if (b->val.for_.value_symbol != NULL) {
			hir_free(b->val.for_.value_symbol);
			b->val.for_.value_symbol = NULL;
		}
		if (b->val.for_.collection != NULL) {
			hir_free_expr(b->val.for_.collection);
			b->val.for_.collection = NULL;
		}
		if (b->val.for_.inner != NULL) {
			hir_free_block(b->val.for_.inner);
			b->val.for_.inner = NULL;
		}
		break;
	case HIR_BLOCK_WHILE:
		if (b->val.while_.cond != NULL) {
			hir_free_expr(b->val.while_.cond);
			b->val.while_.cond = NULL;
		}
		if (b->val.while_.inner != NULL) {
			hir_free_block(b->val.while_.inner);
			b->val.while_.inner = NULL;
		}
		break;
	case HIR_BLOCK_END:
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	/* (b->succ == b) is a loop. */
	if (!b->stop && b->succ != NULL) {
		hir_free_block(b->succ);
		b->succ = NULL;
	}
}

/* Free an hstmt. */
static void
hir_free_stmt(
	struct hir_stmt *s)
{
	if (s->next != NULL) {
		hir_free_stmt(s->next);
		s->next = NULL;
	}
	if (s->lhs != NULL) {
		hir_free_expr(s->lhs);
		s->lhs = NULL;
	}
	if (s->rhs != NULL) {
		hir_free_expr(s->rhs);
		s->rhs = NULL;
	}
}

/* Free an hexpr. */
static void
hir_free_expr(
	struct hir_expr *e)
{
	uint32_t i;

	switch (e->type) {
	case HIR_EXPR_TERM:
		if (e->val.term.term != NULL) {
			hir_free_term(e->val.term.term);
			e->val.term.term = NULL;
		}
		break;
	/* Binary OPs  */
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
	case HIR_EXPR_LAND:
	case HIR_EXPR_LOR:
	case HIR_EXPR_XOR:
	case HIR_EXPR_SHL:
	case HIR_EXPR_SHR:
	case HIR_EXPR_SUBSCR:
		if (e->val.binary.expr[0] != NULL) {
			hir_free_expr(e->val.binary.expr[0]);
			e->val.binary.expr[0] = NULL;
		}
		if (e->val.binary.expr[1] != NULL) {
			hir_free_expr(e->val.binary.expr[1]);
			e->val.binary.expr[1] = NULL;
		}
		break;
	/* Unary OPs */
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
	case HIR_EXPR_PAR:
		if (e->val.unary.expr != NULL) {
			hir_free_expr(e->val.unary.expr);
			e->val.unary.expr = NULL;
		}
		break;
	case HIR_EXPR_DOT:
		if (e->val.dot.obj != NULL) {
			hir_free_expr(e->val.dot.obj);
			e->val.dot.obj = NULL;
		}
		if (e->val.dot.symbol != NULL) {
			hir_free(e->val.dot.symbol);
			e->val.dot.symbol = NULL;
		}
		break;
	case HIR_EXPR_CAPTURE:
		if (e->val.capture.expr != NULL) {
			hir_free_expr(e->val.capture.expr);
			e->val.capture.expr = NULL;
		}
		if (e->val.capture.symbol != NULL) {
			hir_free(e->val.capture.symbol);
			e->val.capture.symbol = NULL;
		}
		break;
	case HIR_EXPR_SELECT:
		if (e->val.select.cond != NULL) {
			hir_free_expr(e->val.select.cond);
			e->val.select.cond = NULL;
		}
		if (e->val.select.if_true != NULL) {
			hir_free_expr(e->val.select.if_true);
			e->val.select.if_true = NULL;
		}
		if (e->val.select.if_false != NULL) {
			hir_free_expr(e->val.select.if_false);
			e->val.select.if_false = NULL;
		}
		break;
	case HIR_EXPR_PMASKSTORE32:
		if (e->val.mask_store.base != NULL)
			hir_free_expr(e->val.mask_store.base);
		if (e->val.mask_store.offset != NULL)
			hir_free_expr(e->val.mask_store.offset);
		if (e->val.mask_store.mask != NULL)
			hir_free_expr(e->val.mask_store.mask);
		break;
	case HIR_EXPR_PGATHER32:
		if (e->val.gather.base != NULL)
			hir_free_expr(e->val.gather.base);
		if (e->val.gather.length != NULL)
			hir_free_expr(e->val.gather.length);
		if (e->val.gather.index != NULL)
			hir_free_expr(e->val.gather.index);
		if (e->val.gather.packed != NULL)
			hir_free_expr(e->val.gather.packed);
		break;
	case HIR_EXPR_CALL:
		if (e->val.call.func != NULL) {
			hir_free_expr(e->val.call.func);
			e->val.call.func = NULL;
		}
		for (i = 0; i < e->val.call.arg_count; i++) {
			if (e->val.call.arg[i] != NULL) {
				hir_free_expr(e->val.call.arg[i]);
				e->val.call.arg[i] = NULL;
			}
		}
		break;
	case HIR_EXPR_THISCALL:
		if (e->val.thiscall.obj != NULL) {
			hir_free_expr(e->val.thiscall.obj);
			e->val.thiscall.obj = NULL;
		}
		if (e->val.thiscall.func != NULL) {
			hir_free(e->val.thiscall.func);
			e->val.thiscall.func = NULL;
		}
		for (i = 0; i < e->val.thiscall.arg_count; i++) {
			if (e->val.thiscall.arg[i] != NULL) {
				hir_free_expr(e->val.thiscall.arg[i]);
				e->val.thiscall.arg[i] = NULL;
			}
		}
		break;
	case HIR_EXPR_ARRAY:
		for (i = 0; i < e->val.array.elem_count; i++) {
			if (e->val.array.elem[i] != NULL) {
				hir_free_expr(e->val.array.elem[i]);
				e->val.array.elem[i] = NULL;
			}
		}
		if (e->val.array.elem != NULL) {
			hir_free(e->val.array.elem);
			e->val.array.elem = NULL;
		}
		break;
	case HIR_EXPR_DICT:
		for (i = 0; i < e->val.dict.kv_count; i++) {
			if (e->val.dict.key[i] != NULL) {
				hir_free(e->val.dict.key[i]);
				e->val.dict.key[i] = NULL;
			}
			if (e->val.dict.value[i] != NULL) {
				hir_free_expr(e->val.dict.value[i]);
				e->val.dict.value[i] = NULL;
			}
		}
		if (e->val.dict.key != NULL) {
			hir_free(e->val.dict.key);
			e->val.dict.key = NULL;
		}
		if (e->val.dict.value != NULL) {
			hir_free(e->val.dict.value);
			e->val.dict.value = NULL;
		}
		break;
	case HIR_EXPR_NEW:
		hir_free(e->val.new_.cls);
		if (e->val.new_.init != NULL)
			hir_free_expr(e->val.new_.init);
		break;
	case HIR_EXPR_PBASE:
	case HIR_EXPR_PLEN:
		/* ABCE unary ops. */
		if (e->val.unary.expr != NULL) {
			hir_free_expr(e->val.unary.expr);
			e->val.unary.expr = NULL;
		}
		break;
	case HIR_EXPR_PCHECK:
	case HIR_EXPR_TYPEIS:
	case HIR_EXPR_PLOAD8U:
	case HIR_EXPR_PSTORE8:
	case HIR_EXPR_PLOAD8S:
	case HIR_EXPR_PLOAD16U:
	case HIR_EXPR_PLOAD16S:
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOAD64:
	case HIR_EXPR_PLOADF32:
	case HIR_EXPR_PSTORE16:
	case HIR_EXPR_PSTORE32:
	case HIR_EXPR_PSTORE64:
	case HIR_EXPR_PSTOREF32:
	case HIR_EXPR_VINDUCTF32:
		/* ABCE binary ops. */
		if (e->val.binary.expr[0] != NULL) {
			hir_free_expr(e->val.binary.expr[0]);
			e->val.binary.expr[0] = NULL;
		}
		if (e->val.binary.expr[1] != NULL) {
			hir_free_expr(e->val.binary.expr[1]);
			e->val.binary.expr[1] = NULL;
		}
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}
	hir_free(e);
}

/* Free an hterm. */
static void
hir_free_term(
	struct hir_term *t)
{
	switch (t->type) {
	case HIR_TERM_INT:
	case HIR_TERM_LONG:
	case HIR_TERM_FLOAT:
	case HIR_TERM_DOUBLE:
		break;
	case HIR_TERM_SYMBOL:
		if (t->val.symbol != NULL) {
			hir_free(t->val.symbol);
			t->val.symbol = NULL;
		}
		break;
	case HIR_TERM_STRING:
		if (t->val.s != NULL) {
			hir_free(t->val.s);
			t->val.s = NULL;
		}
		break;
	case HIR_TERM_EMPTY_ARRAY:
		break;
	case HIR_TERM_EMPTY_DICT:
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}
}

/* Free a local variable list. */
static void
hir_free_local(
	struct hir_local *local)
{
	if (local->next != NULL)
		hir_free_local(local->next);

	hir_free(local->symbol);
}

/* Set a fatal error message. */
static void
hir_fatal(
	int line,
	const char *msg)
{
	hir_error_line = line;

	/*
	 * Store the bare message: every consumer (rt_register_source,
	 * elback, cback) formats the file name and line by itself, so
	 * embedding them here used to print them twice.
	 */
	snprintf(hir_error_message,
		 sizeof(hir_error_message),
		 "%s",
		 msg);
}

/* Show out-of-memory error. */
void hir_out_of_memory(void)
{
	snprintf(hir_error_message,
		 sizeof(hir_error_message),
		 "%s: Out of memory error.",
		 hir_file_name != NULL ? hir_file_name : "");
}

/*
 * Allocator
 */

/* malloc() alternative. */
void *
hir_malloc(
	size_t size)
{
	return arena_alloc(&hir_arena, size);
}

/* strdup() alternative. */
char *
hir_strdup(const char *s)
{
	char *ret;
	size_t len;

	len = strlen(s) + 1;
	ret = arena_alloc(&hir_arena, len);
	if (ret == NULL)
		return NULL;

	memcpy(ret, s, len);
	return ret;
}

/* free() alternative. */
static void
hir_free(
	void *p)
{
	UNUSED_PARAMETER(p);

	/*
	 * In the current implementation, we don't free individual
	 * objects because we use an arena allocator.
	 */
}

/*
 * Debug Printer
 */

void
hir_dump_block(
	struct hir_block *block)
{
	hir_dump_block_at_level(block, 0);
}

static void
hir_dump_block_at_level(
	struct hir_block *block,
	int level)
{
	int i;

	while (block != NULL) {
		for (i = 0; i < level * 4; i++) printf(" ");
		printf("BLOCK(%d)", block->id);

		switch (block->type) {
		case HIR_BLOCK_FUNC:
		{
			printf(" FUNC parent=%d, succ=%d\n", block->parent->id, block->succ->id);

			if (block->val.func.inner != NULL) {
				for (i = 0; i < (level + 1) * 4; i++) printf(" ");
				printf("[INNER]\n");
				hir_dump_block_at_level(block->val.func.inner, level + 1);
			}
			break;
		}
		case HIR_BLOCK_BASIC:
		{
			struct hir_stmt *s;
			if (block->succ != NULL)
				printf(" BASIC parent=%d, succ=%d\n", block->parent->id, block->succ->id);
			else
				printf(" BASIC succ=NULL\n");
			s = block->val.basic.stmt_list;
			while (s != NULL) {
				//hir_dump_stmt(level + 1, s);
				s = s->next;
			}
			break;
		}
		case HIR_BLOCK_FOR:
		{
			if (block->succ != NULL)
				printf(" FOR parent=%d, succ=%d\n", block->parent->id, block->succ->id);
			else
				printf(" FOR succ=NULL\n");

			if (block->val.for_.inner != NULL) {
				for (i = 0; i < (level + 1) * 4; i++) printf(" ");
				printf("[INNER]\n");
				hir_dump_block_at_level(block->val.for_.inner, level + 1);
			}
			break;
		}
		case HIR_BLOCK_END:
		{
			printf(" END\n");
			break;
		}
		case HIR_BLOCK_IF:
			printf(" IF parent=%d, succ=%d, prev=%d, next=%d\n", block->parent->id, block->succ->id, block->val.if_.chain_prev->id, block->val.if_.chain_next->id);
			if (block->val.if_.inner != NULL) {
				for (i = 0; i < (level + 1) * 4; i++) printf(" ");
				printf("[INNER]\n");
				hir_dump_block_at_level(block->val.if_.inner, level + 1);
			}
			if (block->val.if_.chain_next != NULL) {
				for (i = 0; i < (level + 1) * 4; i++) printf(" ");
				printf("[CHAIN]\n");
				hir_dump_block_at_level(block->val.if_.chain_next, level + 1);
			}
			break;
		case HIR_BLOCK_WHILE:
			printf(" WHILE\n");
			break;
		default:
			printf(" SKIP %d\n", block->type);
			break;
		}

		if (block->succ != NULL) {
			if (block->stop) {
				for (i = 0; i < level * 4; i++) printf(" ");
				printf("[STOP %d]\n", block->succ->id);
				break;
			}
		}
		block = block->succ;
	}
}


/*
 * HIR Optimizer Driver
 *
 * The passes themselves live in hir_opt_abce.c / hir_opt_cse.c and
 * are compiled only when NOCT_ENABLE_OPTIMIZER is ON (which defines
 * NOCT_USE_OPTIMIZER).  See docs/design/01-abce.md and
 * docs/design/05-cse.md.
 */

/*
 * Run the HIR optimizer on one function.
 * -O/-O1 runs inline, weak typing and CSE.  -O2 adds ABCE and SIMD;
 * -O3 keeps those passes and permits fused arithmetic during LIR lowering.
 */
bool
hir_optimize_func(
	struct hir_block *func_block,
	int level,
	bool simd_info,
	bool accel_info)
{
	assert(func_block != NULL);
	assert(func_block->type == HIR_BLOCK_FUNC);
	if (getenv("NOCT_PARALLEL_DEBUG") != NULL &&
	    func_block->val.func.func_kind != NOCT_FUNC_GPU &&
	    func_block->val.func.func_kind != NOCT_FUNC_ACCEL &&
	    !hir_parallel_diagnose_func(func_block, stderr,
					"parallel-analysis", false))
		return false;
	if (func_block->val.func.func_kind == NOCT_FUNC_GPU)
		return true;
	if (func_block->val.func.func_kind == NOCT_FUNC_ACCEL) {
		if (accel_info &&
		    !hir_parallel_diagnose_func(func_block, stderr,
					"accel-analysis", true))
			return false;
		if (!hir_opt_accel_func(func_block, accel_info))
			return false;
		return true;
	}
#if defined(NOCT_USE_OPTIMIZER)
	if (level < 1)
		return true;

	if (!hir_opt_inline_func(func_block))
		return false;

	/*
	 * Seed ABCE/SIMD with function-wide scalar facts.  Conversion
	 * intrinsics and mixed numeric promotion are useful before loop
	 * versioning; the final pass below refreshes facts after CSE.
	 */
	if (!hir_opt_typed_func(func_block))
		return false;
	if (level >= 2) {
		if (!hir_opt_abce_func(func_block))
			return false;
		/* SIMD right after ABCE (it consumes the fast-loop marks). */
		if (!hir_opt_simd_func(func_block, simd_info))
			return false;
	}
	if (!hir_opt_cse_func(func_block))
		return false;
	/* After CSE: the lattice must see CAPTURE home assignments. */
	if (!hir_opt_typed_func(func_block))
		return false;

	return true;
#else
	UNUSED_PARAMETER(level);
	UNUSED_PARAMETER(simd_info);

	return true;
#endif
}
