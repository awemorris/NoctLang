/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * NoctLang Public Interface
 */

#ifndef NOCT_NOCT_H
#define NOCT_NOCT_H

#include <stddef.h>

/* bool */
#ifndef __cplusplus
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#else
#ifndef bool
typedef int bool;
enum { false = 0, true = 1 };
#endif
#endif
#endif

/* intN_t, uintN_t */
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#else
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

/* inline */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define NOCT_INLINE		inline		/* C99 */
#elif defined(__GNUC__)
#define NOCT_INLINE		__inline__	/* GCC */
#elif defined(__xlc__)
#define NOCT_INLINE		__inline	/* IBM (PPC64) */
#elif defined(_MSC_VER)
#define NOCT_INLINE		__inline	/* MSVC (x86, x64, arm64) */
#elif defined(INLINE)
#define NOCT_INLINE		INLINE
#else
#define NOCT_INLINE
#endif

/* SYSV ABI */
#if defined(__GNUC__) && defined(_WIN32) && defined(__x86_64__)
#define SYSVABI __attribute__((sysv_abi))
#else
#define SYSVABI
#endif

/* Maximum arguments of a call. */
#define NOCT_ARG_MAX	32

/* Forward declaration */
struct rt_env;
struct rt_frame;
struct rt_value;
struct rt_string;
struct rt_array;
struct rt_dict;
struct rt_func;

/* Friendly-names. */
typedef struct rt_env NoctEnv;
typedef struct rt_frame NoctFrame;
typedef struct rt_value NoctValue;
typedef struct rt_func NoctFunc;
typedef struct rt_string NoctString;

/* Value type. */
enum noct_value_type {
	NOCT_VALUE_INT,
	NOCT_VALUE_FLOAT,
	NOCT_VALUE_STRING,
	NOCT_VALUE_ARRAY,
	NOCT_VALUE_DICT,
	NOCT_VALUE_FUNC,
};

/*
 * Variable value.
 *  - If a value is zero-cleared, it shows an integer zero.
 *  - This struct has 8 bytes on 32-bit architecture and 16 bytes on 64-bit architecture.
 */
struct rt_value {
	/* Offset 0: */
	int type;

	/* 32-bit padding for 64-bit architecture excluding MIPS64. */
#if defined(ARCH_ARM64) || defined(ARCH_X86_64) || defined(ARCH_PPC64)
	int padding;
#endif

	/* Offset 4 in 32-bit, 8 in 64-bit: */
	union {
		int i;
		float f;
		struct rt_string *str;
		struct rt_array *arr;
		struct rt_dict *dict;
		struct rt_func *func;
	} val;
};

/* Create a runtime environment. */
bool
noct_create(
	NoctEnv **rt);

/* Destroy a runtime environment. */
bool
noct_destroy(
	NoctEnv *rt);

/* Get a file name. */
const char *
noct_get_error_file(
	NoctEnv *rt);

/* Get an error line number. */
int
noct_get_error_line(
	NoctEnv *rt);

/* Get an error message. */
const char *
noct_get_error_message(
	NoctEnv *rt);

/* Output an error message. */
void
noct_error(
	NoctEnv *rt,
	const char *msg,
	...);

/* Register functions from a souce text. */
bool
noct_register_source(
	NoctEnv *rt,
	const char *file_name,
	const char *source_text);

/* Register functions from bytecode data. */
bool
noct_register_bytecode(
	NoctEnv *rt,
	uint32_t size,
	uint8_t *data);

/* Register a C function.. */
bool
noct_register_cfunc(
	NoctEnv *rt,
	const char *name,
	int param_count,
	const char *param_name[],
	bool (*cfunc)(NoctEnv *env),
	NoctFunc **ret_func);

/* Call a function. */
bool
noct_call(
	NoctEnv *rt,
	NoctFunc *func,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret);

/* Call a function with a name. */
bool
noct_call_with_name(
	NoctEnv *rt,
	const char *func_name,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret);

/* Make an integer value. */
void
noct_make_int(
	NoctValue *val,
	int i);

static NOCT_INLINE struct rt_value noct_int(int i)
{
	struct rt_value v;
	v.type = NOCT_VALUE_INT;
	v.val.i = i;
	return v;
}

/* Make a floating-point value. */
void
noct_make_float(
	NoctValue *val,
	float f);

static NOCT_INLINE struct rt_value noct_float(float f)
{
	struct rt_value v;
	v.type = NOCT_VALUE_FLOAT;
	v.val.f = f;
	return v;
}

/* Make a string value. */
SYSVABI
bool
noct_make_string(
	NoctEnv *rt,
	NoctValue *val,
	const char *s);

/* Make a string value with a format. */
bool
noct_make_string_format(
	NoctEnv *rt,
	NoctValue *val,
	const char *s,
	...);

/* Make an empty array value. */
SYSVABI
bool
noct_make_empty_array(
	NoctEnv *rt,
	NoctValue *val);

/* Make an empty dictionary value */
SYSVABI
bool
noct_make_empty_dict(
	NoctEnv *rt,
	NoctValue *val);

/* Get a value type. */
bool
noct_get_value_type(
	NoctEnv *rt,
	NoctValue *val,
	int *type);

/* Get an integer value. */
bool
noct_get_int(
	NoctEnv *rt,
	NoctValue *val,
	int *ret);

/* Get a floating-point value. */
bool
noct_get_float(
	NoctEnv *rt,
	NoctValue *val,
	float *ret);

/* Get a string value. */
bool
noct_get_string(
	NoctEnv *rt,
	NoctValue *val,
	const char **ret);

