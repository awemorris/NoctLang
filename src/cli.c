/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Copyright (c) 2025, Awe Morris. All rights reserved.
 */

#include "runtime.h"
#include "ast.h"
#include "hir.h"
#include "lir.h"
#include "cback.h"
#include "elback.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <locale.h>
#include <assert.h>

#ifdef TARGET_WINDOWS
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

/* Runtime's configuration. */
extern bool noct_conf_use_jit;

/* REPL mode. */
extern bool noct_conf_repl_mode;

/* Language code for translation. */
#ifdef USE_GETTEXT_COMPAT
const char *lang_code = "en";
#endif

/* Language runtime. */
static struct rt_env *rt;

/* Forward declaration. */
#if defined(USE_GETTEXT_COMPAT)
static void init_locale(void);
#endif
static void show_usage(void);
static int command_compile(int argc, char *argv[]);
static int command_transpile_c(int argc, char *argv[]);
static int command_transpile_elisp(int argc, char *argv[]);
static int command_run(int argc, char *argv[]);
static int command_repl(void);
static bool compile_source(const char *file_name);
static bool do_transpile_c(const char *out_file, int in_file_count, const char *in_file[]);
static bool add_file_hook_c(const char *fname);
static bool do_transpile_elisp(const char *out_file, int in_file_count, const char *in_file[]);
static bool add_file_hook_elisp(const char *fname);
static bool add_file(const char *fname, bool (*add_file_hook)(const char *));
static bool is_block_start(const char *text, bool *is_func);
static bool accept_func(const char *text);
static bool load_file_content(const char *fname, char **data, size_t *size);
static int wide_printf(const char *format, ...);
static bool register_ffi(struct rt_env *rt);

/*
 * Main
 */

int main(int argc, char *argv[])
{
	char *first_arg;

#if defined(USE_GETTEXT_COMPAT)
	init_locale();
#endif

	if (argc >= 2 && strcmp(argv[1], "--help") == 0 ) {
		show_usage();
		return 1;
	}

	/* REPL */
	if (argc == 1 ||
	    (argc == 2 && strcmp(argv[1], "--disable-jit") == 0))
		return command_repl();

	/* Command */
	first_arg = argv[1];
	if (strcmp(first_arg, "--compile") == 0)
		return command_compile(argc, argv);
	if (strcmp(first_arg, "--ansic") == 0)
		return command_transpile_c(argc, argv);
	if (strcmp(first_arg, "--elisp") == 0)
		return command_transpile_elisp(argc, argv);

	/* Run */
	return command_run(argc, argv);
}

#ifdef _WIN32
#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    /* Dispatch to main(). */
    int argc, i;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    char **argv = malloc(sizeof(char *) * argc);
    for (i = 0; i < argc; i++)
    {
        char tmp[1024];
	WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, tmp, sizeof(tmp) - 1, NULL, NULL);
        argv[i] = strdup(tmp);
    }
    return main(argc, argv);
}
#endif

static void show_usage(void)
{
	wide_printf(_("Usage\n"));
	wide_printf(_("  noct <file>                        ... run a program\n"));
	wide_printf(_("  noct --compile <files>             ... convert to bytecode files\n"));
	wide_printf(_("  noct --ansic <out file> <in files> ... convert to a C source file\n"));
	wide_printf(_("  noct --elisp <out file> <in files> ... convert to an Emacs Lisp source file\n"));
}

/*
 * Translation
 */

/* Called from _() of the pseudo gettext(). */
#ifdef USE_GETTEXT_COMPAT
const char *get_system_language(void)
{
	return lang_code;
}
#endif

/* Initialized the locale. */
#if defined(USE_GETTEXT_COMPAT)
static void init_locale(void)
{
	const char *locale;

	locale = setlocale(LC_ALL, "");

	if (locale == NULL || locale[0] == '\0' || locale[1] == '\0') {
		lang_code = "en";
	}else if (strncmp(locale, "en", 2) == 0)
		lang_code = "en";
	else if (strncmp(locale, "fr", 2) == 0)
		lang_code = "fr";
	else if (strncmp(locale, "de", 2) == 0)
		lang_code = "de";
	else if (strncmp(locale, "it", 2) == 0)
		lang_code = "it";
	else if (strncmp(locale, "es", 2) == 0)
		lang_code = "es";
	else if (strncmp(locale, "el", 2) == 0)
		lang_code = "el";
	else if (strncmp(locale, "ru", 2) == 0)
		lang_code = "ru";
	else if (strncmp(locale, "zh_CN", 5) == 0)
		lang_code = "zh";
	else if (strncmp(locale, "zh_TW", 5) == 0)
		lang_code = "tw";
	else if (strncmp(locale, "ja", 2) == 0)
		lang_code = "ja";
	else
		lang_code = "en";

	setlocale(LC_ALL, "C");
}
#endif

