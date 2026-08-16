/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Immutable function table passed to dynamically loaded Noct libraries. */

#include <noct/noct.h>
#include "noct-api.h"

static const NoctAPI noct_api = {
	NOCT_API_ABI_VERSION_1,
	(uint32_t)sizeof(NoctAPI),
	NOCT_API_FEATURE_CFUNC_DATA | NOCT_API_FEATURE_VM_FINALIZER,
#define NOCT_API_FIELD(ret, name, args) name,
#include <noct/noct_api_fields.def>
#undef NOCT_API_FIELD
};

const NoctAPI *
noct_get_api_table(void)
{
	return &noct_api;
}
