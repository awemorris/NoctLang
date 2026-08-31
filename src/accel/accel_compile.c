/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Read-only HIR analysis and target-neutral accelerator lowering.
 */

#include "accel_private.h"
#include "accel_program.h"
#include "accel_residency.h"
#include "hir_opt_parallel.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define ACCEL_COMPILE_MAX_VISITED	1024

struct accel_compile_scalar {
	const char *name;
	uint32_t args_slot;
	uint32_t binding_index;
	int value_type;
	bool reassigned;
};

struct accel_compile_buffer {
	const struct hir_memory_object *object;
	int args_slot;
	int program_binding;
	bool gpu_read;
	bool gpu_write;
	bool cpu_read;
	bool cpu_write;
	bool returned;
	bool escaped;
	bool unknown_call;
	bool reassigned;
};

struct accel_compile_state {
	struct accel_compile_context base;
	struct hir_memory_catalog catalog;
	struct accel_compile_scalar scalar[ACCEL_MAX_SCALAR_BINDINGS];
	uint32_t scalar_count;
	struct accel_compile_buffer buffer[ACCEL_MAX_BUFFER_BINDINGS];
	uint32_t buffer_count;
	struct hir_block *loop[ACCEL_MAX_KERNELS];
	struct hir_loop_summary *summary[ACCEL_MAX_KERNELS];
	uint32_t loop_count;
	struct accel_program *program;
	enum accel_compile_status status;
};

struct accel_scan_context {
	struct accel_compile_state *state;
	struct hir_block *visited[ACCEL_COMPILE_MAX_VISITED];
	uint32_t visited_count;
};

struct accel_lower_value {
	uint32_t value;
	int type;
};

struct accel_kernel_lower {
	struct accel_compile_state *state;
	struct hir_block *loop;
	struct hir_loop_summary *summary;
	struct accel_ir_kernel *kernel;
	struct accel_ir_builder builder;
	uint32_t global_index;
	bool has_global_index;
	uint32_t uniform_value[ACCEL_MAX_SCALAR_BINDINGS];
	bool has_uniform[ACCEL_MAX_SCALAR_BINDINGS];
	uint32_t local_value[ACCEL_MAX_LOCALS];
	int local_type[ACCEL_MAX_LOCALS];
	bool has_local[ACCEL_MAX_LOCALS];
};

static const char *accel_decline_reason_string(int reason);
static enum accel_compile_status accel_compile_decline(struct accel_compile_state *state, int reason);
static enum accel_compile_status accel_compile_error(struct accel_compile_state *state, int line, const char *message);
static void accel_compile_cleanup(struct accel_compile_state *state);
static bool accel_compile_parameters(struct accel_compile_state *state);
static bool accel_compile_catalog(struct accel_compile_state *state);
static bool accel_compile_find_loops(struct accel_compile_state *state);
static bool accel_compile_analyze_loops(struct accel_compile_state *state);
static bool accel_compile_scan_function(struct accel_compile_state *state);
static bool accel_compile_build_plan(struct accel_compile_state *state);
static bool accel_compile_source_function_index(struct hir_block *func_block, uint32_t *index);
static struct hir_local *accel_compile_find_local(struct hir_block *func_block, const char *symbol);
static int accel_compile_param_slot(struct hir_block *func_block, const char *symbol);
static int accel_compile_scalar_index(const struct accel_compile_state *state, const char *symbol);
static int accel_compile_buffer_catalog_index(const struct accel_compile_state *state, const char *symbol);
static int accel_compile_buffer_program_index(const struct accel_compile_state *state, int object_id);
static bool accel_compile_local_constructor_valid(struct accel_compile_state *state, const struct hir_memory_object *object);
static bool accel_compile_block_seen(struct accel_scan_context *scan, struct hir_block *block);
static bool accel_compile_scan_block(struct accel_scan_context *scan, struct hir_block *block, bool in_gpu_loop);
static bool accel_compile_scan_statement(struct accel_scan_context *scan, const struct hir_stmt *statement, bool in_gpu_loop);
static bool accel_compile_scan_expression(struct accel_scan_context *scan, const struct hir_expr *expression, bool write, bool call_argument, bool returned, bool in_gpu_loop);
static const char *accel_compile_term_symbol(const struct hir_expr *expression);
static bool accel_compile_is_selected_loop(const struct accel_compile_state *state, const struct hir_block *block);
static bool accel_compile_add_scalar_bindings(struct accel_compile_state *state);
static bool accel_compile_mark_gpu_buffers(struct accel_compile_state *state);
static bool accel_compile_add_buffer_bindings(struct accel_compile_state *state);
static bool accel_compile_add_kernels(struct accel_compile_state *state);
static bool accel_compile_finalize_buffers(struct accel_compile_state *state);
static bool accel_compile_add_size_expression(struct accel_compile_state *state, const struct accel_size_expression *expression, uint32_t *index);
static bool accel_compile_size_from_hir(struct accel_compile_state *state, const struct hir_expr *expression, uint32_t *index);
static bool accel_compile_size_adjust(struct accel_compile_state *state, uint32_t base, int64_t adjustment, uint32_t *index);
static bool accel_compile_size_affine(struct accel_compile_state *state, uint32_t base, const struct hir_affine_index *affine, uint32_t *index);
static bool accel_compile_add_kernel_ranges(struct accel_compile_state *state, uint32_t kernel_index, uint32_t start, uint32_t stop);
static bool accel_compile_merge_range(struct accel_compile_state *state, uint32_t buffer_index, uint32_t kernel_index, uint32_t first, uint32_t end);
static bool accel_compile_merge_range_pair(struct accel_compile_state *state, uint32_t *required_first, uint32_t *required_end, uint32_t first, uint32_t end);
static bool accel_compile_build_kernel(struct accel_compile_state *state, uint32_t kernel_index, struct accel_ir_kernel **result);
static bool accel_lower_block(struct accel_kernel_lower *lower, struct hir_block *block);
static bool accel_lower_statement(struct accel_kernel_lower *lower, const struct hir_stmt *statement);
static bool accel_lower_expression(struct accel_kernel_lower *lower, const struct hir_expr *expression, int expected_type, struct accel_lower_value *result);
static bool accel_lower_binary(struct accel_kernel_lower *lower, const struct hir_expr *expression, int expected_type, struct accel_lower_value *result);
static bool accel_lower_comparison(struct accel_kernel_lower *lower, const struct hir_expr *expression, struct accel_lower_value *result);
static bool accel_lower_subscript(struct accel_kernel_lower *lower, const struct hir_expr *expression, struct accel_lower_value *result);
static bool accel_lower_index(struct accel_kernel_lower *lower, const struct hir_expr *expression, struct accel_lower_value *result);
static bool accel_lower_symbol(struct accel_kernel_lower *lower, const char *symbol, struct accel_lower_value *result);
static bool accel_lower_uniform(struct accel_kernel_lower *lower, uint32_t scalar_index, struct accel_lower_value *result);
static bool accel_lower_global_index(struct accel_kernel_lower *lower, struct accel_lower_value *result);
static bool accel_lower_index_adjust(struct accel_kernel_lower *lower, struct accel_lower_value base, struct accel_lower_value adjustment, bool subtract, struct accel_lower_value *result);
static bool accel_lower_constant_i32(struct accel_kernel_lower *lower, int32_t value, struct accel_lower_value *result);
static bool accel_lower_constant_f32(struct accel_kernel_lower *lower, float value, struct accel_lower_value *result);
static bool accel_lower_emit_binary(struct accel_kernel_lower *lower, int opcode, int result_type, struct accel_lower_value left, struct accel_lower_value right, struct accel_lower_value *result);
static bool accel_lower_emit_load(struct accel_kernel_lower *lower, uint32_t buffer_index, int result_type, struct accel_lower_value index, struct accel_lower_value *result);
static bool accel_lower_emit_store(struct accel_kernel_lower *lower, uint32_t buffer_index, struct accel_lower_value index, struct accel_lower_value value);
static bool accel_lower_coerce(struct accel_kernel_lower *lower, struct accel_lower_value value, int expected_type, struct accel_lower_value *result);
static bool accel_lower_append(struct accel_kernel_lower *lower, struct accel_ir_instruction *instruction, struct accel_lower_value *result);
static void accel_lower_instruction_init(struct accel_ir_instruction *instruction, int opcode, int result_type);
static int accel_compile_ir_type_for_packed(int element_kind);
static int accel_compile_ir_type_for_noct(int value_type);

/*
 * Compiles one accelerator-hinted function into a private function plan.
 */
enum accel_compile_status
accel_compile_func(
	struct hir_block *func_block,
	struct accel_function_plan **result)
{
	struct accel_compile_state state;
	enum accel_compile_status status;

	if (result == NULL)
		return ACCEL_COMPILE_ERROR;

	*result = NULL;
	memset(&state, 0, sizeof(state));
	state.base.func_block = func_block;
	state.status = ACCEL_COMPILE_APPLIED;

	if (func_block == NULL) {
		accel_compile_error(
			&state,
			0,
			N_TR("Invalid accelerator function HIR."));
		return ACCEL_COMPILE_ERROR;
	}
	if (func_block->type != HIR_BLOCK_FUNC) {
		accel_compile_error(
			&state,
			func_block->line,
			N_TR("Invalid accelerator function HIR."));
		return ACCEL_COMPILE_ERROR;
	}
	if (!func_block->val.func.is_accel)
		return accel_compile_decline(&state, ACCEL_DECLINE_NOT_ACCEL);
	if (func_block->val.func.return_type != HIR_TYPE_VOID)
		return accel_compile_decline(&state, ACCEL_DECLINE_RETURN_TYPE);

