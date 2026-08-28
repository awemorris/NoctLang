/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * BeUI backend for MS-DOS on the NEC PC-9800 series (DOS/4GW).
 *
 * The GDC display and CGROM glyph drivers are the compiler-neutral cores
 * shared with Boots; this file supplies what the pre-boot environment got
 * from its Stage 1 BIOS gateway.  Under DOS the low megabyte is mapped
 * linear-to-physical and interrupts stay enabled, so the BIOS work areas
 * are read directly and the display mode calls go through INT 18h, which
 * DOS/4GW reflects to real mode.
 *
 * The millisecond clock must not touch i8253 channel 0: DOS timekeeping
 * owns it.  Channel 1 only feeds the beeper, whose speaker gate is
 * separate from the counter, so the clock programs channel 1 as a
 * free-running rate generator and accumulates latched deltas exactly
 * like the Boots Stage 2 timer.  A machine whose channel 1 gate is held
 * off falls back to latch-polling channel 0 with a measured reload; in
 * mode 3 the fraction then jitters by half a period, which the BeUI
 * clock contract tolerates.  DOS beeps while BeUI is open may glitch
 * the fallback-free path; DOS reprograms the channel on its next beep.
 */

#if defined(NOCT_TARGET_PC98DOS)

#include <noct/noct.h>
#include <noct/beui.h>

#include "beui-pc98-auto.h"

#include <conio.h>
#include <i86.h>
#include <string.h>

/* i8253 system timer. */
#define PIT_COUNTER1 0x73U
#define PIT_CONTROL 0x77U
/* Channel 1, low/high byte access, mode 2, binary. */
#define PIT_PROGRAM_CH1 0x74U
#define PIT_LATCH_CH1 0x40U
#define PIT_LATCH_CH0 0x00U
#define PIT_COUNTER0 0x71U

#define TICKS_PER_5MS_2457600HZ 12288U
#define TICKS_PER_5MS_1996800HZ 9984U

/* BIOS work area. */
#define BIOS_SYSTEM_CLOCK_FLAG 0x501U
#define BIOS_KEY_STATE_TABLE 0x52aU

/* The Core-Graph aperture the Cirrus backend expects at board level. */
#define CIRRUS_PHYSICAL_APERTURE 0xf0000000UL

static struct noct_beui_pc98_auto auto_backend;
static struct noct_beui_hal pc98dos_hal;

static struct {
	int initialized;
	int use_channel0;
	uint16_t reload;
	uint16_t last_count;
	uint64_t ticks;
	uint32_t ticks_per_5ms;
} clock_state;

static uint8_t
port_in8(void *context, uint16_t port)
{
	(void)context;
	return (uint8_t)inp(port);
}

static void
port_out8(void *context, uint16_t port, uint8_t value)
{
	(void)context;
	outp(port, value);
}

static uint8_t
read_low_byte(uint32_t address)
{
	return *(volatile uint8_t *)address;
}

/* INT 18h AH=42h/CH=C0h selects the 640x400 display region; AH=40h shows
 * graphics.  AH=41h hides graphics again for the DOS prompt. */
static int
display_reset(void *context)
{
	union REGS regs;

	(void)context;
	memset(&regs, 0, sizeof(regs));
	regs.h.ah = 0x42;
	regs.h.ch = 0xc0;
	int386(0x18, &regs, &regs);
	memset(&regs, 0, sizeof(regs));
	regs.h.ah = 0x40;
	int386(0x18, &regs, &regs);
	return 1;
}

static int
display_stop(void *context)
{
	union REGS regs;

	(void)context;
	memset(&regs, 0, sizeof(regs));
	regs.h.ah = 0x41;
	int386(0x18, &regs, &regs);
	return 1;
}

/* ------------------------------------------------------------------- */
/* Millisecond clock.                                                  */

