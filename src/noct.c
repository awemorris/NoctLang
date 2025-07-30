/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * NoctVM Public Interface
 */

#include <noct/noct.h>
#include "runtime.h"

static char text_buf[65536];

NOCT_DLL
bool
noct_create_vm(
	NoctVM **vm,
	NoctEnv *default_env)
{
	assert(vm != NULL);
	assert(default_env != NULL);

	if (!rt_create_vm(vm, default_env))
		return false;

	return true;
}

NOCT_DLL
bool
noct_destroy_vm(
	NoctVM *vm)
{
	assert(vm != NULL);

	if (!rt_destroy_vm(vm))
		return false;

	return true;
}

NOCT_DLL
bool
noct_register_source(
	NoctVM *vm,
	const char *file_name,
	const char *source_text)
{
	assert(vm != NULL);
	assert(file_name != NULL);
	assert(source_text != NULL);

	if (!rt_register_source(vm, file_name, source_text))
		return false;

	return true;
}

NOCT_DLL
bool
noct_register_bytecode(
	NoctVM *vm,
	uint8_t *data,
	size_t size)
{
	assert(vm != NULL);
	assert(size > 0);
	assert(data != NULL);

	if (!rt_register_bytecode(vm, size, data))
		return false;

	return true;
}

NOCT_DLL
bool
noct_register_cfunc(
	NoctVM *vm,
	const char *name,
	int param_count,
	const char *param_name[],
	bool (*cfunc)(NoctEnv *env),
	NoctFunc **ret_func)
{
	assert(vm != NULL);
	assert(name != NULL);
	assert(param_count >= 0);
	assert(cfunc != NULL);

	if (!rt_register_cfunc(vm, name, param_count, param_name, cfunc, ret_func))
		return false;

	return true;
}

NOCT_DLL
bool
noct_create_thread_env(
	NoctVM *vm,
	NoctEnv **env)
{
	assert(vm != NULL);
	assert(env != NUL);

	if (!rt_create_thread_env(vm, env))
		return false;

	return true;
}

NOCT_DLL
bool
noct_enter_vm(
	NoctEnv *env,
	const char *func_name,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret)
{
	assert(env != NULL);
	assert(func_name != NULL);
	assert(arg_count >= 0);

	if (!rt_enter_vm(env, func_name, arg_count, arg, ret))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_error_file(
	NoctEnv *env,
	const char *msg,
	size_t len)
{
	assert(env != NULL);
	assert(msg != NULL);
	assert(len > 0);

	strncpy(msg,
		rt_get_error_file(env, msg),
		len);

	return true;
}

NOCT_DLL
bool
noct_get_error_line(
	NoctEnv *env,
	int *line)
{
	assert(env != NULL);
	assert(line != NULL);

	*line = rt_get_error_line(env);

	return true;	
}

NOCT_DLL
bool
noct_get_error_message(
	NoctEnv *env,
	const char *msg,
	size_t size)
{
	assert(env != NULL);
	assert(msg != NULL);
	assert(size > 0);

	strncpy(msg,
		rt_get_error_message(env),
		size);

	return true;
}

NOCT_DLL
bool
noct_call(
	NoctEnv *env,
	NoctFunc *func,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret)
{
	assert(env != NULL);
	assert(func != NULL);
	assert(arg_count >= 0);

	if (!rt_call(env, func, arg_count, arg, ret))
		return false;

	return true;
}

NOCT_DLL
bool
noct_assign(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src)
{
	assert(env != NULL);
	assert(dst != NULL);
	assert(src != NULL);

	if (!rt_assign(env, dst, src))
		return false;

	return true;
}

NOCT_DLL
bool
noct_make_int(
	NoctEnv *env,
	NoctValue *val,
	int i)
{
	assert(env != NULL);
	assert(val != NULL);

	val->type = NOCT_VALUE_INT;
	val->val.i = i;

	return true;
}

NOCT_DLL
bool
noct_make_float(
	NoctEnv *env,
	NoctValue *val,
	float f)
{
	assert(env != NULL);
	assert(val != NULL);

	val->type = NOCT_VALUE_FLOAT;
	val->val.f = f;

	return true;
}

NOCT_DLL
bool
noct_make_string(
	NoctEnv *env,
	NoctValue *val,
	const char *data,
	size_t len)
{
	assert(env != NULL);
	assert(val != NULL);
	assert(data != NULL);

	if (!rt_make_string(env, val, data, len))
		return false;

	return true;
}

NOCT_DLL
bool
noct_make_empty_array(
	NoctEnv *env,
	NoctValue *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_make_empty_array(env, val))
		return false;

	return true;
}

