/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: Term.*  (non-standard API) -- Win32 console backend.
 *
 * Implements the target-neutral NoctTermBackend with the classic
 * Win32 Console API (no VT-mode dependency), so it works on every
 * desktop Windows from the NT line onward, including consoles where
 * ENABLE_VIRTUAL_TERMINAL_PROCESSING is unavailable.
 *
 *  - Output goes through WriteConsoleW; UTF-8 from the VM is
 *    converted to UTF-16 here. The console handles double-width CJK
 *    cells by itself.
 *  - write() interprets the small in-band SGR subset (ESC [ ... m)
 *    that callers may embed in row text (remacs uses reverse video
 *    in-band so a row diffs as one string); other escape sequences
 *    are swallowed.
 *  - Input uses ReadConsoleInputW and translates KEY_EVENT records
 *    into the Emacs-style event integers of the POSIX backend:
 *    Alt maps to META, Ctrl to CTRL, arrows and friends to the
 *    NOCT_TERM_KEY_* constants. AltGr (right Alt + left Ctrl) is
 *    plain character input, not Meta.
 *  - The session runs on a private screen buffer, which doubles as
 *    the alternate screen: closing restores the original console
 *    contents like the POSIX ?1049 sequence.
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(NOCT_TARGET_WINDOWS)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef COMMON_LVB_UNDERSCORE
#define COMMON_LVB_UNDERSCORE	0x8000
#endif

/* Decoded key events waiting to be delivered. */
#define EVENT_QUEUE_SIZE	64

/* Records pulled per ReadConsoleInputW call. */
#define READ_CHUNK		32

static struct {
	int open;
	HANDLE input;
	HANDLE screen;		/* Our private buffer while open. */
	HANDLE original;	/* The buffer that was active before. */
	DWORD saved_input_mode;
	int saved_mode_valid;

	WORD base_attr;		/* Attributes to reset to (SGR 0). */
	WORD cur_attr;		/* Current fg/bg/bold/underline bits. */
	int reverse;

	volatile int resized;

	int queue[EVENT_QUEUE_SIZE];
	int queue_head;
	int queue_len;
	WCHAR pending_high;	/* Pending high surrogate, or 0. */

	/* Ctrl+C delivered through the console ctrl handler (see
	 * ctrl_handler below); read on the read_key thread. */
	volatile LONG ctrl_c_count;
	HANDLE wake;		/* Signaled by the ctrl handler. */

	/* Used only to bracket blocking waits for the MT runtime. */
	NoctEnv *env;
} term;

/*
 * With ENABLE_PROCESSED_INPUT off, Ctrl+C is an ordinary key event on
 * a real Windows console and this handler stays dormant. Some hosts
 * (Wine's tty conhost among them) still route ^C through the console
 * ctrl event; translate it back into a C-c key so the binding works
 * everywhere. Runs on its own thread.
 */
static BOOL WINAPI
ctrl_handler(
	DWORD type)
{
	if (type == CTRL_C_EVENT && term.open) {
		InterlockedIncrement(&term.ctrl_c_count);
		if (term.wake != NULL)
			SetEvent(term.wake);
		return TRUE;
	}
	return FALSE;
}

/*
 * Event queue
 */

static void
queue_push(
	int ev)
{
	if (term.queue_len >= EVENT_QUEUE_SIZE)
		return;
	term.queue[(term.queue_head + term.queue_len) % EVENT_QUEUE_SIZE] = ev;
	term.queue_len++;
}

static int
queue_pop(void)
{
	int ev;

	if (term.queue_len == 0)
		return -1;
	ev = term.queue[term.queue_head];
	term.queue_head = (term.queue_head + 1) % EVENT_QUEUE_SIZE;
	term.queue_len--;
	return ev;
}

/*
 * Attributes
 */

/* ANSI color number (0-7) to console RGB bits: red and blue swap. */
static WORD
ansi_to_rgb_bits(
	int n)
{
	WORD w;

	w = 0;
	if (n & 1)
		w |= FOREGROUND_RED;
	if (n & 2)
		w |= FOREGROUND_GREEN;
	if (n & 4)
		w |= FOREGROUND_BLUE;
	return w;
}

/* Map a 256-color index to the 16-color console palette (as 0-15). */
static int
color256_to_16(
	int n)
{
	int r, g, b, bright, base;

	if (n < 0)
		return -1;
	if (n < 16)
		return n;
	if (n >= 232) {
		/* Grayscale ramp. */
		if (n < 238)
			return 0;
		if (n < 244)
			return 8;
		if (n < 250)
			return 7;
		return 15;
	}
	/* 6x6x6 cube. */
	n -= 16;
	r = n / 36;
	g = (n / 6) % 6;
	b = n % 6;
	bright = (r >= 4 || g >= 4 || b >= 4) ? 8 : 0;
	base = (r >= 2 ? 1 : 0) | (g >= 2 ? 2 : 0) | (b >= 2 ? 4 : 0);
	return base | bright;
}

/* Replace the foreground of an attribute word with 16-color index n. */
static WORD
attr_with_fg(
	WORD attr,
	int n)
{
	attr &= (WORD)~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
			FOREGROUND_INTENSITY);
	attr |= ansi_to_rgb_bits(n & 7);
	if (n & 8)
		attr |= FOREGROUND_INTENSITY;
	return attr;
}

