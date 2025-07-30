/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * NoctVM Public Interface
 */

#ifndef NOCT_NOCT_H
#define NOCT_NOCT_H

#include <stddef.h>

/*
 * Definition of the bool type.
 */
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

/*
 * Definitions of the intN_t and uintN_t types.
 */
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

/*
 * Definition of the inline keyword.
 */
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

/*
 * Definition of the import/export keyword.
 */
#if defined(USE_DLL)
#if defined(__GNUC__)
#define NOCT_DLL		__attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define NOCT_DLL		__declspec(dllexport)
#endif
#else
#if defined(__GNUC__)
#define NOCT_DLL
#elif defined(_MSC_VER)
#define NOCT_DLL		__declspec(dllimport)
#endif
#endif

/*
 * Maximum arguments of a call.
 */
#define NOCT_ARG_MAX	32

/*
 * Forward declaration of internal structs.
 */
struct rt_runtime;
struct rt_env;
struct rt_frame;
struct rt_gc_object;
struct rt_value;
struct rt_string;
struct rt_array;
struct rt_dict;
struct rt_func;

/*
 * Friendly-names.
 */
typedef struct rt_vm     NoctVM;
typedef struct rt_env    NoctEnv;
typedef struct rt_frame  NoctFrame;
typedef struct rt_value  NoctValue;
typedef struct rt_func   NoctFunc;
typedef struct rt_string NoctString;

/*
 * Value type.
 */
enum noct_value_type {
	NOCT_VALUE_INT    = 0,
	NOCT_VALUE_FLOAT  = 1,
	NOCT_VALUE_STRING = 2,
	NOCT_VALUE_ARRAY  = 3,
	NOCT_VALUE_DICT   = 4,
	NOCT_VALUE_FUNC   = 5,
};

/*
 * NoctValue implementation.
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
		struct rt_gc_object *obj;
	} val;
};

/*
 * Core Functions
 */

/*
 * Creates a new VM instance.
 *
 * Also returns a pointer to an environment for the calling thread.
 */
NOCT_DLL
bool
noct_create_vm(
	NoctVM **vm,
	NoctEnv *default_env);

/*
 * Destroys the given VM instance and releases associated resources.
 */
NOCT_DLL
bool
noct_destroy_vm(
	NoctVM *vm);

/*
 * Registers functions from a source code text.
 */
NOCT_DLL
bool
noct_register_source(
	NoctVM *vm,
	const char *file_name,
	const char *source_text);

/*
 * Registers functions from compiled bytecode data.
 */
NOCT_DLL
bool
noct_register_bytecode(
	NoctVM *vm,
	uint8_t *data,
	size_t size);

/*
 * Registers an FFI function.
 */
NOCT_DLL
bool
noct_register_cfunc(
	NoctVM *vm,
	const char *name,
	int param_count,
	const char *param_name[],
	bool (*cfunc)(NoctEnv *env),
	NoctFunc **ret_func);

/*
 * Creates a thread-local environment for the current thread.
 *
 * If the calling thread is the one that created the VM,
 * the default environment will be returned.
 */
NOCT_DLL
bool noct_create_thread_env(
	NoctVM *vm,
	NoctEnv **env);

/*
 * Enter the VM and invoke a function by name.
 *
 * Executes the specified function with the given arguments and stores
 * the results in the provided return value slot.
 */
NOCT_DLL
bool
noct_enter_vm(
	NoctEnv *env,
	const char *func_name,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret);

/*
 * Get the file name where the last error occurred.
 */
NOCT_DLL
bool
noct_get_error_file(
	NoctEnv *env,
	const char *msg,
	size_t len);

/*
 * Get the line number where the last error occurred.
 */
NOCT_DLL
bool
noct_get_error_line(
	NoctEnv *env,
	int *line);

/*
 * Get the error message where the last error occurred.
 */
NOCT_DLL
bool
noct_get_error_message(
	NoctEnv *env,
	const char *msg,
	size_t size);

/*
 * Call a function.
 */
NOCT_DLL
bool
noct_call(
	NoctEnv *env,
	NoctFunc *func,
	int arg_count,
	NoctValue *arg,
	NoctValue *ret);

/*
 * Assigns an value.
 */
NOCT_DLL
bool
noct_assign(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src);

/*
 * Create an integer value.
 */
NOCT_DLL
bool
noct_make_int(
	NoctEnv *env,
	NoctValue *val,
	int i);

/*
 * Make a float value.
 */
