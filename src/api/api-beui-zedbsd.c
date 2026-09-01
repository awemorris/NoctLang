/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * zedBSD BeUI backend and evdev state engine.  This is intentionally one
 * target source file; define NOCT_BEUI_ZEDBSD_INPUT_TEST when including it in
 * the pure state-engine host corpus so the descriptor/ioctl adapter is omitted.
 */

#include <noct/noct.h>

#include <zedbsd/input.h>

#include <stddef.h>
#include <stdint.h>

/*
 * Complete platform-private BeUI data contract.  The state engine remains
 * available to its include-based focused test, while the runtime functions
 * below are compiled only for the production translation unit.
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

#define NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES 16U
#define NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD (sizeof(unsigned long) * 8U)
#define NOCT_BEUI_ZEDBSD_INPUT_WORDS(maximum)                                  \
	(((maximum) + 1U + NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD - 1U) /        \
	 NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD)
#define NOCT_BEUI_ZEDBSD_INPUT_EVENT_WORDS NOCT_BEUI_ZEDBSD_INPUT_WORDS(EV_MAX)
#define NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS NOCT_BEUI_ZEDBSD_INPUT_WORDS(KEY_MAX)
#define NOCT_BEUI_ZEDBSD_INPUT_REL_WORDS NOCT_BEUI_ZEDBSD_INPUT_WORDS(REL_MAX)
#define NOCT_BEUI_ZEDBSD_INPUT_ABS_WORDS NOCT_BEUI_ZEDBSD_INPUT_WORDS(ABS_MAX)
enum noct_beui_zedbsd_input_role {
	NOCT_BEUI_ZEDBSD_INPUT_ROLE_NONE = 0,
	NOCT_BEUI_ZEDBSD_INPUT_ROLE_KEYBOARD = 1U << 0,
	NOCT_BEUI_ZEDBSD_INPUT_ROLE_RELATIVE_POINTER = 1U << 1,
	NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER = 1U << 2
};

enum noct_beui_zedbsd_input_update {
	NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE = 0,
	NOCT_BEUI_ZEDBSD_INPUT_UPDATE_KEY = 1U << 0,
	NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER = 1U << 1,
	/* Committed state was visibly cleared after SYN_DROPPED. */
	NOCT_BEUI_ZEDBSD_INPUT_UPDATE_RESET = 1U << 2,
	/* Query EVIOCGKEY/EVIOCGABS and call resync() for this source. */
	NOCT_BEUI_ZEDBSD_INPUT_UPDATE_RESYNC = 1U << 3
};

struct noct_beui_zedbsd_input_capabilities {
	unsigned long event_bits[NOCT_BEUI_ZEDBSD_INPUT_EVENT_WORDS];
	unsigned long key_bits[NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS];
	unsigned long relative_bits[NOCT_BEUI_ZEDBSD_INPUT_REL_WORDS];
	unsigned long absolute_bits[NOCT_BEUI_ZEDBSD_INPUT_ABS_WORDS];
	struct input_absinfo absolute_x;
	struct input_absinfo absolute_y;
};

struct noct_beui_zedbsd_input_source {
	int active;
	unsigned roles;
	int synchronization_lost;
	struct noct_beui_zedbsd_input_capabilities capabilities;
	unsigned long keys[NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS];
	unsigned long staged_keys[NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS];
	int64_t relative_x;
	int64_t relative_y;
	int32_t absolute_x;
	int32_t absolute_y;
	int32_t staged_absolute_x;
	int32_t staged_absolute_y;
	int staged_absolute_x_valid;
	int staged_absolute_y_valid;
	unsigned char partial_event[sizeof(struct input_event)];
	size_t partial_event_size;
};

struct noct_beui_zedbsd_input {
	struct noct_beui_zedbsd_input_source
	    sources[NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES];
	unsigned display_width;
	unsigned display_height;
	unsigned pointer_x;
	unsigned pointer_y;
	unsigned pointer_buttons;
	int pointer_changed;
};

static void noct_beui_zedbsd_input_capabilities_clear(
    struct noct_beui_zedbsd_input_capabilities *capabilities);
static void noct_beui_zedbsd_input_capabilities_set_event(
    struct noct_beui_zedbsd_input_capabilities *capabilities,
    unsigned event_type);
static void noct_beui_zedbsd_input_capabilities_set_key(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned code);
static void noct_beui_zedbsd_input_capabilities_set_relative(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis);
static void noct_beui_zedbsd_input_capabilities_set_absolute(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis,
    const struct input_absinfo *information);