	if (!accel_compile_parameters(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}
	if (!accel_compile_catalog(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}
	if (!accel_compile_find_loops(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}
	if (!accel_compile_analyze_loops(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}
	if (!accel_compile_scan_function(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}
	if (!accel_compile_build_plan(&state)) {
		status = state.status;
		accel_compile_cleanup(&state);
		return status;
	}

	*result = state.base.plan;
	state.base.plan = NULL;
	accel_compile_cleanup(&state);

	return ACCEL_COMPILE_APPLIED;
}

/* Return the stable developer spelling for a decline decision. */
static const char *
accel_decline_reason_string(
	int reason)
{
	/* Map every internal decline reason to one deterministic spelling. */
	switch (reason) {
	case ACCEL_DECLINE_NONE:
		return "none";
	case ACCEL_DECLINE_NOT_ACCEL:
		return "not-accelerator-hinted";
	case ACCEL_DECLINE_RETURN_TYPE:
		return "unsupported-return-type";
	case ACCEL_DECLINE_PARAMETER_TYPE:
		return "unsupported-parameter-type";
	case ACCEL_DECLINE_PARAMETER_ALIAS:
		return "unrestricted-buffer-parameter";
	case ACCEL_DECLINE_LIMIT:
		return "private-limit";
	case ACCEL_DECLINE_MEMORY_CATALOG:
		return "memory-catalog";
	case ACCEL_DECLINE_CONTROL_FLOW:
		return "unsupported-control-flow";
	case ACCEL_DECLINE_LOOP_ANALYSIS:
		return "incomplete-loop-analysis";
	case ACCEL_DECLINE_NOT_DOALL:
		return "not-doall";
	case ACCEL_DECLINE_LOCAL_BUFFER:
		return "local-buffer";
	case ACCEL_DECLINE_RANGE:
		return "unsupported-range";
	case ACCEL_DECLINE_EXPRESSION:
		return "unsupported-expression";
	case ACCEL_DECLINE_BUFFER_ESCAPE:
		return "buffer-reassignment";
	default:
		return NULL;
	}
}

/* Record one non-error eligibility decision. */
static enum accel_compile_status
accel_compile_decline(
	struct accel_compile_state *state,
	int reason)
{
	const char *reason_name;

	reason_name = accel_decline_reason_string(reason);
	if (reason_name == NULL) {
		return accel_compile_error(
			state,
			state->base.func_block != NULL ?
				state->base.func_block->line : 0,
			N_TR("Invalid accelerator decline reason."));
	}

	state->base.decline_reason = reason;
	state->status = ACCEL_COMPILE_DECLINED;

	return ACCEL_COMPILE_DECLINED;
}

/* Report one deterministic hard compiler failure. */
static enum accel_compile_status
accel_compile_error(
	struct accel_compile_state *state,
	int line,
	const char *message)
{
	if (!state->base.error_reported) {
		hir_error(line, message);
		state->base.error_reported = true;
	}

	state->status = ACCEL_COMPILE_ERROR;

	return ACCEL_COMPILE_ERROR;
}

/* Release summaries and partially built owned objects. */
static void
accel_compile_cleanup(
	struct accel_compile_state *state)
{
	uint32_t i;

	/* Release every optimizer summary retained during construction. */
	for (i = 0; i < state->loop_count; i++) {
		hir_loop_summary_free(state->summary[i]);
		state->summary[i] = NULL;
	}

	accel_program_destroy(state->program);
	state->program = NULL;
	accel_function_plan_destroy(state->base.plan);
	state->base.plan = NULL;
}

/* Validate parameters and collect deterministic scalar binding metadata. */
static bool
accel_compile_parameters(
	struct accel_compile_state *state)
{
	struct hir_block *func_block;
	struct hir_local *local;
	uint32_t local_count;
	uint32_t i;
	int ir_type;
	int packed_type;

	func_block = state->base.func_block;
	if (func_block->val.func.param_count > ACCEL_MAX_SCALAR_BINDINGS) {
		accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
		return false;
	}

	local_count = 0;
	local = func_block->val.func.local;

	/* Count locals before any index is used by fixed compiler storage. */
	while (local != NULL) {
		if (local_count >= ACCEL_MAX_LOCALS) {
			accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
			return false;
		}
		if (local->index < 0 || local->index >= ACCEL_MAX_LOCALS) {
			accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
			return false;
		}

		local_count++;
		local = local->next;
	}

	/* Classify every function parameter from its checked annotation. */
	for (i = 0; i < func_block->val.func.param_count; i++) {
		if (func_block->val.func.param_name[i] == NULL) {
			accel_compile_error(
				state,
				func_block->line,
				N_TR("Invalid accelerator parameter metadata."));
			return false;
		}

		packed_type = func_block->val.func.param_packed_type[i];
		if (packed_type >= 0) {
			ir_type = accel_compile_ir_type_for_packed(packed_type);
			if (ir_type == ACCEL_IR_VOID) {
				accel_compile_decline(
					state,
					ACCEL_DECLINE_PARAMETER_TYPE);
				return false;
			}
			if (!func_block->val.func.param_restricted[i]) {
				accel_compile_decline(
					state,
					ACCEL_DECLINE_PARAMETER_ALIAS);
				return false;
			}

			continue;
		}

		ir_type = accel_compile_ir_type_for_noct(
			func_block->val.func.param_type[i]);
		if (ir_type != ACCEL_IR_I32 && ir_type != ACCEL_IR_F32) {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_PARAMETER_TYPE);
			return false;
		}

		state->scalar[state->scalar_count].name =
			func_block->val.func.param_name[i];
		state->scalar[state->scalar_count].args_slot = i;
		state->scalar[state->scalar_count].binding_index =
			state->scalar_count;
		state->scalar[state->scalar_count].value_type = ir_type;
		state->scalar_count++;
	}

	return true;
}

/* Reuse the shared optimizer memory catalog and map its borrowed entries. */
static bool
accel_compile_catalog(
	struct accel_compile_state *state)
{
	const struct hir_memory_object *object;
	uint32_t i;
	int slot;

	if (!hir_memory_catalog_build_func(
		state->base.func_block,
		&state->catalog)) {
		accel_compile_decline(state, ACCEL_DECLINE_MEMORY_CATALOG);
		return false;
	}
	if (state->catalog.count > ACCEL_MAX_BUFFER_BINDINGS) {
		accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
		return false;
	}

	state->buffer_count = state->catalog.count;

	/* Pair every catalog object with its runtime parameter slot. */
	for (i = 0; i < state->catalog.count; i++) {
		object = &state->catalog.object[i];
		state->buffer[i].object = object;
		state->buffer[i].program_binding = -1;
		slot = accel_compile_param_slot(
			state->base.func_block,
			object->symbol);
		state->buffer[i].args_slot = slot;
	}

	return true;
}

/* Find one consecutive top-level region containing only ranged loops. */
static bool
accel_compile_find_loops(
	struct accel_compile_state *state)
{
	struct hir_block *block;
	struct hir_block *visited[ACCEL_COMPILE_MAX_VISITED];
	uint32_t visited_count;
	uint32_t i;
	bool region_started;
	bool region_ended;

	visited_count = 0;
	region_started = false;
	region_ended = false;
	block = state->base.func_block->val.func.inner;

	/* Walk only the function's top-level successor chain. */
	while (block != NULL && block != state->base.func_block->succ) {
		/* Reject a cycle in the top-level successor chain. */
		for (i = 0; i < visited_count; i++) {
			if (visited[i] == block) {
				accel_compile_error(
					state,
					block->line,
					N_TR("Malformed accelerator function control flow."));
				return false;
			}
		}
		if (visited_count >= ACCEL_COMPILE_MAX_VISITED) {
			accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
			return false;
		}

		visited[visited_count++] = block;
		if (block->parent != state->base.func_block) {
			accel_compile_error(
				state,
				block->line,
				N_TR("Malformed accelerator function control flow."));
			return false;
		}

		if (block->type == HIR_BLOCK_BASIC) {
			if (region_started && block->val.basic.stmt_list != NULL)
				region_ended = true;
		} else if (block->type == HIR_BLOCK_FOR) {
			if (region_ended) {
				accel_compile_decline(
					state,
					ACCEL_DECLINE_CONTROL_FLOW);
				return false;
			}
			if (!block->val.for_.is_ranged) {
				accel_compile_decline(
					state,
					ACCEL_DECLINE_CONTROL_FLOW);
				return false;
			}
			if (state->loop_count >= ACCEL_MAX_KERNELS) {
				accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
				return false;
			}

			region_started = true;
			state->loop[state->loop_count++] = block;
		} else {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_CONTROL_FLOW);
			return false;
		}

		block = block->succ;
	}

	if (state->loop_count == 0) {
		accel_compile_decline(state, ACCEL_DECLINE_NOT_DOALL);
		return false;
	}

	return true;
}

/* Classify every selected loop with the shared target-neutral analysis. */
static bool
accel_compile_analyze_loops(
	struct accel_compile_state *state)
{
	struct hir_doall_result doall;
	struct hir_loop_summary *summary;
	uint32_t i;

	/* Analyze and classify selected loops in source order. */
	for (i = 0; i < state->loop_count; i++) {
		summary = NULL;
		if (!hir_loop_analyze(
			state->base.func_block,
			state->loop[i],
			&state->catalog,
			&summary)) {
			accel_compile_error(
				state,
				state->loop[i]->line,
				N_TR("Failed to analyze accelerator loop."));
			return false;
		}

		state->summary[i] = summary;
		if (summary->analysis_status != HIR_ANALYSIS_COMPLETE) {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_LOOP_ANALYSIS);
			return false;
		}
		if (summary->has_nested_loop || summary->has_while_loop) {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_CONTROL_FLOW);
			return false;
		}
		if (!hir_doall_classify(summary, &doall)) {
			accel_compile_error(
				state,
				state->loop[i]->line,
				N_TR("Failed to classify accelerator loop."));
			return false;
		}
		if (doall.classification != HIR_PAR_CLASS_DOALL) {
			accel_compile_decline(state, ACCEL_DECLINE_NOT_DOALL);
			return false;
		}
		if (doall.alias_requirement_count != 0) {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_PARAMETER_ALIAS);
			return false;
		}
	}

	return true;
}

/* Collect use, escape, and reassignment facts without changing live HIR. */
static bool
accel_compile_scan_function(
	struct accel_compile_state *state)
{
	struct accel_scan_context scan;
	uint32_t i;
	int residency;

	memset(&scan, 0, sizeof(scan));
	scan.state = state;
	if (!accel_compile_scan_block(
		&scan,
		state->base.func_block->val.func.inner,
		false)) {
		return false;
	}

	/* Reject GPU-visible bindings outside the initial host-backed subset. */
	for (i = 0; i < state->buffer_count; i++) {
		if (!state->buffer[i].gpu_read && !state->buffer[i].gpu_write)
			continue;

		residency = accel_residency_classify_buffer(
			state->buffer[i].object,
			state->buffer[i].reassigned);
		if (residency == ACCEL_RESIDENCY_UNSUPPORTED) {
			accel_compile_decline(
				state,
				ACCEL_DECLINE_BUFFER_ESCAPE);
			return false;
		}
		if (residency == ACCEL_RESIDENCY_LOCAL_HOST &&
		    !accel_compile_local_constructor_valid(
			state,
			state->buffer[i].object)) {
			accel_compile_decline(state, ACCEL_DECLINE_LOCAL_BUFFER);
			return false;
		}
	}

	return true;
}

/* Resolve this HIR function's stable source-order table index. */
static bool
accel_compile_source_function_index(
	struct hir_block *func_block,
	uint32_t *index)
{
	uint32_t count;
	uint32_t i;

	if (index == NULL)
		return false;

	count = hir_get_function_count();

	/* Find the exact live HIR pointer in the current function table. */
	for (i = 0; i < count; i++) {
		if (hir_get_function(i) == func_block) {
			*index = i;
			return true;
		}
	}

	return false;
}

/* Find one local declaration by its scope-resolved HIR symbol. */
static struct hir_local *
accel_compile_find_local(
	struct hir_block *func_block,
	const char *symbol)
{
	struct hir_local *local;

	if (func_block == NULL)
		return NULL;
	if (symbol == NULL)
		return NULL;

	local = func_block->val.func.local;

	/* Search declarations in the function's deterministic local order. */
	while (local != NULL) {
		if (strcmp(local->symbol, symbol) == 0)
			return local;
		local = local->next;
	}

	return NULL;
}

