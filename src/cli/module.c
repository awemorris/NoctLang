/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * CLI: Source Module Resolver
 */

#include "cli-main.h"

/* Maximum number of --path options. */
#define CLI_MODULE_PATH_MAX	64

/* Search path lists borrowed from argv. */
static const char *cli_module_path[CLI_MODULE_PATH_MAX];
static uint32_t cli_module_path_count;

/* Forward declarations. */
static bool cli_module_name_is_valid(const char *module_name);
static bool cli_module_try_path_list(const char *path_list, const char *module_name, char **resolved_path);
static bool cli_module_try_directory(const char *directory, size_t directory_length, const char *module_name, char **resolved_path);
static bool cli_module_try_suffix(const char *directory, size_t directory_length, const char *module_name, const char *suffix, char **resolved_path);

/*
 * Reset the CLI source-module search path.
 */
void
cli_module_reset(
	void)
{
	cli_module_path_count = 0;
}

/*
 * Append a colon-separated CLI source-module search path.
 */
bool
cli_module_add_path(
	const char *path_list)
{
	if (path_list == NULL || path_list[0] == '\0')
		return false;
	if (cli_module_path_count == CLI_MODULE_PATH_MAX)
		return false;

	cli_module_path[cli_module_path_count] = path_list;
	cli_module_path_count++;

	return true;
}

/*
 * Resolve a source module using the CLI search path.
 */
char *
cli_module_resolve(
	const char *module_name)
{
	char *resolved_path;
	uint32_t i;

	if (!cli_module_name_is_valid(module_name))
		return NULL;

	/* Search the current directory before every explicit path. */
	if (cli_module_try_directory(".", 1, module_name, &resolved_path))
		return resolved_path;

	/* Search explicit path lists in command-line order. */
	for (i = 0; i < cli_module_path_count; i++) {
		if (cli_module_try_path_list(
			cli_module_path[i],
			module_name,
			&resolved_path)) {
			return resolved_path;
		}
	}

	return NULL;
}

/* Check whether a module name is safe to append to a directory. */
static bool
cli_module_name_is_valid(
	const char *module_name)
{
	const unsigned char *p;

	if (module_name == NULL || module_name[0] == '\0')
		return false;
	if (module_name[0] >= '0' && module_name[0] <= '9')
		return false;

	p = (const unsigned char *)module_name;

	/* Accept the same ASCII identifier characters as the lexer. */
	while (*p != '\0') {
		if (!(*p >= 'A' && *p <= 'Z') &&
		    !(*p >= 'a' && *p <= 'z') &&
		    !(*p >= '0' && *p <= '9') &&
		    *p != '_') {
			return false;
		}
		p++;
	}

	return true;
}

/* Search one colon-separated path list. */
static bool
cli_module_try_path_list(
	const char *path_list,
	const char *module_name,
	char **resolved_path)
{
	const char *start;
	const char *p;
	bool drive_colon;

	start = path_list;
	p = path_list;

	/* Search each non-empty directory in the path list. */
	for (;;) {
		drive_colon = false;
		if (*p == ':' && p == start + 1 &&
		    ((start[0] >= 'A' && start[0] <= 'Z') ||
		     (start[0] >= 'a' && start[0] <= 'z')) &&
		    (p[1] == '/' || p[1] == '\\')) {
			drive_colon = true;
		}

		if ((*p == ':' && !drive_colon) || *p == '\0') {
			if (p != start) {
				if (cli_module_try_directory(
					start,
					(size_t)(p - start),
					module_name,
					resolved_path)) {
					return true;
				}
			}

			if (*p == '\0')
				break;
			start = p + 1;
		}

		p++;
	}

	return false;
}

/* Search both source suffixes in one directory. */
static bool
cli_module_try_directory(
	const char *directory,
	size_t directory_length,
	const char *module_name,
	char **resolved_path)
{
	if (cli_module_try_suffix(
		directory,
		directory_length,
		module_name,
		".noct",
		resolved_path)) {
		return true;
	}

	if (cli_module_try_suffix(
		directory,
		directory_length,
		module_name,
		".nct",
		resolved_path)) {
		return true;
	}

	return false;
}

/* Test one directory, module name, and source suffix. */
static bool
cli_module_try_suffix(
	const char *directory,
	size_t directory_length,
	const char *module_name,
	const char *suffix,
	char **resolved_path)
{
	FILE *fp;
	char *candidate;
	char *p;
	size_t module_length;
	size_t suffix_length;
	size_t candidate_size;
	bool needs_separator;

	module_length = strlen(module_name);
	suffix_length = strlen(suffix);
	needs_separator = directory_length != 0 &&
		directory[directory_length - 1] != '/' &&
		directory[directory_length - 1] != '\\';

	if (directory_length > (size_t)-1 - module_length)
		return false;
	candidate_size = directory_length + module_length;
	if (needs_separator) {
		if (candidate_size == (size_t)-1)
			return false;
		candidate_size++;
	}
	if (candidate_size > (size_t)-1 - suffix_length)
		return false;
	candidate_size += suffix_length;
	if (candidate_size == (size_t)-1)
		return false;
	candidate_size++;

	candidate = malloc(candidate_size);
	if (candidate == NULL)
		return false;

	p = candidate;
	memcpy(p, directory, directory_length);
	p += directory_length;
	if (needs_separator)
		*p++ = '/';
	memcpy(p, module_name, module_length);
	p += module_length;
	memcpy(p, suffix, suffix_length + 1);

	fp = fopen(candidate, "rb");
	if (fp == NULL) {
		free(candidate);
		return false;
	}
	fclose(fp);

	*resolved_path = candidate;

	return true;
}
