/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Language Runtime
 */

#include "runtime.h"
#include "intrinsics.h"
#include "interpreter.h"
#include "jit.h"
#include "gc.h"

#if !defined(NO_COMPILATION)
#include "ast.h"
#include "hir.h"
#include "lir.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

/* False assertion */
#define NOT_IMPLEMENTED		0
#define NEVER_COME_HERE		0
#define PINNED_VAR_NOT_FOUND	0

/*
 * Config
 */
bool noct_conf_use_jit = true;
int noct_conf_optimize = 0;
bool noct_conf_repl_mode = 0;

/* Forward declarations. */
static void rt_free_env(struct rt_env *env);
static void rt_free_func(struct rt_env *rt, struct rt_func *func);
static bool rt_register_lir(struct rt_env *rt, struct lir_func *lir);
static bool rt_register_bytecode_function(struct rt_env *rt, uint8_t *data, uint32_t size, int *pos, char *file_name);
static const char *rt_read_bytecode_line(uint8_t *data, uint32_t size, int *pos);
static bool rt_expand_array(struct rt_env *env, struct rt_array **arr, size_t size);
static bool rt_expand_dict(struct rt_env *env, struct rt_dict **dict, size_t size);
static bool rt_get_return(struct rt_env *rt, struct rt_value *val);

/*
 * Create a virtual machine.
 */
bool
rt_create_vm(
	struct rt_vm **vm,
	struct rt_env **default_env)
{
	/* Allocate a struct rt_vm. */
	*vm = noct_malloc(sizeof(struct rt_vm));
	if (*vm == NULL) {
		*default_env = NULL;
		return false;
	}
	memset(*vm, 0, sizeof(struct rt_vm));

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

#if defined(USE_MULTITHREAD)
	/* Initialize for GC. */
	rt_gc_init_env(env);
#endif

	/* Initialize the garbage collector. */
	if (!rt_gc_init(*vm)) {
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}

	/* Register the intrinsics. */
	if (!rt_register_intrinsics(*default_env)) {
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

	/* Free the JIT region. */
	if (noct_conf_use_jit)
		jit_free(vm->env_list);

	/* Free thread environments. */
	env = vm->env_list;
	while (env != NULL) {
		next_env = env->next;
		noct_free(env);
		env = next_env;
	}

	/* Cleanup the garbage collector. */
	rt_gc_cleanup(vm);

	/* Free functions. */
	func = vm->func_list;
	while (func != NULL) {
		next_func = func->next;
		rt_free_func(vm->env_list, func);
		func = next_func;
	}

	/* Free rt_env. */
	noct_free(vm);

	return true;
}

/* Free a function. */
static void
rt_free_func(
	struct rt_env *env,
	struct rt_func *func)
{
	int i;

	noct_free(func->name);
	func->name = NULL;
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (func->param_name[i] != NULL) {
			free(func->param_name[i]);
			func->param_name[i] = NULL;
		}
	}
	noct_free(func->file_name);
	noct_free(func->bytecode);

	if (func->jit_code != NULL)
		func->jit_code = NULL;
}

#if defined(USE_MULTITHREAD)
/*
 * Create an environment for the current thread.
 */
bool
rt_create_thread_env(
	struct rt_env *prev_env,
	struct rt_env **new_env)
{
	struct rt_env *env;

	/* Allocate a struct rt_env. */
	env = noct_malloc(sizeof(struct rt_env));
	if (env == NULL) {
		rt_out_of_memory(prev_env);
		return false;
	}
	memset(env, 0, sizeof(struct rt_env));
	env->vm = prev_env->vm;

	/* Link. */
	env->next = prev_env->vm->env_list;
	prev_env->vm->env_list = env;

	/* Enter the initial stack frame. */
	env->cur_frame_index = 0;
	env->frame = &env->frame_alloc[0];
	env->frame->tmpvar = &env->frame->tmpvar_alloc[0];
	env->frame->tmpvar_size = RT_TMPVAR_MAX;

