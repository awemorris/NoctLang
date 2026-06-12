/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: Console.*
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define NEVER_COME_HERE		(0)

/* Forward declaration. */
static bool cfunc_Math_abs(struct rt_env *env);
static bool cfunc_Math_sqrt(struct rt_env *env);
static bool cfunc_Math_sin(struct rt_env *env);
static bool cfunc_Math_cos(struct rt_env *env);
static bool cfunc_Math_tan(struct rt_env *env);
static bool cfunc_Math_random(struct rt_env *env);

/* FFI table. */
struct ffi_item {
	const char *global_name;
	const char *field_name;
	uint32_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct ffi_item ffi_items[] = {
	{"Math.abs",		"abs",		1,	{"x"},	cfunc_Math_abs},
	{"Math.sqrt",		"sqrt",		1,	{"x"},	cfunc_Math_sqrt},
	{"Math.sin",		"sin",		1,	{"x"},	cfunc_Math_sin},
	{"Math.cos",		"cos",		1,	{"x"},	cfunc_Math_cos},
	{"Math.tan",		"tan",		1,	{"x"},	cfunc_Math_tan},
	{"Math.random",		"random",	0,	{NULL},	cfunc_Math_random},
};

/*
 * Register "Math.*" functions.
 */
NOCT_DLL
bool
noct_register_api_math(
	NoctEnv *env)
{
	NoctValue dict;
	int i;

	srand((unsigned int)time(NULL));

	/* Make a global variable "Math". */
	if (!noct_make_empty_dict(env, &dict))
		return false;
	if (!noct_set_global(env, "Math", &dict))
		return false;

	/* Register functions. */
	for (i = 0; i < (int)(sizeof(ffi_items) / sizeof(struct ffi_item)); i++) {
		NoctValue funcval;

		/* Register a cfunc. */
		if (!noct_register_cfunc(
			    env,
			    ffi_items[i].global_name,
			    ffi_items[i].param_count,
			    ffi_items[i].param,
			    ffi_items[i].cfunc,
			    NULL))
			return false;

		/* Get a function value. */
		if (!noct_get_global(env, ffi_items[i].global_name, &funcval))
			return false;

		/* Make a dictionary element. */
		if (!noct_set_dict_elem_cstr(
			    env,
			    &dict,
			    ffi_items[i].field_name,
			    &funcval))
			return false;
	}

	return true;
}