NOCT_DLL
bool
noct_make_empty_dict(
	NoctEnv *env,
	NoctValue *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_make_empty_dict(env, val))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_value_type(
	NoctEnv *env,
	NoctValue *val,
	int *type)
{
	assert(env != NULL);
	assert(val != NULL);
	assert(type != NULL);

	*type = val->type;

	return true;
}

NOCT_DLL
bool
noct_get_int(
	NoctEnv *env,
	NoctValue *val,
	int *i)
{
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(i != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_INT) {
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	/* Get the value. */
	*i = val->val.i;

	return true;
}

NOCT_DLL
bool
noct_get_float(
	NoctEnv *env,
	NoctValue *val,
	float *f)
{
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(f != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Value is not a float."));
		return false;
	}

	/* Get the value. */
	*f = val->val.f;

	return true;
}

NOCT_DLL
bool
noct_get_string_len(
	NoctEnv *env,
	NoctValue *val,
	size_t *len)
{
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(len != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Value is not a string."));
		return false;
	}

	/* Get the size. */
	*len = val->val.str->len;

	return true;
}

NOCT_DLL
bool
noct_get_string(
	NoctEnv *env,
	NoctValue *val,
	char *s,
	size_t len)
{
	int type;
	size_t src_len;
	size_t copy_len;

	assert(env != NULL);
	assert(val != NULL);
	assert(len != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Value is not a string."));
		return false;
	}

	/* Determine the size to copy. */
	src_len = val->val.str->len;
	if (len < src_len)
		copy_len = len;
	else
		copy_len = src_len;

	/* Copy. */
	memcpy(s, val->val.str->s, copy_len);

	return true;
}

NOCT_DLL
bool
noct_get_func(
	NoctEnv *env,
	NoctValue *val,
	NoctFunc **f)
{
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(f != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_FUNC) {
		noct_error(rt, _("Value is not a function."));
		return false;
	}

	/* Get the function. */
	*f = val->val.func;

	return true;
}

NOCT_DLL
bool
noct_get_array_size(
	NoctEnv *env,
	NoctValue *val,
	int *size)
{
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(size != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Value is not an array."));
		return false;
	}

	/* Get the size. */
	*size = val->val.arr->size;

	return true;
}

NOCT_DLL
bool
noct_get_array_elem(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val)
{
	int type;

	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(val != NULL);

	/* Get the value type. */
	type = array->type;

	/* Check the type. */
	if (type != NOCT_VALUE_ARRAY) {
		noct_error(rt, _("Value is not an array."));
		return false;
	}

	/* Check the array boundary. */
	if (index < 0 || index >= array->val.arr->size) {
		noct_error(env, _("Array index %d is out-of-range."), index);
		return false;
	}
	
	/* Load. */
	*val = array->val.arr->table[index];

	return true;
}

NOCT_DLL
bool
noct_set_array_elem(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(val != NULL);

	/* Check the value type. */
	if (array->type != NOCT_VALUE_ARRAY) {
		noct_error(env, _("Not an array."));
		return false;
	}

	/*
	 * Expand the array if needed.
	 * Note that array->val.arr may be replaced.
	 */
	if (!rt_expand_array(env, array, index + 1))
		return false;
	if (array->val.arr->size < index + 1)
		array->val.arr->size = index + 1;

	/* Store. */
	array->val.arr->table[index] = *val;

	return true;
}

NOCT_DLL
bool
noct_resize_array(
	NoctEnv *env,
	NoctValue *arr,
	int size)
{
	struct rt_array *arr;

	assert(env != NULL);
	assert(arr != NULL);

	arr = array->val.arr;

	if (size > arr->size) {
		/* Expand the array size if needed. */
		if (!rt_expand_array(env, array, size))
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

NOCT_DLL
bool
noct_make_array_copy(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src)
{
	struct rt_array *new_arr;
	int i;

	assert(env != NULL);
	assert(dst != NULL);
	assert(src != NULL);

	/* Check the type. */
	if (src->val.type != NOCT_VALUE_ARRAY) {
		noct_error(env, _("Not an array."));
		return false;
	}

	/* Allocate an array. */
	new_arr = rt_gc_alloc_array(env, src->val.arr->size);
	if (new_arr == NULL)
		return false;

	/* Copy the array with write-barrier. */
	for (i = 0; i < (int)src->val.arr->size; i++) {
		/* Copy. */
		new_arr->table[i] = src->val.arr->table[index];

		/* Write barrier. */
		rt_gc_array_write_barrier(env, new_arr, i, new_arr->table[i]);
	}

	/* Setup the value. */
	dst->type = NOCT_VALUE_ARRAY;
	dst->val.arr = new_arr;

	return true;
}

NOCT_DLL
bool
noct_get_dict_size(
	NoctEnv *env,
	NoctValue *dict,
	int *size)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(size != NULL);

	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Get the size. */
	*size = dict->val.dict->size;

	return true;
}

NOCT_DLL
bool
noct_get_dict_key_by_index(
	NoctEnv *env,
	NoctValue *dict,
	int index,
	const char **key)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(index >= 0);
	assert(key != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Check the boundary. */
	if (index < 0 || index >= dict->val.dict->size) {
		noct_error(env, _("Dictionary index %d is out-of-range."), index);
		return false;
	}

	/* Load. */
	*val = dict->val.dict->key[index];

	return true;
}

NOCT_DLL
bool
noct_get_dict_value_by_index(
	NoctEnv *env,
	NoctValue *dict,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(index >= 0);
	assert(val != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Check the boundary. */
	if (index < 0 || index >= dict->val.dict->size) {
		noct_error(env, _("Dictionary index %d is out-of-range."), index);
		return false;
	}

	/* Load. */
	*val = dict->val.dict->value[index];

	return true;
}

NOCT_DLL
bool
noct_check_dict_key(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	bool *ret)
{
	int i;

	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(ret != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Search the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			/* Found. */
			return true;
		}
	}

	/* Not found. */
	return false;
}

NOCT_DLL
bool
noct_get_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Search the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			/* Succeede. */
			*val = dict->val.dict->value[i];
			return true;
		}
	}

	/* Failed. */
	rt_error(env, _("Dictionary key \"%s\" not found."), key);
	return false;
}