/* Return the runtime argument slot for one parameter symbol. */
static int
accel_compile_param_slot(
	struct hir_block *func_block,
	const char *symbol)
{
	uint32_t i;

	if (func_block == NULL)
		return -1;
	if (symbol == NULL)
		return -1;

	/* Match the symbol against parameters in runtime argument order. */
	for (i = 0; i < func_block->val.func.param_count; i++) {
		if (strcmp(func_block->val.func.param_name[i], symbol) == 0)
			return (int)i;
	}

	return -1;
}

/* Return the private scalar-binding index for one parameter symbol. */
static int
accel_compile_scalar_index(
	const struct accel_compile_state *state,
	const char *symbol)
{
	uint32_t i;

	if (symbol == NULL)
		return -1;

	/* Search scalar bindings in source parameter order. */
	for (i = 0; i < state->scalar_count; i++) {
		if (strcmp(state->scalar[i].name, symbol) == 0)
			return (int)i;
	}

	return -1;
}

/* Return the compiler buffer slot associated with one HIR symbol. */
static int
accel_compile_buffer_catalog_index(
	const struct accel_compile_state *state,
	const char *symbol)
{
	uint32_t i;

	if (symbol == NULL)
		return -1;

	/* Search catalog-backed compiler records by exact symbol. */
	for (i = 0; i < state->buffer_count; i++) {
		if (strcmp(state->buffer[i].object->symbol, symbol) == 0)
			return (int)i;
	}

	return -1;
}

/* Return the region-local device binding for one catalog object id. */
static int
accel_compile_buffer_program_index(
	const struct accel_compile_state *state,
	int object_id)
{
	uint32_t i;

	/* Resolve the optimizer catalog id before reading program metadata. */
	for (i = 0; i < state->buffer_count; i++) {
		if (state->buffer[i].object->id == object_id)
			return state->buffer[i].program_binding;
	}

	return -1;
}

/* Accept only exact constructors that create a fresh CPU Packed object. */
static bool
accel_compile_local_constructor_valid(
	struct accel_compile_state *state,
	const struct hir_memory_object *object)
{
	struct hir_local *local;
	const struct hir_expr *initializer;
	const char *object_name;
	const char *function_name;

	local = accel_compile_find_local(
		state->base.func_block,
		object->symbol);
	if (local == NULL)
		return false;

	initializer = local->initializer;
	if (initializer == NULL || initializer->type != HIR_EXPR_THISCALL)
		return false;
	if (initializer->val.thiscall.arg_count != 1)
		return false;
	if (initializer->val.thiscall.func == NULL)
		return false;

	object_name = accel_compile_term_symbol(initializer->val.thiscall.obj);
	if (object_name == NULL || strcmp(object_name, "Packed") != 0)
		return false;

	function_name = NULL;
	if (object->element_kind == NOCT_PACKED_INT32)
		function_name = "int32";
	else if (object->element_kind == NOCT_PACKED_UINT32)
		function_name = "uint32";
	else if (object->element_kind == NOCT_PACKED_FLOAT32)
		function_name = "float32";
	if (function_name == NULL)
		return false;

	return strcmp(initializer->val.thiscall.func, function_name) == 0;
}

/* Record a visited block and detect graph cycles or excessive input. */
static bool
accel_compile_block_seen(
	struct accel_scan_context *scan,
	struct hir_block *block)
{
	uint32_t i;

	/* Search blocks already visited by the structural scan. */
	for (i = 0; i < scan->visited_count; i++) {
		if (scan->visited[i] == block)
			return true;
	}

	if (scan->visited_count >= ACCEL_COMPILE_MAX_VISITED) {
		accel_compile_decline(scan->state, ACCEL_DECLINE_LIMIT);
		return true;
	}

	scan->visited[scan->visited_count++] = block;

	return false;
}