static WORD
attr_with_bg(
	WORD attr,
	int n)
{
	attr &= (WORD)~(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE |
			BACKGROUND_INTENSITY);
	attr |= (WORD)(ansi_to_rgb_bits(n & 7) << 4);
	if (n & 8)
		attr |= BACKGROUND_INTENSITY;
	return attr;
}

/* The attribute actually programmed into the console. */
static WORD
effective_attr(void)
{
	WORD attr, fg, bg, rest;

	attr = term.cur_attr;
	if (!term.reverse)
		return attr;
	fg = attr & 0x0F;
	bg = (WORD)((attr >> 4) & 0x0F);
	rest = attr & (WORD)~0xFF;
	return (WORD)(rest | (fg << 4) | bg);
}

/*
 * Output
 */

static void
write_wide(
	const WCHAR *text,
	DWORD length)
{
	DWORD written;

	if (length == 0)
		return;
	SetConsoleTextAttribute(term.screen, effective_attr());
	WriteConsoleW(term.screen, text, length, &written, NULL);
}

/* Write one UTF-8 run (no escapes) to the console. */
static void
write_utf8_run(
	const char *utf8,
	int length)
{
	WCHAR stack_buf[256];
	WCHAR *wide;
	int wide_len;

	if (length <= 0)
		return;
	wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8, length, NULL, 0);
	if (wide_len <= 0)
		return;
	if (wide_len <= (int)(sizeof(stack_buf) / sizeof(stack_buf[0]))) {
		wide = stack_buf;
	} else {
		wide = malloc((size_t)wide_len * sizeof(WCHAR));
		if (wide == NULL)
			return;
	}
	MultiByteToWideChar(CP_UTF8, 0, utf8, length, wide, wide_len);
	write_wide(wide, (DWORD)wide_len);
	if (wide != stack_buf)
		free(wide);
}

/* Apply one SGR parameter to the current attributes. */
static void
apply_sgr(
	const int *params,
	int nparam)
{
	int i, n, c;

	if (nparam == 0) {
		term.cur_attr = term.base_attr;
		term.reverse = 0;
		return;
	}
	for (i = 0; i < nparam; i++) {
		n = params[i];
		if (n == 0) {
			term.cur_attr = term.base_attr;
			term.reverse = 0;
		} else if (n == 1) {
			term.cur_attr |= FOREGROUND_INTENSITY;
		} else if (n == 22) {
			term.cur_attr &= (WORD)~FOREGROUND_INTENSITY;
		} else if (n == 4) {
			term.cur_attr |= COMMON_LVB_UNDERSCORE;
		} else if (n == 24) {
			term.cur_attr &= (WORD)~COMMON_LVB_UNDERSCORE;
		} else if (n == 7) {
			term.reverse = 1;
		} else if (n == 27) {
			term.reverse = 0;
		} else if (n >= 30 && n <= 37) {
			term.cur_attr = attr_with_fg(term.cur_attr, n - 30);
		} else if (n >= 90 && n <= 97) {
			term.cur_attr = attr_with_fg(term.cur_attr, (n - 90) | 8);
		} else if (n == 39) {
			term.cur_attr = (WORD)((term.cur_attr & (WORD)~0x0F) |
					       (term.base_attr & 0x0F));
		} else if (n >= 40 && n <= 47) {
			term.cur_attr = attr_with_bg(term.cur_attr, n - 40);
		} else if (n >= 100 && n <= 107) {
			term.cur_attr = attr_with_bg(term.cur_attr, (n - 100) | 8);
		} else if (n == 49) {
			term.cur_attr = (WORD)((term.cur_attr & (WORD)~0xF0) |
					       (term.base_attr & 0xF0));
		} else if ((n == 38 || n == 48) && i + 2 < nparam &&
			   params[i + 1] == 5) {
			c = color256_to_16(params[i + 2]);
			if (c >= 0) {
				if (n == 38)
					term.cur_attr = attr_with_fg(term.cur_attr, c);
				else
					term.cur_attr = attr_with_bg(term.cur_attr, c);
			}
			i += 2;
		}
	}
}

