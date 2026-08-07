/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: Thread.*
 *
 * Threading model:
 *  - Thread.createThread() spawns an OS thread with its own NoctEnv.
 *  - Handles (thread, shared, locked, counter) are dictionaries that
 *    carry a native pointer to a small control block.
 *  - Values that must survive across threads are stored in the handle
 *    dictionary itself ("result", "value" keys), so they are rooted by
 *    whoever holds the handle and are relocated properly by the GC.
 *  - Every potentially blocking operation (mutex wait, join, sleep) is
 *    wrapped in noct_enter_blocking()/noct_leave_blocking() so that a
 *    blocked thread never stalls a stop-the-world GC.
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "atomic.h"

#if defined(NOCT_TARGET_WINDOWS)
#include <windows.h>
#elif defined(NOCT_TARGET_POSIX)
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#else
#error "No thread support for this platform."
#endif

/*
 * Thin thread/mutex abstraction.
 */
#if defined(NOCT_TARGET_WINDOWS)
typedef HANDLE thr_thread;
typedef CRITICAL_SECTION thr_mutex;
#else
typedef pthread_t thr_thread;
typedef pthread_mutex_t thr_mutex;
#endif

/* Magic numbers for the handle control blocks. */
#define THREAD_MAGIC	0x54687264	/* 'Thrd' */
#define SYNC_MAGIC	0x53796e63	/* 'Sync' */
#define COUNTER_MAGIC	0x436e7472	/* 'Cntr' */

/* Maximum nesting depth of a deep copy. */
#define DEEP_COPY_MAX_DEPTH	8

/* Thread handle control block. */
struct thread_obj {
	int magic;
	int joined;
	thr_thread handle;
};

/* Shared/locked handle control block. */
struct sync_obj {
	int magic;
	thr_mutex mutex;
};

/* Counter handle control block. */
struct counter_obj {
	int magic;
	int value;
};

/*
 * Thread startup block.
 *
 * Created and filled by the parent thread, owned and freed by the
 * child. The three values are pinned in the child's environment by the
 * parent, so they are proper GC roots from the moment the child env
 * exists until the child unpins them.
 */
struct thread_start {
	NoctEnv *env;
	NoctValue func_v;
	NoctValue param_v;
	NoctValue handle_v;
};

/*
 * Forward declaration.
 */

/*
 * [Thread Object]
 *
 * var th = Thread.createThread(func, param);
 * var ret = Thread.joinThread(th);
 */
static bool cfunc_Thread_createThread(NoctEnv *env);
static bool cfunc_Thread_joinThread(NoctEnv *env);

/*
 * [Shared Object]
 *
 * var shared = Thread.createShared({msg: ""});
 *
 * func threadA_producer() {
 *     while (true) {
 *         Thread.updateShared(shared, {msg: produce()});
 *     }
 * }
 *
 * func threadB_consumer() {
 *     while (true) {
 *         var snapshot = Thread.snapshotShared(shared);
 *         consume(snapshot.msg);
 *     }
 * }
 */
static bool cfunc_Thread_createShared(NoctEnv *env);
static bool cfunc_Thread_updateShared(NoctEnv *env);
static bool cfunc_Thread_snapshotShared(NoctEnv *env);

/*
 * [Atomic Counter]
 *
 * var counter = Thread.createCounter();
 * Thread.incrementCounter(counter);
 * print(Thread.getCounter(counter));
 */
static bool cfunc_Thread_createCounter(NoctEnv *env);
static bool cfunc_Thread_incrementCounter(NoctEnv *env);
static bool cfunc_Thread_getCounter(NoctEnv *env);

/*
 * [Locked Dictionary]
 *
 * var storage = Thread.createLocked({data: ""});
 * Thread.withLock(storage, (o) => { o.data = makeData(); });
 */
static bool cfunc_Thread_createLocked(NoctEnv *env);
static bool cfunc_Thread_withLock(NoctEnv *env);

/*
 * [Sleep]
 *
 * Thread.sleep(ms);
 */
