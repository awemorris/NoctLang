/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Target-neutral callback backend for the non-standard Term.* API. */

#include <noct/noct.h>
#include <string.h>

struct active_term_backend {
	const struct NoctTermBackend *operations;
	void *context;
};

static struct active_term_backend active;

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
static bool cfunc_Term_syncBegin(NoctEnv *env);
static bool cfunc_Term_syncEnd(NoctEnv *env);
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
	{"Term.open", "open", 0, {NULL}, cfunc_Term_open},
	{"Term.close", "close", 0, {NULL}, cfunc_Term_close},
	{"Term.isTTY", "isTTY", 0, {NULL}, cfunc_Term_isTTY},
	{"Term.size", "size", 0, {NULL}, cfunc_Term_size},
	{"Term.resized", "resized", 0, {NULL}, cfunc_Term_resized},
	{"Term.moveTo", "moveTo", 2, {"row", "col"}, cfunc_Term_moveTo},
	{"Term.write", "write", 1, {"text"}, cfunc_Term_write},
	{"Term.clear", "clear", 0, {NULL}, cfunc_Term_clear},
	{"Term.clearToEol", "clearToEol", 0, {NULL}, cfunc_Term_clearToEol},
	{"Term.setStyle", "setStyle", 1, {"style"}, cfunc_Term_setStyle},
	{"Term.showCursor", "showCursor", 1, {"visible"}, cfunc_Term_showCursor},
	{"Term.flush", "flush", 0, {NULL}, cfunc_Term_flush},
	{"Term.syncBegin", "syncBegin", 0, {NULL}, cfunc_Term_syncBegin},
	{"Term.syncEnd", "syncEnd", 0, {NULL}, cfunc_Term_syncEnd},
	{"Term.readKey", "readKey", 1, {"timeoutMs"}, cfunc_Term_readKey},
	{"Term.pendingInput", "pendingInput", 0, {NULL}, cfunc_Term_pendingInput},
};

struct term_const {
	const char *name;
	int value;
};

static const struct term_const term_consts[] = {
	{"META", NOCT_TERM_MOD_META},
	{"CTRL", NOCT_TERM_MOD_CTRL},
	{"SHIFT", NOCT_TERM_MOD_SHIFT},
	{"KEY_UP", NOCT_TERM_KEY_UP},
	{"KEY_DOWN", NOCT_TERM_KEY_DOWN},
	{"KEY_RIGHT", NOCT_TERM_KEY_RIGHT},
	{"KEY_LEFT", NOCT_TERM_KEY_LEFT},
	{"KEY_HOME", NOCT_TERM_KEY_HOME},
	{"KEY_END", NOCT_TERM_KEY_END},
	{"KEY_PGUP", NOCT_TERM_KEY_PGUP},
	{"KEY_PGDN", NOCT_TERM_KEY_PGDN},
	{"KEY_INSERT", NOCT_TERM_KEY_INSERT},
	{"KEY_DELETE", NOCT_TERM_KEY_DELETE},
	{"KEY_F1", NOCT_TERM_KEY_F1},
	{"KEY_TAB", '\t'},
	{"KEY_RET", '\r'},
	{"KEY_ESC", 0x1b},
	{"KEY_BS", 0x7f},
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

NOCT_DLL
bool
noct_register_api_term_backend(NoctEnv *env,
			       const struct NoctTermBackend *backend,
			       void *context)
{
	NoctValue term_dict, temporary, function;
	size_t index;
	bool ok = false;

	active.operations = backend;
	active.context = context;
	memset(&term_dict, 0, sizeof(term_dict));
	memset(&temporary, 0, sizeof(temporary));
	memset(&function, 0, sizeof(function));
	if (!noct_pin_local(env, 3, &term_dict, &temporary, &function))
		return false;
	if (!noct_make_empty_dict(env, &term_dict) ||
	    !noct_set_global(env, "Term", &term_dict))
		goto cleanup;
	for (index = 0; index < sizeof(term_ffi_items) /
					 sizeof(term_ffi_items[0]); index++) {
		struct term_ffi_item *item = &term_ffi_items[index];

		if (!noct_register_cfunc(env, item->global_name, item->param_count,
					 item->param, item->cfunc, NULL) ||
		    !noct_get_global(env, item->global_name, &function) ||
		    !noct_set_dict_elem_cstr(env, &term_dict, item->field_name,
					     &function))
			goto cleanup;
	}
	for (index = 0; index < sizeof(term_consts) / sizeof(term_consts[0]);
	     index++)
		if (!noct_set_dict_elem_make_int(env, &term_dict,
					 term_consts[index].name, &temporary,
					 term_consts[index].value))
			goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 3, &term_dict, &temporary, &function);
	return ok;
}

static bool
cfunc_Term_open(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->open != NULL ?
		active.operations->open(active.context) : 0);
}

static bool
cfunc_Term_close(NoctEnv *env)
{
	if (active.operations != NULL && active.operations->close != NULL)
		active.operations->close(active.context);
	return return_int(env, active.operations != NULL ? 1 : 0);
}

static bool
cfunc_Term_isTTY(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->is_tty != NULL ?
		active.operations->is_tty(active.context) : 0);
}

