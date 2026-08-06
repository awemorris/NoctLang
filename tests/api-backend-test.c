/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#include <noct/noct.h>
#include <stdio.h>
#include <string.h>

struct mock_state {
	int opened;
	int moved;
	int wrote;
	int styled;
};

static int mock_open(void *context)
{
	((struct mock_state *)context)->opened++;
	return 1;
}

static void mock_close(void *context)
{
	((struct mock_state *)context)->opened--;
}

static int mock_true(void *context)
{
	(void)context;
	return 1;
}

static int mock_size(void *context, unsigned *rows, unsigned *columns)
{
	(void)context;
	*rows = 25;
	*columns = 80;
	return 1;
}

static int mock_move(void *context, unsigned row, unsigned column)
{
	struct mock_state *state = context;

	state->moved = row == 2 && column == 3;
	return state->moved;
}

static int mock_write(void *context, const char *text, size_t length)
{
	struct mock_state *state = context;

	state->wrote = length == 5 && memcmp(text, "hello", 5) == 0;
	return state->wrote;
}

static int mock_style(void *context, const struct NoctTermStyle *style)
{
	struct mock_state *state = context;

	state->styled = style->foreground == -1 &&
		style->background == -1 && style->reverse && !style->bold &&
		!style->underline;
	return state->styled;
}

static int mock_show(void *context, int visible)
{
	(void)context;
	return visible;
}

static int mock_read(void *context, int timeout_ms)
{
	(void)context;
	return timeout_ms == 10 ? NOCT_TERM_KEY_LEFT : -1;
}

static int mock_directory(void *context, const char *path, size_t index,
			  char *name, size_t capacity, int *is_directory)
{
	const char *entry;

	(void)context;
	if (strcmp(path, "/") != 0 || index >= 2)
		return 0;
	entry = index == 0 ? "b.txt" : "A";
	if (strlen(entry) + 1 > capacity)
		return -1;
	strcpy(name, entry);
	*is_directory = index == 1;
	return 1;
}

int main(void)
{
	static const struct NoctTermBackend term_backend = {
		.open = mock_open,
		.close = mock_close,
		.is_tty = mock_true,
		.size = mock_size,
		.resized = mock_true,
		.move_to = mock_move,
		.write = mock_write,
		.clear = mock_true,
		.clear_to_eol = mock_true,
		.set_style = mock_style,
		.show_cursor = mock_show,
		.flush = mock_true,
		.read_key = mock_read,
		.pending_input = mock_true,
	};
	static const struct NoctDirectoryBackend directory_backend = {
		.read = mock_directory,
	};
	static const char source[] =
		"func main() {\n"
		"  if (Term.isTTY() != 1 || Term.open() != 1) { return 1; }\n"
		"  var s = Term.size();\n"
		"  if (s.rows != 25 || s.cols != 80) { return 2; }\n"
		"  if (Term.moveTo(2, 3) != 1 || Term.write(\"hello\") != 1) { return 3; }\n"
		"  if (Term.setStyle({reverse: 1}) != 1) { return 4; }\n"
		"  if (Term.showCursor(1) != 1 || Term.readKey(10) != Term.KEY_LEFT) { return 5; }\n"
		"  var d = FileUtil.listDirectory(\"/\");\n"
		"  if (Array.size(d) != 2 || d[0] != \"A/\" || d[1] != \"b.txt\") { return 6; }\n"
		"  Term.close();\n"
		"  return 0;\n"
		"}\n";
	struct mock_state state = {0};
	NoctVM *vm;
	NoctEnv *env;
	NoctValue result = NOCT_ZERO;
	int value;

	if (!noct_create_vm(&vm, &env, NULL))
		return 10;
	noct_set_directory_backend(&directory_backend, &state);
	if (!noct_register_api_file(env) ||
	    !noct_register_api_term_backend(env, &term_backend, &state) ||
	    !noct_register_source(env, "api-backend-test.noct", source) ||
	    !noct_enter_vm(env, "main", 0, NULL, &result) ||
	    !noct_get_int(env, &result, &value))
		return 11;
	if (!noct_destroy_vm(vm))
		return 12;
	if (value != 0 || state.opened != 0 || !state.moved || !state.wrote ||
	    !state.styled)
		return 20 + value;
	puts("Target backend API test: PASS");
	return 0;
}