static bool cfunc_Thread_sleep(NoctEnv *env);

static void thread_finalizer(void *native_pointer);
static void sync_finalizer(void *native_pointer);
static void counter_finalizer(void *native_pointer);

static bool deep_copy_value(NoctEnv *env, NoctValue *dst, NoctValue *src, int depth);

/* FFI table. */
struct ffi_item {
	const char *global_name;
	const char *package_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct ffi_item ffi_items[] = {
	{"Thread.createThread",		"Thread",		"createThread",			2,	{"func", "param"},	cfunc_Thread_createThread},
	{"Thread.joinThread",		"Thread",		"joinThread",			1,	{"th"},			cfunc_Thread_joinThread},
	{"Thread.createShared",		"Thread",		"createShared",			1,	{"value"},		cfunc_Thread_createShared},
	{"Thread.updateShared",		"Thread",		"updateShared",			2,	{"shared", "value"},	cfunc_Thread_updateShared},
	{"Thread.snapshotShared",	"Thread",		"snapshotShared",		1,	{"shared"},		cfunc_Thread_snapshotShared},
	{"Thread.createCounter",	"Thread",		"createCounter",		0,	{NULL},			cfunc_Thread_createCounter},
	{"Thread.incrementCounter",	"Thread",		"incrementCounter",		1,	{"counter"},		cfunc_Thread_incrementCounter},
	{"Thread.getCounter",		"Thread",		"getCounter",			1,	{"counter"},		cfunc_Thread_getCounter},
	{"Thread.createLocked",		"Thread",		"createLocked",			1,	{"dict"},		cfunc_Thread_createLocked},
	{"Thread.withLock",		"Thread",		"withLock",			2,	{"locked", "func"},	cfunc_Thread_withLock},
	{"Thread.sleep",		"Thread",		"sleep",			1,	{"ms"},			cfunc_Thread_sleep},
};

/*
 * Register "Thread.*" functions.
 */
NOCT_DLL
bool
noct_register_api_thread(
	NoctEnv *env)
{
	NoctValue thread_dict;
	size_t i;

	/* Make global variables "Thread". */
	if (!noct_make_empty_dict(env, &thread_dict))
		return false;
	if (!noct_set_global(env, "Thread", &thread_dict))
		return false;

	/* Register functions. */
	for (i = 0; i < sizeof(ffi_items) / sizeof(struct ffi_item); i++) {
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
		if (!noct_set_dict_elem_cstr(env, &thread_dict, ffi_items[i].field_name, &funcval))
			return false;
	}

	return true;
}

/*
 * Platform helpers
 */

static bool
thr_mutex_init(
	thr_mutex *m)
{
#if defined(NOCT_TARGET_WINDOWS)
	InitializeCriticalSection(m);
	return true;
#else
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(m, &attr) != 0) {
		pthread_mutexattr_destroy(&attr);
		return false;
	}
	pthread_mutexattr_destroy(&attr);
	return true;
#endif
}

static void
thr_mutex_destroy(
	thr_mutex *m)
{
#if defined(NOCT_TARGET_WINDOWS)
	DeleteCriticalSection(m);
#else
	pthread_mutex_destroy(m);
#endif
}

/*
 * Lock a mutex.
 *
 * The wait is wrapped in a blocking region so that a contended lock
 * never stalls a stop-the-world GC.
 */
static void
thr_mutex_lock_blocking(
	NoctEnv *env,
	thr_mutex *m)
{
	noct_enter_blocking(env);
#if defined(NOCT_TARGET_WINDOWS)
	EnterCriticalSection(m);
#else
	pthread_mutex_lock(m);
#endif
	noct_leave_blocking(env);
}

static void
thr_mutex_unlock(
	thr_mutex *m)
{
#if defined(NOCT_TARGET_WINDOWS)
	LeaveCriticalSection(m);
#else
	pthread_mutex_unlock(m);
#endif
}

/*
 * Handle helpers
 */

