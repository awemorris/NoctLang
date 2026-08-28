/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* SDL2-backed BeUI HAL for desktop hosts. */

#include <noct/noct.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Complete platform-private BeUI contract.  SDL2 owns this copy rather than
 * depending on a shared backend or private implementation header.
 */
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

/* Platform-private core lifecycle and drawing contract. */
static int noct_beui_bind(const struct noct_beui_hal *hal);
static int noct_beui_init(void);
static int noct_beui_init_with_hint(unsigned preferred_bits_per_pixel);
static void noct_beui_close(void);
static void noct_beui_cleanup(void);
static int noct_beui_is_open(void);
static int noct_beui_get_display_info(struct noct_beui_display_info *info);
static int noct_beui_fill(const struct noct_beui_rect *rect, uint32_t color);
static int noct_beui_line(unsigned x0, unsigned y0, unsigned x1, unsigned y1,
		   uint32_t color);
static int noct_beui_pattern_fill(const struct noct_beui_rect *rect, uint32_t color,
			   uint64_t pattern);
static int noct_beui_draw_image(unsigned x, unsigned y,
			 const struct noct_beui_image *image);
static int noct_beui_draw_image_region(const struct noct_beui_image *image,
				unsigned source_x, unsigned source_y,
				unsigned width, unsigned height,
				unsigned destination_x, unsigned destination_y);
static int noct_beui_draw_image_pattern(unsigned x, unsigned y,
				 const struct noct_beui_image *image,
				 uint64_t pattern);
static int noct_beui_measure_text(const char *text, unsigned *width, unsigned *height);
static int noct_beui_draw_text(const char *text, unsigned x, unsigned y,
			uint32_t foreground, uint32_t background);
/*
 * Services the backends and reports whether the display is still alive:
 * 1 to keep running, 0 once it has closed.  Scripts drive their main
 * loop from it, so a closed window ends the loop instead of raising.
 */
static int noct_beui_poll(void);
static int noct_beui_flush(void);
static int noct_beui_get_milliseconds(uint64_t *milliseconds);
static int noct_beui_sleep(unsigned milliseconds);
static int noct_beui_is_key_down(int key);
static void noct_beui_drain_input(void);
/* Last known absolute pointer state; 0 when no pointer is available. */
static int noct_beui_get_pointer(unsigned *x, unsigned *y, unsigned *buttons);

/*
 * Platform-private image decoding and handle registry.
 *
 * Only uncompressed Windows BMP with 1, 4, 8, or 24 bits per pixel is
 * decoded.  BMP is used instead of PNG so freestanding hosts such as the
 * Boots pre-boot environment need no DEFLATE implementation.
 */
static int noct_beui_bmp_measure(const void *data, size_t size,
			  enum noct_beui_image_format *format, unsigned *width,
			  unsigned *height, size_t *pixel_bytes);
static int noct_beui_bmp_decode(const void *data, size_t size, void *pixel_storage,
			 size_t pixel_capacity, struct noct_beui_image *image);

/*
 * Decodes a BMP into a registry entry and returns its handle, or 0 on
 * failure.  Handles stay valid until noct_beui_image_destroy(), platform API
 * re-registration, or process teardown.
 */
static int noct_beui_image_load_bmp(const void *data, size_t size);
static const struct noct_beui_image *noct_beui_image_get(int handle);
static int noct_beui_image_destroy(int handle);
/* Platform-private BeUI lifecycle, drawing state, and image registry. */
/*
 * A decoded image lives until the script destroys it or the VM shuts
 * down.  Pixels are appended to the entry so one allocation covers both.
 */
struct noct_beui_image_entry {
	struct noct_beui_image_entry *next;
	int handle;
	struct noct_beui_image image;
	uint8_t pixels[1];
};

#define NOCT_BEUI_IMAGE_SOURCE_MAX (2U * 1024U * 1024U)
#define NOCT_BEUI_IMAGE_PIXELS_MAX (2U * 1024U * 1024U)

struct noct_beui_state {
	const struct noct_beui_hal *hal;
	struct noct_beui_display_info display;
	int display_open;
	int pointer_open;
	int audio_open;
	int close_requested;
	unsigned pointer_x;
	unsigned pointer_y;
	unsigned pointer_buttons;
	struct noct_beui_image_entry *images;
	int next_image_handle;
};

static struct noct_beui_state state;

static int
noct_beui_bind(const struct noct_beui_hal *hal)
{
	if (state.display_open || state.pointer_open || state.audio_open)
		return 0;
	state.hal = hal;
	return 1;
}

static int
noct_beui_init(void)
{
	return noct_beui_init_with_hint(0);
}

static int
noct_beui_init_with_hint(unsigned preferred_bits_per_pixel)
{
	if (state.display_open)
		return 1;
	if (state.hal == NULL || state.hal->display.enter == NULL ||
	    state.hal->display.leave == NULL)
		return 0;
	memset(&state.display, 0, sizeof(state.display));
	state.display.preferred_bits_per_pixel = preferred_bits_per_pixel;
	if (!state.hal->display.enter(state.hal->display.context,
				      &state.display))
		return 0;
	state.display_open = 1;
	state.close_requested = 0;
	state.pointer_buttons = 0;
	/* Input typed before the graphics session belongs to the caller's
	 * previous screen.  Discard it before the application begins waiting
	 * for BeUI keys, just as close() drains keys before returning. */
	noct_beui_drain_input();
	if (state.display.width == 0 || state.display.height == 0)
		goto fail;
	/* A pointer starts centred so scripts never read a stale origin. */
	state.pointer_x = state.display.width / 2U;
	state.pointer_y = state.display.height / 2U;
	if (state.hal->pointer.start != NULL) {
		if (!state.hal->pointer.start(state.hal->pointer.context,
					      &state.display))
			goto fail;
		state.pointer_open = 1;
	}
	return 1;

fail:
	noct_beui_close();
	return 0;
}

static void
noct_beui_close(void)
{
	if (state.hal == NULL)
		return;
	/* Keys held during a session must not leak to the caller. */
	noct_beui_drain_input();
	if (state.audio_open && state.hal->audio.stop != NULL)
		state.hal->audio.stop(state.hal->audio.context);
	state.audio_open = 0;
	if (state.pointer_open && state.hal->pointer.stop != NULL)
		state.hal->pointer.stop(state.hal->pointer.context);
	state.pointer_open = 0;
	if (state.display_open && state.hal->display.leave != NULL)
		state.hal->display.leave(state.hal->display.context);
	state.display_open = 0;
	memset(&state.display, 0, sizeof(state.display));
}

static void
noct_beui_cleanup(void)
{
	struct noct_beui_image_entry *entry = state.images;

	noct_beui_close();
	while (entry != NULL) {
		struct noct_beui_image_entry *next = entry->next;

		free(entry);
		entry = next;
	}
	memset(&state, 0, sizeof(state));
}

static int
noct_beui_is_open(void)
{
	return state.display_open;
}

static int
noct_beui_get_display_info(struct noct_beui_display_info *info)
{
	if (!state.display_open || info == NULL)
		return 0;
	*info = state.display;
	return 1;
}

static int
noct_beui_fill(const struct noct_beui_rect *rect, uint32_t color)
{
	if (!state.display_open || rect == NULL || rect->width == 0 ||
	    rect->height == 0 || rect->x >= state.display.width ||
	    rect->y >= state.display.height ||
	    rect->width > state.display.width - rect->x ||
	    rect->height > state.display.height - rect->y ||
	    state.hal->display.fill == NULL)
		return 0;
	return state.hal->display.fill(state.hal->display.context, rect, color);
}

static int
noct_beui_line(unsigned x0, unsigned y0, unsigned x1, unsigned y1,
		 uint32_t color)
{
	if (!state.display_open || x0 >= state.display.width ||
	    x1 >= state.display.width || y0 >= state.display.height ||
	    y1 >= state.display.height || state.hal->display.line == NULL)
		return 0;
	return state.hal->display.line(state.hal->display.context, x0, y0, x1,
				       y1, color);
}

static int
noct_beui_pattern_fill(const struct noct_beui_rect *rect, uint32_t color,
			 uint64_t pattern)
{
	if (!state.display_open || rect == NULL || rect->width == 0 ||
	    rect->height == 0 || rect->x >= state.display.width ||
	    rect->y >= state.display.height ||
	    rect->width > state.display.width - rect->x ||
	    rect->height > state.display.height - rect->y ||
	    state.hal->display.pattern_fill == NULL)
		return 0;
	return state.hal->display.pattern_fill(state.hal->display.context, rect,
					       color, pattern);
}

static int
image_valid(const struct noct_beui_image *image)
{
	if (image == NULL || image->pixels == NULL ||
	    image->width == 0 || image->height == 0 ||
	    (image->format != NOCT_BEUI_IMAGE_INDEX8 &&
	     image->format != NOCT_BEUI_IMAGE_RGB24) ||
	    (image->format == NOCT_BEUI_IMAGE_INDEX8 &&
	     (image->palette_size == 0 || image->palette_size > 256)))
		return 0;
	if (image->format == NOCT_BEUI_IMAGE_RGB24)
		return image->stride / 3U >= image->width;
	return image->stride >= image->width;
}

