/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * API: Term.*  (non-standard API)
 *
 * A terminal abstraction for writing full-screen console programs.
 *
 * A direct ANSI/VT100 implementation with no curses dependency.
 *
 *  - Output is buffered; Term.flush() writes the whole frame in one
 *    write(2). Redisplay builds a frame and flushes once.
 *  - Input is decoded by a state machine into key events. A key event
 *    is an integer in the GNU Emacs representation: the Unicode
 *    codepoint (or a special-key constant in the private use area)
 *    OR-ed with modifier bits (meta = 1<<27, control = 1<<26,
 *    shift = 1<<25).
 *  - Meta is the ESC prefix (metaSendsEscape). 8-bit meta is not
 *    supported: it is ambiguous with UTF-8 lead bytes.
 *
 * The interface deliberately exposes no escape sequences, so other
 * backends (Win32 console, DOS text mode, a GUI) can replace this
 * implementation without touching callers. Non-POSIX builds get a
 * stub in which Term.isTTY() returns 0 and Term.open() fails.
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(NOCT_TARGET_POSIX)
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <sys/ioctl.h>
#endif

#if !defined(NOCT_TARGET_POSIX)

/*
 * Stub for platforms without a Term backend yet.
 */

NOCT_DLL
bool
noct_register_api_term(
	NoctEnv *env)
{
	NoctValue term_dict;

	memset(&term_dict, 0, sizeof(term_dict));
	noct_pin_local(env, 1, &term_dict);
	if (!noct_make_empty_dict(env, &term_dict))
		return false;
	if (!noct_set_global(env, "Term", &term_dict))
		return false;
	noct_unpin_local(env, 1, &term_dict);
	return true;
}

#else /* NOCT_TARGET_POSIX */

/*
 * Key event encoding. (Emacs-compatible bit layout)
 */
#define MOD_META	(1 << 27)
#define MOD_CTRL	(1 << 26)
#define MOD_SHIFT	(1 << 25)

/* Special keys live in the Unicode private use area. */
#define KEY_BASE	0xE000
#define KEY_UP		(KEY_BASE + 0)
#define KEY_DOWN	(KEY_BASE + 1)
#define KEY_RIGHT	(KEY_BASE + 2)
#define KEY_LEFT	(KEY_BASE + 3)
#define KEY_HOME	(KEY_BASE + 4)
#define KEY_END		(KEY_BASE + 5)
#define KEY_PGUP	(KEY_BASE + 6)
#define KEY_PGDN	(KEY_BASE + 7)
#define KEY_INSERT	(KEY_BASE + 8)
#define KEY_DELETE	(KEY_BASE + 9)
#define KEY_F1		(KEY_BASE + 11)	/* F1..F12 are KEY_F1 + n */

/* ESC disambiguation timeout. */
#define ESC_TIMEOUT_MS	50

/*
 * Terminal state.
 */
static struct {
	bool open;
	struct termios saved;
	bool saved_valid;

	/* Output frame buffer. */
	char *out;
	size_t out_len;
	size_t out_alloc;

	/* Input ring. */
	unsigned char in[256];
	size_t in_head;
	size_t in_len;

	/* SIGWINCH flag. */
	volatile sig_atomic_t resized;
} term;

/*
 * Registration table
 */

static bool cfunc_Term_open(NoctEnv *env);
static bool cfunc_Term_close(NoctEnv *env);
static bool cfunc_Term_isTTY(NoctEnv *env);
static bool cfunc_Term_size(NoctEnv *env);
static bool cfunc_Term_resized(NoctEnv *env);
static bool cfunc_Term_moveTo(NoctEnv *env);
static bool cfunc_Term_write(NoctEnv *env);
static bool cfunc_Term_clear(NoctEnv *env);
static bool cfunc_Term_clearToEol(NoctEnv *env);
static bool cfunc_Term_setStyle(NoctEnv *env);
static bool cfunc_Term_showCursor(NoctEnv *env);
static bool cfunc_Term_flush(NoctEnv *env);
static bool cfunc_Term_readKey(NoctEnv *env);
static bool cfunc_Term_pendingInput(NoctEnv *env);

struct term_ffi_item {
	const char *global_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};
