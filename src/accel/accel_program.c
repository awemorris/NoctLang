/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Owned target-neutral accelerator program plans.
 */

#include "accel_program.h"

#include <stdlib.h>
#include <string.h>

#define ACCEL_INT64_MAX_VALUE	((int64_t)(((uint64_t)-1) >> 1))
#define ACCEL_INT64_MIN_VALUE	(-ACCEL_INT64_MAX_VALUE - 1)

static bool accel_program_error(char *error, size_t error_size, const char *message);
static bool accel_program_add_checked(int64_t left, int64_t right, int64_t *result);
static bool accel_program_sub_checked(int64_t left, int64_t right, int64_t *result);
static bool accel_program_mul_checked(int64_t left, int64_t right, int64_t *result);
static bool accel_program_grow_scalars(struct accel_program *program);
static bool accel_program_grow_size_expressions(struct accel_program *program);
static bool accel_program_grow_buffers(struct accel_program *program);
static bool accel_program_grow_kernels(struct accel_program *program);
static bool accel_function_plan_grow(struct accel_function_plan *plan);
static bool accel_program_validate_size_expressions(const struct accel_program *program, char *error, size_t error_size);
static bool accel_program_validate_scalars(const struct accel_program *program, char *error, size_t error_size);
static bool accel_program_validate_buffers(const struct accel_program *program, char *error, size_t error_size);
static bool accel_program_validate_kernels(const struct accel_program *program, char *error, size_t error_size);
static int accel_program_buffer_value_type(int element_kind);

/*
 * Allocates an empty accelerator program for one consecutive loop region.
 */
struct accel_program *
accel_program_create(
	const char *source_name,
	const char *function_name,
	int source_line,
	uint32_t source_function_index,
	uint32_t region_index,
	int first_block_id,
	int last_block_id)
{
	struct accel_program *program;

	if (source_name == NULL)
		return NULL;
	if (function_name == NULL)
		return NULL;
	if (first_block_id < 0)
		return NULL;
	if (last_block_id < 0)
		return NULL;

	program = noct_calloc(1, sizeof(*program));
	if (program == NULL)
		return NULL;

	program->source_name = noct_strdup(source_name);
	if (program->source_name == NULL) {
		accel_program_destroy(program);
		return NULL;
	}

	program->function_name = noct_strdup(function_name);
	if (program->function_name == NULL) {
		accel_program_destroy(program);
		return NULL;
	}

	program->source_line = source_line;
	program->source_function_index = source_function_index;
	program->region_index = region_index;
	program->first_block_id = first_block_id;
	program->last_block_id = last_block_id;

	return program;
}

/*
 * Clones an accelerator program without cloning backend-owned payload.
 */
struct accel_program *
accel_program_clone(
	const struct accel_program *program)
{
	struct accel_program *result;
	struct accel_scalar_binding scalar;
	struct accel_buffer_binding buffer;
	struct accel_kernel_plan kernel;
	uint32_t ignored;
	uint32_t i;

	if (program == NULL)
		return NULL;

	result = accel_program_create(
		program->source_name,
		program->function_name,
		program->source_line,
		program->source_function_index,
		program->region_index,
		program->first_block_id,
		program->last_block_id);
	if (result == NULL)
		return NULL;

	/* Clone scalar bindings in their deterministic order. */
	for (i = 0; i < program->scalar_count; i++) {
		scalar = program->scalar[i];
		if (!accel_program_add_scalar(result, &scalar, &ignored)) {
			accel_program_destroy(result);
			return NULL;
		}
	}

	/* Clone size expressions without retaining source pointers. */
	for (i = 0; i < program->size_expression_count; i++) {
		if (!accel_program_add_size_expression(
			result,
			&program->size_expression[i],
			&ignored)) {
			accel_program_destroy(result);
			return NULL;
		}
	}

	/* Clone every buffer descriptor and its fixed effect table. */
	for (i = 0; i < program->buffer_count; i++) {
		buffer = program->buffer[i];
		if (!accel_program_add_buffer(result, &buffer, &ignored)) {
			accel_program_destroy(result);
			return NULL;
		}
	}

	/* Clone every typed kernel before transferring it to the result. */
	for (i = 0; i < program->kernel_count; i++) {
		kernel = program->kernel[i];
		kernel.ir = accel_ir_kernel_clone(program->kernel[i].ir);
		if (kernel.ir == NULL) {
			accel_program_destroy(result);
			return NULL;
		}

		if (!accel_program_add_kernel(result, &kernel, &ignored)) {
			accel_ir_kernel_destroy(kernel.ir);
			accel_program_destroy(result);
			return NULL;
		}
	}

	return result;
}