NOCT_DLL
bool
noct_set_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val)
{
	int i;
	size_t len;

	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Search for the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			dict->val.dict->value[i] = *val;
			return true;
		}
	}

	/*
	 * Expand the size.
	 * Note that dict->val.dict may change.
	 */
	if (!rt_expand_dict(env, dict, dict->val.dict->size + 1))
		return false;

	/* Append the key. */
	dict->val.dict->key[dict->val.dict->size] = rt_gc_alloc_dict_key(env, dict->val.dict, key);
	if (dict->val.dict->key[dict->val.dict->size] == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	dict->val.dict->value[dict->val.dict->size] = *val;
	dict->val.dict->size++;

	/* Write-barrier. */
	rt_gc_dict_write_barrier(env, dict->val.dict, i, val);

	return true;
}

NOCT_DLL
bool
noct_remove_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	
	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Search for the key. */
	for (i = 0; i < dict->val.dict->size; i++) {
		if (strcmp(dict->val.dict->key[i], key) == 0) {
			/* Remove the key and value. */
			rt_gc_free_dict_key(env, dict->val.dict->key[i]);
			memmove(&dict->val.dict->key[i],
				&dict->val.dict->key[i + 1],
				sizeof(const char *) * (size_t)(dict->val.dict->size - i - 1));
			memmove(&dict->val.dict->value[i],
				&dict->val.dict->value[i + 1],
				sizeof(struct rt_value) * (size_t)(dict->val.dict->size - i - 1));
			dict->val.dict->key[dict->val.dict->size - 1] = NULL;
			dict->val.dict->value[dict->val.dict->size - 1] = NULL;
			dict->val.dict->size--;
			return true;
		}
	}

	noct_error(env, _("Dictionary key \"%s\" not found."), key);
	return false;
}

