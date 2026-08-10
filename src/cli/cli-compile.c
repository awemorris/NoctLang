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
static bool compile_app(int argc, char *argv[], int first);
static const char *compile_require_path[64];
static uint32_t compile_require_path_count;

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
	bool app = false;
	compile_require_path_count = 0;

	/* Optional compiler diagnostics/settings before input files. */
	while (first < argc) {
		if (strcmp(argv[first], "--app") == 0) {
			if (app) {
				printf("--app may be specified only once.\n");
				return 1;
			}
			app = true;
			first++;
			continue;
		}
		if (strncmp(argv[first], "--optimize-level=", 17) == 0) {
			noct_bcback_set_optimize_level(atoi(argv[first] + 17));
			first++;
			continue;
		}
		if (strcmp(argv[first], "--simd-info") == 0) {
			noct_bcback_set_simd_info(true);
			first++;
			continue;
		}
		if (strncmp(argv[first], "--path=", 7) == 0) {
			if (argv[first][7] == '\0' ||
			    compile_require_path_count ==
			    (uint32_t)(sizeof(compile_require_path) /
				       sizeof(compile_require_path[0]))) {
				printf("Invalid --path option.\n");
				return 1;
			}
			compile_require_path[compile_require_path_count++] =
				argv[first] + 7;
			first++;
			continue;
		}
		break;
	}
	if (argc <= first) {
		show_usage();
		return 1;
	}
	if (app)
		return compile_app(argc, argv, first) ? 0 : 1;

	/* For each argument file. */
	for (i = first; i < argc; i++) {
		/* Compile a source to bytecode. */
		if (!compile_source(argv[i]))
			return 1;
	}

	return 0;
}

static bool
compile_app(int argc, char *argv[], int first)
{
	const char *output;
	int i;

	if (argc - first < 2) {
		printf("--app requires an output .nap file and at least one input .noct file.\n");
		return false;
	}
	output = argv[first];
	if (!noct_bcback_app_start(output)) {
		printf("Invalid Noct App output path: %s\n", output);
		return false;
	}
	{
		uint32_t path_index;
		for (path_index = 0; path_index < compile_require_path_count;
		     path_index++) {
			if (!noct_bcback_app_add_require_path(
				    compile_require_path[path_index])) {
				printf("Invalid or out-of-memory --path option.\n");
				noct_bcback_app_abort();
				return false;
			}
		}
	}
	for (i = first + 1; i < argc; i++) {
		char *source_data;
		size_t source_length;
		if (strcmp(output, argv[i]) == 0) {
			printf("Noct App output and input paths must differ.\n");
			noct_bcback_app_abort();
			return false;
		}
		if (!load_file_content(argv[i], &source_data, &source_length)) {
			noct_bcback_app_abort();
			return false;
		}
		if (!noct_bcback_app_add_source(argv[i], source_data)) {
			free(source_data);
			noct_bcback_app_abort();
			return false;
		}
		free(source_data);
	}
	return noct_bcback_app_finalize();
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