/*
 * Destroys an accelerator program and every object it owns.
 */
void
accel_program_destroy(
	struct accel_program *program)
{
	uint32_t i;

	if (program == NULL)
		return;

	if (program->backend_payload != NULL &&
	    program->destroy_backend_payload != NULL) {
		program->destroy_backend_payload(program->backend_payload);
	}

	/* Release every deep-copied scalar name. */
	for (i = 0; i < program->scalar_count; i++)
		noct_free(program->scalar[i].name);

	/* Release every deep-copied buffer name. */
	for (i = 0; i < program->buffer_count; i++)
		noct_free(program->buffer[i].name);

	/* Release every typed kernel owned by the program. */
	for (i = 0; i < program->kernel_count; i++)
		accel_ir_kernel_destroy(program->kernel[i].ir);

	noct_free(program->kernel);
	noct_free(program->buffer);
	noct_free(program->size_expression);
	noct_free(program->scalar);
	noct_free(program->function_name);
	noct_free(program->source_name);
	noct_free(program);
}

/*
 * Appends a deep-copied scalar binding.
 */
bool
accel_program_add_scalar(
	struct accel_program *program,
	const struct accel_scalar_binding *binding,
	uint32_t *binding_index)
{
	struct accel_scalar_binding *destination;

	if (binding_index != NULL)
		*binding_index = ACCEL_PROGRAM_INDEX_NONE;
	if (program == NULL)
		return false;
	if (binding == NULL)
		return false;
	if (binding->name == NULL)
		return false;
	if (program->scalar_count >= ACCEL_MAX_SCALAR_BINDINGS)
		return false;

	if (program->scalar_count == program->scalar_capacity) {
		if (!accel_program_grow_scalars(program))
			return false;
	}

	destination = &program->scalar[program->scalar_count];
	memset(destination, 0, sizeof(*destination));
	destination->name = noct_strdup(binding->name);
	if (destination->name == NULL)
		return false;

	destination->args_slot = binding->args_slot;
	destination->value_type = binding->value_type;
	if (binding_index != NULL)
		*binding_index = program->scalar_count;
	program->scalar_count++;

	return true;
}

/*
 * Appends a checked size-expression node.
 */
bool
accel_program_add_size_expression(
	struct accel_program *program,
	const struct accel_size_expression *expression,
	uint32_t *expression_index)
{
	if (expression_index != NULL)
		*expression_index = ACCEL_PROGRAM_INDEX_NONE;
	if (program == NULL)
		return false;
	if (expression == NULL)
		return false;
	if (program->size_expression_count >= ACCEL_MAX_SIZE_EXPRESSIONS)
		return false;

	if (program->size_expression_count ==
	    program->size_expression_capacity) {
		if (!accel_program_grow_size_expressions(program))
			return false;
	}

	program->size_expression[program->size_expression_count] = *expression;
	if (expression_index != NULL)
		*expression_index = program->size_expression_count;
	program->size_expression_count++;

	return true;
}

/*
 * Appends a deep-copied buffer binding.
 */
