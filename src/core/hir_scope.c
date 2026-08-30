/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR: scoping
 */

#include <noct/noct.h>
#include "hir.h"
#include "hir_private.h"
#include "ast.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/*
 * Scopes are pushed for the function body and for every if/elif/else,
 * for, and while body. Declarations are pre-scanned when the scope is
 * pushed so that a use before the declaration is a static error.
 */
struct hir_scope_decl {
	char *src_name;
	char *int_name;
	bool declared;
	bool is_let;
	struct hir_scope_decl *next;
};

struct hir_scope {
	struct hir_scope_decl *decls;
	struct hir_scope *parent;
};

static struct hir_scope *hir_scope_top;
static int hir_scope_seq;

/* Forward declarations. */
static struct hir_scope_decl *hir_scope_find_here(const struct hir_scope *scope, const char *src_name);
static struct hir_scope_decl *hir_scope_find(const char *src_name);
static bool hir_scope_add_decl(int line, const char *src_name, bool declared, bool is_let, struct hir_scope_decl **decl_ret);
static bool hir_scope_intern(struct hir_scope_decl *decl);

/*
 * Starts scope processing for one function.
 */
void
hir_scope_begin_func(
	void)
{
	hir_scope_top = NULL;
	hir_scope_seq = 0;
}

/*
 * Pushes a lexical block and pre-scans its declarations.
 */
bool
hir_scope_push(
	const struct ast_stmt_list *stmt_list)
{
	struct hir_scope *scope;
	const struct ast_stmt *stmt;
	const struct ast_expr *lhs;

	scope = hir_malloc(sizeof(struct hir_scope));
	if (scope == NULL) {
		hir_out_of_memory();
		return false;
	}

	memset(scope, 0, sizeof(struct hir_scope));
	scope->parent = hir_scope_top;
	hir_scope_top = scope;

	stmt = stmt_list != NULL ? stmt_list->list : NULL;

	/* Pre-register every declaration in the lexical block. */
	while (stmt != NULL) {
		if (stmt->type == AST_STMT_ASSIGN &&
		    (stmt->val.assign.is_var || stmt->val.assign.is_let)) {
			lhs = stmt->val.assign.lhs;
			if (lhs != NULL &&
			    lhs->type == AST_EXPR_TERM &&
			    lhs->val.term.term != NULL &&
			    lhs->val.term.term->type == AST_TERM_SYMBOL) {
				if (!hir_scope_add_decl(
					stmt->line,
					lhs->val.term.term->val.symbol,
					false,
					stmt->val.assign.is_let,
					NULL)) {
					hir_scope_top = scope->parent;
					return false;
				}
			}
		}
		stmt = stmt->next;
	}

	return true;
}

/*
 * Pops the current lexical block.
 */
void
hir_scope_pop(
	void)
{
	assert(hir_scope_top != NULL);

	hir_scope_top = hir_scope_top->parent;
}

/*
 * Registers an already-declared function parameter.
 */
bool
hir_scope_add_param(
	int line,
	const char *src_name)
{
	struct hir_scope_decl *decl;

	if (!hir_scope_add_decl(line, src_name, true, false, &decl))
		return false;

	decl->int_name = decl->src_name;

	return true;
}

/*
 * Begins a declaration in the current lexical block.
 */
bool
hir_scope_declare(
	int line,
	const char *src_name,
	bool is_let,
	const char **int_name,
	struct hir_scope_decl **decl_ret)
{
	struct hir_scope_decl *decl;
	char msg[256];

	assert(hir_scope_top != NULL);
	assert(src_name != NULL);
	assert(int_name != NULL);
	assert(decl_ret != NULL);

	decl = hir_scope_find_here(hir_scope_top, src_name);
	if (decl == NULL) {
		if (!hir_scope_add_decl(
			line,
			src_name,
			false,
			is_let,
			&decl)) {
			return false;
		}
	}

	if (decl->declared) {
		snprintf(
			msg,
			sizeof(msg),
			N_TR("Variable '%s' is already declared in this scope."),
			src_name);
		hir_error(line, msg);
		return false;
	}

	if (!hir_scope_intern(decl))
		return false;

	decl->is_let = is_let;
	*int_name = decl->int_name;
	*decl_ret = decl;

	return true;
}

/*
 * Makes a declaration visible and ends its TDZ.
 */
void
hir_scope_mark_declared(
	struct hir_scope_decl *decl)
{
	assert(decl != NULL);
	assert(!decl->declared);

	decl->declared = true;
}

/*
 * Resolves a source name to its internal name.
 */