	/* Initialize for GC. */
	rt_gc_init_env(env);

	/* Succeeded. */
	*new_env = env;
	return true;
}
#endif

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
 * Register functions from a souce text.
 */
bool
rt_register_source(
	struct rt_env *env,
	const char *file_name,
	const char *source_text)
{
	struct hir_block *hfunc;
	struct lir_func *lfunc;
	int i, func_count;
	bool is_succeeded;

	is_succeeded = false;
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

		/* For each function. */
		func_count = hir_get_function_count();
		for (i = 0; i < func_count; i++) {
			/* Transform HIR to LIR (bytecode). */
			hfunc = hir_get_function(i);
			if (!lir_build(hfunc, &lfunc)) {
				strncpy(env->file_name, lir_get_file_name(), sizeof(env->file_name) - 1);
				env->line = lir_get_error_line();
				rt_error(env, "%s", lir_get_error_message());
				break;
			}

			/* Make a function object. */
			if (!rt_register_lir(env, lfunc))
				break;

			/* Free a LIR. */
			lir_cleanup(lfunc);
		}
		if (i != func_count)
			break;

		is_succeeded = true;
	} while (0);

	/* Free intermediates. */
	hir_cleanup();
	ast_cleanup();

	/* If failed. */
	if (!is_succeeded)
		return false;

	/* Succeeded. */
	return true;
}

/* Register a function from LIR. */
static bool
rt_register_lir(
	struct rt_env *env,
	struct lir_func *lir)
{
	struct rt_func *func;
	struct rt_bindglobal *global;
	int i;

	func = noct_malloc(sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	memset(func, 0, sizeof(struct rt_func));

	func->name = strdup(lir->func_name);
	if (func->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	func->param_count = lir->param_count;
	for (i = 0; i < lir->param_count; i++) {
		func->param_name[i] = strdup(lir->param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(env);
			return false;
		}
	}
	func->bytecode_size = lir->bytecode_size;
	func->bytecode = noct_malloc((size_t)lir->bytecode_size);
	if (func->bytecode == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	memcpy(func->bytecode, lir->bytecode, (size_t)lir->bytecode_size);
	func->tmpvar_size = lir->tmpvar_size;
	func->file_name = strdup(lir->file_name);
	if (func->file_name == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Insert a bindglobal. */
	global = noct_malloc(sizeof(struct rt_bindglobal));
	if (global == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	global->name = strdup(func->name);
	if (global->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	global->val.type = NOCT_VALUE_FUNC;
	global->val.val.func = func;
	global->next = env->vm->global;
	env->vm->global = global;

	/* Do JIT compilation */
	if (noct_conf_use_jit) {
		/* Write code. */
		jit_build(env, func);

		/* Need to commit before call. */
		env->vm->is_jit_dirty = true;
	}

	/* Link. */
	func->next = env->vm->func_list;
	env->vm->func_list = func;

	return true;
}

/*
 * Register functions from bytecode data.
 */
bool
rt_register_bytecode(
	struct rt_env *env,
	uint32_t size,
	uint8_t *data)
{
	char *file_name;
	const char *line;
	int pos, func_count, i;
	bool succeeded;

	pos = 0;
	file_name = NULL;
	succeeded = false;
	do {
		/* Check "CScript Bytecode". */
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
		file_name = strdup(line);
		if (file_name == NULL)
			break;

		/* Check "Number Of Functions". */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL || strcmp(line, "Number Of Functions") != 0)
			break;

		/* Get a number of functions. */
		line = rt_read_bytecode_line(data, size, &pos);
		if (line == NULL)
			break;
		func_count = atoi(line);

		/* Read functions. */
		for (i = 0; i < func_count; i++) {
			if (!rt_register_bytecode_function(env, data, size, &pos, file_name))
				break;
		}

		succeeded = true;
	} while (0);

	if (file_name != NULL)
		noct_free(file_name);

	if (!succeeded) {
		rt_error(env, _("Failed to load bytecode."));
		return false;
	}

	return true;
}

/* Register a function from bytecode file data. */
static bool
rt_register_bytecode_function(
	struct rt_env *env,
	uint8_t *data,
	uint32_t size,
	int *pos,
	char *file_name)
{
	struct lir_func lfunc;
	const char *line;
	int i;
	bool succeeded;

	memset(&lfunc, 0, sizeof(lfunc));
	lfunc.file_name = file_name;

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
		lfunc.func_name = strdup(line);
		if (lfunc.func_name == NULL)
			break;

		/* Check "Parameters". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Parameters") != 0)
			break;

		/* Get number of parameters. */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL)
			break;
		lfunc.param_count = atoi(line);

		/* Get parameters. */
		for (i = 0; i < lfunc.param_count; i++) {
			line = rt_read_bytecode_line(data, size, pos);
			if (line == NULL)
				break;
			lfunc.param_name[i] = strdup(line);
			if (lfunc.param_name[i] == NULL)
				break;
		}
		if (i != lfunc.param_count)
			break;

		/* Check "Temporary Size". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Temporary Size") != 0)
			break;

		/* Get a local size. */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL)
			break;
		lfunc.tmpvar_size = atoi(line);

		/* Check "Bytecode Size". */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "Bytecode Size") != 0)
			break;

		/* Get a bytecode size. */
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL)
			break;
		lfunc.bytecode_size = atoi(line);