bool
accel_program_add_buffer(
	struct accel_program *program,
	const struct accel_buffer_binding *binding,
	uint32_t *binding_index)
{
	struct accel_buffer_binding *destination;

	if (binding_index != NULL)
		*binding_index = ACCEL_PROGRAM_INDEX_NONE;
	if (program == NULL)
		return false;
	if (binding == NULL)
		return false;
	if (binding->name == NULL)
		return false;
	if (program->buffer_count >= ACCEL_MAX_BUFFER_BINDINGS)
		return false;

	if (program->buffer_count == program->buffer_capacity) {
		if (!accel_program_grow_buffers(program))
			return false;
	}

	destination = &program->buffer[program->buffer_count];
	*destination = *binding;
	destination->name = noct_strdup(binding->name);
	if (destination->name == NULL) {
		memset(destination, 0, sizeof(*destination));
		return false;
	}

	if (binding_index != NULL)
		*binding_index = program->buffer_count;
	program->buffer_count++;

	return true;
}

/*
 * Transfers one typed kernel into a program.
 */
bool
accel_program_add_kernel(
	struct accel_program *program,
	const struct accel_kernel_plan *kernel,
	uint32_t *kernel_index)
{
	struct accel_kernel_plan *destination;

	if (kernel_index != NULL)
		*kernel_index = ACCEL_PROGRAM_INDEX_NONE;
	if (program == NULL)
		return false;
	if (kernel == NULL)
		return false;
	if (kernel->ir == NULL)
		return false;
	if (program->kernel_count >= ACCEL_MAX_KERNELS)
		return false;

	if (program->kernel_count == program->kernel_capacity) {
		if (!accel_program_grow_kernels(program))
			return false;
	}

	destination = &program->kernel[program->kernel_count];
	*destination = *kernel;
	destination->kernel_index = program->kernel_count;
	if (kernel_index != NULL)
		*kernel_index = program->kernel_count;
	program->kernel_count++;

	return true;
}

/*
 * Evaluates one checked size expression with signed scalar inputs.
 */
bool
accel_program_evaluate_size(
	const struct accel_program *program,
	uint32_t expression_index,
	uint32_t scalar_count,
	const int64_t scalar_value[],
	int64_t *result)
{
	int64_t value[ACCEL_MAX_SIZE_EXPRESSIONS];
	const struct accel_size_expression *expression;
	uint32_t i;

	if (program == NULL)
		return false;
	if (result == NULL)
		return false;
	if (program->size_expression_count > ACCEL_MAX_SIZE_EXPRESSIONS)
		return false;
	if (expression_index >= program->size_expression_count)
		return false;
	if (program->size_expression_count != 0 &&
	    program->size_expression == NULL) {
		return false;
	}

	/* Evaluate the topologically ordered DAG through the requested node. */
	for (i = 0; i <= expression_index; i++) {
		expression = &program->size_expression[i];

		/* Evaluate the checked operation represented by this node. */
		switch (expression->opcode) {
		case ACCEL_SIZE_CONSTANT:
			if (expression->value < 0)
				return false;
			value[i] = expression->value;
			break;
		case ACCEL_SIZE_SCALAR:
			if (expression->reference >= scalar_count)
				return false;
			if (scalar_value == NULL)
				return false;
			value[i] = scalar_value[expression->reference];
			break;
		case ACCEL_SIZE_ADD:
			if (expression->left >= i || expression->right >= i)
				return false;
			if (!accel_program_add_checked(
				value[expression->left],
				value[expression->right],
				&value[i])) {
				return false;
			}
			break;
		case ACCEL_SIZE_SUB:
			if (expression->left >= i || expression->right >= i)
				return false;
			if (!accel_program_sub_checked(
				value[expression->left],
				value[expression->right],
				&value[i])) {
				return false;
			}
			break;
		case ACCEL_SIZE_MUL_CONSTANT:
			if (expression->left >= i)
				return false;
			if (!accel_program_mul_checked(
				value[expression->left],
				expression->value,
				&value[i])) {
				return false;
			}
			break;
		case ACCEL_SIZE_MIN:
			if (expression->left >= i || expression->right >= i)
				return false;
			if (value[expression->left] < value[expression->right])
				value[i] = value[expression->left];
			else
				value[i] = value[expression->right];
			break;
		case ACCEL_SIZE_MAX:
			if (expression->left >= i || expression->right >= i)
				return false;
			if (value[expression->left] > value[expression->right])
				value[i] = value[expression->left];
			else
				value[i] = value[expression->right];
			break;
		case ACCEL_SIZE_MAX_ZERO:
			if (expression->left >= i)
				return false;
			if (value[expression->left] > 0)
				value[i] = value[expression->left];
			else
				value[i] = 0;
			break;
		default:
			return false;
		}
	}

	*result = value[expression_index];

	return true;
}

