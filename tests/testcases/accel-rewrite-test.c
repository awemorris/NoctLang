/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Focused transactional accelerator rewrite tests.
 */

#include "accel_context.h"
#include "accel_lir_budget.h"
#include "accel_private.h"
#include "accel_program.h"
#include "accel_rewrite.h"
#include "accel_test_backend.h"
#include "ast.h"
#include "hir.h"
#include "hir_opt.h"
#include "lir.h"
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ACCEL_REWRITE_STRESS_COUNT	40

struct accel_rewrite_test_session {
	struct accel_live_session live;
	uint32_t *orphan_count;
};

static char *read_source(const char *directory, const char *name);
static bool build_case(const char *directory, const char *name, struct hir_block **func_block);
static void cleanup_case(void);
static struct hir_block *find_accel_function(void);
static uint32_t count_locals(const struct hir_block *func_block);
static bool create_context(struct rt_vm *vm, struct accel_test_backend_observer *observer, struct accel_context **context);
static void destroy_context(struct accel_context *context);
static bool expression_is_symbol(const struct hir_expr *expression, const char *symbol);
static bool expression_is_integer(const struct hir_expr *expression, int *value);
static bool expression_is_thiscall(const struct hir_expr *expression, const char *function_name);
static bool block_is_reachable(const struct hir_block *func_block, const struct hir_block *target);
static bool inspect_applied_shape(struct hir_block *func_block, struct hir_block *replacement, struct hir_block *after, uint32_t *program_id);
static bool stress_registry(struct accel_context *context, const struct accel_prepared_program *first, uint32_t first_id);
static bool run_applied_case(const char *directory);
static bool run_zero_parameter_case(const char *directory);
static bool run_compile_decline_case(const char *directory);
static bool run_backend_decline_case(const char *directory);
static bool run_budget_case(const char *directory);
static void orphan_test_session(struct accel_live_session *session);

/*
 * Runs the focused transactional accelerator rewrite tests.
 */
int
main(
	int argc,
	char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s CASE-DIRECTORY\n", argv[0]);
		return 2;
	}

	if (!run_applied_case(argv[1]))
		return 1;
	if (!run_zero_parameter_case(argv[1]))
		return 1;
	if (!run_compile_decline_case(argv[1]))
		return 1;
	if (!run_backend_decline_case(argv[1]))
		return 1;
	if (!run_budget_case(argv[1]))
		return 1;

	puts("PASS");

	return 0;
}

/* Read one owned NUL-terminated fixture from the requested directory. */
static char *
read_source(
	const char *directory,
	const char *name)
{
	char path[1024];
	char *source;
	FILE *file;
	long length;
	size_t read_size;
	int path_length;

	path_length = snprintf(path, sizeof(path), "%s/%s", directory, name);
	if (path_length < 0 || (size_t)path_length >= sizeof(path))
		return NULL;

	file = fopen(path, "rb");
	if (file == NULL)
		return NULL;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return NULL;
	}

	length = ftell(file);
	if (length < 0) {
		fclose(file);
		return NULL;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return NULL;
	}

	source = malloc((size_t)length + 1);
	if (source == NULL) {
		fclose(file);
		return NULL;
	}

	read_size = fread(source, 1, (size_t)length, file);
	if (read_size != (size_t)length) {
		free(source);
		fclose(file);
		return NULL;
	}

	source[length] = '\0';
	fclose(file);

	return source;
}

/* Parse, build, type, and return one fixture accelerator function. */
static bool
build_case(
	const char *directory,
	const char *name,
	struct hir_block **func_block)
{
	char *source;

	*func_block = NULL;
	source = read_source(directory, name);
	if (source == NULL) {
		fprintf(stderr, "failed to read %s\n", name);
		return false;
	}

	if (!ast_build(name, source)) {
		fprintf(
			stderr,
			"%s:%d: %s\n",
			name,
			ast_get_error_line(),
			ast_get_error_message());
		free(source);
		ast_cleanup();
		return false;
	}
	free(source);

	if (!hir_build()) {
		fprintf(
			stderr,
			"%s:%d: %s\n",
			name,
			hir_get_error_line(),
			hir_get_error_message());
		hir_cleanup();
		ast_cleanup();
		return false;
	}

	*func_block = find_accel_function();
	if (*func_block == NULL) {
		fprintf(stderr, "%s has no unique accelerator function\n", name);
		cleanup_case();
		return false;
	}