static struct term_ffi_item term_ffi_items[] = {
	{"Term.open",		"open",		0, {NULL},		cfunc_Term_open},
	{"Term.close",		"close",	0, {NULL},		cfunc_Term_close},
	{"Term.isTTY",		"isTTY",	0, {NULL},		cfunc_Term_isTTY},
	{"Term.size",		"size",		0, {NULL},		cfunc_Term_size},
	{"Term.resized",	"resized",	0, {NULL},		cfunc_Term_resized},
	{"Term.moveTo",		"moveTo",	2, {"row", "col"},	cfunc_Term_moveTo},
	{"Term.write",		"write",	1, {"text"},		cfunc_Term_write},
	{"Term.clear",		"clear",	0, {NULL},		cfunc_Term_clear},
	{"Term.clearToEol",	"clearToEol",	0, {NULL},		cfunc_Term_clearToEol},
	{"Term.setStyle",	"setStyle",	1, {"style"},		cfunc_Term_setStyle},
	{"Term.showCursor",	"showCursor",	1, {"visible"},		cfunc_Term_showCursor},
	{"Term.flush",		"flush",	0, {NULL},		cfunc_Term_flush},
	{"Term.readKey",	"readKey",	1, {"timeoutMs"},	cfunc_Term_readKey},
	{"Term.pendingInput",	"pendingInput",	0, {NULL},		cfunc_Term_pendingInput},
};

/* Key constants exported on the Term dictionary. */
struct term_const {
	const char *name;
	int value;
};
static struct term_const term_consts[] = {
	{"META",	MOD_META},
	{"CTRL",	MOD_CTRL},
	{"SHIFT",	MOD_SHIFT},
	{"KEY_UP",	KEY_UP},
	{"KEY_DOWN",	KEY_DOWN},
	{"KEY_RIGHT",	KEY_RIGHT},
	{"KEY_LEFT",	KEY_LEFT},
	{"KEY_HOME",	KEY_HOME},
	{"KEY_END",	KEY_END},
	{"KEY_PGUP",	KEY_PGUP},
	{"KEY_PGDN",	KEY_PGDN},
	{"KEY_INSERT",	KEY_INSERT},
	{"KEY_DELETE",	KEY_DELETE},
	{"KEY_F1",	KEY_F1},
	{"KEY_TAB",	'\t'},
	{"KEY_RET",	'\r'},
	{"KEY_ESC",	0x1B},
	{"KEY_BS",	0x7F},
};

static void
on_sigwinch(
	int sig)
{
	(void)sig;
	term.resized = 1;
}

/* Restore the terminal; safe to call more than once. */
static void
term_restore(void)
{
	static const char restore_seq[] =
		"\x1B[?25h"	/* show cursor */
		"\x1B[0m"	/* reset attributes */
		"\x1B[>4;0m"	/* modifyOtherKeys off */
		"\x1B[?1049l";	/* leave the alternate screen */

	if (!term.open)
		return;
	(void)!write(STDOUT_FILENO, restore_seq, sizeof(restore_seq) - 1);
	if (term.saved_valid)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &term.saved);
	term.open = false;
}

NOCT_DLL
bool
noct_register_api_term(
	NoctEnv *env)
{
	NoctValue term_dict, tmp;
	size_t i;

	memset(&term_dict, 0, sizeof(term_dict));
	memset(&tmp, 0, sizeof(tmp));
	noct_pin_local(env, 2, &term_dict, &tmp);

	if (!noct_make_empty_dict(env, &term_dict))
		return false;
	if (!noct_set_global(env, "Term", &term_dict))
		return false;

	for (i = 0; i < sizeof(term_ffi_items) / sizeof(struct term_ffi_item); i++) {
		NoctValue funcval;

		memset(&funcval, 0, sizeof(funcval));
		if (!noct_register_cfunc(env,
					 term_ffi_items[i].global_name,
					 term_ffi_items[i].param_count,
					 term_ffi_items[i].param,
					 term_ffi_items[i].cfunc,
					 NULL))
			return false;
		if (!noct_get_global(env, term_ffi_items[i].global_name, &funcval))
			return false;
		if (!noct_set_dict_elem_cstr(env, &term_dict,
					     term_ffi_items[i].field_name, &funcval))
			return false;
	}

	for (i = 0; i < sizeof(term_consts) / sizeof(struct term_const); i++) {
		if (!noct_set_dict_elem_make_int(env, &term_dict,
						 term_consts[i].name, &tmp,
						 term_consts[i].value))
			return false;
	}

	atexit(term_restore);

	noct_unpin_local(env, 2, &term_dict, &tmp);

	return true;
}