/* Scan the reachable HIR graph for buffer and scalar binding effects. */
static bool
accel_compile_scan_block(
	struct accel_scan_context *scan,
	struct hir_block *block,
	bool in_gpu_loop)
{
	struct hir_stmt *statement;
	struct hir_block *chain;
	bool child_gpu_loop;

	if (block == NULL)
		return true;
	if (block == scan->state->base.func_block->succ)
		return true;
	if (accel_compile_block_seen(scan, block))
		return scan->state->status == ACCEL_COMPILE_APPLIED;

	/* Scan expressions and child blocks owned by this block shape. */
	switch (block->type) {
	case HIR_BLOCK_BASIC:
		statement = block->val.basic.stmt_list;

		/* Scan every statement in source order. */
		while (statement != NULL) {
			if (!accel_compile_scan_statement(
				scan,
				statement,
				in_gpu_loop)) {
				return false;
			}
			statement = statement->next;
		}
		break;
	case HIR_BLOCK_IF:
		chain = block;

		/* Scan every arm in the conditional chain. */
		while (chain != NULL) {
			if (!accel_compile_scan_expression(
				scan,
				chain->val.if_.cond,
				false,
				false,
				false,
				in_gpu_loop)) {
				return false;
			}
			if (!accel_compile_scan_block(
				scan,
				chain->val.if_.inner,
				in_gpu_loop)) {
				return false;
			}
			chain = chain->val.if_.chain_next;
		}
		break;
	case HIR_BLOCK_FOR:
		if (!accel_compile_scan_expression(
			scan,
			block->val.for_.start,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}
		if (!accel_compile_scan_expression(
			scan,
			block->val.for_.stop,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}
		if (!accel_compile_scan_expression(
			scan,
			block->val.for_.collection,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}

		child_gpu_loop = in_gpu_loop;
		if (accel_compile_is_selected_loop(scan->state, block))
			child_gpu_loop = true;
		if (!accel_compile_scan_block(
			scan,
			block->val.for_.inner,
			child_gpu_loop)) {
			return false;
		}
		break;
	case HIR_BLOCK_WHILE:
		if (!accel_compile_scan_expression(
			scan,
			block->val.while_.cond,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}
		if (!accel_compile_scan_block(
			scan,
			block->val.while_.inner,
			in_gpu_loop)) {
			return false;
		}
		break;
	default:
		accel_compile_error(
			scan->state,
			block->line,
			N_TR("Malformed accelerator function control flow."));
		return false;
	}

	return accel_compile_scan_block(scan, block->succ, in_gpu_loop);
}

/* Scan both sides of one statement with assignment and return context. */
static bool
accel_compile_scan_statement(
	struct accel_scan_context *scan,
	const struct hir_stmt *statement,
	bool in_gpu_loop)
{
	struct hir_local *local;
	const char *left_symbol;
	bool returned;

	left_symbol = accel_compile_term_symbol(statement->lhs);
	returned = false;
	if (left_symbol != NULL) {
		if (strcmp(left_symbol, "$return") == 0)
			returned = true;
	}

	if (!accel_compile_scan_expression(
		scan,
		statement->rhs,
		false,
		false,
		returned,
		in_gpu_loop)) {
		return false;
	}

	/* A declaration initializes its binding; it is not a reassignment. */
	local = accel_compile_find_local(
		scan->state->base.func_block,
		left_symbol);
	if (local != NULL && local->declaration_stmt == statement)
		return true;

	return accel_compile_scan_expression(
		scan,
		statement->lhs,
		true,
		false,
		false,
		in_gpu_loop);
}

/* Scan one expression while preserving direct buffer-use context. */
static bool
accel_compile_scan_expression(
	struct accel_scan_context *scan,
	const struct hir_expr *expression,
	bool write,
	bool call_argument,
	bool returned,
	bool in_gpu_loop)
{
	struct accel_compile_state *state;
	const char *symbol;
	uint32_t i;
	int buffer_index;
	int scalar_index;
	bool access_write;

	if (expression == NULL)
		return true;

	state = scan->state;

	/* Collect direct effects for this expression shape. */
	switch (expression->type) {
	case HIR_EXPR_TERM:
		symbol = accel_compile_term_symbol(expression);
		if (symbol == NULL)
			return true;

		scalar_index = accel_compile_scalar_index(state, symbol);
		if (scalar_index >= 0 && write)
			state->scalar[scalar_index].reassigned = true;

		buffer_index = accel_compile_buffer_catalog_index(state, symbol);
		if (buffer_index < 0)
			return true;

		if (write) {
			state->buffer[buffer_index].reassigned = true;
			return true;
		}

		state->buffer[buffer_index].escaped = true;
		if (call_argument)
			state->buffer[buffer_index].unknown_call = true;
		if (returned)
			state->buffer[buffer_index].returned = true;

		return true;
	case HIR_EXPR_SUBSCR:
		symbol = accel_compile_term_symbol(expression->val.binary.expr[0]);
		buffer_index = accel_compile_buffer_catalog_index(state, symbol);
		if (buffer_index >= 0) {
			if (in_gpu_loop) {
				if (write)
					state->buffer[buffer_index].gpu_write = true;
				else
					state->buffer[buffer_index].gpu_read = true;
			} else {
				if (write)
					state->buffer[buffer_index].cpu_write = true;
				else
					state->buffer[buffer_index].cpu_read = true;
			}
		} else {
			if (!accel_compile_scan_expression(
				scan,
				expression->val.binary.expr[0],
				false,
				call_argument,
				returned,
				in_gpu_loop)) {
				return false;
			}
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.binary.expr[1],
			false,
			false,
			false,
			in_gpu_loop);
	case HIR_EXPR_PLOAD8U:
	case HIR_EXPR_PLOAD8S:
	case HIR_EXPR_PLOAD16U:
	case HIR_EXPR_PLOAD16S:
	case HIR_EXPR_PLOAD32:
	case HIR_EXPR_PLOAD64:
	case HIR_EXPR_PLOADF32:
	case HIR_EXPR_PSTORE8:
	case HIR_EXPR_PSTORE16:
	case HIR_EXPR_PSTORE32:
	case HIR_EXPR_PSTORE64:
	case HIR_EXPR_PSTOREF32:
		access_write = false;
		if (expression->type == HIR_EXPR_PSTORE8 ||
		    expression->type == HIR_EXPR_PSTORE16 ||
		    expression->type == HIR_EXPR_PSTORE32 ||
		    expression->type == HIR_EXPR_PSTORE64 ||
		    expression->type == HIR_EXPR_PSTOREF32) {
			access_write = true;
		}

		symbol = accel_compile_term_symbol(expression->val.binary.expr[0]);
		buffer_index = accel_compile_buffer_catalog_index(state, symbol);
		if (buffer_index >= 0) {
			if (in_gpu_loop) {
				if (access_write)
					state->buffer[buffer_index].gpu_write = true;
				else
					state->buffer[buffer_index].gpu_read = true;
			} else {
				if (access_write)
					state->buffer[buffer_index].cpu_write = true;
				else
					state->buffer[buffer_index].cpu_read = true;
			}
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.binary.expr[1],
			false,
			false,
			false,
			in_gpu_loop);
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
	case HIR_EXPR_PCHECK:
	case HIR_EXPR_TYPEIS:
	case HIR_EXPR_VINDUCTF32:
		if (!accel_compile_scan_expression(
			scan,
			expression->val.binary.expr[0],
			false,
			call_argument,
			returned,
			in_gpu_loop)) {
			return false;
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.binary.expr[1],
			false,
			call_argument,
			returned,
			in_gpu_loop);
	case HIR_EXPR_NEG:
	case HIR_EXPR_NOT:
	case HIR_EXPR_PAR:
	case HIR_EXPR_PBASE:
	case HIR_EXPR_PLEN:
		return accel_compile_scan_expression(
			scan,
			expression->val.unary.expr,
			false,
			call_argument,
			returned,
			in_gpu_loop);
	case HIR_EXPR_DOT:
		return accel_compile_scan_expression(
			scan,
			expression->val.dot.obj,
			false,
			call_argument,
			returned,
			in_gpu_loop);
	case HIR_EXPR_CALL:
		if (!accel_compile_scan_expression(
			scan,
			expression->val.call.func,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}

		/* Mark every buffer argument as an unknown-call escape. */
		for (i = 0; i < expression->val.call.arg_count; i++) {
			if (!accel_compile_scan_expression(
				scan,
				expression->val.call.arg[i],
				false,
				true,
				false,
				in_gpu_loop)) {
				return false;
			}
		}

		return true;
	case HIR_EXPR_THISCALL:
		if (!accel_compile_scan_expression(
			scan,
			expression->val.thiscall.obj,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}

		/* Mark every method argument as an unknown-call escape. */
		for (i = 0; i < expression->val.thiscall.arg_count; i++) {
			if (!accel_compile_scan_expression(
				scan,
				expression->val.thiscall.arg[i],
				false,
				true,
				false,
				in_gpu_loop)) {
				return false;
			}
		}

		return true;
	case HIR_EXPR_ARRAY:
		/* Scan every array element as a value escape. */
		for (i = 0; i < expression->val.array.elem_count; i++) {
			if (!accel_compile_scan_expression(
				scan,
				expression->val.array.elem[i],
				false,
				true,
				returned,
				in_gpu_loop)) {
				return false;
			}
		}

		return true;
	case HIR_EXPR_DICT:
		/* Scan every dictionary value as a value escape. */
		for (i = 0; i < expression->val.dict.kv_count; i++) {
			if (!accel_compile_scan_expression(
				scan,
				expression->val.dict.value[i],
				false,
				true,
				returned,
				in_gpu_loop)) {
				return false;
			}
		}

		return true;
	case HIR_EXPR_NEW:
		return accel_compile_scan_expression(
			scan,
			expression->val.new_.init,
			false,
			true,
			returned,
			in_gpu_loop);
	case HIR_EXPR_CAPTURE:
		scalar_index = accel_compile_scalar_index(
			state,
			expression->val.capture.symbol);
		if (scalar_index >= 0)
			state->scalar[scalar_index].reassigned = true;

		return accel_compile_scan_expression(
			scan,
			expression->val.capture.expr,
			false,
			call_argument,
			returned,
			in_gpu_loop);
	case HIR_EXPR_SELECT:
		if (!accel_compile_scan_expression(
			scan,
			expression->val.select.cond,
			false,
			call_argument,
			returned,
			in_gpu_loop)) {
			return false;
		}
		if (!accel_compile_scan_expression(
			scan,
			expression->val.select.if_true,
			false,
			call_argument,
			returned,
			in_gpu_loop)) {
			return false;
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.select.if_false,
			false,
			call_argument,
			returned,
			in_gpu_loop);
	case HIR_EXPR_PMASKSTORE32:
		symbol = accel_compile_term_symbol(expression->val.mask_store.base);
		buffer_index = accel_compile_buffer_catalog_index(state, symbol);
		if (buffer_index >= 0) {
			if (in_gpu_loop)
				state->buffer[buffer_index].gpu_write = true;
			else
				state->buffer[buffer_index].cpu_write = true;
		}
		if (!accel_compile_scan_expression(
			scan,
			expression->val.mask_store.offset,
			false,
			false,
			false,
			in_gpu_loop)) {
			return false;
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.mask_store.mask,
			false,
			false,
			false,
			in_gpu_loop);
	case HIR_EXPR_PGATHER32:
		symbol = accel_compile_term_symbol(expression->val.gather.base);
		buffer_index = accel_compile_buffer_catalog_index(state, symbol);
		if (buffer_index >= 0) {
			if (in_gpu_loop)
				state->buffer[buffer_index].gpu_read = true;
			else
				state->buffer[buffer_index].cpu_read = true;
		}

		return accel_compile_scan_expression(
			scan,
			expression->val.gather.index,
			false,
			false,
			false,
			in_gpu_loop);
	default:
		accel_compile_error(
			state,
			state->base.func_block->line,
			N_TR("Malformed accelerator expression HIR."));
		return false;
	}
}

/* Return a plain symbol term without unwrapping other expressions. */
static const char *
accel_compile_term_symbol(
	const struct hir_expr *expression)
{
	if (expression == NULL)
		return NULL;
	if (expression->type != HIR_EXPR_TERM)
		return NULL;
	if (expression->val.term.term == NULL)
		return NULL;
	if (expression->val.term.term->type != HIR_TERM_SYMBOL)
		return NULL;

	return expression->val.term.term->val.symbol;
}

/* Test whether a loop belongs to the selected maximal region. */
static bool
accel_compile_is_selected_loop(
	const struct accel_compile_state *state,
	const struct hir_block *block)
{
	uint32_t i;

	/* Compare against selected loop identities from source traversal. */
	for (i = 0; i < state->loop_count; i++) {
		if (state->loop[i] == block)
			return true;
	}

	return false;
}

/* Construct the complete deep-owned function and region plan. */
static bool
accel_compile_build_plan(
	struct accel_compile_state *state)
{
	struct hir_block *func_block;
	char error[160];
	uint32_t function_index;

	func_block = state->base.func_block;
	if (func_block->val.func.file_name == NULL ||
	    func_block->val.func.name == NULL) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Invalid accelerator function metadata."));
		return false;
	}
	if (!accel_compile_source_function_index(func_block, &function_index)) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Accelerator function is not registered in the HIR table."));
		return false;
	}

	state->base.plan = accel_function_plan_create();
	if (state->base.plan == NULL) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Out of memory while planning accelerator function."));
		return false;
	}

	state->program = accel_program_create(
		func_block->val.func.file_name,
		func_block->val.func.name,
		func_block->line,
		function_index,
		0,
		state->loop[0]->id,
		state->loop[state->loop_count - 1]->id);
	if (state->program == NULL) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Out of memory while planning accelerator region."));
		return false;
	}

	if (!accel_compile_add_scalar_bindings(state))
		return false;
	if (!accel_compile_mark_gpu_buffers(state))
		return false;
	if (!accel_compile_add_buffer_bindings(state))
		return false;
	if (!accel_compile_add_kernels(state))
		return false;
	if (!accel_compile_finalize_buffers(state))
		return false;

	if (!accel_program_validate(state->program, error, sizeof(error))) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Accelerator program validation failed."));
		return false;
	}

	if (!accel_function_plan_add_region(
		state->base.plan,
		state->program)) {
		accel_compile_error(
			state,
			func_block->line,
			N_TR("Out of memory while recording accelerator region."));
		return false;
	}

	state->program = NULL;

	return true;
}

/* Copy scalar parameter bindings into the owned region program. */
static bool
accel_compile_add_scalar_bindings(
	struct accel_compile_state *state)
{
	struct accel_scalar_binding binding;
	uint32_t index;
	uint32_t i;

	/* Add scalar parameters in source argument order. */
	for (i = 0; i < state->scalar_count; i++) {
		memset(&binding, 0, sizeof(binding));
		binding.name = (char *)state->scalar[i].name;
		binding.args_slot = state->scalar[i].args_slot;
		binding.value_type = state->scalar[i].value_type;

		if (!accel_program_add_scalar(state->program, &binding, &index)) {
			accel_compile_error(
				state,
				state->base.func_block->line,
				N_TR("Out of memory while recording accelerator scalar."));
			return false;
		}
		if (index != state->scalar[i].binding_index) {
			accel_compile_error(
				state,
				state->base.func_block->line,
				N_TR("Nondeterministic accelerator scalar binding."));
			return false;
		}
	}

	return true;
}

/* Mark catalog objects referenced by shared loop-analysis summaries. */
static bool
accel_compile_mark_gpu_buffers(
	struct accel_compile_state *state)
{
	const struct hir_memory_access *access;
	uint32_t i;
	uint32_t j;
	int buffer_index;

	/* Mark every analyzed memory access in every selected kernel. */
	for (i = 0; i < state->loop_count; i++) {
		/* Mark each access reported for this kernel. */
		for (j = 0; j < state->summary[i]->access_count; j++) {
			access = &state->summary[i]->access[j];
			buffer_index = -1;

			/* Resolve the catalog object id to compiler metadata. */
			{
				uint32_t k;

				/* Find the buffer carrying this catalog object id. */
				for (k = 0; k < state->buffer_count; k++) {
					if (state->buffer[k].object->id ==
					    access->object_id) {
						buffer_index = (int)k;
						break;
					}
				}
			}

			if (buffer_index < 0) {
				accel_compile_error(
					state,
					access->line,
					N_TR("Accelerator access references an unknown buffer."));
				return false;
			}

			if (access->kind == HIR_MEMORY_READ)
				state->buffer[buffer_index].gpu_read = true;
			else if (access->kind == HIR_MEMORY_WRITE)
				state->buffer[buffer_index].gpu_write = true;
			else {
				accel_compile_error(
					state,
					access->line,
					N_TR("Accelerator access has an invalid effect."));
				return false;
			}
		}
	}

	return true;
}

/* Copy only GPU-visible parameter buffers into the region binding table. */
static bool
accel_compile_add_buffer_bindings(
	struct accel_compile_state *state)
{
	struct accel_buffer_binding binding;
	uint32_t index;
	uint32_t i;
	uint32_t j;
	uint32_t next_args_slot;
	int residency;

	next_args_slot = state->base.func_block->val.func.param_count;

	/* Add accessed buffers in optimizer catalog order. */
	for (i = 0; i < state->buffer_count; i++) {
		if (!state->buffer[i].gpu_read && !state->buffer[i].gpu_write)
			continue;

		residency = accel_residency_classify_buffer(
			state->buffer[i].object,
			state->buffer[i].reassigned);
		if (residency == ACCEL_RESIDENCY_UNSUPPORTED) {
			accel_compile_decline(state, ACCEL_DECLINE_BUFFER_ESCAPE);
			return false;
		}
		if (residency == ACCEL_RESIDENCY_PARAMETER_HOST &&
		    state->buffer[i].args_slot < 0) {
			accel_compile_error(
				state,
				state->buffer[i].object->source_line,
				N_TR("Accelerator buffer has no runtime argument slot."));
			return false;
		}
		if (residency == ACCEL_RESIDENCY_LOCAL_HOST) {
			if (next_args_slot >= HIR_PARAM_SIZE) {
				accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
				return false;
			}
			state->buffer[i].args_slot = (int)next_args_slot;
			next_args_slot++;
		}

		memset(&binding, 0, sizeof(binding));
		binding.name = (char *)state->buffer[i].object->symbol;
		binding.source_line = state->buffer[i].object->source_line;
		binding.element_kind = state->buffer[i].object->element_kind;
		binding.element_width =
			(uint32_t)state->buffer[i].object->element_width;
		binding.origin = residency == ACCEL_RESIDENCY_PARAMETER_HOST ?
			ACCEL_BUFFER_PARAMETER : ACCEL_BUFFER_LOCAL_HOST;
		binding.args_slot = (uint32_t)state->buffer[i].args_slot;
		binding.device_binding = state->program->buffer_count;
		binding.required_first_expression = ACCEL_PROGRAM_INDEX_NONE;
		binding.required_end_expression = ACCEL_PROGRAM_INDEX_NONE;
		binding.required_byte_end_expression = ACCEL_PROGRAM_INDEX_NONE;

		/* Mark every per-kernel range as absent until an access is recorded. */
		for (j = 0; j < ACCEL_MAX_KERNELS; j++) {
			binding.kernel_required_first_expression[j] =
				ACCEL_PROGRAM_INDEX_NONE;
			binding.kernel_required_end_expression[j] =
				ACCEL_PROGRAM_INDEX_NONE;
		}
		binding.host_visible = true;
		binding.cpu_read = state->buffer[i].cpu_read;
		binding.cpu_write = state->buffer[i].cpu_write;
		binding.returned = state->buffer[i].returned;
		binding.escaped = state->buffer[i].escaped;
		binding.unknown_call = state->buffer[i].unknown_call;
		binding.reassigned = state->buffer[i].reassigned;

		if (!accel_program_add_buffer(state->program, &binding, &index)) {
			accel_compile_error(
				state,
				state->buffer[i].object->source_line,
				N_TR("Out of memory while recording accelerator buffer."));
			return false;
		}

		state->buffer[i].program_binding = (int)index;
	}

	if (state->program->buffer_count == 0) {
		accel_compile_decline(state, ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	return true;
}

/* Build range DAG nodes, typed IR, and per-kernel effects in source order. */
static bool
accel_compile_add_kernels(
	struct accel_compile_state *state)
{
	struct accel_size_expression expression;
	struct accel_kernel_plan kernel;
	struct accel_ir_kernel *ir;
	uint32_t start;
	uint32_t stop;
	uint32_t difference;
	uint32_t trip;
	uint32_t kernel_index;
	uint32_t i;

	/* Lower every selected loop into one deterministic kernel entry. */
	for (i = 0; i < state->loop_count; i++) {
		if (!accel_compile_size_from_hir(
			state,
			state->loop[i]->val.for_.start,
			&start)) {
			return false;
		}
		if (!accel_compile_size_from_hir(
			state,
			state->loop[i]->val.for_.stop,
			&stop)) {
			return false;
		}

		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_SUB;
		expression.left = stop;
		expression.right = start;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&difference)) {
			return false;
		}

		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_MAX_ZERO;
		expression.left = difference;
		expression.right = ACCEL_PROGRAM_INDEX_NONE;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&trip)) {
			return false;
		}

		if (!accel_compile_add_kernel_ranges(state, i, start, stop))
			return false;

		ir = NULL;
		if (!accel_compile_build_kernel(state, i, &ir))
			return false;

		memset(&kernel, 0, sizeof(kernel));
		kernel.kernel_index = i;
		kernel.source_line = state->loop[i]->line;
		kernel.loop_block_id = state->loop[i]->id;
		kernel.start_expression = start;
		kernel.stop_expression = stop;
		kernel.trip_expression = trip;
		kernel.ir = ir;

		if (!accel_program_add_kernel(
			state->program,
			&kernel,
			&kernel_index)) {
			accel_ir_kernel_destroy(ir);
			accel_compile_error(
				state,
				state->loop[i]->line,
				N_TR("Out of memory while recording accelerator kernel."));
			return false;
		}
		if (kernel_index != i) {
			accel_compile_error(
				state,
				state->loop[i]->line,
				N_TR("Nondeterministic accelerator kernel index."));
			return false;
		}
	}

	return true;
}

