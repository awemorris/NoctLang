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
#include <math.h>
#include <assert.h>

#define NEVER_COME_HERE		0

static bool rt_intrin_new(struct rt_env *env);
static bool rt_intrin_int(struct rt_env *env);
static bool rt_intrin_float(struct rt_env *env);
static bool rt_intrin_push(struct rt_env *env);
static bool rt_intrin_pop(struct rt_env *env);
static bool rt_intrin_unset(struct rt_env *env);
static bool rt_intrin_newArray(struct rt_env *env);
static bool rt_intrin_resize(struct rt_env *env);
static bool rt_intrin_charAt(struct rt_env *env);
static bool rt_intrin_substring(struct rt_env *env);
static bool rt_intrin_sin(struct rt_env *env);
static bool rt_intrin_cos(struct rt_env *env);
static bool rt_intrin_tan(struct rt_env *env);
static bool rt_intrin_random(struct rt_env *env);
static bool rt_intrin_fast_gc(struct rt_env *env);
static bool rt_intrin_full_gc(struct rt_env *env);
static bool rt_intrin_compact_gc(struct rt_env *env);

struct intrin_item {
	const char *field_name;
	const char *reg_name;
	int param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(struct rt_env *rt);
	bool is_thiscall;
	struct rt_func *func;
} intrin_items[] = {
	{"new",        "new",         2, {"cls", "init"         }, rt_intrin_new,        false, NULL},
	{"int",        "int",         1, {"val"                 }, rt_intrin_int,        false, NULL},
	{"float",      "float",       1, {"val"                 }, rt_intrin_float,      false, NULL},
	{"push",       "__push",      2, {"this", "val"         }, rt_intrin_push,       true,  NULL},
	{"pop",        "__pop",       1, {"this"                }, rt_intrin_pop,        true,  NULL},
	{"newArray",   "newArray",    1, {"size"                }, rt_intrin_newArray,   true,  NULL},
	{"resize",     "__resize",    2, {"this", "size"        }, rt_intrin_resize,     true,  NULL},
	{"charAt",     "__charAt",    2, {"this", "index"       }, rt_intrin_charAt,     true,  NULL},
	{"substring",  "__substring", 3, {"this", "start", "len"}, rt_intrin_substring,  true,  NULL},
	{"sin",        "sin",         1, {"x"                   }, rt_intrin_sin,        false, NULL},
	{"cos",        "cos",         1, {"x"                   }, rt_intrin_cos,        false, NULL},
	{"tan",        "tan",         1, {"x"                   }, rt_intrin_tan,        false, NULL},
	{"random",     "random",      0, {NULL                  }, rt_intrin_random,     false, NULL},
	{"fast_gc",    "fast_gc",     0, {NULL},                   rt_intrin_fast_gc,    false, NULL},
	{"full_gc",    "full_gc",     0, {NULL},                   rt_intrin_full_gc,    false, NULL},
	{"compact_gc", "compact_gc",  0, {NULL},                   rt_intrin_compact_gc, true,  NULL},
#if !defined(USE_MULTITHREAD)
	{"unset",      "__unset",     2, {"this", "key"         }, rt_intrin_unset,      true,  NULL},
#endif
};

static int utf8_to_utf32(const char *mbs, uint32_t *wc);

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

/* new() */
static bool
rt_intrin_new(
	struct rt_env *env)
{
	struct rt_value cls, init, ret, key, val;
	const char *key_s;
	int type, i, count;

	noct_pin_local(env, 5, &cls, &init, &ret, &key, &val);

	if (!noct_get_arg_check_dict(env, 0, &cls))
		return false;
	if (!noct_get_arg_check_dict(env, 1, &init))
		return false;

	/* Make a dictionary. */
	if (!noct_make_empty_dict(env, &ret))
		return false;

	/* Copy the class elements. */
	if (!noct_get_dict_size(env, &cls, &count))
		return false;
	for (i = 0; i < count; i++) {
		if (!noct_get_dict_key_by_index(env, &cls, i, &key))
			return false;
		if (!noct_get_dict_value_by_index(env, &cls, i, &val))
			return false;
		if (!noct_get_string(env, &key, &key_s))
			return false;
		if (!noct_set_dict_elem(env, &ret, key_s, &val))
			return false;
	}

	/* Copy the initializer. */
	if (!noct_get_dict_size(env, &init, &count))
		return false;
	for (i = 0; i < count; i++) {
		if (!noct_get_dict_key_by_index(env, &init, i, &key))
			return false;
		if (!noct_get_dict_value_by_index(env, &init, i, &val))
			return false;
		if (!noct_get_string(env, &key, &key_s))
			return false;
		if (!noct_set_dict_elem(env, &ret, key_s, &val))
			return false;
	}

	/* Set the return value. */
	if (!noct_set_return(env, &ret))
		return false;

	return true;
}

/* int() */
static bool
rt_intrin_int(
	struct rt_env *env)
{
	struct rt_value val, ret;
	const char *val_s;
	int val_i;

	noct_pin_local(env, 2, &val, &ret);

	if (!noct_get_arg_check_string(env, 0, &val, &val_s))
		return false;

	val_i = atoi(val_s);

	if (!noct_set_return_make_int(env, &ret, val_i))
		return false;

	return true;
}

/* float() */
static bool
rt_intrin_float(
	struct rt_env *env)
{
	struct rt_value val, ret;
	const char *val_s;
	float val_f;

	noct_pin_local(env, 2, &val, &ret);