	if (!hir_opt_typed_func(*func_block)) {
		fprintf(stderr, "%s typed pass failed\n", name);
		cleanup_case();
		return false;
	}

	return true;
}

/* Release the current HIR before its source AST arena. */
static void
cleanup_case(
	void)
{
	hir_cleanup();
	ast_cleanup();
}

/* Find the single accelerator-hinted function in the current HIR table. */
static struct hir_block *
find_accel_function(
	void)
{
	struct hir_block *result;
	struct hir_block *func_block;
	uint32_t count;
	uint32_t i;

	result = NULL;
	count = hir_get_function_count();

	/* Find the one fixture function carrying the accelerator hint. */
	for (i = 0; i < count; i++) {
		func_block = hir_get_function(i);
		if (!func_block->val.func.is_accel)
			continue;
		if (result != NULL)
			return NULL;
		result = func_block;
	}

	return result;
}

/* Count current function locals using the same frame-list rule as LIR. */
static uint32_t
count_locals(
	const struct hir_block *func_block)
{
	const struct hir_local *local;
	uint32_t count;

	count = 0;
	local = func_block->val.func.local;

	/* Count every local frame entry. */
	while (local != NULL) {
		count++;
		local = local->next;
	}

	return count;
}

/* Create and attach one fake backend context to a zeroed test VM. */
static bool
create_context(
	struct rt_vm *vm,
	struct accel_test_backend_observer *observer,
	struct accel_context **context)
{
	struct accel_backend_ops ops;
	void *backend_state;

	memset(vm, 0, sizeof(*vm));
	*context = NULL;
	backend_state = NULL;
	if (!accel_test_backend_create(observer, &ops, &backend_state))
		return false;

	if (!accel_context_create(vm, &ops, backend_state, context)) {
		ops.destroy_backend_state(backend_state);
		return false;
	}

	accel_context_attach(*context);

	return true;
}

/* Detach and destroy one test context. */
static void
destroy_context(
	struct accel_context *context)
{
	if (context == NULL)
		return;

	accel_context_detach(context);
	accel_context_destroy(context);
}

/* Match one ordinary HIR symbol term expression. */
static bool
expression_is_symbol(
	const struct hir_expr *expression,
	const char *symbol)
{
	if (expression == NULL)
		return false;
	if (expression->type != HIR_EXPR_TERM)
		return false;
	if (expression->val.term.term == NULL)
		return false;
	if (expression->val.term.term->type != HIR_TERM_SYMBOL)
		return false;
	if (expression->val.term.term->val.symbol == NULL)
		return false;

	return strcmp(expression->val.term.term->val.symbol, symbol) == 0;
}

/* Read one ordinary HIR integer term expression. */
static bool
expression_is_integer(
	const struct hir_expr *expression,
	int *value)
{
	if (expression == NULL)
		return false;
	if (expression->type != HIR_EXPR_TERM)
		return false;
	if (expression->val.term.term == NULL)
		return false;
	if (expression->val.term.term->type != HIR_TERM_INT)
		return false;

	*value = expression->val.term.term->val.i;

	return true;
}

/* Match one compiler-generated private-package member call. */
static bool
expression_is_thiscall(
	const struct hir_expr *expression,
	const char *function_name)
{
	if (expression == NULL)
		return false;
	if (expression->type != HIR_EXPR_THISCALL)
		return false;
	if (!expression_is_symbol(expression->val.thiscall.obj, "__Accel"))
		return false;
	if (expression->val.thiscall.func == NULL)
		return false;

	return strcmp(expression->val.thiscall.func, function_name) == 0;
}

/* Check one block against the reachable top-level successor chain. */
static bool
block_is_reachable(
	const struct hir_block *func_block,
	const struct hir_block *target)
{
	const struct hir_block *block;
	uint32_t visited;

	visited = 0;
	block = func_block->val.func.inner;

	/* Search only blocks emitted on the fixture's top-level chain. */
	while (block != NULL && block != func_block->succ) {
		if (block == target)
			return true;
		if (visited++ > 1024)
			return false;
		if (block->stop)
			break;
		block = block->succ;
	}

	return false;
}

