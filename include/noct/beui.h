/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * BeUI non-standard API: a small graphical environment for scripts.
 *
 * The API grew inside the Boots boot loader as its graphical layer and
 * keeps the same hardware abstraction: a backend owns mode save/restore
 * in enter()/leave(), and every drawing primitive goes through the HAL.
 * Registration binds one HAL per VM; on targets without a backend the
 * module can be registered with a NULL HAL and BeUI.init() reports
 * failure.
 */

#ifndef NOCT_BEUI_H
#define NOCT_BEUI_H

#include <noct/c89compat.h>
#include <noct/noct.h>

#include <stddef.h>

struct noct_beui_rect {
	unsigned x;
	unsigned y;
	unsigned width;
	unsigned height;
};

struct noct_beui_display_info {
	/* Input hint to enter(); zero means the backend's default depth. */
	unsigned preferred_bits_per_pixel;
	unsigned width;
	unsigned height;
	unsigned bits_per_pixel;
	unsigned stride;
};

enum noct_beui_image_format {
	NOCT_BEUI_IMAGE_INDEX8 = 1,
	NOCT_BEUI_IMAGE_RGB24 = 2,
};

/*
 * Images use a target-independent representation.  Indexed images always
 * contain one palette index per pixel; RGB24 pixels are tightly packed in
 * R, G, B order.  Palette entries and solid colors use 0x00RRGGBB.
 */
struct noct_beui_image {
	enum noct_beui_image_format format;
	unsigned width;
	unsigned height;
	size_t stride;
	const uint8_t *pixels;
	uint32_t palette[256];
	unsigned palette_size;
};

enum noct_beui_pointer_button {
	NOCT_BEUI_BUTTON_LEFT = 1U << 0,
	NOCT_BEUI_BUTTON_RIGHT = 1U << 1,
	NOCT_BEUI_BUTTON_MIDDLE = 1U << 2
};

/*
 * Pointer positions are absolute display coordinates.  Targets whose
 * hardware reports motion deltas (the PC-98 bus mouse) integrate and
 * clamp inside their backend, so scripts see one coordinate space on
 * every host.
 */
struct noct_beui_pointer_event {
	unsigned x;
	unsigned y;
	unsigned buttons;
};

struct noct_beui_display_hal {
	void *context;
	int (*enter)(void *context, struct noct_beui_display_info *info);
	void (*leave)(void *context);
	/*
	 * Optional.  Services the host window system and reports whether
	 * the display is still alive: 1 to continue, 0 once the user has
	 * asked to close it, negative on error.  Targets that own the
	 * whole machine leave this NULL and never close.
	 */
	int (*poll_events)(void *context);
	int (*fill)(void *context, const struct noct_beui_rect *rect,
		    uint32_t color);
	int (*line)(void *context, unsigned x0, unsigned y0, unsigned x1,
		    unsigned y1, uint32_t color);
	int (*pattern_fill)(void *context, const struct noct_beui_rect *rect,
			    uint32_t color, uint64_t pattern);
	int (*draw_image)(void *context, unsigned x, unsigned y,
			  const struct noct_beui_image *image);
	int (*draw_image_pattern)(void *context, unsigned x, unsigned y,
				  const struct noct_beui_image *image,
				  uint64_t pattern);
	int (*flush)(void *context, const struct noct_beui_rect *rectangles,
		     size_t rectangle_count);
};

struct noct_beui_glyph_hal {
	void *context;
	int (*measure)(void *context, uint32_t codepoint, unsigned *width,
		       unsigned *height);
	int (*draw)(void *context, unsigned x, unsigned y, uint32_t codepoint,
		    uint32_t foreground, uint32_t background);
};

/*
 * poll() reports the current absolute pointer state: 1 when the event
 * was filled in, 0 when nothing has changed since the last call, and a
 * negative value on error.  start() receives the display geometry so
 * relative-motion hardware can clamp to the visible area.
 */
struct noct_beui_pointer_hal {
	void *context;
	int (*start)(void *context,
		     const struct noct_beui_display_info *display);
	void (*stop)(void *context);
	int (*poll)(void *context, struct noct_beui_pointer_event *event);
};

/*
 * The clock derives time from a polled counter on some targets; callers
 * must invoke milliseconds() (or the core sleep/poll helpers) at least
 * once per hardware counter period or elapsed time is lost.
 */
struct noct_beui_clock_hal {
	void *context;
	uint64_t (*milliseconds)(void *context);
};

struct noct_beui_audio_hal {
	void *context;
	int (*start)(void *context, unsigned sample_rate, unsigned channels);
	void (*stop)(void *context);
	int (*poll)(void *context);
	int (*write)(void *context, const int16_t *samples, size_t frame_count);
};

/*
 * Real-time key state for the NOCT_BEUI_KEY_* namespace plus lowercase
 * ASCII.  is_key_down() returns 1 while the key is held, 0 when it is
 * up, and -1 for keys the target cannot sense.  drain() empties the
 * platform type-ahead buffer so keys held during a game never leak to
 * the caller after BeUI closes; the core calls it from poll and sleep.
 */
struct noct_beui_input_hal {
	void *context;
	int (*is_key_down)(void *context, int key);
	void (*drain)(void *context);
};

struct noct_beui_hal {
	struct noct_beui_display_hal display;
	struct noct_beui_glyph_hal glyph;
	struct noct_beui_pointer_hal pointer;
	struct noct_beui_clock_hal clock;
	struct noct_beui_audio_hal audio;
	struct noct_beui_input_hal input;
};

/*
 * Key codes shared with the Boots BeUI implementation; the same compiled
 * script must observe identical Key.* values on both hosts.
 */