/*
 * Validates an owned program and every contained typed kernel.
 */
bool
accel_program_validate(
	const struct accel_program *program,
	char *error,
	size_t error_size)
{
	if (program == NULL)
		return accel_program_error(error, error_size, "null program");
	if (program->source_name == NULL)
		return accel_program_error(error, error_size, "missing source name");
	if (program->function_name == NULL)
		return accel_program_error(error, error_size, "missing function name");
	if (program->first_block_id < 0 || program->last_block_id < 0)
		return accel_program_error(error, error_size, "invalid region block id");
	if (program->kernel_count == 0)
		return accel_program_error(error, error_size, "empty accelerator region");
	if (program->scalar_count > ACCEL_MAX_SCALAR_BINDINGS)
		return accel_program_error(error, error_size, "scalar binding limit exceeded");
	if (program->buffer_count > ACCEL_MAX_BUFFER_BINDINGS)
		return accel_program_error(error, error_size, "buffer binding limit exceeded");
	if (program->kernel_count > ACCEL_MAX_KERNELS)
		return accel_program_error(error, error_size, "kernel limit exceeded");
	if (program->size_expression_count > ACCEL_MAX_SIZE_EXPRESSIONS) {
		return accel_program_error(
			error,
			error_size,
			"size expression limit exceeded");
	}
	if ((program->backend_payload == NULL) !=
	    (program->destroy_backend_payload == NULL)) {
		return accel_program_error(error, error_size, "invalid backend payload owner");
	}

	if (!accel_program_validate_size_expressions(program, error, error_size))
		return false;
	if (!accel_program_validate_scalars(program, error, error_size))
		return false;
	if (!accel_program_validate_buffers(program, error, error_size))
		return false;
	if (!accel_program_validate_kernels(program, error, error_size))
		return false;

	if (error != NULL && error_size != 0)
		error[0] = '\0';

	return true;
}

/*
 * Allocates an empty function-level compilation plan.
 */
struct accel_function_plan *
accel_function_plan_create(void)
{
	return noct_calloc(1, sizeof(struct accel_function_plan));
}

/*
 * Transfers one region program into a function plan.
 */
bool
accel_function_plan_add_region(
	struct accel_function_plan *plan,
	struct accel_program *program)
{
	if (plan == NULL)
		return false;
	if (program == NULL)
		return false;
	if (plan->region_count >= ACCEL_MAX_KERNELS)
		return false;
	if (plan->generated_local_count > ACCEL_PROGRAM_INDEX_NONE - 2)
		return false;

	if (plan->region_count == plan->region_capacity) {
		if (!accel_function_plan_grow(plan))
			return false;
	}

	plan->region[plan->region_count] = program;
	plan->region_count++;
	plan->generated_local_count += 2;

	return true;
}

/*
 * Returns the number of region programs in a function plan.
 */
uint32_t
accel_function_plan_get_region_count(
	const struct accel_function_plan *plan)
{
	if (plan == NULL)
		return 0;

	return plan->region_count;
}

/*
 * Returns the number of locals needed by the planned rewrite.
 */
uint32_t
accel_function_plan_get_generated_local_count(
	const struct accel_function_plan *plan)
{
	if (plan == NULL)
		return 0;

	return plan->generated_local_count;
}

