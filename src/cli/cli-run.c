/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * CLI: Run Mode
 */

#include <noct/noct.h>
#if defined(NOCT_USE_BEUI)
#include <noct/beui.h>
#endif
#include "cli-main.h"
#if defined(NOCT_USE_ACCEL_DX12)
#include "../core/accel.h"
#endif

#include <errno.h>
#include <limits.h>

#if defined(NOCT_TARGET_WINDOWS)
#include <windows.h>
#elif !defined(NOCT_TARGET_DOS4G) && !defined(NOCT_TARGET_PC98BE)
#include <unistd.h>
#if defined(__linux__)
#include <dirent.h>
#endif
#endif

static NoctVM *vm;
static NoctEnv *env;
static NoctConfig config;
static NoctValue arg;
static int file_arg;
static int prog_arg;
static size_t param_count;
static bool is_oneliner;
static bool show_cpu_list;
static bool show_gpu_list;
static const char *require_path[64];
static uint32_t require_path_count;

static bool parse_options(int argc, char *argv[]);
static bool load_program(int argc, char *argv[]);
static bool load_args(int argc, char *argv[]);
static bool check_params(void);
static bool parse_nonnegative_int(const char *text, int *value);
static bool validate_cpu_affinity(const char *text);
static void print_cpu_list(void);
static void print_gpu_list(void);

/*
 * Top level function for the run mode.
 */