static bool
cfunc_Term_size(NoctEnv *env)
{
	NoctValue result, temporary;
	unsigned rows = 24, columns = 80;
	bool ok = false;

	if (active.operations != NULL && active.operations->size != NULL)
		(void)active.operations->size(active.context, &rows, &columns);
	memset(&result, 0, sizeof(result));
	memset(&temporary, 0, sizeof(temporary));
	if (!noct_pin_local(env, 2, &result, &temporary))
		return false;
	if (!noct_make_empty_dict(env, &result) ||
	    !noct_set_dict_elem_make_int(env, &result, "rows", &temporary,
					 (int)rows) ||
	    !noct_set_dict_elem_make_int(env, &result, "cols", &temporary,
					 (int)columns) ||
	    !noct_set_return(env, &result))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &result, &temporary);
	return ok;
}

static bool
cfunc_Term_resized(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->resized != NULL ?
		active.operations->resized(active.context) : 0);
}

static bool
cfunc_Term_moveTo(NoctEnv *env)
{
	int row, column;

	if (!get_int_arg(env, 0, &row) || !get_int_arg(env, 1, &column))
		return false;
	return return_int(env, active.operations != NULL &&
		active.operations->move_to != NULL && row > 0 && column > 0 ?
		active.operations->move_to(active.context, (unsigned)row,
					   (unsigned)column) : 0);
}

static bool
cfunc_Term_write(NoctEnv *env)
{
	NoctValue value;
	const char *text;
	int result;
	bool ok;

	memset(&value, 0, sizeof(value));
	if (!noct_pin_local(env, 1, &value))
		return false;
	ok = noct_get_arg_check_string(env, 0, &value, &text);
	if (!ok) {
		(void)noct_unpin_local(env, 1, &value);
		return false;
	}
	result = active.operations != NULL && active.operations->write != NULL ?
		active.operations->write(active.context, text, strlen(text)) : 0;
	(void)noct_unpin_local(env, 1, &value);
	return return_int(env, result);
}

static bool
cfunc_Term_clear(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->clear != NULL ?
		active.operations->clear(active.context) : 0);
}

static bool
cfunc_Term_clearToEol(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->clear_to_eol != NULL ?
		active.operations->clear_to_eol(active.context) : 0);
}

static bool
cfunc_Term_setStyle(NoctEnv *env)
{
	NoctValue style, temporary;
	struct NoctTermStyle native = {-1, -1, false, false, false};
	bool has;
	int value;
	bool ok = false;

	memset(&style, 0, sizeof(style));
	memset(&temporary, 0, sizeof(temporary));
	if (!noct_pin_local(env, 2, &style, &temporary))
		return false;
	if (!noct_get_arg_check_dict(env, 0, &style))
		goto cleanup;
#define READ_STYLE(name, field) \
	do { \
		has = false; \
		if (!noct_check_dict_key_cstr(env, &style, name, &has)) \
			goto cleanup; \
		if (has) { \
			if (!noct_get_dict_elem_check_int(env, &style, name, \
						       &temporary, &value)) \
				goto cleanup; \
			native.field = value; \
		} \
	} while (0)
	READ_STYLE("fg", foreground);
	READ_STYLE("bg", background);
	READ_STYLE("bold", bold);
	READ_STYLE("reverse", reverse);
	READ_STYLE("underline", underline);
#undef READ_STYLE
	value = active.operations != NULL && active.operations->set_style != NULL ?
		active.operations->set_style(active.context, &native) : 0;
	ok = return_int(env, value);
cleanup:
	(void)noct_unpin_local(env, 2, &style, &temporary);
	return ok;
}

static bool
cfunc_Term_showCursor(NoctEnv *env)
{
	int visible;

	if (!get_int_arg(env, 0, &visible))
		return false;
	return return_int(env, active.operations != NULL &&
		active.operations->show_cursor != NULL ?
		active.operations->show_cursor(active.context, visible != 0) : 0);
}

static bool
cfunc_Term_flush(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->flush != NULL ?
		active.operations->flush(active.context) : 0);
}

/* Synchronized output is a rendering optimization for ANSI terminals.  A
 * direct framebuffer/text-VRAM backend updates atomically enough by design,
 * so preserving the API as a successful no-op is the correct fallback. */
static bool
cfunc_Term_syncBegin(NoctEnv *env)
{
	return return_int(env, active.operations != NULL ? 1 : 0);
}

static bool
cfunc_Term_syncEnd(NoctEnv *env)
{
	return return_int(env, active.operations != NULL ? 1 : 0);
}

static bool
cfunc_Term_readKey(NoctEnv *env)
{
	int timeout;

	if (!get_int_arg(env, 0, &timeout))
		return false;
	return return_int(env, active.operations != NULL &&
		active.operations->read_key != NULL ?
		active.operations->read_key(active.context, timeout) : -1);
}

static bool
cfunc_Term_pendingInput(NoctEnv *env)
{
	return return_int(env, active.operations != NULL &&
		active.operations->pending_input != NULL ?
		active.operations->pending_input(active.context) : 0);
}