/*
 * Borrows one region program from a function plan.
 */
const struct accel_program *
accel_function_plan_get_region(
	const struct accel_function_plan *plan,
	uint32_t region_index)
{
	if (plan == NULL)
		return NULL;
	if (region_index >= plan->region_count)
		return NULL;

	return plan->region[region_index];
}

/*
 * Destroys a function plan and every region program it owns.
 */
void
accel_function_plan_destroy(
	struct accel_function_plan *plan)
{
	uint32_t i;

	if (plan == NULL)
		return;

	/* Release every region program owned by this transaction plan. */
	for (i = 0; i < plan->region_count; i++)
		accel_program_destroy(plan->region[i]);

	noct_free(plan->region);
	noct_free(plan);
}

/* Copy a stable validation error into the caller's optional buffer. */
static bool
accel_program_error(
	char *error,
	size_t error_size,
	const char *message)
{
	if (error != NULL && error_size != 0) {
		strncpy(error, message, error_size - 1);
		error[error_size - 1] = '\0';
	}

	return false;
}

/* Add two signed values without invoking signed overflow. */
static bool
accel_program_add_checked(
	int64_t left,
	int64_t right,
	int64_t *result)
{
	if (right > 0 && left > ACCEL_INT64_MAX_VALUE - right)
		return false;
	if (right < 0 && left < ACCEL_INT64_MIN_VALUE - right)
		return false;

	*result = left + right;

	return true;
}

/* Subtract two signed values without invoking signed overflow. */
static bool
accel_program_sub_checked(
	int64_t left,
	int64_t right,
	int64_t *result)
{
	if (right > 0 && left < ACCEL_INT64_MIN_VALUE + right)
		return false;
	if (right < 0 && left > ACCEL_INT64_MAX_VALUE + right)
		return false;

	*result = left - right;

	return true;
}

/* Multiply two nonnegative size values without overflow. */
static bool
accel_program_mul_checked(
	int64_t left,
	int64_t right,
	int64_t *result)
{
	if (left < 0)
		return false;
	if (right <= 0)
		return false;
	if (left != 0 && right > ACCEL_INT64_MAX_VALUE / left)
		return false;

	*result = left * right;

	return true;
}

/* Grows the scalar table within its private hard limit. */
static bool
accel_program_grow_scalars(
	struct accel_program *program)
{
	struct accel_scalar_binding *scalar;
	uint32_t capacity;
	size_t size;

	if (program->scalar_capacity == 0)
		capacity = 8;
	else
		capacity = program->scalar_capacity * 2;
	if (capacity > ACCEL_MAX_SCALAR_BINDINGS)
		capacity = ACCEL_MAX_SCALAR_BINDINGS;
	if (capacity <= program->scalar_capacity)
		return false;

	size = sizeof(*scalar) * capacity;
	scalar = noct_realloc(program->scalar, size);
	if (scalar == NULL)
		return false;

	program->scalar = scalar;
	program->scalar_capacity = capacity;

	return true;
}

/* Grows the checked size-expression table within its hard limit. */
static bool
accel_program_grow_size_expressions(
	struct accel_program *program)
{
	struct accel_size_expression *expression;
	uint32_t capacity;
	size_t size;

	if (program->size_expression_capacity == 0)
		capacity = 16;
	else
		capacity = program->size_expression_capacity * 2;
	if (capacity > ACCEL_MAX_SIZE_EXPRESSIONS)
		capacity = ACCEL_MAX_SIZE_EXPRESSIONS;
	if (capacity <= program->size_expression_capacity)
		return false;

	size = sizeof(*expression) * capacity;
	expression = noct_realloc(program->size_expression, size);
	if (expression == NULL)
		return false;

	program->size_expression = expression;
	program->size_expression_capacity = capacity;

	return true;
}