/* Validate the complete ordinary-HIR replacement for one two-kernel region. */
static bool
inspect_applied_shape(
	struct hir_block *func_block,
	struct hir_block *replacement,
	struct hir_block *after,
	uint32_t *program_id)
{
	struct hir_stmt *statement;
	struct hir_expr *expression;
	uint32_t i;
	int value;

	if (replacement == NULL || replacement->type != HIR_BLOCK_BASIC)
		return false;
	if (replacement->parent != func_block || replacement->succ != after)
		return false;
	if (replacement->stop || replacement->is_return_edge ||
	    replacement->is_break_edge || replacement->is_continue_edge)
		return false;
	if (replacement->addr != 0 || replacement->cont_addr != 0)
		return false;

	statement = replacement->val.basic.stmt_list;
	if (statement == NULL || statement->is_bare_return)
		return false;
	if (!expression_is_symbol(statement->lhs, "$accel.args.0"))
		return false;
	if (statement->rhs == NULL || statement->rhs->type != HIR_EXPR_ARRAY)
		return false;
	if (statement->rhs->val.array.elem_count != 3)
		return false;
	if (!expression_is_symbol(statement->rhs->val.array.elem[0], "source"))
		return false;
	if (!expression_is_symbol(statement->rhs->val.array.elem[1], "destination"))
		return false;
	if (!expression_is_symbol(statement->rhs->val.array.elem[2], "n"))
		return false;

	statement = statement->next;
	if (statement == NULL || statement->is_bare_return)
		return false;
	if (!expression_is_symbol(statement->lhs, "$accel.session.0"))
		return false;
	expression = statement->rhs;
	if (!expression_is_thiscall(expression, "begin"))
		return false;
	if (expression->val.thiscall.arg_count != 2)
		return false;
	if (!expression_is_integer(expression->val.thiscall.arg[0], &value))
		return false;
	if (value <= 0)
		return false;
	if (!expression_is_symbol(
		expression->val.thiscall.arg[1],
		"$accel.args.0")) {
		return false;
	}
	*program_id = (uint32_t)value;

	/* Verify deterministic zero-based dispatch indices. */
	for (i = 0; i < 2; i++) {
		statement = statement->next;
		if (statement == NULL || statement->lhs != NULL)
			return false;
		if (statement->is_bare_return)
			return false;
		expression = statement->rhs;
		if (!expression_is_thiscall(expression, "dispatch"))
			return false;
		if (expression->val.thiscall.arg_count != 2)
			return false;
		if (!expression_is_symbol(
			expression->val.thiscall.arg[0],
			"$accel.session.0")) {
			return false;
		}
		if (!expression_is_integer(expression->val.thiscall.arg[1], &value))
			return false;
		if (value != (int)i)
			return false;
	}

	statement = statement->next;
	if (statement == NULL || statement->lhs != NULL)
		return false;
	if (statement->is_bare_return)
		return false;
	expression = statement->rhs;
	if (!expression_is_thiscall(expression, "finish"))
		return false;
	if (expression->val.thiscall.arg_count != 2)
		return false;
	if (!expression_is_symbol(
		expression->val.thiscall.arg[0],
		"$accel.session.0")) {
		return false;
	}
	if (!expression_is_symbol(
		expression->val.thiscall.arg[1],
		"$accel.args.0")) {
		return false;
	}
	if (statement->next != NULL)
		return false;

	return true;
}

