/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Language Runtime
 */

/*
 * [configuration]
 *  - NO_COMPILATION ... No source compilation features.
 *  - USE_DEBUGGER   ... gdb-like debugger feature.
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

/*
 * Config
 */
bool noct_conf_use_jit = true;
int noct_conf_optimize = 0;
bool noct_conf_repl_mode = 0;

/* Text format buffer. */
static char text_buf[65536];

/* Forward declarations. */
static void rt_free_func(struct rt_env *rt, struct rt_func *func);
static bool rt_register_lir(struct rt_env *rt, struct lir_func *lir);
static bool rt_register_bytecode_function(struct rt_env *rt, uint8_t *data, uint32_t size, int *pos, char *file_name);
static const char *rt_read_bytecode_line(uint8_t *data, uint32_t size, int *pos);
static bool rt_expand_array(struct rt_env *rt, struct rt_value *array, size_t size);
static bool rt_expand_dict(struct rt_env *rt, struct rt_value *dict, size_t size);
static bool rt_get_return(struct rt_env *rt, struct rt_value *val);

/*
 * Create a runtime environment.
 */
bool
noct_create(
	struct rt_env **rt)
{
	struct rt_env *env;

	/* Allocate. */
	env = malloc(sizeof(struct rt_env));
	if (env == NULL)
		return false;
	memset(env, 0, sizeof(struct rt_env));

	/* Register the intrinsics. */
	if (!rt_register_intrinsics(env)) {
		free(env);
		return false;
	}

	*rt = env;
	return true;
}

/*
 *  Destroy a runtime environment.
 */
bool
noct_destroy(
	struct rt_env *rt)
{
	struct rt_object_header *obj, *next_obj;
	struct rt_func *func, *next_func;

	/* Free frames. */
	while (rt->frame != NULL)
		rt_leave_frame(rt, NULL);

	/* GC: Free all objects in the garbage list. */
	noct_shallow_gc(rt);

	/* GC: Free all objects in the young list. */
	rt_gc_free_young_objects(rt);

	/* Free functions. */
	func = rt->func_list;
	while (func != NULL) {
		next_func = func->next;
		rt_free_func(rt, func);
		func = next_func;
	}

	/* Free rt_env. */
	free(rt);

	return true;
}

/* Free a function. */
static void
rt_free_func(
	struct rt_env *rt,
	struct rt_func *func)
{
	int i;

	free(func->name);
	func->name = NULL;
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (func->param_name[i] != NULL) {
			free(func->param_name[i]);
			func->param_name[i] = NULL;
		}
	}
	free(func->file_name);
	free(func->bytecode);

	if (func->jit_code != NULL) {
		jit_free(rt, func);
		func->jit_code = NULL;
	}
}

/*
 * Get an error message.
 */
const char *noct_get_error_message(struct rt_env *rt)
{
	return &rt->error_message[0];
}

/*
 * Get an error file name.
 */
const char *noct_get_error_file(struct rt_env *rt)
{
	return &rt->file_name[0];
}

/* Get an error line number. */
int noct_get_error_line(struct rt_env *rt)
{
	return rt->line;
}

/*
 * Register functions from a souce text.
 */
bool
noct_register_source(
	struct rt_env *rt,
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
			strncpy(rt->file_name, ast_get_file_name(), sizeof(rt->file_name) - 1);
			rt->line = ast_get_error_line();
			noct_error(rt, "%s", ast_get_error_message());
			break;
		}

		/* Transform AST to HIR. */
		if (!hir_build()) {
			strncpy(rt->file_name, hir_get_file_name(), sizeof(rt->file_name) - 1);
			rt->line = hir_get_error_line();
			noct_error(rt, "%s", hir_get_error_message());
			break;
		}

		/* For each function. */
		func_count = hir_get_function_count();
		for (i = 0; i < func_count; i++) {
			/* Transform HIR to LIR (bytecode). */
			hfunc = hir_get_function(i);
			if (!lir_build(hfunc, &lfunc)) {
				strncpy(rt->file_name, lir_get_file_name(), sizeof(rt->file_name) - 1);
				rt->line = lir_get_error_line();
				noct_error(rt, "%s", lir_get_error_message());
				break;
			}

			/* Make a function object. */
			if (!rt_register_lir(rt, lfunc))
				break;

			/* Free a LIR. */
			lir_free(lfunc);
		}
		if (i != func_count)
			break;

		is_succeeded = true;
	} while (0);

	/* Free intermediates. */
	hir_free();
	ast_free();

	/* If failed. */
	if (!is_succeeded)
		return false;

	/* Succeeded. */
	return true;
}