/* Make a handle dictionary with a native control block. */
static bool
make_handle_dict(
	NoctEnv *env,
	NoctValue *handle,
	void *native,
	void (*finalizer)(void *))
{
	if (!noct_make_empty_dict(env, handle))
		return false;
	if (!noct_set_dict_native_pointer(env, handle, native, finalizer))
		return false;
	return true;
}

/* Get a control block from a handle dictionary with a magic check. */
static bool
get_handle_native(
	NoctEnv *env,
	NoctValue *handle,
	int magic,
	void **native)
{
	void (*finalizer)(void *);

	if (!noct_get_dict_native_pointer(env, handle, native, &finalizer))
		return false;
	if (*native == NULL || *(int *)*native != magic) {
		noct_error(env, N_TR("Invalid handle."));
		return false;
	}
	return true;
}

/*
 * Deep copy
 */

/* Get a packed element size by type. */
static size_t
packed_elem_size(
	int type)
{
	switch (type) {
	case NOCT_PACKED_INT8:
	case NOCT_PACKED_UINT8:
		return 1;
	case NOCT_PACKED_INT16:
	case NOCT_PACKED_UINT16:
		return 2;
	case NOCT_PACKED_INT32:
	case NOCT_PACKED_UINT32:
	case NOCT_PACKED_FLOAT32:
		return 4;
	default:
		return 8;
	}
}

/*
 * Deep-copy a value.
 *
 * Strings, functions and numbers are copied by value/reference (they
 * are immutable). Arrays, dictionaries and packed arrays are copied
 * structurally. "dst" must be pinned by the caller.
 */
static bool
deep_copy_value(
	NoctEnv *env,
	NoctValue *dst,
	NoctValue *src,
	int depth)
{
	int type;

	if (depth > DEEP_COPY_MAX_DEPTH) {
		noct_error(env, N_TR("Shared value is nested too deeply."));
		return false;
	}

	if (!noct_get_value_type(env, src, &type))
		return false;

	switch (type) {
	case NOCT_VALUE_INT:
	case NOCT_VALUE_FLOAT:
	case NOCT_VALUE_LONG:
	case NOCT_VALUE_DOUBLE:
	case NOCT_VALUE_STRING:
	case NOCT_VALUE_FUNC:
		/* Immutable. Copy the value struct. */
		*dst = *src;
		return true;
	case NOCT_VALUE_ARRAY:
	{
		NoctValue elem, copy;
		size_t size, i;

		if (!noct_pin_local(env, 2, &elem, &copy))
			return false;
		if (!noct_get_array_size(env, src, &size))
			return false;
		if (!noct_make_empty_array(env, dst))
			return false;
		if (size > 0) {
			if (!noct_resize_array(env, dst, size))
				return false;
		}
		for (i = 0; i < size; i++) {
			if (!noct_get_array_elem(env, src, i, &elem))
				return false;
			if (!deep_copy_value(env, &copy, &elem, depth + 1))
				return false;
			if (!noct_set_array_elem(env, dst, i, &copy))
				return false;
		}
		noct_unpin_local(env, 2, &elem, &copy);
		return true;
	}
	case NOCT_VALUE_DICT:
	{
		NoctValue key, elem, copy;
		size_t size, i;

		if (!noct_pin_local(env, 3, &key, &elem, &copy))
			return false;
		if (!noct_get_dict_size(env, src, &size))
			return false;
		if (!noct_make_empty_dict(env, dst))
			return false;
		for (i = 0; i < size; i++) {
			if (!noct_get_dict_by_index(env, src, i, &key, &elem))
				return false;
			if (!deep_copy_value(env, &copy, &elem, depth + 1))
				return false;
			if (!noct_set_dict_elem(env, dst, &key, &copy))
				return false;
		}
		noct_unpin_local(env, 3, &key, &elem, &copy);
		return true;
	}
	case NOCT_VALUE_PACKED:
	{
		int ptype;
		size_t size, esize;
		void *src_buf, *buf;

		if (!noct_get_packed_type(env, src, &ptype))
			return false;
		if (!noct_get_packed_size(env, src, &size))
			return false;
		if (!noct_get_packed_pointer(env, src, &src_buf))
			return false;
		esize = packed_elem_size(ptype);
		buf = noct_malloc(size * esize);
		if (buf == NULL) {
			noct_out_of_memory(env);
			return false;
		}
		memcpy(buf, src_buf, size * esize);

		/*
		 * noct_make_packed() takes the byte size first and the
		 * element count second.
		 */
		if (!noct_make_packed(env, dst, ptype, size * esize, size, buf)) {
			noct_free(buf);
			return false;
		}
		return true;
	}
	default:
		noct_error(env, N_TR("Cannot copy this value type."));
		return false;
	}
}

