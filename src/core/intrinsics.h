/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Core intrinsics
 */

#ifndef NOCT_INTRINSICS_H
#define NOCT_INTRINSICS_H

#include <noct/c89compat.h>

struct rt_env;

/*
 * Intrinsics
 */

/* Register intrinsics. */
bool
rt_register_intrinsics(
	struct rt_env *rt);

#endif