/* Register a function from LIR. */
static bool
rt_register_lir(
	struct rt_env *rt,
	struct lir_func *lir)
{
	struct rt_func *func;
	struct rt_bindglobal *global;
	int i;

	func = malloc(sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memset(func, 0, sizeof(struct rt_func));

	func->name = strdup(lir->func_name);
	if (func->name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	func->param_count = lir->param_count;
	for (i = 0; i < lir->param_count; i++) {
		func->param_name[i] = strdup(lir->param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(rt);
			return false;
		}
	}
	func->bytecode_size = lir->bytecode_size;
	func->bytecode = malloc((size_t)lir->bytecode_size);
	if (func->bytecode == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memcpy(func->bytecode, lir->bytecode, (size_t)lir->bytecode_size);
	func->tmpvar_size = lir->tmpvar_size;
	func->file_name = strdup(lir->file_name);
	if (func->file_name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	/* Insert a bindglobal. */
	global = malloc(sizeof(struct rt_bindglobal));
	if (global == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	global->name = strdup(func->name);
	if (global->name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	global->val.type = NOCT_VALUE_FUNC;
	global->val.val.func = func;
	global->next = rt->global;
	rt->global = global;

	/* Do JIT compilation */
	if (noct_conf_use_jit) {
		if (!jit_build(rt, func))
			return false;
	}

	/* Link. */
	func->next = rt->func_list;
	rt->func_list = func;

	return true;
}

/*
 * Register functions from bytecode data.
 */
bool
noct_register_bytecode(
	struct rt_env *rt,
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
			if (!rt_register_bytecode_function(rt, data, size, &pos, file_name))
				break;
		}

		succeeded = true;
	} while (0);

	if (file_name != NULL)
		free(file_name);

	if (!succeeded) {
		noct_error(rt, _("Failed to load bytecode."));
		return false;
	}

	return true;
}

static bool
rt_register_bytecode_function(
	struct rt_env *rt,
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
		if (!rt_register_lir(rt, &lfunc))
			break;

		/* Check "End Function". */
		(*pos) += lfunc.bytecode_size + 1;
		line = rt_read_bytecode_line(data, size, pos);
		if (line == NULL || strcmp(line, "End Function") != 0)
			break;

		succeeded = true;
	} while (0);

	if (lfunc.func_name != NULL)
		free(lfunc.func_name);

	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (lfunc.param_name[i] != NULL)
			free(lfunc.param_name[i]);
	}

	if (!succeeded) {
		noct_error(rt, _("Failed to load bytecode data."));
		return false;
	}

	return true;
}

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
 * Register a C function.
 */
bool
noct_register_cfunc(
	struct rt_env *rt,
	const char *name,
	int param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	struct rt_func **ret_func)
{
	struct rt_func *func;
	struct rt_bindglobal *global;
	int i;

	func = malloc(sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memset(func, 0, sizeof(struct rt_func));

	func->name = strdup(name);
	if (func->name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	func->param_count = param_count;
	for (i = 0; i < param_count; i++) {
		func->param_name[i] = strdup(param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(rt);
			return false;
		}
	}
	func->cfunc = cfunc;
	func->tmpvar_size = param_count + 1;

	global = malloc(sizeof(struct rt_bindglobal));
	if (global == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	global->name = strdup(name);
	if (global->name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	global->val.type = NOCT_VALUE_FUNC;
	global->val.val.func = func;
	global->next = rt->global;
	rt->global = global;

	if (ret_func != NULL)
	*ret_func = func;

	return true;
}

/*
 * Call a function with a name.
 */
bool
noct_call_with_name(
	struct rt_env *rt,
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
		if (!rt_find_global(rt, func_name, &global))
			break;
		if (global->val.type != NOCT_VALUE_FUNC)
			break;
		func_ok = true;
	} while (0);
	if (!func_ok) {
		noct_error(rt, _("Cannot find function %s."), func_name);
		return false;
	}
	func = global->val.val.func;

	/* Call. */
	if (!noct_call(rt, func, arg_count, arg, ret))
		return false;

	return true;
}

/*
 * Call a function.
 */
bool
noct_call(
	struct rt_env *rt,
	struct rt_func *func,
	int arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	int i;

	/* Allocate a frame for this call. */
	if (!rt_enter_frame(rt, func))
		return false;

	/* Pass args. */
	if (arg_count != func->param_count) {
		noct_error(rt, _("Function arguments not match."));
		return false;
	}
	for (i = 0; i < arg_count; i++)
		rt->frame->tmpvar[i] = arg[i];

	/* Run. */
	if (func->cfunc != NULL) {
		/* Call an intrinsic or an FFI function implemented in C. */
		if (!func->cfunc(rt))
			return false;
	} else {
		/* Set a file name. */
		strncpy(rt->file_name, rt->frame->func->file_name, sizeof(rt->file_name) - 1);

		if (func->jit_code != NULL) {
			/* Call a JIT-generated code. */
			if (!func->jit_code(rt)) {
				//printf("Returned from JIT code (false).\n");
				return false;
			}
			//printf("Returned from JIT code (true).\n");
			//printf("%d: %d\n", rt->frame->tmpvar[0].type, rt->frame->tmpvar[0].val.i);
		} else {
			/* Call the bytecode interpreter. */
			if (!rt_visit_bytecode(rt, func))
				return false;
		}
	}

	/* Get a return value. */
	if (!rt_get_return(rt, ret))
		return false;

	/* Succeeded. */
	rt_leave_frame(rt, ret);

	return true;
}

/*
 * Enter a new calling frame.
 */
bool
rt_enter_frame(
	struct rt_env *rt,
	struct rt_func *func)
{
	struct rt_frame *frame;

	frame = malloc(sizeof(struct rt_frame));
	if (frame == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memset(frame, 0, sizeof(struct rt_frame));
	frame->func = func;
	frame->tmpvar_size = func->tmpvar_size;
	frame->tmpvar = malloc(sizeof(struct rt_value) * (size_t)func->tmpvar_size);
	if (frame->tmpvar == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memset(frame->tmpvar, 0, sizeof(struct rt_value) * (size_t)func->tmpvar_size);

	frame->next = rt->frame;
	rt->frame = frame;

	return true;
}

/*
 * Leave the current calling frame.
 */
void
rt_leave_frame(
	struct rt_env *rt,
	struct rt_value *ret)
{
	struct rt_frame *frame;

	/* Move objects in the shallow list to the garbage lists. */
	rt_gc_move_shallow_to_garbage_list(rt);

	/* Unlink the frame from the list. */
	frame = rt->frame;
	rt->frame = rt->frame->next;

	/* Free. */
	free(frame->tmpvar);
	free(frame);
}

/*
 * Make an integer value.
 */
void
noct_make_int(
	struct rt_value *val,
	int i)
{
	val->type = NOCT_VALUE_INT;
	val->val.i = i;
}

/*
 * Make an floating-point value.
 */
void
noct_make_float(
	struct rt_value *val,
	float f)
{
	val->type = NOCT_VALUE_FLOAT;
	val->val.f = f;
}

/*
 * Make a string value.
 */
SYSVABI
bool
noct_make_string(
	struct rt_env *rt,
	struct rt_value *val,
	const char *s)
{
	struct rt_string *rts;
	size_t len;

	/* Allocate a string. */
	len = strlen(s) + 1;
	rts = rt_gc_allocate_string(rt, len);
	if (rts == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	/* Copy the text. */
	memcpy(rts->s, s, len);

	/* Setup a value. */
	val->type = NOCT_VALUE_STRING;
	val->val.str = rts;

	return true;
}

/*
 * Make a string value with a format.
 */
bool
noct_make_string_format(
	struct rt_env *rt,
	struct rt_value *val,
	const char *s,
	...)
{
	va_list ap;

	va_start(ap, s);
	vsnprintf(text_buf, sizeof(text_buf), s, ap);
	va_end(ap);

	if (!noct_make_string(rt, val, text_buf))
		return false;

	return true;
}

/*
 * Make an empty array value.
 */
SYSVABI
bool
noct_make_empty_array(struct rt_env *rt, struct rt_value *val)
{
	struct rt_array *arr;

	const int START_SIZE = 16;

	/* Allocate an array. */
	arr = rt_gc_allocate_array(rt, START_SIZE);
	if (arr == NULL) {
		rt_out_of_memory(rt);
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
SYSVABI
bool
noct_make_empty_dict(struct rt_env *rt, struct rt_value *val)
{
	struct rt_dict *dict;

	const int START_SIZE = 16;

	/* Allocate a dictionary. */
	dict = rt_gc_allocate_dict(rt, START_SIZE);
	if (dict == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	/* Setup a value. */
	val->type = NOCT_VALUE_DICT;
	val->val.dict = dict;

	return true;
}

/*
 * Get a value type.
 */
bool
noct_get_value_type(
	struct rt_env *rt,
	struct rt_value *val,
	int *type)
{
	assert(rt != NULL);
	assert(val != NULL);
	assert(val->type == NOCT_VALUE_INT ||
	       val->type == NOCT_VALUE_FLOAT ||
	       val->type == NOCT_VALUE_STRING ||
	       val->type == NOCT_VALUE_ARRAY ||
	       val->type == NOCT_VALUE_DICT);
	assert(type != NULL);

	*type = val->type;

	return true;
}

/*
 * Get an integer value.
 */
bool
noct_get_int(
	struct rt_env *rt,
	struct rt_value *val,
	int *ret)
{
	assert(rt != NULL);
	assert(val != NULL);
	assert(val->type == NOCT_VALUE_INT);

	*ret = val->val.i;

	return true;
}

/*
 * Get a floating-point value.
 */
bool
noct_get_float(
	struct rt_env *rt,
	struct rt_value *val,
	float *ret)
{
	assert(rt != NULL);
	assert(val != NULL);
	assert(val->type == NOCT_VALUE_FLOAT);

	*ret = val->val.f;

	return true;
}

/*
 * Get a string value.
 */
bool
noct_get_string(
	struct rt_env *rt,
	struct rt_value *val,
	const char **ret)
{
	assert(rt != NULL);
	assert(val != NULL);
	assert(val->type == NOCT_VALUE_STRING);
	assert(val->val.str != NULL);
	assert(val->val.str->s != NULL);

	*ret = val->val.str->s;

	return true;
}

/*
 * Get a function value.
 */
bool
noct_get_func(
	struct rt_env *rt,
	struct rt_value *val,
	struct rt_func **ret)
{
	assert(rt != NULL);
	assert(val != NULL);
	assert(val->type == NOCT_VALUE_FUNC);
	assert(val->val.func != NULL);

	*ret = val->val.func;

	return true;
}

/*
 * Get an array size.
 */
bool
noct_get_array_size(struct rt_env *rt, struct rt_value *array, int *size)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(array->type == NOCT_VALUE_ARRAY);

	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	*size = array->val.arr->size;

	return true;
}

/*
 * Get an array element.
 */
bool
noct_get_array_elem(struct rt_env *rt, struct rt_value *array, int index, struct rt_value *val)
{
	assert(rt != NULL);
	assert(array != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index < 0 || index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Load. */
	*val = array->val.arr->table[index];

	return true;
}

/*
 * Get an integer-type array element.
 */
bool
noct_get_array_elem_int(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	int *val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (index >= array->val.arr->table[index].type != NOCT_VALUE_INT) {
		noct_error(rt, _("Element %d is not an integer."), index);
		return false;
	}

	*val = array->val.arr->table[index].val.i;
	return true;
}

/*
 * Get a float-type array element.
 */
bool
noct_get_array_elem_float(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	float *val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (index >= array->val.arr->table[index].type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Element %d is not a float."), index);
		return false;
	}

	*val = array->val.arr->table[index].val.f;
	return true;
}

/*
 * Get a string-type array element.
 */
bool
noct_get_array_elem_string(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	const char **val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (array->val.arr->table[index].type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Element %d is not a string."), index);
		return false;
	}

	*val = array->val.arr->table[index].val.str->s;
	return true;
}

/*
 * Get an array-type array element.
 */
bool
noct_get_array_elem_array(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	struct rt_value *val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (array->val.arr->table[index].type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Element %d is not an array."), index);
		return false;
	}

	*val = array->val.arr->table[index];
	return true;
}

/*
 * Get a dictionary-type array element.
 */
bool
noct_get_array_elem_dict(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	struct rt_value *val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (array->val.arr->table[index].type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Element %d is not a dictionary."), index);
		return false;
	}

	*val = array->val.arr->table[index];
	return true;
}

/*
 * Get a function-type array element.
 */
bool
noct_get_array_elem_func(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	struct rt_func **val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index >= array->val.arr->size) {
		noct_error(rt, _("Array index %d is out-of-range."), index);
		return false;
	}

	/* Check the element value type. */
	if (array->val.arr->table[index].type != NOCT_VALUE_FUNC) {
		noct_error(rt, _("Element %d is not a function."), index);
		return false;
	}

	*val = array->val.arr->table[index].val.func;
	return true;
}

/*
 * Set an array element.
 */
bool
noct_set_array_elem(struct rt_env *rt, struct rt_value *array, int index, struct rt_value *val)
{
	assert(rt != NULL);
	assert(array != NULL);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Not an array."));
		return false;
	}

	/*
	 * Expand the array if needed.
	 * array->val.arr may be replaced.
	 */
	if (!rt_expand_array(rt, array, index + 1))
		return false;

	/* Store. */
	if (array->val.arr->size < index + 1)
		array->val.arr->size = index + 1;
	array->val.arr->table[index] = *val;

	/* Move the element to the young list. */
	rt_gc_move_shallow_to_young_list(rt, val);

	return true;
}

/*
 * Set an integer-type array element.
 */
bool
noct_set_array_elem_int(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	int val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(array != NULL);

	noct_make_int(&v, val);
	if (!noct_set_array_elem(rt, array, index, &v))
		return false;

	return true;
}

/*
 * Set a float-type array element.
 */
bool
noct_set_array_elem_float(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	float val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(array != NULL);

	noct_make_float(&v, val);
	if (!noct_set_array_elem(rt, array, index, &v))
		return false;

	return true;
}

/*
 * Set a string-type array element.
 */
bool
noct_set_array_elem_string(
	struct rt_env *rt,
	struct rt_value *array,
	int index,
	const char *val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(array != NULL);

	if (!noct_set_array_elem_string(rt, array, index, val))
		return false;

	return true;
}

/* Expand an array. */
static bool
rt_expand_array(
	struct rt_env *rt,
	struct rt_value *array,
	size_t size)
{
	struct rt_array *arr, *new_arr;

	assert(rt != NULL);
	assert(array->type == NOCT_VALUE_ARRAY);

	arr = array->val.arr;

	/* Expand the table. */
	if (arr->alloc_size < size) {
		size_t orig_size;

		orig_size = arr->alloc_size;

		/* Get the next size. */
		if (size < arr->alloc_size * 2)
			size = arr->alloc_size * 2;
		else
			size = size * 2;

		/* Realloc the table. */
		new_arr = rt_gc_reallocate_array(rt, arr, size);
		if (new_arr == NULL) {
			rt_out_of_memory(rt);
			return false;
		}

		array->val.arr = new_arr;
	}

	return true;
}

bool
noct_resize_array(
	struct rt_env *rt,
	struct rt_value *array,
	int size)
{
	struct rt_array *arr;

	assert(rt != NULL);
	assert(arr->type == NOCT_VALUE_ARRAY);

	arr = array->val.arr;

	if (size > arr->size) {
		/* Expand the array size if needed. */
		if (!rt_expand_array(rt, array, size))
			return false;

		/* Set the element count. */
		arr->size = size;
	} else {
		/* Remove the reminder. */
		memset(&arr->table[size], 0, sizeof(struct rt_value) * (size_t)(arr->size - size - 1));

		/* Set the element count. */
		arr->size = size;
	}

	return true;
}

/*
 * Get a dictionary size.
 */
bool
noct_get_dict_size(struct rt_env *rt, struct rt_value *dict, int *size)
{
	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(size != NULL);

	if (dict->type != NOCT_VALUE_DICT)
		return false;

	*size = dict->val.dict->size;

	return true;
}

/*
 * Get a dictionary value by an index.
 */
bool
noct_get_dict_value_by_index(struct rt_env *rt, struct rt_value *dict, int index, struct rt_value *val)
{
	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(index < dict->val.dict->size);

	if (dict->type != NOCT_VALUE_DICT)
		return false;
	
	*val = dict->val.dict->value[index];
		
	return true;
}

/*
 * Get a dictionary key by an index.
 */
bool
noct_get_dict_key_by_index(struct rt_env *rt, struct rt_value *dict, int index, const char **key)
{
	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(index < dict->val.dict->size);

	if (dict->type != NOCT_VALUE_DICT)
		return false;
	
	*key = dict->val.dict->key[index];

	return true;
}

/*
 * Get a dictionary element.
 */
bool
noct_get_dict_elem(struct rt_env *rt, struct rt_value *dict, const char *key, struct rt_value *val)
{
	int i;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			*val = dict->val.dict->value[i];
			return true;
		}
	}

	noct_error(rt, _("Dictionary key \"%s\" not found."), key);

	return false;
}

/*
 * Get an integer-type dictionary element.
 */
bool
noct_get_dict_elem_int(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	int *val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_INT) {
		noct_error(rt, _("Value for key %s is not an integer."), key);
		return false;
	}

	*val = elem.val.i;
	return true;	
}

/*
 * Get a float-type dictionary element.
 */
bool
noct_get_dict_elem_float(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	float *val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Value for key %s is not a float."), key);
		return false;
	}

	*val = elem.val.f;
	return true;	
}

/*
 * Get a string-type dictionary element.
 */
bool
noct_get_dict_elem_string(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	const char **val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Value for key %s is not a string."), key);
		return false;
	}

	*val = elem.val.str->s;
	return true;	
}

/*
 * Get an array-type dictionary element.
 */
bool
noct_get_dict_elem_array(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Value for key %s is not an array."), key);
		return false;
	}

	*val = elem;
	return true;	
}

/*
 * Get a dictionary-type dictionary element.
 */
bool
noct_get_dict_elem_dict(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Value for key %s is not a dictionary."), key);
		return false;
	}

	*val = elem;
	return true;	
}

/*
 * Get a function-type dictionary element.
 */
bool
noct_get_dict_elem_func(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	struct rt_func **val)
{
	struct rt_value elem;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (dict->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Not a dictionary."));
		return false;
	}
	if (!noct_get_dict_elem(rt, dict, key, &elem))
		return false;
	if (elem.type != NOCT_VALUE_FUNC) {
		noct_error(rt, _("Value for key %s is not a function."), key);
		return false;
	}

	*val = elem.val.func;
	return true;	
}

/*
 * Set a dictionary element.
 */
bool
noct_set_dict_elem(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	int i;
	size_t len;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	/* Search for the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			dict->val.dict->value[i] = *val;
			return true;
		}
	}

	/* Expand the size. */
	if (!rt_expand_dict(rt, dict, dict->val.dict->size + 1))
		return false;

	/* Append the key. */
	len = strlen(key) + 1;
	dict->val.dict->key[dict->val.dict->size] = malloc(len);
	if (dict->val.dict->key[dict->val.dict->size] == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	memcpy(dict->val.dict->key[dict->val.dict->size], key, len);
	dict->val.dict->value[dict->val.dict->size] = *val;
	dict->val.dict->size++;

	/* GC: Move the element to the young list. */
	rt_gc_move_shallow_to_young_list(rt, val);

	/* GC: Increment the heap size. */
	rt_gc_increment_heap_usage_by_dict_add(rt, len);

	return true;
}

/*
 * Set an integer dictionary element.
 */
bool
noct_set_dict_elem_int(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	int val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);

	noct_make_int(&v, val);
	if (!noct_set_dict_elem(rt, dict, key, &v))
		return false;

	return true;
}

/*
 * Set a float dictionary element.
 */
bool
noct_set_dict_elem_float(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	float val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);

	noct_make_float(&v, val);
	if (!noct_set_dict_elem(rt, dict, key, &v))
		return false;

	return true;
}

/*
 * Set a string dictionary element.
 */
bool
noct_set_dict_elem_string(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key,
	const char *val)
{
	struct rt_value v;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);
	assert(val != NULL);