/*
 * Compile
 */

static int command_compile(int argc, char *argv[])
{
	int i;

	/* For each argument file. */
	for (i = 2; i < argc; i++) {
		/* Compile a source to bytecode. */
		if (!compile_source(argv[i]))
			return 1;
	}

	return 1;
}

static bool compile_source(const char *file_name)
{
	char bc_fname[1024];
	FILE *fp;
	char *source_data, *dot;
	size_t source_length;
	int func_count, i, j;

	/* Load an argument source file. */
	if (!load_file_content(file_name, &source_data, &source_length))
		return false;

	/* Do parse, build AST. */
	if (!ast_build(file_name, source_data)) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    ast_get_file_name(),
			    ast_get_error_line(),
			    ast_get_error_message());
		return false;
	}

	/* Transform AST to HIR. */
	if (!hir_build()) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    hir_get_file_name(),
			    hir_get_error_line(),
			    hir_get_error_message());
		return false;
	}

	/* Make an output file name. (*.nb) */
	strcpy(bc_fname, file_name);
	dot = strstr(bc_fname, ".");
	if (dot != NULL)
		strcpy(dot, ".nb");
	else
		strcat(bc_fname, ".nb");

	/* Open an output .nb bytecode file. */
	fp = fopen(bc_fname, "wb");
	if (fp == NULL) {
		wide_printf(_("Cannot open file %s.\n"), bc_fname);
		return false;
	}

	/* Put a file header. */
	func_count = hir_get_function_count();
	fprintf(fp, "Noct Bytecode 1.0\n");
	fprintf(fp, "Source\n");
	fprintf(fp, "%s\n", file_name);
	fprintf(fp, "Number Of Functions\n");
	fprintf(fp, "%d\n", func_count);

	/* For each HIR function. */
	for (i = 0; i < func_count; i++) {
		struct hir_block *hfunc;
		struct lir_func *lfunc;

		/* Transform HIR to LIR (bytecode). */
		hfunc = hir_get_function(i);
		if (!lir_build(hfunc, &lfunc)) {
			wide_printf(_("Error: %s: %d: %s\n"),
				    lir_get_file_name(),
				    lir_get_error_line(),
				    lir_get_error_message());
			return false;
		}

		/* Put a bytcode. */
		fprintf(fp, "Begin Function\n");
		fprintf(fp, "Name\n");
		fprintf(fp, "%s\n", lfunc->func_name);
		fprintf(fp, "Parameters\n");
		fprintf(fp, "%d\n", lfunc->param_count);
		for (j = 0; j < lfunc->param_count; j++)
			fprintf(fp, "%s\n", lfunc->param_name[j]);
		fprintf(fp, "Local Size\n");
		fprintf(fp, "%d\n", lfunc->tmpvar_size);
		fprintf(fp, "Bytecode Size\n");
		fprintf(fp, "%d\n", lfunc->bytecode_size);
		fwrite(lfunc->bytecode, (size_t)lfunc->bytecode_size, 1, fp);
		fprintf(fp, "\nEnd Function\n");

		/* Free a single LIR. */
		lir_free(lfunc);
	}

	fclose(fp);

	/* Free entire HIR. */
	hir_free();

	return true;
}

/*
 * Transpile (C)
 */

static int command_transpile_c(int argc, char *argv[])
{
	if (argc < 4) {
		show_usage();
		return 1;
	}

	if (!do_transpile_c(argv[2], argc - 3, (const char **)&argv[3]))
		return 1;

	return 0;
}

