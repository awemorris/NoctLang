/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Language Runtime
 */

#include <noct/noct.h>
#include "ast.h"
#include "hir.h"
#include "lir.h"
#include "runtime.h"
#include "intrinsics.h"
#include "interpreter.h"
#include "jit.h"
#include "gc.h"
#include "objectmodel.h"
#include "atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

/* False assertions. */
#define NOT_IMPLEMENTED		0
#define NEVER_COME_HERE		0
#define PINNED_VAR_NOT_FOUND	0

/* Finalizer. */
struct rt_vm_finalizer {
	void (*finalizer)(void *userdata);
	void *userdata;
	struct rt_vm_finalizer *next;
};

/* Forward declarations. */
static void rt_free_func(struct rt_env *rt, struct rt_func *func);
static bool rt_register_lir(struct rt_env *rt, struct lir_func *lir);
static bool rt_register_bytecode_func(struct rt_env *rt, uint8_t *data, size_t size, uint32_t *pos, char *file_name, char *init_name_out, size_t init_name_size);
static const char *rt_read_bytecode_line(uint8_t *data, size_t size, uint32_t *pos);
static bool rt_enter_frame(struct rt_env *env, struct rt_func *func);
static void rt_report_jit_result(struct rt_func *func, bool success, const char *reason);
static void rt_report_jit_lifecycle(const char *operation, bool success);
static void rt_invalidate_jit_entries(struct rt_vm *vm);
static bool rt_commit_jit(struct rt_env *env);
static void rt_leave_frame(struct rt_env *env);
static bool rt_init_global(struct rt_env *env);
static void rt_cleanup_global(struct rt_env *env);
static bool rt_expand_global(struct rt_env *env);

static void
rt_report_jit_result(
	struct rt_func *func,
	bool success,
	const char *reason)
{
	if (getenv("NOCT_JIT_DEBUG") != NULL) {
		fprintf(stderr,
			"noct-jit: %s: %s",
			func->name,
			success ? "compiled" : "fallback");
		if (!success && reason != NULL && reason[0] != '\0')
			fprintf(stderr, " reason=%s", reason);
		fputc('\n', stderr);
	}
}

static void
rt_report_jit_lifecycle(
	const char *operation,
	bool success)
{
	if (getenv("NOCT_JIT_DEBUG") != NULL) {
		fprintf(stderr,
			"noct-jit-lifecycle: %s status=%s\n",
			operation,
			success ? "ok" : "failed");
	}
}

static void
rt_invalidate_jit_entries(
	struct rt_vm *vm)
{
	struct rt_func *func;

	for (func = vm->func_list; func != NULL; func = func->next) {
		func->jit_code = NULL;
		func->call_count = -1;
	}
}

/*
 * Initialization
 */

/*
 * Create a virtual machine.
 */
bool
rt_create_vm(
	struct rt_vm **vm,
	struct rt_env **default_env,
	struct rt_config *config)
{
	*vm = NULL;
	*default_env = NULL;

	/* Allocate a struct rt_vm. */
	*vm = noct_malloc(sizeof(struct rt_vm));
	if (*vm == NULL) {
		*default_env = NULL;
		return false;
	}
	memset(*vm, 0, sizeof(struct rt_vm));

	/* Copy the config if specified. */
	if (config != NULL)
		memcpy(&(*vm)->config, config, sizeof(struct rt_config));
	else
		noct_set_default_config(&(*vm)->config);

	/* Allocate a struct rt_env. */
	*default_env = noct_malloc(sizeof(struct rt_env));
	if (*default_env == NULL) {
		noct_free(*vm);
		*vm = NULL;
		return false;
	}
	memset(*default_env, 0, sizeof(struct rt_env));
	(*default_env)->vm = *vm;
	(*vm)->env_list = *default_env;
	/* Enter the initial stack frame. */
	(*default_env)->cur_frame_index = 0;
	(*default_env)->frame = &(*default_env)->frame_alloc[0];
	(*default_env)->frame->tmpvar = &(*default_env)->frame->tmpvar_alloc[0];
	(*default_env)->frame->tmpvar_size = RT_TMPVAR_MAX;
	memset((*default_env)->frame->tmpvar, 0, sizeof(struct rt_value) * RT_TMPVAR_MAX);

	/* Initialize for GC. */
	om_init_env(*default_env);

	/* Initialize the global variables. */
	if (!rt_init_global(*default_env)) {
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}

	/* Initialize the garbage collector. */
	if (!rt_gc_init(*vm)) {
		rt_cleanup_global(*default_env);
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}

	/* Register the intrinsics. */
	if (!rt_register_intrinsics(*default_env)) {
		rt_cleanup_global(*default_env);
		rt_gc_cleanup(*vm);
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}
	return true;
}

/*
 * Destroy a virtual machine.
 */
bool
rt_destroy_vm(
	struct rt_vm *vm)
{
	struct rt_env *env, *next_env;
	struct rt_func *func, *next_func;
	struct rt_vm_finalizer *finalizer;
	struct rt_vm_finalizer *next_finalizer;
	bool jit_cleanup_succeeded = true;

	/* Free the JIT region. */
	if (vm->config.jit_enable && !jit_free(vm->env_list))
		jit_cleanup_succeeded = false;

	/* Run VM-owned native finalizers while the VM is still usable. */
	finalizer = vm->vm_finalizer_list;
	while (finalizer != NULL) {
		next_finalizer = finalizer->next;
		finalizer->finalizer(finalizer->userdata);
		noct_free(finalizer);
		finalizer = next_finalizer;
	}
	vm->vm_finalizer_list = NULL;

	/* Free global variables. */
	rt_cleanup_global(vm->env_list);

	/* Cleanup the garbage collector. */
	rt_gc_cleanup(vm);

	/* Free functions. */
	func = vm->func_list;
	while (func != NULL) {
		next_func = func->next;
		rt_free_func(vm->env_list, func);
		func = next_func;
	}

	/* Free thread environments. */
	env = vm->env_list;
	while (env != NULL) {
		next_env = env->next;
		noct_free(env);
		env = next_env;
	}

	if (vm->config.jit_enable)
		rt_report_jit_lifecycle("destroy", jit_cleanup_succeeded);

	noct_free(vm);

	return jit_cleanup_succeeded;
}

/* Free a function. */
static void
rt_free_func(
	struct rt_env *env,
	struct rt_func *func)
{
	int i;

	UNUSED_PARAMETER(env);

	noct_free(func->name);
	func->name = NULL;

	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (func->param_name[i] != NULL) {
			noct_free(func->param_name[i]);
			func->param_name[i] = NULL;
		}
	}
	noct_free(func->file_name);
	noct_free(func->bytecode);

	if (func->jit_code != NULL)
		func->jit_code = NULL;

	noct_free(func);
}

/*
 * Create an environment for a secondary thread.
 */
#if defined(NOCT_USE_MULTITHREAD)
bool
rt_create_thread_env(
	struct rt_env *prev_env,
	struct rt_env **new_env)
{
	struct rt_vm *vm;
	struct rt_env *env;

	vm = prev_env->vm;

	/* Reuse a parked environment when possible. */
	atomic_spin_lock(&vm->env_free_lock);
	env = vm->env_free_list;
	if (env != NULL)
		vm->env_free_list = env->free_next;
	atomic_spin_unlock(&vm->env_free_lock);