NOCT_DLL
bool
noct_make_float(
	NoctEnv *env,
	NoctValue *val,
	float f);

/*
 * Make a string value.
 */
NOCT_DLL
bool
noct_make_string(
	NoctEnv *env,
	NoctValue *val,
	const char *data,
	size_t len);

/*
 * Make a string value with a format.
 */
NOCT_DLL
bool
noct_make_string_format(
	NoctEnv *env,
	NoctValue *val,
	const char *s,
	...);

/*
 * Make an empty array value.
 */
NOCT_DLL
bool
noct_make_empty_array(
	NoctEnv *env,
	NoctValue *val);

/*
 * Make an empty dictionary value.
 */
NOCT_DLL
bool
noct_make_empty_dict(
	NoctEnv *env,
	NoctValue *val);

/*
 * Get the type tag of a value.
 *
 * The result is one of the NOCT_VALUE_* constants.
 */
NOCT_DLL
bool
noct_get_value_type(
	NoctEnv *env,
	NoctValue *val,
	int *type);

/*
 * Extract an integer from a value with type checking.
 *
 * Fails if the given value is not of integer type.
 */
NOCT_DLL
bool
noct_get_int(
	NoctEnv *env,
	NoctValue *val,
	int *i);

/*
 * Extracts a float from a value with type checking.
 *
 * Fails if the given value is not of float type.
 */
NOCT_DLL
bool
noct_get_float(
	NoctEnv *env,
	NoctValue *val,
	float *f);

/*
 * Retrieves the length of a string from a value with type checking.
 *
 * Fails if the given value is not of string type.
 */
NOCT_DLL
bool
noct_get_string_len(
	NoctEnv *env,
	NoctValue *val,
	size_t *len);

/*
 * Extracts a string from a value with type checking.
 *
 * Fails if the given value is not of string type.
 */
NOCT_DLL
bool
noct_get_string(
	NoctEnv *env,
	NoctValue *val,
	char *s,
	size_t len);

/*
 * Extracts a function from a value with type checking.
 *
 * Fails if the given value is not of function type.
 */
NOCT_DLL
bool
noct_get_func(
	NoctEnv *env,
	NoctValue *val,
	NoctFunc **ret);

/*
 * Arrays
 */

/*
 * Retrieves the number of elements in an array.
 *
 * Fails if the given value is not an array.
 */
NOCT_DLL
bool
noct_get_array_size(
	NoctEnv *env,
	NoctValue *val,
	int *size);

/*
 * Retrieves an element from an array without type checking.
 *
 * The element at the given index is returned as a NoctValue,
 * regardless of its actual type.
 */
NOCT_DLL
bool
noct_get_array_elem(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val);

/*
 * Sets an element in an array at the specified index.
 *
 * The value is passed as a NoctValue and may be of any type.
 * If the index is out of bounds, the array is automatically expanded.
 * The existing element at the index, if any, will be overwritten.
 */
NOCT_DLL
bool
noct_set_array_elem(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val);

/*
 * Resizes an array to the specified size.
 *
 * If the array is shrunk, elements beyond the new size are removed.
 * If the array is expanded, new elements are initialized to integer zero.
 */
NOCT_DLL
bool
noct_resize_array(
	NoctEnv *env,
	NoctValue *arr,
	int size);

/*
 * Creates a shallow copy of an array.
 */
NOCT_DLL
bool
noct_make_array_copy(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src);

/*
 * Retrieves the number of key-value pairs in a dictionary.
 */
NOCT_DLL
bool
noct_get_dict_size(
	NoctEnv *env,
	NoctValue *dict,
	int *size);

/*
 * Retrieves a dictionary key by index.
 *
 * This function can be used to traverse dictionary entries in order.
 */
NOCT_DLL
bool
noct_get_dict_key_by_index(
	NoctEnv *env,
	NoctValue *dict,
	int index,
	const char **key);

/*
 * Retrieves a dictionary value by index.
 *
 * This function can be used to traverse dictionary entries in order.
 */
NOCT_DLL
bool
noct_get_dict_value_by_index(
	NoctEnv *env,
	NoctValue *dict,
	int index,
	NoctValue *val);

/*
 * Checks whether a key exists in a dictionary.
 */
NOCT_DLL
bool
noct_check_dict_key(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	bool *ret);

/*
 * Retrieves a value associated with a key in a dictionary without type checking.
 *
 * The returned value is wrapped by NoctValue and may be of any type.
 * Fails if the key does not exist.
 */
