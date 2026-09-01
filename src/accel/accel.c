/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * CLI-owned accelerator initialization transaction.
 */

#include "accel_cli.h"
#include "accel_context.h"
#include "accel_vulkan.h"
#include "runtime.h"

#include <stdlib.h>

/*
 * Initializes the selected accelerator for one VM.
 *
 * A NULL device name selects the default suitable device.  A non-NULL
 * name is matched exactly against the backend's UTF-8 display name.
 */
bool
accel_initialize(
	NoctVM *vm,
	NoctEnv *env,
	const char *gpu_name)
{
	const struct accel_backend_ops *ops;
	struct accel_context *context;
	void *backend_state;

	ops = NULL;
	context = NULL;
	backend_state = NULL;

	if (vm == NULL || env == NULL)
		return false;
	if (env->vm != vm) {
		rt_error(env, N_TR("Accelerator VM and environment do not match."));
		return false;
	}
	if (vm->accel_optimize_func != NULL ||
	    vm->accel_optimize_userdata != NULL) {
		rt_error(env, N_TR("An accelerator is already attached to this VM."));
		return false;
	}

	if (!accel_vulkan_create(
		env,
		gpu_name,
		&ops,
		&backend_state)) {
		return false;
	}

	if (!accel_context_create(vm, ops, backend_state, &context)) {
		ops->destroy_backend_state(backend_state);
		rt_out_of_memory(env);
		return false;
	}
	backend_state = NULL;

	if (!accel_context_register_runtime(context, env)) {
		accel_context_destroy(context);
		return false;
	}

	accel_context_attach(context);

	return true;
}

/*
 * Detaches and destroys the CLI-owned accelerator before VM destruction.
 */
void
accel_finalize(
	NoctVM *vm)
{
	struct accel_context *context;

	if (vm == NULL)
		return;

	context = vm->accel_optimize_userdata;
	if (context == NULL)
		return;

	accel_context_detach(context);
	accel_context_destroy(context);
}