	if (env == NULL) {
		env = noct_calloc(1, sizeof(struct rt_env));
		if (env == NULL) {
			rt_out_of_memory(prev_env);
			return false;
		}
		env->vm = vm;
		env->cur_frame_index = 0;
		env->frame = &env->frame_alloc[0];
		env->frame->tmpvar = &env->frame->tmpvar_alloc[0];
		env->frame->tmpvar_size = RT_TMPVAR_MAX;

		atomic_spin_lock(&vm->env_free_lock);
		env->next = vm->env_list;
		vm->env_list = env;
		atomic_spin_unlock(&vm->env_free_lock);
	} else {
		env->file_name[0] = '\0';
		env->error_message[0] = '\0';
		env->free_next = NULL;
	}

	/* Succeeded. The env is parked until rt_attach_thread_env(). */
	*new_env = env;

	return true;
}
#endif

/* Adopt an environment in the current thread. */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_attach_thread_env(
	struct rt_env *env)
{
	om_init_env(env);
}
#endif

/*
 * Release an environment.
 */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_release_thread_env(
	struct rt_env *env)
{
	struct rt_vm *vm;

	assert(env != NULL);

	vm = env->vm;
	atomic_spin_lock(&vm->env_free_lock);
	env->free_next = vm->env_free_list;
	vm->env_free_list = env;
	atomic_spin_unlock(&vm->env_free_lock);
}
#endif

/* Detach the current thread's environment for later reuse. */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_detach_thread_env(
	struct rt_env *env)
{
	struct rt_vm *vm;

	assert(env != NULL);

	vm = env->vm;
	env->cur_frame_index = 0;
	env->frame = &env->frame_alloc[0];
	env->frame->tmpvar = &env->frame->tmpvar_alloc[0];
	env->frame->tmpvar_size = RT_TMPVAR_MAX;
	env->frame->pinned_count = 0;
	memset(env->frame->tmpvar_alloc, 0, sizeof(env->frame->tmpvar_alloc));

	om_enter_blocking(env);

	atomic_spin_lock(&vm->env_free_lock);
	env->free_next = vm->env_free_list;
	vm->env_free_list = env;
	atomic_spin_unlock(&vm->env_free_lock);
}
#endif

/*
 * Compilation
 */

/*
 * Register functions from a souce text.
 */
bool
rt_register_source(
	struct rt_env *env,
	const char *file_name,
	const char *source_text)
{
	char init_func_name[256];
	struct hir_block *hfunc;
	struct lir_func *lfunc;
	uint32_t i, func_count;
	bool is_succeeded;

	is_succeeded = false;
	init_func_name[0] = '\0';

	do {
		/* Do parse and build AST. */
		if (!ast_build(file_name, source_text)) {
			strncpy(env->file_name, ast_get_file_name(), sizeof(env->file_name) - 1);
			env->line = ast_get_error_line();
			rt_error(env, "%s", ast_get_error_message());
			break;
		}

		/* Transform AST to HIR. */
		if (!hir_build()) {
			strncpy(env->file_name, hir_get_file_name(), sizeof(env->file_name) - 1);
			env->line = hir_get_error_line();
			rt_error(env, "%s", hir_get_error_message());
			break;
		}

		/* Free intermediates. */
		ast_cleanup();

		/* Configure LIR construction for this VM. */
		lir_set_optimize_level(env->vm->config.optimize_level);
		lir_set_lineinfo(env->vm->config.line_info);

		/* For each function. */
		func_count = hir_get_function_count();
		for (i = 0; i < func_count; i++) {
			/* Run the HIR optimizer. */
			hfunc = hir_get_function(i);
			if (!hir_optimize_func(hfunc,
					       env->vm->config.optimize_level,
					       env->vm->config.simd_info)) {
				rt_error(env, "%s", hir_get_error_message());
				break;
			}

			/* Transform HIR to LIR (bytecode). */
			if (!lir_build(hfunc, &lfunc)) {
				strncpy(env->file_name, lir_get_file_name(), sizeof(env->file_name) - 1);
				env->line = lir_get_error_line();
				rt_error(env, "%s", lir_get_error_message());
				break;
			}

			/* Make a function object. */
			if (!rt_register_lir(env, lfunc))
				break;

			/* Remember a load-time init function. */
			if (strncmp(lfunc->func_name, "$init.", 6) == 0) {
				strncpy(init_func_name, lfunc->func_name,
					sizeof(init_func_name) - 1);
			}

			/* Free a LIR. */
			lir_cleanup(lfunc);
		}
		if (i != func_count)
			break;

		/* Free intermediates. */
		hir_cleanup();

		/* JIT-commit. */
		if (!rt_commit_jit(env))
			break;

		/* Auto-execute the load-time init function ($init section). */
		if (init_func_name[0] != '\0') {
			struct rt_value init_ret;
			if (!rt_call_with_name(env, init_func_name, 0, NULL, &init_ret))
				break;
		}

		is_succeeded = true;
	} while (0);

	return is_succeeded;
}

/* Register a function from LIR. */
static bool
rt_register_lir(
	struct rt_env *env,
	struct lir_func *lir)
{
	struct rt_func *func;
	struct rt_value global;
	uint32_t i;

	func = noct_malloc(sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	memset(func, 0, sizeof(struct rt_func));

	func->name = noct_strdup(lir->func_name);
	if (func->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	func->param_count = lir->param_count;
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		func->param_type[i] = -1;
		func->param_packed_type[i] = -1;
		func->param_restricted[i] = false;
	}
	for (i = 0; i < lir->param_count; i++) {
		func->param_type[i] = lir->param_type[i];
		func->param_packed_type[i] = lir->param_packed_type[i];
		func->param_restricted[i] = lir->param_restricted[i];
	}

	func->return_type = lir->return_type;
	func->return_packed_type = lir->return_packed_type;
	func->return_type_checked = lir->return_type_checked;

	for (i = 0; i < lir->param_count; i++) {
		func->param_name[i] = noct_strdup(lir->param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(env);
			return false;
		}
	}

	func->bytecode_size = lir->bytecode_size;
	if (func->bytecode_size != 0) {
		func->bytecode = noct_malloc((size_t)lir->bytecode_size);
		if (func->bytecode == NULL) {
			rt_out_of_memory(env);
			return false;
		}
		memcpy(func->bytecode, lir->bytecode, (size_t)lir->bytecode_size);
	}

	func->tmpvar_size = lir->tmpvar_size;
	func->has_vector_ops = lir->has_vector_ops;
	func->has_fma_ops = lir->has_fma_ops;

	func->file_name = noct_strdup(lir->file_name);
	if (func->file_name == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Insert a global variable. */
	global.type = NOCT_VALUE_FUNC;
	global.val.func = func;
	if (!rt_set_global(env, func->name, &global))
		return false;

	if (env->vm->config.jit_enable) {
		if (!jit_build(env, func)) {
			rt_report_jit_result(func, false, env->error_message);
			func->jit_code = NULL;
			func->call_count = -1;
			env->error_message[0] = '\0';
			env->line = 0;
		} else {
			rt_report_jit_result(func, true, NULL);
			env->vm->is_jit_dirty = true;
		}
	}

	/* Link. */
	func->next = env->vm->func_list;
	env->vm->func_list = func;

	return true;
}

static bool
rt_parse_bytecode_u32(
	const char *text,
	uint32_t *value)
{
	uint32_t result;
	uint32_t digit;

	if (text == NULL || text[0] == '\0')
		return false;

	result = 0;
	while (*text != '\0') {
		if (*text < '0' || *text > '9')
			return false;
		digit = (uint32_t)(*text - '0');
		if (result > (UINT32_MAX - digit) / 10)
			return false;
		result = result * 10 + digit;
		text++;
	}
	*value = result;

	return true;
}

/*
 * Register functions from bytecode data.
 *
 * data must start from "Noct Bytecode".
 * Do not pass data that starts from a shebang "#!".
 */
bool
rt_register_bytecode(
	struct rt_env *env,
	size_t size,
	uint8_t *data)
{
	char *file_name;
	const char *line;
	uint32_t pos, func_count, i;
	bool succeeded;
	char init_func_name[256];

	file_name = NULL;
	pos = 0;
	succeeded = false;
	init_func_name[0] = '\0';
	do {
		/* Check the magic. */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL || strcmp(line, "Noct Bytecode 1.0") != 0)
			break;

		/* Check "Source". */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL || strcmp(line, "Source") != 0)
			break;

		/* Get a source file name. */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL)
			break;
		file_name = noct_strdup(line);
		if (file_name == NULL)
			break;