static unsigned noct_beui_zedbsd_input_classify(
    const struct noct_beui_zedbsd_input_capabilities *capabilities);

static void noct_beui_zedbsd_input_init(struct noct_beui_zedbsd_input *input,
				 unsigned display_width,
				 unsigned display_height);
static void noct_beui_zedbsd_input_set_display(struct noct_beui_zedbsd_input *input,
					unsigned display_width,
					unsigned display_height);
#ifndef NOCT_BEUI_ZEDBSD_INPUT_TEST
static void noct_beui_zedbsd_input_reset(struct noct_beui_zedbsd_input *input);
#endif
static int noct_beui_zedbsd_input_attach(
    struct noct_beui_zedbsd_input *input,
    const struct noct_beui_zedbsd_input_capabilities *capabilities);
static unsigned noct_beui_zedbsd_input_detach(struct noct_beui_zedbsd_input *input,
				       unsigned source_index);

/*
 * Feed any byte count read from one nonblocking event descriptor.  Complete
 * records are processed in order, while a trailing record is retained in the
 * source slot for the next call.  The returned flags describe visible state
 * changes and whether the OS adapter must perform a resynchronization query.
 */
static unsigned noct_beui_zedbsd_input_feed(struct noct_beui_zedbsd_input *input,
				     unsigned source_index, const void *bytes,
				     size_t byte_count);

/*
 * Complete recovery after UPDATE_RESYNC.  key_bits may be NULL to retain the
 * visible all-up reset.  ABS information is optional; non-NULL values replace
 * the stored range and current raw coordinate.  key_byte_count is truncated
 * safely to the native evdev bitmap size.
 */
static unsigned noct_beui_zedbsd_input_resync(struct noct_beui_zedbsd_input *input,
				       unsigned source_index,
				       const void *key_bits,
				       size_t key_byte_count,
				       const struct input_absinfo *absolute_x,
				       const struct input_absinfo *absolute_y);

static int
noct_beui_zedbsd_input_is_key_down(const struct noct_beui_zedbsd_input *input,
				   int beui_key);
static int noct_beui_zedbsd_input_poll_pointer(struct noct_beui_zedbsd_input *input,
					struct noct_beui_pointer_event *event);

#include <limits.h>
#include <string.h>

static int
bit_is_set(const unsigned long *bits, unsigned maximum, unsigned bit)
{
	if (bits == NULL || bit > maximum)
		return 0;
	return (bits[bit / NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD] &
		(1UL << (bit % NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD))) != 0;
}

static void
bit_set(unsigned long *bits, unsigned maximum, unsigned bit)
{
	if (bits != NULL && bit <= maximum)
		bits[bit / NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD] |=
		    1UL << (bit % NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD);
}

static void
bit_clear(unsigned long *bits, unsigned maximum, unsigned bit)
{
	if (bits != NULL && bit <= maximum)
		bits[bit / NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD] &=
		    ~(1UL << (bit % NOCT_BEUI_ZEDBSD_INPUT_BITS_PER_WORD));
}

static int
is_keyboard_code(unsigned code)
{
	return code > KEY_RESERVED && code < BTN_MOUSE;
}

static int
bitmap_any(const unsigned long *bits, size_t word_count)
{
	size_t i;

	for (i = 0; i < word_count; i++) {
		if (bits[i] != 0UL)
			return 1;
	}
	return 0;
}

static int
source_has_pointer(const struct noct_beui_zedbsd_input_source *source)
{
	return source->active &&
	       (source->roles &
		(NOCT_BEUI_ZEDBSD_INPUT_ROLE_RELATIVE_POINTER |
		 NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER)) != 0U;
}

static int
input_has_pointer(const struct noct_beui_zedbsd_input *input)
{
	unsigned i;

	for (i = 0; i < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; i++) {
		if (source_has_pointer(&input->sources[i]))
			return 1;
	}
	return 0;
}

static unsigned
aggregate_buttons(const struct noct_beui_zedbsd_input *input)
{
	unsigned buttons;
	unsigned i;

	buttons = 0;
	for (i = 0; i < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; i++) {
		const struct noct_beui_zedbsd_input_source *source;

		source = &input->sources[i];
		if (!source_has_pointer(source))
			continue;
		if (bit_is_set(source->keys, KEY_MAX, BTN_LEFT))
			buttons |= NOCT_BEUI_BUTTON_LEFT;
		if (bit_is_set(source->keys, KEY_MAX, BTN_RIGHT))
			buttons |= NOCT_BEUI_BUTTON_RIGHT;
		if (bit_is_set(source->keys, KEY_MAX, BTN_MIDDLE))
			buttons |= NOCT_BEUI_BUTTON_MIDDLE;
	}
	return buttons;
}