NOCT_DLL
bool
noct_make_dict_copy(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src)
{
	struct rt_dict *new_dict;

	assert(env != NULL);
	assert(src != NULL);
	assert(dst != NULL);

	/* Check the type. */
	if (dict->type != NOCT_VALUE_DICT) {
		rt_error(env, _("Not a dictionary."));
		return false;
	}

	/* Make a dictionary */
	new_dict = rt_gc_alloc_dict(env, src->val.dict->size);
	if (new_dict == NULL)
		return false;

	/* Copy the array with write-barrier. */
	for (i = 0; i < (int)src->val.arr->size; i++) {
		/* Copy the key. */
		new_dict->key[i] = rt_gc_alloc_dict_key(env, new_dict, src->val.dict->key[i]);
		if (new_dict->key[i] == NULL)
			return false;

		/* Copy the value. */
		new_dict->value[i] = src->val.dict->value[index];

		/* Write barrier. */
		rt_gc_dict_write_barrier(env, new_dict, i, new_dict->value[i]);
	}

	/* Setup the value. */
	dst->type = NOCT_VALUE_DICT;
	dst->val.dict = new_dict;

	return true;
}

NOCT_DLL
bool
noct_get_arg(
	NoctEnv *env,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(index >= 0);
	assert(val != NULL);

	*val = env->frame->tmpvar[index];

	return true;
}

NOCT_DLL
bool
noct_set_return(
	NoctEnv *env,
	NoctValue *val)
{
	assert(env != NULL);
	assert(val != NULL);

	env->frame->tmpvar[0] = *val;

	return true;
}