		/* Check "Number Of Functions". */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL || strcmp(line, "Number Of Functions") != 0)
			break;

		/* Get a number of functions. */
		line = rt_read_bytecode_line(data, size, &pos);
		if (!rt_parse_bytecode_u32(line, &func_count))
			break;

		/* Read functions. */
		for (i = 0; i < func_count; i++) {
			if (!rt_register_bytecode_func(env,
						       data,
						       size,
						       &pos,
						       file_name,
						       init_func_name,
						       sizeof(init_func_name)))
				break;
		}
		if (i != func_count)
			break;

		if (!rt_commit_jit(env))
			break;

		succeeded = true;
	} while (0);

	if (file_name != NULL)
		noct_free(file_name);

	if (!succeeded) {
		if (env->error_message[0] == '\0')
			noct_error(env, N_TR("Failed to load bytecode data."));
		return false;
	}

	/* Auto-execute the load-time init function ($init section). */
	if (init_func_name[0] != '\0') {
		struct rt_value init_ret;
		if (!rt_call_with_name(env, init_func_name, 0, NULL, &init_ret))
			return false;
	}

	return true;
}

static bool
rt_register_bytecode_func(
	struct rt_env *env,
	uint8_t *data,
	size_t size,
	uint32_t *pos,
	char *file_name,
	char *init_name_out,
	size_t init_name_size)
{
	struct lir_func lfunc;
	const char *line;
	uint32_t i;
	bool succeeded;
	char *function_file_name;
	unsigned int optional_sections;
	bool optional_succeeded;
	enum {
		BYTECODE_PARAM_TYPES = 1U << 0,
		BYTECODE_PARAM_PACKED_TYPES = 1U << 1,
		BYTECODE_PARAM_RESTRICTED = 1U << 2,
		BYTECODE_RETURN_TYPE = 1U << 3,
		BYTECODE_VECTOR_OPS = 1U << 4,
		BYTECODE_FMA_OPS = 1U << 5
	};

	memset(&lfunc, 0, sizeof(lfunc));
	function_file_name = NULL;
	lfunc.file_name = file_name;
	lfunc.return_type = -1;
	lfunc.return_packed_type = -1;
	for (i = 0; i < LIR_PARAM_SIZE; i++) {
		lfunc.param_type[i] = -1;
		lfunc.param_packed_type[i] = -1;
		lfunc.param_restricted[i] = false;
	}

	succeeded = false;
	do {
		/* Check "Begin Function". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Begin Function") != 0)
			break;

		/* Check "Name". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Name") != 0)
			break;

		/* Get a function name. */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL)
			break;
		lfunc.func_name = noct_strdup(line);
		if (lfunc.func_name == NULL)
			break;

		/* Remember a load-time init function. */
		if (strncmp(lfunc.func_name, "$init.", 6) == 0) {
			strncpy(init_name_out, lfunc.func_name, init_name_size - 1);
			init_name_out[init_name_size - 1] = '\0';
		}

		/* Get the function's source name.  Old bytecode used the file-level
		 * source name only, so keep that form readable as well. */
		line = rt_read_bytecode_line(data, size, pos);
		if (line != NULL && strcmp(line, "Source") == 0) {
			line = rt_read_bytecode_line(data, size, pos);
			if (line == NULL)
				break;
			function_file_name = noct_strdup(line);
			if (function_file_name == NULL)
				break;
			lfunc.file_name = function_file_name;
			line = rt_read_bytecode_line(data, size, pos);
		}

		/* Check "Parameters". */
		if (line == NULL || strcmp(line, "Parameters") != 0)
			break;

		/* Get number of parameters. */
		line = rt_read_bytecode_line(data, size, pos);
		if (!rt_parse_bytecode_u32(line, &lfunc.param_count))
			break;
		if (lfunc.param_count > LIR_PARAM_SIZE ||
		    lfunc.param_count > NOCT_ARG_MAX)
			break;

		/* Get parameters. */
		for (i = 0; i < lfunc.param_count; i++) {
			line = rt_read_bytecode_line(data, size, pos);
			if (line == NULL)
				break;
			lfunc.param_name[i] = noct_strdup(line);
			if (lfunc.param_name[i] == NULL)
				break;
		}
		if (i != lfunc.param_count)
			break;

		/* Read every optional metadata section up to "Temporary Size".
		 * The writer has a canonical order, but the format does not require
		 * the reader to encode that order as a chain of one-shot tests. */
		optional_sections = 0;
		optional_succeeded = true;
		line = rt_read_bytecode_line(data, size, pos);
		while (line != NULL && strcmp(line, "Temporary Size") != 0) {
			unsigned int section;

			if (strcmp(line, "Parameter Types") == 0)
				section = BYTECODE_PARAM_TYPES;
			else if (strcmp(line, "Parameter Packed Types") == 0)
				section = BYTECODE_PARAM_PACKED_TYPES;
			else if (strcmp(line, "Parameter Restricted") == 0)
				section = BYTECODE_PARAM_RESTRICTED;
			else if (strcmp(line, "Return Type") == 0)
				section = BYTECODE_RETURN_TYPE;
			else if (strcmp(line, "Vector Ops") == 0)
				section = BYTECODE_VECTOR_OPS;
			else if (strcmp(line, "FMA Ops") == 0)
				section = BYTECODE_FMA_OPS;
			else {
				optional_succeeded = false;
				break;
			}
			if ((optional_sections & section) != 0) {
				optional_succeeded = false;
				break;
			}
			optional_sections |= section;

			if (section == BYTECODE_PARAM_TYPES ||
			    section == BYTECODE_PARAM_PACKED_TYPES ||
			    section == BYTECODE_PARAM_RESTRICTED) {
				for (i = 0; i < lfunc.param_count; i++) {
					line = rt_read_bytecode_line(data, size, pos);
					if (line == NULL)
						break;
					if (section == BYTECODE_PARAM_TYPES)
						lfunc.param_type[i] = atoi(line);
					else if (section == BYTECODE_PARAM_PACKED_TYPES)
						lfunc.param_packed_type[i] = atoi(line);
					else
						lfunc.param_restricted[i] = atoi(line) != 0;
				}
				if (i != lfunc.param_count) {
					optional_succeeded = false;
					break;
				}
			} else if (section == BYTECODE_RETURN_TYPE) {
				line = rt_read_bytecode_line(data, size, pos);
				if (line == NULL) {
					optional_succeeded = false;
					break;
				}
				lfunc.return_type = atoi(line);
				line = rt_read_bytecode_line(data, size, pos);
				if (line == NULL) {
					optional_succeeded = false;
					break;
				}
				lfunc.return_packed_type = atoi(line);
				line = rt_read_bytecode_line(data, size, pos);
				if (line == NULL) {
					optional_succeeded = false;
					break;
				}
				lfunc.return_type_checked = atoi(line) != 0;
			} else {
				line = rt_read_bytecode_line(data, size, pos);
				if (line == NULL) {
					optional_succeeded = false;
					break;
				}
				if (section == BYTECODE_VECTOR_OPS)
					lfunc.has_vector_ops = atoi(line) != 0;
				else
					lfunc.has_fma_ops = atoi(line) != 0;
			}

			line = rt_read_bytecode_line(data, size, pos);
		}
		if (!optional_succeeded)
			break;

		/* "Temporary Size". */
		if (line == NULL || strcmp(line, "Temporary Size") != 0)
			break;

		/* Get a local size. */
		line = rt_read_bytecode_line(data, size, pos);
		if (!rt_parse_bytecode_u32(line, &lfunc.tmpvar_size) ||
		    lfunc.tmpvar_size > LIR_TMPVAR_MAX ||
		    lfunc.tmpvar_size > RT_TMPVAR_MAX)
			break;

		/* Check "Bytecode Size". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Bytecode Size") != 0)
			break;

		/* Get a bytecode size. */
		line = rt_read_bytecode_line(data, size, pos);
		if (!rt_parse_bytecode_u32(line, &lfunc.bytecode_size))
			break;

		/* Validate the raw payload and its textual terminator before making
		 * the function observable through the global table. */
		if ((size_t)*pos > size ||
		    (size_t)lfunc.bytecode_size > size - (size_t)*pos)
			break;
		lfunc.bytecode = data + *pos;
		(*pos) += lfunc.bytecode_size;
		if ((size_t)*pos >= size || data[*pos] != '\n')
			break;
		(*pos)++;

		/* Check "End Function". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "End Function") != 0)
			break;

		/* Load LIR. */
		if (!rt_register_lir(env, &lfunc))
			break;

		succeeded = true;
	} while (0);

	if (lfunc.func_name != NULL)
		noct_free(lfunc.func_name);
	if (function_file_name != NULL)
		noct_free(function_file_name);

	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (lfunc.param_name[i] != NULL)
			noct_free(lfunc.param_name[i]);
	}

	if (!succeeded) {
		noct_error(env, N_TR("Failed to load bytecode data."));
		return false;
	}

	return true;
}