/* Finalize aggregate byte ranges and host transfer requirements. */
static bool
accel_compile_finalize_buffers(
	struct accel_compile_state *state)
{
	struct accel_size_expression expression;
	struct accel_buffer_binding *buffer;
	uint32_t byte_end;
	uint32_t i;
	uint32_t j;
	bool any_write;

	/* Finalize every GPU-visible buffer in device binding order. */
	for (i = 0; i < state->program->buffer_count; i++) {
		buffer = &state->program->buffer[i];
		if (buffer->required_first_expression == ACCEL_PROGRAM_INDEX_NONE ||
		    buffer->required_end_expression == ACCEL_PROGRAM_INDEX_NONE) {
			accel_compile_error(
				state,
				buffer->source_line,
				N_TR("Accelerator buffer has no required range."));
			return false;
		}

		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_MUL_CONSTANT;
		expression.left = buffer->required_end_expression;
		expression.right = ACCEL_PROGRAM_INDEX_NONE;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		expression.value = buffer->element_width;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&byte_end)) {
			return false;
		}
		buffer->required_byte_end_expression = byte_end;

		any_write = false;

		/* Record writes without assuming a dynamic kernel covers the host buffer. */
		for (j = 0; j < state->program->kernel_count; j++) {
			if (buffer->effect[j].write)
				any_write = true;
		}

		/* Preserve untouched host elements across zero and partial ranges. */
		buffer->upload_required = true;
		buffer->download_required = any_write;
		buffer->materialization_required = false;
	}

	return true;
}

/* Append one size node while distinguishing a valid-source limit. */
static bool
accel_compile_add_size_expression(
	struct accel_compile_state *state,
	const struct accel_size_expression *expression,
	uint32_t *index)
{
	if (state->program->size_expression_count >=
	    ACCEL_MAX_SIZE_EXPRESSIONS) {
		accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
		return false;
	}

	if (!accel_program_add_size_expression(
		state->program,
		expression,
		index)) {
		accel_compile_error(
			state,
			state->base.func_block->line,
			N_TR("Out of memory while recording accelerator range."));
		return false;
	}

	return true;
}

/* Convert a pure constant or immutable int parameter to a checked DAG node. */
static bool
accel_compile_size_from_hir(
	struct accel_compile_state *state,
	const struct hir_expr *expression,
	uint32_t *index)
{
	struct accel_size_expression size_expression;
	const char *symbol;
	int64_t constant;
	uint32_t left;
	uint32_t right;
	int scalar_index;

	if (expression == NULL) {
		accel_compile_decline(state, ACCEL_DECLINE_RANGE);
		return false;
	}

	if (hir_opt_expr_constant_i64(expression, &constant)) {
		if (constant < 0 || constant > INT_MAX) {
			accel_compile_decline(state, ACCEL_DECLINE_RANGE);
			return false;
		}

		memset(&size_expression, 0, sizeof(size_expression));
		size_expression.opcode = ACCEL_SIZE_CONSTANT;
		size_expression.left = ACCEL_PROGRAM_INDEX_NONE;
		size_expression.right = ACCEL_PROGRAM_INDEX_NONE;
		size_expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		size_expression.value = constant;

		return accel_compile_add_size_expression(
			state,
			&size_expression,
			index);
	}

	/* Ignore only redundant parentheses in a dynamic range expression. */
	while (expression != NULL && expression->type == HIR_EXPR_PAR)
		expression = expression->val.unary.expr;
	if (expression == NULL) {
		accel_compile_decline(state, ACCEL_DECLINE_RANGE);
		return false;
	}

	symbol = accel_compile_term_symbol(expression);
	if (symbol != NULL) {
		scalar_index = accel_compile_scalar_index(state, symbol);
		if (scalar_index < 0) {
			accel_compile_decline(state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (state->scalar[scalar_index].value_type != ACCEL_IR_I32 ||
		    state->scalar[scalar_index].reassigned) {
			accel_compile_decline(state, ACCEL_DECLINE_RANGE);
			return false;
		}

		memset(&size_expression, 0, sizeof(size_expression));
		size_expression.opcode = ACCEL_SIZE_SCALAR;
		size_expression.left = ACCEL_PROGRAM_INDEX_NONE;
		size_expression.right = ACCEL_PROGRAM_INDEX_NONE;
		size_expression.reference =
			state->scalar[scalar_index].binding_index;

		return accel_compile_add_size_expression(
			state,
			&size_expression,
			index);
	}

	if (expression->type != HIR_EXPR_PLUS &&
	    expression->type != HIR_EXPR_MINUS) {
		accel_compile_decline(state, ACCEL_DECLINE_RANGE);
		return false;
	}

	if (!accel_compile_size_from_hir(
		state,
		expression->val.binary.expr[0],
		&left)) {
		return false;
	}
	if (!accel_compile_size_from_hir(
		state,
		expression->val.binary.expr[1],
		&right)) {
		return false;
	}

	memset(&size_expression, 0, sizeof(size_expression));
	if (expression->type == HIR_EXPR_PLUS)
		size_expression.opcode = ACCEL_SIZE_ADD;
	else
		size_expression.opcode = ACCEL_SIZE_SUB;
	size_expression.left = left;
	size_expression.right = right;
	size_expression.reference = ACCEL_PROGRAM_INDEX_NONE;

	return accel_compile_add_size_expression(
		state,
		&size_expression,
		index);
}

/* Add or subtract one signed affine constant from a size expression. */
static bool
accel_compile_size_adjust(
	struct accel_compile_state *state,
	uint32_t base,
	int64_t adjustment,
	uint32_t *index)
{
	struct accel_size_expression expression;
	uint32_t constant;

	if (adjustment == 0) {
		*index = base;
		return true;
	}

	memset(&expression, 0, sizeof(expression));
	expression.opcode = ACCEL_SIZE_CONSTANT;
	expression.left = ACCEL_PROGRAM_INDEX_NONE;
	expression.right = ACCEL_PROGRAM_INDEX_NONE;
	expression.reference = ACCEL_PROGRAM_INDEX_NONE;
	if (adjustment < 0)
		expression.value = -adjustment;
	else
		expression.value = adjustment;
	if (!accel_compile_add_size_expression(state, &expression, &constant))
		return false;

	memset(&expression, 0, sizeof(expression));
	if (adjustment < 0)
		expression.opcode = ACCEL_SIZE_SUB;
	else
		expression.opcode = ACCEL_SIZE_ADD;
	expression.left = base;
	expression.right = constant;
	expression.reference = ACCEL_PROGRAM_INDEX_NONE;

	return accel_compile_add_size_expression(state, &expression, index);
}

/* Apply one normalized counter-affine index to a loop bound expression. */
static bool
accel_compile_size_affine(
	struct accel_compile_state *state,
	uint32_t base,
	const struct hir_affine_index *affine,
	uint32_t *index)
{
	struct accel_size_expression expression;
	uint32_t invariant;
	uint32_t adjusted;
	int scalar_index;

	if (affine->kind != HIR_AFFINE_COUNTER_OFFSET) {
		accel_compile_decline(state, ACCEL_DECLINE_RANGE);
		return false;
	}

	adjusted = base;
	if (affine->invariant_symbol != NULL) {
		scalar_index = accel_compile_scalar_index(
			state,
			affine->invariant_symbol);
		if (scalar_index < 0) {
			accel_compile_decline(state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (state->scalar[scalar_index].value_type != ACCEL_IR_I32 ||
		    state->scalar[scalar_index].reassigned) {
			accel_compile_decline(state, ACCEL_DECLINE_RANGE);
			return false;
		}

		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_SCALAR;
		expression.left = ACCEL_PROGRAM_INDEX_NONE;
		expression.right = ACCEL_PROGRAM_INDEX_NONE;
		expression.reference =
			state->scalar[scalar_index].binding_index;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&invariant)) {
			return false;
		}

		memset(&expression, 0, sizeof(expression));
		if (affine->invariant_sign < 0)
			expression.opcode = ACCEL_SIZE_SUB;
		else
			expression.opcode = ACCEL_SIZE_ADD;
		expression.left = adjusted;
		expression.right = invariant;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&adjusted)) {
			return false;
		}
	}

	return accel_compile_size_adjust(
		state,
		adjusted,
		affine->offset,
		index);
}