static bool do_transpile_c(const char *out_file, int in_file_count, const char *in_file[])
{
	int i;

	/* Initialize the backend. */
	if (!cback_init(out_file))
		return false;

	/* For each input file or directory. */
	for (i = 0; i < in_file_count; i++) {
		if (!add_file(in_file[i], add_file_hook_c))
			return false;
	}

	/* Put a epilogue code. */
	if (!cback_finalize_dll())
		return false;

	return true;
}

static bool add_file_hook_c(const char *fname)
{
	char *data;
	size_t len;
	int func_count, j;

	/* Load an argument file. */
	if (!load_file_content(fname, &data, &len))
		return false;

	/* Do parse, build AST. */
	if (!ast_build(fname, data)) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    ast_get_file_name(),
			    ast_get_error_line(),
			    ast_get_error_message());
		return false;
	}

	/* Transform AST to HIR. */
	if (!hir_build()) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    hir_get_file_name(),
			    hir_get_error_line(),
			    hir_get_error_message());
		return false;
	}

	/* For each HIR function. */
	func_count = hir_get_function_count();
	for (j = 0; j < func_count; j++) {
		struct hir_block *hfunc;
		struct lir_func *lfunc;

		/* Transform HIR to LIR (bytecode). */
		hfunc = hir_get_function(j);
		if (!lir_build(hfunc, &lfunc)) {
			wide_printf(_("Error: %s: %d: %s\n"),
				    lir_get_file_name(),
				    lir_get_error_line(),
				    lir_get_error_message());
			return false;;
		}

		/* Put a C function. */
		if (!cback_translate_func(lfunc))
			return false;

		/* Free a single LIR. */
		lir_free(lfunc);
	}

	/* Free entire HIR. */
	hir_free();

	return true;
}

/*
 * Transpile (Emacs Lisp)
 */

static int command_transpile_elisp(int argc, char *argv[])
{
	if (argc < 4) {
		show_usage();
		return 1;
	}

	if (!do_transpile_elisp(argv[2], argc - 3, (const char **)&argv[3]))
		return 1;

	return 0;
}

static bool do_transpile_elisp(const char *out_file, int in_file_count, const char *in_file[])
{
	int i;

	/* Initialize the backend. */
	if (!elback_init(out_file))
		return false;

	/* For each input file or directory. */
	for (i = 0; i < in_file_count; i++) {
		if (!add_file(in_file[i], add_file_hook_elisp))
			return false;
	}

	/* Put a epilogue code. */
	if (!elback_finalize())
		return false;

	return true;
}

static bool add_file_hook_elisp(const char *fname)
{
	char *data;
	size_t len;
	int func_count, j;

	/* Load a file. */
	if (!load_file_content(fname, &data, &len))
		return false;

	/* Do parse, build AST. */
	if (!ast_build(fname, data)) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    ast_get_file_name(),
			    ast_get_error_line(),
			    ast_get_error_message());
		return false;
	}

	/* Transform AST to HIR. */
	if (!hir_build()) {
		wide_printf(_("Error: %s: %d: %s\n"),
			    hir_get_file_name(),
			    hir_get_error_line(),
			    hir_get_error_message());
		return false;
	}

	/* For each HIR function. */
	func_count = hir_get_function_count();
	for (j = 0; j < func_count; j++) {
		struct hir_block *hfunc;
		struct lir_func *lfunc;

		/* Put Emacs Lisp. */
		hfunc = hir_get_function(j);
		if (!elback_translate_func(hfunc))
			return false;
	}

	/* Free entire HIR. */
	hir_free();

	return true;
}

/*
 * Recursive Transpile
 */


/*
 * For Windows:
 */
#if defined(TARGET_WINDOWS)

#define BUF_SIZE	1024

static wchar_t wszMessage[BUF_SIZE];
static char szMessage[BUF_SIZE];

const wchar_t *win32_utf8_to_utf16(const char *utf8_message)
{
	assert(utf8_message != NULL);
	MultiByteToWideChar(CP_UTF8, 0, utf8_message, -1, wszMessage, BUF_SIZE - 1);
	return wszMessage;
}

const char *win32_utf16_to_utf8(const wchar_t *utf16_message)
{
	assert(utf16_message != NULL);
	WideCharToMultiByte(CP_UTF8, 0, utf16_message, -1, szMessage, BUF_SIZE - 1, NULL, NULL);
	return szMessage;
}

