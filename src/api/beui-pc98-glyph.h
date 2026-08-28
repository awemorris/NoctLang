/*
 * Boots PC-98 CGROM glyph backend
 * Copyright (C) 2026 Awe Morris
 * SPDX-License-Identifier: Zlib
 */

#ifndef NOCT_BEUI_PC98_GLYPH_H
#define NOCT_BEUI_PC98_GLYPH_H

#include "beui-internal.h"
#include "beui-pc98-gdc.h"

struct noct_beui_pc98_glyph {
	void *io_context;
	uint8_t (*port_in8)(void *context, uint16_t port);
	void (*port_out8)(void *context, uint16_t port, uint8_t value);
	volatile uint8_t *cg_window;
	struct noct_beui_display_hal *display;
	struct {
		uint16_t jis;
		uint8_t valid;
		uint8_t font[32];
	} cache[64];
	unsigned cache_next;
};

void noct_beui_pc98_glyph_default(
	struct noct_beui_pc98_glyph *backend,
	struct noct_beui_display_hal *display,
	noct_beui_pc98_in8_fn port_in8, noct_beui_pc98_out8_fn port_out8,
	void *io_context);
int noct_beui_pc98_glyph_make_hal(struct noct_beui_glyph_hal *hal,
	struct noct_beui_pc98_glyph *backend);
uint16_t noct_beui_pc98_unicode_to_jis(uint32_t codepoint);

#endif