	if (!noct_make_string(rt, &v, val))
		return false;
	if (!noct_set_dict_elem(rt, dict, key, &v))
		return false;

	return true;
}

/* Expand an array. */
static bool
rt_expand_dict(
	struct rt_env *rt,
	struct rt_value *dict,
	size_t size)
{
	struct rt_dict *d;
	char **new_key;
	struct rt_value *new_value;

	assert(rt != NULL);
	assert(dict->type == NOCT_VALUE_DICT);

	d = dict->val.dict;

	/* Expand the table. */
	if (d->alloc_size < size) {
		size_t orig_size;

		orig_size = d->alloc_size;

		/* Get a next size. */
		if (size < d->alloc_size * 2)
			size = d->alloc_size * 2;
		else
			size = size * 2;

		/* Realloc the key table. */
		new_key = malloc(sizeof(const char *) * (size_t)size);
		if (new_key == NULL) {
			rt_out_of_memory(rt);
			return false;
		}
		memcpy(new_key, d->key, sizeof(const char *) * (size_t)d->alloc_size);
		free(d->key);
		d->key = new_key;

		/* Realloc the value table. */
		new_value = malloc(sizeof(struct rt_value) * (size_t)size);
		if (new_value == NULL) {
			rt_out_of_memory(rt);
			return false;
		}
		memcpy(new_value, d->value, sizeof(struct rt_value) * (size_t)d->alloc_size);
		free(d->value);
		d->value = new_value;

		d->alloc_size = size;

		/* GC: Increment the heap size. */
		rt_gc_increment_heap_usage_by_array_expand(rt, size - orig_size);
	}

	return true;
}