static uint16_t
latch_counter(uint8_t latch_command, uint16_t data_port)
{
	uint16_t value;

	_disable();
	outp(PIT_CONTROL, latch_command);
	value = (uint16_t)inp(data_port);
	value = (uint16_t)(value | ((uint16_t)inp(data_port) << 8));
	_enable();
	return value;
}

static int
counter_is_running(uint8_t latch_command, uint16_t data_port)
{
	uint16_t first = latch_counter(latch_command, data_port);
	unsigned spins;

	for (spins = 0; spins < 10000U; spins++)
		if (latch_counter(latch_command, data_port) != first)
			return 1;
	return 0;
}

static void
clock_start(void)
{
	clock_state.ticks_per_5ms =
		(read_low_byte(BIOS_SYSTEM_CLOCK_FLAG) & 0x80U) != 0 ?
		TICKS_PER_5MS_1996800HZ : TICKS_PER_5MS_2457600HZ;
	clock_state.ticks = 0;
	/* Preferred source: channel 1 as a free-running rate generator. */
	outp(PIT_CONTROL, PIT_PROGRAM_CH1);
	outp(PIT_COUNTER1, 0x00);
	outp(PIT_COUNTER1, 0x00);
	if (counter_is_running(PIT_LATCH_CH1, PIT_COUNTER1)) {
		clock_state.use_channel0 = 0;
		clock_state.reload = 0; /* 65536 */
		clock_state.last_count = 0;
	} else {
		/* Gate held off: fall back to the DOS system counter with a
		 * measured reload.  The maximum latched value across several
		 * periods approximates the reload. */
		uint16_t maximum = 0;
		unsigned spins;

		for (spins = 0; spins < 200000U; spins++) {
			uint16_t value = latch_counter(PIT_LATCH_CH0,
						       PIT_COUNTER0);

			if (value > maximum)
				maximum = value;
		}
		clock_state.use_channel0 = 1;
		clock_state.reload = (uint16_t)(maximum + 1U);
		clock_state.last_count = latch_counter(PIT_LATCH_CH0,
						       PIT_COUNTER0);
	}
	clock_state.initialized = 1;
}

static uint64_t
clock_milliseconds(void *context)
{
	uint32_t period;
	uint16_t count;
	uint16_t delta;

	(void)context;
	if (!clock_state.initialized) {
		clock_start();
		return 0;
	}
	if (clock_state.use_channel0) {
		period = clock_state.reload != 0 ? clock_state.reload : 65536U;
		count = latch_counter(PIT_LATCH_CH0, PIT_COUNTER0);
		if (count <= clock_state.last_count)
			delta = (uint16_t)(clock_state.last_count - count);
		else
			delta = (uint16_t)(clock_state.last_count +
					   (period - count));
	} else {
		count = latch_counter(PIT_LATCH_CH1, PIT_COUNTER1);
		delta = (uint16_t)(clock_state.last_count - count);
	}
	clock_state.ticks += delta;
	clock_state.last_count = count;
	return clock_state.ticks * 5U / clock_state.ticks_per_5ms;
}

/* ------------------------------------------------------------------- */
/* Key state and type-ahead drain.                                     */

/* Normalized key code to PC-98 scan code (group * 8 + bit) for the BIOS
 * real-time key state table.  Letters use their lowercase codes. */
