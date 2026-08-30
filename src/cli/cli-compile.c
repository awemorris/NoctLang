/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * CLI: Compile Mode
 */

#include "cli-main.h"

/* Forward declaration. */
static bool compile_source(const char *file_name);

/*
 * The top level function for the compile mode.
 */
int
command_compile(
	int argc,
	char *argv[])
{
	int i;
	int first = 2;
	int optimize_level;
	bool lineinfo;
	enum cli_optimize_level_result optimize_result;

	/* Optional compiler diagnostics/settings before input files. */
	while (first < argc) {
		optimize_result = parse_optimize_level_option(
			argv[first], &optimize_level, &lineinfo);
		if (optimize_result == CLI_OPTIMIZE_LEVEL_VALID) {
			noct_bcback_set_optimize_level(optimize_level);
			noct_bcback_set_lineinfo(lineinfo);
			first++;
			continue;
		}
		if (optimize_result == CLI_OPTIMIZE_LEVEL_INVALID) {
			printf("Invalid optimize-level option %s.\n", argv[first]);
			return 1;
		}
		if (strcmp(argv[first], "--simd-info") == 0) {
			noct_bcback_set_simd_info(true);
			first++;
			continue;
		}
		break;
	}
	if (argc <= first) {
		show_usage();
		return 1;
	}
	/* For each argument file. */
	for (i = first; i < argc; i++) {
		/* Compile a source to bytecode. */
		if (!compile_source(argv[i]))
			return 1;
	}

	return 0;
}

/* Compile a source file. */
static bool
compile_source(
	const char *file_name)
{
	char bc_fname[1024];
	char *source_data, *base, *dot, *slash, *backslash;
	size_t source_length;

	/* Load an argument source file. */
	if (!load_file_content(file_name, &source_data, &source_length))
		return false;

	/* Make an output file name. (*.nb) */
	strcpy(bc_fname, file_name);
	base = bc_fname;
	slash = strrchr(bc_fname, '/');
	backslash = strrchr(bc_fname, '\\');
	if (slash != NULL)
		base = slash + 1;
	if (backslash != NULL && backslash >= base)
		base = backslash + 1;
	dot = strrchr(base, '.');
	if (dot != NULL)
		strcpy(dot, ".nb");
	else
		strcat(bc_fname, ".nb");

	/* Start translation. */
	if (!noct_bcback_start(bc_fname)) {
		free(source_data);
		return false;
	}

	/* Translate. */
	if (!noct_bcback_translate(file_name, source_data)) {
		free(source_data);
		return false;
	}
	free(source_data);

	/* Finalize. */
	if (!noct_bcback_finalize())
		return false;

	return true;
}