/* Read a line from bytecode file data. */
static const char *
rt_read_bytecode_line(
	uint8_t *data,
	size_t size,
	uint32_t *pos)
{
	static char line[1024];
	uint32_t i;

	for (i = 0; i < sizeof(line) - 1; i++) {
		if (*pos >= size)
			return NULL;

		line[i] = (char)data[*pos];
		(*pos)++;
		if (line[i] == '\n') {
			line[i] = '\0';
			return line;
		}
	}
	line[i] = '\0'; // for secuity reason

	return NULL;
}

static struct rt_func *
rt_create_cfunc(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	bool (*cfunc_with_data)(struct rt_env *env, void *userdata),
	void *userdata)
{
	struct rt_func *func;
	uint32_t i;

	if (name == NULL || name[0] == '\0' || param_count > NOCT_ARG_MAX ||
	    (param_count != 0 && param_name == NULL) ||
	    (cfunc == NULL) == (cfunc_with_data == NULL)) {
		rt_error(env, N_TR("Invalid native function registration."));
		return NULL;
	}

	func = noct_calloc(1, sizeof(*func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return NULL;
	}

	func->name = noct_strdup(name);
	if (func->name == NULL)
		goto oom;

	func->param_count = param_count;
	func->return_type = -1;
	func->return_packed_type = -1;
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		func->param_type[i] = -1;
		func->param_packed_type[i] = -1;
	}
	for (i = 0; i < param_count; i++) {
		if (param_name[i] == NULL) {
			rt_error(env, N_TR("Invalid native function parameter name."));
			rt_free_func(env, func);
			return NULL;
		}
		func->param_name[i] = noct_strdup(param_name[i]);
		if (func->param_name[i] == NULL)
			goto oom;
	}

	func->cfunc = cfunc;
	func->cfunc_with_data = cfunc_with_data;
	func->cfunc_userdata = userdata;
	func->tmpvar_size = (uint32_t)param_count + 1;
	return func;

oom:
	rt_out_of_memory(env);
	rt_free_func(env, func);
	return NULL;
}

static bool
rt_publish_cfunc(
	struct rt_env *env,
	struct rt_func *func,
	struct rt_func **ret_func)
{
	struct rt_value global;

	global.type = NOCT_VALUE_FUNC;
	global.val.func = func;
	if (!rt_set_global(env, func->name, &global)) {
		rt_free_func(env, func);
		return false;
	}

	func->next = env->vm->func_list;
	env->vm->func_list = func;
	if (ret_func != NULL)
		*ret_func = func;
	return true;
}

bool
rt_register_cfunc(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	struct rt_func **ret_func)
{
	struct rt_func *func;

	func = rt_create_cfunc(env, name, param_count, param_name,
			       cfunc, NULL, NULL);
	return func != NULL && rt_publish_cfunc(env, func, ret_func);
}

bool
rt_register_cfunc_with_data(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env, void *userdata),
	void *userdata,
	struct rt_func **ret_func)
{
	struct rt_func *func;

	func = rt_create_cfunc(env, name, param_count, param_name,
			       NULL, cfunc, userdata);
	return func != NULL && rt_publish_cfunc(env, func, ret_func);
}

