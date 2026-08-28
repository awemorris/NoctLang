/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2026, Awe Morris
 *
 * zedBSD BeUI backend.  Graphics and input are intentionally reached only
 * through the installed public UAPI; in particular, this file does not use
 * the legacy /dev/console event or key-state interfaces.
 */

#include <noct/beui.h>

#include "beui-zedbsd-input.h"

#include <zedbsd/graphics.h>
#include <zedbsd/input.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define BEUI_ZEDBSD_READ_EVENTS 16U
#define BEUI_ZEDBSD_FLUSH_RECTS 32U
#define BEUI_ZEDBSD_GLYPH_BYTES 256U
#define BEUI_ZEDBSD_RESCAN_MS 1000U

#define BITS_PER_ULONG ((unsigned)(sizeof(unsigned long) * 8U))
#define BIT_WORDS(maximum)                                                     \
	(((unsigned)(maximum) + BITS_PER_ULONG) / BITS_PER_ULONG)

struct beui_zedbsd_source {
	int fd;
	char event_name[32];
	int engine_slot;
};

struct beui_zedbsd_context {
	int graphics_fd;
	uint32_t graphics_capabilities;
	struct noct_beui_display_info display;
	struct noct_beui_zedbsd_input input;
	struct beui_zedbsd_source sources[NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES];
	uint64_t next_rescan;
	int input_initialized;
};

static struct beui_zedbsd_context zedbsd_context;
static struct noct_beui_hal zedbsd_hal;

static int
bit_is_set(const unsigned long *bits, unsigned bit)
{
	return (bits[bit / BITS_PER_ULONG] & (1UL << (bit % BITS_PER_ULONG))) !=
	       0;
}

static int
graphics_has(const struct beui_zedbsd_context *context, uint32_t capability)
{
	return context != NULL && context->graphics_fd >= 0 &&
	       (context->graphics_capabilities & capability) != 0;
}

static void
copy_rect(struct graphics_rect *to, const struct noct_beui_rect *from)
{
	to->x = from->x;
	to->y = from->y;
	to->width = from->width;
	to->height = from->height;
}

static int
zedbsd_display_enter(void *opaque, struct noct_beui_display_info *info)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_mode request;

	if (context == NULL || info == NULL || context->graphics_fd >= 0)
		return 0;
	context->graphics_fd = open("/dev/graphics", O_RDWR);
	if (context->graphics_fd < 0)
		return 0;
	memset(&request, 0, sizeof(request));
	request.preferred_bits_per_pixel = info->preferred_bits_per_pixel;
	if (ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_ENTER, &request) != 0 ||
	    request.width == 0 || request.height == 0) {
		(void)close(context->graphics_fd);
		context->graphics_fd = -1;
		return 0;
	}
	info->width = request.width;
	info->height = request.height;
	info->bits_per_pixel = request.bits_per_pixel;
	info->stride = request.stride;
	context->display = *info;
	context->graphics_capabilities = request.capabilities;
	noct_beui_zedbsd_input_init(&context->input, request.width,
				    request.height);
	context->input_initialized = 1;
	return 1;
}

static int
zedbsd_display_poll(void *opaque)
{
	struct beui_zedbsd_context *context = opaque;

	return context != NULL && context->graphics_fd >= 0;
}

static int
zedbsd_display_fill(void *opaque, const struct noct_beui_rect *rect,
		    uint32_t color)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_fill request;

	if (rect == NULL || !graphics_has(context, ZEDBSD_GRAPHICS_CAP_FILL))
		return 0;
	memset(&request, 0, sizeof(request));
	copy_rect(&request.rect, rect);
	request.color = color;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_FILL_RECT,
		     &request) == 0;
}

static int
zedbsd_display_line(void *opaque, unsigned x0, unsigned y0, unsigned x1,
		    unsigned y1, uint32_t color)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_line request;

	if (!graphics_has(context, ZEDBSD_GRAPHICS_CAP_LINE))
		return 0;
	memset(&request, 0, sizeof(request));
	request.x0 = x0;
	request.y0 = y0;
	request.x1 = x1;
	request.y1 = y1;
	request.color = color;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_DRAW_LINE,
		     &request) == 0;
}

static int
zedbsd_display_pattern(void *opaque, const struct noct_beui_rect *rect,
		       uint32_t color, uint64_t pattern)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_pattern_fill request;

	if (rect == NULL || !graphics_has(context, ZEDBSD_GRAPHICS_CAP_PATTERN))
		return 0;
	memset(&request, 0, sizeof(request));
	copy_rect(&request.rect, rect);
	request.color = color;
	request.pattern = pattern;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_PATTERN_FILL,
		     &request) == 0;
}

