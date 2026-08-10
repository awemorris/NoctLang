/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * BeUI PC-98 display selection, imported from Boots.  Probing prefers
 * the Core-Graph / Cirrus board at 640x480x8 and falls back to the
 * always-present GDC at 640x400x4, so one HAL covers both machines.
 */

#include "beui-pc98-auto.h"

#include <string.h>

static int
auto_enter(void *context, struct noct_beui_display_info *info)
{
	struct noct_beui_pc98_auto *backend = context;

	backend->active = NULL;
	if (backend->cirrus_hal.display.enter != NULL &&
	    backend->cirrus_hal.display.enter(
		backend->cirrus_hal.display.context, info)) {
		backend->active = &backend->cirrus_hal.display;
		return 1;
	}
	if (backend->gdc_hal.display.enter != NULL &&
	    backend->gdc_hal.display.enter(backend->gdc_hal.display.context,
					   info)) {
		backend->active = &backend->gdc_hal.display;
		return 1;
	}
	return 0;
}

static void
auto_leave(void *context)
{
	struct noct_beui_pc98_auto *backend = context;

	if (backend->active != NULL && backend->active->leave != NULL)
		backend->active->leave(backend->active->context);
	backend->active = NULL;
}

static int
auto_fill(void *context, const struct noct_beui_rect *rect, uint32_t color)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL && backend->active->fill != NULL &&
		backend->active->fill(backend->active->context, rect, color);
}

static int
auto_line(void *context, unsigned x0, unsigned y0, unsigned x1, unsigned y1,
	  uint32_t color)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL && backend->active->line != NULL &&
		backend->active->line(backend->active->context, x0, y0, x1, y1,
				      color);
}

static int
auto_pattern_fill(void *context, const struct noct_beui_rect *rect,
		  uint32_t color, uint64_t pattern)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL &&
		backend->active->pattern_fill != NULL &&
		backend->active->pattern_fill(backend->active->context, rect,
					      color, pattern);
}

static int
auto_draw_image(void *context, unsigned x, unsigned y,
		const struct noct_beui_image *image)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL && backend->active->draw_image != NULL &&
		backend->active->draw_image(backend->active->context, x, y, image);
}

static int
auto_draw_image_pattern(void *context, unsigned x, unsigned y,
			const struct noct_beui_image *image,
			uint64_t pattern)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL &&
		backend->active->draw_image_pattern != NULL &&
		backend->active->draw_image_pattern(backend->active->context,
			x, y, image, pattern);
}

static int
auto_flush(void *context, const struct noct_beui_rect *rectangles,
	   size_t rectangle_count)
{
	struct noct_beui_pc98_auto *backend = context;

	return backend->active != NULL &&
		(backend->active->flush == NULL ||
		 backend->active->flush(backend->active->context, rectangles,
					rectangle_count));
}

void
noct_beui_pc98_auto_default(struct noct_beui_pc98_auto *backend,
	noct_beui_display_reset_fn display_reset,
	noct_beui_display_reset_fn display_stop, void *bios_context,
	noct_beui_pc98_in8_fn port_in8, noct_beui_pc98_out8_fn port_out8,
	void *io_context, volatile uint8_t *cirrus_framebuffer)
{
	memset(backend, 0, sizeof(*backend));
	noct_beui_pc98_cirrus_default(&backend->cirrus, port_in8, port_out8,
				       io_context, cirrus_framebuffer);
	noct_beui_pc98_gdc_default(&backend->gdc, display_reset, display_stop,
				    bios_context, port_in8, port_out8,
				    io_context);
	noct_beui_pc98_glyph_default(&backend->glyph, NULL, port_in8, port_out8,
				      io_context);
}

int
noct_beui_pc98_auto_make_hal(struct noct_beui_hal *hal,
			      struct noct_beui_pc98_auto *backend)
{
	if (hal == NULL || backend == NULL ||
	    !noct_beui_pc98_cirrus_make_hal(&backend->cirrus_hal,
					      &backend->cirrus) ||
	    !noct_beui_pc98_gdc_make_hal(&backend->gdc_hal, &backend->gdc))
		return 0;
	memset(hal, 0, sizeof(*hal));
	hal->display.context = backend;
	hal->display.enter = auto_enter;
	hal->display.leave = auto_leave;
	hal->display.fill = auto_fill;
	hal->display.line = auto_line;
	hal->display.pattern_fill = auto_pattern_fill;
	hal->display.draw_image = auto_draw_image;
	hal->display.draw_image_pattern = auto_draw_image_pattern;
	hal->display.flush = auto_flush;
	backend->glyph.display = &hal->display;
	return noct_beui_pc98_glyph_make_hal(&hal->glyph, &backend->glyph);
}