bool
rt_register_vm_finalizer(
	struct rt_env *env,
	void (*finalizer)(void *userdata),
	void *userdata)
{
	struct rt_vm_finalizer *entry;

	if (finalizer == NULL) {
		rt_error(env, N_TR("Invalid VM finalizer."));
		return false;
	}

	entry = noct_malloc(sizeof(*entry));
	if (entry == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	entry->finalizer = finalizer;
	entry->userdata = userdata;
	entry->next = env->vm->vm_finalizer_list;
	env->vm->vm_finalizer_list = entry;
	return true;
}

static bool
rt_commit_jit(struct rt_env *env)
{
	if (env->vm->config.jit_enable && env->vm->is_jit_dirty) {
		if (!jit_commit(env)) {
			rt_invalidate_jit_entries(env->vm);
			(void)jit_free(env);
			env->vm->is_jit_dirty = false;
			rt_error(env, N_TR("JIT memory protection failed."));
			rt_report_jit_lifecycle("publish", false);
			return false;
		}
		env->vm->is_jit_dirty = false;
		rt_report_jit_lifecycle("publish", true);
	}
	return true;
}

/*
 * Call
 */

/*
 * Call a function with a name.
 */
bool
rt_call_with_name(
	struct rt_env *env,
	const char *func_name,
	uint32_t arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	struct rt_value global;
	struct rt_func *func;
	bool func_ok;

	/* Search a function. */
	func_ok = false;
	do {
		if (!rt_check_global(env, func_name))
			break;

		if (!rt_get_global(env, func_name, &global))
			break;

		if (global.type != NOCT_VALUE_FUNC)
			break;

		func_ok = true;
	} while (0);

	if (!func_ok) {
		noct_error(env, N_TR("Cannot find function %s."), func_name);
		return false;
	}

	func = global.val.func;

	/* Call. */
	if (!rt_call(env, func, arg_count, arg, ret))
		return false;

	return true;
}

bool
rt_call(
	struct rt_env *env,
	struct rt_func *func,
	uint32_t arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	char old_file_name[256];
	uint32_t i;

	if (arg_count != func->param_count) {
		noct_error(env, N_TR("%s(): Function arguments not match."), func->name);
		return false;
	}

	/* Allocate a frame for this call. */
	if (!rt_enter_frame(env, func))
		return false;

	env->frame->arg_count = arg_count;

	/*
	 * Every exit below must pop the frame. Leaving it behind would
	 * keep its slots alive as GC roots after the values they refer
	 * to are gone, and would leave the frame index out of step with
	 * the real call depth.
	 */

	/* Pass the args. */
	for (i = 0; i < arg_count; i++)
		env->frame->tmpvar[i] = arg[i];

#if defined(NOCT_USE_MULTITHREAD)
	/* Make a safepoint. */
	om_safepoint(env);
#endif

	/* Run. */
	if (func->cfunc != NULL || func->cfunc_with_data != NULL) {
		/*
		 * Call an intrinsic or an FFI function implemented in C.
		 */
		if ((func->cfunc != NULL && !func->cfunc(env)) ||
		    (func->cfunc_with_data != NULL &&
		     !func->cfunc_with_data(env, func->cfunc_userdata))) {
			rt_leave_frame(env);
			return false;
		}
	} else {
		/*
		 * Call a Noct world function.
		 */

		/* Backup the old file name from the env. */
		strncpy(old_file_name, env->file_name, sizeof(old_file_name) - 1);

		/* Copy the new file name to the env. */
		strncpy(env->file_name, env->frame->func->file_name, sizeof(env->file_name) - 1);

		if (func->jit_code != NULL) {
			/*
			 * The function has a JIT-generated code. Call it.
			 */
			if (getenv("NOCT_JIT_DEBUG") != NULL)
				fprintf(stderr, "noct-jit: %s: native-entry\n",
					func->name);
			if (!func->jit_code(env)) {
				/*
				 * Native code returned false.
				 * Restore the old file name and exit with false.
				 */
				strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
				rt_leave_frame(env);
				return false;
			}
		} else {
			/*
			 * No JIT-generated code. Call the bytecode interpreter.
			 */
			if (!rt_visit_bytecode(env, func)) {
				/*
				 * Interpreter returned false.
				 * Restore the old file name and exit with false.
				 */
				strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
				rt_leave_frame(env);
				return false;
			}
		}

		/* Restore the old file name. */
		strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
	}

	/* Get a return value. */
	if (ret != NULL)
		*ret = env->frame->tmpvar[0];

	/* Succeeded. */
	rt_leave_frame(env);

	return true;
}

/* Enter a new calling frame. */
static bool
rt_enter_frame(
	struct rt_env *env,
	struct rt_func *func)
{
	struct rt_frame *frame;

	/*
	 * Check before incrementing so the frame index stays valid when
	 * the stack is full: the caller's error path still unwinds
	 * against its own (unchanged) frame.
	 */
	if (env->cur_frame_index + 1 >= RT_FRAME_MAX) {
		rt_error(env, N_TR("Stack overflow."));
		return false;
	}
	env->cur_frame_index++;

	frame = &env->frame_alloc[env->cur_frame_index];
	env->frame = frame;
	frame->func = func;
	frame->tmpvar = &frame->tmpvar_alloc[0];
	frame->tmpvar_size = func->tmpvar_size;
	frame->pinned_count = 0;

	/* We can't remove this due to GC. */
	memset(frame->tmpvar, 0, sizeof(struct rt_value) * (size_t)frame->tmpvar_size);

	return true;
}

/* Leave the current calling frame. */
static void
rt_leave_frame(
	struct rt_env *env)
{
	if (--env->cur_frame_index < 0) {
		rt_error(env, N_TR("Stack underflow."));
		abort();
	}

	env->frame = &env->frame_alloc[env->cur_frame_index];
}

/*
 * String
 */

/*
 * Make a string value.
 */
bool
rt_make_string(
	struct rt_env *env,
	struct rt_value *val,
	const char *data)
{
	size_t len;
	uint32_t hash;

	len = strlen(data) + 1; /* Including NUL. */
	hash = 0;
	if (!rt_make_string_with_hash(env, val, data, len, hash))
		return false;

	return true;
}

/*
 * Make a string value. (hash version)
 */
bool
rt_make_string_with_hash(
	struct rt_env *env,
	struct rt_value *val,
	const char *data,
	size_t len,		/* Including NUL */
	uint32_t hash)
{
	struct rt_string *rts;

	/* Allocate a string. */
	rts = rt_gc_alloc_string(env, data, len, hash);
	if (rts == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/*
	 * Here, this thread is "in-flight" and GC won't be executed
	 * in other threads.
	 */

	/* Setup a value. */
	val->type = NOCT_VALUE_STRING;
	val->val.str = rts;

	return true;
}

/*
 * Cache the hash of a string.
 */
void
rt_cache_string_hash(
	struct rt_string *rts)
{
	if (rts->hash == 0)
		rts->hash = noct_string_hash(rts->data);
}

/*
 * Get a string hash. (FNV-1a)
 */
uint32_t
rt_string_hash(
	const char *s)
{
	uint32_t hash = 2166136261u;
	while (*s) {
		hash ^= (uint8_t)*s++;
		hash *= 16777619u;
	}
	return hash;
}

/*
 * Get a string hash and a length. (FNV-1a)
 */
void
rt_string_hash_and_len(
	const char *s,
	uint32_t *hash,
	uint32_t *len)
{
	*len = 0;
	*hash = 2166136261u;
	while (*s) {
		*hash ^= (uint8_t)*s++;
		*hash *= 16777619u;
		*len = *len + 1;
	}
}

/*
 * Arrays and Dictionaries
 */

/*
 * Make an empty array.
 */
bool
rt_make_empty_array(
	struct rt_env *env,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_make_array(env, val))
		return false;

	return true;
}

/*
 * Get the size of an array.
 */
bool
rt_get_array_size(
	struct rt_env *env,
	struct rt_value *arr,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_array_size(env, arr, size))
		return false;

	return true;
}

/*
 * Retrieves an array element.
 */
bool
rt_get_array_elem(
	struct rt_env *env,
	struct rt_value *arr,
	size_t index,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_array(env, arr, index, val))
		return false;

	return true;
}