NOCT_DLL
bool
noct_get_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/*
 * Sets a value for a key in a dictionary.
 */
NOCT_DLL
bool
noct_set_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/*
 * Removes a key-value pair from a dictionary by key.
 */
NOCT_DLL
bool
noct_remove_dict_elem(
	NoctEnv *env,
	NoctValue *dict,
	const char *key);

/*
 * Creates a shallow copy of a dictionary.
 */
NOCT_DLL
bool
noct_make_dict_copy(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src);

/*
 * Retrieves a function argument from the current stack frame.
 */
NOCT_DLL
bool
noct_get_arg(
	NoctEnv *env,
	int index,
	NoctValue *val);

/*
 * Sets the return value for the current stack frame.
 */
NOCT_DLL
bool
noct_set_return(
	NoctEnv *env,
	NoctValue *val);

/*
 * Retrieves the value of a global variable by name.
 *
 * Fails if the global variable with the given name does not exist.
 */
NOCT_DLL
bool
noct_get_global(
	NoctEnv *env,
	const char *name,
	NoctValue *val);

/*
 * Sets a global variable by name.
 */
NOCT_DLL
bool
noct_set_global(
	NoctEnv *env,
	const char *name,
	NoctValue *val);

/*
 * Declare native global variables for use within an FFI function.
 *
 * This informs the GC that the given NoctValue variables are
 * in use and should not be collected during GC.
 *
 * This function should be called with the number of variables,
 * followed by that many NoctValue* arguments.
 */
NOCT_DLL
void
noct_declare_c_global(
	NoctEnv *env,
	int count,
	...);

/*
 * Declare native local variables for use within an FFI function.
 *
 * This informs the GC that the given stack-local NoctValue variables
 * are in use and should not be collected during GC.
 *
 * This function should be called with the number of variables,
 * followed by that many NoctValue* arguments.
 */
NOCT_DLL
void
noct_declare_c_local(
	NoctEnv *env,
	int count,
	...);

/*
 * Triggers a fast garbage collection pass during a periodic pause,
 * such as VSync.
 *
 * If the VM is configured with generational GC, this function performs
 * a collection for the young generation only.
 * If the VM implements incremental GC, this function performs a single
 * incremental GC step.
 */
NOCT_DLL
bool
noct_fast_gc(
	NoctEnv *env);

/*
 * Triggers a full, stop-the-world garbage collection.
 */
NOCT_DLL
bool
noct_full_gc(
	NoctEnv *env);

/*
 * Retrieves the current heap usage, in bytes.
 */
NOCT_DLL
bool
noct_get_heap_usage(
	NoctEnv *env,
	size_t *ret);

/*
 * Writes an error message to the internal buffer.
 *
 * The message can be retrieved later using noct_get_error_message().
 */
NOCT_DLL
void
noct_error(
	NoctEnv *env,
	const char *msg,
	...);

/*
 * Convenience Functions
 */

/* Array Getters */

/*
 * Convenience function to retrieve an integer element from an array
 * with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of integer type.
 */
