/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

/*
 * Atomic Operations
 */

#ifndef NOCT_ATOMIC_H
#define NOCT_ATOMIC_H

#if defined(__GNUC__)

static NOCT_INLINE int atomic_increment(int *v)
{
	int old = __sync_fetch_and_add(v, 1);
	return old;
}

static NOCT_INLINE int atomic_decrement(int *v)
{
	int old = __sync_fetch_and_sub(v, 1);
	return old;
}

static NOCT_INLINE int atomic_read(int *v)
{
	int old = __sync_fetch_and_add(v, 0);
	return old;
}

static NOCT_INLINE void cpu_relax(void)
{
#if defined(__i386__) || defined(__x86_64__)
	// PAUSE
	__builtin_ia32_pause();
#elif defined(__arm__) || defined(__aarch64__)
	// YIELD
	__builtin_arm_yield();
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
	// Spin Loop Hint
	__asm__ __volatile__("or 27,27,27");
#endif
}

#elif defined(_MSC_VER)

#include <intrin.h>

static NOCT_INLINE int atomic_increment(int *v)
{
	_InterlockedExchangeAdd((volatile long *)v, 1);
}

static NOCT_INLINE int atomic_decrement(int *v)
{
	_InterlockedExchangeAdd((volatile long *)v, -1);
}

static NOCT_INLINE int atomic_read(int *v)
{
	_InterlockedExchangeAdd((volatile long *)v, 0);
}

#if defined(_M_IX86) || defined(_M_X64)
#include <immintrin.h>
static NOCT_INLINE void cpu_relax(void)
{
	_mm_pause();
}
#elif defined(_M_ARM64)
static NOCT_INLINE void cpu_relax(void)
{
	// yield
	__emit(0xD503207F);
}
#endif

#endif

#endif