/*
 * Stores an value to an array.
 */
bool
rt_set_array_elem(
	struct rt_env *env,
	struct rt_value *arr,
	size_t index,
	NoctValue *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_array(env, arr, index, val))
		return false;

	return true;
}

/*
 * Resizes an array.
 */
bool
rt_resize_array(
	struct rt_env *env,
	struct rt_value *arr,
	size_t size)
{
	/* Delegate to the object model implementation. */
	if (!om_resize_array(env, arr, size))
		return false;

	return true;
}

/*
 * Make a shallow copy of an array.
 */
bool
rt_make_array_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	/* Delegate to the object model implementation. */
	if (!om_copy_array(env, dst, src))
		return false;

	return true;
}

/*
 * Make an empty dictionary.
 */
bool
rt_make_empty_dict(
	struct rt_env *env,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_make_dict(env, val))
		return false;

	return true;
}

/*
 * Get the size of a dictionary.
 */
bool
rt_get_dict_size(
	struct rt_env *env,
	struct rt_value *dict,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_dict_size(env, dict, size))
		return false;

	return true;
}

/*
 * Get the allocation size of a dictionary.
 */
bool
rt_get_dict_alloc_size(
	struct rt_env *env,
	struct rt_value *dict,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_dict_alloc_size(env, dict, size))
		return false;

	return true;
}

/*
 * Checks if a key exists in a dictionary.
 */
bool
rt_check_dict_key(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	bool *ret)
{
	/* Delegate to the object model implementation. */
	if (!om_check_dict_key(env, dict, key, ret))
		return false;

	return true;
}

/*
 * Checks if a key exists in a dictionary.
 */
bool
rt_check_dict_key_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	bool *ret)
{
	struct rt_value key_val;

	key_val.type = NOCT_VALUE_INT;
	key_val.val.i = 0;
	if (env->frame != NULL)
		rt_pin_local(env, &key_val);
	else
		rt_pin_global(env, &key_val);

	if (!rt_make_string(env, &key_val, key))
		return false;

	/* Delegate to the object model implementation. */
	if (!om_check_dict_key(env, dict, &key_val, ret))
		return false;
		
	if (env->frame != NULL)
		rt_unpin_local(env, &key_val);
	else
		rt_unpin_global(env, &key_val);
	
	return true;
}

/*
 * Get a dictionary key by index.
 */
bool
rt_get_dict_by_index(
	struct rt_env *env,
	struct rt_value *dict,
	size_t index,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict_index(env, dict, index, key, val))
		return false;

	return true;
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict(env, dict, key, val))
		return false;

	return true;	
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	size_t len;

	/* Including NUL. */
	len = strlen(key) + 1;

	/* Delegate to the object model implementation. */
	if (!om_read_dict_with_hash(env,
				    dict,
				    key,
				    len,
				    rt_string_hash(key),
				    val))
		return false;
		
	return true;
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem_with_hash(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict_with_hash(env, dict, key, len, hash, val))
		return false;

	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_dict(env, dict, key, val))
		return false;
		
	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	size_t len;

	/* Including NUL. */
	len = strlen(key) + 1;

	/* Delegate to the object model implementation. */
	if (!om_write_dict_with_hash(env,
				     dict,
				     key,
				     len,
				     rt_string_hash(key),
				     val))
		return false;
	
	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem_with_hash(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_dict_with_hash(env,
				     dict,
				     key,
				     len,
				     hash,
				     val))
		return false;
	
	return true;
}

/*
 * Remove a dictionary key.
 */
bool
rt_remove_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key)
{
	/* Delegate to the object model implementation. */
	if (!om_erase_dict_entry(env, dict, key))
		return false;

	return true;
}

/*
 * Remove a dictionary key. (hash version)
 */
bool
rt_remove_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key)
{
	struct rt_value key_val;

	key_val.type = NOCT_VALUE_INT;
	key_val.val.i = 0;
	if (env->frame != NULL)
		rt_pin_local(env, &key_val);
	else
		rt_pin_global(env, &key_val);

	if (!rt_make_string(env, &key_val, key))
		return false;
	
	/* Delegate to the object model implementation. */
	if (!om_erase_dict_entry(env, dict, &key_val)) {
		rt_unpin_global(env, &key_val);
		return false;
	}
		
	if (env->frame != NULL)
		rt_unpin_local(env, &key_val);
	else
		rt_unpin_global(env, &key_val);
	
	return true;
}

/*
 * Make a shallow copy of a dictionary.
 */
bool
rt_make_dict_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	/* Delegate to the object model implementation. */
	if (!om_copy_dict(env, dst, src))
		return false;

	return true;
}

/*
 * Merges a dictionary.
 */
bool
rt_merge_dict(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src1,
	struct rt_value *src2)
{
	/* Delegate to the object model implementation. */
	if (!om_merge_dict(env, dst, src1, src2))
		return false;

	return true;
}

static struct rt_dict *
rt_get_latest_dict(
	struct rt_env *env,
	struct rt_value *dict)
{
#if defined(NOCT_USE_MULTITHREAD)
	struct rt_dict *real_dict;
	struct rt_dict *next;

	UNUSED_PARAMETER(env);

	real_dict = atomic_load_acquire_ptr((void **)&dict->val.dict);
	while ((next = atomic_load_acquire_ptr((void **)&real_dict->newer)) != NULL)
		real_dict = next;

	return real_dict;
#else
	struct rt_dict *real_dict;
	struct rt_dict *next;

	UNUSED_PARAMETER(env);

	real_dict = dict->val.dict;
	while ((next = real_dict->newer) != NULL)
		real_dict = next;

	return real_dict;
#endif
}

/*
 * Sets the native pointers to a dictionary.
 */
bool
rt_set_dict_native_pointer(
	struct rt_env *env,
	struct rt_value *dict,
	void *native_pointer,
	void (*native_finalizer)(void *native_pointer))
{
	struct rt_dict *real_dict;

	real_dict = rt_get_latest_dict(env, dict);

	real_dict->native_pointer = native_pointer;
	real_dict->native_finalizer = native_finalizer;

	return true;
}

/*
 * Gets the native pointer from a dictionary.
 */
bool
rt_get_dict_native_pointer(
	struct rt_env *env,
	struct rt_value *dict,
	void **native_pointer,
	void (**native_finalizer)(void *native_pointer))
{
	struct rt_dict *real_dict;

	real_dict = rt_get_latest_dict(env, dict);

	*native_pointer = real_dict->native_pointer;
	*native_finalizer = real_dict->native_finalizer;

	return true;
}

/*
 * Make a packed.
 */