static int
zedbsd_display_image_common(void *opaque, unsigned x, unsigned y,
			    const struct noct_beui_image *image,
			    uint64_t pattern, int patterned)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_blit request;
	uint32_t capability;

	if (image == NULL || image->pixels == NULL ||
	    image->stride > UINT32_MAX)
		return 0;
	memset(&request, 0, sizeof(request));
	request.x = x;
	request.y = y;
	request.width = image->width;
	request.height = image->height;
	request.stride = (uint32_t)image->stride;
	request.pixels = (uapi_ptr_t)(uintptr_t)image->pixels;
	if (image->format == NOCT_BEUI_IMAGE_RGB24) {
		request.format = ZEDBSD_GRAPHICS_FORMAT_RGB24;
		capability = ZEDBSD_GRAPHICS_CAP_BLIT_RGB24;
	} else if (image->format == NOCT_BEUI_IMAGE_INDEX8 &&
		   image->palette_size <= 256U) {
		request.format = ZEDBSD_GRAPHICS_FORMAT_INDEX8;
		request.palette = (uapi_ptr_t)(uintptr_t)image->palette;
		request.palette_count = image->palette_size;
		capability = ZEDBSD_GRAPHICS_CAP_BLIT_INDEX8;
	} else {
		return 0;
	}
	if (!graphics_has(context, capability))
		return 0;
	request.pattern = pattern;
	return ioctl(context->graphics_fd,
		     patterned ? ZEDBSD_GRAPHICS_BLIT_PATTERN
			       : ZEDBSD_GRAPHICS_BLIT,
		     &request) == 0;
}

static int
zedbsd_display_image(void *opaque, unsigned x, unsigned y,
		     const struct noct_beui_image *image)
{
	return zedbsd_display_image_common(opaque, x, y, image, 0, 0);
}

static int
zedbsd_display_image_pattern(void *opaque, unsigned x, unsigned y,
			     const struct noct_beui_image *image,
			     uint64_t pattern)
{
	return zedbsd_display_image_common(opaque, x, y, image, pattern, 1);
}

static int
zedbsd_display_flush(void *opaque, const struct noct_beui_rect *rectangles,
		     size_t rectangle_count)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_rect converted[BEUI_ZEDBSD_FLUSH_RECTS];
	struct graphics_flush request;
	size_t index;

	if (context == NULL || context->graphics_fd < 0 ||
	    rectangle_count > BEUI_ZEDBSD_FLUSH_RECTS ||
	    (rectangle_count != 0 && rectangles == NULL))
		return 0;
	/* A non-buffered driver has nothing to flush. */
	if (!graphics_has(context, ZEDBSD_GRAPHICS_CAP_FLUSH))
		return 1;
	for (index = 0; index < rectangle_count; index++)
		copy_rect(&converted[index], &rectangles[index]);
	request.rectangles =
	    rectangle_count == 0 ? 0 : (uapi_ptr_t)(uintptr_t)converted;
	request.rectangle_count = (uint32_t)rectangle_count;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_FLUSH, &request) ==
	       0;
}

static int
zedbsd_glyph_get(struct beui_zedbsd_context *context, uint32_t codepoint,
		 struct graphics_glyph *glyph, uint8_t *bitmap, size_t capacity)
{
	if (glyph == NULL || bitmap == NULL ||
	    !graphics_has(context, ZEDBSD_GRAPHICS_CAP_GLYPH) ||
	    capacity > UINT32_MAX)
		return 0;
	memset(glyph, 0, sizeof(*glyph));
	glyph->codepoint = codepoint;
	glyph->bitmap = (uapi_ptr_t)(uintptr_t)bitmap;
	glyph->bitmap_capacity = (uint32_t)capacity;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_GET_GLYPH, glyph) ==
		   0 &&
	       glyph->bitmap_size <= capacity;
}

static int
zedbsd_glyph_measure(void *opaque, uint32_t codepoint, unsigned *width,
		     unsigned *height)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_glyph glyph;
	uint8_t bitmap[BEUI_ZEDBSD_GLYPH_BYTES];

	if (width == NULL || height == NULL ||
	    !zedbsd_glyph_get(context, codepoint, &glyph, bitmap,
			      sizeof(bitmap)))
		return 0;
	*width = glyph.advance != 0 ? glyph.advance : glyph.width;
	*height = glyph.height;
	return 1;
}

