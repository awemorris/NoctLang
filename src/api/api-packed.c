/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: Packed.*
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>

#define NEVER_COME_HERE		(0)

/* Forward declaration. */
static bool cfunc_Packed_int8(struct rt_env *env);
static bool cfunc_Packed_uint8(struct rt_env *env);
static bool cfunc_Packed_int16(struct rt_env *env);
static bool cfunc_Packed_uint16(struct rt_env *env);
static bool cfunc_Packed_int32(struct rt_env *env);
static bool cfunc_Packed_uint32(struct rt_env *env);
static bool cfunc_Packed_int64(struct rt_env *env);
static bool cfunc_Packed_uint64(struct rt_env *env);
static bool cfunc_Packed_float32(struct rt_env *env);
static bool cfunc_Packed_float64(struct rt_env *env);

/* FFI table. */
struct ffi_item {
	const char *global_name;
	const char *field_name;
	uint32_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct ffi_item ffi_items[] = {
	{"Packed.int8",		"int8",		1,	{"len"},	cfunc_Packed_int8},
	{"Packed.uint8",	"uint8",	1,	{"len"},	cfunc_Packed_uint8},
	{"Packed.int16",	"int16",	1,	{"len"},	cfunc_Packed_int16},
	{"Packed.uint16",	"uint16",	1,	{"len"},	cfunc_Packed_uint16},
	{"Packed.int32",	"int32",	1,	{"len"},	cfunc_Packed_int32},
	{"Packed.uint32",	"uint32",	1,	{"len"},	cfunc_Packed_uint32},
	{"Packed.int64",	"int64",	1,	{"len"},	cfunc_Packed_int64},
	{"Packed.uint64",	"uint64",	1,	{"len"},	cfunc_Packed_uint64},
	{"Packed.float32",	"float32",	1,	{"len"},	cfunc_Packed_float32},
	{"Packed.float64",	"float64",	1,	{"len"},	cfunc_Packed_float64},
};

/*
 * Register "Packed.*" functions.
 */
NOCT_DLL
bool
noct_register_api_packed(
	NoctEnv *env)
{
	NoctValue dict;
	int i;

	/* Make a global variable "Packed". */
	if (!noct_make_empty_dict(env, &dict))
		return false;
	if (!noct_set_global(env, "Packed", &dict))
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
		if (!noct_set_dict_elem(
			    env,
			    &dict,
			    ffi_items[i].field_name,
			    &funcval))
			return false;
	}

	return true;
}

/* Packed.int8() */
static bool
cfunc_Packed_int8(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_INT8, i_size, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.uint8() */
static bool
cfunc_Packed_uint8(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_UINT8, i_size, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.int16() */
static bool
cfunc_Packed_int16(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_INT16, i_size * 2, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.uint16() */
static bool
cfunc_Packed_uint16(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_UINT16, i_size * 2, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.int32() */
static bool
cfunc_Packed_int32(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_INT32, i_size * 4, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.uint32() */
static bool
cfunc_Packed_uint32(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_UINT32, i_size * 4, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.int64() */
static bool
cfunc_Packed_int64(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_INT64, i_size * 8, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.uint64() */
static bool
cfunc_Packed_uint64(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_UINT64, i_size * 8, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.float32() */
static bool
cfunc_Packed_float32(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_FLOAT32, i_size * 4, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}

/* Packed.float64() */
static bool
cfunc_Packed_float64(
	NoctEnv *env)
{
	NoctValue v_size, v_ret;
	uint32_t i_size;

	noct_pin_local(env, 2, &v_size, &v_ret);

	if (!noct_get_arg_check_int(env, 0, &v_size, &i_size))
		return false;

	if (i_size == 0) {
		noct_error(env, "Packed size is 0.");
		return false;
	}

	if (!noct_make_packed(env, &v_ret, NOCT_PACKED_FLOAT32, i_size * 8, i_size, NULL))
		return false;
	if (!noct_set_return(env, &v_ret))
		return false;

	noct_unpin_local(env, 2, &v_size, &v_ret);

	return true;
}
