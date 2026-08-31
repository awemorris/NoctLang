/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Target-neutral accelerator buffer residency classification.
 */

#include "accel_residency.h"

/*
 * Classifies the initial host-backed residency subset conservatively.
 */
int
accel_residency_classify_buffer(
	const struct hir_memory_object *object,
	bool reassigned)
{
	if (object == NULL)
		return ACCEL_RESIDENCY_UNSUPPORTED;
	if (reassigned)
		return ACCEL_RESIDENCY_UNSUPPORTED;

	if (object->storage == HIR_MEMORY_STORAGE_PARAMETER)
		return ACCEL_RESIDENCY_PARAMETER_HOST;
	if (object->storage == HIR_MEMORY_STORAGE_LOCAL)
		return ACCEL_RESIDENCY_LOCAL_HOST;

	return ACCEL_RESIDENCY_UNSUPPORTED;
}
