/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/* Bytecode backend, including the multi-source executable .nap writer. */

#include <noct/noct.h>
#include "ast.h"
#include "hir.h"
#include "lir.h"
#include "bytecode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#include <process.h>
#define bcback_getpid _getpid
#else
#include <unistd.h>
#include <sys/stat.h>
#define bcback_getpid getpid
#endif

struct app_func {
	struct lir_func *func;
	struct app_func *next;
};

struct app_name {
	char *name;
	char *file;
	struct app_name *next;
};

static FILE *fp;
static int bcback_optimize_level;
static bool bcback_simd_info;
static bool app_active;
static char *app_output;
static struct app_func *app_func_head;
static struct app_func *app_func_tail;
static struct app_name *app_init_head;
static struct app_name *app_init_tail;
static struct app_name *app_public;
static struct app_name *app_source;
static struct app_name *app_source_tail;
static uint32_t app_func_count;
static uint32_t app_init_count;
static uint32_t app_main_count;
static uint32_t app_main_params;

static char *
bcback_strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = malloc(n);
	if (p != NULL)
		memcpy(p, s, n);
	return p;
}

static bool
bcback_path_is_relative(const char *path)
{
	return path != NULL && path[0] != '\0' && path[0] != '/' &&
	       path[0] != '\\' && !(path[0] != '\0' && path[1] == ':');
}

static bool
bcback_has_suffix(const char *path, const char *suffix)
{
	size_t a = strlen(path);
	size_t b = strlen(suffix);
	return a >= b && strcmp(path + a - b, suffix) == 0;
}

static char *
bcback_normalize_path(const char *path)
{
	char *copy = bcback_strdup(path);
	char *p;
	if (copy == NULL)
		return NULL;
	for (p = copy; *p != '\0'; p++)
		if (*p == '\\') *p = '/';
	return copy;
}

static char *
bcback_internal_name(const char *prefix, const char *path)
{
	static const char hex[] = "0123456789abcdef";
	size_t plen = strlen(prefix);
	size_t n = strlen(path);
	char *out = malloc(plen + n * 2 + 1);
	char *p;
	size_t i;

	if (out == NULL)
		return NULL;
	memcpy(out, prefix, plen);
	p = out + plen;
	for (i = 0; i < n; i++) {
		unsigned char c = (unsigned char)path[i];
		*p++ = hex[c >> 4];
		*p++ = hex[c & 15];
	}
	*p = '\0';
	return out;
}

static bool
bcback_write_header(FILE *out, const char *source, uint32_t count)
{
	return fprintf(out, "Noct Bytecode 1.0\nSource\n%s\n"
		       "Number Of Functions\n%u\n", source, count) >= 0;
}

static bool
bcback_write_function(FILE *out, const struct lir_func *f)
{
	uint32_t j;
	int any;

	if (fprintf(out, "Begin Function\nName\n%s\nParameters\n%u\n",
		    f->func_name, f->param_count) < 0)
		return false;
	for (j = 0; j < f->param_count; j++)
		if (fprintf(out, "%s\n", f->param_name[j]) < 0) return false;
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_type[j] >= 0) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Types\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_type[j]) < 0) return false;
	}
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_packed_type[j] >= 0) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Packed Types\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_packed_type[j]) < 0) return false;
	}
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_restricted[j]) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Restricted\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_restricted[j] ? 1 : 0) < 0)
				return false;
	}
	if (f->return_type >= 0 &&
	    fprintf(out, "Return Type\n%d\n%d\n%d\n", f->return_type,
		    f->return_packed_type, f->return_type_checked ? 1 : 0) < 0)
		return false;
	if (f->has_vector_ops && fprintf(out, "Vector Ops\n1\n") < 0)
		return false;
	if (fprintf(out, "Temporary Size\n%u\nBytecode Size\n%u\n",
		    f->tmpvar_size, f->bytecode_size) < 0)
		return false;
	if (f->bytecode_size != 0 &&
	    fwrite(f->bytecode, 1, f->bytecode_size, out) != f->bytecode_size)
		return false;
	return fprintf(out, "\nEnd Function\n") >= 0;
}