/*
 * Backend operations
 */

static int
win32_is_tty(
	void *context)
{
	DWORD mode;
	HANDLE in, out;

	(void)context;
	in = GetStdHandle(STD_INPUT_HANDLE);
	out = GetStdHandle(STD_OUTPUT_HANDLE);
	if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE)
		return 0;
	if (!GetConsoleMode(in, &mode) || !GetConsoleMode(out, &mode))
		return 0;
	return 1;
}

static void win32_close(void *context);

static int
win32_open(
	void *context)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD size;
	SMALL_RECT window;
	DWORD mode;

	(void)context;
	if (term.open)
		return 1;
	if (!win32_is_tty(NULL))
		return 0;

	term.input = GetStdHandle(STD_INPUT_HANDLE);
	term.original = GetStdHandle(STD_OUTPUT_HANDLE);

	if (GetConsoleMode(term.input, &term.saved_input_mode))
		term.saved_mode_valid = 1;
	/* Raw keys: no line input, no echo, no ^C signal; window resize
	 * events on; quick-edit off so a stray click cannot freeze us. */
	mode = ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
	SetConsoleMode(term.input, mode);

	if (!GetConsoleScreenBufferInfo(term.original, &info))
		return 0;
	size.X = (SHORT)(info.srWindow.Right - info.srWindow.Left + 1);
	size.Y = (SHORT)(info.srWindow.Bottom - info.srWindow.Top + 1);

	term.screen = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, CONSOLE_TEXTMODE_BUFFER,
						NULL);
	if (term.screen == INVALID_HANDLE_VALUE) {
		if (term.saved_mode_valid)
			SetConsoleMode(term.input, term.saved_input_mode);
		return 0;
	}

	/* Shrink the fresh buffer to the visible window so that buffer
	 * coordinates are screen coordinates and there is no scrollback.
	 * Window first or size first depends on which is larger; doing
	 * both orders tolerates either. */
	window.Left = 0;
	window.Top = 0;
	window.Right = (SHORT)(size.X - 1);
	window.Bottom = (SHORT)(size.Y - 1);
	SetConsoleWindowInfo(term.screen, TRUE, &window);
	SetConsoleScreenBufferSize(term.screen, size);
	SetConsoleWindowInfo(term.screen, TRUE, &window);

	term.base_attr = (WORD)(info.wAttributes & 0xFF);
	if ((term.base_attr & 0x0F) == 0)
		term.base_attr = (WORD)((term.base_attr & 0xF0) | 0x07);
	term.cur_attr = term.base_attr;
	term.reverse = 0;
	term.queue_head = 0;
	term.queue_len = 0;
	term.pending_high = 0;
	term.resized = 0;
	term.ctrl_c_count = 0;
	if (term.wake == NULL)
		term.wake = CreateEventW(NULL, FALSE, FALSE, NULL);
	SetConsoleCtrlHandler(ctrl_handler, TRUE);

	SetConsoleActiveScreenBuffer(term.screen);
	term.open = 1;
	return 1;
}

static void
win32_close(
	void *context)
{
	(void)context;
	if (!term.open)
		return;
	SetConsoleCtrlHandler(ctrl_handler, FALSE);
	SetConsoleActiveScreenBuffer(term.original);
	CloseHandle(term.screen);
	term.screen = INVALID_HANDLE_VALUE;
	if (term.saved_mode_valid)
		SetConsoleMode(term.input, term.saved_input_mode);
	term.open = 0;
}

static int
win32_size(
	void *context,
	unsigned *rows,
	unsigned *columns)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE target;

	(void)context;
	target = term.open ? term.screen : GetStdHandle(STD_OUTPUT_HANDLE);
	if (!GetConsoleScreenBufferInfo(target, &info))
		return 0;
	*rows = (unsigned)(info.srWindow.Bottom - info.srWindow.Top + 1);
	*columns = (unsigned)(info.srWindow.Right - info.srWindow.Left + 1);
	return 1;
}

static int
win32_resized(
	void *context)
{
	int v;

	(void)context;
	v = term.resized ? 1 : 0;
	term.resized = 0;
	return v;
}