NOCT_DLL
bool
noct_get_global(
	NoctEnv *env,
	const char *name,
	NoctValue *val)
{
	assert(env != NULL);
	assert(name != NULL);
	assert(val != NULL);

	if (!rt_get_global(env, name, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	return true;
}

NOCT_DLL
bool
noct_set_global(
	NoctEnv *env,
	const char *name,
	NoctValue *val)
{
	assert(env != NULL);
	assert(name != NULL);
	assert(val != NULL);

	if (!rt_set_global(env, name, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	return true;
}

NOCT_DLL
void
noct_declare_c_global(
	NoctEnv *env,
	int count,
	...)
{
	int i;
	va_list ap;

	assert(env != NULL);
	assert(count > 0);

	va_start(ap, count);
	for (i = 0; i < count; i++) {
		if (!rt_declare_c_global(env, val))
			return false;
	}
	va_end(ap);

	return true;
}

NOCT_DLL
void
noct_declare_local(
	NoctEnv *env,
	int count,
	...)
{
	int i;
	va_list ap;

	assert(env != NULL);
	assert(count > 0);

	va_start(ap, count);
	for (i = 0; i < count; i++) {
		if (!rt_declare_c_local(env, val))
			return false;
	}
	va_end(ap);

	return true;
}

NOCT_DLL
bool
noct_fast_gc(
	NoctEnv *env)
{
	if (!rt_gc_level1_gc(env))
		return false;

	return true;
}

NOCT_DLL
bool
noct_full_gc(
	NoctEnv *env)
{
	if (!rt_gc_full_gc(env))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_heap_usage(
	NoctEnv *env,
	size_t *ret)
{
	if (!rt_gc_get_heap_usage(env, ret))
		return false;

	return true;
}

NOCT_DLL
void
noct_error(
	NoctEnv *env,
	const char *msg,
	...)
{
	va_list ap;

	va_start(ap, msg);
	rt_error_va(env, msg, ap);
	va_end(ap);
}

/*
 * Convenience Functions
 */

/*
 * String
 */

NOCT_DLL
bool
noct_make_string_format(
	NoctEnv *env,
	NoctValue *val,
	const char *s,
	...)
{
	va_list ap;

	assert(env != NULL);
	assert(val != NULL);
	assert(s != NULL);

	va_start(ap, s);
	vsnprintf(text_buf, sizeof(text_buf), s, ap);
	va_end(ap);

	if (!rt_make_string(env, val, text_buf))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_string_dup(
	NoctEnv *env,
	NoctValue *val,
	char **s)
{
}
	int type;

	assert(env != NULL);
	assert(val != NULL);
	assert(len != NULL);

	/* Get the value type. */
	type = val->type;

	/* Check the type. */
	if (type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Value is not a string."));
		return false;
	}

	/* Copy. */
*s = strdup(val->val.str->s);

	return true;

/*
 * Array Getters
 */

NOCT_DLL
bool
noct_get_array_elem_check_int(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val,
	int *i)
{
	int type;

	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(i != NULL);

	/* Get the element. */
	if (!noct_get_array_elem(env, array, index, var))
		return false;

	/* Check the element value type. */
	if (!noct_get_value_type(env, var, &type))
		return false;
	if (type != NOCT_VALUE_INT) {
		noct_error(rt, _("Element %d is not an integer."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_int(env, tmp, i))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_array_elem_check_float(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val,
	float *f)
{
	int type;

	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(f != NULL);

	/* Get the element. */
	if (!noct_get_array_elem(env, array, index, val))
		return false;

	/* Check the element value type. */
	if (!noct_get_value_type(env, var, &type))
		return false
	if (type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Element %d is not a float."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_float(env, tmp, f))
		return false;

	return true;
}

NOCT_DLL
bool
noct_get_array_elem_string(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctVal *val,
	size_t len,
	const char **s)
{
	int type;

	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(f != NULL);

	/* Get the element. */
	if (!noct_get_array_elem(env, array, index, var))
		return false;

	/* Check the element value type. */
	if (!noct_get_value_type(env, var, &type))
		return false
	if (type != NOCT_VALUE_FLOAT) {
		noct_error(rt, _("Element %d is not a float."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_float(env, tmp, f))
		return false;

	return true;
}

	/* Get the element. */
	if (!rt_get_array_elem(env, array, index, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_STRING) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a string."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_string(env, backing_val, s)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_array_elem_array(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctVal *val)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_array_elem(env, array, index, &val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (val->type != NOCT_VALUE_ARRAY) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not an array."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_array_elem_dict(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_array_elem(env, array, index, &val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (val->type != NOCT_VALUE_ARRAY) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a dictionary."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_array_elem_func(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctFunc **f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_array_elem(env, array, index, &val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (val->type != NOCT_VALUE_FUNC) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a function."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_func(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

/*
 * Array Setters
 */

NOCT_DLL
bool
noct_set_array_elem_int(
	NoctEnv *env,
	NoctValue *array,
	int index,
	int i)
{
	struct rt_value *tmp;

	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	tmp = RT_TLS_TMP(env);		

	/* Make an integer value. */
	if (!rt_make_int(env, tmp, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Get the element. */
	if (!rt_set_array_elem(env, array, index, tmp)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();
	
	return true;
}

NOCT_DLL
bool
noct_set_array_elem_float(
	NoctEnv *env,
	NoctValue *array,
	int index,
	float f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make an integer value. */
	if (!rt_make_float(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Get the element. */
	if (!rt_set_array_elem(env, array, index, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();
	
	return true;
}

NOCT_DLL
bool
noct_set_array_elem_make_string(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val,
	const char *s)
{
	assert(env != NULL);
	assert(array != NULL);
	assert(index >= 0);
	assert(backing_val != NULL);

	/* Make an integer value. */
	if (!noct_make_string(env, val, s))
		return false;

	/* Get the element. */
	if (!noct_set_array_elem(env, array, index, val))
		return false;
	
	return true;
}

/*
 * Dictionary Getters
 */

NOCT_DLL
bool
noct_get_dict_elem_int(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	int *i,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(i != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_INT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not an integer."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_int(env, backing_val, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_dict_elem_float(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	float *f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(f != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_FLOAT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a float."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_float(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_dict_elem_string(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	const char **s,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(s != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_STRING) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a string."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_string(env, backing_val, s)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_dict_elem_array(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_ARRAY) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not an array."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_dict_elem_dict(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_DICT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a dictionary."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_dict_elem_func(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctFunc **f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(f != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the element. */
	if (!rt_get_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the element value type. */
	if (backing_val->type != NOCT_VALUE_FUNC) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a function."), index);
		return false;
	}

	/* Get the value. */
	if (!rt_get_func(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

/*
 * Dictionary Setters
 */

NOCT_DLL
bool
noct_set_dict_elem_int(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	int i,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(i != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make an integer value. */
	if (!rt_make_int(env, backing_val, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the key-value pair. */
	if (!rt_set_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_set_dict_elem_float(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	float f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(i != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make a float value. */
	if (!rt_make_float(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the key-value pair. */
	if (!rt_set_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_set_dict_elem_string(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	const char *s,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(s != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make a string value. */
	if (!rt_make_string(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the key-value pair. */
	if (!rt_set_dict_elem(env, dict, key, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

/*
 * Argument Getters
 */

NOCT_DLL
bool
noct_get_arg_int(
	NoctEnv *env,
	int index,
	int *i,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(dict != NULL);
	assert(key != NULL);
	assert(i != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (backing_val->type != NOCT_VALUE_INT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not an integer."), index);
		return false;
	}

	/* Get the integer. */
	if (!rt_get_int(env, backing_val, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_arg_float(
	NoctEnv *env,
	int index,
	float *f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(index >= 0);
	assert(f != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (backing_val->type != NOCT_VALUE_FLOAT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a float."), index);
		return false;
	}

	/* Get the float. */
	if (!rt_get_float(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_arg_string(
	NoctEnv *env,
	int index,
	const char **s,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(index >= 0);
	assert(s != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (backing_val->type != NOCT_VALUE_STRING) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a string."), index);
		return false;
	}

	/* Get the float. */
	if (!rt_get_float(env, backing_val, f)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_arg_array(
	NoctEnv *env,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(index >= 0);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (val->type != NOCT_VALUE_ARRAY) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not an array."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}


NOCT_DLL
bool
noct_get_arg_dict(
	NoctEnv *env,
	int index,
	NoctValue *val)
{
	assert(env != NULL);
	assert(index >= 0);
	assert(val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (val->type != NOCT_VALUE_DICT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a dictionary."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_get_arg_func(
	NoctEnv *env,
	int index,
	NoctFunc **f,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(key != NULL);
	assert(f != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Get the argument. */
	if (!rt_get_arg(env, index, val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Check the value type. */
	if (val->type != NOCT_VALUE_DICT) {
		RT_GC_LEAVE_VALUE_GUARD();
		noct_error(rt, _("Element %d is not a dictionary."), index);
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

/*
 * Return Setter
 */

NOCT_DLL
bool
noct_set_return_int(
	NoctEnv *env,
	int i,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make an integer value. */
	if (!rt_make_int(env, backing_val, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the return value. */
	if (!rt_set_return(env, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_set_return_float(
	NoctEnv *env,
	float val,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make a float value. */
	if (!rt_make_int(env, backing_val, i)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the return value. */
	if (!rt_set_return(env, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}

NOCT_DLL
bool
noct_set_return_string(
	NoctEnv *env,
	const char *s,
	NoctValue *backing_val)
{
	assert(env != NULL);
	assert(s != NULL);
	assert(backing_val != NULL);

	RT_GC_ENTER_VALUE_GUARD();

	/* Make a float value. */
	if (!rt_make_string(env, backing_val, s)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	/* Set the return value. */
	if (!rt_set_return(env, backing_val)) {
		RT_GC_LEAVE_VALUE_GUARD();
		return false;
	}

	RT_GC_LEAVE_VALUE_GUARD();

	return true;
}