enum noct_beui_key_code {
	NOCT_BEUI_KEY_ESCAPE = 0x1b,
	NOCT_BEUI_KEY_BACKSPACE = 0x08,
	NOCT_BEUI_KEY_TAB = 0x09,
	NOCT_BEUI_KEY_ENTER = 0x0d,
	NOCT_BEUI_KEY_PAGE_UP = 0x136,
	NOCT_BEUI_KEY_PAGE_DOWN = 0x137,
	NOCT_BEUI_KEY_INSERT = 0x138,
	NOCT_BEUI_KEY_DELETE = 0x139,
	NOCT_BEUI_KEY_UP = 0x13a,
	NOCT_BEUI_KEY_LEFT = 0x13b,
	NOCT_BEUI_KEY_RIGHT = 0x13c,
	NOCT_BEUI_KEY_DOWN = 0x13d,
	NOCT_BEUI_KEY_HOME = 0x13e,
	NOCT_BEUI_KEY_END = 0x13f,
	NOCT_BEUI_KEY_F1 = 0x162,
	NOCT_BEUI_KEY_F2 = 0x163,
	NOCT_BEUI_KEY_F3 = 0x164,
	NOCT_BEUI_KEY_F4 = 0x165,
	NOCT_BEUI_KEY_F5 = 0x166,
	NOCT_BEUI_KEY_F6 = 0x167,
	NOCT_BEUI_KEY_F7 = 0x168,
	NOCT_BEUI_KEY_F8 = 0x169,
	NOCT_BEUI_KEY_F9 = 0x16a,
	NOCT_BEUI_KEY_F10 = 0x16b,
	/* State-only: modifiers never appear in buffered key streams. */
	NOCT_BEUI_KEY_SHIFT = 0x170
};

/* Core lifecycle and drawing; see src/api/beui-core.c. */
int noct_beui_bind(const struct noct_beui_hal *hal);
int noct_beui_init(void);
int noct_beui_init_with_hint(unsigned preferred_bits_per_pixel);
void noct_beui_close(void);
void noct_beui_cleanup(void);
int noct_beui_is_open(void);
int noct_beui_get_display_info(struct noct_beui_display_info *info);
int noct_beui_fill(const struct noct_beui_rect *rect, uint32_t color);
int noct_beui_line(unsigned x0, unsigned y0, unsigned x1, unsigned y1,
		    uint32_t color);
int noct_beui_pattern_fill(const struct noct_beui_rect *rect, uint32_t color,
			    uint64_t pattern);
int noct_beui_draw_image(unsigned x, unsigned y,
			  const struct noct_beui_image *image);
int noct_beui_draw_image_region(const struct noct_beui_image *image,
				 unsigned source_x, unsigned source_y,
				 unsigned width, unsigned height,
				 unsigned destination_x,
				 unsigned destination_y);
int noct_beui_draw_image_pattern(unsigned x, unsigned y,
				  const struct noct_beui_image *image,
				  uint64_t pattern);
int noct_beui_measure_text(const char *text, unsigned *width,
			    unsigned *height);
int noct_beui_draw_text(const char *text, unsigned x, unsigned y,
			 uint32_t foreground, uint32_t background);
/*
 * Services the backends and reports whether the display is still alive:
 * 1 to keep running, 0 once it has closed.  Scripts drive their main
 * loop from it, so a closed window ends the loop instead of raising.
 */
int noct_beui_poll(void);
int noct_beui_flush(void);
int noct_beui_get_milliseconds(uint64_t *milliseconds);
int noct_beui_sleep(unsigned milliseconds);
int noct_beui_is_key_down(int key);
void noct_beui_drain_input(void);
/* Last known absolute pointer state; 0 when no pointer is available. */
int noct_beui_get_pointer(unsigned *x, unsigned *y, unsigned *buttons);

/*
 * Image decoding and the handle registry; see src/api/beui-image.c.
 *
 * Only uncompressed Windows BMP with 1, 4, 8, or 24 bits per pixel is
 * decoded.  BMP is used instead of PNG so freestanding hosts such as the
 * Boots pre-boot environment need no DEFLATE implementation.
 */
int noct_beui_bmp_measure(const void *data, size_t size,
			   enum noct_beui_image_format *format,
			   unsigned *width, unsigned *height,
			   size_t *pixel_bytes);
int noct_beui_bmp_decode(const void *data, size_t size, void *pixel_storage,
			  size_t pixel_capacity, struct noct_beui_image *image);

/*
 * Decodes a BMP into a registry entry and returns its handle, or 0 on
 * failure.  Handles stay valid until noct_beui_image_destroy() or
 * noct_beui_cleanup().
 */
int noct_beui_image_load_bmp(const void *data, size_t size);
const struct noct_beui_image *noct_beui_image_get(int handle);
int noct_beui_image_destroy(int handle);

/*
 * Register the "BeUI.*" API and the "Key" dictionary against a HAL.
 * (non-standard API)
 */
NOCT_DLL
bool
noct_register_api_beui(
	NoctEnv *env,
	const struct noct_beui_hal *hal);

/*
 * Register BeUI backed by the PC-98 MS-DOS backend (GDC display, CGROM
 * glyphs, polled i8253 clock, BIOS key state).  Only built when
 * NOCT_TARGET_PC98DOS is defined.
 */
NOCT_DLL
bool
noct_register_api_beui_pc98dos(
	NoctEnv *env);

/* Register BeUI backed by an SDL2 desktop window. */
NOCT_DLL
bool
noct_register_api_beui_sdl2(
	NoctEnv *env);

#endif