static unsigned
refresh_buttons(struct noct_beui_zedbsd_input *input)
{
	unsigned buttons;

	buttons = aggregate_buttons(input);
	if (buttons == input->pointer_buttons)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	input->pointer_buttons = buttons;
	input->pointer_changed = 1;
	return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER;
}

static unsigned
clamp_coordinate(int64_t coordinate, unsigned extent)
{
	if (extent == 0U || coordinate <= 0)
		return 0U;
	if ((uint64_t)coordinate >= (uint64_t)extent)
		return extent - 1U;
	return (unsigned)coordinate;
}

static unsigned
center_pointer(struct noct_beui_zedbsd_input *input)
{
	unsigned x;
	unsigned y;

	x = input->display_width == 0U ? 0U : input->display_width / 2U;
	y = input->display_height == 0U ? 0U : input->display_height / 2U;
	if (input->pointer_x == x && input->pointer_y == y)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	input->pointer_x = x;
	input->pointer_y = y;
	input->pointer_changed = 1;
	return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER;
}

static unsigned
apply_relative(unsigned coordinate, int64_t delta, unsigned extent)
{
	uint64_t magnitude;

	if (extent == 0U)
		return 0U;
	coordinate = clamp_coordinate(coordinate, extent);
	if (delta >= 0) {
		if ((uint64_t)delta >= (uint64_t)(extent - 1U - coordinate))
			return extent - 1U;
		return coordinate + (unsigned)delta;
	}
	/* -(INT64_MIN) is not representable, so form the magnitude without
	 * negating that value directly. */
	magnitude = (uint64_t)(-(delta + 1)) + 1U;
	if (magnitude >= coordinate)
		return 0U;
	return coordinate - (unsigned)magnitude;
}

static unsigned
scale_absolute(int32_t value, const struct input_absinfo *information,
	       unsigned extent)
{
	int64_t offset;
	uint64_t span;
	uint64_t scaled;

	if (extent <= 1U || information == NULL ||
	    information->maximum <= information->minimum)
		return 0U;
	if (value <= information->minimum)
		return 0U;
	if (value >= information->maximum)
		return extent - 1U;
	offset = (int64_t)value - information->minimum;
	span = (uint64_t)((int64_t)information->maximum - information->minimum);
	scaled = (uint64_t)offset * (uint64_t)(extent - 1U);
	return (unsigned)(scaled / span);
}

static int64_t
saturating_add(int64_t left, int32_t right)
{
	if (right > 0 && left > INT64_MAX - right)
		return INT64_MAX;
	if (right < 0 && left < INT64_MIN - right)
		return INT64_MIN;
	return left + right;
}

static void
noct_beui_zedbsd_input_capabilities_clear(
    struct noct_beui_zedbsd_input_capabilities *capabilities)
{
	if (capabilities != NULL)
		memset(capabilities, 0, sizeof(*capabilities));
}

static void
noct_beui_zedbsd_input_capabilities_set_event(
    struct noct_beui_zedbsd_input_capabilities *capabilities,
    unsigned event_type)
{
	if (capabilities != NULL)
		bit_set(capabilities->event_bits, EV_MAX, event_type);
}

static void
noct_beui_zedbsd_input_capabilities_set_key(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned code)
{
	if (capabilities == NULL)
		return;
	bit_set(capabilities->event_bits, EV_MAX, EV_KEY);
	bit_set(capabilities->key_bits, KEY_MAX, code);
}

static void
noct_beui_zedbsd_input_capabilities_set_relative(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis)
{
	if (capabilities == NULL)
		return;
	bit_set(capabilities->event_bits, EV_MAX, EV_REL);
	bit_set(capabilities->relative_bits, REL_MAX, axis);
}

static void
noct_beui_zedbsd_input_capabilities_set_absolute(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis,
    const struct input_absinfo *information)
{
	if (capabilities == NULL)
		return;
	bit_set(capabilities->event_bits, EV_MAX, EV_ABS);
	bit_set(capabilities->absolute_bits, ABS_MAX, axis);
	if (information == NULL)
		return;
	if (axis == ABS_X)
		capabilities->absolute_x = *information;
	else if (axis == ABS_Y)
		capabilities->absolute_y = *information;
}