/* Get a function value. */
bool
noct_get_func(
	NoctEnv *rt,
	NoctValue *val,
	NoctFunc **ret);

/* Get an array size. */
bool
noct_get_array_size(
	NoctEnv *rt,
	NoctValue *val,
	int *size);

/* Get an array element. */
bool
noct_get_array_elem(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	NoctValue *val);

/* Get an integer-type array element. */
bool
noct_get_array_elem_int(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	int *val);

/* Get a float-type array element. */
bool
noct_get_array_elem_float(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	float *val);

/* Get a string-type array element. */
bool
noct_get_array_elem_string(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	const char **val);

/* Get an array-type array element. */
bool
noct_get_array_elem_array(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	NoctValue *val);

/* Get an dictionary-type array element. */
bool
noct_get_array_elem_dict(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	NoctValue *val);

/* Get a function-type array element. */
bool
noct_get_array_elem_func(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	NoctFunc **val);

/* Set an array element. */
bool
noct_set_array_elem(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	NoctValue *val);

/* Set an integer-type array element. */
bool
noct_set_array_elem_int(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	int val);

/* Set a float-type array element. */
bool
noct_set_array_elem_float(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	float val);

/* Set a string-type array element. */
bool
noct_set_array_elem_string(
	NoctEnv *rt,
	NoctValue *array,
	int index,
	const char *val);

/* Resize an array. */
bool
noct_resize_array(
	NoctEnv *rt,
	NoctValue *arr,
	int size);

/* Get a dictionary size. */
bool
noct_get_dict_size(
	NoctEnv *rt,
	NoctValue *dict,
	int *size);

/* Get a dictionary value by an index. (For traverse) */
bool
noct_get_dict_value_by_index(
	NoctEnv *rt,
	NoctValue *dict,
	int index,
	NoctValue *val);

/* Get a dictionary key by an index. (For traverse) */
bool
noct_get_dict_key_by_index(
	NoctEnv *rt,
	NoctValue *dict,
	int index,
	const char **key);

/* Get a dictionary element. */
bool
noct_get_dict_elem(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/* Get an integer-type dictionary element. */
bool
noct_get_dict_elem_int(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	int *val);

/* Get a float-type dictionary element. */
bool
noct_get_dict_elem_float(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	float *val);

/* Get a string-type dictionary element. */
bool
noct_get_dict_elem_string(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	const char **val);

/* Get an array-type dictionary element. */
bool
noct_get_dict_elem_array(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/* Get a dictionary-type dictionary element. */
bool
noct_get_dict_elem_dict(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/* Get a dictionary-type dictionary element. */
bool
noct_get_dict_elem_func(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	NoctFunc **val);

/* Set a dictionary element. */
bool
noct_set_dict_elem(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/* Set an integer dictionary element. */
bool
noct_set_dict_elem_int(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	int val);

/* Set a float dictionary element. */
bool
noct_set_dict_elem_float(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	float val);

/* Set a string dictionary element. */
bool
noct_set_dict_elem_string(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key,
	const char *val);

/* Remove a dictionary element. */
bool
noct_remove_dict_elem(
	NoctEnv *rt,
	NoctValue *dict,
	const char *key);

/* Make a copy of a dictionary. */
bool
noct_make_dict_copy(
	NoctEnv *rt,
	NoctValue *src,
	NoctValue *dst);

/* Get a call argument. */
bool
noct_get_arg(
	NoctEnv *rt,
	int index,
	NoctValue *val);

/* Get an integer-type call argument. */
bool
noct_get_arg_int(
	NoctEnv *rt,
	int index,
	int *val);

/* Get a float-type call argument. */
bool
noct_get_arg_float(
	NoctEnv *rt,
	int index,
	float *val);

/* Get a string-type call argument. */
bool
noct_get_arg_string(
	NoctEnv *rt,
	int index,
	const char **val);

/* Get an array-type call argument. */
bool
noct_get_arg_array(
	NoctEnv *rt,
	int index,
	NoctValue *val);

/* Get a dictionary-type call argument. */
bool
noct_get_arg_dict(
	NoctEnv *rt,
	int index,
	NoctValue *val);

/* Get a function-type call argument. */
bool
noct_get_arg_func(
	NoctEnv *rt,
	int index,
	NoctFunc **val);

/* Set a return value. */
bool
noct_set_return(
	NoctEnv *rt,
	NoctValue *val);

/* Set an integer-type return value. */
bool
noct_set_return_int(
	NoctEnv *rt,
	int val);

/* Set a float-type return value. */
bool
noct_set_return_float(
	NoctEnv *rt,
	int val);

/* Set a string-type return value. */
bool
noct_set_return_string(
	NoctEnv *rt,
	const char *val);

/* Get a global variable. */
bool
noct_get_global(
	NoctEnv *rt,
	const char *name,
	NoctValue *val);

/* Make a variable global. */
bool
noct_set_global(
	NoctEnv *rt,
	const char *name,
	NoctValue *val);

/* Set a native reference status of an object. */
bool
noct_set_native_reference(
	NoctEnv *rt,
	NoctValue *val,
	bool is_enabled);

/* Do a shallow GC for nursery space. */
bool
noct_shallow_gc(
	NoctEnv *rt);

/* Do a deep GC for tenured space. */
bool
noct_deep_gc(
	NoctEnv *rt);

/* Get an approximate memory usage in bytes. */
bool
rt_get_heap_usage(
	NoctEnv *rt,
	size_t *ret);

#endif