/* Force table growth, a cancelled hole, and a later non-reused ID. */
static bool
stress_registry(
	struct accel_context *context,
	const struct accel_prepared_program *first,
	uint32_t first_id)
{
	struct accel_prepared_program prepared[ACCEL_REWRITE_STRESS_COUNT];
	struct accel_prepared_program final_program[1];
	struct accel_registry_reservation *reservation;
	struct accel_registry_reservation *cancelled;
	struct accel_registry_reservation *final_reservation;
	struct accel_registry_commit_guard guard;
	const struct accel_prepared_program *borrowed;
	const struct accel_program *source_program;
	void *first_payload;
	uint32_t cancelled_id;
	uint32_t final_id;
	uint32_t prepared_count;
	uint32_t i;
	enum accel_compile_status status;

	memset(prepared, 0, sizeof(prepared));
	memset(final_program, 0, sizeof(final_program));
	memset(&guard, 0, sizeof(guard));
	reservation = NULL;
	cancelled = NULL;
	final_reservation = NULL;
	prepared_count = 0;
	first_payload = first->payload;
	source_program = accel_test_backend_get_program(first);
	if (source_program == NULL)
		return false;

	if (!accel_context_reserve_programs(
		context,
		ACCEL_REWRITE_STRESS_COUNT,
		&reservation)) {
		return false;
	}

	/* Prepare independent owned payloads for every no-fail publication slot. */
	for (i = 0; i < ACCEL_REWRITE_STRESS_COUNT; i++) {
		status = context->ops.prepare_program(
			context->backend_state,
			source_program,
			&prepared[i]);
		if (status != ACCEL_COMPILE_APPLIED)
			goto fail;
		prepared_count++;
	}

	if (!accel_context_lock_commit(context, reservation, &guard))
		goto fail;
	accel_context_publish_programs_locked(&guard, prepared);
	reservation = NULL;
	prepared_count = 0;
	accel_context_unlock_commit(&guard);

	borrowed = accel_context_lookup_program(context, first_id);
	if (borrowed != first || borrowed->payload != first_payload)
		return false;

	if (!accel_context_reserve_programs(context, 1, &cancelled))
		return false;
	cancelled_id = accel_registry_reservation_get_id(cancelled, 0);
	accel_context_cancel_reservation(context, cancelled);
	cancelled = NULL;
	if (accel_context_lookup_program(context, cancelled_id) != NULL)
		return false;

	if (!accel_context_reserve_programs(context, 1, &final_reservation))
		return false;
	final_id = accel_registry_reservation_get_id(final_reservation, 0);
	if (final_id <= cancelled_id)
		goto fail;

	status = context->ops.prepare_program(
		context->backend_state,
		source_program,
		&final_program[0]);
	if (status != ACCEL_COMPILE_APPLIED)
		goto fail;

	if (!accel_context_lock_commit(context, final_reservation, &guard))
		goto fail;
	accel_context_publish_programs_locked(&guard, final_program);
	final_reservation = NULL;
	accel_context_unlock_commit(&guard);

	if (accel_context_lookup_program(context, final_id) == NULL)
		return false;

	return true;

fail:
	if (guard.locked)
		accel_context_unlock_commit(&guard);
	if (reservation != NULL)
		accel_context_cancel_reservation(context, reservation);
	if (cancelled != NULL)
		accel_context_cancel_reservation(context, cancelled);
	if (final_reservation != NULL)
		accel_context_cancel_reservation(context, final_reservation);

	/* Release only fake programs not transferred into registry ownership. */
	for (i = 0; i < prepared_count; i++) {
		context->ops.destroy_prepared_program(
			context->backend_state,
			&prepared[i]);
	}
	context->ops.destroy_prepared_program(
		context->backend_state,
		&final_program[0]);

	return false;
}