static int
noct_beui_draw_image(unsigned x, unsigned y,
		       const struct noct_beui_image *image)
{
	if (!state.display_open || !image_valid(image) ||
	    x >= state.display.width || y >= state.display.height ||
	    image->width > state.display.width - x ||
	    image->height > state.display.height - y ||
	    state.hal->display.draw_image == NULL)
		return 0;
	return state.hal->display.draw_image(state.hal->display.context, x, y,
					     image);
}

static int
noct_beui_draw_image_region(const struct noct_beui_image *image,
			      unsigned source_x, unsigned source_y,
			      unsigned width, unsigned height,
			      unsigned destination_x,
			      unsigned destination_y)
{
	struct noct_beui_image region;
	size_t pixel_size;
	size_t offset;

	if (!image_valid(image) || width == 0 || height == 0 ||
	    source_x >= image->width || source_y >= image->height ||
	    width > image->width - source_x ||
	    height > image->height - source_y ||
	    source_y > (size_t)-1 / image->stride)
		return 0;
	pixel_size = image->format == NOCT_BEUI_IMAGE_RGB24 ? 3U : 1U;
	offset = (size_t)source_y * image->stride;
	if (source_x > ((size_t)-1 - offset) / pixel_size)
		return 0;
	offset += (size_t)source_x * pixel_size;
	region = *image;
	region.width = width;
	region.height = height;
	region.pixels += offset;
	return noct_beui_draw_image(destination_x, destination_y, &region);
}

static int
noct_beui_draw_image_pattern(unsigned x, unsigned y,
			       const struct noct_beui_image *image,
			       uint64_t pattern)
{
	if (!state.display_open || !image_valid(image) ||
	    x >= state.display.width || y >= state.display.height ||
	    image->width > state.display.width - x ||
	    image->height > state.display.height - y ||
	    state.hal->display.draw_image_pattern == NULL)
		return 0;
	return state.hal->display.draw_image_pattern(
		state.hal->display.context, x, y, image, pattern);
}