NOCT_DLL
bool
noct_get_array_elem_int(
	NoctEnv *env,
	NoctValue *array,
	int index,
	int *i,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve a float element from an array with
 * type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of float type.
 */
NOCT_DLL
bool
noct_get_array_elem_float(
	NoctEnv *env,
	NoctValue *array,
	int index,
	float *f,
	NoctValue *backing_val);


/*
 * Convenience function to retrieve a string element from an array
 * with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of string type.
 */
NOCT_DLL
bool
noct_get_array_elem_string(
	NoctEnv *env,
	NoctValue *array,
	int index,
	const char **val,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve an array element from an array
 * with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of array type.
 */
NOCT_DLL
bool
noct_get_array_elem_array(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val);

/*
 * Convenience function to retrieve an array element from a dictionary
 * with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of dictionary type.
 */
NOCT_DLL
bool
noct_get_array_elem_dict(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctValue *val);

/*
 * Convenience function to retrieve a function element from a
 * dictionary with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the element at the given index is not of function type.
 */
NOCT_DLL
bool
noct_get_array_elem_func(
	NoctEnv *env,
	NoctValue *array,
	int index,
	NoctFunc **f,
	NoctValue *backing_val);

/*
 * Convenience function to set an integer element in an array.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Semantically equivalent to wrapping the integer in a NoctValue
 * and calling noct_set_array_elem().
 */
NOCT_DLL
bool
noct_set_array_elem_int(
	NoctEnv *env,
	NoctValue *array,
	int index,
	int val,
	NoctValue *backing_val);

/*
 * Convenience function to set a float element in an array.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Semantically equivalent to wrapping the float in a NoctValue
 * and calling noct_set_array_elem().
 */
NOCT_DLL
bool
noct_set_array_elem_float(
	NoctEnv *env,
	NoctValue *array,
	int index,
	float f,
	NoctValue *backing_val);

/*
 * Convenience function to set a string element in an array.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Semantically equivalent to creating a string NoctValue with
 * noct_make_string() and passing it to noct_set_array_elem().
 */
NOCT_DLL
bool
noct_set_array_elem_string(
	NoctEnv *env,
	NoctValue *array,
	int index,
	const char *s,
	NoctValue *backing_val);

/* Dictionaries */

/*
 * Convenience function to retrieve an integer value associated with a
 * key in a dictionary with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of integer type.
 */
NOCT_DLL
bool
noct_get_dict_elem_int(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	int *i,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve a float value associated with a
 * key in a dictionary with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of float type.
 */
NOCT_DLL
bool
noct_get_dict_elem_float(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	float *f,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve a string value associated with a
 * key in a dictionary with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of string type.
 */
NOCT_DLL
bool
noct_get_dict_elem_string(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	const char **s,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve an array value associated with a
 * key in a dictionary with type checking.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of array type.
 */
NOCT_DLL
bool
noct_get_dict_elem_array(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/*
 * Convenience function to retrieve a dictionary value associated with a
 * key in a dictionary with type checking.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of dictionary type.
 */
NOCT_DLL
bool
noct_get_dict_elem_dict(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctValue *val);

/*
 * Convenience function to retrieve a function value associated with a
 * key in a dictionary with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 *
 * Fails if the key does not exist.
 * Fails if the associated value is not of function type.
 */
NOCT_DLL
bool
noct_get_dict_elem_func(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	NoctFunc **f,
	NoctValue *backing_val);

/*
 * Convenience function to set an integer value for a key in a dictionary.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_dict_elem_int(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	int i,
	NoctValue *backing_val);

/*
 * Convenience function to set a float value for a key in a dictionary.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_dict_elem_float(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	float f,
	NoctValue *backing_val);

/*
 * Convenience function to set a string value for a key in a dictionary.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_dict_elem_string(
	NoctEnv *env,
	NoctValue *dict,
	const char *key,
	const char *val,
	NoctValue *backing_val);

/* Arguments */

/*
 * Convenience function to retrieve an integer function argument with
 * type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_get_arg_int(
	NoctEnv *env,
	int index,
	int *i,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve a float function argument with
 * type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_get_arg_float(
	NoctEnv *env,
	int index,
	float *f,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve a string function argument with
 * type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_get_arg_string(
	NoctEnv *env,
	int index,
	const char **s,
	NoctValue *backing_val);

/*
 * Convenience function to retrieve an array function argument with
 * type checking.
 */
NOCT_DLL
bool
noct_get_arg_array(
	NoctEnv *env,
	int index,
	NoctValue *val);

/*
 * Convenience function to retrieve a dictionary function argument with
 * type checking.
 */
NOCT_DLL
bool
noct_get_arg_dict(
	NoctEnv *env,
	int index,
	NoctValue *val);

/*
 * Convenience function to retrieve a function argument of function
 * type with type checking.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_get_arg_func(
	NoctEnv *env,
	int index,
	NoctFunc **f,
	NoctValue *backing_val);

/* Return Values */

/*
 * Convenience function to set an integer return value for the current
 * stack frame.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_return_int(
	NoctEnv *env,
	int i,
	NoctValue *backing_val);

/*
 * Convenience function to set a float return value for the current
 * stack frame.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_return_float(
	NoctEnv *env,
	float f,
	NoctValue *backing_val);

/*
 * Convenience function to set a string return value for the current
 * stack frame.
 *
 * The `backing_val` parameter must point to a variable previously
 * declared via `noct_declare_c_local()`. This variable serves as a
 * pinned storage location: during the call, it temporarily holds the
 * element's value so that, even if a parallel GC is triggered midway,
 * the value remains protected and stable.
 */
NOCT_DLL
bool
noct_set_return_string(
	NoctEnv *env,
	const char *s,
	NoctValue *backing_val);

#endif