		/* Load LIR. */
		lfunc.bytecode = data + *pos;
		if (!rt_register_lir(env, &lfunc))
			break;

		/* Check "End Function". */
		(*pos) += lfunc.bytecode_size + 1;
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "End Function") != 0)
			break;

		succeeded = true;
	} while (0);

	if (lfunc.func_name != NULL)
		noct_free(lfunc.func_name);

	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (lfunc.param_name[i] != NULL)
			noct_free(lfunc.param_name[i]);
	}

	if (!succeeded) {
		noct_error(env, _("Failed to load bytecode data."));
		return false;
	}

	return true;
}

/* Read a line from bytecode file data. */
static const char *
rt_read_bytecode_line(
	uint8_t *data,
	uint32_t size,
	int *pos)
{
	static char line[1024];
	int i;

	for (i = 0; i < (int)sizeof(line); i++) {
		if (*pos >= (int)size)
			return NULL;

		line[i] = (char)data[*pos];
		(*pos)++;
		if (line[i] == '\n') {
			line[i] = '\0';
			return line;
		}
	}
	return NULL;
}

/*
 * Register an FFI C function.
 */
bool
rt_register_cfunc(
	struct rt_env *env,
	const char *name,
	int param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	struct rt_func **ret_func)
{
	struct rt_func *func;
	struct rt_bindglobal *global;
	int i;

	func = noct_malloc(sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	memset(func, 0, sizeof(struct rt_func));

	func->name = strdup(name);
	if (func->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	func->param_count = param_count;
	for (i = 0; i < param_count; i++) {
		func->param_name[i] = strdup(param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(env);
			return false;
		}
	}
	func->cfunc = cfunc;
	func->tmpvar_size = param_count + 1;

	global = noct_malloc(sizeof(struct rt_bindglobal));
	if (global == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	global->name = strdup(name);
	if (global->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	global->val.type = NOCT_VALUE_FUNC;
	global->val.val.func = func;
	global->next = env->vm->global;
	env->vm->global = global;

	if (ret_func != NULL)
		*ret_func = func;

	return true;
}

/*
 * Call a function with a name.
 */
bool
rt_call_with_name(
	struct rt_env *env,
	const char *func_name,
	int arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	struct rt_bindglobal *global;
	struct rt_func *func;
	bool func_ok;

	/* Search a function. */
	func_ok = false;
	do {
		if (!rt_find_global(env, func_name, &global))
			break;
		if (global->val.type != NOCT_VALUE_FUNC)
			break;
		func_ok = true;
	} while (0);
	if (!func_ok) {
		noct_error(env, _("Cannot find function %s."), func_name);
		return false;
	}
	func = global->val.val.func;

	/* Call. */
	if (!rt_call(env, func, arg_count, arg, ret))
		return false;

	return true;
}

/*
 * Call a function.
 */
bool
rt_call(
	struct rt_env *env,
	struct rt_func *func,
	int arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	int i;

#if defined(USE_MULTITHREAD)
	/* Make a GC safe point. */
	rt_gc_safepoint(env);
#endif

	/* Commit JIT-compiled code. */
	if (noct_conf_use_jit && env->vm->is_jit_dirty) {
		jit_commit(env);
		env->vm->is_jit_dirty = false;
	}

	/* Allocate a frame for this call. */
	if (!rt_enter_frame(env, func))
		return false;

	/* Pass args. */
	if (arg_count != func->param_count) {
		noct_error(env, _("Function arguments not match."));
		return false;
	}
	for (i = 0; i < arg_count; i++)
		env->frame->tmpvar[i] = arg[i];

	/* Run. */
	if (func->cfunc != NULL) {
		/* Call an intrinsic or an FFI function implemented in C. */
		if (!func->cfunc(env))
			return false;
	} else {
		/* Set a file name. */
		strncpy(env->file_name, env->frame->func->file_name, sizeof(env->file_name) - 1);

		if (func->jit_code != NULL) {
			/* Call a JIT-generated code. */
			if (!func->jit_code(env)) {
				//printf("Returned from JIT code (false).\n");
				return false;
			}
			//printf("Returned from JIT code (true).\n");
			//printf("%d: %d\n", env->frame->tmpvar[0].type, env->frame->tmpvar[0].val.i);
		} else {
			/* Call the bytecode interpreter. */
			if (!rt_visit_bytecode(env, func))
				return false;
		}
	}

	/* Get a return value. */
	if (!rt_get_return(env, ret))
		return false;

	/* Succeeded. */
	rt_leave_frame(env, ret);

	return true;
}

/*
 * Enter a new calling frame.
 */
bool
rt_enter_frame(
	struct rt_env *env,
	struct rt_func *func)
{
	struct rt_frame *frame;

	if (++env->cur_frame_index >= RT_FRAME_MAX) {
		rt_error(env, _("Stack overflow."));
		return false;
	}

	frame = &env->frame_alloc[env->cur_frame_index];
	env->frame = frame;
	frame->func = func;
	frame->tmpvar = &frame->tmpvar_alloc[0];
	frame->tmpvar_size = func->tmpvar_size;

	/* We can't remove this due to GC. */
	memset(frame->tmpvar, 0, sizeof(struct rt_value) * (size_t)frame->tmpvar_size);

	return true;
}

/*
 * Leave the current calling frame.
 */
void
rt_leave_frame(
	struct rt_env *env,
	struct rt_value *ret)
{
	struct rt_frame *frame;

	if (--env->cur_frame_index < 0) {
		rt_error(env, _("Stack underflow."));
		abort();
	}

	env->frame = &env->frame_alloc[env->cur_frame_index];
}

/*
 * Make a string value.
 */
bool
rt_make_string(
	struct rt_env *env,
	struct rt_value *val,
	const char *data)
{
	struct rt_string *rts;

	/* Allocate a string. */
	rts = rt_gc_alloc_string(env, strlen(data) + 1, data);
	if (rts == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Setup a value. */
	val->type = NOCT_VALUE_STRING;
	val->val.str = rts;

	return true;
}

/*
 * Make an empty array value.
 */
bool
rt_make_empty_array(
	struct rt_env *env,
	struct rt_value *val)
{
	struct rt_array *arr;

	const int START_SIZE = 16;

	/* Allocate an array. */
	arr = rt_gc_alloc_array(env, START_SIZE);
	if (arr == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Setup a value. */
	val->type = NOCT_VALUE_ARRAY;
	val->val.arr = arr;

	return true;
}

/*
 * Make an empty dictionary value.
 */
bool
rt_make_empty_dict(
	struct rt_env *env,
	struct rt_value *val)
{
	struct rt_dict *dict;

	const int START_SIZE = 16;

	/* Allocate a dictionary. */
	dict = rt_gc_alloc_dict(env, START_SIZE);
	if (dict == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Setup a value. */
	val->type = NOCT_VALUE_DICT;
	val->val.dict = dict;

	return true;
}

/*
 * Retrieves an array element.
 */
bool
rt_get_array_elem(
	struct rt_env *env,
	struct rt_array *arr,
	int index,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(arr != NULL);
	assert(index >= 0);
	assert(index < arr->size);
	assert(val != NULL);

	*val = arr->table[index];

	return true;
}

/*
 * Stores an value to an array.
 */
bool
rt_set_array_elem(
	struct rt_env *env,
	struct rt_array *arr,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(arr != NULL);
	assert(index >= 0);
	assert(val != NULL);

	/*
	 * Expand the array if needed.
	 * Note that array->val.arr may be replaced.
	 */
	if (!rt_expand_array(env, &arr, index + 1))
		return false;
	if (arr->size < index + 1)
		arr->size = index + 1;

	/* Store. */
	arr->table[index] = *val;

	/* GC: Write barrier for the remember set. */
	if (val->type == NOCT_VALUE_STRING ||
	    val->type == NOCT_VALUE_ARRAY ||
	    val->type == NOCT_VALUE_DICT)
		rt_gc_array_write_barrier(env, arr, index, val);

	return true;
}

/*
 * Resizes an array.
 */
bool
rt_resize_array(
	struct rt_env *env,
	struct rt_array *arr,
	int size)
{
	assert(env != NULL);
	assert(arr != NULL);

	if (size > arr->size) {
		/* Expand the array size if needed. */
		if (!rt_expand_array(env, &arr, size))
			return false;

		/* Set the element count. */
		arr->size = size;
	} else {
		/* Remove (zero-fill) the reminder. */
		memset(&arr->table[size], 0, sizeof(struct rt_value) * (size_t)(arr->size - size - 1));

		/* Set the element count. */
		arr->size = size;
	}

	return true;
}

/* Expand an array. */
static bool
rt_expand_array(
	struct rt_env *env,
	struct rt_array **arr,
	size_t size)
{
	struct rt_array *old_arr, *new_arr;
	int i;

	assert(env != NULL);

	old_arr = *arr;

	/* Expand the table. */
	if (old_arr->alloc_size < size) {
		size_t old_size;

		old_size = old_arr->alloc_size;

		/* Get the next size. */
		if (size < old_size * 2)
			size = old_size * 2;
		else
			size = size * 2;

		/* Allocate the new array. */
		new_arr = rt_gc_alloc_array(env, size);
		if (new_arr == NULL) {
			rt_out_of_memory(env);
			return false;
		}

		/* Copy the values with write barrier. */
		for (i = 0; i < (int)old_arr->size; i++) {
			/* Copy. */
			new_arr->table[i] = old_arr->table[i];

			/* Write barrier. */
			rt_gc_array_write_barrier(env, new_arr, i, &new_arr->table[i]);
		}

		/* The older array reference is unlinked here. */
		*arr = new_arr;
	}

	return true;
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem(
	struct rt_env *env,
	struct rt_dict *dict,
	const char *key,
	struct rt_value *val)
{
	int i;

	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);
	
	/* Search the key. */
	for (i = 0; i < dict->size; i++) {
		if (strcmp(dict->key[i].val.str->data, key) == 0) {
			/* Succeeded. */
			*val = dict->value[i];
			return true;
		}
	}

	/* Failed. */
	rt_error(env, _("Dictionary key \"%s\" not found."), key);
	return false;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem(
	struct rt_env *env,
	struct rt_dict *dict,
	const char *key,
	struct rt_value *val)
{
	int i;

	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);
	
	/* Search for the key. */
	for (i = 0; i < dict->size; i++) {
		if (strcmp(dict->key[i].val.str->data, key) == 0) {
			dict->value[i] = *val;
			return true;
		}
	}

	/* Expand the size. */
	if (!rt_expand_dict(env, &dict, dict->size + 1))
		return false;

	/* Append the key. */
	if (!rt_make_string(env, &dict->key[dict->size], key))
		return false;
	dict->value[dict->size] = *val;
	dict->size++;

	/* GC: Write barrier for the remember set. */
	if (val->type == NOCT_VALUE_STRING ||
	    val->type == NOCT_VALUE_ARRAY ||
	    val->type == NOCT_VALUE_DICT) {
		rt_gc_dict_write_barrier(env, dict, i, &dict->key[dict->size]);
		rt_gc_dict_write_barrier(env, dict, i, val);
	}

	return true;
}

/* Expand an array. */
static bool
rt_expand_dict(
	struct rt_env *env,
	struct rt_dict **dict,
	size_t size)
{
	struct rt_dict *old_dict, *new_dict;
	int i;

	assert(env != NULL);
	assert(dict != NULL);

	old_dict = *dict;

	/* Expand the tables. */
	if (old_dict->alloc_size < size) {
		size_t old_size;

		old_size = old_dict->alloc_size;

		/* Get the next size. */
		if (size < old_size * 2)
			size = old_size * 2;
		else
			size = size * 2;

		/* Allocate the new array. */
		new_dict = rt_gc_alloc_dict(env, size);
		if (new_dict == NULL) {
			rt_out_of_memory(env);
			return false;
		}

		/* Copy the values with write barrier. */
		for (i = 0; i < (int)old_dict->size; i++) {
			/* Copy the key. */
			new_dict->key[i] = old_dict->key[i];

			/* Copy the value. */
			new_dict->value[i] = old_dict->value[i];

			/* Write barrier. */
			rt_gc_dict_write_barrier(env, new_dict, i, &new_dict->key[i]);
			rt_gc_dict_write_barrier(env, new_dict, i, &new_dict->value[i]);
		}

		/* The older array reference is unlinked here. */
		*dict = new_dict;
	}

	return true;
}

/* Get a return value. */
static bool
rt_get_return(
	struct rt_env *env,
	struct rt_value *val)
{
	*val = env->frame->tmpvar[0];

	return true;
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
	struct rt_bindglobal *global;

	global = env->vm->global;
	while (global != NULL) {
		if (strcmp(global->name, name) == 0)
			break;
		global = global->next;
	}
	if (global == NULL) {
		noct_error(env, _("Global variable \"%s\" not found."), name);
		return false;
	}

	*val = global->val;

	return true;
}

/* Find a global variable. */
bool
rt_find_global(
	struct rt_env *env,
	const char *name,
	struct rt_bindglobal **global)
{
	struct rt_bindglobal *g;

	g = env->vm->global;
	while (g != NULL) {
		if (strcmp(g->name, name) == 0) {
			*global = g;
			return true;
		}
		g = g->next;
	}

	*global = NULL;

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
	struct rt_bindglobal *global;

	/* Find a bindglobal. */
	if (!rt_find_global(env, name, &global)) {
		/* Add a global. */
		if (!rt_add_global(env, name, &global))
			return false;
	}

	/* Set the value. */
	global->val = *val;

	return true;
}

/*
 * Add a global variable.
 */
bool
rt_add_global(
	struct rt_env *env,
	const char *name,
	struct rt_bindglobal **global)
{
	struct rt_bindglobal *g;

	g = noct_malloc(sizeof(struct rt_bindglobal));
	if (g == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	g->name = strdup(name);
	if (g->name == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	g->val.type = NOCT_VALUE_INT;
	g->val.val.i = 0;

	g->next = env->vm->global;
	env->vm->global = g;

	*global = g;

	return true;
}

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
 * Retrieves the approximate memory usage, in bytes.
 */
bool
rt_get_heap_usage(
	struct rt_env *env,
	size_t *ret)
{
	if (!rt_gc_get_heap_usage(env, ret))
		return false;

	return true;
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
	noct_error(env, _("Out of memory."));
}