/* Build required ranges and memory effects for one kernel summary. */
static bool
accel_compile_add_kernel_ranges(
	struct accel_compile_state *state,
	uint32_t kernel_index,
	uint32_t start,
	uint32_t stop)
{
	const struct hir_loop_summary *summary;
	const struct hir_memory_access *access;
	struct accel_buffer_binding *buffer;
	struct accel_buffer_effect *effect;
	uint32_t write_count[ACCEL_MAX_BUFFER_BINDINGS];
	bool exact_write[ACCEL_MAX_BUFFER_BINDINGS];
	uint32_t first;
	uint32_t end;
	uint32_t i;
	uint32_t buffer_index;
	int program_index;

	memset(write_count, 0, sizeof(write_count));
	memset(exact_write, 0, sizeof(exact_write));
	summary = state->summary[kernel_index];

	/* Convert every shared-analysis access into range and effect metadata. */
	for (i = 0; i < summary->access_count; i++) {
		access = &summary->access[i];
		program_index = accel_compile_buffer_program_index(
			state,
			access->object_id);
		if (program_index < 0) {
			accel_compile_error(
				state,
				access->line,
				N_TR("Accelerator access has no buffer binding."));
			return false;
		}
		buffer_index = (uint32_t)program_index;
		buffer = &state->program->buffer[buffer_index];
		effect = &buffer->effect[kernel_index];

		if (!accel_compile_size_affine(
			state,
			start,
			&access->index,
			&first)) {
			return false;
		}
		if (!accel_compile_size_affine(
			state,
			stop,
			&access->index,
			&end)) {
			return false;
		}
		if (!accel_compile_merge_range(
			state,
			buffer_index,
			kernel_index,
			first,
			end)) {
			return false;
		}

		if (access->kind == HIR_MEMORY_READ) {
			effect->read = true;
			if (!effect->write)
				effect->read_before_write = true;
		} else if (access->kind == HIR_MEMORY_WRITE) {
			effect->write = true;
			write_count[buffer_index]++;
			if (access->index.kind == HIR_AFFINE_COUNTER_OFFSET)
				exact_write[buffer_index] = true;
		} else {
			accel_compile_error(
				state,
				access->line,
				N_TR("Accelerator access has an invalid effect."));
			return false;
		}
	}

	/* Recognize one unconditional counter-affine write as a full range write. */
	for (i = 0; i < state->program->buffer_count; i++) {
		if (write_count[i] == 1 && exact_write[i])
			state->program->buffer[i].effect[kernel_index].full_overwrite = true;
	}

	return true;
}

/* Merge one access range into a buffer's function-region aggregate. */
static bool
accel_compile_merge_range(
	struct accel_compile_state *state,
	uint32_t buffer_index,
	uint32_t kernel_index,
	uint32_t first,
	uint32_t end)
{
	struct accel_buffer_binding *buffer;

	if (kernel_index >= ACCEL_MAX_KERNELS) {
		accel_compile_error(
			state,
			state->base.func_block->line,
			N_TR("Invalid accelerator kernel index."));
		return false;
	}

	if (buffer_index >= state->program->buffer_count) {
		accel_compile_error(
			state,
			state->base.func_block->line,
			N_TR("Invalid accelerator buffer binding."));
		return false;
	}

	buffer = &state->program->buffer[buffer_index];
	if (!accel_compile_merge_range_pair(
		state,
		&buffer->required_first_expression,
		&buffer->required_end_expression,
		first,
		end)) {
		return false;
	}
	if (!accel_compile_merge_range_pair(
		state,
		&buffer->kernel_required_first_expression[kernel_index],
		&buffer->kernel_required_end_expression[kernel_index],
		first,
		end)) {
		return false;
	}

	return true;
}

/* Merge one access range into one pair of size-expression roots. */
static bool
accel_compile_merge_range_pair(
	struct accel_compile_state *state,
	uint32_t *required_first,
	uint32_t *required_end,
	uint32_t first,
	uint32_t end)
{
	struct accel_size_expression expression;
	uint32_t merged;

	if (*required_first == ACCEL_PROGRAM_INDEX_NONE) {
		*required_first = first;
	} else {
		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_MIN;
		expression.left = *required_first;
		expression.right = first;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&merged)) {
			return false;
		}
		*required_first = merged;
	}

	if (*required_end == ACCEL_PROGRAM_INDEX_NONE) {
		*required_end = end;
	} else {
		memset(&expression, 0, sizeof(expression));
		expression.opcode = ACCEL_SIZE_MAX;
		expression.left = *required_end;
		expression.right = end;
		expression.reference = ACCEL_PROGRAM_INDEX_NONE;
		if (!accel_compile_add_size_expression(
			state,
			&expression,
			&merged)) {
			return false;
		}
		*required_end = merged;
	}

	return true;
}

/* Allocate and lower one selected loop into a validated typed kernel. */
static bool
accel_compile_build_kernel(
	struct accel_compile_state *state,
	uint32_t kernel_index,
	struct accel_ir_kernel **result)
{
	struct accel_kernel_lower lower;
	struct accel_ir_kernel *kernel;
	char name[512];
	char error[160];
	uint32_t i;
	int length;
	int value_type;

	*result = NULL;
	length = snprintf(
		name,
		sizeof(name),
		"%s$accel$%u",
		state->base.func_block->val.func.name,
		(unsigned int)kernel_index);
	if (length < 0 || (size_t)length >= sizeof(name)) {
		accel_compile_decline(state, ACCEL_DECLINE_LIMIT);
		return false;
	}

	kernel = accel_ir_kernel_create(
		name,
		state->loop[kernel_index]->line,
		state->loop[kernel_index]->id,
		state->program->scalar_count,
		state->program->buffer_count);
	if (kernel == NULL) {
		accel_compile_error(
			state,
			state->loop[kernel_index]->line,
			N_TR("Out of memory while allocating accelerator kernel."));
		return false;
	}

	/* Copy each program buffer's scalar storage type into the kernel. */
	for (i = 0; i < state->program->buffer_count; i++) {
		value_type = accel_compile_ir_type_for_packed(
			state->program->buffer[i].element_kind);
		if (!accel_ir_kernel_set_buffer_type(kernel, i, value_type)) {
			accel_ir_kernel_destroy(kernel);
			accel_compile_error(
				state,
				state->loop[kernel_index]->line,
				N_TR("Invalid accelerator kernel buffer type."));
			return false;
		}
	}

	memset(&lower, 0, sizeof(lower));
	lower.state = state;
	lower.loop = state->loop[kernel_index];
	lower.summary = state->summary[kernel_index];
	lower.kernel = kernel;
	accel_ir_builder_init(&lower.builder, kernel);

	if (!accel_lower_block(&lower, lower.loop->val.for_.inner)) {
		accel_ir_kernel_destroy(kernel);
		return false;
	}

	if (!accel_ir_kernel_validate(kernel, error, sizeof(error))) {
		accel_ir_kernel_destroy(kernel);
		accel_compile_error(
			state,
			state->loop[kernel_index]->line,
			N_TR("Generated accelerator IR failed validation."));
		return false;
	}

	*result = kernel;

	return true;
}

/* Lower the straight-line BASIC chain forming one loop body. */
static bool
accel_lower_block(
	struct accel_kernel_lower *lower,
	struct hir_block *block)
{
	struct hir_block *visited[ACCEL_COMPILE_MAX_VISITED];
	struct hir_stmt *statement;
	uint32_t visited_count;
	uint32_t i;

	visited_count = 0;

	/* Visit straight-line loop blocks until the ordinary back edge. */
	while (block != NULL) {
		/* Reject a premature cycle in the loop-body chain. */
		for (i = 0; i < visited_count; i++) {
			if (visited[i] == block) {
				accel_compile_decline(
					lower->state,
					ACCEL_DECLINE_CONTROL_FLOW);
				return false;
			}
		}
		if (visited_count >= ACCEL_COMPILE_MAX_VISITED) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_LIMIT);
			return false;
		}

		visited[visited_count++] = block;
		if (block->type != HIR_BLOCK_BASIC || block->parent != lower->loop) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_CONTROL_FLOW);
			return false;
		}
		if (block->is_return_edge ||
		    block->is_break_edge ||
		    block->is_continue_edge) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_CONTROL_FLOW);
			return false;
		}

		statement = block->val.basic.stmt_list;

		/* Lower each source statement in deterministic execution order. */
		while (statement != NULL) {
			if (!accel_lower_statement(lower, statement))
				return false;
			statement = statement->next;
		}

		if (block->stop) {
			if (block->succ != lower->loop->val.for_.inner) {
				accel_compile_decline(
					lower->state,
					ACCEL_DECLINE_CONTROL_FLOW);
				return false;
			}

			return true;
		}

		block = block->succ;
	}

	accel_compile_error(
		lower->state,
		lower->loop->line,
		N_TR("Malformed accelerator loop body."));

	return false;
}

