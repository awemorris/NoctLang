/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/* Bytecode backend, including the multi-source executable .nap writer. */

#include <noct/noct.h>
#include "ast.h"
#include "hir.h"
#include "lir.h"
#include "bytecode.h"
#include "module.h"

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

enum app_module_state {
	APP_MODULE_LOADING,
	APP_MODULE_LOADED,
	APP_MODULE_FAILED
};

struct app_module {
	char *key;
	int state;
	struct app_module *next;
};

static FILE *fp;
static int bcback_optimize_level = 1;
static bool bcback_lineinfo = true;
static bool bcback_simd_info;
static bool app_active;
static char *app_output;
static char *app_physical_output;
static bool app_package_cache;
static struct app_func *app_func_head;
static struct app_func *app_func_tail;
static struct app_name *app_init_head;
static struct app_name *app_init_tail;
static struct app_name *app_public;
static struct app_name *app_source;
static struct app_name *app_source_tail;
static struct app_name *app_prototype_source;
static struct app_name *app_prototype_source_tail;
static uint32_t app_func_count;
static uint32_t app_init_count;
static uint32_t app_main_count;
static uint32_t app_main_params;
static struct module_paths app_require_path;
static bool app_require_path_ready;
static struct app_module *app_module_list;

static bool app_add_source_internal(const char *source_file_name,
				    const char *source_data, char *key,
				    bool explicit_input);

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
bcback_write_accel_program(FILE *out, const struct accel_program *program)
{
	uint32_t i, j;
	const struct accel_buffer_desc *buffer;
	const struct accel_kernel *kernel;
	const struct accel_program_step *step;

	if (program == NULL) return true;
	if (fprintf(out, "Accelerator Program\n%u %d %u %u %u %u %u\n",
		    program->descriptor_version, program->source_line,
		    program->outer_param_count, program->expr_count,
		    program->buffer_count, program->kernel_count,
		    program->step_count) < 0) return false;
	for (i = 0; i < program->outer_param_count; i++) {
		const struct accel_param_range *range;
		range = &program->outer_param_range[i];
		if (fprintf(out, "%u %d %d %lld %lld\n",
			    program->outer_param_effect[i], range->status,
			    range->has_access ? 1 : 0,
			    (long long)range->min_offset,
			    (long long)range->max_offset) < 0) return false;
	}
	for (i = 0; i < program->expr_count; i++)
		if (fprintf(out, "%d %d %d %d %lld\n", program->expr[i].op,
			    program->expr[i].left, program->expr[i].right,
			    program->expr[i].ref,
			    (long long)program->expr[i].value) < 0) return false;
	for (i = 0; i < program->buffer_count; i++) {
		buffer = &program->buffer[i];
		if (fprintf(out,
			    "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n%s\n",
			    buffer->id, buffer->source_line, buffer->origin,
			    buffer->outer_param, buffer->element_kind,
			    buffer->element_width, buffer->length_expr,
			    buffer->read_start_expr, buffer->read_end_expr,
			    buffer->write_start_expr, buffer->write_end_expr,
			    buffer->first_step, buffer->last_step,
			    buffer->initially_defined ? 1 : 0,
			    buffer->upload ? 1 : 0, buffer->download ? 1 : 0,
			    buffer->name) < 0) return false;
	}
	for (i = 0; i < program->kernel_count; i++) {
		kernel = program->kernel[i];
		if (fprintf(out, "%u %d %d %d %d %d %d %u\n%s\n",
			    kernel->descriptor_version, kernel->func_kind,
			    kernel->eligible ? 1 : 0, kernel->rejection_reason,
			    kernel->parallel_mode, kernel->source_line,
			    kernel->dispatch_param, kernel->param_count,
			    kernel->name) < 0) return false;
		for (j = 0; j < kernel->param_count; j++)
			if (fprintf(out, "%d %d %d %u %d %d %lld %lld\n",
				    kernel->param_type[j], kernel->param_packed_type[j],
				    kernel->param_transport[j], kernel->param_effect[j],
				    kernel->param_range[j].status,
				    kernel->param_range[j].has_access ? 1 : 0,
				    (long long)kernel->param_range[j].min_offset,
				    (long long)kernel->param_range[j].max_offset) < 0)
				return false;
		if (fprintf(out, "%u %lu %lu\n", kernel->content_hash,
			    (unsigned long)kernel->glsl_size,
			    (unsigned long)kernel->hlsl_size) < 0) return false;
		if (kernel->glsl_size != 0 &&
		    fwrite(kernel->glsl, 1, kernel->glsl_size, out) !=
			kernel->glsl_size) return false;
		if (fprintf(out, "\n") < 0) return false;
		if (kernel->hlsl_size != 0 &&
		    fwrite(kernel->hlsl, 1, kernel->hlsl_size, out) !=
			kernel->hlsl_size) return false;
		if (fprintf(out, "\n") < 0) return false;
	}
	for (i = 0; i < program->step_count; i++) {
		step = &program->step[i];
		if (fprintf(out, "%d %d %d %d %d %u %d %d %d %d %d %u\n",
			    step->kind, step->source_line, step->kernel,
			    step->fold_kernel, step->trip_expr, step->block_size,
			    step->result_buffer, step->scratch_buffer,
			    step->scratch_buffer2,
			    step->reduction_operator, step->reduction_type,
			    step->binding_count) < 0) return false;
		for (j = 0; j < step->binding_count; j++)
			if (fprintf(out, "%d %d %d\n", step->binding[j].kind,
				    step->binding[j].kernel_param,
				    step->binding[j].value) < 0) return false;
	}
	return fprintf(out, "End Accelerator Program\n") >= 0;
}