/*
 * Output buffering
 */

static bool
out_put(
	NoctEnv *env,
	const char *s,
	size_t len)
{
	if (term.out_len + len > term.out_alloc) {
		size_t new_alloc = term.out_alloc == 0 ? 8192 : term.out_alloc;
		char *p;
		while (new_alloc < term.out_len + len)
			new_alloc *= 2;
		p = noct_realloc(term.out, new_alloc);
		if (p == NULL) {
			noct_out_of_memory(env);
			return false;
		}
		term.out = p;
		term.out_alloc = new_alloc;
	}
	memcpy(term.out + term.out_len, s, len);
	term.out_len += len;
	return true;
}

static bool
out_put_cstr(
	NoctEnv *env,
	const char *s)
{
	return out_put(env, s, strlen(s));
}

static bool
return_int(
	NoctEnv *env,
	int64_t v)
{
	NoctValue ret;

	memset(&ret, 0, sizeof(ret));
	noct_pin_local(env, 1, &ret);

	/*
	 * Return an int, not a long: conditions and arithmetic stay in
	 * the VM's common int path. Buffer positions beyond 2^31 are out
	 * of scope for v1.
	 */
	if (!noct_set_return_make_int(env, &ret, (int)v))
		return false;
	noct_unpin_local(env, 1, &ret);
	return true;
}

/*
 * Session
 */

static bool
cfunc_Term_isTTY(
	NoctEnv *env)
{
	return return_int(env, isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) ? 1 : 0);
}

static bool
cfunc_Term_open(
	NoctEnv *env)
{
	struct termios raw;
	struct sigaction sa;
	static const char enter_seq[] =
		"\x1B[?1049h"	/* alternate screen */
		"\x1B[2J"	/* clear */
		"\x1B[H"	/* home */
		"\x1B[>4;2m";	/* modifyOtherKeys=2 */

	if (term.open)
		return return_int(env, 1);
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		return return_int(env, 0);

	if (tcgetattr(STDIN_FILENO, &term.saved) != 0)
		return return_int(env, 0);
	term.saved_valid = true;

	raw = term.saved;
	raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= (tcflag_t)~OPOST;
	raw.c_cflag |= CS8;
	raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
		return return_int(env, 0);

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_sigwinch;
	sigaction(SIGWINCH, &sa, NULL);

	(void)!write(STDOUT_FILENO, enter_seq, sizeof(enter_seq) - 1);

	term.open = true;
	term.in_head = 0;
	term.in_len = 0;
	term.out_len = 0;

	return return_int(env, 1);
}

static bool
cfunc_Term_close(
	NoctEnv *env)
{
	term_restore();
	return return_int(env, 1);
}

static bool
cfunc_Term_size(
	NoctEnv *env)
{
	NoctValue ret, tmp;
	struct winsize ws;
	int rows, cols;

	rows = 24;
	cols = 80;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
		rows = ws.ws_row;
		cols = ws.ws_col;
	}

	memset(&ret, 0, sizeof(ret));
	memset(&tmp, 0, sizeof(tmp));
	noct_pin_local(env, 2, &ret, &tmp);
	if (!noct_make_empty_dict(env, &ret))
		return false;
	if (!noct_set_dict_elem_make_int(env, &ret, "rows", &tmp, rows))
		return false;
	if (!noct_set_dict_elem_make_int(env, &ret, "cols", &tmp, cols))
		return false;
	if (!noct_set_return(env, &ret))
		return false;
	noct_unpin_local(env, 2, &ret, &tmp);
	return true;
}

static bool
cfunc_Term_resized(
	NoctEnv *env)
{
	int v;

	v = term.resized ? 1 : 0;
	term.resized = 0;
	return return_int(env, v);
}

/*
 * Output
 */

static bool
get_int_arg(
	NoctEnv *env,
	uint32_t index,
	int64_t *out)
{
	NoctValue v;
	int64_t l;
	int i;

	memset(&v, 0, sizeof(v));
	noct_pin_local(env, 1, &v);
	if (noct_get_arg_check_long(env, index, &v, &l)) {
		*out = l;
	} else if (noct_get_arg_check_int(env, index, &v, &i)) {
		*out = i;
	} else {
		return false;
	}
	noct_unpin_local(env, 1, &v);
	return true;
}