/* Lower one scalar assignment or Packed element store. */
static bool
accel_lower_statement(
	struct accel_kernel_lower *lower,
	const struct hir_stmt *statement)
{
	struct accel_lower_value index;
	struct accel_lower_value value;
	struct hir_local *local;
	const char *symbol;
	int buffer_catalog_index;
	int buffer_program_index;
	int expected_type;

	if (statement->lhs == NULL) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	if (statement->lhs->type == HIR_EXPR_SUBSCR) {
		symbol = accel_compile_term_symbol(
			statement->lhs->val.binary.expr[0]);
		buffer_catalog_index = accel_compile_buffer_catalog_index(
			lower->state,
			symbol);
		if (buffer_catalog_index < 0) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}

		buffer_program_index = lower->state->buffer[
			buffer_catalog_index].program_binding;
		if (buffer_program_index < 0) {
			accel_compile_error(
				lower->state,
				statement->line,
				N_TR("Accelerator store has no buffer binding."));
			return false;
		}

		expected_type = accel_compile_ir_type_for_packed(
			lower->state->buffer[buffer_catalog_index].object->element_kind);
		if (!accel_lower_index(
			lower,
			statement->lhs->val.binary.expr[1],
			&index)) {
			return false;
		}
		if (!accel_lower_expression(
			lower,
			statement->rhs,
			expected_type,
			&value)) {
			return false;
		}

		return accel_lower_emit_store(
			lower,
			(uint32_t)buffer_program_index,
			index,
			value);
	}

	symbol = accel_compile_term_symbol(statement->lhs);
	if (symbol == NULL) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}
	if (strcmp(symbol, "$return") == 0) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_CONTROL_FLOW);
		return false;
	}

	local = accel_compile_find_local(lower->state->base.func_block, symbol);
	if (local == NULL) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}
	if (local->is_parameter ||
	    local->declaration_kind == HIR_LOCAL_DECL_LOOP_COUNTER) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_NOT_DOALL);
		return false;
	}
	if (local->index < 0 || local->index >= ACCEL_MAX_LOCALS) {
		accel_compile_decline(lower->state, ACCEL_DECLINE_LIMIT);
		return false;
	}

	expected_type = accel_compile_ir_type_for_noct(local->declared_type);
	if (expected_type != ACCEL_IR_I32 && expected_type != ACCEL_IR_F32) {
		expected_type = accel_compile_ir_type_for_noct(local->proven_type);
	}
	if (expected_type != ACCEL_IR_I32 && expected_type != ACCEL_IR_F32) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	if (!accel_lower_expression(
		lower,
		statement->rhs,
		expected_type,
		&value)) {
		return false;
	}

	lower->local_value[local->index] = value.value;
	lower->local_type[local->index] = value.type;
	lower->has_local[local->index] = true;

	return true;
}

/* Lower one source expression and apply the only safe BOOL-to-I32 coercion. */
static bool
accel_lower_expression(
	struct accel_kernel_lower *lower,
	const struct hir_expr *expression,
	int expected_type,
	struct accel_lower_value *result)
{
	struct accel_lower_value value;
	struct accel_lower_value zero;
	const struct hir_term *term;
	const char *symbol;

	if (expression == NULL) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	value.value = ACCEL_IR_VALUE_NONE;
	value.type = ACCEL_IR_VOID;

	/* Lower the exact expression shape without target-specific source text. */
	switch (expression->type) {
	case HIR_EXPR_TERM:
		term = expression->val.term.term;
		if (term == NULL) {
			accel_compile_error(
				lower->state,
				lower->loop->line,
				N_TR("Malformed accelerator term HIR."));
			return false;
		}

		/* Lower supported literals or resolve one current scalar value. */
		switch (term->type) {
		case HIR_TERM_INT:
			if (!accel_lower_constant_i32(lower, term->val.i, &value))
				return false;
			break;
		case HIR_TERM_FLOAT:
			if (!accel_lower_constant_f32(lower, term->val.f, &value))
				return false;
			break;
		case HIR_TERM_SYMBOL:
			symbol = term->val.symbol;
			if (!accel_lower_symbol(lower, symbol, &value))
				return false;
			break;
		default:
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		break;
	case HIR_EXPR_PAR:
		return accel_lower_expression(
			lower,
			expression->val.unary.expr,
			expected_type,
			result);
	case HIR_EXPR_NEG:
		if (!accel_lower_expression(
			lower,
			expression->val.unary.expr,
			ACCEL_IR_VOID,
			&value)) {
			return false;
		}
		if (value.type == ACCEL_IR_I32) {
			if (!accel_lower_constant_i32(lower, 0, &zero))
				return false;
		} else if (value.type == ACCEL_IR_F32) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		} else {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}

		if (!accel_lower_emit_binary(
			lower,
			ACCEL_IR_SUB,
			value.type,
			zero,
			value,
			&value)) {
			return false;
		}
		break;
	case HIR_EXPR_SUBSCR:
		if (!accel_lower_subscript(lower, expression, &value))
			return false;
		break;
	case HIR_EXPR_LT:
	case HIR_EXPR_LTE:
	case HIR_EXPR_GT:
	case HIR_EXPR_GTE:
	case HIR_EXPR_EQ:
	case HIR_EXPR_NEQ:
		if (!accel_lower_comparison(lower, expression, &value))
			return false;
		break;
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
		if (!accel_lower_binary(
			lower,
			expression,
			expected_type,
			&value)) {
			return false;
		}
		break;
	default:
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	return accel_lower_coerce(lower, value, expected_type, result);
}

/* Lower numeric, bitwise, shift, and constrained division operations. */
static bool
accel_lower_binary(
	struct accel_kernel_lower *lower,
	const struct hir_expr *expression,
	int expected_type,
	struct accel_lower_value *result)
{
	struct accel_lower_value left;
	struct accel_lower_value right;
	int64_t constant;
	int opcode;

	if (!accel_lower_expression(
		lower,
		expression->val.binary.expr[0],
		ACCEL_IR_VOID,
		&left)) {
		return false;
	}

	if (expression->type == HIR_EXPR_DIV ||
	    expression->type == HIR_EXPR_MOD) {
		if (left.type != ACCEL_IR_I32) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (!hir_opt_expr_constant_i64(
			expression->val.binary.expr[1],
			&constant)) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (constant <= 0 || constant > INT_MAX) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (!accel_lower_constant_i32(
			lower,
			(int32_t)constant,
			&right)) {
			return false;
		}

		if (expression->type == HIR_EXPR_DIV)
			opcode = ACCEL_IR_DIV_I32;
		else
			opcode = ACCEL_IR_MOD_I32;
	} else if (expression->type == HIR_EXPR_SHL ||
		   expression->type == HIR_EXPR_SHR) {
		if (left.type != ACCEL_IR_I32) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (!hir_opt_expr_constant_i64(
			expression->val.binary.expr[1],
			&constant)) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (constant < 0 || constant > 31) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
		if (!accel_lower_constant_i32(
			lower,
			(int32_t)constant,
			&right)) {
			return false;
		}

		if (expression->type == HIR_EXPR_SHL)
			opcode = ACCEL_IR_SHIFT_LEFT;
		else
			opcode = ACCEL_IR_SHIFT_RIGHT_LOGICAL;
	} else {
		if (!accel_lower_expression(
			lower,
			expression->val.binary.expr[1],
			ACCEL_IR_VOID,
			&right)) {
			return false;
		}
		if (left.type != right.type) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}

		/* Select the typed operation without introducing conversions. */
		switch (expression->type) {
		case HIR_EXPR_PLUS:
			opcode = ACCEL_IR_ADD;
			break;
		case HIR_EXPR_MINUS:
			opcode = ACCEL_IR_SUB;
			break;
		case HIR_EXPR_MUL:
			opcode = ACCEL_IR_MUL;
			break;
		case HIR_EXPR_AND:
			opcode = ACCEL_IR_BIT_AND;
			break;
		case HIR_EXPR_OR:
			opcode = ACCEL_IR_BIT_OR;
			break;
		case HIR_EXPR_XOR:
			opcode = ACCEL_IR_BIT_XOR;
			break;
		default:
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}
	}

	if (left.type != ACCEL_IR_I32 && left.type != ACCEL_IR_F32) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}
	if ((opcode == ACCEL_IR_BIT_AND ||
	     opcode == ACCEL_IR_BIT_OR ||
	     opcode == ACCEL_IR_BIT_XOR ||
	     opcode == ACCEL_IR_SHIFT_LEFT ||
	     opcode == ACCEL_IR_SHIFT_RIGHT_LOGICAL ||
	     opcode == ACCEL_IR_DIV_I32 ||
	     opcode == ACCEL_IR_MOD_I32) &&
	    left.type != ACCEL_IR_I32) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	if (!accel_lower_emit_binary(
		lower,
		opcode,
		left.type,
		left,
		right,
		result)) {
		return false;
	}

	return accel_lower_coerce(lower, *result, expected_type, result);
}

/* Lower signed/ordered comparisons to an explicit Boolean IR result. */
static bool
accel_lower_comparison(
	struct accel_kernel_lower *lower,
	const struct hir_expr *expression,
	struct accel_lower_value *result)
{
	struct accel_lower_value left;
	struct accel_lower_value right;
	int opcode;

	if (!accel_lower_expression(
		lower,
		expression->val.binary.expr[0],
		ACCEL_IR_VOID,
		&left)) {
		return false;
	}
	if (!accel_lower_expression(
		lower,
		expression->val.binary.expr[1],
		ACCEL_IR_VOID,
		&right)) {
		return false;
	}
	if (left.type != right.type) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}
	if (left.type != ACCEL_IR_I32 && left.type != ACCEL_IR_F32) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	/* Preserve signed integer and ordered/unordered float semantics by opcode. */
	switch (expression->type) {
	case HIR_EXPR_EQ:
		opcode = ACCEL_IR_COMPARE_EQ;
		break;
	case HIR_EXPR_NEQ:
		opcode = ACCEL_IR_COMPARE_NE;
		break;
	case HIR_EXPR_LT:
		opcode = ACCEL_IR_COMPARE_LT;
		break;
	case HIR_EXPR_LTE:
		opcode = ACCEL_IR_COMPARE_LTE;
		break;
	case HIR_EXPR_GT:
		opcode = ACCEL_IR_COMPARE_GT;
		break;
	case HIR_EXPR_GTE:
		opcode = ACCEL_IR_COMPARE_GTE;
		break;
	default:
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Invalid accelerator comparison HIR."));
		return false;
	}

	return accel_lower_emit_binary(
		lower,
		opcode,
		ACCEL_IR_BOOL,
		left,
		right,
		result);
}

/* Lower one supported Packed element read. */
static bool
accel_lower_subscript(
	struct accel_kernel_lower *lower,
	const struct hir_expr *expression,
	struct accel_lower_value *result)
{
	struct accel_lower_value index;
	const char *symbol;
	int catalog_index;
	int program_index;
	int value_type;

	symbol = accel_compile_term_symbol(expression->val.binary.expr[0]);
	catalog_index = accel_compile_buffer_catalog_index(lower->state, symbol);
	if (catalog_index < 0) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	program_index = lower->state->buffer[catalog_index].program_binding;
	if (program_index < 0) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Accelerator load has no buffer binding."));
		return false;
	}

	value_type = accel_compile_ir_type_for_packed(
		lower->state->buffer[catalog_index].object->element_kind);
	if (!accel_lower_index(
		lower,
		expression->val.binary.expr[1],
		&index)) {
		return false;
	}

	return accel_lower_emit_load(
		lower,
		(uint32_t)program_index,
		value_type,
		index,
		result);
}

/* Lower one counter-affine source index into checked dispatch arithmetic. */
static bool
accel_lower_index(
	struct accel_kernel_lower *lower,
	const struct hir_expr *expression,
	struct accel_lower_value *result)
{
	struct hir_affine_index affine;
	struct accel_lower_value index;
	struct accel_lower_value adjustment;
	const struct hir_expr *start;
	const char *symbol;
	int64_t constant;
	int scalar_index;