static unsigned
noct_beui_zedbsd_input_classify(
    const struct noct_beui_zedbsd_input_capabilities *capabilities)
{
	unsigned code;
	unsigned roles;

	/* EVIOCGBIT(0) is the event-type bitmap.  Like Linux evdev it does
	 * not provide a distinct query for SYN codes; zedBSD registration
	 * already requires SYN_REPORT for every input device. */
	if (capabilities == NULL ||
	    !bit_is_set(capabilities->event_bits, EV_MAX, EV_SYN))
		return NOCT_BEUI_ZEDBSD_INPUT_ROLE_NONE;
	roles = NOCT_BEUI_ZEDBSD_INPUT_ROLE_NONE;
	if (bit_is_set(capabilities->event_bits, EV_MAX, EV_KEY)) {
		for (code = KEY_RESERVED + 1U; code < BTN_MOUSE; code++) {
			if (is_keyboard_code(code) &&
			    bit_is_set(capabilities->key_bits, KEY_MAX, code)) {
				roles |= NOCT_BEUI_ZEDBSD_INPUT_ROLE_KEYBOARD;
				break;
			}
		}
	}
	if (bit_is_set(capabilities->event_bits, EV_MAX, EV_REL) &&
	    bit_is_set(capabilities->relative_bits, REL_MAX, REL_X) &&
	    bit_is_set(capabilities->relative_bits, REL_MAX, REL_Y))
		roles |= NOCT_BEUI_ZEDBSD_INPUT_ROLE_RELATIVE_POINTER;
	if (bit_is_set(capabilities->event_bits, EV_MAX, EV_ABS) &&
	    bit_is_set(capabilities->absolute_bits, ABS_MAX, ABS_X) &&
	    bit_is_set(capabilities->absolute_bits, ABS_MAX, ABS_Y))
		roles |= NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER;
	return roles;
}

static void
noct_beui_zedbsd_input_init(struct noct_beui_zedbsd_input *input,
			    unsigned display_width, unsigned display_height)
{
	if (input == NULL)
		return;
	memset(input, 0, sizeof(*input));
	input->display_width = display_width;
	input->display_height = display_height;
	input->pointer_x = display_width == 0U ? 0U : display_width / 2U;
	input->pointer_y = display_height == 0U ? 0U : display_height / 2U;
}

static void
noct_beui_zedbsd_input_set_display(struct noct_beui_zedbsd_input *input,
				   unsigned display_width,
				   unsigned display_height)
{
	unsigned old_x;
	unsigned old_y;
	unsigned old_width;
	unsigned old_height;

	if (input == NULL)
		return;
	old_x = input->pointer_x;
	old_y = input->pointer_y;
	old_width = input->display_width;
	old_height = input->display_height;
	input->display_width = display_width;
	input->display_height = display_height;
	input->pointer_x =
	    old_width == 0U && display_width != 0U
		? display_width / 2U
		: clamp_coordinate(input->pointer_x, display_width);
	input->pointer_y =
	    old_height == 0U && display_height != 0U
		? display_height / 2U
		: clamp_coordinate(input->pointer_y, display_height);
	if (old_x != input->pointer_x || old_y != input->pointer_y)
		input->pointer_changed = 1;
}

#ifndef NOCT_BEUI_ZEDBSD_INPUT_TEST
static void
noct_beui_zedbsd_input_reset(struct noct_beui_zedbsd_input *input)
{
	unsigned width;
	unsigned height;

	if (input == NULL)
		return;
	width = input->display_width;
	height = input->display_height;
	noct_beui_zedbsd_input_init(input, width, height);
}
#endif

static int
noct_beui_zedbsd_input_attach(
    struct noct_beui_zedbsd_input *input,
    const struct noct_beui_zedbsd_input_capabilities *capabilities)
{
	struct noct_beui_zedbsd_input_source *source;
	unsigned roles;
	unsigned i;

	if (input == NULL || capabilities == NULL)
		return -1;
	roles = noct_beui_zedbsd_input_classify(capabilities);
	if (roles == NOCT_BEUI_ZEDBSD_INPUT_ROLE_NONE)
		return -1;
	for (i = 0; i < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; i++) {
		if (!input->sources[i].active)
			break;
	}
	if (i == NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES)
		return -1;
	source = &input->sources[i];
	memset(source, 0, sizeof(*source));
	source->active = 1;
	source->roles = roles;
	source->capabilities = *capabilities;
	if ((roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER) != 0U) {
		source->absolute_x = capabilities->absolute_x.value;
		source->absolute_y = capabilities->absolute_y.value;
		source->staged_absolute_x = source->absolute_x;
		source->staged_absolute_y = source->absolute_y;
		input->pointer_x = scale_absolute(source->absolute_x,
						  &capabilities->absolute_x,
						  input->display_width);
		input->pointer_y = scale_absolute(source->absolute_y,
						  &capabilities->absolute_y,
						  input->display_height);
	}
	if (source_has_pointer(source))
		input->pointer_changed = 1;
	return (int)i;
}