int command_run(int argc, char *argv[])
{
	NoctValue ret;

	noct_set_default_config(&config);

	/* Parse options. */
	if (!parse_options(argc, argv))
		return 1;
	if (show_cpu_list) {
		print_cpu_list();
		return 0;
	}
	if (show_gpu_list) {
		print_gpu_list();
		return 0;
	}

	/* Check if a file is specified. */
	if (file_arg == argc && !is_oneliner) {
		/* No file specified, enter REPL. */
		if (argc == 1) {
#if defined(NOCT_USE_REPL)
			return command_repl();
#else
			show_usage();
			return 1;
#endif
		}
		return 1;
	}

	/* Create a runtime. */
	if (!noct_create_vm(&vm, &env, &config)) {
		wide_printf(N_TR("Out of memory.\n"));
		return 1;
	}
	{
		uint32_t i;
		for (i = 0; i < require_path_count; i++) {
			if (!noct_add_require_path(vm, require_path[i])) {
				wide_printf(N_TR("Out of memory.\n"));
				noct_destroy_vm(vm);
				return 1;
			}
		}
	}

	/* Register libraries. */
	if (!noct_register_api_system(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
	if (!noct_register_api_console(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
	if (!noct_register_api_file(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#if defined(NOCT_USE_MULTITHREAD)
	if (!noct_register_api_thread(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#endif
#if defined(NOCT_USE_HTTPSERVER)
	if (!noct_register_api_httpserver(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#endif
#if defined(NOCT_USE_TERM)
	if (!noct_register_api_term(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#endif
#if defined(NOCT_USE_BEUI)
#if defined(NOCT_TARGET_PC98DOS)
	if (!noct_register_api_beui_pc98dos(env)) {
#else
	/* A host with no BeUI backend still gets the module, bound to no
	 * hardware: BeUI.init() reports failure and the rest of the API
	 * stays loadable, so a graphical script degrades instead of
	 * failing to resolve BeUI.* at all. */
	if (!noct_register_api_beui(env, NULL)) {
#endif
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#endif
#if defined(NOCT_USE_PROCESS)
	if (!noct_register_api_process(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return false;
	}
#endif

	/* Register native functions. */
	if (!register_cli_cfunc(env)) {
		wide_printf(N_TR("Out of memory.\n"));
		return 1;
	}

	/* Load program. */
	if (!load_program(argc, argv))
		return 1;

	/* Load arguments. */
	if (!load_args(argc, argv))
		return 1;

	/* Check main parameters. */
	if (!check_params())
		return 1;

	/* Run the "main()" function. */
	if (!noct_enter_vm(env,
			   "main",
			   param_count == 0 ? 0 : 1,
			   &arg,
			   &ret)) {
		const char *file, *msg;
		int line;
		noct_get_error_file(env, &file);
		noct_get_error_line(env, &line);
		noct_get_error_message(env, &msg);
		wide_printf(N_TR("%s:%d: Error: %s\n"), file, line, msg);
		return 1;
	}

	/* Destroy the runtime. */
	if (!noct_destroy_vm(vm))
		return 1;

	return 0;
}

static bool
parse_options(
	int argc,
	char *argv[])
{
	int i;
	int optimize_level;
	bool lineinfo;
	enum cli_optimize_level_result optimize_result;

	file_arg = 1;
	is_oneliner = false;
	show_cpu_list = false;
	show_gpu_list = false;
	require_path_count = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			break;

		if (strcmp(argv[i], "-j0") == 0) {
			config.jit_enable = false;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "-j") == 0) {
			config.jit_enable = true;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--cpu") == 0) {
			config.auto_parallel = 1;
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--cpu=", 6) == 0) {
			if (!parse_nonnegative_int(argv[i] + 6,
					   &config.auto_parallel)) {
				wide_printf(N_TR("Invalid --cpu option.\n"));
				return false;
			}
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--cpu-pe=", 9) == 0) {
			if (!parse_nonnegative_int(argv[i] + 9, &config.cpu_pe) ||
			    config.cpu_pe == 0) {
				wide_printf(N_TR("Invalid --cpu-pe option.\n"));
				return false;
			}
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--cpu-affinity=", 15) == 0) {
			if (!validate_cpu_affinity(argv[i] + 15)) {
				wide_printf(N_TR("Invalid --cpu-affinity option.\n"));
				return false;
			}
			config.cpu_affinity = argv[i] + 15;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--cpu-list") == 0) {
			show_cpu_list = true;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--gpu") == 0) {
			config.gpu_enable = true;
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gpu-name=", 11) == 0) {
			if (argv[i][11] == '\0') {
				wide_printf(N_TR("Invalid --gpu-name option.\n"));
				return false;
			}
			config.gpu_name = argv[i] + 11;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--gpu-list") == 0) {
			show_gpu_list = true;
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--jit-code-size=", 16) == 0) {
			config.jit_code_size = (size_t)atoi(argv[i] + 16);
			file_arg++;
			continue;
		}
		optimize_result = parse_optimize_level_option(
			argv[i], &optimize_level, &lineinfo);
		if (optimize_result == CLI_OPTIMIZE_LEVEL_VALID) {
			config.optimize_level = optimize_level;
			config.lineinfo = lineinfo;
			if (optimize_level == 9) {
				config.auto_parallel = 1;
				config.gpu_enable = true;
			}
			file_arg++;
			continue;
		}
		if (optimize_result == CLI_OPTIMIZE_LEVEL_INVALID) {
			wide_printf(N_TR("Invalid optimize-level option %s.\n"),
				    argv[i]);
			return false;
		}
		if (strcmp(argv[i], "--simd-info") == 0) {
			config.simd_info = true;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--accel=vulkan") == 0) {
#if !defined(NOCT_USE_ACCEL_VULKAN)
			wide_printf(N_TR("Vulkan accelerator support is not available in this build.\n"));
			return false;
#else
			config.accel_enable = true;
			config.accel_backend = NOCT_ACCEL_BACKEND_VULKAN;
			file_arg++;
			continue;
#endif
		}
		if (strcmp(argv[i], "--accel=opengl") == 0) {
#if !defined(NOCT_USE_ACCEL_OPENGL)
			wide_printf(N_TR("OpenGL accelerator support is not available in this build.\n"));
			return false;
#else
			config.accel_enable = true;
			config.accel_backend = NOCT_ACCEL_BACKEND_OPENGL;
			file_arg++;
			continue;
#endif
		}
		if (strcmp(argv[i], "--accel=dx12") == 0) {
#if !defined(NOCT_USE_ACCEL_DX12)
			wide_printf(N_TR("DirectX 12 accelerator support is not available in this build.\n"));
			return false;
#else
			config.accel_enable = true;
			config.accel_backend = NOCT_ACCEL_BACKEND_DX12;
			file_arg++;
			continue;
#endif
		}
		if (strcmp(argv[i], "--disable-accel") == 0) {
			config.accel_enable = false;
			config.accel_backend = NOCT_ACCEL_BACKEND_NONE;
			file_arg++;
			continue;
		}
		if (strcmp(argv[i], "--accel-info") == 0) {
			config.accel_info = true;
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--path=", 7) == 0) {
			if (argv[i][7] == '\0' ||
			    require_path_count == (uint32_t)(sizeof(require_path) /
							 sizeof(require_path[0]))) {
				wide_printf(N_TR("Invalid --path option.\n"));
				return false;
			}
			require_path[require_path_count++] = argv[i] + 7;
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gc-nursery-size=", 18) == 0) {
			config.gc_nursery_size = (size_t)atoi(argv[i] + 18);
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gc-graduate-size=", 21) == 0) {
			config.gc_graduate_size = (size_t)atoi(argv[i] + 21);
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gc-tenure-size=", 17) == 0) {
			config.gc_tenure_size = (size_t)atoi(argv[i] + 17);
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gc-lop-threshold=", 18) == 0) {
			config.gc_lop_threshold = (size_t)atoi(argv[i] + 18);
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--gc-promotion-threshold=", 25) == 0) {
			config.gc_promotion_threshold = (size_t)atoi(argv[i] + 25);
			file_arg++;
			continue;
		}
		if (strncmp(argv[i], "--one-line", 10) == 0 ||
		    strcmp(argv[i], "-e") == 0) {
			if (argc <= (int)i + 1) {
				wide_printf(N_TR("Specify a command.\n"));
				return 1;
			}
			is_oneliner = true;
			prog_arg = i + 1;
			i++;
			file_arg++;
			continue;
		}

		wide_printf(N_TR("Unknown option %s.\n"), argv[i]);
		return false;
	}

	return true;
}

static bool
parse_nonnegative_int(const char *text, int *value)
{
	char *end;
	long parsed;

	if (text == NULL || text[0] == '\0')
		return false;
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno == ERANGE || end == text || *end != '\0' || parsed < 0
#if LONG_MAX > INT_MAX
	    || parsed > INT_MAX
#endif
	   )
		return false;
	*value = (int)parsed;
	return true;
}

static bool
validate_cpu_affinity(const char *text)
{
	const char *p = text;

	if (p == NULL || *p == '\0')
		return false;
	for (;;) {
		if (*p < '0' || *p > '9')
			return false;
		while (*p >= '0' && *p <= '9')
			p++;
		if (*p == '\0')
			return true;
		if (*p++ != ',' || *p == '\0')
			return false;
	}
}

#if defined(__linux__)
static int
read_cpu_topology_value(int cpu, const char *name)
{
	char path[256];
	FILE *fp;
	int value = -1;

	snprintf(path, sizeof(path),
		 "/sys/devices/system/cpu/cpu%d/topology/%s", cpu, name);
	fp = fopen(path, "r");
	if (fp != NULL) {
		if (fscanf(fp, "%d", &value) != 1)
			value = -1;
		fclose(fp);
	}
	return value;
}

static int
read_cpu_numa_node(int cpu)
{
	char path[256];
	DIR *dir;
	struct dirent *entry;
	int node = -1;

	snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d", cpu);
	dir = opendir(path);
	if (dir == NULL)
		return -1;
	while ((entry = readdir(dir)) != NULL) {
		if (strncmp(entry->d_name, "node", 4) == 0 &&
		    entry->d_name[4] >= '0' && entry->d_name[4] <= '9') {
			node = atoi(entry->d_name + 4);
			break;
		}
	}
	closedir(dir);
	return node;
}
#endif

static void
print_cpu_list(void)
{
	int count;
	int i;

#if defined(NOCT_TARGET_WINDOWS)
	count = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#elif defined(NOCT_TARGET_DOS4G) || defined(NOCT_TARGET_PC98BE)
	count = 1;
#else
	count = (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
	if (count < 1)
		count = 1;
	wide_printf("Logical CPUs: %d\n", count);
	wide_printf("ID  NUMA  Package  Core  SMT\n");
	for (i = 0; i < count; i++) {
#if defined(__linux__)
		int package = read_cpu_topology_value(i, "physical_package_id");
		int core = read_cpu_topology_value(i, "core_id");
		int node = read_cpu_numa_node(i);
		int smt = 0;
		int j;
		for (j = 0; j < i; j++) {
			if (read_cpu_topology_value(j, "physical_package_id") ==
				package &&
			    read_cpu_topology_value(j, "core_id") == core)
				smt++;
		}
		wide_printf("%d  %d     %d        %d     %d\n",
			    i, node, package, core, smt);
#else
		wide_printf("%d  ?     ?        ?     ?\n", i);
#endif
	}
}

static void
print_gpu_list(void)
{
#if defined(NOCT_USE_ACCEL_DX12)
	if (accel_dx12_list_devices())
		return;
	wide_printf(N_TR("No compatible DirectX 12 adapters are available.\n"));
#else
	wide_printf(N_TR("GPU backend is not linked; no devices are available.\n"));
#endif
}

static bool
load_program(
	int argc,
	char *argv[])
{
	static char entire[32768];
	char *data;
	size_t len;

	UNUSED_PARAMETER(argc);

	/* Load an one liner if exists. */
	if (is_oneliner) {
		/* Make a function. */
		snprintf(entire, sizeof(entire), "func main() { %s; }", argv[prog_arg]);
		if (!noct_register_source(env, "oneliner", entire)) {
			const char *file, *msg;
			int line;

			noct_get_error_file(env, &file);
			noct_get_error_line(env, &line);
			noct_get_error_message(env, &msg);
			wide_printf(N_TR("%s:%d: Error: %s\n"), file, line, msg);
			return false;
		}
		return true;
	}

	/* Load a file content. */
	if (!load_file_content(argv[file_arg], &data, &len))
		return false;

	/* Check for raw bytecode or the exact executable .nap prefix. */
	if (!((len >= strlen(NOCT_BYTECODE_HEADER) &&
	       memcmp(data, NOCT_BYTECODE_HEADER,
		      strlen(NOCT_BYTECODE_HEADER)) == 0) ||
	      (len >= strlen(NOCT_APP_SHEBANG) + strlen(NOCT_BYTECODE_HEADER) &&
	       memcmp(data, NOCT_APP_SHEBANG, strlen(NOCT_APP_SHEBANG)) == 0 &&
	       memcmp(data + strlen(NOCT_APP_SHEBANG), NOCT_BYTECODE_HEADER,
		      strlen(NOCT_BYTECODE_HEADER)) == 0))) {
		/* It's a source file. */
		if (!noct_register_source(env, argv[file_arg], data)) {
			const char *file, *msg;
			int line;

			noct_get_error_file(env, &file);
			noct_get_error_line(env, &line);
			noct_get_error_message(env, &msg);
			wide_printf(N_TR("%s:%d: Error: %s\n"), file, line, msg);
			free(data);
			return false;
		}
	} else {
		/* It's a bytecode file. */
		if (!noct_register_bytecode(env, (void *)data, (uint32_t)len)) {
			const char *file, *msg;
			int line;

			noct_get_error_file(env, &file);
			noct_get_error_line(env, &line);
			noct_get_error_message(env, &msg);
			wide_printf(N_TR("%s:%d: Error: %s\n"), file, line, msg);
			free(data);
			return false;
		}
	}

	free(data);
	return true;
}

static bool
load_args(
	int argc,
	char *argv[])
{
	NoctValue val;

	/* Make the arguments for "main()". */
        if (!noct_make_empty_array(env, &arg))
                return false;

#if defined(NOCT_TARGET_WINDOWS)
	{
		int i;
		size_t index;

		index = 0;
		for (i = file_arg + 1; i < __argc; i++) {
			const wchar_t *wstr = __wargv[i];
			char *utf8_buf = NULL;
			int size_needed = 0;

			size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
			if (size_needed <= 0)
				return false;

			utf8_buf = malloc(size_needed);
			if (!utf8_buf)
				return false;

			WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8_buf, size_needed, NULL, NULL);

			if (!noct_set_array_elem_make_string(env, &arg, index++, &val, utf8_buf)) {
				free(utf8_buf);
				return false;
			}
			free(utf8_buf);
		}
	}
#else
	{
		int i;
		size_t index;

		index = 0;
		for (i = file_arg + 1; i < argc; i++) {
			if (!noct_set_array_elem_make_string(env, &arg, index++, &val, argv[i]))
				return false;
		}

	}
#endif

	return true;
}

static bool
check_params(void)
{
        NoctValue main_val;
	NoctFunc *main_func;

	/* Check for main(). */
	if (!noct_get_global(env, "main", &main_val)) {
		wide_printf(N_TR("main() is not defined\n"));
		return false;
	}

	if (!noct_get_func(env, &main_val, &main_func))
		return false;

	if (!noct_get_func_param_count(env, main_func, &param_count))
		return false;

	return true;
}