/*
 * Remove a dictionary element.
 */
bool
noct_remove_dict_elem(
	struct rt_env *rt,
	struct rt_value *dict,
	const char *key)
{
	int i;

	assert(rt != NULL);
	assert(dict != NULL);
	assert(dict->type == NOCT_VALUE_DICT);
	assert(key != NULL);

	/* Search for the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			/* Remove the key and value. */
			free(dict->val.dict->key[i]);
			memmove(&dict->val.dict->key[i],
				&dict->val.dict->key[i + 1],
				sizeof(const char *) * (size_t)(dict->val.dict->size - i - 1));
			memmove(&dict->val.dict->value[i],
				&dict->val.dict->value[i + 1],
				sizeof(struct rt_value) * (size_t)(dict->val.dict->size - i - 1));
			dict->val.dict->size--;
			return true;
		}
	}

	noct_error(rt, _("Dictionary key \"%s\" not found."), key);
	return false;
}

/*
 * Make a shallow copy of a dictionary.
 */
bool
noct_make_dict_copy(
	struct rt_env *rt,
	struct rt_value *src,
	struct rt_value *dst)
{
	int size, i;

	assert(rt != NULL);
	assert(src != NULL);
	assert(src->type == NOCT_VALUE_DICT);
	assert(dst != NULL);