/* Grows the buffer table within its private hard limit. */
static bool
accel_program_grow_buffers(
	struct accel_program *program)
{
	struct accel_buffer_binding *buffer;
	uint32_t capacity;
	size_t size;

	if (program->buffer_capacity == 0)
		capacity = 8;
	else
		capacity = program->buffer_capacity * 2;
	if (capacity > ACCEL_MAX_BUFFER_BINDINGS)
		capacity = ACCEL_MAX_BUFFER_BINDINGS;
	if (capacity <= program->buffer_capacity)
		return false;

	size = sizeof(*buffer) * capacity;
	buffer = noct_realloc(program->buffer, size);
	if (buffer == NULL)
		return false;

	program->buffer = buffer;
	program->buffer_capacity = capacity;

	return true;
}

/* Grows the kernel table within its private hard limit. */
static bool
accel_program_grow_kernels(
	struct accel_program *program)
{
	struct accel_kernel_plan *kernel;
	uint32_t capacity;
	size_t size;

	if (program->kernel_capacity == 0)
		capacity = 4;
	else
		capacity = program->kernel_capacity * 2;
	if (capacity > ACCEL_MAX_KERNELS)
		capacity = ACCEL_MAX_KERNELS;
	if (capacity <= program->kernel_capacity)
		return false;

	size = sizeof(*kernel) * capacity;
	kernel = noct_realloc(program->kernel, size);
	if (kernel == NULL)
		return false;

	program->kernel = kernel;
	program->kernel_capacity = capacity;

	return true;
}

/* Grows a function plan's region-owner table. */
static bool
accel_function_plan_grow(
	struct accel_function_plan *plan)
{
	struct accel_program **region;
	uint32_t capacity;
	size_t size;

	if (plan->region_capacity == 0)
		capacity = 2;
	else
		capacity = plan->region_capacity * 2;
	if (capacity > ACCEL_MAX_KERNELS)
		capacity = ACCEL_MAX_KERNELS;
	if (capacity <= plan->region_capacity)
		return false;

	size = sizeof(*region) * capacity;
	region = noct_realloc(plan->region, size);
	if (region == NULL)
		return false;

	plan->region = region;
	plan->region_capacity = capacity;

	return true;
}

/* Validate the topologically ordered checked size-expression DAG. */
static bool
accel_program_validate_size_expressions(
	const struct accel_program *program,
	char *error,
	size_t error_size)
{
	const struct accel_size_expression *expression;
	uint32_t i;

	if (program->size_expression_count != 0 &&
	    program->size_expression == NULL) {
		return accel_program_error(error, error_size, "missing size expression table");
	}

	/* Validate every expression against only earlier nodes. */
	for (i = 0; i < program->size_expression_count; i++) {
		expression = &program->size_expression[i];

		/* Validate the operand form selected by the size opcode. */
		switch (expression->opcode) {
		case ACCEL_SIZE_CONSTANT:
			if (expression->value < 0) {
				return accel_program_error(
					error,
					error_size,
					"negative size constant");
			}
			break;
		case ACCEL_SIZE_SCALAR:
			if (expression->reference >= program->scalar_count) {
				return accel_program_error(
					error,
					error_size,
					"invalid size scalar reference");
			}
			if (program->scalar[expression->reference].value_type !=
			    ACCEL_IR_I32) {
				return accel_program_error(
					error,
					error_size,
					"noninteger size scalar");
			}
			break;
		case ACCEL_SIZE_ADD:
		case ACCEL_SIZE_SUB:
		case ACCEL_SIZE_MIN:
		case ACCEL_SIZE_MAX:
			if (expression->left >= i || expression->right >= i) {
				return accel_program_error(
					error,
					error_size,
					"invalid binary size expression");
			}
			break;
		case ACCEL_SIZE_MUL_CONSTANT:
			if (expression->left >= i || expression->value <= 0) {
				return accel_program_error(
					error,
					error_size,
					"invalid size multiplication");
			}
			break;
		case ACCEL_SIZE_MAX_ZERO:
			if (expression->left >= i) {
				return accel_program_error(
					error,
					error_size,
					"invalid zero-clamped size expression");
			}
			break;
		default:
			return accel_program_error(error, error_size, "invalid size opcode");
		}
	}

	return true;
}

