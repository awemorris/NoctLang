/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Target-neutral cfunc layer for the non-standard BeUI.* API.  The
 * embedder supplies a noct_beui_hal; scripts observe the same BeUI.*
 * and Key.* surface as the Boots pre-boot environment, so a bytecode
 * program compiled once runs on both hosts.
 */

#include <noct/noct.h>
#include <noct/beui.h>

#include <string.h>

static bool cfunc_BeUI_init(NoctEnv *env);
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

struct beui_ffi_item {
	const char *global_name;
	const char *field_name;
	size_t param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(NoctEnv *env);
};

static struct beui_ffi_item beui_ffi_items[] = {
	{"BeUI.init", "init", 0, {NULL}, cfunc_BeUI_init},
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
};

/* Key names shared with the Boots BeUI implementation. */
static const struct {
	const char *name;
	int value;
} beui_keys[] = {
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

static bool
cfunc_BeUI_poll(NoctEnv *env)
{
	if (!noct_beui_poll()) {
		noct_error(env, "BeUI.poll failed.");
		return false;
	}
	return return_int(env, 1);
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
register_key_dictionary(NoctEnv *env)
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
	for (index = 0; index < sizeof(beui_keys) / sizeof(beui_keys[0]);
	     index++)
		if (!noct_set_dict_elem_make_int(env, &dictionary,
						 beui_keys[index].name,
						 &scratch,
						 beui_keys[index].value))
			goto cleanup;
	if (!noct_set_global(env, "Key", &dictionary))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &dictionary, &scratch);
	return ok;
}

NOCT_DLL
bool
noct_register_api_beui(NoctEnv *env, const struct noct_beui_hal *hal)
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
	if (!register_key_dictionary(env))
		goto cleanup;
	ok = true;
cleanup:
	(void)noct_unpin_local(env, 2, &beui_dict, &function);
	return ok;
}
