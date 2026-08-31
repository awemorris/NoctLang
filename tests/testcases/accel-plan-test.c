/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Focused target-neutral accelerator plan tests.
 */

#include "accel_private.h"
#include "accel_program.h"
#include "ast.h"
#include "hir.h"
#include "hir_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_source(const char *directory, const char *name);
static struct hir_block *find_accel_function(void);
static bool run_applied_case(const char *directory);
static bool run_local_host_case(const char *directory);
static bool run_declined_case(const char *directory, const char *name);
static bool run_invalid_ir_case(void);
static bool build_case(const char *directory, const char *name, struct hir_block **func_block);
static void cleanup_case(void);

/*
 * Runs the focused accelerator plan tests.
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
	if (!run_local_host_case(argv[1]))
		return 1;
	if (!run_declined_case(argv[1], "declined.noct"))
		return 1;
	if (!run_declined_case(argv[1], "local-alias.noct"))
		return 1;
	if (!run_declined_case(argv[1], "f32-neg.noct"))
		return 1;
	if (!run_invalid_ir_case())
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

/* Compile and validate a two-kernel int/float accelerator candidate. */
static bool
run_applied_case(
	const char *directory)
{
	struct accel_function_plan *plan;
	struct accel_program *clone;
	struct hir_block *func_block;
	const struct accel_program *program;
	int64_t scalar_value[ACCEL_MAX_SCALAR_BINDINGS];
	int64_t trip;
	int64_t required_end;
	char error[160];
	enum accel_compile_status status;
	uint32_t i;
	bool valid;

	plan = NULL;
	func_block = NULL;
	if (!build_case(directory, "doall.noct", &func_block))
		return false;

	status = accel_compile_func(func_block, &plan);
	if (status != ACCEL_COMPILE_APPLIED || plan == NULL) {
		fprintf(stderr, "doall.noct was not applied\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}
	if (!func_block->val.func.is_accel) {
		fprintf(stderr, "analysis mutated the accelerator hint\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}
	if (accel_function_plan_get_region_count(plan) != 1 ||
	    accel_function_plan_get_generated_local_count(plan) != 2) {
		fprintf(stderr, "incorrect function plan shape\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	program = accel_function_plan_get_region(plan, 0);
	if (program == NULL || program->kernel_count != 3) {
		fprintf(stderr, "incorrect region kernel count\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}
	if (!accel_program_validate(program, error, sizeof(error))) {
		fprintf(stderr, "program validation failed: %s\n", error);
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	/* Keep the initial transfer policy conservative for dynamic ranges. */
	for (i = 0; i < program->buffer_count; i++) {
		if (!program->buffer[i].upload_required) {
			fprintf(stderr, "unsafe upload elision in initial plan\n");
			cleanup_case();
			accel_function_plan_destroy(plan);
			return false;
		}
	}

	memset(scalar_value, 0, sizeof(scalar_value));

	/* Supply dynamic n and scale values by program scalar-binding order. */
	for (i = 0; i < program->scalar_count; i++) {
		if (strcmp(program->scalar[i].name, "n") == 0)
			scalar_value[i] = 7;
	}
	if (!accel_program_evaluate_size(
		program,
		program->kernel[0].trip_expression,
		program->scalar_count,
		scalar_value,
		&trip)) {
		fprintf(stderr, "trip evaluation failed\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}
	if (trip != 7) {
		fprintf(stderr, "incorrect dynamic trip result\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	required_end = -1;

	/* Verify the affine source[i + 1] requirement is preserved in the DAG. */
	for (i = 0; i < program->buffer_count; i++) {
		if (strcmp(program->buffer[i].name, "source") != 0)
			continue;
		if (!accel_program_evaluate_size(
			program,
			program->buffer[i].required_end_expression,
			program->scalar_count,
			scalar_value,
			&required_end)) {
			fprintf(stderr, "required range evaluation failed\n");
			cleanup_case();
			accel_function_plan_destroy(plan);
			return false;
		}
	}
	if (required_end != 8) {
		fprintf(stderr, "affine required range was not preserved\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	clone = accel_program_clone(program);
	if (clone == NULL) {
		fprintf(stderr, "program clone failed\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	/* Destroy all HIR/AST arenas before checking deep-owned plan strings. */
	cleanup_case();
	valid = accel_program_validate(program, error, sizeof(error));
	if (!valid || strcmp(program->function_name, "transform") != 0) {
		fprintf(stderr, "program retained source-arena storage\n");
		accel_program_destroy(clone);
		accel_function_plan_destroy(plan);
		return false;
	}
	if (!accel_program_validate(clone, error, sizeof(error))) {
		fprintf(stderr, "cloned program validation failed: %s\n", error);
		accel_program_destroy(clone);
		accel_function_plan_destroy(plan);
		return false;
	}

	accel_program_destroy(clone);
	accel_function_plan_destroy(plan);

	return true;
}

/* Compile one ordinary local Packed as a CPU-backed session argument. */
static bool
run_local_host_case(
	const char *directory)
{
	struct accel_function_plan *plan;
	struct hir_block *func_block;
	const struct accel_program *program;
	enum accel_compile_status status;
	uint32_t i;
	bool found;

	plan = NULL;
	func_block = NULL;
	found = false;
	if (!build_case(directory, "local.noct", &func_block))
		return false;

	status = accel_compile_func(func_block, &plan);
	if (status != ACCEL_COMPILE_APPLIED || plan == NULL) {
		fprintf(stderr, "local.noct was not applied\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	program = accel_function_plan_get_region(plan, 0);
	if (program == NULL) {
		fprintf(stderr, "local.noct has no region program\n");
		cleanup_case();
		accel_function_plan_destroy(plan);
		return false;
	}

	/* Locate the local by stable source symbol and verify its host contract. */
	for (i = 0; i < program->buffer_count; i++) {
		if (strcmp(program->buffer[i].name, "temporary") != 0)
			continue;
		if (program->buffer[i].origin != ACCEL_BUFFER_LOCAL_HOST ||
		    program->buffer[i].args_slot !=
		    func_block->val.func.param_count ||
		    !program->buffer[i].upload_required) {
			fprintf(stderr, "incorrect local host residency plan\n");
			cleanup_case();
			accel_function_plan_destroy(plan);
			return false;
		}
		found = true;
	}

	cleanup_case();
	accel_function_plan_destroy(plan);
	if (!found) {
		fprintf(stderr, "local host buffer was not planned\n");
		return false;
	}

	return true;
}

/* Compile one valid CPU function that the initial GPU subset must decline. */
static bool
run_declined_case(
	const char *directory,
	const char *name)
{
	struct accel_function_plan *plan;
	struct hir_block *func_block;
	enum accel_compile_status status;

	plan = NULL;
	func_block = NULL;
	if (!build_case(directory, name, &func_block))
		return false;

	status = accel_compile_func(func_block, &plan);
	cleanup_case();
	if (status != ACCEL_COMPILE_DECLINED || plan != NULL) {
		fprintf(stderr, "%s did not decline cleanly\n", name);
		accel_function_plan_destroy(plan);
		return false;
	}

	return true;
}

/* Ensure the validator rejects a use-before-definition buffer index. */
static bool
run_invalid_ir_case(
	void)
{
	struct accel_ir_instruction instruction;
	struct accel_ir_builder builder;
	struct accel_ir_kernel *kernel;
	uint32_t result;
	char error[160];
	bool valid;

	kernel = accel_ir_kernel_create("invalid", 1, 1, 0, 1);
	if (kernel == NULL)
		return false;
	if (!accel_ir_kernel_set_buffer_type(kernel, 0, ACCEL_IR_I32)) {
		accel_ir_kernel_destroy(kernel);
		return false;
	}

	accel_ir_builder_init(&builder, kernel);
	memset(&instruction, 0, sizeof(instruction));
	instruction.opcode = ACCEL_IR_BUFFER_LOAD;
	instruction.result_type = ACCEL_IR_I32;
	instruction.result = ACCEL_IR_VALUE_NONE;
	instruction.operand[0] = 7;
	instruction.operand[1] = ACCEL_IR_VALUE_NONE;
	instruction.operand[2] = ACCEL_IR_VALUE_NONE;
	instruction.reference = 0;
	if (!accel_ir_builder_append(&builder, &instruction, &result)) {
		accel_ir_kernel_destroy(kernel);
		return false;
	}

	valid = accel_ir_kernel_validate(kernel, error, sizeof(error));
	accel_ir_kernel_destroy(kernel);
	if (valid) {
		fprintf(stderr, "invalid IR was accepted\n");
		return false;
	}

	return true;
}

/* Parse, build, type, and return the fixture's accelerator HIR function. */
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
		fprintf(stderr, "%s:%d: %s\n", name, ast_get_error_line(), ast_get_error_message());
		free(source);
		ast_cleanup();
		return false;
	}
	free(source);

	if (!hir_build()) {
		fprintf(stderr, "%s:%d: %s\n", name, hir_get_error_line(), hir_get_error_message());
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