/* Validate scalar names, types, and distinct runtime argument slots. */
static bool
accel_program_validate_scalars(
	const struct accel_program *program,
	char *error,
	size_t error_size)
{
	uint32_t i;
	uint32_t j;

	if (program->scalar_count != 0 && program->scalar == NULL)
		return accel_program_error(error, error_size, "missing scalar table");

	/* Validate every scalar descriptor and compare prior slots. */
	for (i = 0; i < program->scalar_count; i++) {
		if (program->scalar[i].name == NULL)
			return accel_program_error(error, error_size, "missing scalar name");
		if (program->scalar[i].args_slot >= HIR_PARAM_SIZE)
			return accel_program_error(error, error_size, "invalid scalar argument slot");
		if (program->scalar[i].value_type != ACCEL_IR_I32 &&
		    program->scalar[i].value_type != ACCEL_IR_F32) {
			return accel_program_error(error, error_size, "invalid scalar type");
		}

		/* Reject a second descriptor for the same runtime argument. */
		for (j = 0; j < i; j++) {
			if (program->scalar[j].args_slot ==
			    program->scalar[i].args_slot) {
				return accel_program_error(
					error,
					error_size,
					"duplicate scalar argument slot");
			}
		}
	}

	return true;
}

/* Validate buffer names, ranges, effects, and binding namespaces. */
static bool
accel_program_validate_buffers(
	const struct accel_program *program,
	char *error,
	size_t error_size)
{
	const struct accel_buffer_binding *buffer;
	uint32_t i;
	uint32_t j;
	int value_type;

	if (program->buffer_count != 0 && program->buffer == NULL)
		return accel_program_error(error, error_size, "missing buffer table");

	/* Validate every buffer descriptor and compare prior namespaces. */
	for (i = 0; i < program->buffer_count; i++) {
		buffer = &program->buffer[i];
		if (buffer->name == NULL)
			return accel_program_error(error, error_size, "missing buffer name");
		if (buffer->origin != ACCEL_BUFFER_PARAMETER &&
		    buffer->origin != ACCEL_BUFFER_LOCAL_HOST) {
			return accel_program_error(error, error_size, "unsupported buffer origin");
		}
		if (buffer->args_slot >= HIR_PARAM_SIZE)
			return accel_program_error(error, error_size, "invalid buffer argument slot");
		if (buffer->device_binding != i)
			return accel_program_error(error, error_size, "nondeterministic buffer binding");
		if (buffer->element_width != 4)
			return accel_program_error(error, error_size, "invalid buffer element width");

		value_type = accel_program_buffer_value_type(buffer->element_kind);
		if (value_type == ACCEL_IR_VOID)
			return accel_program_error(error, error_size, "invalid buffer element kind");
		if (buffer->required_first_expression >=
		    program->size_expression_count) {
			return accel_program_error(error, error_size, "invalid buffer range start");
		}
		if (buffer->required_end_expression >=
		    program->size_expression_count) {
			return accel_program_error(error, error_size, "invalid buffer range end");
		}
		if (buffer->required_byte_end_expression >=
		    program->size_expression_count) {
			return accel_program_error(error, error_size, "invalid buffer byte range");
		}

		/* Validate the exact range owned by every kernel effect. */
		for (j = 0; j < program->kernel_count; j++) {
			if (!buffer->effect[j].read && !buffer->effect[j].write) {
				if (buffer->kernel_required_first_expression[j] !=
				    ACCEL_PROGRAM_INDEX_NONE ||
				    buffer->kernel_required_end_expression[j] !=
				    ACCEL_PROGRAM_INDEX_NONE) {
					return accel_program_error(
						error,
						error_size,
						"unused kernel has a buffer range");
				}
				continue;
			}
			if (buffer->kernel_required_first_expression[j] >=
			    program->size_expression_count) {
				return accel_program_error(
					error,
					error_size,
					"invalid kernel buffer range start");
			}
			if (buffer->kernel_required_end_expression[j] >=
			    program->size_expression_count) {
				return accel_program_error(
					error,
					error_size,
					"invalid kernel buffer range end");
			}
		}
		if (!buffer->host_visible)
			return accel_program_error(error, error_size, "host buffer is not host visible");

		/* Reject collisions in either runtime or device binding namespace. */
		for (j = 0; j < i; j++) {
			if (program->buffer[j].args_slot == buffer->args_slot) {
				return accel_program_error(
					error,
					error_size,
					"duplicate buffer argument slot");
			}
			if (program->buffer[j].device_binding ==
			    buffer->device_binding) {
				return accel_program_error(
					error,
					error_size,
					"duplicate device binding");
			}
		}

		/* Ensure one argument is not both scalar and buffer typed. */
		for (j = 0; j < program->scalar_count; j++) {
			if (program->scalar[j].args_slot == buffer->args_slot) {
				return accel_program_error(
					error,
					error_size,
					"argument has two binding types");
			}
		}
	}

	return true;
}