NOCT_DLL void
noct_bcback_set_optimize_level(int level)
{
	bcback_optimize_level = level;
}

NOCT_DLL void
noct_bcback_set_simd_info(bool enable)
{
	bcback_simd_info = enable;
}

NOCT_DLL bool
noct_bcback_start(const char *out_file_name)
{
	fp = fopen(out_file_name, "wb");
	if (fp == NULL) {
		printf("Failed to open file \"%s\".\n", out_file_name);
		return false;
	}
	return true;
}

NOCT_DLL bool
noct_bcback_translate(const char *source_file_name, const char *source_data)
{
	uint32_t count;
	uint32_t i;
	bool ok = false;

	if (!ast_build(source_file_name, source_data)) {
		printf(N_TR("Error: %s:%d: %s\n"), ast_get_file_name(),
		       ast_get_error_line(), ast_get_error_message());
		ast_cleanup();
		return false;
	}
	if (!hir_build()) {
		printf(N_TR("Error: %s:%d: %s\n"), hir_get_file_name(),
		       hir_get_error_line(), hir_get_error_message());
		ast_cleanup();
		return false;
	}
	lir_set_optimize_level(bcback_optimize_level);
	count = hir_get_function_count();
	if (!bcback_write_header(fp, source_file_name, count))
		goto cleanup;
	for (i = 0; i < count; i++) {
		struct hir_block *hfunc = hir_get_function(i);
		struct lir_func *lfunc;
		if (!hir_optimize_func(hfunc, bcback_optimize_level,
				       bcback_simd_info)) {
			printf(N_TR("Error: %s\n"), hir_get_error_message());
			goto cleanup;
		}
		if (!lir_build(hfunc, &lfunc)) {
			printf(N_TR("Error: %s:%d: %s\n"), lir_get_file_name(),
			       lir_get_error_line(), lir_get_error_message());
			goto cleanup;
		}
		if (!bcback_write_function(fp, lfunc)) {
			lir_cleanup(lfunc);
			goto cleanup;
		}
		lir_cleanup(lfunc);
	}
	ok = true;
cleanup:
	hir_cleanup();
	ast_cleanup();
	return ok;
}

NOCT_DLL bool
noct_bcback_finalize(void)
{
	bool ok;
	if (fp == NULL)
		return false;
	ok = fclose(fp) == 0;
	fp = NULL;
	return ok;
}

static void
app_free_names(struct app_name *n)
{
	while (n != NULL) {
		struct app_name *next = n->next;
		free(n->name);
		free(n->file);
		free(n);
		n = next;
	}
}

NOCT_DLL void
noct_bcback_app_abort(void)
{
	struct app_func *f = app_func_head;
	while (f != NULL) {
		struct app_func *next = f->next;
		lir_cleanup(f->func);
		free(f);
		f = next;
	}
	app_free_names(app_init_head);
	app_free_names(app_public);
	app_free_names(app_source);
	free(app_output);
	app_output = NULL;
	app_func_head = app_func_tail = NULL;
	app_init_head = app_init_tail = NULL;
	app_public = app_source = app_source_tail = NULL;
	app_func_count = app_init_count = app_main_count = app_main_params = 0;
	app_active = false;
}

static bool
app_add_name(struct app_name **head, struct app_name **tail,
	     const char *name, const char *file)
{
	struct app_name *n = malloc(sizeof(*n));
	if (n == NULL)
		return false;
	n->name = bcback_strdup(name);
	n->file = file != NULL ? bcback_strdup(file) : NULL;
	n->next = NULL;
	if (n->name == NULL || (file != NULL && n->file == NULL)) {
		free(n->name); free(n->file); free(n); return false;
	}
	if (*tail != NULL)
		(*tail)->next = n;
	else
		*head = n;
	*tail = n;
	return true;
}

static bool
app_register_public_fixed(const char *name, const char *file)
{
	struct app_name *n;
	for (n = app_public; n != NULL; n = n->next) {
		if (strcmp(n->name, name) == 0) {
			printf("Duplicate public symbol \"%s\": %s and %s.\n",
			       name, n->file, file);
			return false;
		}
	}
	if (strncmp(name, "__noct_nap_", 11) == 0) {
		printf("Reserved Noct App symbol \"%s\" in %s.\n", name, file);
		return false;
	}
	n = malloc(sizeof(*n));
	if (n == NULL) return false;
	n->name = bcback_strdup(name); n->file = bcback_strdup(file);
	if (n->name == NULL || n->file == NULL) {
		free(n->name); free(n->file); free(n); return false;
	}
	n->next = app_public; app_public = n;
	return true;
}