/*
 * Thread.createThread()
 */

/* Thread entry routine. */
#if defined(NOCT_TARGET_WINDOWS)
static DWORD WINAPI
thread_entry(
	LPVOID arg)
#else
static void *
thread_entry(
	void *arg)
#endif
{
	struct thread_start *start;
	NoctEnv *env;
	NoctValue func_v, param_v, handle_v, ret_v, tmp;
	NoctFunc *f;

	start = (struct thread_start *)arg;
	env = start->env;

	/* Adopt the environment the parent created for us. */
	noct_attach_thread_env(env);

	memset(&func_v, 0, sizeof(NoctValue));
	memset(&param_v, 0, sizeof(NoctValue));
	memset(&handle_v, 0, sizeof(NoctValue));
	memset(&ret_v, 0, sizeof(NoctValue));
	memset(&tmp, 0, sizeof(NoctValue));
	noct_pin_local(env, 5, &func_v, &param_v, &handle_v, &ret_v, &tmp);

	/*
	 * Take over the arguments. Both the source and the destination
	 * slots are pinned, so this is safe against a concurrent GC.
	 */
	func_v = start->func_v;
	param_v = start->param_v;
	handle_v = start->handle_v;

	/* Release the startup block. */
	noct_unpin_local(env, 3, &start->func_v, &start->param_v, &start->handle_v);
	noct_free(start);
	start = NULL;

	/* Call the thread function. */
	if (!noct_get_func(env, &func_v, &f)) {
		noct_detach_thread_env(env);
#if defined(NOCT_TARGET_WINDOWS)
		return 0;
#else
		return NULL;
#endif
	}
	if (noct_call(env, f, 1, &param_v, &ret_v)) {
		/* Store the result into the handle dictionary. */
		noct_set_dict_elem_cstr(env, &handle_v, "result", &ret_v);
	} else {
		/* Store the error message into the handle dictionary. */
		const char *msg = NULL;
		noct_get_error_message(env, &msg);
		noct_set_dict_elem_make_string(env, &handle_v, "error", &tmp,
					       msg != NULL ? msg : "unknown error");
	}

	/* Park and recycle this env. */
	noct_detach_thread_env(env);

#if defined(NOCT_TARGET_WINDOWS)
	return 0;
#else
	return NULL;
#endif
}