/* Validate kernel metadata and every contained typed IR stream. */
static bool
accel_program_validate_kernels(
	const struct accel_program *program,
	char *error,
	size_t error_size)
{
	const struct accel_kernel_plan *kernel;
	char ir_error[128];
	uint32_t i;
	uint32_t j;
	int value_type;

	if (program->kernel_count != 0 && program->kernel == NULL)
		return accel_program_error(error, error_size, "missing kernel table");

	/* Validate every kernel and its program-level range expressions. */
	for (i = 0; i < program->kernel_count; i++) {
		kernel = &program->kernel[i];
		if (kernel->kernel_index != i)
			return accel_program_error(error, error_size, "nondeterministic kernel index");
		if (kernel->loop_block_id < 0)
			return accel_program_error(error, error_size, "invalid loop block id");
		if (kernel->start_expression >= program->size_expression_count)
			return accel_program_error(error, error_size, "invalid kernel start");
		if (kernel->stop_expression >= program->size_expression_count)
			return accel_program_error(error, error_size, "invalid kernel stop");
		if (kernel->trip_expression >= program->size_expression_count)
			return accel_program_error(error, error_size, "invalid kernel trip");
		if (kernel->ir == NULL)
			return accel_program_error(error, error_size, "missing typed kernel");
		if (kernel->ir->scalar_binding_count != program->scalar_count)
			return accel_program_error(error, error_size, "kernel scalar table mismatch");
		if (kernel->ir->buffer_binding_count != program->buffer_count)
			return accel_program_error(error, error_size, "kernel buffer table mismatch");

		/* Match each kernel buffer type to the program descriptor. */
		for (j = 0; j < program->buffer_count; j++) {
			value_type = accel_program_buffer_value_type(
				program->buffer[j].element_kind);
			if (kernel->ir->buffer_value_type[j] != value_type) {
				return accel_program_error(
					error,
					error_size,
					"kernel buffer type mismatch");
			}
		}

		if (!accel_ir_kernel_validate(kernel->ir, ir_error, sizeof(ir_error)))
			return accel_program_error(error, error_size, ir_error);
	}

	return true;
}

/* Map a supported Packed element kind to its source scalar IR type. */
static int
accel_program_buffer_value_type(
	int element_kind)
{
	if (element_kind == NOCT_PACKED_INT32)
		return ACCEL_IR_I32;
	if (element_kind == NOCT_PACKED_UINT32)
		return ACCEL_IR_I32;
	if (element_kind == NOCT_PACKED_FLOAT32)
		return ACCEL_IR_F32;

	return ACCEL_IR_VOID;
}