static bool
cfunc_Term_moveTo(
	NoctEnv *env)
{
	int64_t row, col;
	char seq[32];

	if (!get_int_arg(env, 0, &row))
		return false;
	if (!get_int_arg(env, 1, &col))
		return false;
	snprintf(seq, sizeof(seq), "\x1B[%d;%dH", (int)row, (int)col);
	if (!out_put_cstr(env, seq))
		return false;
	return return_int(env, 1);
}

static bool
cfunc_Term_write(
	NoctEnv *env)
{
	NoctValue text;
	const char *text_s;

	memset(&text, 0, sizeof(text));
	noct_pin_local(env, 1, &text);
	if (!noct_get_arg_check_string(env, 0, &text, &text_s))
		return false;
	if (!out_put_cstr(env, text_s))
		return false;
	noct_unpin_local(env, 1, &text);
	return return_int(env, 1);
}

static bool
cfunc_Term_clear(
	NoctEnv *env)
{
	if (!out_put_cstr(env, "\x1B[2J\x1B[H"))
		return false;
	return return_int(env, 1);
}

static bool
cfunc_Term_clearToEol(
	NoctEnv *env)
{
	if (!out_put_cstr(env, "\x1B[K"))
		return false;
	return return_int(env, 1);
}

static bool
cfunc_Term_setStyle(
	NoctEnv *env)
{
	NoctValue style, tmp;
	char seq[64];
	int fg, bg, bold, reverse, underline;

	memset(&style, 0, sizeof(style));
	memset(&tmp, 0, sizeof(tmp));
	noct_pin_local(env, 2, &style, &tmp);
	if (!noct_get_arg_check_dict(env, 0, &style))
		return false;

	fg = -1;
	bg = -1;
	bold = 0;
	reverse = 0;
	underline = 0;
	{
		bool has;
		if (noct_check_dict_key_cstr(env, &style, "fg", &has) && has)
			noct_get_dict_elem_check_int(env, &style, "fg", &tmp, &fg);
		if (noct_check_dict_key_cstr(env, &style, "bg", &has) && has)
			noct_get_dict_elem_check_int(env, &style, "bg", &tmp, &bg);
		if (noct_check_dict_key_cstr(env, &style, "bold", &has) && has)
			noct_get_dict_elem_check_int(env, &style, "bold", &tmp, &bold);
		if (noct_check_dict_key_cstr(env, &style, "reverse", &has) && has)
			noct_get_dict_elem_check_int(env, &style, "reverse", &tmp, &reverse);
		if (noct_check_dict_key_cstr(env, &style, "underline", &has) && has)
			noct_get_dict_elem_check_int(env, &style, "underline", &tmp, &underline);
	}

	if (!out_put_cstr(env, "\x1B[0m"))
		return false;
	if (bold && !out_put_cstr(env, "\x1B[1m"))
		return false;
	if (underline && !out_put_cstr(env, "\x1B[4m"))
		return false;
	if (reverse && !out_put_cstr(env, "\x1B[7m"))
		return false;
	if (fg >= 0) {
		snprintf(seq, sizeof(seq), "\x1B[38;5;%dm", fg);
		if (!out_put_cstr(env, seq))
			return false;
	}
	if (bg >= 0) {
		snprintf(seq, sizeof(seq), "\x1B[48;5;%dm", bg);
		if (!out_put_cstr(env, seq))
			return false;
	}

	noct_unpin_local(env, 2, &style, &tmp);
	return return_int(env, 1);
}

static bool
cfunc_Term_showCursor(
	NoctEnv *env)
{
	int64_t visible;

	if (!get_int_arg(env, 0, &visible))
		return false;
	if (!out_put_cstr(env, visible ? "\x1B[?25h" : "\x1B[?25l"))
		return false;
	return return_int(env, 1);
}

static bool
cfunc_Term_flush(
	NoctEnv *env)
{
	size_t off;
	ssize_t n;

	off = 0;
	while (off < term.out_len) {
		n = write(STDOUT_FILENO, term.out + off, term.out_len - off);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		off += (size_t)n;
	}
	term.out_len = 0;
	return return_int(env, 1);
}

/*
 * Input
 */