static bool
bcback_write_function(FILE *out, const struct lir_func *f)
{
	uint32_t j;
	int any;

	if (fprintf(out, "Begin Function\nName\n%s\nSource\n%s\nParameters\n%u\n",
		    f->func_name, f->file_name, f->param_count) < 0)
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
		if (f->param_accel_access[j] != ACCEL_ACCESS_NONE) any = 1;
	if (any) {
		if (fprintf(out, "Parameter Accel Access\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_accel_access[j]) < 0)
				return false;
	}
	any = 0;
	for (j = 0; j < f->param_count; j++)
		if (f->param_accel_transport[j] == ACCEL_TRANSPORT_DEVICE_PTR)
			any = 1;
	if (any) {
		if (fprintf(out, "Parameter Accel Transport\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%d\n", f->param_accel_transport[j]) < 0)
				return false;
		if (fprintf(out, "Parameter Accel Effects\n") < 0) return false;
		for (j = 0; j < f->param_count; j++)
			if (fprintf(out, "%u\n", f->param_accel_effect[j]) < 0)
				return false;
	}
	if (f->accel_kernel != NULL && f->func_kind == NOCT_FUNC_ACCEL) {
		if (fprintf(out, "Parameter Accel Ranges\n") < 0) return false;
		for (j = 0; j < f->param_count; j++) {
			const struct accel_param_range *range;
			range = &f->accel_kernel->param_range[j];
			if (fprintf(out, "%d %d %lld %lld\n", range->status,
				    range->has_access ? 1 : 0,
				    (long long)range->min_offset,
				    (long long)range->max_offset) < 0)
				return false;
		}
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
	if (f->func_kind != NOCT_FUNC_NORMAL &&
	    fprintf(out, "Function Kind\n%d\n", f->func_kind) < 0)
		return false;
	if (f->func_kind == NOCT_FUNC_FAST) {
		const struct fast_signature *sig;
		sig = &f->fast_signature;
		if (!sig->valid ||
		    fprintf(out, "Fast Signature\n%d\n%u\n",
			    sig->version, sig->param_count) < 0)
			return false;
		for (j = 0; j < sig->param_count; j++) {
			int axis;
			const struct fast_param_contract *param;
			param = &sig->param[j];
			if (fprintf(out, "%d %d %d %d",
				    param->value_type, param->packed_type,
				    param->restricted ? 1 : 0, param->rank) < 0)
				return false;
			for (axis = 0; axis < NOCT_FAST_RANK_MAX; axis++) {
				const struct fast_extent *extent;
				extent = &param->extent[axis];
				if (fprintf(out, " %d %lld %d", extent->kind,
					    (long long)extent->constant,
					    extent->param_index) < 0)
					return false;
			}
			if (fprintf(out, "\n") < 0) return false;
		}
		if (fprintf(out, "%d\n", sig->return_type) < 0)
			return false;
	}
	if (f->accel_kernel != NULL && f->accel_kernel->parallel_mode !=
	    ACCEL_PARALLEL_NOT_APPLICABLE &&
	    fprintf(out, "Accelerator Parallel Mode\n%d\n",
		    f->accel_kernel->parallel_mode) < 0)
		return false;
	if (f->func_kind != NOCT_FUNC_NORMAL && f->accel_kernel != NULL) {
		const struct accel_kernel *kernel;
		size_t glsl_size, hlsl_size;

		kernel = f->accel_kernel;
		glsl_size = kernel != NULL ? kernel->glsl_size : 0;
		hlsl_size = kernel != NULL ? kernel->hlsl_size : 0;
		if (fprintf(out, "Accelerator\n%d\n%d\n%d\n%d\n%d\n%u\n"
			    "GLSL Size\n%lu\n",
			    kernel != NULL && kernel->eligible ? 1 : 0,
			    kernel != NULL ? kernel->rejection_reason : 0,
			    kernel != NULL ? kernel->source_line : 0,
			    kernel != NULL ? kernel->output_param : -1,
			    kernel != NULL ? kernel->dispatch_param : -1,
			    kernel != NULL ? kernel->content_hash : 0,
			    (unsigned long)glsl_size) < 0)
			return false;
		if (glsl_size != 0 &&
		    fwrite(kernel->glsl, 1, glsl_size, out) != glsl_size)
			return false;
		if (fprintf(out, "\n") < 0) return false;
		if (fprintf(out, "HLSL Size\n%lu\n", (unsigned long)hlsl_size) < 0)
			return false;
		if (hlsl_size != 0 &&
		    fwrite(kernel->hlsl, 1, hlsl_size, out) != hlsl_size)
			return false;
		if (fprintf(out, "\n") < 0) return false;
	}
	if (!bcback_write_accel_program(out, f->accel_program))
		return false;
	if (f->has_fma_ops && fprintf(out, "FMA Ops\n1\n") < 0)
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
	bcback_lineinfo = level == 0;
}

NOCT_DLL void
noct_bcback_set_lineinfo(bool enable)
{
	bcback_lineinfo = enable;
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
	if (ast_get_require_count() != 0) {
		printf("%s", N_TR("Error: require is supported by --compile --app, not a standalone .nb file.\n"));
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
	lir_set_lineinfo(bcback_lineinfo);
	count = hir_get_function_count();
	if (!bcback_write_header(fp, source_file_name, count))
		goto cleanup;
	for (i = 0; i < count; i++) {
		struct hir_block *hfunc = hir_get_function(i);
		struct lir_func *lfunc;
		if (!hir_optimize_func(hfunc, bcback_optimize_level,
				       bcback_simd_info, false)) {
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
	struct app_module *m;
	while (f != NULL) {
		struct app_func *next = f->next;
		lir_cleanup(f->func);
		free(f);
		f = next;
	}
	app_free_names(app_init_head);
	app_free_names(app_public);
	app_free_names(app_source);
	app_free_names(app_prototype_source);
	m = app_module_list;
	while (m != NULL) {
		struct app_module *next = m->next;
		free(m->key);
		free(m);
		m = next;
	}
	if (app_require_path_ready)
		module_paths_cleanup(&app_require_path);
	free(app_output);
	free(app_physical_output);
	app_output = NULL;
	app_physical_output = NULL;
	app_package_cache = false;
	app_func_head = app_func_tail = NULL;
	app_init_head = app_init_tail = NULL;
	app_public = app_source = app_source_tail = NULL;
	app_prototype_source = app_prototype_source_tail = NULL;
	app_func_count = app_init_count = app_main_count = app_main_params = 0;
	app_module_list = NULL;
	app_require_path_ready = false;
	app_active = false;
	hir_fast_prototypes_reset();
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
	app_physical_output = bcback_strdup(out_file_name);
	if (app_physical_output == NULL) {
		free(app_output);
		app_output = NULL;
		return false;
	}
	if (!module_paths_init(&app_require_path)) {
		free(app_output);
		free(app_physical_output);
		app_output = NULL;
		app_physical_output = NULL;
		return false;
	}
	app_require_path_ready = true;
	hir_fast_prototypes_reset();
	app_active = true;
	return true;
}

NOCT_DLL bool
noct_bcback_package_start(const char *physical_output,
			  const char *logical_output)
{
	if (app_active || physical_output == NULL || logical_output == NULL ||
	    logical_output[0] == '\0' ||
	    !bcback_has_suffix(physical_output, ".nbp"))
		return false;
	app_output = bcback_strdup(logical_output);
	app_physical_output = bcback_strdup(physical_output);
	if (app_output == NULL || app_physical_output == NULL) {
		free(app_output);
		free(app_physical_output);
		app_output = app_physical_output = NULL;
		return false;
	}
	if (!module_paths_init(&app_require_path)) {
		free(app_output);
		free(app_physical_output);
		app_output = app_physical_output = NULL;
		return false;
	}
	app_require_path_ready = true;
	app_package_cache = true;
	hir_fast_prototypes_reset();
	app_active = true;
	return true;
}

NOCT_DLL bool
noct_bcback_app_add_require_path(const char *path_list)
{
	return app_active && module_paths_add(&app_require_path, path_list);
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

static struct app_module *
app_find_module(const char *key)
{
	struct app_module *module;
	for (module = app_module_list; module != NULL; module = module->next)
		if (strcmp(module->key, key) == 0)
			return module;
	return NULL;
}

static struct app_module *
app_new_module(char *key)
{
	struct app_module *module;
	module = malloc(sizeof(*module));
	if (module == NULL)
		return NULL;
	module->key = key;
	module->state = APP_MODULE_LOADING;
	module->next = app_module_list;
	app_module_list = module;
	return module;
}

static void
app_free_require_names(char **name, uint32_t count)
{
	uint32_t i;
	if (name == NULL)
		return;
	for (i = 0; i < count; i++)
		free(name[i]);
	free(name);
}

static bool
app_prototype_seen(const char *key)
{
	struct app_name *entry;

	for (entry = app_prototype_source; entry != NULL; entry = entry->next)
		if (strcmp(entry->name, key) == 0)
			return true;
	return false;
}

static bool
app_scan_source_internal(const char *source_file_name,
			 const char *source_data, const char *key)
{
	char *logical_name;
	char **require_name;
	uint32_t require_count;
	uint32_t i;
	bool ast_ready;
	bool ok;

	if (app_prototype_seen(key))
		return true;
	if (!app_add_name(&app_prototype_source,
			  &app_prototype_source_tail, key, NULL))
		return false;
	logical_name = NULL;
	require_name = NULL;
	require_count = 0;
	ast_ready = false;
	ok = false;
	if (!bcback_path_is_relative(source_file_name) ||
	    (!bcback_has_suffix(source_file_name, ".noct") &&
	     !bcback_has_suffix(source_file_name, ".nct")))
		goto cleanup;
	logical_name = bcback_normalize_path(source_file_name);
	if (logical_name == NULL)
		goto cleanup;
	ast_ready = true;
	if (!ast_build(logical_name, source_data)) {
		printf(N_TR("Error: %s:%d: %s\n"), ast_get_file_name(),
		       ast_get_error_line(), ast_get_error_message());
		goto cleanup;
	}
	if (!hir_fast_prototypes_collect()) {
		printf(N_TR("Error: %s:%d: %s\n"), logical_name,
		       hir_get_error_line(), hir_get_error_message());
		goto cleanup;
	}
	require_count = ast_get_require_count();
	if (require_count != 0) {
		require_name = calloc(require_count, sizeof(*require_name));
		if (require_name == NULL)
			goto cleanup;
		for (i = 0; i < require_count; i++) {
			require_name[i] = bcback_strdup(ast_get_require_name(i));
			if (require_name[i] == NULL)
				goto cleanup;
		}
	}
	ast_cleanup();
	ast_ready = false;
	for (i = 0; i < require_count; i++) {
		char *physical;
		char *dependency_logical;
		char *dependency_data;

		if (!module_resolve(&app_require_path, require_name[i], &physical,
				    &dependency_logical, &dependency_data)) {
			printf("Cannot resolve required module \"%s\" from %s.\n",
			       require_name[i], logical_name);
			goto cleanup;
		}
		if (!app_scan_source_internal(dependency_logical,
					      dependency_data, physical)) {
			free(physical);
			free(dependency_logical);
			free(dependency_data);
			goto cleanup;
		}
		free(physical);
		free(dependency_logical);
		free(dependency_data);
	}
	ok = true;
cleanup:
	if (ast_ready)
		ast_cleanup();
	app_free_require_names(require_name, require_count);
	free(logical_name);
	return ok;
}

NOCT_DLL bool
noct_bcback_app_scan_source(const char *source_file_name,
			    const char *source_data)
{
	char *key;
	bool result;

	if (!app_active)
		return false;
	key = module_path_key(source_file_name);
	if (key == NULL)
		return false;
	result = app_scan_source_internal(source_file_name, source_data, key);
	free(key);
	return result;
}

static bool
app_add_source_internal(const char *source_file_name,
			const char *source_data, char *key,
			bool explicit_input)
{
	struct app_module *module;
	char *init_name;
	char *package_init_name;
	char **require_name;
	uint32_t require_count;
	uint32_t i;
	uint32_t count;
	bool ast_ready;
	bool hir_ready;
	bool ok;
	char *logical_name;

	module = app_find_module(key);
	if (module != NULL) {
		free(key);
		if (explicit_input) {
			printf("Duplicate Noct App input \"%s\".\n",
			       source_file_name);
			return false;
		}
		if (module->state == APP_MODULE_LOADING) {
			printf("Circular require involving \"%s\".\n",
			       source_file_name);
			return false;
		}
		return module->state == APP_MODULE_LOADED;
	}
	module = app_new_module(key);
	if (module == NULL) {
		free(key);
		return false;
	}
	init_name = NULL;
	package_init_name = NULL;
	require_name = NULL;
	require_count = 0;
	ast_ready = false;
	hir_ready = false;
	ok = false;
	logical_name = NULL;

	if (!bcback_path_is_relative(source_file_name) ||
	    (!bcback_has_suffix(source_file_name, ".noct") &&
	     !bcback_has_suffix(source_file_name, ".nct")))
		goto cleanup;
	logical_name = bcback_normalize_path(source_file_name);
	if (logical_name == NULL || strcmp(logical_name, app_output) == 0)
		goto cleanup;
	ast_ready = true;
	if (!ast_build(logical_name, source_data)) {
		printf(N_TR("Error: %s:%d: %s\n"), ast_get_file_name(),
		       ast_get_error_line(), ast_get_error_message());
		goto cleanup;
	}
	if (ast_get_package_init_name() != NULL) {
		package_init_name = bcback_strdup(ast_get_package_init_name());
		if (package_init_name == NULL)
			goto cleanup;
	}
	require_count = ast_get_require_count();
	if (require_count != 0) {
		require_name = calloc(require_count, sizeof(*require_name));
		if (require_name == NULL)
			goto cleanup;
		for (i = 0; i < require_count; i++) {
			require_name[i] = bcback_strdup(ast_get_require_name(i));
			if (require_name[i] == NULL)
				goto cleanup;
		}
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
	lir_set_lineinfo(bcback_lineinfo);
	for (i = 0; i < count; i++) {
		struct hir_block *h = hir_get_function(i);
		struct lir_func *l;
		if (!hir_optimize_func(h, bcback_optimize_level, bcback_simd_info,
				       false)) {
			printf(N_TR("Error: %s\n"), hir_get_error_message()); goto cleanup;
		}
		if (!lir_build(h, &l)) {
			printf(N_TR("Error: %s:%d: %s\n"), lir_get_file_name(),
			       lir_get_error_line(), lir_get_error_message()); goto cleanup;
		}
		if (!app_append_func(l)) { lir_cleanup(l); goto cleanup; }
	}
	/* AST/HIR are global compiler state; release them before recursion. */
	hir_cleanup();
	hir_ready = false;
	ast_cleanup();
	ast_ready = false;

	for (i = 0; i < require_count; i++) {
		char *physical;
		char *dependency_logical;
		char *dependency_data;

		if (!module_resolve(&app_require_path, require_name[i], &physical,
				    &dependency_logical, &dependency_data)) {
			printf("Cannot resolve required module \"%s\" from %s.\n",
			       require_name[i], logical_name);
			goto cleanup;
		}
		if (!app_add_source_internal(dependency_logical, dependency_data,
					     physical, false)) {
			free(dependency_logical);
			free(dependency_data);
			goto cleanup;
		}
		free(dependency_logical);
		free(dependency_data);
	}

	/* Postorder gives dependency initializers precedence over importers. */
	if (init_name != NULL) {
		if (!app_add_name(&app_init_head, &app_init_tail, init_name,
				  logical_name)) goto cleanup;
		app_init_count++;
	}
	if (package_init_name != NULL) {
		if (!app_add_name(&app_init_head, &app_init_tail,
				  package_init_name, logical_name))
			goto cleanup;
		app_init_count++;
	}
	if (!app_add_name(&app_source, &app_source_tail, logical_name, NULL))
		goto cleanup;
	module->state = APP_MODULE_LOADED;
	ok = true;
cleanup:
	if (!ok)
		module->state = APP_MODULE_FAILED;
	free(init_name);
	free(package_init_name);
	free(logical_name);
	app_free_require_names(require_name, require_count);
	if (hir_ready) hir_cleanup();
	if (ast_ready) ast_cleanup();
	return ok;
}

NOCT_DLL bool
noct_bcback_app_add_source(const char *source_file_name,
			    const char *source_data)
{
	char *key;

	if (!app_active)
		return false;
	if (!noct_bcback_app_scan_source(source_file_name, source_data))
		return false;
	key = module_path_key(source_file_name);
	if (key == NULL)
		return false;
	return app_add_source_internal(source_file_name, source_data, key, true);
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
	if (!hir_optimize_func(h, bcback_optimize_level, bcback_simd_info, false) ||
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
	if (!app_package_cache &&
	    (app_main_count != 1 || app_main_params > 1)) {
		printf("Noct App requires exactly one public main() with zero or one parameter.\n");
		noct_bcback_app_abort(); return false;
	}
	if (!app_build_aggregate()) { noct_bcback_app_abort(); return false; }
	tmp = malloc(strlen(app_physical_output) + 48);
	if (tmp == NULL) { noct_bcback_app_abort(); return false; }
	sprintf(tmp, "%s.tmp.%ld", app_physical_output, (long)bcback_getpid());
	out = fopen(tmp, "wb");
	if (out == NULL) goto done;
	if ((!app_package_cache &&
	     fwrite(NOCT_APP_SHEBANG, 1, strlen(NOCT_APP_SHEBANG), out) !=
	     strlen(NOCT_APP_SHEBANG)) ||
	    !bcback_write_header(out, app_output, app_func_count)) goto close_out;
	for (f = app_func_head; f != NULL; f = f->next)
		if (!bcback_write_function(out, f->func)) goto close_out;
	if (fclose(out) != 0) { out = NULL; goto done; }
	out = NULL;
#if !defined(_WIN32)
	if (!app_package_cache)
	{
		struct stat st;
		if (stat(tmp, &st) != 0 || chmod(tmp, st.st_mode | S_IXUSR) != 0)
			goto done;
	}
#endif
	if (rename(tmp, app_physical_output) != 0) goto done;
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