static uint32_t
decode_utf8(const char **cursor)
{
	const unsigned char *text = (const unsigned char *)*cursor;
	uint32_t codepoint;
	unsigned length;
	unsigned index;

	if (text[0] < 0x80U) {
		(*cursor)++;
		return text[0];
	}
	if ((text[0] & 0xe0U) == 0xc0U) {
		codepoint = text[0] & 0x1fU;
		length = 2;
	} else if ((text[0] & 0xf0U) == 0xe0U) {
		codepoint = text[0] & 0x0fU;
		length = 3;
	} else if ((text[0] & 0xf8U) == 0xf0U) {
		codepoint = text[0] & 0x07U;
		length = 4;
	} else {
		(*cursor)++;
		return 0xfffdU;
	}
	for (index = 1; index < length; index++) {
		if ((text[index] & 0xc0U) != 0x80U) {
			(*cursor)++;
			return 0xfffdU;
		}
		codepoint = (codepoint << 6) | (text[index] & 0x3fU);
	}
	if ((length == 2 && codepoint < 0x80U) ||
	    (length == 3 && codepoint < 0x800U) ||
	    (length == 4 && codepoint < 0x10000U) || codepoint > 0x10ffffU ||
	    (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
		(*cursor)++;
		return 0xfffdU;
	}
	*cursor += length;
	return codepoint;
}

static int
noct_beui_measure_text(const char *text, unsigned *width, unsigned *height)
{
	const char *cursor = text;
	unsigned line_width = 0;
	unsigned maximum_width = 0;
	unsigned total_height = 16;

	if (!state.display_open || text == NULL || width == NULL ||
	    height == NULL || state.hal->glyph.measure == NULL)
		return 0;
	while (*cursor != '\0') {
		uint32_t codepoint = decode_utf8(&cursor);
		unsigned glyph_width;
		unsigned glyph_height;

		if (codepoint == '\r')
			continue;
		if (codepoint == '\n') {
			if (line_width > maximum_width)
				maximum_width = line_width;
			line_width = 0;
			if (total_height > (unsigned)-1 - 16U)
				return 0;
			total_height += 16U;
			continue;
		}
		if (!state.hal->glyph.measure(state.hal->glyph.context, codepoint,
			&glyph_width, &glyph_height) || glyph_height > 16U ||
		    line_width > (unsigned)-1 - glyph_width)
			return 0;
		line_width += glyph_width;
	}
	if (line_width > maximum_width)
		maximum_width = line_width;
	*width = maximum_width;
	*height = total_height;
	return 1;
}

static int
noct_beui_draw_text(const char *text, unsigned x, unsigned y,
	uint32_t foreground, uint32_t background)
{
	const char *cursor = text;
	unsigned origin_x = x;
	unsigned width;
	unsigned height;

	if (!noct_beui_measure_text(text, &width, &height) ||
	    x > state.display.width || y > state.display.height ||
	    width > state.display.width - x ||
	    height > state.display.height - y || state.hal->glyph.draw == NULL)
		return 0;
	while (*cursor != '\0') {
		uint32_t codepoint = decode_utf8(&cursor);
		unsigned glyph_width;
		unsigned glyph_height;

		if (codepoint == '\r')
			continue;
		if (codepoint == '\n') {
			x = origin_x;
			y += 16U;
			continue;
		}
		if (!state.hal->glyph.measure(state.hal->glyph.context, codepoint,
			&glyph_width, &glyph_height) ||
		    !state.hal->glyph.draw(state.hal->glyph.context, x, y,
			codepoint, foreground, background))
			return 0;
		x += glyph_width;
	}
	return 1;
}

static int
noct_beui_poll(void)
{
	struct noct_beui_pointer_event event;

	if (!state.display_open || state.close_requested)
		return 0;
	if (state.hal->display.poll_events != NULL &&
	    state.hal->display.poll_events(state.hal->display.context) != 1) {
		/* A closed window is sticky: every later poll reports it, so a
		 * script loop ends on the iteration after the user closes it. */
		state.close_requested = 1;
		return 0;
	}
	noct_beui_drain_input();
	if (state.pointer_open && state.hal->pointer.poll != NULL) {
		int updated;

		memset(&event, 0, sizeof(event));
		updated = state.hal->pointer.poll(state.hal->pointer.context,
						  &event);
		if (updated < 0) {
			state.close_requested = 1;
			return 0;
		}
		if (updated > 0) {
			state.pointer_x = event.x < state.display.width ?
				event.x : state.display.width - 1U;
			state.pointer_y = event.y < state.display.height ?
				event.y : state.display.height - 1U;
			state.pointer_buttons = event.buttons;
		}
	}
	if (state.audio_open && state.hal->audio.poll != NULL &&
	    !state.hal->audio.poll(state.hal->audio.context)) {
		state.close_requested = 1;
		return 0;
	}
	return 1;
}

static int
noct_beui_get_pointer(unsigned *x, unsigned *y, unsigned *buttons)
{
	if (!state.display_open || !state.pointer_open)
		return 0;
	if (x != NULL)
		*x = state.pointer_x;
	if (y != NULL)
		*y = state.pointer_y;
	if (buttons != NULL)
		*buttons = state.pointer_buttons;
	return 1;
}

static int
noct_beui_flush(void)
{
	if (!state.display_open)
		return 0;
	if (state.hal->display.flush == NULL)
		return 1;
	return state.hal->display.flush(state.hal->display.context, NULL, 0);
}

static int
noct_beui_get_milliseconds(uint64_t *milliseconds)
{
	if (milliseconds == NULL || state.hal == NULL ||
	    state.hal->clock.milliseconds == NULL)
		return 0;
	*milliseconds = state.hal->clock.milliseconds(state.hal->clock.context);
	return 1;
}

static int
noct_beui_sleep(unsigned milliseconds)
{
	uint64_t start;
	uint64_t now;

	if (!noct_beui_get_milliseconds(&start))
		return 0;
	/* The busy loop doubles as the clock poll and keeps the pointer,
	 * audio, and type-ahead backends serviced while the script idles. */
	do {
		noct_beui_drain_input();
		/* A window closed mid-sleep ends the wait; the script sees it
		 * on its next BeUI.poll(). */
		if (state.display_open && !noct_beui_poll())
			break;
		if (!noct_beui_get_milliseconds(&now))
			return 0;
	} while (now - start < milliseconds);
	return 1;
}

static int
noct_beui_is_key_down(int key)
{
	if (state.hal == NULL || state.hal->input.is_key_down == NULL)
		return -1;
	return state.hal->input.is_key_down(state.hal->input.context, key);
}

static void
noct_beui_drain_input(void)
{
	if (state.hal != NULL && state.hal->input.drain != NULL)
		state.hal->input.drain(state.hal->input.context);
}

static int
noct_beui_image_load_bmp(const void *data, size_t size)
{
	struct noct_beui_image_entry *entry;
	enum noct_beui_image_format format;
	unsigned width;
	unsigned height;
	size_t pixel_size;

	if (data == NULL || size == 0 || size > NOCT_BEUI_IMAGE_SOURCE_MAX ||
	    !noct_beui_bmp_measure(data, size, &format, &width, &height,
				   &pixel_size) ||
	    pixel_size == 0 || pixel_size > NOCT_BEUI_IMAGE_PIXELS_MAX)
		return 0;
	entry = malloc(offsetof(struct noct_beui_image_entry, pixels) +
		       pixel_size);
	if (entry == NULL)
		return 0;
	if (!noct_beui_bmp_decode(data, size, entry->pixels, pixel_size,
				  &entry->image)) {
		free(entry);
		return 0;
	}
	if (state.next_image_handle <= 0)
		state.next_image_handle = 1;
	entry->handle = state.next_image_handle++;
	entry->next = state.images;
	state.images = entry;
	return entry->handle;
}

static const struct noct_beui_image *
noct_beui_image_get(int handle)
{
	struct noct_beui_image_entry *entry;

	if (handle <= 0)
		return NULL;
	for (entry = state.images; entry != NULL; entry = entry->next)
		if (entry->handle == handle)
			return &entry->image;
	return NULL;
}

static int
noct_beui_image_destroy(int handle)
{
	struct noct_beui_image_entry **link;

	if (handle <= 0)
		return 0;
	for (link = &state.images; *link != NULL; link = &(*link)->next) {
		struct noct_beui_image_entry *entry = *link;

		if (entry->handle != handle)
			continue;
		*link = entry->next;
		free(entry);
		return 1;
	}
	return 0;
}

/* Platform-private BMP decoder. */
struct bmp_layout {
	const uint8_t *bytes;
	size_t size;
	size_t data_offset;
	size_t source_stride;
	size_t output_stride;
	size_t output_size;
	size_t palette_offset;
	unsigned palette_size;
	unsigned width;
	unsigned height;
	unsigned bits_per_pixel;
	int top_down;
	enum noct_beui_image_format format;
};

static uint16_t
read_u16(const uint8_t *bytes)
{
	return (uint16_t)(bytes[0] | (uint16_t)bytes[1] << 8);
}

static uint32_t
read_u32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 |
	       (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int32_t
read_s32(const uint8_t *bytes)
{
	return (int32_t)read_u32(bytes);
}

static int
add_overflows(size_t left, size_t right)
{
	return left > SIZE_MAX - right;
}

static int
multiply_overflows(size_t left, size_t right)
{
	return left != 0 && right > SIZE_MAX / left;
}

static int
parse_layout(const void *data, size_t size, struct bmp_layout *layout)
{
	const uint8_t *bytes = data;
	uint32_t dib_size;
	uint32_t data_offset;
	uint32_t colors_used;
	int32_t signed_width;
	int32_t signed_height;
	size_t row_bits;
	size_t palette_end;
	size_t source_bytes;
	unsigned bytes_per_pixel;

	if (bytes == NULL || layout == NULL || size < 54U || bytes[0] != 'B' ||
	    bytes[1] != 'M')
		return 0;
	dib_size = read_u32(bytes + 14);
	data_offset = read_u32(bytes + 10);
	if (dib_size < 40U || add_overflows(14U, dib_size) ||
	    14U + dib_size > size || data_offset > size)
		return 0;
	signed_width = read_s32(bytes + 18);
	signed_height = read_s32(bytes + 22);
	if (signed_width <= 0 || signed_height == 0 || signed_height == INT32_MIN ||
	    read_u16(bytes + 26) != 1U || read_u32(bytes + 30) != 0U)
		return 0;
	memset(layout, 0, sizeof(*layout));
	layout->bytes = bytes;
	layout->size = size;
	layout->data_offset = data_offset;
	layout->width = (unsigned)signed_width;
	layout->height = signed_height < 0 ? (unsigned)-signed_height :
		(unsigned)signed_height;
	layout->top_down = signed_height < 0;
	layout->bits_per_pixel = read_u16(bytes + 28);
	switch (layout->bits_per_pixel) {
	case 1:
	case 4:
	case 8:
		layout->format = NOCT_BEUI_IMAGE_INDEX8;
		bytes_per_pixel = 1;
		colors_used = read_u32(bytes + 46);
		layout->palette_size = colors_used != 0 ? colors_used :
			1U << layout->bits_per_pixel;
		if (layout->palette_size == 0 || layout->palette_size > 256U)
			return 0;
		layout->palette_offset = 14U + dib_size;
		if (multiply_overflows(layout->palette_size, 4U) ||
		    add_overflows(layout->palette_offset,
				  (size_t)layout->palette_size * 4U))
			return 0;
		palette_end = layout->palette_offset +
			(size_t)layout->palette_size * 4U;
		if (palette_end > data_offset || palette_end > size)
			return 0;
		break;
	case 24:
		layout->format = NOCT_BEUI_IMAGE_RGB24;
		bytes_per_pixel = 3;
		break;
	default:
		return 0;
	}
	if (multiply_overflows(layout->width, layout->bits_per_pixel))
		return 0;
	row_bits = (size_t)layout->width * layout->bits_per_pixel;
	if (add_overflows(row_bits, 31U))
		return 0;
	layout->source_stride = ((row_bits + 31U) / 32U) * 4U;
	if (multiply_overflows(layout->width, bytes_per_pixel))
		return 0;
	layout->output_stride = (size_t)layout->width * bytes_per_pixel;
	if (multiply_overflows(layout->source_stride, layout->height) ||
	    multiply_overflows(layout->output_stride, layout->height))
		return 0;
	source_bytes = layout->source_stride * layout->height;
	layout->output_size = layout->output_stride * layout->height;
	if (add_overflows(data_offset, source_bytes) ||
	    data_offset + source_bytes > size)
		return 0;
	return 1;
}

static int
noct_beui_bmp_measure(const void *data, size_t size,
		       enum noct_beui_image_format *format,
		       unsigned *width, unsigned *height, size_t *pixel_bytes)
{
	struct bmp_layout layout;

	if (format == NULL || width == NULL || height == NULL ||
	    pixel_bytes == NULL || !parse_layout(data, size, &layout))
		return 0;
	*format = layout.format;
	*width = layout.width;
	*height = layout.height;
	*pixel_bytes = layout.output_size;
	return 1;
}

static int
noct_beui_bmp_decode(const void *data, size_t size, void *pixel_storage,
		      size_t pixel_capacity, struct noct_beui_image *image)
{
	struct bmp_layout layout;
	uint8_t *output = pixel_storage;
	unsigned y;

	if (output == NULL || image == NULL ||
	    !parse_layout(data, size, &layout) ||
	    pixel_capacity < layout.output_size)
		return 0;
	memset(image, 0, sizeof(*image));
	image->format = layout.format;
	image->width = layout.width;
	image->height = layout.height;
	image->stride = layout.output_stride;
	image->pixels = output;
	image->palette_size = layout.palette_size;
	for (y = 0; y < layout.palette_size; y++) {
		const uint8_t *entry = layout.bytes + layout.palette_offset +
			(size_t)y * 4U;

		image->palette[y] = (uint32_t)entry[2] << 16 |
			(uint32_t)entry[1] << 8 | entry[0];
	}
	for (y = 0; y < layout.height; y++) {
		unsigned source_y = layout.top_down ? y : layout.height - 1U - y;
		const uint8_t *source = layout.bytes + layout.data_offset +
			(size_t)source_y * layout.source_stride;
		uint8_t *destination = output + (size_t)y * layout.output_stride;
		unsigned x;

		if (layout.bits_per_pixel == 1U) {
			for (x = 0; x < layout.width; x++)
				destination[x] = (uint8_t)(
					(source[x >> 3] >> (7U - (x & 7U))) & 1U);
		} else if (layout.bits_per_pixel == 4U) {
			for (x = 0; x < layout.width; x++)
				destination[x] = (uint8_t)(
					(source[x >> 1] >> ((x & 1U) ? 0U : 4U)) &
					0x0fU);
		} else if (layout.bits_per_pixel == 8U) {
			memcpy(destination, source, layout.width);
		} else {
			for (x = 0; x < layout.width; x++) {
				destination[(size_t)x * 3U] = source[(size_t)x * 3U + 2U];
				destination[(size_t)x * 3U + 1U] = source[(size_t)x * 3U + 1U];
				destination[(size_t)x * 3U + 2U] = source[(size_t)x * 3U];
			}
		}
	}
	return 1;
}

/*
 * Complete BeUI language binding for this platform.  It is intentionally
 * owned by this source rather than shared through a backend dispatcher.
 */

static bool cfunc_BeUI_init(NoctEnv *env);
static bool cfunc_BeUI_initWithHint(NoctEnv *env);
static bool cfunc_BeUI_close(NoctEnv *env);
static bool cfunc_BeUI_isOpen(NoctEnv *env);
static bool cfunc_BeUI_getWidth(NoctEnv *env);
static bool cfunc_BeUI_getHeight(NoctEnv *env);
static bool cfunc_BeUI_poll(NoctEnv *env);
static bool cfunc_BeUI_flush(NoctEnv *env);
static bool cfunc_BeUI_fill(NoctEnv *env);
static bool cfunc_BeUI_line(NoctEnv *env);
static bool cfunc_BeUI_patternFill(NoctEnv *env);
static bool cfunc_BeUI_textWidth(NoctEnv *env);
static bool cfunc_BeUI_textHeight(NoctEnv *env);
static bool cfunc_BeUI_drawText(NoctEnv *env);
static bool cfunc_BeUI_getMilliseconds(NoctEnv *env);
static bool cfunc_BeUI_sleep(NoctEnv *env);
static bool cfunc_BeUI_isKeyDown(NoctEnv *env);
static bool cfunc_BeUI_getPointerX(NoctEnv *env);
static bool cfunc_BeUI_getPointerY(NoctEnv *env);
static bool cfunc_BeUI_getPointerButtons(NoctEnv *env);
static bool cfunc_BeUI_loadImage(NoctEnv *env);
static bool cfunc_BeUI_getImageWidth(NoctEnv *env);
static bool cfunc_BeUI_getImageHeight(NoctEnv *env);
static bool cfunc_BeUI_drawImage(NoctEnv *env);
static bool cfunc_BeUI_drawImageRegion(NoctEnv *env);
static bool cfunc_BeUI_drawImagePattern(NoctEnv *env);
static bool cfunc_BeUI_destroyImage(NoctEnv *env);

struct beui_ffi_item {
	const char *global_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};

static struct beui_ffi_item beui_ffi_items[] = {
	{"BeUI.init", "init", 0, {NULL}, cfunc_BeUI_init},
	{"BeUI.initWithHint", "initWithHint", 1, {"bitsPerPixel"},
	 cfunc_BeUI_initWithHint},
	{"BeUI.close", "close", 0, {NULL}, cfunc_BeUI_close},
	{"BeUI.isOpen", "isOpen", 0, {NULL}, cfunc_BeUI_isOpen},
	{"BeUI.getWidth", "getWidth", 0, {NULL}, cfunc_BeUI_getWidth},
	{"BeUI.getHeight", "getHeight", 0, {NULL}, cfunc_BeUI_getHeight},
	{"BeUI.poll", "poll", 0, {NULL}, cfunc_BeUI_poll},
	{"BeUI.flush", "flush", 0, {NULL}, cfunc_BeUI_flush},
	{"BeUI.fill", "fill", 5, {"x", "y", "width", "height", "color"},
	 cfunc_BeUI_fill},
	{"BeUI.line", "line", 5, {"x0", "y0", "x1", "y1", "color"},
	 cfunc_BeUI_line},
	{"BeUI.patternFill", "patternFill", 6,
	 {"x", "y", "width", "height", "color", "pattern"},
	 cfunc_BeUI_patternFill},
	{"BeUI.textWidth", "textWidth", 1, {"text"}, cfunc_BeUI_textWidth},
	{"BeUI.textHeight", "textHeight", 1, {"text"}, cfunc_BeUI_textHeight},
	{"BeUI.drawText", "drawText", 5,
	 {"text", "x", "y", "foreground", "background"}, cfunc_BeUI_drawText},
	{"BeUI.getMilliseconds", "getMilliseconds", 0, {NULL},
	 cfunc_BeUI_getMilliseconds},
	{"BeUI.sleep", "sleep", 1, {"milliseconds"}, cfunc_BeUI_sleep},
	{"BeUI.isKeyDown", "isKeyDown", 1, {"key"}, cfunc_BeUI_isKeyDown},
	{"BeUI.getPointerX", "getPointerX", 0, {NULL},
	 cfunc_BeUI_getPointerX},
	{"BeUI.getPointerY", "getPointerY", 0, {NULL},
	 cfunc_BeUI_getPointerY},
	{"BeUI.getPointerButtons", "getPointerButtons", 0, {NULL},
	 cfunc_BeUI_getPointerButtons},
	{"BeUI.loadImage", "loadImage", 1, {"bytes"}, cfunc_BeUI_loadImage},
	{"BeUI.getImageWidth", "getImageWidth", 1, {"image"},
	 cfunc_BeUI_getImageWidth},
	{"BeUI.getImageHeight", "getImageHeight", 1, {"image"},
	 cfunc_BeUI_getImageHeight},
	{"BeUI.drawImage", "drawImage", 3, {"image", "x", "y"},
	 cfunc_BeUI_drawImage},
	{"BeUI.drawImageRegion", "drawImageRegion", 7,
	 {"image", "sourceX", "sourceY", "width", "height", "x", "y"},
	 cfunc_BeUI_drawImageRegion},
	{"BeUI.drawImagePattern", "drawImagePattern", 4,
	 {"image", "x", "y", "pattern"}, cfunc_BeUI_drawImagePattern},
	{"BeUI.destroyImage", "destroyImage", 1, {"image"},
	 cfunc_BeUI_destroyImage},
};

struct beui_int_constant {
	const char *name;
	int value;
};

/* Key names shared with the Boots BeUI implementation. */
static const struct beui_int_constant beui_keys[] = {
	{"Escape", NOCT_BEUI_KEY_ESCAPE},
	{"Tab", NOCT_BEUI_KEY_TAB},
	{"Enter", NOCT_BEUI_KEY_ENTER},
	{"Backspace", NOCT_BEUI_KEY_BACKSPACE},
	{"Delete", NOCT_BEUI_KEY_DELETE},
	{"Insert", NOCT_BEUI_KEY_INSERT},
	{"Up", NOCT_BEUI_KEY_UP},
	{"Down", NOCT_BEUI_KEY_DOWN},
	{"Left", NOCT_BEUI_KEY_LEFT},
	{"Right", NOCT_BEUI_KEY_RIGHT},
	{"Home", NOCT_BEUI_KEY_HOME},
	{"End", NOCT_BEUI_KEY_END},
	{"PageUp", NOCT_BEUI_KEY_PAGE_UP},
	{"PageDown", NOCT_BEUI_KEY_PAGE_DOWN},
	{"F1", NOCT_BEUI_KEY_F1}, {"F2", NOCT_BEUI_KEY_F2},
	{"F3", NOCT_BEUI_KEY_F3}, {"F4", NOCT_BEUI_KEY_F4},
	{"F5", NOCT_BEUI_KEY_F5}, {"F6", NOCT_BEUI_KEY_F6},
	{"F7", NOCT_BEUI_KEY_F7}, {"F8", NOCT_BEUI_KEY_F8},
	{"F9", NOCT_BEUI_KEY_F9}, {"F10", NOCT_BEUI_KEY_F10},
	{"Space", ' '},
	{"Shift", NOCT_BEUI_KEY_SHIFT},
};

/* Bit values returned by BeUI.getPointerButtons. */
static const struct beui_int_constant beui_buttons[] = {
	{"Left", NOCT_BEUI_BUTTON_LEFT},
	{"Right", NOCT_BEUI_BUTTON_RIGHT},
	{"Middle", NOCT_BEUI_BUTTON_MIDDLE},
};

static bool
return_int(NoctEnv *env, int value)
{
	NoctValue result;
	bool ok;

	memset(&result, 0, sizeof(result));
	if (!noct_pin_local(env, 1, &result))
		return false;
	ok = noct_set_return_make_int(env, &result, value);
	(void)noct_unpin_local(env, 1, &result);
	return ok;
}

static bool
get_int_arg(NoctEnv *env, uint32_t index, int *result)
{
	NoctValue value;
	int64_t long_value;
	bool ok;

	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	ok = noct_get_arg_check_long(env, index, &value, &long_value);
	if (!ok) {
		int int_value;

		ok = noct_get_arg_check_int(env, index, &value, &int_value);
		if (ok)
			long_value = int_value;
	}
	if (ok)
		*result = (int)long_value;
	(void)noct_unpin_local(env, 1, &value);
	return ok;
}

static bool
cfunc_BeUI_init(NoctEnv *env)
{
	return return_int(env, noct_beui_init() ? 1 : 0);
}

static bool
cfunc_BeUI_initWithHint(NoctEnv *env)
{
	int bits_per_pixel;

	if (!get_int_arg(env, 0, &bits_per_pixel) ||
	    (bits_per_pixel != 8 && bits_per_pixel != 24)) {
		noct_error(env,
			   "BeUI.initWithHint expects 8 or 24 bits per pixel.");
		return false;
	}
	return return_int(env,
		noct_beui_init_with_hint((unsigned)bits_per_pixel) ? 1 : 0);
}

static bool
cfunc_BeUI_close(NoctEnv *env)
{
	noct_beui_close();
	return return_int(env, 1);
}

static bool
cfunc_BeUI_isOpen(NoctEnv *env)
{
	return return_int(env, noct_beui_is_open() ? 1 : 0);
}

static bool
cfunc_BeUI_getWidth(NoctEnv *env)
{
	struct noct_beui_display_info info;

	if (!noct_beui_get_display_info(&info)) {
		noct_error(env, "BeUI is not open.");
		return false;
	}
	return return_int(env, (int)info.width);
}

static bool
cfunc_BeUI_getHeight(NoctEnv *env)
{
	struct noct_beui_display_info info;

	if (!noct_beui_get_display_info(&info)) {
		noct_error(env, "BeUI is not open.");
		return false;
	}
	return return_int(env, (int)info.height);
}

/*
 * Returns 1 while the display is alive and 0 once it has closed, so the
 * canonical loop is "while (BeUI.poll()) { ... }".  Targets that own the
 * whole machine never return 0.
 */
static bool
cfunc_BeUI_poll(NoctEnv *env)
{
	return return_int(env, noct_beui_poll() ? 1 : 0);
}

static bool
cfunc_BeUI_flush(NoctEnv *env)
{
	if (!noct_beui_flush()) {
		noct_error(env, "BeUI.flush failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_fill(NoctEnv *env)
{
	struct noct_beui_rect rectangle;
	int x, y, width, height, color;

	if (!get_int_arg(env, 0, &x) || !get_int_arg(env, 1, &y) ||
	    !get_int_arg(env, 2, &width) || !get_int_arg(env, 3, &height) ||
	    !get_int_arg(env, 4, &color) || x < 0 || y < 0 || width <= 0 ||
	    height <= 0 || color < 0 || color > 0xffffff) {
		noct_error(env, "BeUI.fill received an invalid argument.");
		return false;
	}
	rectangle.x = (unsigned)x;
	rectangle.y = (unsigned)y;
	rectangle.width = (unsigned)width;
	rectangle.height = (unsigned)height;
	if (!noct_beui_fill(&rectangle, (uint32_t)color)) {
		noct_error(env, "BeUI.fill failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_line(NoctEnv *env)
{
	int x0, y0, x1, y1, color;

	if (!get_int_arg(env, 0, &x0) || !get_int_arg(env, 1, &y0) ||
	    !get_int_arg(env, 2, &x1) || !get_int_arg(env, 3, &y1) ||
	    !get_int_arg(env, 4, &color) || x0 < 0 || y0 < 0 || x1 < 0 ||
	    y1 < 0 || color < 0 || color > 0xffffff) {
		noct_error(env, "BeUI.line received an invalid argument.");
		return false;
	}
	if (!noct_beui_line((unsigned)x0, (unsigned)y0, (unsigned)x1,
			    (unsigned)y1, (uint32_t)color)) {
		noct_error(env, "BeUI.line failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_patternFill(NoctEnv *env)
{
	struct noct_beui_rect rectangle;
	NoctValue value;
	int x, y, width, height, color;
	int64_t pattern;
	bool ok;

	if (!get_int_arg(env, 0, &x) || !get_int_arg(env, 1, &y) ||
	    !get_int_arg(env, 2, &width) || !get_int_arg(env, 3, &height) ||
	    !get_int_arg(env, 4, &color) || x < 0 || y < 0 || width <= 0 ||
	    height <= 0 || color < 0 || color > 0xffffff) {
		noct_error(env,
			   "BeUI.patternFill received an invalid argument.");
		return false;
	}
	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	ok = noct_get_arg_check_long(env, 5, &value, &pattern);
	if (!ok) {
		int int_pattern;

		ok = noct_get_arg_check_int(env, 5, &value, &int_pattern);
		if (ok)
			pattern = (int64_t)(uint32_t)int_pattern;
	}
	(void)noct_unpin_local(env, 1, &value);
	if (!ok) {
		noct_error(env,
			   "BeUI.patternFill received an invalid argument.");
		return false;
	}
	rectangle.x = (unsigned)x;
	rectangle.y = (unsigned)y;
	rectangle.width = (unsigned)width;
	rectangle.height = (unsigned)height;
	if (!noct_beui_pattern_fill(&rectangle, (uint32_t)color,
				    (uint64_t)pattern)) {
		noct_error(env, "BeUI.patternFill failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
measure_text_arg(NoctEnv *env, const char *api, unsigned *width,
		 unsigned *height)
{
	NoctValue value;
	const char *text;
	bool ok = false;

	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	if (!noct_get_arg_check_string(env, 0, &value, &text) ||
	    !noct_beui_measure_text(text, width, height)) {
		noct_error(env, "%s failed.", api);
		goto cleanup;
	}
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 1, &value);
	return ok;
}

static bool
cfunc_BeUI_textWidth(NoctEnv *env)
{
	unsigned width, height;

	if (!measure_text_arg(env, "BeUI.textWidth", &width, &height))
		return false;
	return return_int(env, (int)width);
}

static bool
cfunc_BeUI_textHeight(NoctEnv *env)
{
	unsigned width, height;

	if (!measure_text_arg(env, "BeUI.textHeight", &width, &height))
		return false;
	return return_int(env, (int)height);
}

static bool
cfunc_BeUI_drawText(NoctEnv *env)
{
	NoctValue value;
	const char *text;
	int x, y, foreground, background;
	bool ok = false;

	if (!get_int_arg(env, 1, &x) || !get_int_arg(env, 2, &y) ||
	    !get_int_arg(env, 3, &foreground) ||
	    !get_int_arg(env, 4, &background) || x < 0 || y < 0 ||
	    foreground < 0 || foreground > 0xffffff || background < 0 ||
	    background > 0xffffff) {
		noct_error(env, "BeUI.drawText received an invalid argument.");
		return false;
	}
	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	if (!noct_get_arg_check_string(env, 0, &value, &text) ||
	    !noct_beui_draw_text(text, (unsigned)x, (unsigned)y,
				 (uint32_t)foreground,
				 (uint32_t)background)) {
		noct_error(env, "BeUI.drawText failed.");
		goto cleanup;
	}
	ok = return_int(env, 1);
cleanup:
	(void)noct_unpin_local(env, 1, &value);
	return ok;
}

static bool
cfunc_BeUI_getMilliseconds(NoctEnv *env)
{
	uint64_t milliseconds;

	if (!noct_beui_get_milliseconds(&milliseconds)) {
		noct_error(env, "BeUI.getMilliseconds is unavailable.");
		return false;
	}
	return return_int(env, (int)(milliseconds & 0x7fffffffu));
}

static bool
cfunc_BeUI_sleep(NoctEnv *env)
{
	int milliseconds;

	if (!get_int_arg(env, 0, &milliseconds) || milliseconds < 0 ||
	    milliseconds > 3600000) {
		noct_error(env, "BeUI.sleep received an invalid argument.");
		return false;
	}
	if (!noct_beui_sleep((unsigned)milliseconds)) {
		noct_error(env, "BeUI.sleep is unavailable.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_isKeyDown(NoctEnv *env)
{
	int key;

	if (!get_int_arg(env, 0, &key) || key < 0) {
		noct_error(env, "BeUI.isKeyDown received an invalid argument.");
		return false;
	}
	/* Keys the target cannot sense read as released. */
	return return_int(env, noct_beui_is_key_down(key) == 1);
}

static bool
pointer_field(NoctEnv *env, const char *api, unsigned *x, unsigned *y,
	      unsigned *buttons)
{
	if (!noct_beui_get_pointer(x, y, buttons)) {
		noct_error(env, "%s is unavailable.", api);
		return false;
	}
	return true;
}

static bool
cfunc_BeUI_getPointerX(NoctEnv *env)
{
	unsigned x;

	if (!pointer_field(env, "BeUI.getPointerX", &x, NULL, NULL))
		return false;
	return return_int(env, (int)x);
}

static bool
cfunc_BeUI_getPointerY(NoctEnv *env)
{
	unsigned y;

	if (!pointer_field(env, "BeUI.getPointerY", NULL, &y, NULL))
		return false;
	return return_int(env, (int)y);
}

static bool
cfunc_BeUI_getPointerButtons(NoctEnv *env)
{
	unsigned buttons;

	if (!pointer_field(env, "BeUI.getPointerButtons", NULL, NULL, &buttons))
		return false;
	return return_int(env, (int)buttons);
}

/*
 * BeUI.loadImage takes the file contents rather than a path: BeUI draws
 * and the File API reads, so the graphical layer needs no filesystem of
 * its own and behaves identically on every host.
 */
static bool
cfunc_BeUI_loadImage(NoctEnv *env)
{
	NoctValue value;
	void *data;
	size_t size;
	int handle;
	bool ok = false;

	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	if (!noct_get_arg_check_packed(env, 0, &value, NOCT_PACKED_UINT8) ||
	    !noct_get_packed_size(env, &value, &size) ||
	    !noct_get_packed_pointer(env, &value, &data)) {
		noct_error(env, "BeUI.loadImage expects a byte array.");
		goto cleanup;
	}
	handle = noct_beui_image_load_bmp(data, size);
	if (handle == 0) {
		noct_error(env, "BeUI.loadImage received an unsupported image.");
		goto cleanup;
	}
	ok = return_int(env, handle);
cleanup:
	(void)noct_unpin_local(env, 1, &value);
	return ok;
}

static bool
cfunc_BeUI_getImageWidth(NoctEnv *env)
{
	const struct noct_beui_image *image;
	int handle;

	if (!get_int_arg(env, 0, &handle) ||
	    (image = noct_beui_image_get(handle)) == NULL) {
		noct_error(env,
			   "BeUI.getImageWidth received an invalid handle.");
		return false;
	}
	return return_int(env, (int)image->width);
}

static bool
cfunc_BeUI_getImageHeight(NoctEnv *env)
{
	const struct noct_beui_image *image;
	int handle;

	if (!get_int_arg(env, 0, &handle) ||
	    (image = noct_beui_image_get(handle)) == NULL) {
		noct_error(env,
			   "BeUI.getImageHeight received an invalid handle.");
		return false;
	}
	return return_int(env, (int)image->height);
}

static bool
cfunc_BeUI_drawImage(NoctEnv *env)
{
	const struct noct_beui_image *image;
	int handle, x, y;

	if (!get_int_arg(env, 0, &handle) || !get_int_arg(env, 1, &x) ||
	    !get_int_arg(env, 2, &y) || x < 0 || y < 0 ||
	    (image = noct_beui_image_get(handle)) == NULL ||
	    !noct_beui_draw_image((unsigned)x, (unsigned)y, image)) {
		noct_error(env, "BeUI.drawImage failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_drawImageRegion(NoctEnv *env)
{
	const struct noct_beui_image *image;
	int handle, source_x, source_y, width, height, x, y;

	if (!get_int_arg(env, 0, &handle) ||
	    !get_int_arg(env, 1, &source_x) ||
	    !get_int_arg(env, 2, &source_y) ||
	    !get_int_arg(env, 3, &width) ||
	    !get_int_arg(env, 4, &height) || !get_int_arg(env, 5, &x) ||
	    !get_int_arg(env, 6, &y) || source_x < 0 || source_y < 0 ||
	    width <= 0 || height <= 0 || x < 0 || y < 0 ||
	    (image = noct_beui_image_get(handle)) == NULL ||
	    !noct_beui_draw_image_region(image, (unsigned)source_x,
					 (unsigned)source_y, (unsigned)width,
					 (unsigned)height, (unsigned)x,
					 (unsigned)y)) {
		noct_error(env, "BeUI.drawImageRegion failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_drawImagePattern(NoctEnv *env)
{
	const struct noct_beui_image *image;
	NoctValue value;
	int handle, x, y;
	int64_t pattern;
	bool ok;

	if (!get_int_arg(env, 0, &handle) || !get_int_arg(env, 1, &x) ||
	    !get_int_arg(env, 2, &y) || x < 0 || y < 0) {
		noct_error(env,
			   "BeUI.drawImagePattern received an invalid argument.");
		return false;
	}
	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	ok = noct_get_arg_check_long(env, 3, &value, &pattern);
	if (!ok) {
		int int_pattern;

		ok = noct_get_arg_check_int(env, 3, &value, &int_pattern);
		if (ok)
			pattern = (int64_t)(uint32_t)int_pattern;
	}
	(void)noct_unpin_local(env, 1, &value);
	if (!ok || (image = noct_beui_image_get(handle)) == NULL ||
	    !noct_beui_draw_image_pattern((unsigned)x, (unsigned)y, image,
					  (uint64_t)pattern)) {
		noct_error(env, "BeUI.drawImagePattern failed.");
		return false;
	}
	return return_int(env, 1);
}

static bool
cfunc_BeUI_destroyImage(NoctEnv *env)
{
	int handle;

	if (!get_int_arg(env, 0, &handle) || !noct_beui_image_destroy(handle)) {
		noct_error(env, "BeUI.destroyImage received an invalid handle.");
		return false;
	}
	return return_int(env, 1);
}

static bool
register_int_dictionary(NoctEnv *env, const char *name,
			const struct beui_int_constant *entries, size_t count)
{
	NoctValue dictionary;
	NoctValue scratch;
	size_t index;
	bool ok = false;

	memset(&dictionary, 0, sizeof(dictionary));
	memset(&scratch, 0, sizeof(scratch));
	if (!noct_pin_local(env, 2, &dictionary, &scratch))
		return false;
	if (!noct_make_empty_dict(env, &dictionary))
		goto cleanup;
	for (index = 0; index < count; index++)
		if (!noct_set_dict_elem_make_int(env, &dictionary,
						 entries[index].name, &scratch,
						 entries[index].value))
			goto cleanup;
	if (!noct_set_global(env, name, &dictionary))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &dictionary, &scratch);
	return ok;
}


static bool
register_beui_api(NoctEnv *env, const struct noct_beui_hal *hal)
{
	NoctValue beui_dict, function;
	size_t index;
	bool ok = false;

	if (!noct_beui_bind(hal))
		return false;
	memset(&beui_dict, 0, sizeof(beui_dict));
	memset(&function, 0, sizeof(function));
	if (!noct_pin_local(env, 2, &beui_dict, &function))
		return false;
	if (!noct_make_empty_dict(env, &beui_dict) ||
	    !noct_set_global(env, "BeUI", &beui_dict))
		goto cleanup;
	for (index = 0; index < sizeof(beui_ffi_items) /
					 sizeof(beui_ffi_items[0]); index++) {
		struct beui_ffi_item *item = &beui_ffi_items[index];

		if (!noct_register_cfunc(env, item->global_name,
					 item->param_count, item->param,
					 item->cfunc, NULL) ||
		    !noct_get_global(env, item->global_name, &function) ||
		    !noct_set_dict_elem_cstr(env, &beui_dict, item->field_name,
					     &function))
			goto cleanup;
	}
	if (!register_int_dictionary(env, "Key", beui_keys,
				     sizeof(beui_keys) / sizeof(beui_keys[0])) ||
	    !register_int_dictionary(env, "Button", beui_buttons,
				     sizeof(beui_buttons) /
					     sizeof(beui_buttons[0])))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &beui_dict, &function);
	return ok;
}

#define BEUI_SDL2_WIDTH 640U
#define BEUI_SDL2_HEIGHT 400U

struct beui_sdl2_context {
	SDL_Window *window;
	SDL_Surface *framebuffer;
	SDL_AudioDeviceID audio_device;
	unsigned audio_channels;
	int video_initialized;
	int timer_initialized;
	int audio_initialized;
	int alive;
};

static struct beui_sdl2_context sdl2_context;
static struct noct_beui_hal sdl2_hal;

static uint32_t
sdl2_color(uint32_t color)
{
	return 0xff000000U | (color & 0x00ffffffU);
}

static int
sdl2_pattern_bit(uint64_t pattern, unsigned x, unsigned y)
{
	uint8_t row;

	row = (uint8_t)(pattern >> ((y & 7U) * 8U));
	return (row & (uint8_t)(0x80U >> (x & 7U))) != 0;
}

static void
sdl2_put_pixel(struct beui_sdl2_context *context, unsigned x, unsigned y,
	       uint32_t color)
{
	uint32_t *row;

	row = (uint32_t *)((uint8_t *)context->framebuffer->pixels +
			   y * (unsigned)context->framebuffer->pitch);
	row[x] = sdl2_color(color);
}

static int
sdl2_lock_framebuffer(struct beui_sdl2_context *context)
{
	if (context == NULL || context->framebuffer == NULL)
		return 0;
	return !SDL_MUSTLOCK(context->framebuffer) ||
		SDL_LockSurface(context->framebuffer) == 0;
}

static void
sdl2_unlock_framebuffer(struct beui_sdl2_context *context)
{
	if (SDL_MUSTLOCK(context->framebuffer))
		SDL_UnlockSurface(context->framebuffer);
}

static int
sdl2_enter(void *opaque, struct noct_beui_display_info *info)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL || info == NULL)
		return 0;
	if (!context->video_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
			return 0;
		context->video_initialized = 1;
	}
	context->window = SDL_CreateWindow("Noct BeUI",
					   SDL_WINDOWPOS_CENTERED,
					   SDL_WINDOWPOS_CENTERED,
					   (int)BEUI_SDL2_WIDTH,
					   (int)BEUI_SDL2_HEIGHT,
					   SDL_WINDOW_SHOWN);
	if (context->window == NULL)
		return 0;
	context->framebuffer = SDL_CreateRGBSurfaceWithFormat(
		0, (int)BEUI_SDL2_WIDTH, (int)BEUI_SDL2_HEIGHT, 32,
		SDL_PIXELFORMAT_ARGB8888);
	if (context->framebuffer == NULL) {
		SDL_DestroyWindow(context->window);
		context->window = NULL;
		return 0;
	}
	SDL_FillRect(context->framebuffer, NULL, sdl2_color(0));
	context->alive = 1;
	info->width = BEUI_SDL2_WIDTH;
	info->height = BEUI_SDL2_HEIGHT;
	info->bits_per_pixel = 32;
	info->stride = (unsigned)context->framebuffer->pitch;
	return 1;
}

static void
sdl2_audio_stop(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context != NULL && context->audio_device != 0) {
		SDL_ClearQueuedAudio(context->audio_device);
		SDL_CloseAudioDevice(context->audio_device);
		context->audio_device = 0;
		context->audio_channels = 0;
	}
}

static void
sdl2_leave(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL)
		return;
	sdl2_audio_stop(context);
	if (context->framebuffer != NULL) {
		SDL_FreeSurface(context->framebuffer);
		context->framebuffer = NULL;
	}
	if (context->window != NULL) {
		SDL_DestroyWindow(context->window);
		context->window = NULL;
	}
	context->alive = 0;
}

static int
sdl2_poll_events(void *opaque)
{
	struct beui_sdl2_context *context;
	SDL_Event event;

	context = opaque;
	if (context == NULL || context->window == NULL)
		return 0;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT ||
		    (event.type == SDL_WINDOWEVENT &&
		     event.window.windowID == SDL_GetWindowID(context->window) &&
		     event.window.event == SDL_WINDOWEVENT_CLOSE))
			context->alive = 0;
	}
	return context->alive;
}

static int
sdl2_fill(void *opaque, const struct noct_beui_rect *rect, uint32_t color)
{
	struct beui_sdl2_context *context;
	SDL_Rect destination;

	context = opaque;
	if (context == NULL || context->framebuffer == NULL || rect == NULL)
		return 0;
	destination.x = (int)rect->x;
	destination.y = (int)rect->y;
	destination.w = (int)rect->width;
	destination.h = (int)rect->height;
	return SDL_FillRect(context->framebuffer, &destination,
			    sdl2_color(color)) == 0;
}

static int
sdl2_line(void *opaque, unsigned x0, unsigned y0, unsigned x1,
	  unsigned y1, uint32_t color)
{
	struct beui_sdl2_context *context;
	int x;
	int y;
	int target_x;
	int target_y;
	int delta_x;
	int delta_y;
	int step_x;
	int step_y;
	int error;

	context = opaque;
	if (!sdl2_lock_framebuffer(context))
		return 0;
	x = (int)x0;
	y = (int)y0;
	target_x = (int)x1;
	target_y = (int)y1;
	delta_x = target_x >= x ? target_x - x : x - target_x;
	step_x = x < target_x ? 1 : -1;
	delta_y = target_y >= y ? y - target_y : target_y - y;
	step_y = y < target_y ? 1 : -1;
	error = delta_x + delta_y;
	for (;;) {
		int twice_error;

		sdl2_put_pixel(context, (unsigned)x, (unsigned)y, color);
		if (x == target_x && y == target_y)
			break;
		twice_error = error * 2;
		if (twice_error >= delta_y) {
			error += delta_y;
			x += step_x;
		}
		if (twice_error <= delta_x) {
			error += delta_x;
			y += step_y;
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_pattern_fill(void *opaque, const struct noct_beui_rect *rect,
		   uint32_t color, uint64_t pattern)
{
	struct beui_sdl2_context *context;
	unsigned x;
	unsigned y;

	context = opaque;
	if (rect == NULL || !sdl2_lock_framebuffer(context))
		return 0;
	for (y = rect->y; y < rect->y + rect->height; y++) {
		for (x = rect->x; x < rect->x + rect->width; x++) {
			if (sdl2_pattern_bit(pattern, x, y))
				sdl2_put_pixel(context, x, y, color);
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static uint32_t
sdl2_image_pixel(const struct noct_beui_image *image, unsigned x, unsigned y)
{
	const uint8_t *pixel;

	pixel = image->pixels + y * image->stride;
	if (image->format == NOCT_BEUI_IMAGE_INDEX8)
		return image->palette[pixel[x]];
	pixel += x * 3U;
	return ((uint32_t)pixel[0] << 16) | ((uint32_t)pixel[1] << 8) |
		pixel[2];
}

static int
sdl2_draw_image_common(struct beui_sdl2_context *context, unsigned x,
			unsigned y, const struct noct_beui_image *image,
			uint64_t pattern, int patterned)
{
	unsigned source_x;
	unsigned source_y;

	if (image == NULL || image->pixels == NULL ||
	    !sdl2_lock_framebuffer(context))
		return 0;
	for (source_y = 0; source_y < image->height; source_y++) {
		for (source_x = 0; source_x < image->width; source_x++) {
			if (!patterned || sdl2_pattern_bit(pattern, x + source_x,
							 y + source_y))
				sdl2_put_pixel(context, x + source_x, y + source_y,
					       sdl2_image_pixel(image, source_x,
								source_y));
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_draw_image(void *opaque, unsigned x, unsigned y,
		 const struct noct_beui_image *image)
{
	return sdl2_draw_image_common(opaque, x, y, image, 0, 0);
}

static int
sdl2_draw_image_pattern(void *opaque, unsigned x, unsigned y,
			 const struct noct_beui_image *image,
			 uint64_t pattern)
{
	return sdl2_draw_image_common(opaque, x, y, image, pattern, 1);
}

static int
sdl2_flush(void *opaque, const struct noct_beui_rect *rectangles,
	   size_t rectangle_count)
{
	struct beui_sdl2_context *context;
	SDL_Surface *window_surface;

	(void)rectangles;
	(void)rectangle_count;
	context = opaque;
	if (context == NULL || context->window == NULL ||
	    context->framebuffer == NULL)
		return 0;
	window_surface = SDL_GetWindowSurface(context->window);
	if (window_surface == NULL ||
	    SDL_BlitSurface(context->framebuffer, NULL, window_surface, NULL) != 0)
		return 0;
	return SDL_UpdateWindowSurface(context->window) == 0;
}

/* A compact built-in 5x7 desktop font.  Lowercase intentionally shares
 * uppercase shapes; non-ASCII codepoints get a visible fallback box. */
static void
sdl2_ascii_glyph(uint32_t codepoint, uint8_t rows[7])
{
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static const uint8_t glyphs[][7] = {
		{14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
		{14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
		{31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
		{14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
		{14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
		{17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
		{17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
		{14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
		{14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
		{15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
		{17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
		{17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
		{17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
		{14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
		{14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
		{2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
		{14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
		{14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
	};
	const char *found;

	memset(rows, 0, 7);
	if (codepoint >= 'a' && codepoint <= 'z')
		codepoint -= 'a' - 'A';
	found = strchr(alphabet, (int)codepoint);
	if (found != NULL) {
		memcpy(rows, glyphs[found - alphabet], 7);
		return;
	}
	switch (codepoint) {
	case '.': rows[6] = 4; break;
	case ',': rows[5] = 4; rows[6] = 8; break;
	case ':': rows[2] = 4; rows[5] = 4; break;
	case ';': rows[2] = 4; rows[5] = 4; rows[6] = 8; break;
	case '-': rows[3] = 31; break;
	case '_': rows[6] = 31; break;
	case '+': rows[2] = 4; rows[3] = 31; rows[4] = 4; break;
	case '/': rows[0] = 1; rows[1] = 2; rows[2] = 2; rows[3] = 4;
		rows[4] = 8; rows[5] = 8; rows[6] = 16; break;
	case '\\': rows[0] = 16; rows[1] = 8; rows[2] = 8; rows[3] = 4;
		rows[4] = 2; rows[5] = 2; rows[6] = 1; break;
	case '!': rows[0] = 4; rows[1] = 4; rows[2] = 4; rows[3] = 4;
		rows[5] = 4; break;
	case '?': rows[0] = 14; rows[1] = 17; rows[2] = 1; rows[3] = 2;
		rows[4] = 4; rows[6] = 4; break;
	case '(': rows[0] = 2; rows[1] = 4; rows[2] = 8; rows[3] = 8;
		rows[4] = 8; rows[5] = 4; rows[6] = 2; break;
	case ')': rows[0] = 8; rows[1] = 4; rows[2] = 2; rows[3] = 2;
		rows[4] = 2; rows[5] = 4; rows[6] = 8; break;
	case '[': rows[0] = 14; rows[1] = 8; rows[2] = 8; rows[3] = 8;
		rows[4] = 8; rows[5] = 8; rows[6] = 14; break;
	case ']': rows[0] = 14; rows[1] = 2; rows[2] = 2; rows[3] = 2;
		rows[4] = 2; rows[5] = 2; rows[6] = 14; break;
	case '=': rows[2] = 31; rows[4] = 31; break;
	case '*': rows[1] = 21; rows[2] = 14; rows[3] = 31;
		rows[4] = 14; rows[5] = 21; break;
	case ' ': break;
	default: rows[0] = 14; rows[1] = 17; rows[2] = 1; rows[3] = 2;
		rows[4] = 4; rows[6] = 4; break;
	}
}

static int
sdl2_glyph_measure(void *opaque, uint32_t codepoint, unsigned *width,
		   unsigned *height)
{
	(void)opaque;
	if (width == NULL || height == NULL)
		return 0;
	*width = codepoint < 0x80U ? 8U : 16U;
	*height = 16U;
	return 1;
}

static int
sdl2_glyph_draw(void *opaque, unsigned x, unsigned y, uint32_t codepoint,
		uint32_t foreground, uint32_t background)
{
	struct beui_sdl2_context *context;
	struct noct_beui_rect rect;
	uint8_t rows[7];
	unsigned width;
	unsigned row;
	unsigned column;

	context = opaque;
	if (context == NULL || context->framebuffer == NULL)
		return 0;
	width = codepoint < 0x80U ? 8U : 16U;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = 16;
	if (!sdl2_fill(context, &rect, background))
		return 0;
	if (!sdl2_lock_framebuffer(context))
		return 0;
	if (codepoint >= 0x80U) {
		for (column = 1; column + 1 < width; column++) {
			sdl2_put_pixel(context, x + column, y + 1, foreground);
			sdl2_put_pixel(context, x + column, y + 14, foreground);
		}
		for (row = 1; row < 15; row++) {
			sdl2_put_pixel(context, x + 1, y + row, foreground);
			sdl2_put_pixel(context, x + width - 2, y + row, foreground);
		}
		sdl2_unlock_framebuffer(context);
		return 1;
	}
	sdl2_ascii_glyph(codepoint, rows);
	for (row = 0; row < 7; row++) {
		for (column = 0; column < 5; column++) {
			if ((rows[row] & (uint8_t)(16U >> column)) != 0) {
				sdl2_put_pixel(context, x + column + 1,
					       y + row * 2U + 1U, foreground);
				sdl2_put_pixel(context, x + column + 1,
					       y + row * 2U + 2U, foreground);
			}
		}
	}
	sdl2_unlock_framebuffer(context);
	return 1;
}

static int
sdl2_pointer_start(void *opaque,
		   const struct noct_beui_display_info *display)
{
	struct beui_sdl2_context *context;

	context = opaque;
	if (context == NULL || context->window == NULL || display == NULL)
		return 0;
	SDL_WarpMouseInWindow(context->window, (int)(display->width / 2U),
			      (int)(display->height / 2U));
	return 1;
}

static void
sdl2_pointer_stop(void *opaque)
{
	(void)opaque;
}

static int
sdl2_pointer_poll(void *opaque, struct noct_beui_pointer_event *event)
{
	struct beui_sdl2_context *context;
	int x;
	int y;
	uint32_t buttons;

	context = opaque;
	if (context == NULL || context->window == NULL || event == NULL)
		return -1;
	buttons = SDL_GetMouseState(&x, &y);
	event->x = x < 0 ? 0U : (unsigned)x;
	event->y = y < 0 ? 0U : (unsigned)y;
	event->buttons = 0;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_LEFT;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_RIGHT;
	if ((buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0)
		event->buttons |= NOCT_BEUI_BUTTON_MIDDLE;
	return 1;
}

static uint64_t
sdl2_milliseconds(void *opaque)
{
	struct beui_sdl2_context *context;
	uint64_t counter;
	uint64_t frequency;

	context = opaque;
	if (context != NULL && !context->timer_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_TIMER) == 0)
			context->timer_initialized = 1;
	}
	counter = SDL_GetPerformanceCounter();
	frequency = SDL_GetPerformanceFrequency();
	if (frequency == 0)
		return 0;
	return counter / frequency * 1000U +
		(counter % frequency) * 1000U / frequency;
}

static int
sdl2_audio_start(void *opaque, unsigned sample_rate, unsigned channels)
{
	struct beui_sdl2_context *context;
	SDL_AudioSpec desired;
	SDL_AudioSpec obtained;

	context = opaque;
	if (context == NULL || sample_rate == 0 ||
	    (channels != 1 && channels != 2))
		return 0;
	if (!context->audio_initialized) {
		SDL_SetMainReady();
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
			return 0;
		context->audio_initialized = 1;
	}
	memset(&desired, 0, sizeof(desired));
	desired.freq = (int)sample_rate;
	desired.format = AUDIO_S16SYS;
	desired.channels = (uint8_t)channels;
	desired.samples = 1024;
	context->audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained,
						    SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (context->audio_device == 0 || obtained.format != AUDIO_S16SYS ||
	    obtained.channels != channels) {
		sdl2_audio_stop(context);
		return 0;
	}
	context->audio_channels = channels;
	SDL_PauseAudioDevice(context->audio_device, 0);
	return 1;
}

static int
sdl2_audio_poll(void *opaque)
{
	struct beui_sdl2_context *context;

	context = opaque;
	return context != NULL && context->audio_device != 0 &&
		SDL_GetAudioDeviceStatus(context->audio_device) != SDL_AUDIO_STOPPED;
}

static int
sdl2_audio_write(void *opaque, const int16_t *samples, size_t frame_count)
{
	struct beui_sdl2_context *context;
	size_t bytes;

	context = opaque;
	if (context == NULL || context->audio_device == 0 || samples == NULL ||
	    frame_count > UINT_MAX / context->audio_channels / sizeof(*samples))
		return 0;
	bytes = frame_count * context->audio_channels * sizeof(*samples);
	return SDL_QueueAudio(context->audio_device, samples, (uint32_t)bytes) == 0;
}

static SDL_Scancode
sdl2_key_scancode(int key)
{
	switch (key) {
	case NOCT_BEUI_KEY_ESCAPE: return SDL_SCANCODE_ESCAPE;
	case NOCT_BEUI_KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
	case NOCT_BEUI_KEY_TAB: return SDL_SCANCODE_TAB;
	case NOCT_BEUI_KEY_ENTER: return SDL_SCANCODE_RETURN;
	case NOCT_BEUI_KEY_PAGE_UP: return SDL_SCANCODE_PAGEUP;
	case NOCT_BEUI_KEY_PAGE_DOWN: return SDL_SCANCODE_PAGEDOWN;
	case NOCT_BEUI_KEY_INSERT: return SDL_SCANCODE_INSERT;
	case NOCT_BEUI_KEY_DELETE: return SDL_SCANCODE_DELETE;
	case NOCT_BEUI_KEY_UP: return SDL_SCANCODE_UP;
	case NOCT_BEUI_KEY_LEFT: return SDL_SCANCODE_LEFT;
	case NOCT_BEUI_KEY_RIGHT: return SDL_SCANCODE_RIGHT;
	case NOCT_BEUI_KEY_DOWN: return SDL_SCANCODE_DOWN;
	case NOCT_BEUI_KEY_HOME: return SDL_SCANCODE_HOME;
	case NOCT_BEUI_KEY_END: return SDL_SCANCODE_END;
	case NOCT_BEUI_KEY_F1: return SDL_SCANCODE_F1;
	case NOCT_BEUI_KEY_F2: return SDL_SCANCODE_F2;
	case NOCT_BEUI_KEY_F3: return SDL_SCANCODE_F3;
	case NOCT_BEUI_KEY_F4: return SDL_SCANCODE_F4;
	case NOCT_BEUI_KEY_F5: return SDL_SCANCODE_F5;
	case NOCT_BEUI_KEY_F6: return SDL_SCANCODE_F6;
	case NOCT_BEUI_KEY_F7: return SDL_SCANCODE_F7;
	case NOCT_BEUI_KEY_F8: return SDL_SCANCODE_F8;
	case NOCT_BEUI_KEY_F9: return SDL_SCANCODE_F9;
	case NOCT_BEUI_KEY_F10: return SDL_SCANCODE_F10;
	default:
		if (key >= 0x20 && key < 0x7f)
			return SDL_GetScancodeFromKey((SDL_Keycode)key);
		return SDL_SCANCODE_UNKNOWN;
	}
}

static int
sdl2_is_key_down(void *opaque, int key)
{
	struct beui_sdl2_context *context;
	const uint8_t *keyboard;
	SDL_Scancode scancode;

	context = opaque;
	if (context == NULL || context->window == NULL)
		return -1;
	SDL_PumpEvents();
	keyboard = SDL_GetKeyboardState(NULL);
	if (key == NOCT_BEUI_KEY_SHIFT)
		return keyboard[SDL_SCANCODE_LSHIFT] ||
			keyboard[SDL_SCANCODE_RSHIFT];
	scancode = sdl2_key_scancode(key);
	if (scancode == SDL_SCANCODE_UNKNOWN)
		return -1;
	return keyboard[scancode] != 0;
}

static void
sdl2_drain_input(void *opaque)
{
	(void)opaque;
	SDL_PumpEvents();
	SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
}

NOCT_DLL
bool
noct_register_api_beui(NoctEnv *env)
{
	noct_beui_cleanup();
	memset(&sdl2_hal, 0, sizeof(sdl2_hal));
	sdl2_hal.display.context = &sdl2_context;
	sdl2_hal.display.enter = sdl2_enter;
	sdl2_hal.display.leave = sdl2_leave;
	sdl2_hal.display.poll_events = sdl2_poll_events;
	sdl2_hal.display.fill = sdl2_fill;
	sdl2_hal.display.line = sdl2_line;
	sdl2_hal.display.pattern_fill = sdl2_pattern_fill;
	sdl2_hal.display.draw_image = sdl2_draw_image;
	sdl2_hal.display.draw_image_pattern = sdl2_draw_image_pattern;
	sdl2_hal.display.flush = sdl2_flush;
	sdl2_hal.glyph.context = &sdl2_context;
	sdl2_hal.glyph.measure = sdl2_glyph_measure;
	sdl2_hal.glyph.draw = sdl2_glyph_draw;
	sdl2_hal.pointer.context = &sdl2_context;
	sdl2_hal.pointer.start = sdl2_pointer_start;
	sdl2_hal.pointer.stop = sdl2_pointer_stop;
	sdl2_hal.pointer.poll = sdl2_pointer_poll;
	sdl2_hal.clock.context = &sdl2_context;
	sdl2_hal.clock.milliseconds = sdl2_milliseconds;
	sdl2_hal.audio.context = &sdl2_context;
	sdl2_hal.audio.start = sdl2_audio_start;
	sdl2_hal.audio.stop = sdl2_audio_stop;
	sdl2_hal.audio.poll = sdl2_audio_poll;
	sdl2_hal.audio.write = sdl2_audio_write;
	sdl2_hal.input.context = &sdl2_context;
	sdl2_hal.input.is_key_down = sdl2_is_key_down;
	sdl2_hal.input.drain = sdl2_drain_input;
	return register_beui_api(env, &sdl2_hal);
}
