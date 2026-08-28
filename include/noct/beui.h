/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * Public entry point for the non-standard BeUI scripting API.  The selected
 * platform implementation owns its HAL and all rendering state privately.
 */

#ifndef NOCT_BEUI_H
#define NOCT_BEUI_H

#include <noct/c89compat.h>
#include <noct/noct.h>

NOCT_DLL
bool noct_register_api_beui(NoctEnv *env);

#endif
