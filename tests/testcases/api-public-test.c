/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#define _POSIX_C_SOURCE 200809L

#include <noct/noct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
require_global(
	NoctEnv *env,
	const char *name)
{
	NoctValue value;

	memset(&value, 0, sizeof(value));
	return noct_get_global(env, name, &value) ? 1 : 0;
}

static int
test_fileutil_euc_jp(
	NoctEnv *env)
{
	static const unsigned char encoded[] = {
		'A', 0xa4, 0xa2, 0x8e, 0xb1
	};
	static const char expected[] = "A\xe3\x81\x82\xef\xbd\xb1";
	NoctValue fileutil;
	NoctValue function_value;
	NoctValue argument;
	NoctValue result;
	NoctFunc *function;
	const char *decoded;
	char path[] = "/tmp/noct-api-euc-jp-XXXXXX";
	ssize_t written;
	int descriptor;
	int ok = 0;

	memset(&fileutil, 0, sizeof(fileutil));
	memset(&function_value, 0, sizeof(function_value));
	memset(&argument, 0, sizeof(argument));
	memset(&result, 0, sizeof(result));
	descriptor = mkstemp(path);
	if (descriptor < 0)
		return 0;
	written = write(descriptor, encoded, sizeof(encoded));
	if (close(descriptor) != 0 || written != (ssize_t)sizeof(encoded))
		goto cleanup_file;

	if (!noct_pin_local(env, 4, &fileutil, &function_value, &argument,
			    &result))
		goto cleanup_file;
	if (!noct_get_global(env, "FileUtil", &fileutil) ||
	    !noct_get_dict_elem_check_func(env, &fileutil, "readTextEucJp",
					   &function_value, &function) ||
	    !noct_make_string(env, &argument, path) ||
	    !noct_call(env, function, 1, &argument, &result) ||
	    !noct_get_string(env, &result, &decoded))
		goto cleanup_values;
	ok = strcmp(decoded, expected) == 0;

cleanup_values:
	(void)noct_unpin_local(env, 4, &fileutil, &function_value, &argument,
			       &result);
cleanup_file:
	(void)unlink(path);
	return ok;
}

int
main(void)
{
	NoctVM *vm;
	NoctEnv *env;

	if (!noct_create_vm(&vm, &env, NULL))
		return 10;
	if (!noct_register_api_file(env) || !noct_register_api_term(env))
		return 11;
	if (!require_global(env, "File") ||
	    !require_global(env, "FileUtil") ||
	    !require_global(env, "Term"))
		return 12;
	if (!test_fileutil_euc_jp(env))
		return 13;
	if (!noct_destroy_vm(vm))
		return 14;
	puts("Public File/Term API registration: PASS");
	puts("Public FileUtil EUC-JP decoding: PASS");
	return 0;
}