/* Implementation of Thread.createThread() */
static bool
cfunc_Thread_createThread(
	NoctEnv *env)
{
	NoctValue func, param, handle;
	NoctFunc *f;
	struct thread_obj *obj;
	struct thread_start *start;
	bool created;

	memset(&func, 0, sizeof(NoctValue));
	memset(&param, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &func, &param, &handle);

	/* Get parameters. */
	if (!noct_get_arg_check_func(env, 0, &func, &f))
		return false;
	if (!noct_get_arg(env, 1, &param))
		return false;

	/* Make a control block. */
	obj = noct_malloc(sizeof(struct thread_obj));
	if (obj == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(obj, 0, sizeof(struct thread_obj));
	obj->magic = THREAD_MAGIC;

	/* Make a handle dictionary. */
	if (!make_handle_dict(env, &handle, obj, thread_finalizer)) {
		noct_free(obj);
		return false;
	}

	/* Make a startup block. */
	start = noct_malloc(sizeof(struct thread_start));
	if (start == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(start, 0, sizeof(struct thread_start));

	/*
	 * Create the child's environment here, while this thread is
	 * in-flight: no stop-the-world section can be running, so
	 * linking the new env into the VM's env list is race-free.
	 */
	if (!noct_create_thread_env(env, &start->env)) {
		noct_free(start);
		noct_error(env, N_TR("Cannot create a thread env."));
		return false;
	}

	/*
	 * Hand over the arguments. Pinning them in the child's
	 * environment makes them GC roots before the child runs.
	 */
	start->func_v = func;
	start->param_v = param;
	start->handle_v = handle;
	noct_pin_local(start->env, 3, &start->func_v, &start->param_v, &start->handle_v);

	/* Start the thread. */
#if defined(NOCT_TARGET_WINDOWS)
	obj->handle = CreateThread(NULL, 0, thread_entry, start, 0, NULL);
	created = obj->handle != NULL;
#else
	created = pthread_create(&obj->handle, NULL, thread_entry, start) == 0;
#endif
	if (!created) {
		noct_unpin_local(start->env, 3, &start->func_v, &start->param_v, &start->handle_v);
		noct_release_thread_env(start->env);
		noct_free(start);
		noct_error(env, N_TR("Cannot create a thread."));
		return false;
	}

	/* Make a return value. */
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 3, &func, &param, &handle);

	return true;
}

static void
thread_finalizer(
	void *native_pointer)
{
	struct thread_obj *obj;

	obj = (struct thread_obj *)native_pointer;
	if (obj == NULL)
		return;

	/* Let an unjoined thread release its resources on exit. */
	if (!obj->joined) {
#if defined(NOCT_TARGET_WINDOWS)
		CloseHandle(obj->handle);
#else
		pthread_detach(obj->handle);
#endif
	}

	noct_free(obj);
}

/* Implementation of Thread.joinThread() */
static bool
cfunc_Thread_joinThread(
	NoctEnv *env)
{
	NoctValue handle, ret, err;
	struct thread_obj *obj;
	bool has_result, has_error;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	memset(&err, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &handle, &ret, &err);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, THREAD_MAGIC, (void **)&obj))
		return false;

	if (obj->joined) {
		noct_error(env, N_TR("Thread is already joined."));
		return false;
	}

	/* Wait for the thread. This is a blocking region. */
	noct_enter_blocking(env);
#if defined(NOCT_TARGET_WINDOWS)
	WaitForSingleObject(obj->handle, INFINITE);
	CloseHandle(obj->handle);
#else
	pthread_join(obj->handle, NULL);
#endif
	noct_leave_blocking(env);

	obj->joined = 1;

	/* Propagate a thread error if any. */
	if (!noct_check_dict_key_cstr(env, &handle, "error", &has_error))
		return false;
	if (has_error) {
		const char *msg = NULL;
		if (!noct_get_dict_elem_check_string(env, &handle, "error", &err, &msg))
			return false;
		noct_error(env, N_TR("Thread error: %s"), msg);
		return false;
	}

	/* Make a return value. */
	if (!noct_check_dict_key_cstr(env, &handle, "result", &has_result))
		return false;
	if (has_result) {
		if (!noct_get_dict_elem_cstr(env, &handle, "result", &ret))
			return false;
	}
	if (!noct_set_return(env, &ret))
		return false;

	noct_unpin_local(env, 3, &handle, &ret, &err);

	return true;
}

/*
 * Shared Object
 */

static void
sync_finalizer(
	void *native_pointer)
{
	struct sync_obj *obj;

	obj = (struct sync_obj *)native_pointer;
	if (obj == NULL)
		return;

	thr_mutex_destroy(&obj->mutex);
	noct_free(obj);
}

