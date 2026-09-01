/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Detached accelerator HIR rewrite construction.
 */

#include "accel_rewrite.h"
#include "hir.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACCEL_REWRITE_MAX_BLOCKS	4096
#define ACCEL_REWRITE_NAME_SIZE	64

struct accel_rewrite_region {
	struct hir_block **link;
	struct hir_block *first;
	struct hir_block *last;
	struct hir_block *replacement;
	char args_name[ACCEL_REWRITE_NAME_SIZE];
	char session_name[ACCEL_REWRITE_NAME_SIZE];
};

struct accel_rewrite {
	struct hir_block *func_block;
	uint32_t region_count;
	struct accel_rewrite_region *region;
	bool locals_added;
	bool committed;
};

static enum accel_compile_status accel_rewrite_error(struct hir_block *func_block, const char *message);
static bool accel_rewrite_find_region(struct hir_block *func_block, const struct accel_program *program, struct hir_block *search_start, struct hir_block *search_prev, struct accel_rewrite_region *region, struct hir_block **next_search, struct hir_block **next_prev);
static bool accel_rewrite_validate_kernels(const struct accel_program *program, struct hir_block *first, struct hir_block *last);
static bool accel_rewrite_local_exists(const struct hir_block *func_block, const char *name);
static bool accel_rewrite_build_region(struct hir_block *func_block, const struct accel_program *program, uint32_t program_id, struct accel_rewrite_region *region);
static struct hir_block *accel_rewrite_new_block(struct hir_block *func_block, struct hir_block *last, int line);
static struct hir_stmt *accel_rewrite_new_statement(int line, struct hir_expr *lhs, struct hir_expr *rhs);
static struct hir_expr *accel_rewrite_new_symbol(const char *symbol);
static struct hir_expr *accel_rewrite_new_integer(int value);
static struct hir_expr *accel_rewrite_new_args_array(const struct hir_block *func_block, const struct accel_program *program);
static struct hir_expr *accel_rewrite_new_thiscall(const char *function_name, uint32_t arg_count, struct hir_expr *const argument[]);
static bool accel_rewrite_append_statement(struct hir_block *block, struct hir_stmt **tail, struct hir_stmt *statement);

/*
 * Stages detached ordinary HIR for every planned accelerator region.
 */
enum accel_compile_status
accel_rewrite_stage(
	struct hir_block *func_block,
	const struct accel_function_plan *plan,
	const struct accel_registry_reservation *reservation,
	struct accel_rewrite **result)
{
	struct accel_rewrite *rewrite;
	const struct accel_program *program;
	struct hir_block *search;
	struct hir_block *previous;
	struct hir_block *next_search;
	struct hir_block *next_previous;
	uint32_t program_id;
	uint32_t expected_local_count;
	uint32_t i;
	uint32_t j;

	if (result == NULL)
		return ACCEL_COMPILE_ERROR;

	*result = NULL;

	if (func_block == NULL)
		return accel_rewrite_error(func_block, N_TR("Invalid accelerator rewrite function."));
	if (func_block->type != HIR_BLOCK_FUNC)
		return accel_rewrite_error(func_block, N_TR("Invalid accelerator rewrite function."));
	if (plan == NULL)
		return accel_rewrite_error(func_block, N_TR("Missing accelerator function plan."));
	if (reservation == NULL)
		return accel_rewrite_error(func_block, N_TR("Missing accelerator registry reservation."));
	if (plan->region_count == 0)
		return accel_rewrite_error(func_block, N_TR("Empty accelerator function plan."));
	if (plan->region_count > UINT32_MAX / 2)
		return accel_rewrite_error(func_block, N_TR("Invalid accelerator local count."));

	expected_local_count = plan->region_count * 2;
	if (plan->generated_local_count != expected_local_count) {
		return accel_rewrite_error(
			func_block,
			N_TR("Invalid accelerator local count."));
	}

