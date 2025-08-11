/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Intrinsics Implementation
 */

#include "runtime.h"
#include "intrinsics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NEVER_COME_HERE		0

static bool rt_intrin_push(struct rt_env *rt);
static bool rt_intrin_pop(struct rt_env *rt);
static bool rt_intrin_unset(struct rt_env *rt);
static bool rt_intrin_newArray(struct rt_env *rt);
static bool rt_intrin_resize(struct rt_env *rt);
static bool rt_intrin_substring(struct rt_env *rt);
static bool rt_intrin_fast_gc(struct rt_env *rt);
static bool rt_intrin_full_gc(struct rt_env *rt);
static bool rt_intrin_compact_gc(struct rt_env *rt);

struct intrin_item {
	const char *field_name;
	const char *reg_name;
	int param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(struct rt_env *rt);
	bool is_thiscall;
	struct rt_func *func;
} intrin_items[] = {
	{"push",       "__push",      2, {"this", "val"         }, rt_intrin_push,       true, NULL},
	{"pop",        "__pop",       1, {"this"                }, rt_intrin_pop,        true, NULL},
	{"newArray",   "newArray",    1, {"size"                }, rt_intrin_newArray,   true, NULL},
	{"resize",     "__resize",    2, {"this", "size"        }, rt_intrin_resize,     true, NULL},
	{"substring",  "__substring", 3, {"this", "start", "len"}, rt_intrin_substring,  true, NULL},
	{"fast_gc",    "fast_gc",     0, {NULL},                   rt_intrin_fast_gc,    true, NULL},
	{"full_gc",    "full_gc",     0, {NULL},                   rt_intrin_full_gc,    true, NULL},
	{"compact_gc", "compact_gc",  0, {NULL},                   rt_intrin_compact_gc, true, NULL},
#if !defined(USE_MULTITHREAD)
	{"unset",      "__unset",     2, {"this", "key"         }, rt_intrin_unset,      true, NULL},
#endif
};

bool
rt_register_intrinsics(
	struct rt_env *rt)
{
	int i;

	for (i = 0; i < (int)(sizeof(intrin_items) / sizeof(struct intrin_item)); i++) {
		if (!noct_register_cfunc(rt,
					 intrin_items[i].reg_name,
					 intrin_items[i].param_count,
					 intrin_items[i].param,
					 intrin_items[i].cfunc,
					 &intrin_items[i].func))
			return false;
	}

	return true;
}

bool
rt_get_intrin_thiscall_func(
	struct rt_env *env,
	const char *name,
	struct rt_func **func)
{
	int i;

	for (i = 0; i < (int)(sizeof(intrin_items) / sizeof(struct intrin_item)); i++) {
		if (!intrin_items[i].is_thiscall)
			continue;
		if (strcmp(intrin_items[i].field_name, name) != 0)
			continue;

		/* Found. */
		*func = intrin_items[i].func;
		return true;
	}

	/* Not found/ */
	*func = NULL;
	return true;
}

/* push() */
static bool
rt_intrin_push(
	struct rt_env *env)
{
	struct rt_value arr, val;
	int size;

	noct_pin_local(env, 2, &arr, &val);

	if (!noct_get_arg_check_array(env, 0, &arr))
		return false;
	if (!noct_get_arg(env, 1, &val))
		return false;

	if (!noct_get_array_size(env, &arr, &size))
		return false;
	if (!noct_set_array_elem(env, &arr, size, &val))
		return false;
	if (!noct_set_return(env, &arr))
		return false;

	return true;
}

/* pop() */
static bool
rt_intrin_pop(
	struct rt_env *env)
{
	struct rt_value arr, val;
	int size;

	noct_pin_local(env, 2, &arr, &val);

	if (!noct_get_arg_check_array(env, 0, &arr))
		return false;

	if (!noct_get_array_size(env, &arr, &size))
		return false;
	if (size == 0) {
		noct_error(env, N_TR("Empty array."));
		return false;
	}

	if (!noct_get_array_elem(env, &arr, size - 1, &val))
		return false;
	if (!noct_resize_array(env, &arr, size - 1))
		return false;

	if (!noct_set_return(env, &val))
		return false;

	return true;
}

/* newArray() */
static bool
rt_intrin_newArray(
	struct rt_env *env)
{
	struct rt_value arr, size;
	int size_i;

	noct_pin_local(env, 1, &size);

	if (!noct_get_arg_check_int(env, 0, &size, &size_i))
		return false;

	if (!noct_make_empty_array(env, &arr))
		return false;
	if (!noct_resize_array(env, &arr, size_i))
		return false;
	if (!noct_set_return(env, &arr))
		return false;

	return true;
}

/* resize() */
static bool
rt_intrin_resize(
	struct rt_env *env)
{
	struct rt_value arr, size;
	int size_i;

	noct_pin_local(env, 2, &arr, &size);

	if (!noct_get_arg_check_array(env, 0, &arr))
		return false;
	if (!noct_get_arg_check_int(env, 1, &size, &size_i))
		return false;

	if (!noct_resize_array(env, &arr, size_i))
		return false;

	return true;
}

/* substring() */
static bool
rt_intrin_substring(
	struct rt_env *env)
{
	struct rt_value str, start, len, ret;
	const char *str_s;
	int start_i, len_i, slen;
	char *s;

	if (!noct_get_arg_check_string(env, 0, &str, &str_s))
		return false;
	if (!noct_get_arg_check_int(env, 1, &start, &start_i))
		return false;
	if (!noct_get_arg_check_int(env, 2, &len, &len_i))
		return false;

	if (start_i == -1)
		start_i = 0;

	slen = (int)strlen(str_s);
	if (len_i < 0)
		len_i = slen;
	if (start_i + len_i > slen)
		len_i = slen - start_i;

	s = noct_malloc((size_t)(len_i + 1));
	if (s == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	strncpy(s, str_s + start_i, (size_t)len_i);

	if (!noct_set_return_make_string(env, &ret, s))
		return false;

	noct_free(s);

	return true;
}

/* fast_gc() */
static bool
rt_intrin_fast_gc(
	struct rt_env *env)
{
	noct_fast_gc(env);
	return true;
}

/* full_gc() */
static bool
rt_intrin_full_gc(
	struct rt_env *env)
{
	noct_full_gc(env);
	return true;
}

/* compact_gc() */
static bool
rt_intrin_compact_gc(
	struct rt_env *env)
{
	noct_compact_gc(env);
	return true;
}

/*
 * The following are not thread-safe.
 */
#if !defined(USE_MULTITHREAD)

/* unset() */
static bool
rt_intrin_unset(
	struct rt_env *env)
{
	struct rt_value arr, val;
	const char *val_s;

	noct_pin_local(env, 2, &arr, &val);

	if (!noct_get_arg_check_dict(env, 0, &arr))
		return false;
	if (!noct_get_arg_check_string(env, 1, &val, &val_s))
		return false;

	if (!noct_remove_dict_elem(env, &arr, val_s))
		return false;

	return true;
}

#endif
