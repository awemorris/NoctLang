/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Target-neutral accelerator buffer residency classification.
 */

#ifndef NOCT_ACCEL_RESIDENCY_H
#define NOCT_ACCEL_RESIDENCY_H

#include "hir_opt_parallel.h"

enum accel_residency_class {
	ACCEL_RESIDENCY_PARAMETER_HOST,
	ACCEL_RESIDENCY_LOCAL_HOST,
	ACCEL_RESIDENCY_LOCAL_DEVICE,
	ACCEL_RESIDENCY_LOCAL_DEVICE_RETURN,
	ACCEL_RESIDENCY_UNSUPPORTED
};

/*
 * Classifies one GPU-visible logical buffer without changing its HIR.
 */
int
accel_residency_classify_buffer(
	const struct hir_memory_object *object,
	bool reassigned);

#endif