bool
rt_make_packed(
	struct rt_env *env,
	struct rt_value *val,
	int type,
	size_t size,
	size_t elem_size,
	void *preallocated,
	void *native_pointer,
	void (*native_finalizer)(void *native_pointer))
{
	struct rt_packed *packed;

	assert(env != NULL);
	assert(val != NULL);
	assert(size > 0);
	assert(elem_size > 0);
	assert((native_pointer == NULL) == (native_finalizer == NULL));
	assert(preallocated != NULL || native_pointer == NULL);

	/* Allocate an array. */
	packed = rt_gc_alloc_packed(env,
				    type,
				    size,
				    elem_size,
				    preallocated,
				    native_pointer,
				    native_finalizer);
	if (packed == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/*
	 * Here, this thread is "in-flight" and GC won't be executed
	 * in other threads.
	 */

	/* Setup a value. */
	val->type = NOCT_VALUE_PACKED;
	val->val.packed = packed;

	return true;
}

bool
rt_get_packed_native_pointer(
	struct rt_env *env,
	struct rt_value *packed,
	void **native_pointer,
	void (**native_finalizer)(void *native_pointer))
{
	UNUSED_PARAMETER(env);

	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(native_pointer != NULL);
	assert(native_finalizer != NULL);

	*native_pointer = packed->val.packed->native_pointer;
	*native_finalizer = packed->val.packed->native_finalizer;
	return true;
}

bool
rt_finalize_packed(
	struct rt_env *env,
	struct rt_value *packed)
{
	struct rt_packed *p;
	void *native_pointer;
	void (*native_finalizer)(void *native_pointer);

	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);

	p = packed->val.packed;
	assert(p != NULL);
	if (p->native_finalizer == NULL)
		return true;

	native_pointer = p->native_pointer;
	native_finalizer = p->native_finalizer;

	p->native_pointer = NULL;
	p->native_finalizer = NULL;
	p->packed_buffer = NULL;
	p->elem_size = 0;

	native_finalizer(native_pointer);

	return true;
}

/*
 * Get the element type of a packed.
 */
bool
rt_get_packed_type(
	struct rt_env *env,
	struct rt_value *packed,
	int *type)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(type != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	/* Get the type. */
	*type = packed->val.packed->type;

	return true;
}

/*
 * Get the element count of a packed.
 */
bool
rt_get_packed_size(
	struct rt_env *env,
	struct rt_value *packed,
	size_t *size)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(size != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	/* Get the type. */
	*size = packed->val.packed->elem_size;

	return true;
}

/*
 * Retrieves an int8 packed element.
 */