/* Apply, inspect, and stress one complete two-kernel transaction. */
static bool
run_applied_case(
	const char *directory)
{
	struct rt_vm vm;
	struct accel_test_backend_observer observer;
	struct accel_context *context;
	struct hir_block *func_block;
	struct hir_block *block;
	struct hir_block *previous;
	struct hir_block *first_loop;
	struct hir_block *last_loop;
	struct hir_block *after;
	struct hir_block *replacement;
	const struct accel_prepared_program *prepared;
	const struct accel_program *program;
	struct accel_rewrite_test_session session[2];
	uint32_t local_count;
	uint32_t program_id;
	uint32_t orphan_count;
	bool success;

	context = NULL;
	func_block = NULL;
	orphan_count = 0;
	memset(session, 0, sizeof(session));
	if (!build_case(directory, "two-kernel.noct", &func_block))
		return false;

	previous = NULL;
	first_loop = NULL;
	last_loop = NULL;
	block = func_block->val.func.inner;

	/* Locate the original maximal top-level loop group. */
	while (block != NULL && block != func_block->succ) {
		if (block->type == HIR_BLOCK_FOR) {
			if (first_loop == NULL)
				first_loop = block;
			last_loop = block;
		} else if (first_loop == NULL) {
			previous = block;
		}
		if (block->stop)
			break;
		block = block->succ;
	}

	if (first_loop == NULL || last_loop == NULL) {
		fprintf(stderr, "two-kernel fixture has no loop group\n");
		cleanup_case();
		return false;
	}
	after = last_loop->succ;
	local_count = count_locals(func_block);

	if (!create_context(&vm, &observer, &context)) {
		cleanup_case();
		return false;
	}

	success = vm.accel_optimize_func(
		func_block,
		vm.accel_optimize_userdata);
	if (!success) {
		fprintf(
			stderr,
			"applied callback failed: %s\n",
			hir_get_error_message());
		destroy_context(context);
		cleanup_case();
		return false;
	}

	if (func_block->val.func.is_accel) {
		fprintf(stderr, "applied hint was not consumed\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}
	if (count_locals(func_block) != local_count + 2) {
		fprintf(stderr, "generated locals were not added exactly once\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	if (previous == NULL)
		replacement = func_block->val.func.inner;
	else
		replacement = previous->succ;

	if (!inspect_applied_shape(
		func_block,
		replacement,
		after,
		&program_id)) {
		fprintf(stderr, "generated HIR shape is incorrect\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}
	if (block_is_reachable(func_block, first_loop) ||
	    block_is_reachable(func_block, last_loop)) {
		fprintf(stderr, "original accelerator loop remains reachable\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	prepared = accel_context_lookup_program(context, program_id);
	if (prepared == NULL) {
		fprintf(stderr, "published accelerator program is missing\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}
	program = accel_test_backend_get_program(prepared);
	if (program == NULL || program->kernel_count != 2) {
		fprintf(stderr, "fake backend did not retain the two-kernel plan\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	if (!stress_registry(context, prepared, program_id)) {
		fprintf(stderr, "registry stability test failed\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	session[0].live.orphan_locked = orphan_test_session;
	session[0].orphan_count = &orphan_count;
	session[1].live.orphan_locked = orphan_test_session;
	session[1].orphan_count = &orphan_count;
	accel_context_state_lock(context);
	accel_context_link_session_locked(context, &session[0].live);
	accel_context_link_session_locked(context, &session[1].live);
	accel_context_unlink_session_locked(context, &session[0].live);
	session[0].live.orphan_locked(&session[0].live);
	accel_context_state_unlock(context);

	accel_context_detach(context);
	cleanup_case();
	if (strcmp(program->function_name, "transform") != 0) {
		fprintf(stderr, "prepared program retained HIR arena storage\n");
		accel_context_destroy(context);
		return false;
	}

	accel_context_destroy(context);
	if (orphan_count != 2) {
		fprintf(stderr, "live sessions were not orphaned exactly once\n");
		return false;
	}
	if (observer.prepare_count != observer.destroy_program_count) {
		fprintf(stderr, "prepared payload ownership was unbalanced\n");
		return false;
	}
	if (observer.destroy_state_count != 1) {
		fprintf(stderr, "fake backend state was not destroyed once\n");
		return false;
	}

	return true;
}

/* Record one state-locked session orphan callback. */
static void
orphan_test_session(
	struct accel_live_session *session)
{
	struct accel_rewrite_test_session *test_session;

	test_session = (struct accel_rewrite_test_session *)session;
	if (test_session->orphan_count == NULL)
		return;

	(*test_session->orphan_count)++;
}

/* Apply a zero-parameter region and require the empty-array term shape. */
static bool
run_zero_parameter_case(
	const char *directory)
{
	struct rt_vm vm;
	struct accel_test_backend_observer observer;
	struct accel_context *context;
	struct accel_function_plan *plan;
	struct accel_prepared_program prepared[1];
	struct accel_registry_reservation *reservation;
	struct accel_registry_commit_guard guard;
	struct accel_rewrite *rewrite;
	struct hir_block *func_block;
	struct hir_block *block;
	struct hir_stmt *statement;
	struct accel_program *owned_program;
	struct accel_ir_kernel *ir;
	struct accel_kernel_plan kernel;
	const struct accel_program *program;
	enum accel_compile_status status;
	uint32_t serialized_tmpvar_size;
	uint32_t ignored;

	context = NULL;
	plan = NULL;
	reservation = NULL;
	rewrite = NULL;
	owned_program = NULL;
	ir = NULL;
	func_block = NULL;
	serialized_tmpvar_size = 0;
	memset(prepared, 0, sizeof(prepared));
	memset(&guard, 0, sizeof(guard));
	memset(&kernel, 0, sizeof(kernel));
	if (!build_case(directory, "zero-parameter.noct", &func_block))
		return false;
	if (!create_context(&vm, &observer, &context)) {
		cleanup_case();
		return false;
	}

	block = func_block->val.func.inner;

	/* Find the fixture's one ranged loop for a minimal owned test plan. */
	while (block != NULL && block != func_block->succ) {
		if (block->type == HIR_BLOCK_FOR)
			break;
		block = block->succ;
	}
	if (block == NULL || block == func_block->succ)
		goto fail;

	owned_program = accel_program_create(
		"zero-parameter.noct",
		"transform",
		block->line,
		0,
		0,
		block->id,
		block->id);
	if (owned_program == NULL)
		goto fail;

	ir = accel_ir_kernel_create("kernel0", block->line, block->id, 0, 0);
	if (ir == NULL)
		goto fail;

	kernel.kernel_index = 0;
	kernel.source_line = block->line;
	kernel.loop_block_id = block->id;
	kernel.ir = ir;
	if (!accel_program_add_kernel(owned_program, &kernel, &ignored))
		goto fail;
	ir = NULL;

	plan = accel_function_plan_create();
	if (plan == NULL)
		goto fail;
	if (!accel_function_plan_add_region(plan, owned_program))
		goto fail;
	owned_program = NULL;

	program = accel_function_plan_get_region(plan, 0);
	if (program == NULL)
		goto fail;

	status = context->ops.prepare_program(
		context->backend_state,
		program,
		&prepared[0]);
	if (status != ACCEL_COMPILE_APPLIED)
		goto fail;
	if (!accel_context_reserve_programs(context, 1, &reservation))
		goto fail;

	status = accel_lir_budget_check(
		func_block,
		plan,
		&serialized_tmpvar_size);
	if (status != ACCEL_COMPILE_APPLIED)
		goto fail;
	status = accel_rewrite_stage(
		func_block,
		plan,
		reservation,
		&rewrite);
	if (status != ACCEL_COMPILE_APPLIED)
		goto fail;
	if (!accel_context_lock_commit(context, reservation, &guard))
		goto fail;
	if (!accel_rewrite_add_locals(rewrite))
		goto fail;
	accel_context_publish_programs_locked(&guard, prepared);
	reservation = NULL;
	accel_rewrite_commit(rewrite);
	accel_context_unlock_commit(&guard);

	block = func_block->val.func.inner;

	/* Skip the retained CPU prefix before the replacement block. */
	while (block != NULL && block->type != HIR_BLOCK_BASIC)
		block = block->succ;

	/* Find the generated args assignment among retained basic blocks. */
	while (block != NULL && block->val.basic.stmt_list != NULL) {
		if (expression_is_symbol(
			block->val.basic.stmt_list->lhs,
			"$accel.args.0")) {
			break;
		}
		block = block->succ;
	}
	if (block == NULL)
		goto fail;

	statement = block->val.basic.stmt_list;
	if (statement == NULL || statement->rhs == NULL ||
	    statement->rhs->type != HIR_EXPR_TERM ||
	    statement->rhs->val.term.term == NULL ||
	    statement->rhs->val.term.term->type != HIR_TERM_EMPTY_ARRAY) {
		fprintf(stderr, "zero-parameter rewrite did not use empty-array term\n");
		goto fail;
	}

	accel_rewrite_destroy(rewrite);
	accel_function_plan_destroy(plan);
	destroy_context(context);
	cleanup_case();

	return true;

fail:
	if (guard.locked)
		accel_context_unlock_commit(&guard);
	if (reservation != NULL)
		accel_context_cancel_reservation(context, reservation);
	context->ops.destroy_prepared_program(
		context->backend_state,
		&prepared[0]);
	accel_ir_kernel_destroy(ir);
	accel_program_destroy(owned_program);
	accel_rewrite_destroy(rewrite);
	accel_function_plan_destroy(plan);
	destroy_context(context);
	cleanup_case();
	fprintf(stderr, "zero-parameter rewrite transaction failed\n");

	return false;
}

/* Preserve HIR exactly when target-neutral eligibility declines. */
static bool
run_compile_decline_case(
	const char *directory)
{
	struct rt_vm vm;
	struct accel_test_backend_observer observer;
	struct accel_context *context;
	struct hir_block *func_block;
	struct hir_block *inner;
	struct hir_local *local;
	bool success;

	context = NULL;
	func_block = NULL;
	if (!build_case(directory, "declined.noct", &func_block))
		return false;
	if (!create_context(&vm, &observer, &context)) {
		cleanup_case();
		return false;
	}

	inner = func_block->val.func.inner;
	local = func_block->val.func.local;
	success = vm.accel_optimize_func(
		func_block,
		vm.accel_optimize_userdata);
	if (!success || !func_block->val.func.is_accel ||
	    func_block->val.func.inner != inner ||
	    func_block->val.func.local != local) {
		fprintf(stderr, "compile decline mutated live HIR\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}
	if (observer.prepare_count != 0) {
		fprintf(stderr, "backend ran after target-neutral decline\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	destroy_context(context);
	cleanup_case();

	return true;
}

/* Preserve HIR and clean ownership when the selected backend declines. */
static bool
run_backend_decline_case(
	const char *directory)
{
	struct rt_vm vm;
	struct accel_test_backend_observer observer;
	struct accel_context *context;
	struct hir_block *func_block;
	struct hir_block *inner;
	struct hir_local *local;
	bool success;

	context = NULL;
	func_block = NULL;
	if (!build_case(directory, "two-kernel.noct", &func_block))
		return false;
	if (!create_context(&vm, &observer, &context)) {
		cleanup_case();
		return false;
	}

	observer.prepare_status = ACCEL_COMPILE_DECLINED;
	inner = func_block->val.func.inner;
	local = func_block->val.func.local;
	success = vm.accel_optimize_func(
		func_block,
		vm.accel_optimize_userdata);
	if (!success || !func_block->val.func.is_accel ||
	    func_block->val.func.inner != inner ||
	    func_block->val.func.local != local) {
		fprintf(stderr, "backend decline mutated live HIR\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}
	if (observer.prepare_count != 1 || observer.destroy_program_count != 0) {
		fprintf(stderr, "backend decline ownership is incorrect\n");
		destroy_context(context);
		cleanup_case();
		return false;
	}

	destroy_context(context);
	cleanup_case();

	return true;
}

/* Decline a rewrite at the slot boundary while the original CPU HIR builds. */
static bool
run_budget_case(
	const char *directory)
{
	struct accel_function_plan *plan;
	struct hir_block *func_block;
	struct lir_func *lir_func;
	char name[64];
	enum accel_compile_status status;
	uint32_t serialized_tmpvar_size;
	uint32_t local_count;
	int length;

	plan = NULL;
	func_block = NULL;
	lir_func = NULL;
	if (!build_case(directory, "budget.noct", &func_block))
		return false;

	status = accel_compile_func(func_block, &plan);
	if (status != ACCEL_COMPILE_APPLIED || plan == NULL) {
		fprintf(stderr, "budget fixture did not produce a plan\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	local_count = count_locals(func_block);

	/* Raise only the final frame base, leaving the empty CPU loop simple. */
	while (local_count < 121) {
		length = snprintf(
			name,
			sizeof(name),
			"$budget.%lu",
			(unsigned long)local_count);
		if (length < 0 || (size_t)length >= sizeof(name)) {
			accel_function_plan_destroy(plan);
			cleanup_case();
			return false;
		}

		if (!hir_add_local(func_block, name)) {
			accel_function_plan_destroy(plan);
			cleanup_case();
			return false;
		}
		local_count++;
	}

	serialized_tmpvar_size = 0;
	status = accel_lir_budget_check(
		func_block,
		plan,
		&serialized_tmpvar_size);
	if (status != ACCEL_COMPILE_DECLINED || serialized_tmpvar_size != 0) {
		fprintf(stderr, "slot-boundary rewrite did not decline\n");
		accel_function_plan_destroy(plan);
		cleanup_case();
		return false;
	}
	if (!func_block->val.func.is_accel) {
		fprintf(stderr, "budget preflight consumed the hint\n");
		accel_function_plan_destroy(plan);
		cleanup_case();
		return false;
	}

	lir_set_optimize_level(1);
	if (!lir_build(func_block, &lir_func)) {
		fprintf(stderr, "CPU fallback exceeded the LIR slot budget\n");
		accel_function_plan_destroy(plan);
		cleanup_case();
		return false;
	}
	if (lir_func->tmpvar_size > LIR_TMPVAR_MAX) {
		fprintf(stderr, "CPU fallback serialized too many slots\n");
		lir_cleanup(lir_func);
		accel_function_plan_destroy(plan);
		cleanup_case();
		return false;
	}

	lir_cleanup(lir_func);
	accel_function_plan_destroy(plan);
	cleanup_case();

	return true;
}
