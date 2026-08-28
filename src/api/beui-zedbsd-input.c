/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#include "beui-zedbsd-input.h"

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

void
noct_beui_zedbsd_input_capabilities_clear(
    struct noct_beui_zedbsd_input_capabilities *capabilities)
{
	if (capabilities != NULL)
		memset(capabilities, 0, sizeof(*capabilities));
}

void
noct_beui_zedbsd_input_capabilities_set_event(
    struct noct_beui_zedbsd_input_capabilities *capabilities,
    unsigned event_type)
{
	if (capabilities != NULL)
		bit_set(capabilities->event_bits, EV_MAX, event_type);
}

void
noct_beui_zedbsd_input_capabilities_set_key(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned code)
{
	if (capabilities == NULL)
		return;
	bit_set(capabilities->event_bits, EV_MAX, EV_KEY);
	bit_set(capabilities->key_bits, KEY_MAX, code);
}

void
noct_beui_zedbsd_input_capabilities_set_relative(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis)
{
	if (capabilities == NULL)
		return;
	bit_set(capabilities->event_bits, EV_MAX, EV_REL);
	bit_set(capabilities->relative_bits, REL_MAX, axis);
}

void
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

unsigned
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

void
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

void
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

void
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

int
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

unsigned
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

unsigned
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

unsigned
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

int
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

int
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