static unsigned
noct_beui_zedbsd_input_detach(struct noct_beui_zedbsd_input *input,
			      unsigned source_index)
{
	struct noct_beui_zedbsd_input_source *source;
	unsigned result;
	int had_keys;
	int had_pointer;

	if (input == NULL || source_index >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	source = &input->sources[source_index];
	if (!source->active)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	had_keys = bitmap_any(source->keys, NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS);
	had_pointer = source_has_pointer(source);
	memset(source, 0, sizeof(*source));
	result = had_keys ? NOCT_BEUI_ZEDBSD_INPUT_UPDATE_KEY
			  : NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	result |= refresh_buttons(input);
	if (had_pointer && !input_has_pointer(input)) {
		input->pointer_buttons = 0U;
		result |= center_pointer(input);
		/* Publish one final all-up/centered state even though no source
		 * remains.  This prevents a button held at detach from becoming
		 * permanently stuck in the BeUI core. */
		input->pointer_changed = 1;
		result |= NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER;
	}
	return result;
}

static unsigned
reset_source_after_drop(struct noct_beui_zedbsd_input *input,
			struct noct_beui_zedbsd_input_source *source)
{
	unsigned result;
	int had_keys;

	had_keys = bitmap_any(source->keys, NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS);
	memset(source->keys, 0, sizeof(source->keys));
	memset(source->staged_keys, 0, sizeof(source->staged_keys));
	source->relative_x = 0;
	source->relative_y = 0;
	source->staged_absolute_x_valid = 0;
	source->staged_absolute_y_valid = 0;
	source->synchronization_lost = 1;
	result = NOCT_BEUI_ZEDBSD_INPUT_UPDATE_RESET;
	if (had_keys)
		result |= NOCT_BEUI_ZEDBSD_INPUT_UPDATE_KEY;
	result |= refresh_buttons(input);
	/* Relative coordinates cannot be queried back after queue loss.  A
	 * centered state is the deterministic, visible reset from which later
	 * deltas continue. */
	if ((source->roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_RELATIVE_POINTER) !=
	    0U)
		result |= center_pointer(input);
	return result;
}

static unsigned
commit_source(struct noct_beui_zedbsd_input *input,
	      struct noct_beui_zedbsd_input_source *source)
{
	unsigned old_x;
	unsigned old_y;
	unsigned result;
	int keys_changed;

	old_x = input->pointer_x;
	old_y = input->pointer_y;
	keys_changed = memcmp(source->keys, source->staged_keys,
			      sizeof(source->keys)) != 0;
	memcpy(source->keys, source->staged_keys, sizeof(source->keys));
	if ((source->roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER) !=
	    0U) {
		if (source->staged_absolute_x_valid)
			source->absolute_x = source->staged_absolute_x;
		if (source->staged_absolute_y_valid)
			source->absolute_y = source->staged_absolute_y;
		if (source->staged_absolute_x_valid ||
		    source->staged_absolute_y_valid) {
			input->pointer_x =
			    scale_absolute(source->absolute_x,
					   &source->capabilities.absolute_x,
					   input->display_width);
			input->pointer_y =
			    scale_absolute(source->absolute_y,
					   &source->capabilities.absolute_y,
					   input->display_height);
		}
	}
	if ((source->roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_RELATIVE_POINTER) !=
	    0U) {
		input->pointer_x = apply_relative(
		    input->pointer_x, source->relative_x, input->display_width);
		input->pointer_y =
		    apply_relative(input->pointer_y, source->relative_y,
				   input->display_height);
	}
	source->relative_x = 0;
	source->relative_y = 0;
	source->staged_absolute_x_valid = 0;
	source->staged_absolute_y_valid = 0;
	result = keys_changed ? NOCT_BEUI_ZEDBSD_INPUT_UPDATE_KEY
			      : NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	result |= refresh_buttons(input);
	if (old_x != input->pointer_x || old_y != input->pointer_y) {
		input->pointer_changed = 1;
		result |= NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER;
	}
	return result;
}

static unsigned
process_event(struct noct_beui_zedbsd_input *input,
	      struct noct_beui_zedbsd_input_source *source,
	      const struct input_event *event)
{
	if (event->type == EV_SYN && event->code == SYN_DROPPED)
		return reset_source_after_drop(input, source);
	if (source->synchronization_lost) {
		if (event->type == EV_SYN && event->code == SYN_REPORT)
			return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_RESYNC;
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	}
	if (event->type == EV_SYN) {
		if (event->code == SYN_REPORT)
			return commit_source(input, source);
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	}
	if (event->type == EV_KEY && event->code <= KEY_MAX &&
	    bit_is_set(source->capabilities.event_bits, EV_MAX, EV_KEY) &&
	    bit_is_set(source->capabilities.key_bits, KEY_MAX, event->code)) {
		if (event->value == 0)
			bit_clear(source->staged_keys, KEY_MAX, event->code);
		else if (event->value == 1 || event->value == 2)
			bit_set(source->staged_keys, KEY_MAX, event->code);
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	}
	if (event->type == EV_REL && event->code <= REL_MAX &&
	    bit_is_set(source->capabilities.event_bits, EV_MAX, EV_REL) &&
	    bit_is_set(source->capabilities.relative_bits, REL_MAX,
		       event->code)) {
		if (event->code == REL_X)
			source->relative_x =
			    saturating_add(source->relative_x, event->value);
		else if (event->code == REL_Y)
			source->relative_y =
			    saturating_add(source->relative_y, event->value);
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	}
	if (event->type == EV_ABS && event->code <= ABS_MAX &&
	    bit_is_set(source->capabilities.event_bits, EV_MAX, EV_ABS) &&
	    bit_is_set(source->capabilities.absolute_bits, ABS_MAX,
		       event->code)) {
		if (event->code == ABS_X) {
			source->staged_absolute_x = event->value;
			source->staged_absolute_x_valid = 1;
		} else if (event->code == ABS_Y) {
			source->staged_absolute_y = event->value;
			source->staged_absolute_y_valid = 1;
		}
	}
	return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
}

static unsigned
noct_beui_zedbsd_input_feed(struct noct_beui_zedbsd_input *input,
			    unsigned source_index, const void *bytes,
			    size_t byte_count)
{
	struct noct_beui_zedbsd_input_source *source;
	const unsigned char *cursor;
	unsigned result;

	if (input == NULL ||
	    source_index >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES ||
	    (bytes == NULL && byte_count != 0U))
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	source = &input->sources[source_index];
	if (!source->active)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	cursor = bytes;
	result = NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	while (byte_count != 0U) {
		size_t copy_size;

		copy_size =
		    sizeof(source->partial_event) - source->partial_event_size;
		if (copy_size > byte_count)
			copy_size = byte_count;
		memcpy(source->partial_event + source->partial_event_size,
		       cursor, copy_size);
		source->partial_event_size += copy_size;
		cursor += copy_size;
		byte_count -= copy_size;
		if (source->partial_event_size == sizeof(struct input_event)) {
			struct input_event event;

			memcpy(&event, source->partial_event, sizeof(event));
			source->partial_event_size = 0U;
			result |= process_event(input, source, &event);
		}
	}
	return result;
}

static unsigned
noct_beui_zedbsd_input_resync(struct noct_beui_zedbsd_input *input,
			      unsigned source_index, const void *key_bits,
			      size_t key_byte_count,
			      const struct input_absinfo *absolute_x,
			      const struct input_absinfo *absolute_y)
{
	struct noct_beui_zedbsd_input_source *source;
	unsigned long queried[NOCT_BEUI_ZEDBSD_INPUT_KEY_WORDS];
	unsigned old_x;
	unsigned old_y;
	unsigned result;
	unsigned code;
	int keys_changed;

	if (input == NULL || source_index >= NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	source = &input->sources[source_index];
	if (!source->active)
		return NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	memset(queried, 0, sizeof(queried));
	if (key_bits != NULL) {
		if (key_byte_count > sizeof(queried))
			key_byte_count = sizeof(queried);
		memcpy(queried, key_bits, key_byte_count);
	}
	keys_changed = 0;
	for (code = 0; code <= KEY_MAX; code++) {
		int down;

		down = bit_is_set(queried, KEY_MAX, code) &&
		       bit_is_set(source->capabilities.key_bits, KEY_MAX, code);
		if (down != bit_is_set(source->keys, KEY_MAX, code))
			keys_changed = 1;
		if (down)
			bit_set(source->keys, KEY_MAX, code);
		else
			bit_clear(source->keys, KEY_MAX, code);
	}
	memcpy(source->staged_keys, source->keys, sizeof(source->keys));
	old_x = input->pointer_x;
	old_y = input->pointer_y;
	if (absolute_x != NULL &&
	    bit_is_set(source->capabilities.absolute_bits, ABS_MAX, ABS_X)) {
		source->capabilities.absolute_x = *absolute_x;
		source->absolute_x = absolute_x->value;
	}
	if (absolute_y != NULL &&
	    bit_is_set(source->capabilities.absolute_bits, ABS_MAX, ABS_Y)) {
		source->capabilities.absolute_y = *absolute_y;
		source->absolute_y = absolute_y->value;
	}
	if ((source->roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_ABSOLUTE_POINTER) !=
		0U &&
	    (absolute_x != NULL || absolute_y != NULL)) {
		input->pointer_x = scale_absolute(
		    source->absolute_x, &source->capabilities.absolute_x,
		    input->display_width);
		input->pointer_y = scale_absolute(
		    source->absolute_y, &source->capabilities.absolute_y,
		    input->display_height);
	}
	source->relative_x = 0;
	source->relative_y = 0;
	source->staged_absolute_x_valid = 0;
	source->staged_absolute_y_valid = 0;
	source->synchronization_lost = 0;
	result = keys_changed ? NOCT_BEUI_ZEDBSD_INPUT_UPDATE_KEY
			      : NOCT_BEUI_ZEDBSD_INPUT_UPDATE_NONE;
	result |= refresh_buttons(input);
	if (old_x != input->pointer_x || old_y != input->pointer_y) {
		input->pointer_changed = 1;
		result |= NOCT_BEUI_ZEDBSD_INPUT_UPDATE_POINTER;
	}
	return result;
}

static int
beui_key_to_evdev(int key)
{
	static const uint16_t letters[26] = {
	    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
	    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
	    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
	static const uint16_t digits[10] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
					    KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};

	if (key >= 'a' && key <= 'z')
		return letters[key - 'a'];
	if (key >= '0' && key <= '9')
		return digits[key - '0'];
	switch (key) {
	case ' ':
		return KEY_SPACE;
	case '-':
	case '_':
		return KEY_MINUS;
	case '=':
	case '+':
		return KEY_EQUAL;
	case '[':
	case '{':
		return KEY_LEFTBRACE;
	case ']':
	case '}':
		return KEY_RIGHTBRACE;
	case ';':
	case ':':
		return KEY_SEMICOLON;
	case '\'':
	case '"':
		return KEY_APOSTROPHE;
	case '`':
	case '~':
		return KEY_GRAVE;
	case '\\':
	case '|':
		return KEY_BACKSLASH;
	case ',':
	case '<':
		return KEY_COMMA;
	case '.':
	case '>':
		return KEY_DOT;
	case '/':
	case '?':
		return KEY_SLASH;
	case '!':
		return KEY_1;
	case '@':
		return KEY_2;
	case '#':
		return KEY_3;
	case '$':
		return KEY_4;
	case '%':
		return KEY_5;
	case '^':
		return KEY_6;
	case '&':
		return KEY_7;
	case '*':
		return KEY_8;
	case '(':
		return KEY_9;
	case ')':
		return KEY_0;
	case NOCT_BEUI_KEY_ESCAPE:
		return KEY_ESC;
	case NOCT_BEUI_KEY_BACKSPACE:
		return KEY_BACKSPACE;
	case NOCT_BEUI_KEY_TAB:
		return KEY_TAB;
	case NOCT_BEUI_KEY_ENTER:
		return KEY_ENTER;
	case NOCT_BEUI_KEY_PAGE_UP:
		return KEY_PAGEUP;
	case NOCT_BEUI_KEY_PAGE_DOWN:
		return KEY_PAGEDOWN;
	case NOCT_BEUI_KEY_INSERT:
		return KEY_INSERT;
	case NOCT_BEUI_KEY_DELETE:
		return KEY_DELETE;
	case NOCT_BEUI_KEY_UP:
		return KEY_UP;
	case NOCT_BEUI_KEY_LEFT:
		return KEY_LEFT;
	case NOCT_BEUI_KEY_RIGHT:
		return KEY_RIGHT;
	case NOCT_BEUI_KEY_DOWN:
		return KEY_DOWN;
	case NOCT_BEUI_KEY_HOME:
		return KEY_HOME;
	case NOCT_BEUI_KEY_END:
		return KEY_END;
	case NOCT_BEUI_KEY_F1:
		return KEY_F1;
	case NOCT_BEUI_KEY_F2:
		return KEY_F2;
	case NOCT_BEUI_KEY_F3:
		return KEY_F3;
	case NOCT_BEUI_KEY_F4:
		return KEY_F4;
	case NOCT_BEUI_KEY_F5:
		return KEY_F5;
	case NOCT_BEUI_KEY_F6:
		return KEY_F6;
	case NOCT_BEUI_KEY_F7:
		return KEY_F7;
	case NOCT_BEUI_KEY_F8:
		return KEY_F8;
	case NOCT_BEUI_KEY_F9:
		return KEY_F9;
	case NOCT_BEUI_KEY_F10:
		return KEY_F10;
	default:
		return -1;
	}
}

static int
source_key_state(const struct noct_beui_zedbsd_input *input, unsigned code,
		 int alternate_code)
{
	unsigned i;
	int supported;

	supported = 0;
	for (i = 0; i < NOCT_BEUI_ZEDBSD_INPUT_MAX_SOURCES; i++) {
		const struct noct_beui_zedbsd_input_source *source;

		source = &input->sources[i];
		if (!source->active ||
		    (source->roles & NOCT_BEUI_ZEDBSD_INPUT_ROLE_KEYBOARD) ==
			0U)
			continue;
		if (bit_is_set(source->capabilities.key_bits, KEY_MAX, code)) {
			supported = 1;
			if (bit_is_set(source->keys, KEY_MAX, code))
				return 1;
		}
		if (alternate_code >= 0 &&
		    bit_is_set(source->capabilities.key_bits, KEY_MAX,
			       (unsigned)alternate_code)) {
			supported = 1;
			if (bit_is_set(source->keys, KEY_MAX,
				       (unsigned)alternate_code))
				return 1;
		}
	}
	return supported ? 0 : -1;
}

static int
noct_beui_zedbsd_input_is_key_down(const struct noct_beui_zedbsd_input *input,
				   int beui_key)
{
	int code;

	if (input == NULL)
		return -1;
	if (beui_key == NOCT_BEUI_KEY_SHIFT)
		return source_key_state(input, KEY_LEFTSHIFT, KEY_RIGHTSHIFT);
	code = beui_key_to_evdev(beui_key);
	if (code < 0)
		return -1;
	return source_key_state(input, (unsigned)code, -1);
}

static int
noct_beui_zedbsd_input_poll_pointer(struct noct_beui_zedbsd_input *input,
				    struct noct_beui_pointer_event *event)
{
	if (input == NULL || event == NULL || !input->pointer_changed)
		return 0;
	event->x = input->pointer_x;
	event->y = input->pointer_y;
	event->buttons = input->pointer_buttons;
	input->pointer_changed = 0;
	return 1;
}

#ifndef NOCT_BEUI_ZEDBSD_INPUT_TEST

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

#include <stdlib.h>

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
uapi_bit_is_set(const unsigned long *bits, unsigned bit)
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
		if (uapi_bit_is_set(event_bits, code))
			noct_beui_zedbsd_input_capabilities_set_event(
			    capabilities, code);
	}
	if (uapi_bit_is_set(event_bits, EV_KEY)) {
		memset(key_bits, 0, sizeof(key_bits));
		if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) !=
		    0)
			return 0;
		for (code = 0; code <= KEY_MAX; code++) {
			if (uapi_bit_is_set(key_bits, code))
				noct_beui_zedbsd_input_capabilities_set_key(
				    capabilities, code);
		}
	}
	if (uapi_bit_is_set(event_bits, EV_REL)) {
		memset(relative_bits, 0, sizeof(relative_bits));
		if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relative_bits)),
			  relative_bits) != 0)
			return 0;
		for (code = 0; code <= REL_MAX; code++) {
			if (uapi_bit_is_set(relative_bits, code))
				noct_beui_zedbsd_input_capabilities_set_relative(
				    capabilities, code);
		}
	}
	if (uapi_bit_is_set(event_bits, EV_ABS)) {
		memset(absolute_bits, 0, sizeof(absolute_bits));
		if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absolute_bits)),
			  absolute_bits) != 0)
			return 0;
		for (code = 0; code <= ABS_MAX; code++) {
			if (!uapi_bit_is_set(absolute_bits, code))
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

NOCT_DLL
bool
noct_register_api_beui(NoctEnv *env)
{
	unsigned index;

	noct_beui_cleanup();
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
	return register_beui_api(env, &zedbsd_hal);
}

#endif /* !NOCT_BEUI_ZEDBSD_INPUT_TEST */
