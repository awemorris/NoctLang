/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Host resolver test for mixed source, bytecode, and application loading.
 */

#include <noct/noct.h>

#include "bytecode.h"
#include "bytecode_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum test_mode {
	TEST_MODE_SOURCE,
	TEST_MODE_BYTECODE,
	TEST_MODE_APP
};

struct test_mapping {
	char *name;
	char *path;
	uint32_t call_count;
};

static struct test_mapping *mapping_table;
static uint32_t mapping_count;
static uint32_t resolver_call_count;

static char *test_resolve_module(const char *module_name);
static bool test_print(NoctEnv *env);
static bool test_read_file(const char *path, uint8_t **data, size_t *size);
static bool test_add_mapping(const char *argument);
static void test_cleanup_mappings(void);
static bool test_register_input(NoctEnv *env, enum test_mode mode, const char *path, uint8_t *data, size_t size);
static bool test_run_main(NoctEnv *env);
static bool test_check_resolver_counts(enum test_mode mode);

/*
 * Loads one fixture through the public embedding API and runs main().
 */
int
main(
	int argc,
	char *argv[])
{
	NoctVM *vm;
	NoctEnv *env;
	NoctConfig config;
	uint8_t *input;
	size_t input_size;
	enum test_mode mode;
	uint32_t i;
	bool vm_created;
	bool succeeded;
	const char *parameter_name[1];

	vm = NULL;
	env = NULL;
	input = NULL;
	input_size = 0;
	vm_created = false;
	succeeded = false;
	parameter_name[0] = "msg";

	if (argc < 3) {
		fprintf(stderr, "Usage: bytecode-require-host source|bytecode|app INPUT [NAME=PATH ...]\n");
		goto cleanup;
	}
	if (strcmp(argv[1], "source") == 0) {
		mode = TEST_MODE_SOURCE;
	} else if (strcmp(argv[1], "bytecode") == 0) {
		mode = TEST_MODE_BYTECODE;
	} else if (strcmp(argv[1], "app") == 0) {
		mode = TEST_MODE_APP;
	} else {
		fprintf(stderr, "Unknown test mode.\n");
		goto cleanup;
	}

	/* Retain every test-only resolver mapping. */
	for (i = 3; i < (uint32_t)argc; i++) {
		if (!test_add_mapping(argv[i]))
			goto cleanup;
	}

	if (!test_read_file(argv[2], &input, &input_size))
		goto cleanup;

	noct_set_default_config(&config);
	config.require_resolver = test_resolve_module;
	if (!noct_create_vm(&vm, &env, &config)) {
		fprintf(stderr, "Cannot create test VM.\n");
		goto cleanup;
	}
	vm_created = true;

	if (!noct_register_cfunc(
		env,
		"print",
		1,
		parameter_name,
		test_print,
		NULL)) {
		fprintf(stderr, "Cannot register test print function.\n");
		goto cleanup;
	}
	if (!test_register_input(
		env,
		mode,
		argv[2],
		input,
		input_size)) {
		const char *message;

		if (noct_get_error_message(env, &message))
			fprintf(stderr, "Registration failed: %s\n", message);
		goto cleanup;
	}
	if (!test_run_main(env))
		goto cleanup;
	if (!test_check_resolver_counts(mode))
		goto cleanup;

	succeeded = true;

cleanup:
	if (vm_created && !noct_destroy_vm(vm))
		succeeded = false;
	free(input);
	test_cleanup_mappings();

	return succeeded ? 0 : 1;
}

/* Resolve one known module to a fresh malloc-compatible path copy. */
static char *
test_resolve_module(
	const char *module_name)
{
	char *path;
	size_t path_size;
	uint32_t i;

	/* Resolve each exact module name through the test table. */
	resolver_call_count++;
	for (i = 0; i < mapping_count; i++) {
		if (strcmp(mapping_table[i].name, module_name) != 0)
			continue;

		mapping_table[i].call_count++;
		path_size = strlen(mapping_table[i].path);
		if (path_size == SIZE_MAX)
			return NULL;
		path = malloc(path_size + 1);
		if (path == NULL)
			return NULL;
		memcpy(path, mapping_table[i].path, path_size + 1);

		return path;
	}

	return NULL;
}

/* Print one checked String argument without a generic serializer. */
static bool
test_print(
	NoctEnv *env)
{
	NoctValue value;
	const char *message;
	bool pinned;
	bool succeeded;

	memset(&value, 0, sizeof(value));
	pinned = false;
	succeeded = false;

	if (!noct_pin_local(env, 1, &value))
		goto cleanup;
	pinned = true;
	if (!noct_get_arg_check_string(env, 0, &value, &message))
		goto cleanup;
	if (fputs(message, stdout) == EOF || fputc('\n', stdout) == EOF) {
		noct_error(env, "Cannot write test output.");
		goto cleanup;
	}

	succeeded = true;

cleanup:
	if (pinned && !noct_unpin_local(env, 1, &value))
		succeeded = false;

	return succeeded;
}