	if (!noct_make_empty_dict(rt, dst))
		return false;

	size = src->val.dict->size;
	for (i = 0; i < size; i++) {
		const char *key;
		struct rt_value val;

		if (!noct_get_dict_key_by_index(rt, src, i, &key))
			return false;
		if (!noct_get_dict_value_by_index(rt, src, i, &val))
			return false;
		if (!noct_set_dict_elem(rt, dst, key, &val))
			return false;
	}

	return true;
}

/*
 * Get a call argument.
 */
bool
noct_get_arg(
	struct rt_env *rt,
	int index,
	struct rt_value *val)
{
	assert(index < rt->frame->tmpvar_size);

	*val = rt->frame->tmpvar[index];

	return true;
}

/*
 * Get an integer-type call argument.
 */
bool
noct_get_arg_int(
	struct rt_env *rt,
	int index,
	int *val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_INT) {
		noct_error(rt, _("Argument (%d: %s) not an integer."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = arg->val.i;
	return true;
}

/*
 * Get a float-type call argument.
 */
bool
noct_get_arg_float(
	struct rt_env *rt,
	int index,
	float *val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Argument (%d: %s) not a float."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = arg->val.f;
	return true;
}

/*
 * Get a string-type call argument.
 */
bool
noct_get_arg_string(
	struct rt_env *rt,
	int index,
	const char **val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Argument (%d: %s) not a string."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = arg->val.str->s;
	return true;
}

/* Get an array-type call argument. */
bool
noct_get_arg_array(
	struct rt_env *rt,
	int index,
	struct rt_value *val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Argument (%d: %s) is not an array."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = *arg;
	return true;
}

/* Get a dictionary-type call argument. */
bool
noct_get_arg_dict(
	struct rt_env *rt,
	int index,
	struct rt_value *val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Argument (%d: %s) is not a dictionary."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = *arg;
	return true;
}

/* Get a function-type call argument. */
bool
noct_get_arg_func(
	struct rt_env *rt,
	int index,
	struct rt_func **val)
{
	struct rt_value *arg;

	assert(rt != NULL);
	assert(index < rt->frame->tmpvar_size);
	assert(val != NULL);

	arg = &rt->frame->tmpvar[index];

	if (arg->type != NOCT_VALUE_DICT) {
		noct_error(rt, _("Argument (%d: %s) is not a dictionary."),
			 index,
			 rt->frame->func->param_name[index]);
		return false;
	}

	*val = arg->val.func;
	return true;
}

/*
 * Set a return value.
 */
bool
noct_set_return(
	struct rt_env *rt,
	struct rt_value *val)
{
	int ret_index;

	rt->frame->tmpvar[0] = *val;

	/* Move to the young list. */
	rt_gc_move_shallow_to_young_list(rt, val);

	return true;
}

/*
 * Set an integer-type return value.
 */
bool
noct_set_return_int(
	struct rt_env *rt,
	int val)
{
	struct rt_value v;

	noct_make_int(&v, val);
	if (!noct_set_return(rt, &v))
		return false;

	return true;
}

/*
 * Set a float-type return value.
 */
bool
noct_set_return_float(
	struct rt_env *rt,
	int val)
{
	struct rt_value v;

	noct_make_float(&v, val);
	if (!noct_set_return(rt, &v))
		return false;

	return true;
}

/*
 * Set a string-type return value.
 */
bool
noct_set_return_string(
	struct rt_env *rt,
	const char *val)
{
	struct rt_value v;

	if (!noct_make_string(rt, &v, val))
		return false;
	if (!noct_set_return(rt, &v))
		return false;

	return true;
}

/*
 * Get a return value.
 */
bool
rt_get_return(
	struct rt_env *rt,
	struct rt_value *val)
{
	int ret_index;

	*val = rt->frame->tmpvar[0];

	return true;
}

/*
 * Get a global variable.
 */
bool
noct_get_global(
	struct rt_env *rt,
	const char *name,
	struct rt_value *val)
{
	struct rt_bindglobal *global;

	global = rt->global;
	while (global != NULL) {
		if (strcmp(global->name, name) == 0)
			break;
		global = global->next;
	}
	if (global == NULL) {
		noct_error(rt, _("Global variable \"%s\" not found."), name);
		return false;
	}

	*val = global->val;

	return true;
}

/* Find a global variable. */
bool
rt_find_global(
	struct rt_env *rt,
	const char *name,
	struct rt_bindglobal **global)
{
	struct rt_bindglobal *g;

	g = rt->global;
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
noct_set_global(
	struct rt_env *rt,
	const char *name,
	struct rt_value *val)
{
	struct rt_bindglobal *global;

	/* Move to the young list. */
	rt_gc_move_shallow_to_young_list(rt, val);

	/* Find a bindglobal. */
	if (!rt_find_global(rt, name, &global)) {
		/* Add a global. */
		if (!rt_add_global(rt, name, &global))
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
	struct rt_env *rt,
	const char *name,
	struct rt_bindglobal **global)
{
	struct rt_bindglobal *g;

	g = malloc(sizeof(struct rt_bindglobal));
	if (g == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	g->name = strdup(name);
	if (g->name == NULL) {
		rt_out_of_memory(rt);
		return false;
	}
	g->val.type = NOCT_VALUE_INT;
	g->val.val.i = 0;

	g->next = rt->global;
	rt->global = g;

	*global = g;

	return true;
}

/*
 * Error Handling
 */

/* Output an error message.*/
void
noct_error(
	struct rt_env *rt,
	const char *msg,
	...)
{
	va_list ap;

	va_start(ap, msg);
	vsnprintf(rt->error_message, sizeof(rt->error_message), msg, ap);
	va_end(ap);
}

/* Output an out-of-memory message. */
void
rt_out_of_memory(
	struct rt_env *rt)
{
	noct_error(rt, _("Out of memory."));
}