	if (!hir_opt_normalize_index(
		expression,
		lower->loop->val.for_.counter_symbol,
		&affine)) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Failed to normalize accelerator index."));
		return false;
	}
	if (affine.kind != HIR_AFFINE_COUNTER_OFFSET) {
		accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
		return false;
	}

	if (!accel_lower_global_index(lower, &index))
		return false;

	start = lower->loop->val.for_.start;
	if (hir_opt_expr_constant_i64(start, &constant)) {
		if (constant < 0 || constant > INT_MAX) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (constant != 0) {
			if (!accel_lower_constant_i32(
				lower,
				(int32_t)constant,
				&adjustment)) {
				return false;
			}
			if (!accel_lower_index_adjust(
				lower,
				index,
				adjustment,
				false,
				&index)) {
				return false;
			}
		}
	} else {
		/* Dynamic starts are limited to one immutable int parameter. */
		while (start != NULL && start->type == HIR_EXPR_PAR)
			start = start->val.unary.expr;
		symbol = accel_compile_term_symbol(start);
		scalar_index = accel_compile_scalar_index(lower->state, symbol);
		if (scalar_index < 0 || lower->state->scalar[scalar_index].reassigned) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (lower->state->scalar[scalar_index].value_type != ACCEL_IR_I32) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (!accel_lower_uniform(
			lower,
			(uint32_t)scalar_index,
			&adjustment)) {
			return false;
		}
		if (!accel_lower_index_adjust(
			lower,
			index,
			adjustment,
			false,
			&index)) {
			return false;
		}
	}

	if (affine.invariant_symbol != NULL) {
		scalar_index = accel_compile_scalar_index(
			lower->state,
			affine.invariant_symbol);
		if (scalar_index < 0 || lower->state->scalar[scalar_index].reassigned) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (lower->state->scalar[scalar_index].value_type != ACCEL_IR_I32) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_RANGE);
			return false;
		}
		if (!accel_lower_uniform(
			lower,
			(uint32_t)scalar_index,
			&adjustment)) {
			return false;
		}
		if (!accel_lower_index_adjust(
			lower,
			index,
			adjustment,
			affine.invariant_sign < 0,
			&index)) {
			return false;
		}
	}

	if (affine.offset != 0) {
		if (!accel_lower_constant_i32(
			lower,
			(int32_t)affine.offset,
			&adjustment)) {
			return false;
		}
		if (!accel_lower_index_adjust(
			lower,
			index,
			adjustment,
			false,
			&index)) {
			return false;
		}
	}

	*result = index;

	return true;
}

/* Resolve an immutable scalar parameter or current loop-local SSA value. */
static bool
accel_lower_symbol(
	struct accel_kernel_lower *lower,
	const char *symbol,
	struct accel_lower_value *result)
{
	struct hir_local *local;
	int scalar_index;

	if (symbol == NULL) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Malformed accelerator symbol HIR."));
		return false;
	}
	if (strcmp(symbol, lower->loop->val.for_.counter_symbol) == 0) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	scalar_index = accel_compile_scalar_index(lower->state, symbol);
	if (scalar_index >= 0) {
		if (lower->state->scalar[scalar_index].reassigned) {
			accel_compile_decline(
				lower->state,
				ACCEL_DECLINE_EXPRESSION);
			return false;
		}

		return accel_lower_uniform(
			lower,
			(uint32_t)scalar_index,
			result);
	}

	local = accel_compile_find_local(lower->state->base.func_block, symbol);
	if (local == NULL || local->index < 0 || local->index >= ACCEL_MAX_LOCALS) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}
	if (!lower->has_local[local->index]) {
		accel_compile_decline(
			lower->state,
			ACCEL_DECLINE_EXPRESSION);
		return false;
	}

	result->value = lower->local_value[local->index];
	result->type = lower->local_type[local->index];

	return true;
}

/* Emit or reuse the uniform value for one scalar program binding. */
static bool
accel_lower_uniform(
	struct accel_kernel_lower *lower,
	uint32_t scalar_index,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;
	uint32_t binding_index;

	if (scalar_index >= lower->state->scalar_count) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Invalid accelerator scalar binding."));
		return false;
	}

	binding_index = lower->state->scalar[scalar_index].binding_index;
	if (lower->has_uniform[binding_index]) {
		result->value = lower->uniform_value[binding_index];
		result->type = lower->state->scalar[scalar_index].value_type;
		return true;
	}

	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_UNIFORM,
		lower->state->scalar[scalar_index].value_type);
	instruction.reference = binding_index;
	if (!accel_lower_append(lower, &instruction, result))
		return false;

	lower->uniform_value[binding_index] = result->value;
	lower->has_uniform[binding_index] = true;

	return true;
}

/* Emit or reuse this kernel's one-dimensional invocation index. */
static bool
accel_lower_global_index(
	struct accel_kernel_lower *lower,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;

	if (lower->has_global_index) {
		result->value = lower->global_index;
		result->type = ACCEL_IR_INDEX_U32;
		return true;
	}

	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_GLOBAL_INDEX,
		ACCEL_IR_INDEX_U32);
	if (!accel_lower_append(lower, &instruction, result))
		return false;

	lower->global_index = result->value;
	lower->has_global_index = true;

	return true;
}

/* Apply one signed I32 adjustment to dispatch-only INDEX_U32 arithmetic. */
static bool
accel_lower_index_adjust(
	struct accel_kernel_lower *lower,
	struct accel_lower_value base,
	struct accel_lower_value adjustment,
	bool subtract,
	struct accel_lower_value *result)
{
	int opcode;

	if (base.type != ACCEL_IR_INDEX_U32 ||
	    adjustment.type != ACCEL_IR_I32) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Invalid accelerator index arithmetic."));
		return false;
	}

	if (subtract)
		opcode = ACCEL_IR_SUB;
	else
		opcode = ACCEL_IR_ADD;

	return accel_lower_emit_binary(
		lower,
		opcode,
		ACCEL_IR_INDEX_U32,
		base,
		adjustment,
		result);
}

/* Emit one exact signed 32-bit constant. */
static bool
accel_lower_constant_i32(
	struct accel_kernel_lower *lower,
	int32_t value,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;

	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_CONST_I32,
		ACCEL_IR_I32);
	instruction.literal_bits = (uint32_t)value;

	return accel_lower_append(lower, &instruction, result);
}

/* Emit one exact IEEE-754 single-precision bit pattern. */
static bool
accel_lower_constant_f32(
	struct accel_kernel_lower *lower,
	float value,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;
	uint32_t bits;

	memcpy(&bits, &value, sizeof(bits));
	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_CONST_F32,
		ACCEL_IR_F32);
	instruction.literal_bits = bits;

	return accel_lower_append(lower, &instruction, result);
}

/* Emit one two-operand typed instruction. */
static bool
accel_lower_emit_binary(
	struct accel_kernel_lower *lower,
	int opcode,
	int result_type,
	struct accel_lower_value left,
	struct accel_lower_value right,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;

	accel_lower_instruction_init(&instruction, opcode, result_type);
	instruction.operand[0] = left.value;
	instruction.operand[1] = right.value;

	return accel_lower_append(lower, &instruction, result);
}

/* Emit one typed buffer load through an INDEX_U32 address. */
static bool
accel_lower_emit_load(
	struct accel_kernel_lower *lower,
	uint32_t buffer_index,
	int result_type,
	struct accel_lower_value index,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;

	if (index.type != ACCEL_IR_INDEX_U32) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Invalid accelerator buffer index type."));
		return false;
	}

	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_BUFFER_LOAD,
		result_type);
	instruction.operand[0] = index.value;
	instruction.reference = buffer_index;

	return accel_lower_append(lower, &instruction, result);
}

/* Emit one typed buffer store through an INDEX_U32 address. */
static bool
accel_lower_emit_store(
	struct accel_kernel_lower *lower,
	uint32_t buffer_index,
	struct accel_lower_value index,
	struct accel_lower_value value)
{
	struct accel_ir_instruction instruction;
	struct accel_lower_value unused;

	if (index.type != ACCEL_IR_INDEX_U32) {
		accel_compile_error(
			lower->state,
			lower->loop->line,
			N_TR("Invalid accelerator buffer index type."));
		return false;
	}

	accel_lower_instruction_init(
		&instruction,
		ACCEL_IR_BUFFER_STORE,
		ACCEL_IR_VOID);
	instruction.operand[0] = index.value;
	instruction.operand[1] = value.value;
	instruction.reference = buffer_index;

	return accel_lower_append(lower, &instruction, &unused);
}

/* Preserve exact types or materialize a Noct integer Boolean as 0/1. */
static bool
accel_lower_coerce(
	struct accel_kernel_lower *lower,
	struct accel_lower_value value,
	int expected_type,
	struct accel_lower_value *result)
{
	struct accel_ir_instruction instruction;
	struct accel_lower_value one;
	struct accel_lower_value zero;

	if (expected_type == ACCEL_IR_VOID || expected_type == value.type) {
		*result = value;
		return true;
	}

	if (value.type == ACCEL_IR_BOOL && expected_type == ACCEL_IR_I32) {
		if (!accel_lower_constant_i32(lower, 1, &one))
			return false;
		if (!accel_lower_constant_i32(lower, 0, &zero))
			return false;

		accel_lower_instruction_init(
			&instruction,
			ACCEL_IR_SELECT,
			ACCEL_IR_I32);
		instruction.operand[0] = value.value;
		instruction.operand[1] = one.value;
		instruction.operand[2] = zero.value;

		return accel_lower_append(lower, &instruction, result);
	}

	accel_compile_decline(lower->state, ACCEL_DECLINE_EXPRESSION);

	return false;
}

/* Append one initialized instruction and classify builder failure. */
static bool
accel_lower_append(
	struct accel_kernel_lower *lower,
	struct accel_ir_instruction *instruction,
	struct accel_lower_value *result)
{
	uint32_t value;

	if (!accel_ir_builder_append(&lower->builder, instruction, &value)) {
		if (lower->builder.limit_exceeded) {
			accel_compile_decline(lower->state, ACCEL_DECLINE_LIMIT);
		} else {
			accel_compile_error(
				lower->state,
				lower->loop->line,
				N_TR("Out of memory while building accelerator IR."));
		}

		return false;
	}

	if (result != NULL) {
		result->value = value;
		result->type = instruction->result_type;
	}

	return true;
}

/* Initialize all unused instruction fields to explicit sentinels. */
static void
accel_lower_instruction_init(
	struct accel_ir_instruction *instruction,
	int opcode,
	int result_type)
{
	memset(instruction, 0, sizeof(*instruction));
	instruction->opcode = opcode;
	instruction->result_type = result_type;
	instruction->result = ACCEL_IR_VALUE_NONE;
	instruction->operand[0] = ACCEL_IR_VALUE_NONE;
	instruction->operand[1] = ACCEL_IR_VALUE_NONE;
	instruction->operand[2] = ACCEL_IR_VALUE_NONE;
	instruction->reference = ACCEL_IR_REFERENCE_NONE;
}

/* Map supported Packed storage kinds to typed scalar IR values. */
static int
accel_compile_ir_type_for_packed(
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

/* Map checked Noct scalar tags to initial accelerator scalar types. */
static int
accel_compile_ir_type_for_noct(
	int value_type)
{
	if (value_type == NOCT_VALUE_INT)
		return ACCEL_IR_I32;
	if (value_type == NOCT_VALUE_FLOAT)
		return ACCEL_IR_F32;

	return ACCEL_IR_VOID;
}
