/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Intrinsics implementation
 */

#include "runtime.h"
#include "intrinsics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NEVER_COME_HERE		0

static bool rt_intrin_global(struct rt_env *rt);
static bool rt_intrin_push(struct rt_env *rt);
static bool rt_intrin_pop(struct rt_env *rt);
static bool rt_intrin_unset(struct rt_env *rt);
static bool rt_intrin_resize(struct rt_env *rt);
static bool rt_intrin_substring(struct rt_env *rt);

struct intrin_item {
	const char *field_name;
	const char *reg_name;
	int param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(struct rt_env *rt);
	bool is_thiscall;
	struct rt_func *func;
} intrin_items[] = {
	{"global",    "__global",    2, {"this", "name"        }, rt_intrin_global,    true, NULL},
	{"push",      "__push",      2, {"this", "val"         }, rt_intrin_push,      true, NULL},
	{"pop",       "__pop",       1, {"this"                }, rt_intrin_pop,       true, NULL},
	{"unset",     "__unset",     2, {"this", "key"         }, rt_intrin_unset,     true, NULL},
	{"resize",    "__resize",    2, {"this", "size"        }, rt_intrin_resize,    true, NULL},
	{"substring", "__substring", 3, {"this", "start", "len"}, rt_intrin_substring, true, NULL},
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
	struct rt_env *rt,
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

/* global() */
static bool
rt_intrin_global(
	struct rt_env *rt)
{
	struct rt_value val;
	const char *name;

	if (!noct_get_arg(rt, 0, &val))
		return false;
	if (!noct_get_arg_string(rt, 1, &name))
		return false;
	if (!noct_set_global(rt, name, &val))
		return false;

	return true;
}

/* push() */
static bool
rt_intrin_push(
	struct rt_env *rt)
{
	struct rt_value arr, val;

	if (!noct_get_arg(rt, 0, &arr))
		return false;
	if (!noct_get_arg(rt, 1, &val))
		return false;

	switch (arr.type) {
	case NOCT_VALUE_INT:
	case NOCT_VALUE_FLOAT:
	case NOCT_VALUE_FUNC:
	case NOCT_VALUE_STRING:
	case NOCT_VALUE_DICT:
		noct_error(rt, "Not an array.");
		break;
	case NOCT_VALUE_ARRAY:
		if (!noct_set_array_elem(rt, &arr, arr.val.arr->size, &val))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	if (!noct_set_return(rt, &arr))
		return false;

	return true;
}

/* pop() */
static bool
rt_intrin_pop(
	struct rt_env *rt)
{
	struct rt_value arr, val;
	int size;

	if (!noct_get_arg(rt, 0, &arr))
		return false;
	if (!noct_get_arg(rt, 1, &val))
		return false;

	switch (arr.type) {
	case NOCT_VALUE_INT:
	case NOCT_VALUE_FLOAT:
	case NOCT_VALUE_FUNC:
	case NOCT_VALUE_STRING:
	case NOCT_VALUE_DICT:
		noct_error(rt, "Not an array.");
		return false;
	case NOCT_VALUE_ARRAY:
		if (!noct_get_array_size(rt, &arr, &size))
			return false;
		if (size == 0) {
			noct_error(rt, "Empty array.");
			return false;
		}
		if (!noct_get_array_elem(rt, &arr, size - 1, &val))
			return false;
		if (!noct_resize_array(rt, &arr, size - 1))
			return false;
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	if (!noct_set_return(rt, &val))
		return false;

	return true;
}

/* unset() */
static bool
rt_intrin_unset(
	struct rt_env *rt)
{
	struct rt_value arr, val;

	if (!noct_get_arg(rt, 0, &arr))
		return false;
	if (!noct_get_arg(rt, 1, &val))
		return false;

	switch (arr.type) {
	case NOCT_VALUE_INT:
	case NOCT_VALUE_FLOAT:
	case NOCT_VALUE_FUNC:
	case NOCT_VALUE_STRING:
	case NOCT_VALUE_DICT:
		noct_error(rt, _("Not a dictionary."));
		break;
	case NOCT_VALUE_ARRAY:
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	if (val.type != NOCT_VALUE_STRING) {
		noct_error(rt, _("Subscript not a string."));
		return false;
	}

	if (!noct_remove_dict_elem(rt, &arr, val.val.str->s))
		return false;

	return true;
}

/* resize() */
static bool
rt_intrin_resize(
	struct rt_env *rt)
{
	struct rt_value arr, size;

	if (!noct_get_arg(rt, 0, &arr))
		return false;
	if (!noct_get_arg(rt, 1, &size))
		return false;

	switch (arr.type) {
	case NOCT_VALUE_INT:
	case NOCT_VALUE_FLOAT:
	case NOCT_VALUE_FUNC:
	case NOCT_VALUE_STRING:
	case NOCT_VALUE_DICT:
		noct_error(rt, _("Not an array."));
		break;
	case NOCT_VALUE_ARRAY:
		break;
	default:
		assert(NEVER_COME_HERE);
		break;
	}

	if (size.type != NOCT_VALUE_INT) {
		noct_error(rt, _("Value is not an integer."));
		return false;
	}

	if (!noct_resize_array(rt, &arr, size.val.i))
		return false;

	return true;
}

/* substring() */
static bool
rt_intrin_substring(
	struct rt_env *rt)
{
	struct rt_value str_v, start_v, len_v, ret_v;
	int start_i, len_i, slen;
	char *s;

	if (!noct_get_arg(rt, 0, &str_v))
		return false;
	if (!noct_get_arg(rt, 1, &start_v))
		return false;
	if (!noct_get_arg(rt, 2, &len_v))
		return false;

	if (str_v.type != NOCT_VALUE_STRING) {
		noct_error(rt, "Not a string.");
		return false;
	}
	if (start_v.type != NOCT_VALUE_INT) {
		noct_error(rt, "Not an integer.");
		return false;
	}
	if (len_v.type != NOCT_VALUE_INT) {
		noct_error(rt, "Not an integer.");
		return false;
	}

	start_i = start_v.val.i;
	if (start_i == -1)
		start_i = 0;

	slen = (int)strlen(str_v.val.str->s);
	len_i = len_v.val.i;
	if (len_i < 0)
		len_i = slen;
	if (start_i + len_i > slen)
		len_i = slen - start_i;

	s = malloc((size_t)(len_i + 1));
	if (s == NULL) {
		rt_out_of_memory(rt);
		return false;
	}

	strncpy(s, str_v.val.str->s + start_i, (size_t)len_i);

	if (!noct_make_string(rt, &ret_v, s))
		return false;

	free(s);

	if (!noct_set_return(rt, &ret_v))
		return false;

	return true;
}
