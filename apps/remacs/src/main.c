/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * remacs
 * Copyright (c) 2026, Awe Morris
 */

/*
 * remacs CLI.
 *
 *   remacs                 run the editor (editor/boot.noct)
 *   remacs FILE            run the editor and visit FILE
 *   remacs --script FILE   run a Noct script with the remacs APIs
 *                          registered (used by the test harness)
 */

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "remacs.h"

/* VM configuration (REMACS_OPT_LEVEL selects the optimization level). */
static NoctConfig vm_config;

#ifndef REMACS_EDITOR_DIR
#define REMACS_EDITOR_DIR "editor"
#endif

#ifndef REMACS_GEN_DIR
#define REMACS_GEN_DIR "generated"
#endif

#ifndef REMACS_VERSION
#define REMACS_VERSION "0.0.1"
#endif

/*
 * Editor sources.
 *
 * The logic layer holds oracle-comparable functions and is loaded in
 * script mode too, so tests can call them. The UI layer defines main()
 * and is only loaded when running as the editor.
 */
static const char *logic_sources[] = {
	"buffer.noct",
	"commands.noct",
	"keys.noct",
	"lisp.noct",
	"lispbuiltins.noct",
	"lispcompile.noct",
	"skk.noct",
};
static const char *ui_sources[] = {
	"minibuf.noct",
	"isearch.noct",
	"replace.noct",
	"shell.noct",
	"gud.noct",
	"compile.noct",
	"window.noct",
	"keymap.noct",
	"redisplay.noct",
	"boot.noct",
};

static char *
load_file(
	const char *path)
{
	FILE *fp;
	long size;
	char *data;

	fp = fopen(path, "rb");
	if (fp == NULL)
		return NULL;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (size < 0) {
		fclose(fp);
		return NULL;
	}
	data = malloc((size_t)size + 1);
	if (data == NULL) {
		fclose(fp);
		return NULL;
	}
	if (size > 0 && fread(data, (size_t)size, 1, fp) != 1) {
		free(data);
		fclose(fp);
		return NULL;
	}
	data[size] = '\0';
	fclose(fp);
	return data;
}

static void
print_error(
	NoctEnv *env)
{
	const char *file, *msg;
	int line;

	noct_get_error_file(env, &file);
	noct_get_error_line(env, &line);
	noct_get_error_message(env, &msg);
	fprintf(stderr, "%s:%d: Error: %s\n",
		file != NULL ? file : "?", line, msg != NULL ? msg : "?");
}

static bool
register_apis(
	NoctEnv *env)
{
	if (!noct_register_api_system(env))
		return false;
	if (!noct_register_api_console(env))
		return false;
	if (!noct_register_api_file(env))
		return false;
#if defined(NOCT_USE_MULTITHREAD)
	if (!noct_register_api_thread(env))
		return false;
#endif
	if (!noct_register_api_term(env))
		return false;
	if (!noct_register_api_process(env))
		return false;
	if (!remacs_register_api_util(env))
		return false;
	return true;
}

static bool
load_source_file(
	NoctEnv *env,
	const char *path)
{
	char *text;
	bool ok;

	text = load_file(path);
	if (text == NULL) {
		fprintf(stderr, "remacs: cannot read %s\n", path);
		return false;
	}
	ok = noct_register_source(env, path, text);
	free(text);
	if (!ok) {
		print_error(env);
		return false;
	}
	return true;
}