static int
zedbsd_glyph_draw(void *opaque, unsigned x, unsigned y, uint32_t codepoint,
		  uint32_t foreground, uint32_t background)
{
	struct beui_zedbsd_context *context = opaque;
	struct graphics_glyph glyph;
	struct graphics_blit blit;
	uint8_t bitmap[BEUI_ZEDBSD_GLYPH_BYTES];

	if (!graphics_has(context, ZEDBSD_GRAPHICS_CAP_BLIT_MONO1) ||
	    !zedbsd_glyph_get(context, codepoint, &glyph, bitmap,
			      sizeof(bitmap)))
		return 0;
	memset(&blit, 0, sizeof(blit));
	blit.x = x;
	blit.y = y;
	blit.width = glyph.width;
	blit.height = glyph.height;
	blit.format = ZEDBSD_GRAPHICS_FORMAT_MONO1;
	blit.stride = glyph.stride;
	blit.pixels = (uapi_ptr_t)(uintptr_t)bitmap;
	blit.foreground = foreground;
	blit.background = background;
	return ioctl(context->graphics_fd, ZEDBSD_GRAPHICS_BLIT, &blit) == 0;
}

static uint64_t
zedbsd_milliseconds(void *opaque)
{
	struct timespec now;

	(void)opaque;
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
		return 0;
	return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
}

static int
zedbsd_input_query_capabilities(
    int fd, struct noct_beui_zedbsd_input_capabilities *capabilities)
{
	unsigned long event_bits[BIT_WORDS(EV_MAX)];
	unsigned long key_bits[BIT_WORDS(KEY_MAX)];
	unsigned long relative_bits[BIT_WORDS(REL_MAX)];
	unsigned long absolute_bits[BIT_WORDS(ABS_MAX)];
	struct input_absinfo absolute;
	struct input_id identity;
	int version;
	unsigned code;

	if (capabilities == NULL)
		return 0;
	memset(event_bits, 0, sizeof(event_bits));
	memset(&identity, 0, sizeof(identity));
	version = 0;
	if (ioctl(fd, EVIOCGVERSION, &version) != 0)
		return 0;
	if (ioctl(fd, EVIOCGID, &identity) != 0)
		return 0;
	if (ioctl(fd, EVIOCGBIT(0, sizeof(event_bits)), event_bits) != 0)
		return 0;
	noct_beui_zedbsd_input_capabilities_clear(capabilities);
	for (code = 0; code <= EV_MAX; code++) {
		if (bit_is_set(event_bits, code))
			noct_beui_zedbsd_input_capabilities_set_event(
			    capabilities, code);
	}
	if (bit_is_set(event_bits, EV_KEY)) {
		memset(key_bits, 0, sizeof(key_bits));
		if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) !=
		    0)
			return 0;
		for (code = 0; code <= KEY_MAX; code++) {
			if (bit_is_set(key_bits, code))
				noct_beui_zedbsd_input_capabilities_set_key(
				    capabilities, code);
		}
	}
	if (bit_is_set(event_bits, EV_REL)) {
		memset(relative_bits, 0, sizeof(relative_bits));
		if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relative_bits)),
			  relative_bits) != 0)
			return 0;
		for (code = 0; code <= REL_MAX; code++) {
			if (bit_is_set(relative_bits, code))
				noct_beui_zedbsd_input_capabilities_set_relative(
				    capabilities, code);
		}
	}
	if (bit_is_set(event_bits, EV_ABS)) {
		memset(absolute_bits, 0, sizeof(absolute_bits));
		if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absolute_bits)),
			  absolute_bits) != 0)
			return 0;
		for (code = 0; code <= ABS_MAX; code++) {
			if (!bit_is_set(absolute_bits, code))
				continue;
			memset(&absolute, 0, sizeof(absolute));
			if (ioctl(fd, EVIOCGABS(code), &absolute) == 0)
				noct_beui_zedbsd_input_capabilities_set_absolute(
				    capabilities, code, &absolute);
		}
	}
	return noct_beui_zedbsd_input_classify(capabilities) !=
	       NOCT_BEUI_ZEDBSD_INPUT_ROLE_NONE;
}

static unsigned
zedbsd_input_resync_source(struct beui_zedbsd_context *context,
			   unsigned source_index)
{
	unsigned long key_bits[BIT_WORDS(KEY_MAX)];
	struct input_absinfo absolute_x;
	struct input_absinfo absolute_y;
	const struct input_absinfo *absolute_x_pointer;
	const struct input_absinfo *absolute_y_pointer;
	const void *key_pointer;
	int fd;

	if (context == NULL ||
	    source_index >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES ||
	    context->sources[source_index].fd < 0)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	fd = context->sources[source_index].fd;
	memset(key_bits, 0, sizeof(key_bits));
	key_pointer = ioctl(fd, EVIOCGKEY(sizeof(key_bits)), key_bits) == 0
			  ? (const void *)key_bits
			  : NULL;
	memset(&absolute_x, 0, sizeof(absolute_x));
	absolute_x_pointer =
	    ioctl(fd, EVIOCGABS(ABS_X), &absolute_x) == 0 ? &absolute_x : NULL;
	memset(&absolute_y, 0, sizeof(absolute_y));
	absolute_y_pointer =
	    ioctl(fd, EVIOCGABS(ABS_Y), &absolute_y) == 0 ? &absolute_y : NULL;
	return noct_beui_zedbsd_input_resync(
	    &context->input, source_index, key_pointer, sizeof(key_bits),
	    absolute_x_pointer, absolute_y_pointer);
}