/* Get file list in directory (for Windows) */
static bool add_file(const char *fname, bool (*add_file_hook)(const char *))
{
	WIN32_FIND_DATAW wfd;
	HANDLE hFind;
	DWORD dwAttr;

	dwAttr = GetFileAttributesW(win32_utf8_to_utf16(fname));
	if (dwAttr == INVALID_FILE_ATTRIBUTES)
		return false;
	if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
	{
		wchar_t wszFindPath[BUF_SIZE];

		_snwprintf(wszFindPath, BUF_SIZE, L"%s\\*.*", win32_utf8_to_utf16(fname));

		hFind = FindFirstFileW(wszFindPath, &wfd);
		if(hFind == INVALID_HANDLE_VALUE)
			return false;
		do
		{
			wchar_t wszNextPath[BUF_SIZE];

			if (wcscmp(wfd.cFileName, L".") == 0)
				continue;
			if (wcscmp(wfd.cFileName, L"..") == 0)
				continue;

			_snwprintf(wszNextPath, BUF_SIZE, L"%s\\%s", win32_utf8_to_utf16(fname), wfd.cFileName);
			if (!add_file(win32_utf16_to_utf8(wszNextPath), add_file_hook))
				return false;
		}
		while(FindNextFileW(hFind, &wfd));
	}
	else
	{
		wide_printf(_("Adding file %s\n"), fname);
		if (!add_file_hook(fname))
			return false;
	}

	return true;
}

/*
 * For macOS and Linux:
 */
#else

/* Get directory file list (for Mac and Linux) */
static bool add_file(const char *fname, bool (*add_file_hook)(const char *))
{
	struct stat st;

	if (stat(fname, &st) != 0) {
		printf(_("Cannot find %s.\n"), fname);
		return false;
	}
	if (S_ISDIR(st.st_mode)) {
		struct dirent **names;
		int count;
		int i;

		printf(_("Searching directory %s.\n"), fname);

		/* Get directory content. */
		count = scandir(fname, &names, NULL, alphasort);
		if (count < 0) {
			wide_printf(_("Skipping empty directory %s.\n"), fname);
			return false;
		}

		for (i = 0; i < count; i++) {
			char next_path[256 + 1];

			if (strcmp(names[i]->d_name, ".") == 0)
				continue;
			if (strcmp(names[i]->d_name, "..") == 0)
				continue;

			snprintf(next_path, sizeof(next_path), "%s/%s", fname, names[i]->d_name);
			if (!add_file(next_path, add_file_hook))
				return false;
		}
	} else if(S_ISREG(st.st_mode)) {
		wide_printf(_("Adding file %s.\n"), fname);
		if (!add_file_hook(fname))
			return false;
	}

	return true;
}

#endif

/*
 * Run
 */

int command_run(int argc, char *argv[])
{
	struct rt_value ret;
	int file_arg;
	int i;
	char *data;
	size_t len;
	int arg_count;
	struct rt_value *arg_value;

	/* Parse options. */
	file_arg = 1;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			break;

		if (strcmp(argv[1], "--disable-jit") == 0) {
			noct_conf_use_jit = false;
			file_arg++;
			continue;
		}

		wide_printf(_("Unknown option %s.\n"), argv[1]);
		return 1;
	}

	/* Check if a file is specified. */
	if (file_arg == argc) {
		wide_printf(_("Specify a file.\n"));
		return 1;
	}

	/* Create a runtime. */
	if (!noct_create(&rt))
		return 1;

	/* Register FFI functions. */
	if (!register_ffi(rt))
		return 1;

	/* Load a source file content. */
	if (!load_file_content(argv[i], &data, &len))
		return 1;

	/* Compile a source file. */
	if (!noct_register_source(rt, argv[i], data)) {
		wide_printf(_("%s:%d: error: %s\n"),
			    noct_get_error_file(rt),
			    noct_get_error_line(rt),
			    noct_get_error_message(rt));
		return 1;
	}

	/* Make the args. */
	arg_count = argc - file_arg - 1;
	if (arg_count > 0) {
		arg_value = malloc(sizeof(struct rt_value) * arg_count);
		if (arg_value == NULL)
			return 1;
		for (i = 0; i < arg_count; i++) {
			if (!noct_make_string(rt, &arg_value[i], argv[file_arg + i + 1]))
				return 1;
		}
	} else {
		arg_value = NULL;
	}

	/* Run the "main()" function. */
	if (!noct_call_with_name(rt, "main", NULL, arg_count, arg_value, &ret)) {
		wide_printf(_("%s:%d: error: %s\n"),
			      noct_get_error_file(rt),
			      noct_get_error_line(rt),
			      noct_get_error_message(rt));
		return 1;
	}

	/* Destroy the runtime. */
	if (!noct_destroy(rt))
		return 1;

	return 0;
}