static bool
app_append_func(struct lir_func *func)
{
	struct app_func *n;
	if (app_func_count == UINT32_MAX)
		return false;
	n = malloc(sizeof(*n));
	if (n == NULL)
		return false;
	n->func = func;
	n->next = NULL;
	if (app_func_tail != NULL) app_func_tail->next = n;
	else app_func_head = n;
	app_func_tail = n;
	app_func_count++;
	return true;
}

NOCT_DLL bool
noct_bcback_app_start(const char *out_file_name)
{
	if (app_active || !bcback_path_is_relative(out_file_name) ||
	    !bcback_has_suffix(out_file_name, ".nap"))
		return false;
	app_output = bcback_normalize_path(out_file_name);
	if (app_output == NULL)
		return false;
	app_active = true;
	return true;
}

static bool
app_collect_init_symbols(struct hir_block *init, const char *file)
{
	struct hir_block *b;
	for (b = init->val.func.inner; b != NULL; b = b->succ) {
		struct hir_stmt *s;
		if (b->type != HIR_BLOCK_BASIC) break;
		for (s = b->val.basic.stmt_list; s != NULL; s = s->next) {
			const char *name;
			if (s->lhs == NULL || s->lhs->type != HIR_EXPR_TERM ||
			    s->lhs->val.term.term->type != HIR_TERM_SYMBOL)
				continue;
			name = s->lhs->val.term.term->val.symbol;
			if (name[0] != '$' && !app_register_public_fixed(name, file))
				return false;
		}
		if (b->stop) break;
	}
	return true;
}

NOCT_DLL bool
noct_bcback_app_add_source(const char *source_file_name,
			    const char *source_data)
{
	struct app_name *sn;
	char *init_name = NULL;
	uint32_t i;
	uint32_t count;
	bool ast_ready = false;
	bool hir_ready = false;
	bool ok = false;
	char *logical_name;

	if (!app_active || !bcback_path_is_relative(source_file_name) ||
	    !bcback_has_suffix(source_file_name, ".noct"))
		return false;
	logical_name = bcback_normalize_path(source_file_name);
	if (logical_name == NULL)
		return false;
	if (strcmp(logical_name, app_output) == 0) {
		free(logical_name);
		return false;
	}
	for (sn = app_source; sn != NULL; sn = sn->next) {
		if (strcmp(sn->name, logical_name) == 0) {
			printf("Duplicate Noct App input \"%s\".\n", logical_name);
			free(logical_name);
			return false;
		}
	}
	ast_ready = true;
	if (!ast_build(logical_name, source_data)) {
		printf(N_TR("Error: %s:%d: %s\n"), ast_get_file_name(),
		       ast_get_error_line(), ast_get_error_message());
		goto cleanup;
	}
	hir_ready = true;
	if (!hir_build()) {
		printf(N_TR("Error: %s:%d: %s\n"), hir_get_file_name(),
		       hir_get_error_line(), hir_get_error_message());
		goto cleanup;
	}
	count = hir_get_function_count();
	for (i = 0; i < count; i++) {
		struct hir_block *h = hir_get_function(i);
		const char *name = h->val.func.name;
		if (strncmp(name, "$init.", 6) == 0) {
			if (!app_collect_init_symbols(h, logical_name)) goto cleanup;
			init_name = bcback_internal_name("__noct_nap_file_init_",
						 logical_name);
			if (init_name == NULL || !hir_set_function_name(h, init_name))
				goto cleanup;
		} else if (strncmp(name, "$static.", 8) != 0) {
			if (!app_register_public_fixed(name, logical_name)) goto cleanup;
			if (strcmp(name, "main") == 0) {
				app_main_count++;
				app_main_params = h->val.func.param_count;
			}
		}
	}
	lir_set_optimize_level(bcback_optimize_level);
	for (i = 0; i < count; i++) {
		struct hir_block *h = hir_get_function(i);
		struct lir_func *l;
		if (!hir_optimize_func(h, bcback_optimize_level, bcback_simd_info)) {
			printf(N_TR("Error: %s\n"), hir_get_error_message()); goto cleanup;
		}
		if (!lir_build(h, &l)) {
			printf(N_TR("Error: %s:%d: %s\n"), lir_get_file_name(),
			       lir_get_error_line(), lir_get_error_message()); goto cleanup;
		}
		if (!app_append_func(l)) { lir_cleanup(l); goto cleanup; }
	}
	if (init_name != NULL) {
		if (!app_add_name(&app_init_head, &app_init_tail, init_name,
				  logical_name)) goto cleanup;
		app_init_count++;
	}
	if (!app_add_name(&app_source, &app_source_tail, logical_name, NULL))
		goto cleanup;
	ok = true;
cleanup:
	free(init_name);
	free(logical_name);
	if (hir_ready) hir_cleanup();
	if (ast_ready) ast_cleanup();
	return ok;
}