/* Read one complete fixture as explicit-size binary data plus a terminator. */
static bool
test_read_file(
	const char *path,
	uint8_t **data,
	size_t *size)
{
	FILE *stream;
	long file_size;
	size_t read_size;
	bool succeeded;

	*data = NULL;
	*size = 0;
	stream = NULL;
	succeeded = false;

	stream = fopen(path, "rb");
	if (stream == NULL)
		goto cleanup;
	if (fseek(stream, 0, SEEK_END) != 0)
		goto cleanup;
	file_size = ftell(stream);
	if (file_size < 0)
		goto cleanup;
	if (fseek(stream, 0, SEEK_SET) != 0)
		goto cleanup;

	read_size = (size_t)file_size;
	if ((long)read_size != file_size || read_size == SIZE_MAX)
		goto cleanup;
	*data = malloc(read_size + 1);
	if (*data == NULL)
		goto cleanup;
	if (fread(*data, 1, read_size, stream) != read_size)
		goto cleanup;
	(*data)[read_size] = '\0';
	*size = read_size;
	succeeded = true;

cleanup:
	if (stream != NULL && fclose(stream) != 0)
		succeeded = false;
	if (!succeeded) {
		fprintf(stderr, "Cannot read fixture %s.\n", path);
		free(*data);
		*data = NULL;
		*size = 0;
	}

	return succeeded;
}

/* Parse and retain one exact NAME=PATH resolver mapping. */
static bool
test_add_mapping(
	const char *argument)
{
	struct test_mapping *table;
	struct test_mapping *mapping;
	const char *separator;
	char *name;
	char *path;
	size_t name_size;
	size_t path_size;
	uint32_t count;

	separator = strchr(argument, '=');
	if (separator == NULL || separator == argument || separator[1] == '\0')
		return false;
	if (mapping_count == UINT32_MAX)
		return false;
	count = mapping_count + 1;
	if (sizeof(*table) > SIZE_MAX / (size_t)count)
		return false;

	name_size = (size_t)(separator - argument);
	path_size = strlen(separator + 1);
	if (name_size == SIZE_MAX || path_size == SIZE_MAX)
		return false;
	name = malloc(name_size + 1);
	if (name == NULL)
		return false;
	memcpy(name, argument, name_size);
	name[name_size] = '\0';

	path = malloc(path_size + 1);
	if (path == NULL) {
		free(name);
		return false;
	}
	memcpy(path, separator + 1, path_size + 1);

	table = realloc(mapping_table, (size_t)count * sizeof(*table));
	if (table == NULL) {
		free(path);
		free(name);
		return false;
	}
	mapping_table = table;
	mapping = &mapping_table[mapping_count];
	memset(mapping, 0, sizeof(*mapping));
	mapping->name = name;
	mapping->path = path;
	mapping_count = count;

	return true;
}

/* Release every test-only resolver mapping. */
static void
test_cleanup_mappings(
	void)
{
	uint32_t i;

	/* Release mapping strings in command-line order. */
	for (i = 0; i < mapping_count; i++) {
		free(mapping_table[i].path);
		free(mapping_table[i].name);
	}
	free(mapping_table);
	mapping_table = NULL;
	mapping_count = 0;
}

/* Register one source, standalone module, or application fixture. */
static bool
test_register_input(
	NoctEnv *env,
	enum test_mode mode,
	const char *path,
	uint8_t *data,
	size_t size)
{
	const uint8_t *payload;
	size_t payload_size;
	size_t shebang_size;
	uint32_t registration_size;

	if (mode == TEST_MODE_SOURCE) {
		if (memchr(data, '\0', size) != NULL)
			return false;

		return noct_register_source(env, path, (const char *)data);
	}

	payload = data;
	payload_size = size;
	shebang_size = strlen(NOCT_APP_SHEBANG);
	if (payload_size >= shebang_size &&
	    memcmp(payload, NOCT_APP_SHEBANG, shebang_size) == 0) {
		payload += shebang_size;
		payload_size -= shebang_size;
	}
	if (!bytecode_file_check_registration_size(
		payload_size,
		&registration_size)) {
		return false;
	}

	return noct_register_bytecode(
		env,
		(uint8_t *)payload,
		registration_size);
}

/* Resolve and call the public main function with pinned host slots. */
static bool
test_run_main(
	NoctEnv *env)
{
	NoctValue global;
	NoctValue result;
	NoctFunc *function;
	bool pinned;
	bool succeeded;

	memset(&global, 0, sizeof(global));
	memset(&result, 0, sizeof(result));
	pinned = false;
	succeeded = false;

	if (!noct_pin_local(env, 2, &global, &result))
		goto cleanup;
	pinned = true;
	if (!noct_get_global(env, "main", &global))
		goto cleanup;
	if (!noct_get_func(env, &global, &function))
		goto cleanup;
	if (!noct_call(env, function, 0, NULL, &result))
		goto cleanup;

	succeeded = true;

cleanup:
	if (pinned && !noct_unpin_local(env, 2, &global, &result))
		succeeded = false;

	return succeeded;
}

/* Check resolver counts for external modules or a self-contained app. */
static bool
test_check_resolver_counts(
	enum test_mode mode)
{
	uint32_t expected_total;
	uint32_t expected_count;
	uint32_t i;

	expected_count = mode == TEST_MODE_APP ? 0 : 1;
	expected_total = mode == TEST_MODE_APP ? 0 : mapping_count;
	if (resolver_call_count != expected_total) {
		fprintf(
			stderr,
			"Resolver total was %u, expected %u.\n",
			resolver_call_count,
			expected_total);
		return false;
	}

	/* Require one call per supplied name, or none for an application. */
	for (i = 0; i < mapping_count; i++) {
		if (mapping_table[i].call_count != expected_count) {
			fprintf(
				stderr,
				"Resolver count for %s was %u, expected %u.\n",
				mapping_table[i].name,
				mapping_table[i].call_count,
				expected_count);
			return false;
		}
	}

	return true;
}