bool
rt_get_packed_elem(
	struct rt_env *env,
	struct rt_value *packed,
	size_t index,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(val != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	if (index >= packed->val.packed->elem_size) {
		rt_error(env, N_TR("Packed index %ld is out-of-range."), index);
		return false;
	}

	switch (packed->val.packed->type) {
	case NOCT_PACKED_INT8:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int8_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT8:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((uint8_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT16:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int16_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT16:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((uint16_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT32:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int32_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT32:
		val->type = NOCT_VALUE_INT;
		val->val.i = (int32_t)*((uint32_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT64:
		val->type = NOCT_VALUE_LONG;
		val->val.l = (int64_t)*((int64_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT64:
		val->type = NOCT_VALUE_LONG;
		val->val.l = (int64_t)*((uint64_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_FLOAT32:
		val->type = NOCT_VALUE_FLOAT;
		val->val.f = *((float *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_FLOAT64:
		val->type = NOCT_VALUE_DOUBLE;
		val->val.lf = *((double *)(packed->val.packed->packed_buffer) + index);
		break;
	}

	return true;
}

/*
 * Stores an value to a packed.
 */
bool
rt_set_packed_elem(
	struct rt_env *env,
	struct rt_value *packed,
	size_t index,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);

	assert(val != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	if (index >= packed->val.packed->elem_size) {
		rt_error(env, N_TR("Packed index %ld is out-of-range."), index);
		return false;
	}

	switch (packed->val.packed->type) {
	case NOCT_PACKED_INT8:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT8:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT16:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT16:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)(int)val->val.f;
 			break;
		case NOCT_VALUE_DOUBLE:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)(uint64_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)(uint64_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(int64_t)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(int64_t)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_FLOAT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_FLOAT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	default:
		assert(0);
		break;
	}

	return true;
}

/*
 * Make a copy of a packed.
 */
bool
rt_make_packed_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	struct rt_packed *dst_packed;
	size_t size;

	assert(env != NULL);
	assert(dst != NULL);
	assert(dst->type == NOCT_VALUE_PACKED);
	assert(dst->val.packed != NULL);
	assert(dst->val.packed->packed_buffer != NULL);
	assert(src->type == NOCT_VALUE_PACKED);
	assert(src->val.packed != NULL);
	assert(src->val.packed->packed_buffer != NULL);

	/* Determine the byte size. */
	switch (src->val.packed->type) {
	case NOCT_PACKED_INT8:
	case NOCT_PACKED_UINT8:
		size = src->val.packed->elem_size;
		break;
	case NOCT_PACKED_INT16:
	case NOCT_PACKED_UINT16:
		size = src->val.packed->elem_size * 2;
		break;
	case NOCT_PACKED_INT32:
	case NOCT_PACKED_UINT32:
	case NOCT_PACKED_FLOAT32:
		size = src->val.packed->elem_size * 4;
		break;
	default:
		size = src->val.packed->elem_size * 8;
		break;
	}

	/* Allocate an array. */
	dst_packed = rt_gc_alloc_packed(env,
					 src->val.packed->type,
					 size,
					 src->val.packed->elem_size,
					 NULL,
					 NULL,
					 NULL);
	if (dst_packed == NULL)
		return false;

	/*
	 * In this section, it is guaranteed that GC is not executed
	 * in other threads because this thread is "in-flight" and
	 * a GC execution waits for all threads become not in-flight.
	 */

	memcpy(dst_packed->packed_buffer, src->val.packed->packed_buffer, size);

	dst->type = NOCT_VALUE_PACKED;
	dst->val.packed = dst_packed;

	return true;
}

/*
 * Global Variable
 */

#if !defined(NOCT_USE_MULTITHREAD)

#define ACQUIRE_GLOBAL()
#define RELEASE_GLOBAL()

#else

#define ACQUIRE_GLOBAL()								\
	do {										\
		while (1) {							\
			int old = atomic_fetch_add_acquire_int(			\
				&env->vm->global_var_counter, 1);			\
			if (old == 0)						\
				break;							\
			atomic_fetch_sub_release_int(				\
				&env->vm->global_var_counter, 1);			\
		}									\
	} while (0)

#define RELEASE_GLOBAL()								\
	do {										\
		atomic_fetch_sub_release_int(&env->vm->global_var_counter, 1);	\
	} while (0)

#endif

/* Initialize the global variables. */
static bool
rt_init_global(
	struct rt_env *env)
{
	const uint32_t START_SIZE = 2;

	assert(env->vm->global == NULL);

	/* Allocate the table. */
	env->vm->global = noct_calloc(sizeof(struct rt_bindglobal) * START_SIZE, 1);
	if (env->vm->global == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	env->vm->global_alloc_size = START_SIZE;
	env->vm->global_size = 0;

	return true;
}

/* Cleanup the global variables. */
static void
rt_cleanup_global(
	struct rt_env *env)
{
	uint32_t i;

	assert(env->vm->global != NULL);

	for (i = 0; i < env->vm->global_alloc_size; i++) {
		if (env->vm->global[i].name != NULL) {
			noct_free(env->vm->global[i].name);
			env->vm->global[i].name = NULL;
		}
	}
	noct_free(env->vm->global);
	env->vm->global = NULL;
}

/*
 * Check if a global variable exists.
 */
bool
rt_check_global(
	struct rt_env *env,
	const char *name)
{
	uint32_t index, i, len, hash;

	ACQUIRE_GLOBAL();

	rt_string_hash_and_len(name, &hash, &len);
	len++;	/* Including NUL. */

	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		if (env->vm->global[i].is_removed)
			continue;
		if (env->vm->global[i].name == NULL) {
			/* Not found. */
			RELEASE_GLOBAL();
			return false;
		}
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) != 0)
			continue;

		/* Found. */
		RELEASE_GLOBAL();
		return true;
	}

	/* Not found. */
	RELEASE_GLOBAL();
	return false;
}

/*
 * Get a global variable.
 */
bool
rt_get_global(
	struct rt_env *env,
	const char *name,
	struct rt_value *val)
{
	size_t len;
	uint32_t hash;

	len = strlen(name) + 1; /* Including NUL. */
	hash = rt_string_hash(name);

	if (!rt_get_global_with_hash(env, name, len, hash, val))
		return false;

	return true;
}

/*
 * Get a global variable. (hash version)
 */
bool
rt_get_global_with_hash(
	struct rt_env *env,
	const char *name,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	uint32_t index, i;

	ACQUIRE_GLOBAL();

	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		if (env->vm->global[i].is_removed)
			continue;
		if (env->vm->global[i].name == NULL)
			break;
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) != 0)
			continue;

		/* Found. */
		*val = env->vm->global[i].val;
		RELEASE_GLOBAL();
		return true;
	}

	/* Not found. */
	RELEASE_GLOBAL();
	rt_error(env, N_TR("Symbol \"%s\" not found."), name);
	return false;
}

/*
 * Set a global variable.
 */
bool
rt_set_global(
	struct rt_env *env,
	const char *name,
	struct rt_value *val)
{
	size_t len;
	uint32_t hash;

	len = strlen(name) + 1;	/* Including NUL. */
	hash = rt_string_hash(name);
	if (!rt_set_global_with_hash(env, name, len, hash, val))
		return false;

	return true;
}

/* Mark an already-registered global binding immutable. */
bool
rt_mark_global_const(
	struct rt_env *env,
	const char *name)
{
	uint32_t i;

	ACQUIRE_GLOBAL();
	for (i = 0; i < env->vm->global_alloc_size; i++) {
		if (env->vm->global[i].name == NULL ||
		    env->vm->global[i].is_removed)
			continue;
		if (strcmp(env->vm->global[i].name, name) == 0) {
			env->vm->global[i].is_const = true;
			RELEASE_GLOBAL();
			return true;
		}
	}
	RELEASE_GLOBAL();
	rt_error(env, N_TR("Symbol \"%s\" not found."), name);
	return false;
}

/*
 * Set a global variable.
 */
bool
rt_set_global_with_hash(
	struct rt_env *env,
	const char *name,
	size_t len,		/* Including NUL. */
	uint32_t hash,
	struct rt_value *val)
{
	uint32_t index, i;

	ACQUIRE_GLOBAL();

	/* Reisze if 75% is used. */
	if (env->vm->global_size >= env->vm->global_alloc_size / 4 * 3) {
		if (!rt_expand_global(env)) {
			RELEASE_GLOBAL();
			return false;
		}
	}

	/* Search a place to insert or overwrite. */
	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		/* If found an empty entry. */
		if (env->vm->global[i].is_removed ||
		    env->vm->global[i].name == NULL) {
			/* Insert a new entry. */
			env->vm->global[i].name = noct_strdup(name);
			if (env->vm->global[i].name == NULL) {
				RELEASE_GLOBAL();
				rt_out_of_memory(env);
				return false;
			}
			env->vm->global[i].name_len = (uint32_t)len;
			env->vm->global[i].name_hash = hash;
			env->vm->global[i].val = *val;
			env->vm->global_size++;
			RELEASE_GLOBAL();
			return true;
		}

		/* If found an existing entry. */
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) == 0) {
			/* Reject assignment to a constant (let) binding. */
			if (env->vm->global[i].is_const) {
				RELEASE_GLOBAL();
				rt_error(env, N_TR("Cannot assign to constant \"%s\"."), name);
				return false;
			}
			/* Overwrite the existing entry value. */
			env->vm->global[i].val = *val;
			RELEASE_GLOBAL();
			return true;
		}
	}

	/* No empty entry. */
	assert(NEVER_COME_HERE);
	RELEASE_GLOBAL();
	return false;
}

/* Expand the global variable table. */
static bool
rt_expand_global(
	struct rt_env *env)
{
	struct rt_bindglobal *old_tbl,*new_tbl;
	uint32_t old_size, new_size, i, j, index;

	/* Allocate the new table. */
	old_size = env->vm->global_alloc_size;
	old_tbl = env->vm->global;
	new_size = old_size * 2;
	new_tbl = noct_calloc(sizeof(struct rt_bindglobal) * new_size, 1);
	if (new_tbl == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Rehash (copy). */
	for (i = 0; i < old_size; i++) {
		if (old_tbl[i].name == NULL || old_tbl[i].is_removed)
			continue;
		index = rt_string_hash(old_tbl[i].name) & (new_size - 1) ;
		for (j = index;
		     j != ((index - 1 + new_size) & (new_size - 1));
		     j = (j + 1) & (new_size - 1)) {
			if (new_tbl[j].name == NULL) {
				new_tbl[j].name = old_tbl[i].name;
				new_tbl[j].name_len = old_tbl[i].name_len;
				new_tbl[j].name_hash = old_tbl[i].name_hash;
				new_tbl[j].val = old_tbl[i].val;
				new_tbl[j].is_const = old_tbl[i].is_const;
				break;
			}
		}
	}

	noct_free(old_tbl);
	env->vm->global = new_tbl;
	env->vm->global_alloc_size = new_size;

	return true;
}

/*
 * Pinning Native APIs
 */

/*
 * Pins a C global variable.
 */
bool
rt_pin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_pin_global(env, val))
		return false;

	return true;
}

/*
 * Unpins a C global variable.
 */
bool
rt_unpin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_unpin_global(env, val))
		return false;

	return true;
}

/*
 * Pin a C local variable.
 */
bool
rt_pin_local(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_pin_local(env, val))
		return false;

	return true;
}

/*
 * Unpin a C local variable.
 */
bool
rt_unpin_local(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_unpin_local(env, val))
		return false;

	return true;
}

/*
 * Make a safepoint.
 */
bool
rt_safepoint(
	struct rt_env *env)
{
	om_safepoint(env);

	return true;
}

/*
 * Error Handling
 */

/*
 * Get an error message.
 */
const char *
rt_get_error_message(
	struct rt_env *env)
{
	return &env->error_message[0];
}

/*
 * Get an error file name.
 */
const char *
rt_get_error_file(
	struct rt_env *env)
{
	return &env->file_name[0];
}

/*
 * Get an error line number.
 */
int
rt_get_error_line(
	struct rt_env *env)
{
	return env->line;
}

/*
 * Output an error message.
 */
void
rt_error(
	struct rt_env *env,
	const char *msg,
	...)
{
	va_list ap;

	va_start(ap, msg);
	vsnprintf(env->error_message, sizeof(env->error_message) - 1, msg, ap);
	va_end(ap);
}

/*
 * Output an out-of-memory message.
 */
void
rt_out_of_memory(
	struct rt_env *env)
{
	noct_error(env, N_TR("Out of memory."));
}
