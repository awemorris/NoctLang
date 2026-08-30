/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * HIR private functions
 */

#ifndef NOCT_HIR_PRIVATE_H
#define NOCT_HIR_PRIVATE_H

#include <noct/noct.h>

struct ast_stmt_list;
struct hir_block;
struct hir_scope_decl;

extern char *hir_file_name;

/*
 * Allocates memory from the HIR arena.
 */
void *
hir_malloc(
	size_t size);

/*
 * Duplicates a string in the HIR arena.
 */
char *
hir_strdup(
	const char *s);

/*
 * Reports an out-of-memory error while constructing HIR.
 */
void
hir_out_of_memory(
	void);

/*
 * Adds a local variable to a function block.
 */
bool
hir_add_local(
	struct hir_block *cur_block,
	const char *symbol);

/*
 * Allocates a fresh HIR block identifier.
 */
int
hir_next_block_id(
	void);

/*
 * Start scope processing for one function.
 */
void
hir_scope_begin_func(void);

/*
 * Push a lexical block and pre-scan its declarations.
 */
bool
hir_scope_push(
	const struct ast_stmt_list *stmt_list);

/*
 * Pop the current lexical block.
 */
void
hir_scope_pop(void);

/*
 * Register an already-declared function parameter.
 */
bool
hir_scope_add_param(
	int line,
	const char *src_name);

/*
 * Begin a declaration. The binding remains in its TDZ until
 * hir_scope_mark_declared() is called.
 */
bool
hir_scope_declare(
	int line,
	const char *src_name,
	bool is_let,
	const char **int_name,
	struct hir_scope_decl **decl);

/*
 * Make a declaration visible, ending its TDZ.
 */
void
hir_scope_mark_declared(
	struct hir_scope_decl *decl);

/*
 * Resolve a source name. *int_name is NULL when the name takes the
 * global/compiler-internal path.
 */
bool
hir_scope_resolve(
	int line,
	const char *src_name,
	const char **int_name);

/*
 * Reject assignment when int_name denotes a let binding.
 */
bool
hir_scope_check_assign(
	int line,
	const char *int_name);

#endif