/*
 * REPL
 */

int command_repl(void)
{
	NoctEnv *rt;

	/* Will make variables global. */
	noct_conf_repl_mode = true;

	/* No JIT. */
	noct_conf_use_jit = false;

	/* Create a runtime. */
	if (!noct_create(&rt))
		return 1;

	/* Register FFI functions. */
	if (!register_ffi(rt))
		return 1;

	wide_printf(_("Noct Programming Language version 0.1\n"));
	wide_printf(_("REPL mode enabled.\n"));

	while (1) {
		char line[4096];
		char entire[32768];
		char *start;
		bool is_func;
		NoctValue ret, zero;

		memset(line, 0, sizeof(line));
		memset(entire, 0, sizeof(entire));

		printf("> ");
		if (fgets(line, sizeof(line) - 1, stdin) == NULL)
			break;

		if (!is_block_start(line, &is_func)) {
			/* Single line. */
			strncpy(entire, "func repl() {", sizeof(entire) - 1);
			strncat(entire, line, sizeof(entire) - 1);
			strncat(entire, "; }", sizeof(entire) - 1);

			/* Compile the source. */
			if (!noct_register_source(rt, "REPL", entire)) {
				wide_printf(_("%s:%d: error: %s\n"),
					    noct_get_error_file(rt),
					    noct_get_error_line(rt),
					    noct_get_error_message(rt));
				continue;
			}

			/* Run the "repl()" function. */
			if (!noct_call_with_name(rt, "repl", NULL, 0, NULL, &ret)) {
				wide_printf(_("%s:%d: error: %s\n"),
					    noct_get_error_file(rt),
					    noct_get_error_line(rt),
					    noct_get_error_message(rt));
				continue;
			}
		} else {
			if (!is_func)
				strncpy(entire, "func repl() {", sizeof(entire) - 1);
			else
				strcpy(entire, "");
			start = &entire[strlen(entire)];

			/* Multiple lines. */
			strncat(entire, line, sizeof(entire) - 1);
			while (!accept_func(start)) {
				printf(". ");
				if (fgets(line, sizeof(line) - 1, stdin) == NULL)
					break;
				strncat(entire, line, sizeof(entire) - 1);
			}

			if (!is_func)
				strncat(entire, "}", sizeof(entire) - 1);

			/* Compile the source. */
			if (!noct_register_source(rt, "REPL", entire)) {
				wide_printf(_("%s:%d: error: %s\n"),
					    noct_get_error_file(rt),
					    noct_get_error_line(rt),
					    noct_get_error_message(rt));
				continue;
			}

			if (!is_func) {
				/* Run the "repl()" function. */
				if (!noct_call_with_name(rt, "repl", NULL, 0, NULL, &ret)) {
					wide_printf(_("%s:%d: error: %s\n"),
						    noct_get_error_file(rt),
						    noct_get_error_line(rt),
						    noct_get_error_message(rt));
					continue;
				}
			}
		}


		/* Make the "repl()" function updatable. */
		noct_make_int(&zero, 0);
		if (!noct_set_global(rt, "repl", &zero))
			return 1;
	}

	/* Destroy the runtime. */
	if (!noct_destroy(rt))
		return 1;

	return 0;
}