	if (!noct_get_arg_check_string(env, 0, &val, &val_s))
		return false;

	val_f = (float)atof(val_s);

	if (!noct_set_return_make_float(env, &ret, val_f))
		return false;

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

/* charAt() */
static bool
rt_intrin_charAt(
	struct rt_env *env)
{
	struct rt_value str, index, ret;
	const char *str_s;
	int index_i;
	size_t len;
	const char *s;
	char d[8];
	int i, ofs;

	noct_pin_local(env, 2, &str, &index, &ret);

	if (!noct_get_arg_check_string(env, 0, &str, &str_s))
		return false;
	if (!noct_get_arg_check_int(env, 1, &index, &index_i))
		return false;

	if (!noct_get_string_len(env, &str, &len))
		return false;

	if (index_i < 0) {
		if (!noct_make_string(env, &ret, ""))
			return false;
		if (!noct_set_return(env, &ret))
			return false;
		return true;
	}

	s = str_s;
	i = 0;
	ofs = 0;
	while (*s != '\0' && i <= index_i) {
		uint32_t codepoint;
		int mblen;

		mblen = utf8_to_utf32(s, &codepoint);
		if (mblen <= 0) {
			/* UTF-8 error. */
			break;
		}

		if (i == index_i) {
			/* Succeeded. */
			strncpy(d, &str_s[ofs], mblen);
			d[mblen] = '\0';
			if (!noct_set_return_make_string(env, &ret, d))
				return false;
			return true;
		}

		s += mblen;
		ofs += mblen;
		i++;
	}

	/* Failed. */
	if (!noct_make_string(env, &ret, ""))
		return false;
	if (!noct_set_return(env, &ret))
		return false;
	return true;
}

/* Get a top character of a utf-8 string as a utf-32. */
static int utf8_to_utf32(const char *mbs, uint32_t *wc)
{
	size_t mbslen, octets, i;
	uint32_t ret;

	assert(mbs != NULL);

	/* If mbs is empty. */
	mbslen = strlen(mbs);
	if(mbslen == 0)
		return 0;

	/* Check the first byte, get an octet count. */
	if (mbs[0] == '\0')
		octets = 0;
	else if ((mbs[0] & 0x80) == 0)
		octets = 1;
	else if ((mbs[0] & 0xe0) == 0xc0)
		octets = 2;
	else if ((mbs[0] & 0xf0) == 0xe0)
		octets = 3;
	else if ((mbs[0] & 0xf8) == 0xf0)
		octets = 4;
	else
		return -1;	/* Not suppoerted. */

	/* Check the mbs length. */
	if (mbslen < octets)
		return -1;	/* mbs is too short. */

	/* Check for 2-4 bytes. */
	for (i = 1; i < octets; i++) {
		if((mbs[i] & 0xc0) != 0x80)
			return -1;	/* Non-understandable */
	}

	/* Compose a utf-32 character. */
	switch (octets) {
	case 0:
		ret = 0;
		break;
	case 1:
		ret = (uint32_t)mbs[0];
		break;
	case 2:
		ret = (uint32_t)(((mbs[0] & 0x1f) << 6) |
				 (mbs[1] & 0x3f));
		break;
	case 3:
		ret = (uint32_t)(((mbs[0] & 0x0f) << 12) |
				 ((mbs[1] & 0x3f) << 6) |
				 (mbs[2] & 0x3f));
		break;
	case 4:
		ret = (uint32_t)(((mbs[0] & 0x07) << 18) |
				 ((mbs[1] & 0x3f) << 12) |
				 ((mbs[2] & 0x3f) << 6) |
				 (mbs[3] & 0x3f));
		break;
	default:
		/* never come here */
		assert(0);
		return -1;
	}

	/* Store the result. */
	if(wc != NULL)
		*wc = ret;

	/* Return the octet count. */
	return (int)octets;
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

/* sin() */
static bool
rt_intrin_sin(
	struct rt_env *env)
{
	struct rt_value x, ret;
	float x_f;
	float sinx;

	noct_pin_local(env, 2, &x, &ret);

	if (!noct_get_arg_check_float(env, 0, &x, &x_f))
		return false;

	sinx = sinf(x_f);

	if (!noct_set_return_make_float(env, &ret, sinx))
		return false;

	return true;
}

/* cos() */
static bool
rt_intrin_cos(
	struct rt_env *env)
{
	struct rt_value x, ret;
	float x_f;
	float cosx;

	noct_pin_local(env, 2, &x, &ret);

	if (!noct_get_arg_check_float(env, 0, &x, &x_f))
		return false;

	cosx = cosf(x_f);

	if (!noct_set_return_make_float(env, &ret, cosx))
		return false;

	return true;
}

/* tan() */
static bool
rt_intrin_tan(
	struct rt_env *env)
{
	struct rt_value x, ret;
	float x_f;
	float tanx;

	noct_pin_local(env, 2, &x, &ret);

	if (!noct_get_arg_check_float(env, 0, &x, &x_f))
		return false;

	tanx = tanf(x_f);

	if (!noct_set_return_make_float(env, &ret, tanx))
		return false;

	return true;
}

/* random() */
static bool
rt_intrin_random(
	struct rt_env *env)
{
	struct rt_value ret;
	float r;

	noct_pin_local(env, 1, &ret);

	r = (float)rand() / (float)RAND_MAX;

	if (!noct_set_return_make_float(env, &ret, r))
		return false;

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