static int
win32_move_to(
	void *context,
	unsigned row,
	unsigned column)
{
	COORD pos;

	(void)context;
	if (!term.open)
		return 0;
	pos.X = (SHORT)(column - 1);
	pos.Y = (SHORT)(row - 1);
	SetConsoleCursorPosition(term.screen, pos);
	return 1;
}

static int
win32_write(
	void *context,
	const char *utf8,
	size_t length)
{
	size_t i, run_start;
	int params[8];
	int nparam, acc, has_digit;
	unsigned char c;

	(void)context;
	if (!term.open)
		return 0;

	/* Copy out plain runs; interpret ESC [ ... m in-band. */
	i = 0;
	run_start = 0;
	while (i < length) {
		if ((unsigned char)utf8[i] != 0x1B) {
			i++;
			continue;
		}
		write_utf8_run(utf8 + run_start, (int)(i - run_start));
		if (i + 1 < length && utf8[i + 1] == '[') {
			nparam = 0;
			acc = 0;
			has_digit = 0;
			i += 2;
			while (i < length) {
				c = (unsigned char)utf8[i];
				if (c >= '0' && c <= '9') {
					acc = acc * 10 + (c - '0');
					has_digit = 1;
					i++;
					continue;
				}
				if (c == ';') {
					if (nparam < 8)
						params[nparam++] =
							has_digit ? acc : 0;
					acc = 0;
					has_digit = 0;
					i++;
					continue;
				}
				break;
			}
			if (has_digit && nparam < 8)
				params[nparam++] = acc;
			if (i < length) {
				if (utf8[i] == 'm')
					apply_sgr(params, nparam);
				/* Other final bytes: swallow. */
				i++;
			}
		} else {
			/* Lone ESC or a non-CSI sequence: drop the ESC. */
			i++;
		}
		run_start = i;
	}
	write_utf8_run(utf8 + run_start, (int)(i - run_start));
	return 1;
}

static int
win32_clear(
	void *context)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	COORD home;
	DWORD cells, written;

	(void)context;
	if (!term.open)
		return 0;
	if (!GetConsoleScreenBufferInfo(term.screen, &info))
		return 0;
	home.X = 0;
	home.Y = 0;
	cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
	FillConsoleOutputCharacterW(term.screen, L' ', cells, home, &written);
	FillConsoleOutputAttribute(term.screen, effective_attr(), cells, home,
				   &written);
	SetConsoleCursorPosition(term.screen, home);
	return 1;
}

static int
win32_clear_to_eol(
	void *context)
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	DWORD cells, written;

	(void)context;
	if (!term.open)
		return 0;
	if (!GetConsoleScreenBufferInfo(term.screen, &info))
		return 0;
	cells = (DWORD)(info.dwSize.X - info.dwCursorPosition.X);
	FillConsoleOutputCharacterW(term.screen, L' ', cells,
				    info.dwCursorPosition, &written);
	FillConsoleOutputAttribute(term.screen, effective_attr(), cells,
				   info.dwCursorPosition, &written);
	return 1;
}

static int
win32_set_style(
	void *context,
	const struct NoctTermStyle *style)
{
	int c;

	(void)context;
	term.cur_attr = term.base_attr;
	term.reverse = style->reverse ? 1 : 0;
	if (style->bold)
		term.cur_attr |= FOREGROUND_INTENSITY;
	if (style->underline)
		term.cur_attr |= COMMON_LVB_UNDERSCORE;
	c = color256_to_16(style->foreground);
	if (c >= 0)
		term.cur_attr = attr_with_fg(term.cur_attr, c);
	c = color256_to_16(style->background);
	if (c >= 0)
		term.cur_attr = attr_with_bg(term.cur_attr, c);
	return 1;
}

static int
win32_show_cursor(
	void *context,
	int visible)
{
	CONSOLE_CURSOR_INFO cursor;

	(void)context;
	if (!term.open)
		return 0;
	if (!GetConsoleCursorInfo(term.screen, &cursor))
		return 0;
	cursor.bVisible = visible ? TRUE : FALSE;
	SetConsoleCursorInfo(term.screen, &cursor);
	return 1;
}

static int
win32_flush(
	void *context)
{
	/* Output is unbuffered: WriteConsoleW takes effect immediately. */
	(void)context;
	return 1;
}

/*
 * Input
 */