static bool is_block_start(const char *text, bool *is_func)
{
	const char *s;

	s = text;
	while (*s == ' ')
		s++;

	*is_func = false;
	if (strncmp(s, "func", 4) == 0) {
		if (s[4] == ' ' || s[4] == '\t' || s[4] == '\n' || s[4] == '(') {
			*is_func = true;
			return true;
		}
	}
	if (strncmp(s, "if", 2) == 0) {
		if (s[2] == ' ' || s[2] == '\t' || s[2] == '\n' || s[2] == '(')
			return true;
	}
	if (strncmp(s, "for", 2) == 0) {
		if (s[3] == ' ' || s[3] == '\t' || s[3] == '\n' || s[3] == '(')
			return true;
	}
	if (strncmp(s, "while", 5) == 0) {
		if (s[5] == ' ' || s[5] == '\t' || s[5] == '\n' || s[5] == '(')
			return true;
	}

	return false;
}

static bool accept_func(const char *text)
{
	int open, close;
	const char *s;

	open = 0;
	close = 0;
	s = text;
	while(*s) {
		if (*s == '{')
			open++;
		else if (*s == '}')
			close++;
		s++;
	}

	if (open > 0 && open == close)
		return true; /* Matched. */

	return false;
}

/*
 * Helpers
 */

static bool load_file_content(const char *fname, char **data, size_t *size)
{
	FILE *fp;
	long pos;

	/* Open the file. */
	fp = fopen(fname, "rb");
	if (fp == NULL) {
		wide_printf(_("Cannot open file %s.\n"), fname);
		return false;
	}

	/* Get the file size. */
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Allocate a buffer. */
	*data = malloc(*size + 1);
	if (*data == NULL) {
		wide_printf(_("Out of memory.\n"));
		return false;
	}

	/* Read the data. */
	if (fread(*data, 1, *size, fp) != *size) {
		wide_printf(_("Cannot read file %s.\n"), fname);
		return false;
	}

	/* Terminate the string. */
	(*data)[*size] = '\0';

	fclose(fp);

	return true;
}

/* Print to console. (supports wide characters) */
static int wide_printf(const char *format, ...)
{
	static char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

#if defined(_WIN32)
	/* Use wprintf() and wide-string. (Otherwise, we'll see garbages.) */
	static wchar_t wbuf[4096];
	MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, sizeof(wbuf));
	return wprintf(L"%ls", wbuf);
#else
	return printf("%s", buf);
#endif
}

/*
 * FFI Functions
 */

/* FFI function implementation. */
static bool cfunc_import(struct rt_env *rt);
static bool cfunc_print(struct rt_env *rt);
static bool cfunc_readline(struct rt_env *rt);
static bool cfunc_readint(struct rt_env *rt);
static bool cfunc_readfilelines(struct rt_env *rt);
static bool cfunc_writefilelines(struct rt_env *rt);
static bool cfunc_shell(struct rt_env *rt);

/* FFI table. */
struct ffi_item {
	const char *name;
	int param_count;
	const char *param[NOCT_ARG_MAX];
	bool (*cfunc)(struct rt_env *rt);
} ffi_items[] = {
	{"import", 1, {"file"}, cfunc_import},
	{"print", 1, {"msg"}, cfunc_print},
	{"readline", 0, {}, cfunc_readline},
	{"readint", 0, {}, cfunc_readint},
	{"readfilelines", 1, {"file"}, cfunc_readfilelines},
	{"writefilelines", 2, {"file", "lines"}, cfunc_writefilelines},
	{"shell", 1, {"cmd"}, cfunc_shell},
};

/* Register FFI functions. */
static bool
register_ffi(
	struct rt_env *rt)
{
	int i;

	for (i = 0; i < (int)(sizeof(ffi_items) / sizeof(struct ffi_item)); i++) {
		if (!noct_register_cfunc(rt,
					 ffi_items[i].name,
					 ffi_items[i].param_count,
					 ffi_items[i].param,
					 ffi_items[i].cfunc,
					 NULL))
			return false;
	}

	return true;
}

/* Implementation of import() */
static bool
cfunc_import(
	struct rt_env *rt)
{
	const char *file;
	char *data;
	size_t len;

	if (!noct_get_arg_string(rt, 0, &file))
		return false;

	/* Load a source file content. */
	if (!load_file_content(file, &data, &len))
		return false;

	/* Compile a source file. */
	if (!noct_register_source(rt, file, data)) {
		wide_printf("%s:%d: error: %s\n",
			    noct_get_error_file(rt),
			    noct_get_error_line(rt),
			    noct_get_error_message(rt));
		return false;
	}

