/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Accelerator: Private Mutex
 */

#include "accel_mutex.h"

#include <assert.h>
#include <stdlib.h>

/*
 * Initializes an accelerator mutex.
 */
bool
accel_mutex_init(
	struct accel_mutex *mutex)
{
#if !defined(NOCT_TARGET_WINDOWS)
	pthread_mutexattr_t attr;
	int result;
	int destroy_result;

#endif
	assert(mutex != NULL);
	assert(!mutex->initialized);

#if defined(NOCT_TARGET_WINDOWS)
	InitializeSRWLock(&mutex->native);
	mutex->initialized = true;
#else
	result = pthread_mutexattr_init(&attr);
	if (result != 0)
		return false;

	result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
	if (result != 0) {
		(void)pthread_mutexattr_destroy(&attr);
		return false;
	}

	result = pthread_mutex_init(&mutex->native, &attr);
	destroy_result = pthread_mutexattr_destroy(&attr);
	if (result != 0)
		return false;
	if (destroy_result != 0) {
		(void)pthread_mutex_destroy(&mutex->native);
		return false;
	}

	mutex->initialized = true;
#endif

	return true;
}

/*
 * Locks an accelerator mutex.
 */
void
accel_mutex_lock(
	struct accel_mutex *mutex)
{
#if !defined(NOCT_TARGET_WINDOWS)
	int result;

#endif
	assert(mutex != NULL);
	assert(mutex->initialized);

#if defined(NOCT_TARGET_WINDOWS)
	AcquireSRWLockExclusive(&mutex->native);
#else
	result = pthread_mutex_lock(&mutex->native);
	if (result != 0)
		abort();
#endif
}

/*
 * Unlocks an accelerator mutex.
 */
void
accel_mutex_unlock(
	struct accel_mutex *mutex)
{
#if !defined(NOCT_TARGET_WINDOWS)
	int result;

#endif
	assert(mutex != NULL);
	assert(mutex->initialized);

#if defined(NOCT_TARGET_WINDOWS)
	ReleaseSRWLockExclusive(&mutex->native);
#else
	result = pthread_mutex_unlock(&mutex->native);
	if (result != 0)
		abort();
#endif
}

/*
 * Destroys an accelerator mutex.
 */
void
accel_mutex_destroy(
	struct accel_mutex *mutex)
{
#if !defined(NOCT_TARGET_WINDOWS)
	int result;

#endif
	if (mutex == NULL)
		return;
	if (!mutex->initialized)
		return;

#if !defined(NOCT_TARGET_WINDOWS)
	result = pthread_mutex_destroy(&mutex->native);
	if (result != 0)
		abort();
#endif

	mutex->initialized = false;
}