/* Special (non-character) virtual keys, or -1. */
static int
vk_to_special(
	WORD vk)
{
	switch (vk) {
	case VK_UP:	return NOCT_TERM_KEY_UP;
	case VK_DOWN:	return NOCT_TERM_KEY_DOWN;
	case VK_RIGHT:	return NOCT_TERM_KEY_RIGHT;
	case VK_LEFT:	return NOCT_TERM_KEY_LEFT;
	case VK_HOME:	return NOCT_TERM_KEY_HOME;
	case VK_END:	return NOCT_TERM_KEY_END;
	case VK_PRIOR:	return NOCT_TERM_KEY_PGUP;
	case VK_NEXT:	return NOCT_TERM_KEY_PGDN;
	case VK_INSERT:	return NOCT_TERM_KEY_INSERT;
	case VK_DELETE:	return NOCT_TERM_KEY_DELETE;
	default:
		if (vk >= VK_F1 && vk <= VK_F12)
			return NOCT_TERM_KEY_F1 + (vk - VK_F1);
		return -1;
	}
}

/* Translate one KEY_EVENT record into events on the queue. */
static void
process_key_event(
	const KEY_EVENT_RECORD *key)
{
	DWORD state;
	int alt, ctrl, shift, altgr;
	int special, ev, repeat, i;
	unsigned int ch;

	state = key->dwControlKeyState;
	alt = (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
	ctrl = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
	shift = (state & SHIFT_PRESSED) != 0;
	/* AltGr arrives as right-Alt + left-Ctrl and produces plain
	 * characters on European/JP layouts. */
	altgr = (state & RIGHT_ALT_PRESSED) != 0 &&
		(state & LEFT_CTRL_PRESSED) != 0 &&
		key->uChar.UnicodeChar != 0;

	ch = (unsigned int)key->uChar.UnicodeChar;

	if (!key->bKeyDown) {
		/* Alt+numpad input is delivered on the Alt key-up. */
		if (key->wVirtualKeyCode == VK_MENU && ch != 0)
			queue_push((int)ch);
		return;
	}

	/* Pure modifier presses. */
	switch (key->wVirtualKeyCode) {
	case VK_SHIFT:
	case VK_CONTROL:
	case VK_MENU:
	case VK_LWIN:
	case VK_RWIN:
	case VK_CAPITAL:
	case VK_NUMLOCK:
	case VK_SCROLL:
		return;
	default:
		break;
	}

	repeat = key->wRepeatCount > 0 ? key->wRepeatCount : 1;
	ev = -1;

	special = vk_to_special(key->wVirtualKeyCode);
	if (special >= 0) {
		ev = special;
		if (alt)
			ev |= NOCT_TERM_MOD_META;
		if (ctrl)
			ev |= NOCT_TERM_MOD_CTRL;
		if (shift)
			ev |= NOCT_TERM_MOD_SHIFT;
	} else if (key->wVirtualKeyCode == VK_BACK) {
		/* The Backspace key is DEL, like the POSIX backend. */
		ev = 0x7F;
		if (alt)
			ev |= NOCT_TERM_MOD_META;
	} else if (ch == 0) {
		/* No translated character (e.g. Alt/Ctrl chords the
		 * console does not cook). Derive from the virtual key. */
		if (alt || ctrl) {
			unsigned int c;

			c = MapVirtualKeyW(key->wVirtualKeyCode,
					   MAPVK_VK_TO_CHAR) & 0x7FFFFFFF;
			if (c >= 'A' && c <= 'Z' && !shift)
				c = c - 'A' + 'a';
			if (c >= 0x20) {
				ev = (int)c;
				if (alt)
					ev |= NOCT_TERM_MOD_META;
				if (ctrl)
					ev |= NOCT_TERM_MOD_CTRL;
			}
		}
	} else if (altgr) {
		ev = (int)ch;
	} else if (ch < 0x20) {
		/* Control characters, following the POSIX decoding. */
		if (ch == 0x09 && !ctrl) {
			ev = '\t';
		} else if (ch == 0x0D && !ctrl) {
			ev = '\r';
		} else if (ch == 0x1B) {
			ev = 0x1B;
		} else if (ch == 0x00) {
			ev = NOCT_TERM_MOD_CTRL | 0x20;	/* C-SPC / C-@ */
		} else if (ch == 0x0A) {
			ev = NOCT_TERM_MOD_CTRL | 'j';
		} else if (ch <= 0x1A) {
			ev = NOCT_TERM_MOD_CTRL | (int)(ch + 0x60);
		} else {
			ev = NOCT_TERM_MOD_CTRL | (int)(ch + 0x40);
		}
		if (ev >= 0 && alt)
			ev |= NOCT_TERM_MOD_META;
	} else if (ch >= 0xD800 && ch < 0xDC00) {
		term.pending_high = (WCHAR)ch;
		return;
	} else if (ch >= 0xDC00 && ch < 0xE000) {
		if (term.pending_high != 0) {
			ev = 0x10000 +
			     (((int)term.pending_high - 0xD800) << 10) +
			     ((int)ch - 0xDC00);
			term.pending_high = 0;
		}
	} else {
		/* Printable. Ctrl+digit/symbol cooks to the plain char
		 * with the Ctrl flag set; keep the modifier. Shift is
		 * already folded into the character. */
		ev = (int)ch;
		if (ch == 0x20 && ctrl)
			ev |= NOCT_TERM_MOD_CTRL;
		else if (ctrl)
			ev |= NOCT_TERM_MOD_CTRL;
		if (alt)
			ev |= NOCT_TERM_MOD_META;
	}

	if (ev < 0)
		return;
	for (i = 0; i < repeat; i++)
		queue_push(ev);
}

/* Drain everything the console currently has into the queue. */
static void
drain_input(void)
{
	INPUT_RECORD records[READ_CHUNK];
	DWORD count, avail, i;

	for (;;) {
		if (!GetNumberOfConsoleInputEvents(term.input, &avail) ||
		    avail == 0)
			return;
		if (!ReadConsoleInputW(term.input, records, READ_CHUNK, &count))
			return;
		for (i = 0; i < count; i++) {
			if (records[i].EventType == KEY_EVENT)
				process_key_event(&records[i].Event.KeyEvent);
			else if (records[i].EventType ==
				 WINDOW_BUFFER_SIZE_EVENT)
				term.resized = 1;
		}
		if (count < READ_CHUNK)
			return;
	}
}

static int
win32_read_key(
	void *context,
	int timeout_ms)
{
	DWORD start, elapsed, wait, wait_ret;
	int ev;

	(void)context;
	if (!term.open)
		return -1;

	start = GetTickCount();
	for (;;) {
		{
			LONG pending_c = InterlockedExchange(&term.ctrl_c_count, 0);
			while (pending_c-- > 0)
				queue_push(NOCT_TERM_MOD_CTRL | 'c');
		}

		ev = queue_pop();
		if (ev >= 0)
			return ev;

		if (timeout_ms < 0) {
			wait = INFINITE;
		} else {
			elapsed = GetTickCount() - start;
			if (elapsed > (DWORD)timeout_ms)
				return -1;
			wait = (DWORD)timeout_ms - elapsed;
		}

		if (term.env != NULL)
			noct_enter_blocking(term.env);
		{
			HANDLE handles[2];

			handles[0] = term.input;
			handles[1] = term.wake;
			wait_ret = WaitForMultipleObjects(
				term.wake != NULL ? 2 : 1, handles, FALSE,
				wait);
		}
		if (term.env != NULL)
			noct_leave_blocking(term.env);

		if (wait_ret == WAIT_OBJECT_0)
			drain_input();
		else if (wait_ret != WAIT_OBJECT_0 + 1)
			return -1;
		/* A resize (or a swallowed event) may leave the queue
		 * empty; loop and re-check the deadline. */
	}
}

static int
win32_pending_input(
	void *context)
{
	(void)context;
	if (!term.open)
		return 0;
	if (term.ctrl_c_count > 0)
		return 1;
	if (term.queue_len > 0)
		return 1;
	drain_input();
	return term.queue_len > 0 ? 1 : 0;
}

/*
 * Registration
 */

static const struct NoctTermBackend win32_backend = {
	win32_open,
	win32_close,
	win32_is_tty,
	win32_size,
	win32_resized,
	win32_move_to,
	win32_write,
	win32_clear,
	win32_clear_to_eol,
	win32_set_style,
	win32_show_cursor,
	win32_flush,
	win32_read_key,
	win32_pending_input
};

/* Restore the console; safe to call more than once. */
static void
term_restore(void)
{
	win32_close(NULL);
}

NOCT_DLL
bool
noct_register_api_term(
	NoctEnv *env)
{
	term.env = env;
	if (!noct_register_api_term_backend(env, &win32_backend, NULL))
		return false;
	atexit(term_restore);
	return true;
}

#else

/* Not Windows: this translation unit is empty. */
typedef int noct_api_term_win32_unused;

#endif /* NOCT_TARGET_WINDOWS */
