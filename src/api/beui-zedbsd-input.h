/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 *
 * Pure evdev state engine for the zedBSD BeUI backend.  The operating-system
 * adapter owns descriptors and ioctls; this module owns capability-derived
 * roles, packet boundaries, synchronization loss, and aggregate BeUI state.
 */

#ifndef NOCT_BEUI_ZEDBSD_INPUT_H
#define NOCT_BEUI_ZEDBSD_INPUT_H

#include <noct/beui.h>

#include <stddef.h>
#include <stdint.h>
#include <zedbsd/input.h>

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

void noct_beui_zedbsd_input_capabilities_clear(
    struct noct_beui_zedbsd_input_capabilities *capabilities);
void noct_beui_zedbsd_input_capabilities_set_event(
    struct noct_beui_zedbsd_input_capabilities *capabilities,
    unsigned event_type);
void noct_beui_zedbsd_input_capabilities_set_key(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned code);
void noct_beui_zedbsd_input_capabilities_set_relative(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis);
void noct_beui_zedbsd_input_capabilities_set_absolute(
    struct noct_beui_zedbsd_input_capabilities *capabilities, unsigned axis,
    const struct input_absinfo *information);
unsigned noct_beui_zedbsd_input_classify(
    const struct noct_beui_zedbsd_input_capabilities *capabilities);

void noct_beui_zedbsd_input_init(struct noct_beui_zedbsd_input *input,
				 unsigned display_width,
				 unsigned display_height);
void noct_beui_zedbsd_input_set_display(struct noct_beui_zedbsd_input *input,
					unsigned display_width,
					unsigned display_height);
void noct_beui_zedbsd_input_reset(struct noct_beui_zedbsd_input *input);
int noct_beui_zedbsd_input_attach(
    struct noct_beui_zedbsd_input *input,
    const struct noct_beui_zedbsd_input_capabilities *capabilities);
unsigned noct_beui_zedbsd_input_detach(struct noct_beui_zedbsd_input *input,
				       unsigned source_index);

/*
 * Feed any byte count read from one nonblocking event descriptor.  Complete
 * records are processed in order, while a trailing record is retained in the
 * source slot for the next call.  The returned flags describe visible state
 * changes and whether the OS adapter must perform a resynchronization query.
 */
unsigned noct_beui_zedbsd_input_feed(struct noct_beui_zedbsd_input *input,
				     unsigned source_index, const void *bytes,
				     size_t byte_count);

/*
 * Complete recovery after UPDATE_RESYNC.  key_bits may be NULL to retain the
 * visible all-up reset.  ABS information is optional; non-NULL values replace
 * the stored range and current raw coordinate.  key_byte_count is truncated
 * safely to the native evdev bitmap size.
 */
unsigned noct_beui_zedbsd_input_resync(struct noct_beui_zedbsd_input *input,
				       unsigned source_index,
				       const void *key_bits,
				       size_t key_byte_count,
				       const struct input_absinfo *absolute_x,
				       const struct input_absinfo *absolute_y);

int
noct_beui_zedbsd_input_is_key_down(const struct noct_beui_zedbsd_input *input,
				   int beui_key);
int noct_beui_zedbsd_input_poll_pointer(struct noct_beui_zedbsd_input *input,
					struct noct_beui_pointer_event *event);

#endif