/* Pull more bytes into the ring; the wait is a blocking region. */
static int
in_fill(
	NoctEnv *env,
	int timeout_ms)
{
	struct pollfd pfd;
	ssize_t n;
	int poll_ret;
	unsigned char buf[64];
	size_t i;

	if (term.in_len > 0)
		return 1;

	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	noct_enter_blocking(env);
	for (;;) {
		poll_ret = poll(&pfd, 1, timeout_ms);
		if (poll_ret >= 0)
			break;
		if (errno == EINTR) {
			/* A resize interrupts the wait; report it upward. */
			if (term.resized) {
				poll_ret = 0;
				break;
			}
			continue;
		}
		break;
	}
	noct_leave_blocking(env);

	if (poll_ret <= 0)
		return 0;

	n = read(STDIN_FILENO, buf, sizeof(buf));
	if (n <= 0)
		return 0;
	for (i = 0; i < (size_t)n && term.in_len < sizeof(term.in); i++) {
		term.in[(term.in_head + term.in_len) % sizeof(term.in)] = buf[i];
		term.in_len++;
	}
	return 1;
}

static int
in_peek(
	size_t offset)
{
	if (offset >= term.in_len)
		return -1;
	return term.in[(term.in_head + offset) % sizeof(term.in)];
}

static void
in_consume(
	size_t n)
{
	assert(n <= term.in_len);
	term.in_head = (term.in_head + n) % sizeof(term.in);
	term.in_len -= n;
}

/* Decode one CSI sequence already in the ring. Returns the event or -1
 * if incomplete; *consumed is set on success. */
