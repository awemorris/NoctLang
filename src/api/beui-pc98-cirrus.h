/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 1996-2024, Keiichi Tabata
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * BeUI NEC PC-9821 Core-Graph / Cirrus GD5440 display backend, imported
 * from Boots.  The register sequence is adapted from StratoHAL
 * 98disp_cirrus.c at commit 76e909577bdf4629f11e473539b446a948fef830 and
 * is deliberately limited to the Core-Graph path at 640x480x8/24.
 * Port I/O and the linear framebuffer are injected by the embedder so
 * the driver stays compiler and host neutral.
 */

#ifndef NOCT_BEUI_PC98_CIRRUS_H
#define NOCT_BEUI_PC98_CIRRUS_H

#include <noct/beui.h>
#include "beui-pc98-gdc.h"

#include <stdint.h>

#define NOCT_BEUI_CIRRUS_WIDTH 640U
#define NOCT_BEUI_CIRRUS_HEIGHT 480U
#define NOCT_BEUI_CIRRUS_STRIDE_8 NOCT_BEUI_CIRRUS_WIDTH
#define NOCT_BEUI_CIRRUS_STRIDE_24 (NOCT_BEUI_CIRRUS_WIDTH * 3U)

struct noct_beui_pc98_cirrus {
	void *io_context;
	uint8_t (*port_in8)(void *context, uint16_t port);
	void (*port_out8)(void *context, uint16_t port, uint8_t value);
	volatile uint8_t *framebuffer;
	uint8_t saved_sleep;
	uint8_t saved_window;
	uint8_t saved_linear;
	uint8_t saved_relay;
	uint8_t bits_per_pixel;
	uint8_t active;
};

/*
 * framebuffer is the host's view of the board's linear aperture.  A
 * target that maps physical memory one-to-one passes the aperture
 * address itself; a hosted target passes whatever its memory manager
 * returned for that physical range.
 */
void noct_beui_pc98_cirrus_default(
	struct noct_beui_pc98_cirrus *backend,
	noct_beui_pc98_in8_fn port_in8, noct_beui_pc98_out8_fn port_out8,
	void *io_context, volatile uint8_t *framebuffer);
int noct_beui_pc98_cirrus_make_hal(
	struct noct_beui_hal *hal,
	struct noct_beui_pc98_cirrus *backend);

#endif