/* Make a shared/locked handle. */
static bool
make_sync_handle(
	NoctEnv *env,
	NoctValue *handle)
{
	struct sync_obj *obj;

	obj = noct_malloc(sizeof(struct sync_obj));
	if (obj == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(obj, 0, sizeof(struct sync_obj));
	obj->magic = SYNC_MAGIC;
	if (!thr_mutex_init(&obj->mutex)) {
		noct_free(obj);
		noct_error(env, N_TR("Cannot create a mutex."));
		return false;
	}

	if (!make_handle_dict(env, handle, obj, sync_finalizer)) {
		thr_mutex_destroy(&obj->mutex);
		noct_free(obj);
		return false;
	}

	return true;
}

/* Implementation of Thread.createShared() */
static bool
cfunc_Thread_createShared(
	NoctEnv *env)
{
	NoctValue value, copy, handle;

	memset(&value, 0, sizeof(NoctValue));
	memset(&copy, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &value, &copy, &handle);

	/* Get parameters. */
	if (!noct_get_arg(env, 0, &value))
		return false;

	/* Make a handle. */
	if (!make_sync_handle(env, &handle))
		return false;

	/* Store a deep copy as the initial value. */
	if (!deep_copy_value(env, &copy, &value, 0))
		return false;
	if (!noct_set_dict_elem_cstr(env, &handle, "value", &copy))
		return false;

	/* Make a return value. */
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 3, &value, &copy, &handle);

	return true;
}

/* Implementation of Thread.updateShared() */
static bool
cfunc_Thread_updateShared(
	NoctEnv *env)
{
	NoctValue handle, value, copy, ret;
	struct sync_obj *obj;
	bool ok;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&value, 0, sizeof(NoctValue));
	memset(&copy, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 4, &handle, &value, &copy, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!noct_get_arg(env, 1, &value))
		return false;
	if (!get_handle_native(env, &handle, SYNC_MAGIC, (void **)&obj))
		return false;

	/* Store a deep copy under the lock. */
	thr_mutex_lock_blocking(env, &obj->mutex);
	ok = deep_copy_value(env, &copy, &value, 0);
	if (ok)
		ok = noct_set_dict_elem_cstr(env, &handle, "value", &copy);
	thr_mutex_unlock(&obj->mutex);
	if (!ok)
		return false;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 4, &handle, &value, &copy, &ret);

	return true;
}

/* Implementation of Thread.snapshotShared() */
static bool
cfunc_Thread_snapshotShared(
	NoctEnv *env)
{
	NoctValue handle, value, copy;
	struct sync_obj *obj;
	bool ok;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&value, 0, sizeof(NoctValue));
	memset(&copy, 0, sizeof(NoctValue));
	noct_pin_local(env, 3, &handle, &value, &copy);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, SYNC_MAGIC, (void **)&obj))
		return false;

	/* Take a deep copy under the lock. */
	thr_mutex_lock_blocking(env, &obj->mutex);
	ok = noct_get_dict_elem_cstr(env, &handle, "value", &value);
	if (ok)
		ok = deep_copy_value(env, &copy, &value, 0);
	thr_mutex_unlock(&obj->mutex);
	if (!ok)
		return false;

	/* Make a return value. */
	if (!noct_set_return(env, &copy))
		return false;

	noct_unpin_local(env, 3, &handle, &value, &copy);

	return true;
}

/*
 * Atomic Counter
 */

static void
counter_finalizer(
	void *native_pointer)
{
	if (native_pointer != NULL)
		noct_free(native_pointer);
}

/* Implementation of Thread.createCounter() */
static bool
cfunc_Thread_createCounter(
	NoctEnv *env)
{
	NoctValue handle;
	struct counter_obj *obj;

	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 1, &handle);

	/* Make a control block. */
	obj = noct_malloc(sizeof(struct counter_obj));
	if (obj == NULL) {
		noct_out_of_memory(env);
		return false;
	}
	memset(obj, 0, sizeof(struct counter_obj));
	obj->magic = COUNTER_MAGIC;

	/* Make a handle dictionary. */
	if (!make_handle_dict(env, &handle, obj, counter_finalizer)) {
		noct_free(obj);
		return false;
	}

	/* Make a return value. */
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 1, &handle);

	return true;
}

