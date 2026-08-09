/*
 * Boots BeUI NEC PC-9800 GDC safe-mode backend
 * Copyright (C) 2026 Awe Morris
 * Copyright (C) 1996-2024 Keiichi Tabata
 * SPDX-License-Identifier: Zlib
 *
 * Display sequencing is adapted from StratoHAL 98disp_gdc.c at commit
 * 76e909577bdf4629f11e473539b446a948fef830. This Boots version is altered
 * to preserve text VRAM and update only requested rectangles.
 */

#ifndef NOCT_BEUI_PC98_H
#define NOCT_BEUI_PC98_H

#include <noct/beui.h>

#include <stdint.h>

#define NOCT_BEUI_GDC_PLANE_BYTES (640U * 400U / 8U)

typedef int (*noct_beui_display_reset_fn)(void *context);
typedef uint8_t (*noct_beui_pc98_in8_fn)(void *context, uint16_t port);
typedef void (*noct_beui_pc98_out8_fn)(void *context, uint16_t port,
					uint8_t value);

struct noct_beui_pc98_gdc {
	void *bios_context;
	noct_beui_display_reset_fn display_reset;
	noct_beui_display_reset_fn display_stop;
	void *io_context;
	uint8_t (*port_in8)(void *context, uint16_t port);
	void (*port_out8)(void *context, uint16_t port, uint8_t value);
	volatile uint8_t *planes[4];
};

void noct_beui_pc98_gdc_default(
	struct noct_beui_pc98_gdc *backend,
	noct_beui_display_reset_fn display_reset,
	noct_beui_display_reset_fn display_stop, void *bios_context,
	noct_beui_pc98_in8_fn port_in8, noct_beui_pc98_out8_fn port_out8,
	void *io_context);
int noct_beui_pc98_gdc_make_hal(struct noct_beui_hal *hal,
				   struct noct_beui_pc98_gdc *backend);
int noct_beui_pc98_gdc_clear_graphics(
	struct noct_beui_pc98_gdc *backend);

#endif
