/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * BeUI PC-98 display selection, imported from Boots.  Probing prefers
 * the Core-Graph / Cirrus board at 640x480x8 and falls back to the
 * always-present GDC at 640x400x4, so one HAL covers both machines.
 */

#ifndef NOCT_BEUI_PC98_AUTO_H
#define NOCT_BEUI_PC98_AUTO_H

#include "beui-pc98-cirrus.h"
#include "beui-pc98-glyph.h"
#include "beui-pc98-gdc.h"

struct noct_beui_pc98_auto {
	struct noct_beui_pc98_cirrus cirrus;
	struct noct_beui_pc98_gdc gdc;
	struct noct_beui_pc98_glyph glyph;
	struct noct_beui_hal cirrus_hal;
	struct noct_beui_hal gdc_hal;
	struct noct_beui_display_hal *active;
};

void noct_beui_pc98_auto_default(
	struct noct_beui_pc98_auto *backend,
	noct_beui_display_reset_fn display_reset,
	noct_beui_display_reset_fn display_stop, void *bios_context,
	noct_beui_pc98_in8_fn port_in8, noct_beui_pc98_out8_fn port_out8,
	void *io_context, volatile uint8_t *cirrus_framebuffer);
int noct_beui_pc98_auto_make_hal(struct noct_beui_hal *hal,
	struct noct_beui_pc98_auto *backend);

#endif