static int
decode_csi(
	size_t start,
	size_t *consumed)
{
	int params[4];
	int nparam;
	size_t i;
	int c, acc, has_digit;

	nparam = 0;
	acc = 0;
	has_digit = 0;
	i = start;
	for (;;) {
		c = in_peek(i);
		if (c < 0)
			return -1;	/* Incomplete. */
		if (c >= '0' && c <= '9') {
			acc = acc * 10 + (c - '0');
			has_digit = 1;
			i++;
			continue;
		}
		if (c == ';') {
			if (nparam < 4)
				params[nparam++] = has_digit ? acc : 0;
			acc = 0;
			has_digit = 0;
			i++;
			continue;
		}
		break;
	}
	if (nparam < 4)
		params[nparam++] = has_digit ? acc : 0;
	*consumed = i + 1 - start;

	/* Final byte. */
	{
		int mod_bits = 0;
		int base = -1;

		/* xterm modifier parameter: 1=none 2=S 3=M 4=M-S 5=C ... */
		int m = 0;
		if (nparam >= 2)
			m = params[1] > 0 ? params[1] - 1 : 0;
		if (m & 1)
			mod_bits |= MOD_SHIFT;
		if (m & 2)
			mod_bits |= MOD_META;
		if (m & 4)
			mod_bits |= MOD_CTRL;

		switch (c) {
		case 'A': base = KEY_UP; break;
		case 'B': base = KEY_DOWN; break;
		case 'C': base = KEY_RIGHT; break;
		case 'D': base = KEY_LEFT; break;
		case 'H': base = KEY_HOME; break;
		case 'F': base = KEY_END; break;
		case '~':
			switch (params[0]) {
			case 1: base = KEY_HOME; break;
			case 2: base = KEY_INSERT; break;
			case 3: base = KEY_DELETE; break;
			case 4: base = KEY_END; break;
			case 5: base = KEY_PGUP; break;
			case 6: base = KEY_PGDN; break;
			case 11: case 12: case 13: case 14: case 15:
				base = KEY_F1 + params[0] - 11; break;
			case 17: case 18: case 19: case 20: case 21:
				base = KEY_F1 + params[0] - 12; break;
			case 23: case 24:
				base = KEY_F1 + params[0] - 13; break;
			case 27:
				/* modifyOtherKeys: CSI 27 ; mod ; code ~ */
				if (nparam >= 3) {
					int code = params[2];
					int mm = params[1] > 0 ? params[1] - 1 : 0;
					int bits = 0;
					if (mm & 1)
						bits |= MOD_SHIFT;
					if (mm & 2)
						bits |= MOD_META;
					if (mm & 4)
						bits |= MOD_CTRL;
					return bits | code;
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		if (base < 0)
			return 0;	/* Unknown: swallow silently. */
		return mod_bits | base;
	}
}

/* Decode one event from the ring. Returns the event, or -1 if more
 * bytes are needed (possibly after a timeout). */
static int
decode_event(
	NoctEnv *env,
	bool esc_timed_out)
{
	int c0, c1;
	size_t consumed;
	int ev;

	c0 = in_peek(0);
	if (c0 < 0)
		return -1;

	/* Plain control characters. */
	if (c0 != 0x1B && c0 < 0x20) {
		in_consume(1);
		/* TAB and RET keep their traditional identities;
		   LF is C-j, as in Emacs. */
		if (c0 == '\t' || c0 == '\r')
			return c0;
		if (c0 == '\n')
			return MOD_CTRL | 0x6A;
		/* NUL is C-SPC (typed as Ctrl+Space or Ctrl+@). */
		if (c0 == 0x00)
			return MOD_CTRL | 0x20;
		/* 0x01..0x1A are C-a..C-z; 0x1B..0x1F are C-[ .. C-_. */
		if (c0 <= 0x1A)
			return MOD_CTRL | (c0 + 0x60);
		return MOD_CTRL | (c0 + 0x40);
	}
	if (c0 == 0x7F) {
		in_consume(1);
		return 0x7F;	/* Backspace. */
	}

	/* ASCII. */
	if (c0 >= 0x20 && c0 < 0x7F && c0 != 0x1B) {
		in_consume(1);
		return c0;
	}

	/* UTF-8 lead byte. */
	if (c0 >= 0xC0) {
		size_t len =
			(c0 & 0xE0) == 0xC0 ? 2 :
			(c0 & 0xF0) == 0xE0 ? 3 : 4;
		size_t i;
		uint32_t cp;

		if (term.in_len < len)
			return -1;
		cp = (uint32_t)(c0 & (0xFF >> (len + 1)));
		for (i = 1; i < len; i++) {
			int cc = in_peek(i);
			if ((cc & 0xC0) != 0x80) {
				/* Broken sequence: drop the lead byte. */
				in_consume(1);
				return 0;
			}
			cp = (cp << 6) | (uint32_t)(cc & 0x3F);
		}
		in_consume(len);
		return (int)cp;
	}

	/* ESC ... */
	c1 = in_peek(1);
	if (c1 < 0) {
		/* Alone so far: either a pending sequence or a bare ESC. */
		if (esc_timed_out) {
			in_consume(1);
			return 0x1B;
		}
		return -1;
	}
	if (c1 == '[') {
		ev = decode_csi(2, &consumed);
		if (ev < 0)
			return -1;
		in_consume(2 + consumed);
		return ev;
	}
	if (c1 == 'O') {
		int c2 = in_peek(2);
		if (c2 < 0)
			return -1;
		in_consume(3);
		switch (c2) {
		case 'A': return KEY_UP;
		case 'B': return KEY_DOWN;
		case 'C': return KEY_RIGHT;
		case 'D': return KEY_LEFT;
		case 'H': return KEY_HOME;
		case 'F': return KEY_END;
		case 'P': return KEY_F1;
		case 'Q': return KEY_F1 + 1;
		case 'R': return KEY_F1 + 2;
		case 'S': return KEY_F1 + 3;
		default:  return 0;
		}
	}

	/* ESC prefix meta: decode the rest and add the meta bit. */
	in_consume(1);
	ev = decode_event(env, true);
	if (ev <= 0)
		return 0x1B;
	return MOD_META | ev;
}

static bool
cfunc_Term_readKey(
	NoctEnv *env)
{
	int64_t timeout_ms;
	int ev;
	bool esc_wait;

	if (!get_int_arg(env, 0, &timeout_ms))
		return false;

	esc_wait = false;
	for (;;) {
		ev = decode_event(env, esc_wait);
		if (ev >= 0 && ev != 0)
			return return_int(env, ev);
		if (ev == 0)
			continue;	/* Swallowed an unknown sequence. */

		/* Need more bytes. */
		{
			int wait = (int)timeout_ms;
			if (term.in_len > 0 && in_peek(0) == 0x1B)
				wait = ESC_TIMEOUT_MS;
			if (!in_fill(env, wait)) {
				if (term.in_len > 0 && in_peek(0) == 0x1B) {
					/* The ESC stands alone. */
					esc_wait = true;
					continue;
				}
				return return_int(env, -1);
			}
		}
	}
}

static bool
cfunc_Term_pendingInput(
	NoctEnv *env)
{
	struct pollfd pfd;

	if (term.in_len > 0)
		return return_int(env, 1);
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;
	return return_int(env, poll(&pfd, 1, 0) > 0 ? 1 : 0);
}

#endif /* NOCT_TARGET_POSIX */