bool
hir_scope_resolve(
	int line,
	const char *src_name,
	const char **int_name)
{
	struct hir_scope_decl *decl;
	char msg[256];

	assert(src_name != NULL);
	assert(int_name != NULL);

	*int_name = NULL;
	if (src_name[0] == '$')
		return true;

	decl = hir_scope_find(src_name);
	if (decl == NULL)
		return true;

	if (!decl->declared) {
		snprintf(
			msg,
			sizeof(msg),
			N_TR("Variable '%s' is used before its declaration."),
			src_name);
		hir_error(line, msg);
		return false;
	}

	*int_name = decl->int_name;

	return true;
}

/*
 * Rejects assignment to an immutable binding.
 */
bool
hir_scope_check_assign(
	int line,
	const char *int_name)
{
	struct hir_scope *scope;
	struct hir_scope_decl *decl;
	char msg[256];

	assert(int_name != NULL);

	scope = hir_scope_top;

	/* Search every active lexical scope. */
	while (scope != NULL) {
		decl = scope->decls;

		/* Search every declaration in this scope. */
		while (decl != NULL) {
			if (decl->int_name != NULL &&
			    strcmp(decl->int_name, int_name) == 0) {
				if (decl->is_let) {
					snprintf(
						msg,
						sizeof(msg),
						N_TR("Cannot assign to 'let' variable '%s'."),
						decl->src_name);
					hir_error(line, msg);
					return false;
				}
				return true;
			}
			decl = decl->next;
		}
		scope = scope->parent;
	}

	return true;
}

static struct hir_scope_decl *
hir_scope_find_here(
	const struct hir_scope *scope,
	const char *src_name)
{
	struct hir_scope_decl *decl;

	if (scope == NULL)
		return NULL;

	decl = scope->decls;

	/* Search declarations in the selected scope. */
	while (decl != NULL) {
		if (strcmp(decl->src_name, src_name) == 0)
			return decl;
		decl = decl->next;
	}

	return NULL;
}

static struct hir_scope_decl *
hir_scope_find(
	const char *src_name)
{
	struct hir_scope *scope;
	struct hir_scope_decl *decl;

	scope = hir_scope_top;

	/* Search from the innermost scope outward. */
	while (scope != NULL) {
		decl = hir_scope_find_here(scope, src_name);
		if (decl != NULL)
			return decl;
		scope = scope->parent;
	}

	return NULL;
}

static bool
hir_scope_add_decl(
	int line,
	const char *src_name,
	bool declared,
	bool is_let,
	struct hir_scope_decl **decl_ret)
{
	struct hir_scope_decl *decl;
	char msg[256];

	assert(hir_scope_top != NULL);
	assert(src_name != NULL);

	if (hir_scope_find_here(hir_scope_top, src_name) != NULL) {
		snprintf(
			msg,
			sizeof(msg),
			N_TR("Variable '%s' is already declared in this scope."),
			src_name);
		hir_error(line, msg);
		return false;
	}

	decl = hir_malloc(sizeof(struct hir_scope_decl));
	if (decl == NULL) {
		hir_out_of_memory();
		return false;
	}

	memset(decl, 0, sizeof(struct hir_scope_decl));
	decl->src_name = hir_strdup(src_name);
	if (decl->src_name == NULL) {
		hir_out_of_memory();
		return false;
	}
	decl->declared = declared;
	decl->is_let = is_let;
	decl->next = hir_scope_top->decls;
	hir_scope_top->decls = decl;

	if (decl_ret != NULL)
		*decl_ret = decl;

	return true;
}

static bool
hir_scope_intern(
	struct hir_scope_decl *decl)
{
	char suffix[32];
	size_t name_size;

	assert(hir_scope_top != NULL);
	assert(decl != NULL);

	/*
	 * Function-scope declarations retain their source names. Inner
	 * declarations are renamed unconditionally because HIR keeps a flat
	 * function-local list; the renamed binding must not remain visible
	 * after its lexical scope is popped.
	 */
	if (hir_scope_top->parent == NULL) {
		decl->int_name = decl->src_name;
		return true;
	}

	hir_scope_seq++;
	snprintf(suffix, sizeof(suffix), "$%d", hir_scope_seq);
	name_size = strlen(decl->src_name) + strlen(suffix) + 1;
	decl->int_name = hir_malloc(name_size);
	if (decl->int_name == NULL) {
		hir_out_of_memory();
		return false;
	}
	snprintf(decl->int_name, name_size, "%s%s", decl->src_name, suffix);

	return true;
}
