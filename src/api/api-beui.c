/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Compile-time dispatcher for the selected BeUI target backend. */

#include <noct/beui.h>

#if defined(NOCT_TARGET_PC98DOS)
bool noct_register_api_beui_pc98dos(NoctEnv *env);
#elif defined(NOCT_USE_BEUI_ZEDBSD)
bool noct_register_api_beui_zedbsd(NoctEnv *env);
#elif defined(NOCT_USE_BEUI_SDL2)
bool noct_register_api_beui_sdl2(NoctEnv *env);
#endif

bool
noct_register_api_beui(NoctEnv *env)
{
#if defined(NOCT_TARGET_PC98DOS)
	return noct_register_api_beui_pc98dos(env);
#elif defined(NOCT_USE_BEUI_ZEDBSD)
	return noct_register_api_beui_zedbsd(env);
#elif defined(NOCT_USE_BEUI_SDL2)
	return noct_register_api_beui_sdl2(env);
#else
	/* Keep the module loadable when the build has no display backend. */
	return noct_register_api_beui_with_hal(env, NULL);
#endif
}
