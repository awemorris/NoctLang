/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* VM-local object-model dispatch for lifecycle and non-runtime callers. */

#include "objectmodel.h"
#include "objectmodel-backend.h"

static INLINE bool
use_mt(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	return env->vm->config.object_model == NOCT_OBJECT_MODEL_MULTI;
#else
	UNUSED_PARAMETER(env);
	return false;
#endif
}

bool
om_freeze_dict(
	struct rt_env *env,
	struct rt_value *dict)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env))
		return om_mt_freeze_dict(env, dict);
#endif
	return om_st_freeze_dict(env, dict);
}

bool
om_enter_gc(
	struct rt_env *env,
	int level)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env))
		return om_mt_enter_gc(env, level);
#endif
	return om_st_enter_gc(env, level);
}

void
om_leave_gc(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env)) {
		om_mt_leave_gc(env);
		return;
	}
#endif
	om_st_leave_gc(env);
}

void
om_safepoint(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env)) {
		om_mt_safepoint(env);
		return;
	}
#endif
	om_st_safepoint(env);
}

void
om_init_env(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env)) {
		om_mt_init_env(env);
		return;
	}
#endif
	om_st_init_env(env);
}

void
om_enter_blocking(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env)) {
		om_mt_enter_blocking(env);
		return;
	}
#endif
	om_st_enter_blocking(env);
}

void
om_leave_blocking(
	struct rt_env *env)
{
#if defined(NOCT_USE_MULTITHREAD)
	if (use_mt(env)) {
		om_mt_leave_blocking(env);
		return;
	}
#endif
	om_st_leave_blocking(env);
}