static bool
app_build_aggregate(void)
{
	const char **names;
	struct app_name *n;
	char *agg;
	uint32_t i;
	struct hir_block *h;
	struct lir_func *l;
	bool ok = false;

	names = app_init_count != 0 ? malloc(sizeof(*names) * app_init_count) : NULL;
	if (app_init_count != 0 && names == NULL) return false;
	i = 0;
	for (n = app_init_head; n != NULL; n = n->next) names[i++] = n->name;
	agg = bcback_internal_name("$init.__noct_nap_", app_output);
	if (agg == NULL) { free(names); return false; }
	if (!ast_build_app_initializer(app_output, agg, names, app_init_count))
		goto cleanup_ast;
	if (!hir_build()) {
		hir_cleanup();
		goto cleanup_ast;
	}
	h = hir_get_function(0);
	if (!hir_optimize_func(h, bcback_optimize_level, bcback_simd_info) ||
	    !lir_build(h, &l))
		goto cleanup_hir;
	if (!app_append_func(l)) { lir_cleanup(l); goto cleanup_hir; }
	ok = true;
cleanup_hir:
	hir_cleanup();
cleanup_ast:
	ast_cleanup();
	free(agg); free(names); return ok;
}

NOCT_DLL bool
noct_bcback_app_finalize(void)
{
	char *tmp;
	FILE *out;
	struct app_func *f;
	bool ok = false;

	if (!app_active)
		return false;
	if (app_func_count == 0) {
		noct_bcback_app_abort();
		return false;
	}
	if (app_main_count != 1 || app_main_params > 1) {
		printf("Noct App requires exactly one public main() with zero or one parameter.\n");
		noct_bcback_app_abort(); return false;
	}
	if (!app_build_aggregate()) { noct_bcback_app_abort(); return false; }
	tmp = malloc(strlen(app_output) + 48);
	if (tmp == NULL) { noct_bcback_app_abort(); return false; }
	sprintf(tmp, "%s.tmp.%ld", app_output, (long)bcback_getpid());
	out = fopen(tmp, "wb");
	if (out == NULL) goto done;
	if (fwrite(NOCT_APP_SHEBANG, 1, strlen(NOCT_APP_SHEBANG), out) !=
	    strlen(NOCT_APP_SHEBANG) ||
	    !bcback_write_header(out, app_output, app_func_count)) goto close_out;
	for (f = app_func_head; f != NULL; f = f->next)
		if (!bcback_write_function(out, f->func)) goto close_out;
	if (fclose(out) != 0) { out = NULL; goto done; }
	out = NULL;
#if !defined(_WIN32)
	{
		struct stat st;
		if (stat(tmp, &st) != 0 || chmod(tmp, st.st_mode | S_IXUSR) != 0)
			goto done;
	}
#endif
	if (rename(tmp, app_output) != 0) goto done;
	ok = true;
	goto done;
close_out:
	fclose(out); out = NULL;
done:
	if (!ok) remove(tmp);
	free(tmp);
	noct_bcback_app_abort();
	return ok;
}