/* Implementation of Thread.incrementCounter() */
static bool
cfunc_Thread_incrementCounter(
	NoctEnv *env)
{
	NoctValue handle, ret;
	struct counter_obj *obj;
	int new_value;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &handle, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, COUNTER_MAGIC, (void **)&obj))
		return false;

	/* Increment. */
	new_value = atomic_fetch_add_release_int(&obj->value, 1) + 1;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, new_value))
		return false;

	noct_unpin_local(env, 2, &handle, &ret);

	return true;
}

/* Implementation of Thread.getCounter() */
static bool
cfunc_Thread_getCounter(
	NoctEnv *env)
{
	NoctValue handle, ret;
	struct counter_obj *obj;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &handle, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!get_handle_native(env, &handle, COUNTER_MAGIC, (void **)&obj))
		return false;

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, atomic_load_acquire_int(&obj->value)))
		return false;

	noct_unpin_local(env, 2, &handle, &ret);

	return true;
}

/*
 * Locked Dictionary
 */

/* Implementation of Thread.createLocked() */
static bool
cfunc_Thread_createLocked(
	NoctEnv *env)
{
	NoctValue value, handle;

	memset(&value, 0, sizeof(NoctValue));
	memset(&handle, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &value, &handle);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &value))
		return false;

	/* Make a handle. */
	if (!make_sync_handle(env, &handle))
		return false;

	/*
	 * Store the dictionary as-is: it is intentionally shared and
	 * must only be touched under Thread.withLock().
	 */
	if (!noct_set_dict_elem_cstr(env, &handle, "value", &value))
		return false;

	/* Make a return value. */
	if (!noct_set_return(env, &handle))
		return false;

	noct_unpin_local(env, 2, &value, &handle);

	return true;
}

/* Implementation of Thread.withLock() */
static bool
cfunc_Thread_withLock(
	NoctEnv *env)
{
	NoctValue handle, func, value, ret;
	NoctFunc *f;
	struct sync_obj *obj;
	bool ok;

	memset(&handle, 0, sizeof(NoctValue));
	memset(&func, 0, sizeof(NoctValue));
	memset(&value, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 4, &handle, &func, &value, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_dict(env, 0, &handle))
		return false;
	if (!noct_get_arg_check_func(env, 1, &func, &f))
		return false;
	if (!get_handle_native(env, &handle, SYNC_MAGIC, (void **)&obj))
		return false;

	/* Call the function under the lock. */
	thr_mutex_lock_blocking(env, &obj->mutex);
	ok = noct_get_dict_elem_cstr(env, &handle, "value", &value);
	if (ok)
		ok = noct_call(env, f, 1, &value, &ret);
	thr_mutex_unlock(&obj->mutex);
	if (!ok)
		return false;

	/* Make a return value. */
	if (!noct_set_return(env, &ret))
		return false;

	noct_unpin_local(env, 4, &handle, &func, &value, &ret);

	return true;
}

/*
 * Sleep
 */

/* Implementation of Thread.sleep() */
static bool
cfunc_Thread_sleep(
	NoctEnv *env)
{
	NoctValue ms, ret;
	size_t ms_n;

	memset(&ms, 0, sizeof(NoctValue));
	memset(&ret, 0, sizeof(NoctValue));
	noct_pin_local(env, 2, &ms, &ret);

	/* Get parameters. */
	if (!noct_get_arg_check_int_long(env, 0, &ms, &ms_n))
		return false;

	/* Sleep. This is a blocking region. */
	noct_enter_blocking(env);
#if defined(NOCT_TARGET_WINDOWS)
	Sleep((DWORD)ms_n);
#else
	{
		struct timespec ts;
		ts.tv_sec = (time_t)(ms_n / 1000);
		ts.tv_nsec = (long)(ms_n % 1000) * 1000000L;
		while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
			;
	}
#endif
	noct_leave_blocking(env);

	/* Make a return value. */
	if (!noct_set_return_make_int(env, &ret, 1))
		return false;

	noct_unpin_local(env, 2, &ms, &ret);

	return true;
}
