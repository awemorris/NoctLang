/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Private CLI accelerator initialization boundary.
 */

#ifndef NOCT_ACCEL_CLI_H
#define NOCT_ACCEL_CLI_H

#include "noct.h"

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
	const char *gpu_name);

#endif