	return true;
}

/* Implementation of print() */
static bool
cfunc_print(
	struct rt_env *rt)
{
	struct rt_value msg;
	const char *s;
	float f;
	int i;
	int type;

	if (!noct_get_arg(rt, 0, &msg))
		return false;

	if (!noct_get_value_type(rt, &msg, &type))
		return false;

	switch (type) {
	case NOCT_VALUE_INT:
		if (!noct_get_int(rt, &msg, &i))
			return false;
		wide_printf("%i\n", i);
		break;
	case NOCT_VALUE_FLOAT:
		if (!noct_get_float(rt, &msg, &f))
			return false;
		wide_printf("%f\n", f);
		break;
	case NOCT_VALUE_STRING:
		if (!noct_get_string(rt, &msg, &s))
			return false;
		wide_printf("%s\n", s);
		break;
	default:
		wide_printf("[object]\n");
		break;
	}

	return true;
}

/* Implementation of readline() */
static bool
cfunc_readline(
	struct rt_env *rt)
{
	struct rt_value ret;
	char buf[1024];
	int len;

	memset(buf, 0, sizeof(buf));

	if (fgets(buf, sizeof(buf) - 1, stdin) == NULL)
		strcpy(buf, "");

	len = (int)strlen(buf);
	if (len > 0)
		buf[len - 1] = '\0';
	
	if (!noct_make_string(rt, &ret, buf))
		return false;
	if (!noct_set_return(rt, &ret))
		return false;

	return true;
}

/* Implementation of readint() */
static bool cfunc_readint(struct rt_env *rt)
{
	struct rt_value ret;
	char buf[1024];

	memset(buf, 0, sizeof(buf));

	if (fgets(buf, sizeof(buf) - 1, stdin) == NULL)
		strcpy(buf, "");
	
	if (!noct_set_return_int(rt, atoi(buf)))
		return false;

	return true;
}

/* Implementation of readfilelines() */
static bool
cfunc_readfilelines(
	struct rt_env *rt)
{
	const char *file;
	FILE *fp;
	char buf[4096];
	struct rt_value array;
	struct rt_value line;
	int index;

	if (!noct_get_arg_string(rt, 0, &file))
		return false;

	fp = fopen(file, "r");
	if (fp == NULL) {
		noct_error(rt, _("Cannon open file %s."), file);
		return false;
	}

	if (!noct_make_empty_array(rt, &array))
		return false;

	index = 0;
	while (fgets(buf, sizeof(buf) - 2, fp) != NULL) {
		int len;

		len = strlen(buf);
		if (len > 0) {
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
		}

		if (!noct_make_string(rt, &line, buf))
			return false;
		if (!noct_set_array_elem(rt, &array, index, &line))
			return false;
		index++;
	}

	fclose(fp);

	if (!noct_set_return(rt, &array))
		return false;

	return true;
}

/* Implementation of writefilelines() */
static bool
cfunc_writefilelines(
	struct rt_env *rt)
{
	const char *file;
	struct rt_value array;
	FILE *fp;
	int i, size;

	if (!noct_get_arg_string(rt, 0, &file))
		return false;
	if (!noct_get_arg_array(rt, 1, &array))
		return false;

	fp = fopen(file, "wb");
	if (fp == NULL) {
		noct_error(rt, _("Cannon open file %s."), file);
		return false;
	}

	if (!noct_get_array_size(rt, &array, &size))
		return false;

	for (i = 0; i < size; i++) {
		const char *line;

		if (!noct_get_array_elem_string(rt, &array, i, &line))
			return false;

		fprintf(fp, "%s\n", line);
	}

	fclose(fp);

	if (!noct_set_return_int(rt, 1))
		return false;

	return true;
}

/* Implementation of shell() */
static bool
cfunc_shell(
	struct rt_env *rt)
{
	struct rt_value ret;
	const char *s;
	int type;
	int cmd_ret;

	/* Get a "cmd" parameer. */
	if (!noct_get_arg_string(rt, 0, &s))
		return false;

	/* Run a command. */
	cmd_ret = system(s);

	/* Make a return value. */
	if (!noct_set_return_int(rt, cmd_ret))
		return false;

	return true;
}