	rewrite = noct_calloc(1, sizeof(*rewrite));
	if (rewrite == NULL) {
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	rewrite->region = noct_calloc(
		plan->region_count,
		sizeof(*rewrite->region));
	if (rewrite->region == NULL) {
		noct_free(rewrite);
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	rewrite->func_block = func_block;
	rewrite->region_count = plan->region_count;
	search = func_block->val.func.inner;
	previous = NULL;

	/* Resolve every planned region against the unchanged top-level chain. */
	for (i = 0; i < plan->region_count; i++) {
		program = accel_function_plan_get_region(plan, i);
		if (program == NULL) {
			accel_rewrite_destroy(rewrite);
			return accel_rewrite_error(
				func_block,
				N_TR("Invalid accelerator region plan."));
		}

		if (!accel_rewrite_find_region(
			func_block,
			program,
			search,
			previous,
			&rewrite->region[i],
			&next_search,
			&next_previous)) {
			accel_rewrite_destroy(rewrite);
			return accel_rewrite_error(
				func_block,
				N_TR("Accelerator plan does not match live HIR."));
		}

		program_id = accel_registry_reservation_get_id(reservation, i);
		if (program_id == 0 || program_id > (uint32_t)INT_MAX) {
			accel_rewrite_destroy(rewrite);
			return accel_rewrite_error(
				func_block,
				N_TR("Invalid accelerator program identifier."));
		}

		if (!accel_rewrite_build_region(
			func_block,
			program,
			program_id,
			&rewrite->region[i])) {
			accel_rewrite_destroy(rewrite);
			return ACCEL_COMPILE_ERROR;
		}

		/* Reject duplicate compiler-owned names before adding any local. */
		for (j = 0; j < i; j++) {
			if (strcmp(
				rewrite->region[j].args_name,
				rewrite->region[i].args_name) == 0 ||
			    strcmp(
				rewrite->region[j].session_name,
				rewrite->region[i].session_name) == 0) {
				accel_rewrite_destroy(rewrite);
				return accel_rewrite_error(
					func_block,
					N_TR("Duplicate accelerator region index."));
			}
		}

		search = next_search;
		previous = next_previous;
	}

	*result = rewrite;

	return ACCEL_COMPILE_APPLIED;
}

/*
 * Adds all generated locals as the last fallible HIR mutation.
 */
bool
accel_rewrite_add_locals(
	struct accel_rewrite *rewrite)
{
	uint32_t i;

	if (rewrite == NULL)
		return false;
	if (rewrite->locals_added)
		return false;
	if (rewrite->committed)
		return false;

	/* Add roots in the same deterministic order used by the HIR shape. */
	for (i = 0; i < rewrite->region_count; i++) {
		if (!hir_add_local(
			rewrite->func_block,
			rewrite->region[i].args_name)) {
			return false;
		}

		if (!hir_add_local(
			rewrite->func_block,
			rewrite->region[i].session_name)) {
			return false;
		}
	}

	rewrite->locals_added = true;

	return true;
}

/*
 * Commits every staged link swap without allocation or failure.
 */
void
accel_rewrite_commit(
	struct accel_rewrite *rewrite)
{
	uint32_t i;

	assert(rewrite != NULL);
	assert(rewrite->locals_added);
	assert(!rewrite->committed);

	/* Replace every disjoint top-level region atomically under its guard. */
	for (i = 0; i < rewrite->region_count; i++) {
		assert(*rewrite->region[i].link == rewrite->region[i].first);
		*rewrite->region[i].link = rewrite->region[i].replacement;
	}

	rewrite->func_block->val.func.is_accel = false;
	rewrite->committed = true;
}

/*
 * Destroys rewrite staging metadata without freeing arena objects.
 */
void
accel_rewrite_destroy(
	struct accel_rewrite *rewrite)
{
	if (rewrite == NULL)
		return;

	noct_free(rewrite->region);
	noct_free(rewrite);
}

/* Report one deterministic hard rewrite failure. */
static enum accel_compile_status
accel_rewrite_error(
	struct hir_block *func_block,
	const char *message)
{
	int line;

	line = 0;
	if (func_block != NULL)
		line = func_block->line;

	hir_error(line, message);

	return ACCEL_COMPILE_ERROR;
}

/* Resolve one source-ordered maximal region and its predecessor link. */
static bool
accel_rewrite_find_region(
	struct hir_block *func_block,
	const struct accel_program *program,
	struct hir_block *search_start,
	struct hir_block *search_prev,
	struct accel_rewrite_region *region,
	struct hir_block **next_search,
	struct hir_block **next_prev)
{
	struct hir_block *block;
	struct hir_block *previous;
	uint32_t visited;
	bool found_last;

	assert(func_block != NULL);
	assert(program != NULL);
	assert(region != NULL);
	assert(next_search != NULL);
	assert(next_prev != NULL);

	block = search_start;
	previous = search_prev;
	visited = 0;

	/* Preserve top-level blocks before the next planned region. */
	while (block != NULL && block != func_block->succ) {
		if (visited++ >= ACCEL_REWRITE_MAX_BLOCKS)
			return false;
		if (block->parent != func_block)
			return false;
		if (block->id == program->first_block_id)
			break;

		previous = block;
		block = block->succ;
	}

	if (block == NULL || block == func_block->succ)
		return false;

	region->first = block;
	if (previous == NULL)
		region->link = &func_block->val.func.inner;
	else
		region->link = &previous->succ;

	found_last = false;

	/* Find the inclusive end of the planned top-level region. */
	while (block != NULL && block != func_block->succ) {
		if (visited++ >= ACCEL_REWRITE_MAX_BLOCKS)
			return false;
		if (block->parent != func_block)
			return false;
		if (block->id == program->last_block_id) {
			found_last = true;
			break;
		}

		block = block->succ;
	}

	if (!found_last)
		return false;
	if (!accel_rewrite_validate_kernels(
		program,
		region->first,
		block)) {
		return false;
	}

	region->last = block;
	*next_search = block->succ;
	*next_prev = block;

	return true;
}

/* Match every kernel loop ID against the selected live region. */
static bool
accel_rewrite_validate_kernels(
	const struct accel_program *program,
	struct hir_block *first,
	struct hir_block *last)
{
	struct hir_block *block;
	uint32_t kernel_index;
	uint32_t visited;

	if (program->kernel_count == 0)
		return false;

	kernel_index = 0;
	visited = 0;
	block = first;

	/* Match selected ranged loops in source order. */
	while (block != NULL) {
		if (visited++ >= ACCEL_REWRITE_MAX_BLOCKS)
			return false;

		if (block->type == HIR_BLOCK_FOR) {
			if (!block->val.for_.is_ranged)
				return false;
			if (kernel_index >= program->kernel_count)
				return false;
			if (program->kernel[kernel_index].loop_block_id != block->id)
				return false;
			kernel_index++;
		} else if (block->type == HIR_BLOCK_BASIC) {
			if (block->val.basic.stmt_list != NULL)
				return false;
		} else {
			return false;
		}

		if (block == last)
			break;
		block = block->succ;
	}

	if (block != last)
		return false;
	if (kernel_index != program->kernel_count)
		return false;

	return true;
}

/* Check whether a generated symbol would collide with a live local. */
static bool
accel_rewrite_local_exists(
	const struct hir_block *func_block,
	const char *name)
{
	const struct hir_local *local;

	local = func_block->val.func.local;

	/* Search every current local before staging compiler-owned names. */
	while (local != NULL) {
		if (strcmp(local->symbol, name) == 0)
			return true;
		local = local->next;
	}

	return false;
}

/* Build one detached begin/dispatch/finish basic block. */
static bool
accel_rewrite_build_region(
	struct hir_block *func_block,
	const struct accel_program *program,
	uint32_t program_id,
	struct accel_rewrite_region *region)
{
	struct hir_block *block;
	struct hir_stmt *statement;
	struct hir_stmt *tail;
	struct hir_expr *lhs;
	struct hir_expr *rhs;
	struct hir_expr *argument[2];
	uint32_t i;
	int length;

	length = snprintf(
		region->args_name,
		sizeof(region->args_name),
		"$accel.args.%lu",
		(unsigned long)program->region_index);
	if (length < 0 || (size_t)length >= sizeof(region->args_name)) {
		hir_error(program->source_line, N_TR("Accelerator local name is too long."));
		return false;
	}

	length = snprintf(
		region->session_name,
		sizeof(region->session_name),
		"$accel.session.%lu",
		(unsigned long)program->region_index);
	if (length < 0 || (size_t)length >= sizeof(region->session_name)) {
		hir_error(program->source_line, N_TR("Accelerator local name is too long."));
		return false;
	}

	if (accel_rewrite_local_exists(func_block, region->args_name) ||
	    accel_rewrite_local_exists(func_block, region->session_name)) {
		hir_error(program->source_line, N_TR("Accelerator local name collision."));
		return false;
	}

	block = accel_rewrite_new_block(
		func_block,
		region->last,
		region->first->line);
	if (block == NULL)
		return false;

	tail = NULL;
	lhs = accel_rewrite_new_symbol(region->args_name);
	if (lhs == NULL)
		return false;

	rhs = accel_rewrite_new_args_array(func_block, program);
	if (rhs == NULL)
		return false;

	statement = accel_rewrite_new_statement(block->line, lhs, rhs);
	if (statement == NULL)
		return false;

	if (!accel_rewrite_append_statement(block, &tail, statement))
		return false;

	lhs = accel_rewrite_new_symbol(region->session_name);
	if (lhs == NULL)
		return false;

	argument[0] = accel_rewrite_new_integer((int)program_id);
	if (argument[0] == NULL)
		return false;

	argument[1] = accel_rewrite_new_symbol(region->args_name);
	if (argument[1] == NULL)
		return false;

	rhs = accel_rewrite_new_thiscall("begin", 2, argument);
	if (rhs == NULL)
		return false;

	statement = accel_rewrite_new_statement(block->line, lhs, rhs);
	if (statement == NULL)
		return false;

	if (!accel_rewrite_append_statement(block, &tail, statement))
		return false;

	/* Emit every region kernel dispatch in deterministic source order. */
	for (i = 0; i < program->kernel_count; i++) {
		argument[0] = accel_rewrite_new_symbol(region->session_name);
		if (argument[0] == NULL)
			return false;

		argument[1] = accel_rewrite_new_integer((int)i);
		if (argument[1] == NULL)
			return false;

		rhs = accel_rewrite_new_thiscall("dispatch", 2, argument);
		if (rhs == NULL)
			return false;

		statement = accel_rewrite_new_statement(block->line, NULL, rhs);
		if (statement == NULL)
			return false;

		if (!accel_rewrite_append_statement(block, &tail, statement))
			return false;
	}

	argument[0] = accel_rewrite_new_symbol(region->session_name);
	if (argument[0] == NULL)
		return false;

	argument[1] = accel_rewrite_new_symbol(region->args_name);
	if (argument[1] == NULL)
		return false;

	rhs = accel_rewrite_new_thiscall("finish", 2, argument);
	if (rhs == NULL)
		return false;

	statement = accel_rewrite_new_statement(block->line, NULL, rhs);
	if (statement == NULL)
		return false;

	if (!accel_rewrite_append_statement(block, &tail, statement))
		return false;

	region->replacement = block;

	return true;
}

/* Allocate and zero one detached replacement basic block. */
static struct hir_block *
accel_rewrite_new_block(
	struct hir_block *func_block,
	struct hir_block *last,
	int line)
{
	struct hir_block *block;

	block = hir_malloc(sizeof(*block));
	if (block == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(block, 0, sizeof(*block));
	block->type = HIR_BLOCK_BASIC;
	block->line = line;
	block->parent = func_block;
	block->succ = last->succ;
	block->id = hir_next_block_id();

	return block;
}

/* Allocate and zero one detached statement. */
static struct hir_stmt *
accel_rewrite_new_statement(
	int line,
	struct hir_expr *lhs,
	struct hir_expr *rhs)
{
	struct hir_stmt *statement;

	assert(rhs != NULL);

	statement = hir_malloc(sizeof(*statement));
	if (statement == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(statement, 0, sizeof(*statement));
	statement->line = line;
	statement->lhs = lhs;
	statement->rhs = rhs;

	return statement;
}

/* Allocate a zeroed symbol term expression and its owned string. */
static struct hir_expr *
accel_rewrite_new_symbol(
	const char *symbol)
{
	struct hir_expr *expression;
	struct hir_term *term;

	assert(symbol != NULL);

	expression = hir_malloc(sizeof(*expression));
	if (expression == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(expression, 0, sizeof(*expression));
	expression->type = HIR_EXPR_TERM;

	term = hir_malloc(sizeof(*term));
	if (term == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(term, 0, sizeof(*term));
	term->type = HIR_TERM_SYMBOL;
	term->val.symbol = hir_strdup(symbol);
	if (term->val.symbol == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	expression->val.term.term = term;

	return expression;
}

/* Allocate a zeroed integer term expression. */
static struct hir_expr *
accel_rewrite_new_integer(
	int value)
{
	struct hir_expr *expression;
	struct hir_term *term;

	expression = hir_malloc(sizeof(*expression));
	if (expression == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(expression, 0, sizeof(*expression));
	expression->type = HIR_EXPR_TERM;

	term = hir_malloc(sizeof(*term));
	if (term == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(term, 0, sizeof(*term));
	term->type = HIR_TERM_INT;
	term->val.i = value;
	expression->val.term.term = term;

	return expression;
}

/* Build the ordered parameter and host-local runtime argument array. */
static struct hir_expr *
accel_rewrite_new_args_array(
	const struct hir_block *func_block,
	const struct accel_program *program)
{
	struct hir_expr *expression;
	struct hir_term *term;
	struct hir_expr **element;
	uint32_t element_count;
	uint32_t i;
	size_t size;

	element_count = func_block->val.func.param_count;

	/* Include every CPU-backed local at its planned dense argument slot. */
	for (i = 0; i < program->buffer_count; i++) {
		if (program->buffer[i].origin != ACCEL_BUFFER_LOCAL_HOST)
			continue;
		if (program->buffer[i].args_slot >= HIR_PARAM_SIZE)
			return NULL;
		if (program->buffer[i].args_slot >= element_count)
			element_count = program->buffer[i].args_slot + 1;
	}

	if (element_count == 0) {
		expression = hir_malloc(sizeof(*expression));
		if (expression == NULL) {
			hir_out_of_memory();
			return NULL;
		}

		memset(expression, 0, sizeof(*expression));
		expression->type = HIR_EXPR_TERM;

		term = hir_malloc(sizeof(*term));
		if (term == NULL) {
			hir_out_of_memory();
			return NULL;
		}

		memset(term, 0, sizeof(*term));
		term->type = HIR_TERM_EMPTY_ARRAY;
		expression->val.term.term = term;

		return expression;
	}

	if (element_count > HIR_PARAM_SIZE) {
		hir_error(func_block->line, N_TR("Invalid accelerator parameter count."));
		return NULL;
	}

	expression = hir_malloc(sizeof(*expression));
	if (expression == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(expression, 0, sizeof(*expression));
	expression->type = HIR_EXPR_ARRAY;
	expression->val.array.elem_count = element_count;

	size = sizeof(*element) * element_count;
	element = hir_malloc(size);
	if (element == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(element, 0, size);
	expression->val.array.elem = element;

	/* Preserve function parameters in declaration order. */
	for (i = 0; i < func_block->val.func.param_count; i++) {
		element[i] = accel_rewrite_new_symbol(
			func_block->val.func.param_name[i]);
		if (element[i] == NULL)
			return NULL;
	}

	/* Append only planned CPU-backed local buffers after the parameters. */
	for (i = 0; i < program->buffer_count; i++) {
		if (program->buffer[i].origin != ACCEL_BUFFER_LOCAL_HOST)
			continue;
		if (element[program->buffer[i].args_slot] != NULL)
			return NULL;
		element[program->buffer[i].args_slot] = accel_rewrite_new_symbol(
			program->buffer[i].name);
		if (element[program->buffer[i].args_slot] == NULL)
			return NULL;
	}

	/* Reject a malformed sparse runtime argument namespace. */
	for (i = 0; i < element_count; i++) {
		if (element[i] == NULL)
			return NULL;
	}

	return expression;
}

/* Build one ordinary private-package member call. */
static struct hir_expr *
accel_rewrite_new_thiscall(
	const char *function_name,
	uint32_t arg_count,
	struct hir_expr *const argument[])
{
	struct hir_expr *expression;
	uint32_t i;

	assert(function_name != NULL);
	assert(argument != NULL);

	if (arg_count >= HIR_PARAM_SIZE)
		return NULL;

	expression = hir_malloc(sizeof(*expression));
	if (expression == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	memset(expression, 0, sizeof(*expression));
	expression->type = HIR_EXPR_THISCALL;
	expression->val.thiscall.obj = accel_rewrite_new_symbol("__Accel");
	if (expression->val.thiscall.obj == NULL)
		return NULL;

	expression->val.thiscall.func = hir_strdup(function_name);
	if (expression->val.thiscall.func == NULL) {
		hir_out_of_memory();
		return NULL;
	}

	expression->val.thiscall.arg_count = arg_count;

	/* Copy the already detached arguments into the inline HIR array. */
	for (i = 0; i < arg_count; i++) {
		if (argument[i] == NULL)
			return NULL;
		expression->val.thiscall.arg[i] = argument[i];
	}

	return expression;
}

/* Append one detached statement to a replacement block. */
static bool
accel_rewrite_append_statement(
	struct hir_block *block,
	struct hir_stmt **tail,
	struct hir_stmt *statement)
{
	assert(block != NULL);
	assert(block->type == HIR_BLOCK_BASIC);
	assert(tail != NULL);
	assert(statement != NULL);

	if (*tail == NULL)
		block->val.basic.stmt_list = statement;
	else
		(*tail)->next = statement;

	*tail = statement;

	return true;
}