static int
zedbsd_input_event_name(const char *name)
{
	const char *cursor;

	if (name == NULL || strncmp(name, "event", 5) != 0 || name[5] == '\0')
		return 0;
	for (cursor = name + 5; *cursor != '\0'; cursor++) {
		if (*cursor < '0' || *cursor > '9')
			return 0;
	}
	return 1;
}

static int
zedbsd_input_has_event_name(const struct beui_zedbsd_context *context,
			    const char *event_name)
{
	unsigned index;

	for (index = 0; index < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; index++) {
		if (context->sources[index].fd >= 0 &&
		    strcmp(context->sources[index].event_name, event_name) == 0)
			return 1;
	}
	return 0;
}

static void
zedbsd_input_discover(struct beui_zedbsd_context *context, int force)
{
	struct noct_beui_zedbsd_input_capabilities capabilities;
	struct dirent *entry;
	DIR *directory;
	uint64_t now;

	if (context == NULL || !context->input_initialized)
		return;
	now = zedbsd_milliseconds(context);
	if (!force && context->next_rescan != 0 && now < context->next_rescan)
		return;
	context->next_rescan = now + BEUI_ZEDBSD_RESCAN_MS;
	directory = opendir("/dev/input");
	if (directory == NULL)
		return;
	while ((entry = readdir(directory)) != NULL) {
		char path[32];
		int fd;
		int slot;
		int length;

		if (!zedbsd_input_event_name(entry->d_name) ||
		    strlen(entry->d_name) >=
			sizeof(context->sources[0].event_name) ||
		    zedbsd_input_has_event_name(context, entry->d_name))
			continue;
		length = snprintf(path, sizeof(path), "/dev/input/%s",
				  entry->d_name);
		if (length < 0 || (size_t)length >= sizeof(path))
			continue;
		fd = open(path, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;
		if (!zedbsd_input_query_capabilities(fd, &capabilities)) {
			(void)close(fd);
			continue;
		}
		slot = noct_beui_zedbsd_input_attach(&context->input,
						     &capabilities);
		if (slot < 0 ||
		    (unsigned)slot >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES) {
			(void)close(fd);
			continue;
		}
		context->sources[slot].fd = fd;
		(void)strcpy(context->sources[slot].event_name, entry->d_name);
		context->sources[slot].engine_slot = slot;
		(void)zedbsd_input_resync_source(context, (unsigned)slot);
	}
	(void)closedir(directory);
}

static void
zedbsd_input_detach_source(struct beui_zedbsd_context *context,
			   unsigned source_index)
{
	if (context == NULL ||
	    source_index >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES)
		return;
	if (context->sources[source_index].fd >= 0)
		(void)close(context->sources[source_index].fd);
	context->sources[source_index].fd = -1;
	context->sources[source_index].event_name[0] = '\0';
	context->sources[source_index].engine_slot = -1;
	(void)noct_beui_zedbsd_input_detach(&context->input, source_index);
	context->next_rescan =
	    zedbsd_milliseconds(context) + BEUI_ZEDBSD_RESCAN_MS;
}

static void
zedbsd_input_service_source(struct beui_zedbsd_context *context,
			    unsigned source_index)
{
	unsigned char
	    buffer[sizeof(struct input_event) * BEUI_ZEDBSD_READ_EVENTS];
	unsigned iteration;

	for (iteration = 0; iteration < BEUI_ZEDBSD_READ_EVENTS; iteration++) {
		ssize_t count;
		unsigned update;

		count = read(context->sources[source_index].fd, buffer,
			     sizeof(buffer));
		if (count > 0) {
			update = noct_beui_zedbsd_input_feed(
			    &context->input, source_index, buffer,
			    (size_t)count);
			if ((update & NOCT_BEUI_ZEDBSD_INPUT_UPDATE_RESYNC) !=
			    0)
				(void)zedbsd_input_resync_source(context,
								 source_index);
			continue;
		}
		if (count == 0) {
			zedbsd_input_detach_source(context, source_index);
			return;
		}
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			zedbsd_input_detach_source(context, source_index);
		return;
	}
}

static void
zedbsd_input_service(struct beui_zedbsd_context *context)
{
	unsigned index;

	if (context == NULL || !context->input_initialized)
		return;
	for (index = 0; index < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; index++) {
		if (context->sources[index].fd >= 0)
			zedbsd_input_service_source(context, index);
	}
	zedbsd_input_discover(context, 0);
}

static void
zedbsd_input_close(struct beui_zedbsd_context *context)
{
	unsigned index;

	if (context == NULL)
		return;
	for (index = 0; index < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; index++) {
		if (context->sources[index].fd >= 0)
			zedbsd_input_detach_source(context, index);
	}
	noct_beui_zedbsd_input_reset(&context->input);
	context->input_initialized = 0;
	context->next_rescan = 0;
}

static void
zedbsd_display_leave(void *opaque)
{
	struct beui_zedbsd_context *context = opaque;

	if (context == NULL)
		return;
	zedbsd_input_close(context);
	if (context->graphics_fd >= 0)
		(void)close(context->graphics_fd);
	context->graphics_fd = -1;
	context->graphics_capabilities = 0;
	memset(&context->display, 0, sizeof(context->display));
}

static int
zedbsd_pointer_start(void *opaque, const struct noct_beui_display_info *display)
{
	struct beui_zedbsd_context *context = opaque;

	if (context == NULL || display == NULL || !context->input_initialized)
		return 0;
	noct_beui_zedbsd_input_set_display(&context->input, display->width,
					   display->height);
	zedbsd_input_discover(context, 1);
	return 1;
}

static void
zedbsd_pointer_stop(void *opaque)
{
	(void)opaque;
}

static int
zedbsd_pointer_poll(void *opaque, struct noct_beui_pointer_event *event)
{
	struct beui_zedbsd_context *context = opaque;

	if (context == NULL || !context->input_initialized)
		return -1;
	zedbsd_input_service(context);
	return noct_beui_zedbsd_input_poll_pointer(&context->input, event);
}

static int
zedbsd_key_state(void *opaque, int key)
{
	struct beui_zedbsd_context *context = opaque;

	if (context == NULL || !context->input_initialized)
		return -1;
	zedbsd_input_service(context);
	return noct_beui_zedbsd_input_is_key_down(&context->input, key);
}

static void
zedbsd_input_drain(void *opaque)
{
	zedbsd_input_service(opaque);
}

bool
noct_register_api_beui_zedbsd(NoctEnv *env)
{
	unsigned index;

	memset(&zedbsd_context, 0, sizeof(zedbsd_context));
	zedbsd_context.graphics_fd = -1;
	for (index = 0; index < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; index++) {
		zedbsd_context.sources[index].fd = -1;
		zedbsd_context.sources[index].engine_slot = -1;
	}
	memset(&zedbsd_hal, 0, sizeof(zedbsd_hal));
	zedbsd_hal.display.context = &zedbsd_context;
	zedbsd_hal.display.enter = zedbsd_display_enter;
	zedbsd_hal.display.leave = zedbsd_display_leave;
	zedbsd_hal.display.poll_events = zedbsd_display_poll;
	zedbsd_hal.display.fill = zedbsd_display_fill;
	zedbsd_hal.display.line = zedbsd_display_line;
	zedbsd_hal.display.pattern_fill = zedbsd_display_pattern;
	zedbsd_hal.display.draw_image = zedbsd_display_image;
	zedbsd_hal.display.draw_image_pattern = zedbsd_display_image_pattern;
	zedbsd_hal.display.flush = zedbsd_display_flush;
	zedbsd_hal.glyph.context = &zedbsd_context;
	zedbsd_hal.glyph.measure = zedbsd_glyph_measure;
	zedbsd_hal.glyph.draw = zedbsd_glyph_draw;
	zedbsd_hal.pointer.context = &zedbsd_context;
	zedbsd_hal.pointer.start = zedbsd_pointer_start;
	zedbsd_hal.pointer.stop = zedbsd_pointer_stop;
	zedbsd_hal.pointer.poll = zedbsd_pointer_poll;
	zedbsd_hal.clock.context = &zedbsd_context;
	zedbsd_hal.clock.milliseconds = zedbsd_milliseconds;
	zedbsd_hal.input.context = &zedbsd_context;
	zedbsd_hal.input.is_key_down = zedbsd_key_state;
	zedbsd_hal.input.drain = zedbsd_input_drain;
	return noct_register_api_beui(env, &zedbsd_hal);
}
