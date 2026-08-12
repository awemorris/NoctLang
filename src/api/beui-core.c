/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * BeUI lifecycle and hardware abstraction, imported from the Boots
 * graphical layer (beui/beui.c) when BeUI was promoted to a Noct
 * non-standard API.
 */

#include <noct/beui.h>

#include <stdlib.h>
#include <string.h>

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

int
noct_beui_bind(const struct noct_beui_hal *hal)
{
	if (state.display_open || state.pointer_open || state.audio_open)
		return 0;
	state.hal = hal;
	return 1;
}

int
noct_beui_init(void)
{
	return noct_beui_init_with_hint(0);
}

int
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

void
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

void
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

int
noct_beui_is_open(void)
{
	return state.display_open;
}

int
noct_beui_get_display_info(struct noct_beui_display_info *info)
{
	if (!state.display_open || info == NULL)
		return 0;
	*info = state.display;
	return 1;
}

int
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

int
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

int
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

int
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

int
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

int
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

int
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

int
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

int
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

int
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

int
noct_beui_flush(void)
{
	if (!state.display_open)
		return 0;
	if (state.hal->display.flush == NULL)
		return 1;
	return state.hal->display.flush(state.hal->display.context, NULL, 0);
}

int
noct_beui_get_milliseconds(uint64_t *milliseconds)
{
	if (milliseconds == NULL || state.hal == NULL ||
	    state.hal->clock.milliseconds == NULL)
		return 0;
	*milliseconds = state.hal->clock.milliseconds(state.hal->clock.context);
	return 1;
}

int
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

int
noct_beui_is_key_down(int key)
{
	if (state.hal == NULL || state.hal->input.is_key_down == NULL)
		return -1;
	return state.hal->input.is_key_down(state.hal->input.context, key);
}

void
noct_beui_drain_input(void)
{
	if (state.hal != NULL && state.hal->input.drain != NULL)
		state.hal->input.drain(state.hal->input.context);
}

int
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

const struct noct_beui_image *
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

int
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
