/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#ifndef NOCT_ACCEL_MUTEX_H
#define NOCT_ACCEL_MUTEX_H

#include <noct/c89compat.h>

#if defined(NOCT_TARGET_WINDOWS)
#include <windows.h>
#else
#include <pthread.h>
#endif

struct accel_mutex {
#if defined(NOCT_TARGET_WINDOWS)
	SRWLOCK native;
#else
	pthread_mutex_t native;
#endif
	bool initialized;
};

bool accel_mutex_init(struct accel_mutex *mutex);
void accel_mutex_lock(struct accel_mutex *mutex);
void accel_mutex_unlock(struct accel_mutex *mutex);
void accel_mutex_destroy(struct accel_mutex *mutex);

#endif