static int
key_to_scan(int key)
{
	static const uint8_t letters[26] = {
		0x1d, 0x2d, 0x2b, 0x1f, 0x12, 0x20, 0x21, 0x22, 0x17,
		0x23, 0x24, 0x25, 0x2f, 0x2e, 0x18, 0x19, 0x10, 0x13,
		0x1e, 0x14, 0x15, 0x2c, 0x11, 0x2a, 0x16, 0x29,
	};

	if (key >= 'a' && key <= 'z')
		return letters[key - 'a'];
	if (key >= '1' && key <= '9')
		return 0x01 + (key - '1');
	switch (key) {
	case '0': return 0x0a;
	case ' ': return 0x34;
	case NOCT_BEUI_KEY_ESCAPE: return 0x00;
	case NOCT_BEUI_KEY_TAB: return 0x0f;
	case NOCT_BEUI_KEY_ENTER: return 0x1c;
	case NOCT_BEUI_KEY_BACKSPACE: return 0x0e;
	case NOCT_BEUI_KEY_INSERT: return 0x38;
	case NOCT_BEUI_KEY_DELETE: return 0x39;
	case NOCT_BEUI_KEY_UP: return 0x3a;
	case NOCT_BEUI_KEY_LEFT: return 0x3b;
	case NOCT_BEUI_KEY_RIGHT: return 0x3c;
	case NOCT_BEUI_KEY_DOWN: return 0x3d;
	case NOCT_BEUI_KEY_HOME: return 0x3e;
	case NOCT_BEUI_KEY_PAGE_UP: return 0x36;
	case NOCT_BEUI_KEY_PAGE_DOWN: return 0x37;
	case NOCT_BEUI_KEY_SHIFT: return 0x70;
	default: return -1;
	}
}

static int
input_is_key_down(void *context, int key)
{
	int scan = key_to_scan(key);
	uint8_t bits;

	(void)context;
	if (scan < 0)
		return -1;
	/* The ROM keyboard handler keeps a 16-byte bitmap of pressed keys
	 * at 0000:052Ah; with interrupts enabled it is always current. */
	bits = read_low_byte(BIOS_KEY_STATE_TABLE + ((unsigned)scan >> 3));
	return (bits >> (scan & 7)) & 1;
}

static void
input_drain(void *context)
{
	(void)context;
	while (kbhit())
		(void)getch();
}

/* ------------------------------------------------------------------- */
/* Core-Graph aperture.                                                */

/*
 * Protected mode under DOS/4GW does not map the board aperture, so ask
 * DPMI (INT 31h, AX=0800h "Physical Address Mapping") for a linear view
 * of it.  DOS/4GW runs a zero-based flat model, so the linear address it
 * returns is usable as a near pointer.  A machine without the board — or
 * a DPMI host that refuses the mapping — yields NULL, and the display
 * selector then falls back to the GDC.
 */
static volatile uint8_t *
map_cirrus_aperture(void)
{
	union REGS regs;
	unsigned long size = NOCT_BEUI_CIRRUS_VISIBLE_BYTES;
	unsigned long linear;

	memset(&regs, 0, sizeof(regs));
	regs.w.ax = 0x0800;
	regs.w.bx = (unsigned short)(CIRRUS_PHYSICAL_APERTURE >> 16);
	regs.w.cx = (unsigned short)(CIRRUS_PHYSICAL_APERTURE & 0xffff);
	regs.w.si = (unsigned short)(size >> 16);
	regs.w.di = (unsigned short)(size & 0xffff);
	int386(0x31, &regs, &regs);
	if (regs.w.cflag != 0)
		return NULL;
	linear = ((unsigned long)regs.w.bx << 16) | regs.w.cx;
	if (linear == 0)
		return NULL;
	return (volatile uint8_t *)linear;
}

/* ------------------------------------------------------------------- */

NOCT_DLL
bool
noct_register_api_beui_pc98dos(NoctEnv *env)
{
	memset(&pc98dos_hal, 0, sizeof(pc98dos_hal));
	noct_beui_pc98_auto_default(&auto_backend, display_reset, display_stop,
				    NULL, port_in8, port_out8, NULL,
				    map_cirrus_aperture());
	if (!noct_beui_pc98_auto_make_hal(&pc98dos_hal, &auto_backend))
		return false;
	pc98dos_hal.clock.context = NULL;
	pc98dos_hal.clock.milliseconds = clock_milliseconds;
	pc98dos_hal.input.context = NULL;
	pc98dos_hal.input.is_key_down = input_is_key_down;
	pc98dos_hal.input.drain = input_drain;
	return noct_register_api_beui_with_hal(env, &pc98dos_hal);
}

#endif /* NOCT_TARGET_PC98DOS */