int
main(
	int argc,
	char *argv[])
{
	NoctVM *vm;
	NoctEnv *env;
	NoctValue ret;
	NoctValue arg;
	const char *script;
	const char *visit;
	uint32_t arg_count;
	int i;

	script = NULL;
	visit = NULL;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
			script = argv[++i];
		} else if (strcmp(argv[i], "--version") == 0) {
			printf("remacs %s\n", REMACS_VERSION);
			return 0;
		} else if (argv[i][0] != '-') {
			visit = argv[i];
		} else {
			fprintf(stderr, "remacs: unknown option %s\n", argv[i]);
			return 1;
		}
	}

	{
		const char *opt_env;
		noct_set_default_config(&vm_config);
		opt_env = getenv("REMACS_OPT_LEVEL");
		if (opt_env != NULL)
			vm_config.optimize_level = atoi(opt_env);
		/* Debugging aid: interpreted frames are visible to gdb. */
		opt_env = getenv("REMACS_NO_JIT");
		if (opt_env != NULL && atoi(opt_env) != 0)
			vm_config.jit_enable = false;
	}
	if (!noct_create_vm(&vm, &env, &vm_config)) {
		fprintf(stderr, "remacs: out of memory\n");
		return 1;
	}
	if (!register_apis(env)) {
		fprintf(stderr, "remacs: API registration failed\n");
		return 1;
	}

	if (script != NULL) {
		/* Harness mode: the logic layer plus one script. */
		char path[1024];
		size_t n;

		for (n = 0; n < sizeof(logic_sources) / sizeof(logic_sources[0]); n++) {
			snprintf(path, sizeof(path), "%s/%s",
				 REMACS_EDITOR_DIR, logic_sources[n]);
			if (!load_source_file(env, path))
				return 1;
		}
		snprintf(path, sizeof(path), "%s/%s", REMACS_GEN_DIR, "lisp-bridge.noct");
		if (!load_source_file(env, path))
			return 1;
		snprintf(path, sizeof(path), "%s/%s", REMACS_GEN_DIR, "napi-init.noct");
		if (!load_source_file(env, path))
			return 1;
		if (!load_source_file(env, script))
			return 1;
		memset(&ret, 0, sizeof(ret));
		if (!noct_enter_vm(env, "remacsInitCore", 0, NULL, &ret)) {
			print_error(env);
			return 1;
		}
		if (!noct_enter_vm(env, "main", 0, NULL, &ret)) {
			print_error(env);
			return 1;
		}
	} else {
		/* Editor mode. */
		char path[1024];
		size_t n;

		for (n = 0; n < sizeof(logic_sources) / sizeof(logic_sources[0]); n++) {
			snprintf(path, sizeof(path), "%s/%s",
				 REMACS_EDITOR_DIR, logic_sources[n]);
			if (!load_source_file(env, path))
				return 1;
		}
		for (n = 0; n < sizeof(ui_sources) / sizeof(ui_sources[0]); n++) {
			snprintf(path, sizeof(path), "%s/%s",
				 REMACS_EDITOR_DIR, ui_sources[n]);
			if (!load_source_file(env, path))
				return 1;
		}
		snprintf(path, sizeof(path), "%s/%s", REMACS_GEN_DIR, "lisp-bridge.noct");
		if (!load_source_file(env, path))
			return 1;
		snprintf(path, sizeof(path), "%s/%s", REMACS_GEN_DIR, "napi-init.noct");
		if (!load_source_file(env, path))
			return 1;

		memset(&ret, 0, sizeof(ret));
		if (!noct_enter_vm(env, "remacsInitCore", 0, NULL, &ret)) {
			print_error(env);
			return 1;
		}

		memset(&ret, 0, sizeof(ret));
		memset(&arg, 0, sizeof(arg));
		arg_count = 0;
		if (visit != NULL) {
			if (!noct_make_string(env, &arg, visit)) {
				fprintf(stderr, "remacs: out of memory\n");
				return 1;
			}
			arg_count = 1;
		} else {
			if (!noct_make_string(env, &arg, "")) {
				fprintf(stderr, "remacs: out of memory\n");
				return 1;
			}
			arg_count = 1;
		}
		if (!noct_enter_vm(env, "main", arg_count, &arg, &ret)) {
			print_error(env);
			return 1;
		}
	}

	if (!noct_destroy_vm(vm))
		return 1;

	return 0;
}
