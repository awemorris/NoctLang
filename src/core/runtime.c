/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Language Runtime
 */

#include <noct/noct.h>
#include "ast.h"
#include "bytecode.h"
#include "bytecode_file.h"
#include "hir.h"
#include "hir_fast_checked.h"
#include "lir.h"
#include "runtime.h"
#include "intrinsics.h"
#include "interpreter.h"
#include "jit.h"
#include "gc.h"
#include "objectmodel.h"
#include "atomic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

/* False assertions. */
#define NOT_IMPLEMENTED		0
#define NEVER_COME_HERE		0
#define PINNED_VAR_NOT_FOUND	0
#define RT_MODULE_INITIAL	16

/* Required source state. */
enum rt_required_source_state {
	RT_REQUIRED_SOURCE_LOADING,
	RT_REQUIRED_SOURCE_LOADED,
	RT_REQUIRED_SOURCE_FAILED
};

/* Detached module state used while preparing one registration closure. */
enum rt_module_artifact_state {
	RT_MODULE_UNPREPARED,
	RT_MODULE_PREPARING,
	RT_MODULE_PREPARED,
	RT_MODULE_LOADING,
	RT_MODULE_LOADED,
	RT_MODULE_FAILED
};

/* Detached artifact kind. */
enum rt_module_artifact_kind {
	RT_MODULE_SOURCE,
	RT_MODULE_BYTECODE
};

/* Finalizer. */
struct rt_vm_finalizer {
	void (*finalizer)(void *userdata);
	void *userdata;
	struct rt_vm_finalizer *next;
};

/* A source returned by the host's require resolver. */
struct rt_required_source {
	char *module_name;
	char *path;
	char *error_file;
	char *error_message;
	int error_line;
	enum rt_required_source_state state;
	struct rt_required_source *next;
};

/* One source or bytecode artifact in a transient registration closure. */
struct rt_module_artifact {
	char *path;
	char *file_name;
	uint8_t *storage;
	const uint8_t *data;
	size_t data_size;
	char **require_name;
	uint32_t require_count;
	struct bytecode_file_module bytecode;
	struct lir_func **lir_function;
	uint32_t function_count;
	const char *initializer_name;
	enum rt_module_artifact_kind kind;
	enum rt_module_artifact_state state;
	bool owns_storage;
	bool is_required;
};

/* One module-name resolution retained for the complete closure lifetime. */
struct rt_module_binding {
	char *name;
	uint32_t artifact_index;
	struct rt_required_source *required_source;
};

/* Transient, side-effect-free module registration graph. */
struct rt_module_graph {
	struct rt_env *env;
	struct rt_module_artifact *artifact;
	uint32_t artifact_count;
	uint32_t artifact_capacity;
	struct rt_module_binding *binding;
	uint32_t binding_count;
	uint32_t binding_capacity;
	uint32_t *postorder;
	uint32_t postorder_count;
	uint32_t postorder_capacity;
};

/* Forward declarations. */
static void rt_free_func(struct rt_env *rt, struct rt_func *func);
static bool rt_register_source_graph(struct rt_env *env, const char *file_name, const char *source_text);
static bool rt_register_bytecode_graph(struct rt_env *env, const uint8_t *data, size_t size);
static bool rt_register_app(struct rt_env *env, const struct bytecode_file_app *app);
static void rt_module_graph_cleanup(struct rt_module_graph *graph);
static bool rt_module_graph_seed_prototypes(struct rt_module_graph *graph, const char *file_name);
static bool rt_module_graph_grow_artifacts(struct rt_module_graph *graph);
static bool rt_module_graph_grow_bindings(struct rt_module_graph *graph);
static bool rt_module_graph_grow_postorder(struct rt_module_graph *graph);
static int rt_module_graph_find_path(const struct rt_module_graph *graph, const char *path);
static int rt_module_graph_find_binding(const struct rt_module_graph *graph, const char *name);
static bool rt_module_graph_add_binding(struct rt_module_graph *graph, const char *name, uint32_t artifact_index, struct rt_required_source *required_source);
static bool rt_module_graph_add_source_root(struct rt_module_graph *graph, const char *file_name, const char *source_text, uint32_t *index);
static bool rt_module_graph_add_bytecode_root(struct rt_module_graph *graph, const uint8_t *data, size_t size, uint32_t *index);
static bool rt_module_graph_add_required(struct rt_module_graph *graph, const char *parent_file, const char *module_name, uint32_t *index);
static bool rt_module_graph_read_required(struct rt_module_graph *graph, const char *parent_file, const char *module_name, char *path, uint32_t *index);
static bool rt_module_graph_prepare(struct rt_module_graph *graph, uint32_t artifact_index);
static bool rt_module_graph_prepare_source(struct rt_module_graph *graph, struct rt_module_artifact *artifact);
static bool rt_module_graph_prepare_bytecode(struct rt_module_graph *graph, struct rt_module_artifact *artifact);
static bool rt_module_graph_add_bytecode_prototypes(struct rt_module_graph *graph, const struct bytecode_file_module *module, const char *file_name);
static bool rt_module_graph_compile_sources(struct rt_module_graph *graph);
static bool rt_module_graph_compile_source(struct rt_module_graph *graph, struct rt_module_artifact *artifact);
static bool rt_module_graph_validate_symbols(struct rt_module_graph *graph);
static bool rt_module_graph_publish_states(struct rt_module_graph *graph);
static void rt_module_graph_finish_states(struct rt_module_graph *graph, bool succeeded);
static const char *rt_module_artifact_function_name(const struct rt_module_artifact *artifact, uint32_t function_index);
static bool rt_module_graph_register(struct rt_module_graph *graph);
static bool rt_register_bytecode_descriptor(struct rt_env *env, const struct bytecode_file_function *function);
static bool rt_register_bytecode_modules(struct rt_env *env, uint32_t module_count, const struct bytecode_file_module module[], uint32_t order_count, const uint32_t order[]);
static bool rt_build_app_order(struct rt_env *env, const struct bytecode_file_app *app, uint32_t **order, uint32_t *order_count);
static bool rt_visit_app_module(struct rt_env *env, const struct bytecode_file_app *app, uint32_t module_index, unsigned char state[], uint32_t order[], uint32_t *order_count);
static int rt_find_app_binding(const struct bytecode_file_app *app, const char *module_name);
static struct rt_required_source *rt_find_required_module(struct rt_vm *vm, const char *module_name);
static struct rt_required_source *rt_find_required_source(struct rt_vm *vm, const char *path);
static struct rt_required_source *rt_add_required_module_state(struct rt_env *env, const char *module_name, const char *path, enum rt_required_source_state state);
static void rt_fail_required_module_state(struct rt_env *env, struct rt_required_source *required_source);
static struct rt_required_source *rt_add_required_alias(struct rt_env *env, const char *module_name, const struct rt_required_source *source);
static void rt_cleanup_required_sources(struct rt_vm *vm);
static char *rt_make_required_source_name(struct rt_env *env, const char *module_name, const char *path);
static void rt_set_error_file(struct rt_env *env, const char *file_name);
static bool rt_register_lir(struct rt_env *rt, struct lir_func *lir);
static bool rt_check_fast_call(struct rt_env *env, struct rt_func *func, uint32_t arg_count);
static bool rt_enter_frame(struct rt_env *env, struct rt_func *func);
static void rt_report_jit_result(struct rt_func *func, bool success, const char *reason);
static void rt_report_jit_lifecycle(const char *operation, bool success);
static void rt_invalidate_jit_entries(struct rt_vm *vm);
static bool rt_commit_jit(struct rt_env *env);
static void rt_leave_frame(struct rt_env *env);
static bool rt_init_global(struct rt_env *env);
static void rt_cleanup_global(struct rt_env *env);
static bool rt_expand_global(struct rt_env *env);

static void
rt_report_jit_result(
	struct rt_func *func,
	bool success,
	const char *reason)
{
	if (getenv("NOCT_JIT_DEBUG") != NULL) {
		fprintf(stderr,
			"noct-jit: %s: %s",
			func->name,
			success ? "compiled" : "fallback");
		if (!success && reason != NULL && reason[0] != '\0')
			fprintf(stderr, " reason=%s", reason);
		fputc('\n', stderr);
	}
}

static void
rt_report_jit_lifecycle(
	const char *operation,
	bool success)
{
	if (getenv("NOCT_JIT_DEBUG") != NULL) {
		fprintf(stderr,
			"noct-jit-lifecycle: %s status=%s\n",
			operation,
			success ? "ok" : "failed");
	}
}

static void
rt_invalidate_jit_entries(
	struct rt_vm *vm)
{
	struct rt_func *func;

	for (func = vm->func_list; func != NULL; func = func->next) {
		func->jit_code = NULL;
		func->call_count = -1;
	}
}

/*
 * Initialization
 */

/*
 * Create a virtual machine.
 */
bool
rt_create_vm(
	struct rt_vm **vm,
	struct rt_env **default_env,
	struct rt_config *config)
{
	*vm = NULL;
	*default_env = NULL;

	/* Allocate a struct rt_vm. */
	*vm = noct_malloc(sizeof(struct rt_vm));
	if (*vm == NULL) {
		*default_env = NULL;
		return false;
	}
	memset(*vm, 0, sizeof(struct rt_vm));

	/* Copy the config if specified. */
	if (config != NULL)
		memcpy(&(*vm)->config, config, sizeof(struct rt_config));
	else
		noct_set_default_config(&(*vm)->config);

	/* Allocate a struct rt_env. */
	*default_env = noct_malloc(sizeof(struct rt_env));
	if (*default_env == NULL) {
		noct_free(*vm);
		*vm = NULL;
		return false;
	}
	memset(*default_env, 0, sizeof(struct rt_env));
	(*default_env)->vm = *vm;
	(*vm)->env_list = *default_env;
	/* Enter the initial stack frame. */
	(*default_env)->cur_frame_index = 0;
	(*default_env)->frame = &(*default_env)->frame_alloc[0];
	(*default_env)->frame->tmpvar = &(*default_env)->frame->tmpvar_alloc[0];
	(*default_env)->frame->tmpvar_size = RT_TMPVAR_MAX;
	memset((*default_env)->frame->tmpvar, 0, sizeof(struct rt_value) * RT_TMPVAR_MAX);

	/* Initialize for GC. */
	om_init_env(*default_env);

	/* Initialize the global variables. */
	if (!rt_init_global(*default_env)) {
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}

	/* Initialize the garbage collector. */
	if (!rt_gc_init(*vm)) {
		rt_cleanup_global(*default_env);
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}

	/* Register the intrinsics. */
	if (!rt_register_intrinsics(*default_env)) {
		rt_cleanup_global(*default_env);
		rt_gc_cleanup(*vm);
		noct_free(*default_env);
		noct_free(*vm);
		return false;
	}
	return true;
}

/*
 * Destroy a virtual machine.
 */
bool
rt_destroy_vm(
	struct rt_vm *vm)
{
	struct rt_env *env, *next_env;
	struct rt_func *func, *next_func;
	struct rt_vm_finalizer *finalizer;
	struct rt_vm_finalizer *next_finalizer;
	bool jit_cleanup_succeeded = true;

	/* Free the JIT region. */
	if (vm->config.jit_enable && !jit_free(vm->env_list))
		jit_cleanup_succeeded = false;

	/* Run VM-owned native finalizers while the VM is still usable. */
	finalizer = vm->vm_finalizer_list;
	while (finalizer != NULL) {
		next_finalizer = finalizer->next;
		finalizer->finalizer(finalizer->userdata);
		noct_free(finalizer);
		finalizer = next_finalizer;
	}
	vm->vm_finalizer_list = NULL;

	/* Free global variables. */
	rt_cleanup_global(vm->env_list);

	/* Cleanup the garbage collector. */
	rt_gc_cleanup(vm);

	/* Free functions. */
	func = vm->func_list;
	while (func != NULL) {
		next_func = func->next;
		rt_free_func(vm->env_list, func);
		func = next_func;
	}

	/* Free required source load state. */
	rt_cleanup_required_sources(vm);

	/* Free thread environments. */
	env = vm->env_list;
	while (env != NULL) {
		next_env = env->next;
		noct_free(env);
		env = next_env;
	}

	if (vm->config.jit_enable)
		rt_report_jit_lifecycle("destroy", jit_cleanup_succeeded);

	noct_free(vm);

	return jit_cleanup_succeeded;
}

/* Free a function. */
static void
rt_free_func(
	struct rt_env *env,
	struct rt_func *func)
{
	int i;

	UNUSED_PARAMETER(env);

	noct_free(func->name);
	func->name = NULL;

	/* Release every possibly constructed parameter name. */
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		if (func->param_name[i] != NULL) {
			noct_free(func->param_name[i]);
			func->param_name[i] = NULL;
		}
	}
	noct_free(func->file_name);
	noct_free(func->bytecode);
	fast_signature_free(&func->fast_signature);

	if (func->jit_code != NULL)
		func->jit_code = NULL;

	noct_free(func);
}

/*
 * Create an environment for a secondary thread.
 */
#if defined(NOCT_USE_MULTITHREAD)
bool
rt_create_thread_env(
	struct rt_env *prev_env,
	struct rt_env **new_env)
{
	struct rt_vm *vm;
	struct rt_env *env;

	vm = prev_env->vm;

	/* Reuse a parked environment when possible. */
	atomic_spin_lock(&vm->env_free_lock);
	env = vm->env_free_list;
	if (env != NULL)
		vm->env_free_list = env->free_next;
	atomic_spin_unlock(&vm->env_free_lock);

	if (env == NULL) {
		env = noct_calloc(1, sizeof(struct rt_env));
		if (env == NULL) {
			rt_out_of_memory(prev_env);
			return false;
		}
		env->vm = vm;
		env->cur_frame_index = 0;
		env->frame = &env->frame_alloc[0];
		env->frame->tmpvar = &env->frame->tmpvar_alloc[0];
		env->frame->tmpvar_size = RT_TMPVAR_MAX;

		atomic_spin_lock(&vm->env_free_lock);
		env->next = vm->env_list;
		vm->env_list = env;
		atomic_spin_unlock(&vm->env_free_lock);
	} else {
		env->file_name[0] = '\0';
		env->error_message[0] = '\0';
		env->free_next = NULL;
	}

	/* Succeeded. The env is parked until rt_attach_thread_env(). */
	*new_env = env;

	return true;
}
#endif

/* Adopt an environment in the current thread. */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_attach_thread_env(
	struct rt_env *env)
{
	om_init_env(env);
}
#endif

/*
 * Release an environment.
 */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_release_thread_env(
	struct rt_env *env)
{
	struct rt_vm *vm;

	assert(env != NULL);

	vm = env->vm;
	atomic_spin_lock(&vm->env_free_lock);
	env->free_next = vm->env_free_list;
	vm->env_free_list = env;
	atomic_spin_unlock(&vm->env_free_lock);
}
#endif

/* Detach the current thread's environment for later reuse. */
#if defined(NOCT_USE_MULTITHREAD)
void
rt_detach_thread_env(
	struct rt_env *env)
{
	struct rt_vm *vm;

	assert(env != NULL);

	vm = env->vm;
	env->cur_frame_index = 0;
	env->frame = &env->frame_alloc[0];
	env->frame->tmpvar = &env->frame->tmpvar_alloc[0];
	env->frame->tmpvar_size = RT_TMPVAR_MAX;
	env->frame->pinned_count = 0;
	memset(env->frame->tmpvar_alloc, 0, sizeof(env->frame->tmpvar_alloc));

	om_enter_blocking(env);

	atomic_spin_lock(&vm->env_free_lock);
	env->free_next = vm->env_free_list;
	vm->env_free_list = env;
	atomic_spin_unlock(&vm->env_free_lock);
}
#endif

/*
 * Compilation
 */

/*
 * Register functions from a source text.
 */
bool
rt_register_source(
	struct rt_env *env,
	const char *file_name,
	const char *source_text)
{
	if (env == NULL || file_name == NULL || source_text == NULL)
		return false;

	return rt_register_source_graph(env, file_name, source_text);
}

/*
 * Registers functions from bytecode data.
 *
 * data must start from a raw module or application magic line.
 */
bool
rt_register_bytecode(
	struct rt_env *env,
	size_t size,
	uint8_t *data)
{
	struct bytecode_file_app app;
	struct bytecode_file_error error;
	enum bytecode_file_kind kind;
	bool succeeded;

	memset(&app, 0, sizeof(app));

	if (env == NULL || data == NULL || size == 0)
		return false;

	kind = bytecode_file_detect(data, size);
	if (kind == BYTECODE_FILE_MODULE_1_0 ||
	    kind == BYTECODE_FILE_MODULE_1_1) {
		return rt_register_bytecode_graph(env, data, size);
	}
	if (kind != BYTECODE_FILE_APP_1_0) {
		noct_error(env, N_TR("Failed to load bytecode data."));
		return false;
	}

	if (!bytecode_file_inspect_app(data, size, &app, &error)) {
		noct_error(env, N_TR("Failed to load application data."));
		return false;
	}

	succeeded = rt_register_app(env, &app);
	bytecode_file_cleanup_app(&app);

	return succeeded;
}

/* Prepare and load one source-rooted module closure. */
static bool
rt_register_source_graph(
	struct rt_env *env,
	const char *file_name,
	const char *source_text)
{
	struct rt_module_graph graph;
	uint32_t root_index;
	bool states_published;
	bool succeeded;

	memset(&graph, 0, sizeof(graph));
	graph.env = env;
	states_published = false;
	succeeded = false;
	hir_fast_checked_reset_prototypes();

	if (!rt_module_graph_seed_prototypes(&graph, file_name))
		goto cleanup;
	if (!rt_module_graph_add_source_root(
		&graph,
		file_name,
		source_text,
		&root_index)) {
		goto cleanup;
	}
	if (!rt_module_graph_prepare(&graph, root_index))
		goto cleanup;
	if (!rt_module_graph_compile_sources(&graph))
		goto cleanup;
	if (!rt_module_graph_validate_symbols(&graph))
		goto cleanup;
	if (!rt_module_graph_publish_states(&graph)) {
		states_published = true;
		goto cleanup;
	}
	states_published = true;
	if (!rt_module_graph_register(&graph))
		goto cleanup;

	succeeded = true;

cleanup:
	if (!states_published && graph.binding_count > 0) {
		(void)rt_module_graph_publish_states(&graph);
		states_published = true;
	}
	if (states_published)
		rt_module_graph_finish_states(&graph, succeeded);
	rt_module_graph_cleanup(&graph);
	hir_fast_checked_reset_prototypes();

	return succeeded;
}

/* Prepare and load one standalone bytecode-rooted module closure. */
static bool
rt_register_bytecode_graph(
	struct rt_env *env,
	const uint8_t *data,
	size_t size)
{
	struct rt_module_graph graph;
	uint32_t root_index;
	bool states_published;
	bool succeeded;

	memset(&graph, 0, sizeof(graph));
	graph.env = env;
	states_published = false;
	succeeded = false;
	hir_fast_checked_reset_prototypes();

	if (!rt_module_graph_seed_prototypes(&graph, "<bytecode>"))
		goto cleanup;
	if (!rt_module_graph_add_bytecode_root(
		&graph,
		data,
		size,
		&root_index)) {
		goto cleanup;
	}
	if (!rt_module_graph_prepare(&graph, root_index))
		goto cleanup;
	if (!rt_module_graph_compile_sources(&graph))
		goto cleanup;
	if (!rt_module_graph_validate_symbols(&graph))
		goto cleanup;
	if (!rt_module_graph_publish_states(&graph)) {
		states_published = true;
		goto cleanup;
	}
	states_published = true;
	if (!rt_module_graph_register(&graph))
		goto cleanup;

	succeeded = true;

cleanup:
	if (!states_published && graph.binding_count > 0) {
		(void)rt_module_graph_publish_states(&graph);
		states_published = true;
	}
	if (states_published)
		rt_module_graph_finish_states(&graph, succeeded);
	rt_module_graph_cleanup(&graph);
	hir_fast_checked_reset_prototypes();

	return succeeded;
}

/* Release every transient artifact, binding, and traversal index. */
static void
rt_module_graph_cleanup(
	struct rt_module_graph *graph)
{
	struct rt_module_artifact *artifact;
	uint32_t i;
	uint32_t j;

	/* Release every detached module and its compiled functions. */
	for (i = 0; i < graph->artifact_count; i++) {
		artifact = &graph->artifact[i];

		/* Release every LIR function built from a source artifact. */
		for (j = 0; j < artifact->function_count; j++) {
			if (artifact->lir_function != NULL &&
			    artifact->lir_function[j] != NULL) {
				lir_cleanup(artifact->lir_function[j]);
			}
		}
		noct_free(artifact->lir_function);

		/* Release source require names copied beyond the AST lifetime. */
		if (artifact->kind == RT_MODULE_SOURCE) {
			for (j = 0; j < artifact->require_count; j++) {
				if (artifact->require_name != NULL)
					noct_free(artifact->require_name[j]);
			}
			noct_free(artifact->require_name);
		}
		bytecode_file_cleanup_module(&artifact->bytecode);
		noct_free(artifact->file_name);
		if (artifact->owns_storage)
			noct_free(artifact->storage);
		free(artifact->path);
	}
	noct_free(graph->artifact);

	/* Release every resolver name retained by the graph. */
	for (i = 0; i < graph->binding_count; i++)
		noct_free(graph->binding[i].name);
	noct_free(graph->binding);
	noct_free(graph->postorder);
	memset(graph, 0, sizeof(*graph));
}

/* Seed closure prototype validation with functions already in the VM. */
static bool
rt_module_graph_seed_prototypes(
	struct rt_module_graph *graph,
	const char *file_name)
{
	struct rt_func *function;
	struct rt_value global;
	const struct fast_signature *signature;

	function = graph->env->vm->func_list;

	/* Retain every still-published function contract. */
	while (function != NULL) {
		if (!rt_check_global(graph->env, function->name)) {
			function = function->next;
			continue;
		}
		if (!rt_get_global(graph->env, function->name, &global))
			return false;
		if (global.type != NOCT_VALUE_FUNC ||
		    global.val.func != function) {
			function = function->next;
			continue;
		}

		signature = function->is_fast ?
			&function->fast_signature : NULL;
		if (!hir_fast_checked_add_prototype(
			function->name,
			function->is_fast,
			signature)) {
			rt_set_error_file(graph->env, file_name);
			graph->env->line = hir_get_error_line();
			rt_error(graph->env, "%s", hir_get_error_message());
			return false;
		}

		function = function->next;
	}

	return true;
}

/* Grow the transient artifact table for one new module. */
static bool
rt_module_graph_grow_artifacts(
	struct rt_module_graph *graph)
{
	struct rt_module_artifact *artifact;
	uint32_t capacity;

	if (graph->artifact_count < graph->artifact_capacity)
		return true;

	capacity = graph->artifact_capacity == 0 ?
		RT_MODULE_INITIAL : graph->artifact_capacity * 2;
	if (capacity < graph->artifact_capacity)
		goto oom;
	if (sizeof(*artifact) > SIZE_MAX / (size_t)capacity)
		goto oom;

	artifact = noct_realloc(
		graph->artifact,
		(size_t)capacity * sizeof(*artifact));
	if (artifact == NULL)
		goto oom;

	graph->artifact = artifact;
	graph->artifact_capacity = capacity;

	return true;

oom:
	rt_out_of_memory(graph->env);

	return false;
}

/* Grow the transient module-name binding table. */
static bool
rt_module_graph_grow_bindings(
	struct rt_module_graph *graph)
{
	struct rt_module_binding *binding;
	uint32_t capacity;

	if (graph->binding_count < graph->binding_capacity)
		return true;

	capacity = graph->binding_capacity == 0 ?
		RT_MODULE_INITIAL : graph->binding_capacity * 2;
	if (capacity < graph->binding_capacity)
		goto oom;
	if (sizeof(*binding) > SIZE_MAX / (size_t)capacity)
		goto oom;

	binding = noct_realloc(
		graph->binding,
		(size_t)capacity * sizeof(*binding));
	if (binding == NULL)
		goto oom;

	graph->binding = binding;
	graph->binding_capacity = capacity;

	return true;

oom:
	rt_out_of_memory(graph->env);

	return false;
}

/* Grow the dependency-first traversal table. */
static bool
rt_module_graph_grow_postorder(
	struct rt_module_graph *graph)
{
	uint32_t *postorder;
	uint32_t capacity;

	if (graph->postorder_count < graph->postorder_capacity)
		return true;

	capacity = graph->postorder_capacity == 0 ?
		RT_MODULE_INITIAL : graph->postorder_capacity * 2;
	if (capacity < graph->postorder_capacity)
		goto oom;
	if (sizeof(*postorder) > SIZE_MAX / (size_t)capacity)
		goto oom;

	postorder = noct_realloc(
		graph->postorder,
		(size_t)capacity * sizeof(*postorder));
	if (postorder == NULL)
		goto oom;

	graph->postorder = postorder;
	graph->postorder_capacity = capacity;

	return true;

oom:
	rt_out_of_memory(graph->env);

	return false;
}

/* Find one resolver artifact by its exact path byte string. */
static int
rt_module_graph_find_path(
	const struct rt_module_graph *graph,
	const char *path)
{
	uint32_t i;

	/* Compare only required artifacts that own resolver paths. */
	for (i = 0; i < graph->artifact_count; i++) {
		if (graph->artifact[i].path == NULL)
			continue;
		if (strcmp(graph->artifact[i].path, path) == 0)
			return (int)i;
	}

	return -1;
}

/* Find one module name already resolved in this registration closure. */
static int
rt_module_graph_find_binding(
	const struct rt_module_graph *graph,
	const char *name)
{
	uint32_t i;

	/* Compare each exact source-level module identifier. */
	for (i = 0; i < graph->binding_count; i++) {
		if (strcmp(graph->binding[i].name, name) == 0)
			return (int)i;
	}

	return -1;
}

/* Retain one exact module-name mapping in the transient graph. */
static bool
rt_module_graph_add_binding(
	struct rt_module_graph *graph,
	const char *name,
	uint32_t artifact_index,
	struct rt_required_source *required_source)
{
	struct rt_module_binding *binding;
	char *name_copy;

	if (!rt_module_graph_grow_bindings(graph))
		return false;
	name_copy = noct_strdup(name);
	if (name_copy == NULL) {
		rt_out_of_memory(graph->env);
		return false;
	}

	binding = &graph->binding[graph->binding_count];
	binding->name = name_copy;
	binding->artifact_index = artifact_index;
	binding->required_source = required_source;
	graph->binding_count++;

	return true;
}

/* Add the caller-owned source root to a transient graph. */
static bool
rt_module_graph_add_source_root(
	struct rt_module_graph *graph,
	const char *file_name,
	const char *source_text,
	uint32_t *index)
{
	struct rt_module_artifact *artifact;

	if (!rt_module_graph_grow_artifacts(graph))
		return false;

	artifact = &graph->artifact[graph->artifact_count];
	memset(artifact, 0, sizeof(*artifact));
	artifact->file_name = noct_strdup(file_name);
	if (artifact->file_name == NULL) {
		rt_out_of_memory(graph->env);
		return false;
	}
	artifact->data = (const uint8_t *)source_text;
	artifact->data_size = strlen(source_text);
	artifact->kind = RT_MODULE_SOURCE;
	*index = graph->artifact_count;
	graph->artifact_count++;

	return true;
}

/* Add the caller-owned bytecode root to a transient graph. */
static bool
rt_module_graph_add_bytecode_root(
	struct rt_module_graph *graph,
	const uint8_t *data,
	size_t size,
	uint32_t *index)
{
	struct rt_module_artifact *artifact;

	if (data == NULL || size == 0)
		return false;
	if (!rt_module_graph_grow_artifacts(graph))
		return false;

	artifact = &graph->artifact[graph->artifact_count];
	memset(artifact, 0, sizeof(*artifact));
	artifact->file_name = noct_strdup("<bytecode>");
	if (artifact->file_name == NULL) {
		rt_out_of_memory(graph->env);
		return false;
	}
	artifact->data = data;
	artifact->data_size = size;
	artifact->kind = RT_MODULE_BYTECODE;
	*index = graph->artifact_count;
	graph->artifact_count++;

	return true;
}

/* Resolve one unique module name and retain its exact artifact mapping. */
static bool
rt_module_graph_add_required(
	struct rt_module_graph *graph,
	const char *parent_file,
	const char *module_name,
	uint32_t *index)
{
	struct rt_required_source *path_source;
	struct rt_required_source *required_source;
	char *path;
	int artifact_index;
	int binding_index;

	binding_index = rt_module_graph_find_binding(graph, module_name);
	if (binding_index >= 0) {
		*index = graph->binding[(uint32_t)binding_index].artifact_index;
		return true;
	}

	required_source = rt_find_required_module(
		graph->env->vm,
		module_name);
	if (required_source != NULL) {
		if (required_source->state == RT_REQUIRED_SOURCE_LOADED) {
			*index = UINT32_MAX;
			return rt_module_graph_add_binding(
				graph,
				module_name,
				*index,
				required_source);
		}

		rt_set_error_file(graph->env, parent_file);
		graph->env->line = 0;
		if (required_source->state == RT_REQUIRED_SOURCE_LOADING) {
			rt_error(
				graph->env,
				N_TR("Circular require involving '%s'."),
				module_name);
		} else {
			if (required_source->error_file != NULL)
				rt_set_error_file(
					graph->env,
					required_source->error_file);
			graph->env->line = required_source->error_line;
			if (required_source->error_message != NULL) {
				rt_error(
					graph->env,
					"%s",
					required_source->error_message);
			} else {
				rt_error(
					graph->env,
					N_TR("Required module '%s' previously failed to load."),
					module_name);
			}
		}
		return false;
	}

	if (graph->env->vm->config.require_resolver == NULL) {
		rt_set_error_file(graph->env, parent_file);
		graph->env->line = 0;
		rt_error(
			graph->env,
			N_TR("require is not available in this environment."));
		return false;
	}

	path = graph->env->vm->config.require_resolver(module_name);
	if (path == NULL || path[0] == '\0') {
		free(path);
		rt_set_error_file(graph->env, parent_file);
		graph->env->line = 0;
		rt_error(
			graph->env,
			N_TR("Cannot resolve required module '%s'."),
			module_name);
		required_source = rt_add_required_module_state(
			graph->env,
			module_name,
			NULL,
			RT_REQUIRED_SOURCE_FAILED);
		if (required_source != NULL)
			rt_fail_required_module_state(graph->env, required_source);
		return false;
	}

	artifact_index = rt_module_graph_find_path(graph, path);
	if (artifact_index >= 0) {
		free(path);
		*index = (uint32_t)artifact_index;
		return rt_module_graph_add_binding(
			graph,
			module_name,
			*index,
			NULL);
	}

	path_source = rt_find_required_source(graph->env->vm, path);
	if (path_source != NULL) {
		required_source = rt_add_required_alias(
			graph->env,
			module_name,
			path_source);
		free(path);
		if (required_source == NULL)
			return false;
		if (required_source->state == RT_REQUIRED_SOURCE_LOADED) {
			*index = UINT32_MAX;
			return rt_module_graph_add_binding(
				graph,
				module_name,
				*index,
				required_source);
		}

		rt_set_error_file(graph->env, parent_file);
		graph->env->line = 0;
		if (required_source->state == RT_REQUIRED_SOURCE_LOADING) {
			rt_error(
				graph->env,
				N_TR("Circular require involving '%s'."),
				module_name);
			rt_fail_required_module_state(
				graph->env,
				required_source);
		} else {
			if (required_source->error_file != NULL) {
				rt_set_error_file(
					graph->env,
					required_source->error_file);
			}
			graph->env->line = required_source->error_line;
			if (required_source->error_message != NULL) {
				rt_error(
					graph->env,
					"%s",
					required_source->error_message);
			} else {
				rt_error(
					graph->env,
					N_TR("Required module '%s' previously failed to load."),
					module_name);
			}
		}

		return false;
	}

	required_source = rt_add_required_module_state(
		graph->env,
		module_name,
		path,
		RT_REQUIRED_SOURCE_LOADING);
	if (required_source == NULL) {
		free(path);
		return false;
	}

	if (!rt_module_graph_read_required(
		graph,
		parent_file,
		module_name,
		path,
		index)) {
		rt_fail_required_module_state(graph->env, required_source);
		return false;
	}

	if (!rt_module_graph_add_binding(
		graph,
		module_name,
		*index,
		required_source)) {
		rt_fail_required_module_state(graph->env, required_source);
		return false;
	}

	return true;
}

/* Read and classify one resolver-selected artifact exactly once. */
static bool
rt_module_graph_read_required(
	struct rt_module_graph *graph,
	const char *parent_file,
	const char *module_name,
	char *path,
	uint32_t *index)
{
	struct rt_module_artifact *artifact;
	const uint8_t *payload;
	uint8_t *storage;
	FILE *stream;
	char *file_name;
	long file_size;
	size_t read_size;
	size_t payload_size;
	size_t shebang_size;
	uint32_t registration_size;
	int existing_index;
	enum bytecode_file_kind kind;
	bool has_shebang;
	bool succeeded;

	storage = NULL;
	stream = NULL;
	file_name = NULL;
	succeeded = false;

	existing_index = rt_module_graph_find_path(graph, path);
	if (existing_index >= 0) {
		free(path);
		*index = (uint32_t)existing_index;
		return true;
	}

	stream = fopen(path, "rb");
	if (stream == NULL)
		goto read_error;
	if (fseek(stream, 0, SEEK_END) != 0)
		goto read_error;
	file_size = ftell(stream);
	if (file_size < 0)
		goto read_error;
	if (fseek(stream, 0, SEEK_SET) != 0)
		goto read_error;

	read_size = (size_t)file_size;
	if ((long)read_size != file_size || read_size == SIZE_MAX)
		goto read_error;
	storage = noct_malloc(read_size + 1);
	if (storage == NULL) {
		rt_out_of_memory(graph->env);
		goto cleanup;
	}
	if (fread(storage, 1, read_size, stream) != read_size)
		goto read_error;
	storage[read_size] = '\0';
	if (fclose(stream) != 0) {
		stream = NULL;
		goto read_error;
	}
	stream = NULL;

	payload = storage;
	payload_size = read_size;
	shebang_size = strlen(NOCT_APP_SHEBANG);
	has_shebang = false;
	if (payload_size >= shebang_size &&
	    memcmp(payload, NOCT_APP_SHEBANG, shebang_size) == 0) {
		payload += shebang_size;
		payload_size -= shebang_size;
		has_shebang = true;
	}

	kind = bytecode_file_detect(payload, payload_size);
	if (kind == BYTECODE_FILE_MODULE_UNKNOWN) {
		rt_error(
			graph->env,
			N_TR("Unsupported or malformed required bytecode '%s'."),
			path);
		goto cleanup;
	}
	if (kind == BYTECODE_FILE_APP_UNKNOWN ||
	    kind == BYTECODE_FILE_APP_1_0 ||
	    (has_shebang &&
	     (kind == BYTECODE_FILE_MODULE_1_0 ||
	      kind == BYTECODE_FILE_MODULE_1_1))) {
		rt_error(
			graph->env,
			N_TR("Application container cannot be required as a module."));
		goto cleanup;
	}

	file_name = rt_make_required_source_name(
		graph->env,
		module_name,
		path);
	if (file_name == NULL)
		goto cleanup;

	if (kind == BYTECODE_FILE_UNKNOWN) {
		if (memchr(payload, '\0', payload_size) != NULL) {
			rt_error(
				graph->env,
				N_TR("NUL in required source module '%s'."),
				path);
			goto cleanup;
		}
	} else if (!bytecode_file_check_registration_size(
		payload_size,
		&registration_size)) {
		rt_error(
			graph->env,
			N_TR("Required bytecode module is too large."));
		goto cleanup;
	}

	if (!rt_module_graph_grow_artifacts(graph))
		goto cleanup;
	artifact = &graph->artifact[graph->artifact_count];
	memset(artifact, 0, sizeof(*artifact));
	artifact->path = path;
	artifact->file_name = file_name;
	artifact->storage = storage;
	artifact->data = payload;
	artifact->data_size = payload_size;
	artifact->kind = kind == BYTECODE_FILE_UNKNOWN ?
		RT_MODULE_SOURCE : RT_MODULE_BYTECODE;
	artifact->owns_storage = true;
	artifact->is_required = true;
	*index = graph->artifact_count;
	graph->artifact_count++;
	succeeded = true;

cleanup:
	if (stream != NULL)
		fclose(stream);
	if (!succeeded) {
		noct_free(file_name);
		noct_free(storage);
		free(path);
	}

	return succeeded;

read_error:
	rt_set_error_file(graph->env, parent_file);
	graph->env->line = 0;
	rt_error(
		graph->env,
		N_TR("Cannot read required module '%s'."),
		path);
	goto cleanup;
}

/* Prepare one artifact and append it after all of its dependencies. */
static bool
rt_module_graph_prepare(
	struct rt_module_graph *graph,
	uint32_t artifact_index)
{
	struct rt_module_artifact *artifact;
	const char *module_name;
	uint32_t dependency_index;
	uint32_t require_count;
	uint32_t i;

	artifact = &graph->artifact[artifact_index];
	if (artifact->state == RT_MODULE_PREPARED)
		return true;
	if (artifact->state == RT_MODULE_PREPARING) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = 0;
		rt_error(
			graph->env,
			N_TR("Circular require involving '%s'."),
			artifact->file_name);
		return false;
	}

	artifact->state = RT_MODULE_PREPARING;
	if (artifact->kind == RT_MODULE_SOURCE) {
		if (!rt_module_graph_prepare_source(graph, artifact))
			return false;
	} else {
		if (!rt_module_graph_prepare_bytecode(graph, artifact))
			return false;
	}

	require_count = graph->artifact[artifact_index].require_count;

	/* Prepare every require edge in declaration order. */
	for (i = 0; i < require_count; i++) {
		module_name = graph->artifact[artifact_index].require_name[i];
		if (!rt_module_graph_add_required(
			graph,
			graph->artifact[artifact_index].file_name,
			module_name,
			&dependency_index)) {
			return false;
		}
		if (dependency_index == UINT32_MAX)
			continue;
		if (!rt_module_graph_prepare(graph, dependency_index))
			return false;
	}

	if (!rt_module_graph_grow_postorder(graph))
		return false;
	graph->postorder[graph->postorder_count] = artifact_index;
	graph->postorder_count++;
	graph->artifact[artifact_index].state = RT_MODULE_PREPARED;

	return true;
}

/* Collect source metadata without registering functions or initializers. */
static bool
rt_module_graph_prepare_source(
	struct rt_module_graph *graph,
	struct rt_module_artifact *artifact)
{
	uint32_t require_count;
	uint32_t i;
	bool ast_started;
	bool succeeded;

	require_count = 0;
	ast_started = false;
	succeeded = false;

	ast_started = true;
	if (!ast_build(
		artifact->file_name,
		(const char *)artifact->data)) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = ast_get_error_line();
		rt_error(graph->env, "%s", ast_get_error_message());
		goto cleanup;
	}

	if (!hir_collect_fast_prototypes()) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = hir_get_error_line();
		rt_error(graph->env, "%s", hir_get_error_message());
		goto cleanup;
	}

	require_count = ast_get_require_count();
	if (require_count > 0) {
		if (sizeof(*artifact->require_name) >
		    SIZE_MAX / (size_t)require_count) {
			rt_out_of_memory(graph->env);
			goto cleanup;
		}
		artifact->require_name = noct_calloc(
			(size_t)require_count,
			sizeof(*artifact->require_name));
		if (artifact->require_name == NULL) {
			rt_out_of_memory(graph->env);
			goto cleanup;
		}
	}
	artifact->require_count = require_count;

	/* Preserve require names before releasing the AST arena. */
	for (i = 0; i < require_count; i++) {
		artifact->require_name[i] = noct_strdup(ast_get_require_name(i));
		if (artifact->require_name[i] == NULL) {
			rt_out_of_memory(graph->env);
			goto cleanup;
		}
	}

	succeeded = true;

cleanup:
	if (ast_started)
		ast_cleanup();

	return succeeded;
}

/* Collect strict bytecode metadata and externally callable prototypes. */
static bool
rt_module_graph_prepare_bytecode(
	struct rt_module_graph *graph,
	struct rt_module_artifact *artifact)
{
	struct bytecode_file_error error;

	if (!bytecode_file_inspect_module(
		artifact->data,
		artifact->data_size,
		&artifact->bytecode,
		&error)) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = 0;
		rt_error(graph->env, N_TR("Failed to load bytecode data."));
		return false;
	}

	if (!rt_module_graph_add_bytecode_prototypes(
		graph,
		&artifact->bytecode,
		artifact->file_name)) {
		return false;
	}

	artifact->require_count = artifact->bytecode.require_count;
	artifact->require_name = artifact->bytecode.require_name;
	artifact->function_count = artifact->bytecode.function_count;

	return true;
}

/* Add every public bytecode function contract to the closure registry. */
static bool
rt_module_graph_add_bytecode_prototypes(
	struct rt_module_graph *graph,
	const struct bytecode_file_module *module,
	const char *file_name)
{
	const struct bytecode_file_function *function;
	const struct fast_signature *signature;
	uint32_t i;

	/* Add non-static, non-initializer functions in record order. */
	for (i = 0; i < module->function_count; i++) {
		function = &module->function[i];
		if (strncmp(function->name, "$static.", 8) == 0)
			continue;
		if (strncmp(function->name, "$init.", 6) == 0)
			continue;

		signature = function->is_fast ?
			&function->fast_signature : NULL;
		if (!hir_fast_checked_add_prototype(
			function->name,
			function->is_fast,
			signature)) {
			rt_set_error_file(graph->env, file_name);
			graph->env->line = hir_get_error_line();
			rt_error(graph->env, "%s", hir_get_error_message());
			return false;
		}
	}

	return true;
}

/* Compile every source artifact after the full prototype closure exists. */
static bool
rt_module_graph_compile_sources(
	struct rt_module_graph *graph)
{
	struct rt_module_artifact *artifact;
	uint32_t i;

	/* Compile in dependency-first order without publishing functions. */
	for (i = 0; i < graph->postorder_count; i++) {
		artifact = &graph->artifact[graph->postorder[i]];
		if (artifact->kind != RT_MODULE_SOURCE)
			continue;
		if (!rt_module_graph_compile_source(graph, artifact))
			return false;
	}

	return true;
}

/* Build detached LIR functions for one source artifact. */
static bool
rt_module_graph_compile_source(
	struct rt_module_graph *graph,
	struct rt_module_artifact *artifact)
{
	struct hir_block *hir_function;
	struct lir_func *lir_function;
	uint32_t function_count;
	uint32_t i;
	bool ast_started;
	bool hir_started;
	bool succeeded;

	function_count = 0;
	ast_started = false;
	hir_started = false;
	succeeded = false;

	ast_started = true;
	if (!ast_build(
		artifact->file_name,
		(const char *)artifact->data)) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = ast_get_error_line();
		rt_error(graph->env, "%s", ast_get_error_message());
		goto cleanup;
	}

	hir_started = true;
	if (!hir_build()) {
		rt_set_error_file(graph->env, artifact->file_name);
		graph->env->line = hir_get_error_line();
		rt_error(graph->env, "%s", hir_get_error_message());
		goto cleanup;
	}
	ast_cleanup();
	ast_started = false;

	function_count = hir_get_function_count();
	if (function_count > 0) {
		if (sizeof(*artifact->lir_function) >
		    SIZE_MAX / (size_t)function_count) {
			rt_out_of_memory(graph->env);
			goto cleanup;
		}
		artifact->lir_function = noct_calloc(
			(size_t)function_count,
			sizeof(*artifact->lir_function));
		if (artifact->lir_function == NULL) {
			rt_out_of_memory(graph->env);
			goto cleanup;
		}
	}
	artifact->function_count = function_count;

	lir_set_optimize_level(graph->env->vm->config.optimize_level);
	lir_set_lineinfo(graph->env->vm->config.line_info);

	/* Optimize and lower each source function without VM publication. */
	for (i = 0; i < function_count; i++) {
		hir_function = hir_get_function(i);
		if (!hir_optimize_func(
			hir_function,
			graph->env->vm->config.optimize_level,
			graph->env->vm->config.simd_info,
			graph->env->vm->accel_optimize_func,
			graph->env->vm->accel_optimize_userdata)) {
			rt_set_error_file(graph->env, hir_get_file_name());
			graph->env->line = hir_get_error_line();
			rt_error(graph->env, "%s", hir_get_error_message());
			goto cleanup;
		}

		lir_function = NULL;
		if (!lir_build(hir_function, &lir_function)) {
			rt_set_error_file(graph->env, lir_get_file_name());
			graph->env->line = lir_get_error_line();
			rt_error(graph->env, "%s", lir_get_error_message());
			goto cleanup;
		}
		artifact->lir_function[i] = lir_function;
	}

	succeeded = true;

cleanup:
	if (hir_started)
		hir_cleanup();
	if (ast_started)
		ast_cleanup();

	return succeeded;
}

/* Validate closure-wide link names and per-module initializer counts. */
static bool
rt_module_graph_validate_symbols(
	struct rt_module_graph *graph)
{
	struct rt_module_artifact *artifact;
	const char *name;
	const char *previous_name;
	uint32_t artifact_index;
	uint32_t previous_artifact;
	uint32_t function_index;
	uint32_t previous_function;
	uint32_t initializer_count;

	/* Validate each function against every function that precedes it. */
	for (artifact_index = 0;
	     artifact_index < graph->artifact_count;
	     artifact_index++) {
		artifact = &graph->artifact[artifact_index];
		initializer_count = 0;

		/* Validate names and locate the optional module initializer. */
		for (function_index = 0;
		     function_index < artifact->function_count;
		     function_index++) {
			name = rt_module_artifact_function_name(
				artifact,
				function_index);
			if (name == NULL || name[0] == '\0')
				goto malformed;
			if (strncmp(name, "$init.", 6) == 0) {
				initializer_count++;
				artifact->initializer_name = name;
			}

			/* Compare against every function in prior artifacts. */
			for (previous_artifact = 0;
			     previous_artifact <= artifact_index;
			     previous_artifact++) {
				uint32_t limit;

				limit = graph->artifact[
					previous_artifact].function_count;
				if (previous_artifact == artifact_index)
					limit = function_index;

				/* Reject every exact duplicate link name. */
				for (previous_function = 0;
				     previous_function < limit;
				     previous_function++) {
					previous_name = rt_module_artifact_function_name(
						&graph->artifact[previous_artifact],
						previous_function);
					if (strcmp(name, previous_name) == 0) {
						rt_error(
							graph->env,
							N_TR("Duplicate function '%s' in require closure."),
							name);
						return false;
					}
				}
			}
		}

		if (initializer_count > 1)
			goto malformed;
	}

	return true;

malformed:
	rt_error(graph->env, N_TR("Malformed module function directory."));

	return false;
}

/* Publish persistent module-name/path states before VM mutation. */
static bool
rt_module_graph_publish_states(
	struct rt_module_graph *graph)
{
	struct rt_module_binding *binding;
	struct rt_module_artifact *artifact;
	struct rt_required_source *required_source;
	uint32_t i;

	/* Allocate one persistent state for each newly resolved module name. */
	for (i = 0; i < graph->binding_count; i++) {
		binding = &graph->binding[i];
		if (binding->required_source != NULL)
			continue;
		if (binding->artifact_index >= graph->artifact_count)
			return false;
		artifact = &graph->artifact[binding->artifact_index];
		if (artifact->path == NULL)
			return false;

		required_source = noct_calloc(1, sizeof(*required_source));
		if (required_source == NULL) {
			rt_out_of_memory(graph->env);
			return false;
		}
		required_source->module_name = noct_strdup(binding->name);
		if (required_source->module_name == NULL) {
			noct_free(required_source);
			rt_out_of_memory(graph->env);
			return false;
		}
		required_source->path = noct_strdup(artifact->path);
		if (required_source->path == NULL) {
			noct_free(required_source->module_name);
			noct_free(required_source);
			rt_out_of_memory(graph->env);
			return false;
		}

		required_source->state = RT_REQUIRED_SOURCE_LOADING;
		required_source->next = graph->env->vm->required_source_list;
		graph->env->vm->required_source_list = required_source;
		binding->required_source = required_source;
	}

	return true;
}

/* Complete every newly published persistent module state. */
static void
rt_module_graph_finish_states(
	struct rt_module_graph *graph,
	bool succeeded)
{
	struct rt_required_source *required_source;
	uint32_t i;

	/* Mark only states created or reused by this graph. */
	for (i = 0; i < graph->binding_count; i++) {
		required_source = graph->binding[i].required_source;
		if (required_source == NULL)
			continue;
		if (required_source->state != RT_REQUIRED_SOURCE_LOADING)
			continue;

		required_source->state = succeeded ?
			RT_REQUIRED_SOURCE_LOADED : RT_REQUIRED_SOURCE_FAILED;
		if (!succeeded) {
			required_source->error_file = noct_strdup(
				graph->env->file_name);
			required_source->error_message = noct_strdup(
				graph->env->error_message);
			required_source->error_line = graph->env->line;
		}
	}
}

/* Return one borrowed link name from a detached source or bytecode module. */
static const char *
rt_module_artifact_function_name(
	const struct rt_module_artifact *artifact,
	uint32_t function_index)
{
	if (function_index >= artifact->function_count)
		return NULL;
	if (artifact->kind == RT_MODULE_SOURCE) {
		if (artifact->lir_function == NULL ||
		    artifact->lir_function[function_index] == NULL) {
			return NULL;
		}

		return artifact->lir_function[function_index]->func_name;
	}

	return artifact->bytecode.function[function_index].name;
}

/* Publish all closure functions, then execute initializers postorder. */
static bool
rt_module_graph_register(
	struct rt_module_graph *graph)
{
	struct rt_module_artifact *artifact;
	struct rt_value initializer_result;
	uint32_t i;
	uint32_t j;

	/* Register every function in dependency-first module order. */
	for (i = 0; i < graph->postorder_count; i++) {
		artifact = &graph->artifact[graph->postorder[i]];
		artifact->state = RT_MODULE_LOADING;

		/* Publish each function in its original declaration order. */
		for (j = 0; j < artifact->function_count; j++) {
			if (artifact->kind == RT_MODULE_SOURCE) {
				if (!rt_register_lir(
					graph->env,
					artifact->lir_function[j])) {
					artifact->state = RT_MODULE_FAILED;
					return false;
				}
			} else if (!rt_register_bytecode_descriptor(
				graph->env,
				&artifact->bytecode.function[j])) {
				artifact->state = RT_MODULE_FAILED;
				return false;
			}
		}
	}

	if (!rt_commit_jit(graph->env))
		return false;

	/* Execute each optional initializer after all dependency functions exist. */
	for (i = 0; i < graph->postorder_count; i++) {
		artifact = &graph->artifact[graph->postorder[i]];
		if (artifact->initializer_name != NULL) {
			memset(&initializer_result, 0, sizeof(initializer_result));
			if (!rt_call_with_name(
				graph->env,
				artifact->initializer_name,
				0,
				NULL,
				&initializer_result)) {
				artifact->state = RT_MODULE_FAILED;
				return false;
			}
		}
		artifact->state = RT_MODULE_LOADED;
	}

	return true;
}

/* Convert one inspected bytecode function into the runtime LIR view. */
static bool
rt_register_bytecode_descriptor(
	struct rt_env *env,
	const struct bytecode_file_function *function)
{
	struct lir_func lir_function;
	uint32_t i;

	if (function->param_count > LIR_PARAM_SIZE ||
	    function->param_count > NOCT_ARG_MAX) {
		return false;
	}

	memset(&lir_function, 0, sizeof(lir_function));
	lir_function.tmpvar_size = function->tmpvar_size;
	lir_function.bytecode_size = function->bytecode.size;
	lir_function.bytecode = (uint8_t *)function->bytecode.data;
	lir_function.file_name = function->source;
	lir_function.func_name = function->name;
	lir_function.param_count = function->param_count;

	/* Copy the inspected parameter view into the fixed LIR arrays. */
	for (i = 0; i < function->param_count; i++) {
		lir_function.param_name[i] = function->param_name[i];
		lir_function.param_type[i] = function->param_type[i];
		lir_function.param_packed_type[i] = function->param_packed_type[i];
		lir_function.param_restricted[i] = function->param_restricted[i];
	}
	lir_function.return_type = function->return_type;
	lir_function.return_packed_type = function->return_packed_type;
	lir_function.return_type_checked = function->return_type_checked;
	lir_function.has_vector_ops = function->has_vector_ops;
	lir_function.is_fast = function->is_fast;
	lir_function.fast_signature = function->fast_signature;
	lir_function.has_fma_ops = function->has_fma_ops;

	return rt_register_lir(env, &lir_function);
}

/* Register one fully inspected application without filesystem resolution. */
static bool
rt_register_app(
	struct rt_env *env,
	const struct bytecode_file_app *app)
{
	struct rt_module_graph prototype_graph;
	uint32_t *order;
	uint32_t order_count;
	uint32_t i;
	bool succeeded;

	memset(&prototype_graph, 0, sizeof(prototype_graph));
	prototype_graph.env = env;
	order = NULL;
	order_count = 0;
	succeeded = false;
	hir_fast_checked_reset_prototypes();

	if (!rt_module_graph_seed_prototypes(&prototype_graph, "<application>"))
		goto cleanup;

	/* Collect all public bytecode contracts before any VM mutation. */
	for (i = 0; i < app->module_count; i++) {
		if (!rt_module_graph_add_bytecode_prototypes(
			&prototype_graph,
			&app->module[i],
			app->module[i].source)) {
			goto cleanup;
		}
	}

	if (!rt_build_app_order(env, app, &order, &order_count))
		goto cleanup;
	if (!rt_register_bytecode_modules(
		env,
		app->module_count,
		app->module,
		order_count,
		order)) {
		goto cleanup;
	}

	succeeded = true;

cleanup:
	noct_free(order);
	hir_fast_checked_reset_prototypes();

	return succeeded;
}

/* Register and initialize inspected bytecode modules in dependency order. */
static bool
rt_register_bytecode_modules(
	struct rt_env *env,
	uint32_t module_count,
	const struct bytecode_file_module module[],
	uint32_t order_count,
	const uint32_t order[])
{
	const struct bytecode_file_module *current_module;
	const char **initializer_name;
	struct rt_value initializer_result;
	uint32_t module_index;
	uint32_t i;
	uint32_t j;
	bool succeeded;

	initializer_name = NULL;
	succeeded = false;
	if (module_count == 0 || order_count != module_count)
		return false;
	if (sizeof(*initializer_name) > SIZE_MAX / (size_t)module_count)
		return false;

	initializer_name = noct_calloc(
		(size_t)module_count,
		sizeof(*initializer_name));
	if (initializer_name == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Register every function from dependency to importer. */
	for (i = 0; i < order_count; i++) {
		module_index = order[i];
		if (module_index >= module_count)
			goto cleanup;
		current_module = &module[module_index];

		/* Publish each function and remember its optional initializer. */
		for (j = 0; j < current_module->function_count; j++) {
			if (strncmp(
				current_module->function[j].name,
				"$init.",
				6) == 0) {
				if (initializer_name[module_index] != NULL)
					goto cleanup;
				initializer_name[module_index] =
					current_module->function[j].name;
			}
			if (!rt_register_bytecode_descriptor(
				env,
				&current_module->function[j])) {
				goto cleanup;
			}
		}
	}

	if (!rt_commit_jit(env))
		goto cleanup;

	/* Execute each module initializer in the same dependency-first order. */
	for (i = 0; i < order_count; i++) {
		module_index = order[i];
		if (initializer_name[module_index] == NULL)
			continue;

		memset(&initializer_result, 0, sizeof(initializer_result));
		if (!rt_call_with_name(
			env,
			initializer_name[module_index],
			0,
			NULL,
			&initializer_result)) {
			goto cleanup;
		}
	}

	succeeded = true;

cleanup:
	noct_free(initializer_name);

	return succeeded;
}

/* Derive dependency-first application module order from its binding table. */
static bool
rt_build_app_order(
	struct rt_env *env,
	const struct bytecode_file_app *app,
	uint32_t **order,
	uint32_t *order_count)
{
	unsigned char *state;
	uint32_t i;
	bool succeeded;

	*order = NULL;
	*order_count = 0;
	state = NULL;
	succeeded = false;

	if (app->module_count == 0)
		return false;
	if (sizeof(**order) > SIZE_MAX / (size_t)app->module_count)
		return false;

	state = noct_calloc((size_t)app->module_count, sizeof(*state));
	if (state == NULL) {
		rt_out_of_memory(env);
		goto cleanup;
	}
	*order = noct_malloc((size_t)app->module_count * sizeof(**order));
	if (*order == NULL) {
		rt_out_of_memory(env);
		goto cleanup;
	}

	/* Traverse every explicit root in manifest order. */
	for (i = 0; i < app->root_count; i++) {
		if (!rt_visit_app_module(
			env,
			app,
			app->root_index[i],
			state,
			*order,
			order_count)) {
			goto cleanup;
		}
	}
	if (*order_count != app->module_count) {
		rt_error(env, N_TR("Application contains an unreachable module."));
		goto cleanup;
	}

	succeeded = true;

cleanup:
	noct_free(state);
	if (!succeeded) {
		noct_free(*order);
		*order = NULL;
		*order_count = 0;
	}

	return succeeded;
}

/* Visit one application module and all dependencies exactly once. */
static bool
rt_visit_app_module(
	struct rt_env *env,
	const struct bytecode_file_app *app,
	uint32_t module_index,
	unsigned char state[],
	uint32_t order[],
	uint32_t *order_count)
{
	const struct bytecode_file_module *module;
	uint32_t dependency_index;
	uint32_t i;
	int binding_index;

	if (module_index >= app->module_count)
		return false;
	if (state[module_index] == 2)
		return true;
	if (state[module_index] == 1) {
		rt_error(env, N_TR("Circular require in application container."));
		return false;
	}

	state[module_index] = 1;
	module = &app->module[module_index];

	/* Visit every manifest-bound require in declaration order. */
	for (i = 0; i < module->require_count; i++) {
		binding_index = rt_find_app_binding(app, module->require_name[i]);
		if (binding_index < 0) {
			rt_error(env, N_TR("Missing application module binding."));
			return false;
		}
		dependency_index = app->binding[
			(uint32_t)binding_index].module_index;
		if (!rt_visit_app_module(
			env,
			app,
			dependency_index,
			state,
			order,
			order_count)) {
			return false;
		}
	}

	state[module_index] = 2;
	order[*order_count] = module_index;
	(*order_count)++;

	return true;
}

/* Find one exact module-name binding inside an application manifest. */
static int
rt_find_app_binding(
	const struct bytecode_file_app *app,
	const char *module_name)
{
	uint32_t i;

	/* Search every already validated unique binding. */
	for (i = 0; i < app->binding_count; i++) {
		if (strcmp(app->binding[i].module_name, module_name) == 0)
			return (int)i;
	}

	return -1;
}

/* Find persistent required-module state by exact source-level name. */
static struct rt_required_source *
rt_find_required_module(
	struct rt_vm *vm,
	const char *module_name)
{
	struct rt_required_source *required_source;

	/* Search every module name already resolved by this VM. */
	for (required_source = vm->required_source_list;
	     required_source != NULL;
	     required_source = required_source->next) {
		if (required_source->module_name == NULL)
			continue;
		if (strcmp(required_source->module_name, module_name) == 0)
			return required_source;
	}

	return NULL;
}

/* Find required source state by resolved path. */
static struct rt_required_source *
rt_find_required_source(
	struct rt_vm *vm,
	const char *path)
{
	struct rt_required_source *required_source;

	/* Search every required source already seen by this VM. */
	for (required_source = vm->required_source_list;
	     required_source != NULL;
	     required_source = required_source->next) {
		if (required_source->path == NULL)
			continue;
		if (strcmp(required_source->path, path) == 0)
			return required_source;
	}

	return NULL;
}

/* Publish one persistent state for an exact module-name resolution. */
static struct rt_required_source *
rt_add_required_module_state(
	struct rt_env *env,
	const char *module_name,
	const char *path,
	enum rt_required_source_state state)
{
	struct rt_required_source *required_source;

	required_source = noct_calloc(1, sizeof(*required_source));
	if (required_source == NULL)
		goto out_of_memory;
	required_source->module_name = noct_strdup(module_name);
	if (required_source->module_name == NULL)
		goto out_of_memory;
	if (path != NULL) {
		required_source->path = noct_strdup(path);
		if (required_source->path == NULL)
			goto out_of_memory;
	}
	required_source->state = state;
	required_source->next = env->vm->required_source_list;
	env->vm->required_source_list = required_source;

	return required_source;

out_of_memory:
	if (required_source != NULL) {
		noct_free(required_source->path);
		noct_free(required_source->module_name);
		noct_free(required_source);
	}
	rt_out_of_memory(env);

	return NULL;
}

/* Save the current runtime diagnostic in one persistent failed state. */
static void
rt_fail_required_module_state(
	struct rt_env *env,
	struct rt_required_source *required_source)
{
	noct_free(required_source->error_file);
	noct_free(required_source->error_message);
	required_source->error_file = noct_strdup(env->file_name);
	required_source->error_message = noct_strdup(env->error_message);
	required_source->error_line = env->line;
	required_source->state = RT_REQUIRED_SOURCE_FAILED;
}

/* Retain another module name for an exact persistent artifact path. */
static struct rt_required_source *
rt_add_required_alias(
	struct rt_env *env,
	const char *module_name,
	const struct rt_required_source *source)
{
	struct rt_required_source *required_source;

	required_source = rt_add_required_module_state(
		env,
		module_name,
		source->path,
		source->state);
	if (required_source == NULL)
		return NULL;
	if (source->error_file != NULL) {
		required_source->error_file = noct_strdup(source->error_file);
		if (required_source->error_file == NULL)
			goto out_of_memory;
	}
	if (source->error_message != NULL) {
		required_source->error_message = noct_strdup(
			source->error_message);
		if (required_source->error_message == NULL)
			goto out_of_memory;
	}
	required_source->error_line = source->error_line;

	return required_source;

out_of_memory:
	noct_free(required_source->error_message);
	noct_free(required_source->error_file);
	required_source->error_message = NULL;
	required_source->error_file = NULL;
	required_source->state = RT_REQUIRED_SOURCE_FAILED;
	rt_out_of_memory(env);

	return NULL;
}

/* Free every required source state in a VM. */
static void
rt_cleanup_required_sources(
	struct rt_vm *vm)
{
	struct rt_required_source *required_source;
	struct rt_required_source *next;

	required_source = vm->required_source_list;

	/* Free every required source entry. */
	while (required_source != NULL) {
		next = required_source->next;
		noct_free(required_source->module_name);
		noct_free(required_source->error_file);
		noct_free(required_source->error_message);
		noct_free(required_source->path);
		noct_free(required_source);
		required_source = next;
	}

	vm->required_source_list = NULL;
}

/* Make a stable logical name for one required source. */
static char *
rt_make_required_source_name(
	struct rt_env *env,
	const char *module_name,
	const char *path)
{
	const char *suffix;
	char *file_name;
	size_t path_length;
	size_t prefix_length;
	size_t module_length;
	size_t suffix_length;
	size_t file_name_size;

	suffix = ".noct";
	path_length = strlen(path);
	if (path_length >= 4 && strcmp(path + path_length - 4, ".nct") == 0)
		suffix = ".nct";
	else if (path_length >= 4 &&
		 strcmp(path + path_length - 4, ".nbc") == 0) {
		suffix = ".nbc";
	}

	prefix_length = strlen("@require/");
	module_length = strlen(module_name);
	suffix_length = strlen(suffix);
	if (prefix_length > SIZE_MAX - module_length)
		goto oom;
	file_name_size = prefix_length + module_length;
	if (file_name_size > SIZE_MAX - suffix_length)
		goto oom;
	file_name_size += suffix_length;
	if (file_name_size == SIZE_MAX)
		goto oom;
	file_name_size++;
	file_name = noct_malloc(file_name_size);
	if (file_name == NULL)
		goto oom;

	snprintf(
		file_name,
		file_name_size,
		"@require/%s%s",
		module_name,
		suffix);

	return file_name;

oom:
	rt_out_of_memory(env);

	return NULL;
}

/* Set the current error file without truncation ambiguity. */
static void
rt_set_error_file(
	struct rt_env *env,
	const char *file_name)
{
	strncpy(env->file_name, file_name, sizeof(env->file_name) - 1);
	env->file_name[sizeof(env->file_name) - 1] = '\0';
}

/* Register a function from LIR. */
static bool
rt_register_lir(
	struct rt_env *env,
	struct lir_func *lir)
{
	struct rt_func *func;
	struct rt_value global;
	uint32_t i;

	func = noct_calloc(1, sizeof(struct rt_func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	fast_signature_init(&func->fast_signature);
	func->is_fast = lir->is_fast;
	if (!fast_signature_clone(
		&func->fast_signature,
		&lir->fast_signature)) {
		rt_out_of_memory(env);
		rt_free_func(env, func);
		return false;
	}

	func->name = noct_strdup(lir->func_name);
	if (func->name == NULL) {
		rt_out_of_memory(env);
		rt_free_func(env, func);
		return false;
	}

	func->param_count = lir->param_count;

	/* Initialize every parameter contract slot. */
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		func->param_type[i] = -1;
		func->param_packed_type[i] = -1;
		func->param_restricted[i] = false;
	}

	/* Copy the declared parameter contracts. */
	for (i = 0; i < lir->param_count; i++) {
		func->param_type[i] = lir->param_type[i];
		func->param_packed_type[i] = lir->param_packed_type[i];
		func->param_restricted[i] = lir->param_restricted[i];
	}

	func->return_type = lir->return_type;
	func->return_packed_type = lir->return_packed_type;
	func->return_type_checked = lir->return_type_checked;

	/* Copy every parameter name. */
	for (i = 0; i < lir->param_count; i++) {
		func->param_name[i] = noct_strdup(lir->param_name[i]);
		if (func->param_name[i] == NULL) {
			rt_out_of_memory(env);
			rt_free_func(env, func);
			return false;
		}
	}

	func->bytecode_size = lir->bytecode_size;
	if (func->bytecode_size != 0) {
		func->bytecode = noct_malloc((size_t)lir->bytecode_size);
		if (func->bytecode == NULL) {
			rt_out_of_memory(env);
			rt_free_func(env, func);
			return false;
		}
		memcpy(func->bytecode, lir->bytecode, (size_t)lir->bytecode_size);
	}

	func->tmpvar_size = lir->tmpvar_size;
	func->has_vector_ops = lir->has_vector_ops;
	func->has_fma_ops = lir->has_fma_ops;

	func->file_name = noct_strdup(lir->file_name);
	if (func->file_name == NULL) {
		rt_out_of_memory(env);
		rt_free_func(env, func);
		return false;
	}

	/* Insert a global variable. */
	global.type = NOCT_VALUE_FUNC;
	global.val.func = func;
	if (!rt_set_global(env, func->name, &global)) {
		rt_free_func(env, func);
		return false;
	}

	if (env->vm->config.jit_enable) {
		if (!jit_build(env, func)) {
			rt_report_jit_result(func, false, env->error_message);
			func->jit_code = NULL;
			func->call_count = -1;
			env->error_message[0] = '\0';
			env->line = 0;
		} else {
			rt_report_jit_result(func, true, NULL);
			env->vm->is_jit_dirty = true;
		}
	}

	/* Link. */
	func->next = env->vm->func_list;
	env->vm->func_list = func;

	return true;
}

static struct rt_func *
rt_create_cfunc(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	bool (*cfunc_with_data)(struct rt_env *env, void *userdata),
	void *userdata)
{
	struct rt_func *func;
	uint32_t i;

	if (name == NULL || name[0] == '\0' || param_count > NOCT_ARG_MAX ||
	    (param_count != 0 && param_name == NULL) ||
	    (cfunc == NULL) == (cfunc_with_data == NULL)) {
		rt_error(env, N_TR("Invalid native function registration."));
		return NULL;
	}

	func = noct_calloc(1, sizeof(*func));
	if (func == NULL) {
		rt_out_of_memory(env);
		return NULL;
	}

	fast_signature_init(&func->fast_signature);

	func->name = noct_strdup(name);
	if (func->name == NULL)
		goto oom;

	func->param_count = param_count;
	func->return_type = -1;
	func->return_packed_type = -1;
	for (i = 0; i < NOCT_ARG_MAX; i++) {
		func->param_type[i] = -1;
		func->param_packed_type[i] = -1;
	}
	for (i = 0; i < param_count; i++) {
		if (param_name[i] == NULL) {
			rt_error(env, N_TR("Invalid native function parameter name."));
			rt_free_func(env, func);
			return NULL;
		}
		func->param_name[i] = noct_strdup(param_name[i]);
		if (func->param_name[i] == NULL)
			goto oom;
	}

	func->cfunc = cfunc;
	func->cfunc_with_data = cfunc_with_data;
	func->cfunc_userdata = userdata;
	func->tmpvar_size = (uint32_t)param_count + 1;
	return func;

oom:
	rt_out_of_memory(env);
	rt_free_func(env, func);
	return NULL;
}

static bool
rt_publish_cfunc(
	struct rt_env *env,
	struct rt_func *func,
	struct rt_func **ret_func)
{
	struct rt_value global;

	global.type = NOCT_VALUE_FUNC;
	global.val.func = func;
	if (!rt_set_global(env, func->name, &global)) {
		rt_free_func(env, func);
		return false;
	}

	func->next = env->vm->func_list;
	env->vm->func_list = func;
	if (ret_func != NULL)
		*ret_func = func;
	return true;
}

bool
rt_register_cfunc(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env),
	struct rt_func **ret_func)
{
	struct rt_func *func;

	func = rt_create_cfunc(env, name, param_count, param_name,
			       cfunc, NULL, NULL);
	return func != NULL && rt_publish_cfunc(env, func, ret_func);
}

bool
rt_register_cfunc_with_data(
	struct rt_env *env,
	const char *name,
	size_t param_count,
	const char *param_name[],
	bool (*cfunc)(struct rt_env *env, void *userdata),
	void *userdata,
	struct rt_func **ret_func)
{
	struct rt_func *func;

	func = rt_create_cfunc(env, name, param_count, param_name,
			       NULL, cfunc, userdata);
	return func != NULL && rt_publish_cfunc(env, func, ret_func);
}

bool
rt_register_vm_finalizer(
	struct rt_env *env,
	void (*finalizer)(void *userdata),
	void *userdata)
{
	struct rt_vm_finalizer *entry;

	if (finalizer == NULL) {
		rt_error(env, N_TR("Invalid VM finalizer."));
		return false;
	}

	entry = noct_malloc(sizeof(*entry));
	if (entry == NULL) {
		rt_out_of_memory(env);
		return false;
	}
	entry->finalizer = finalizer;
	entry->userdata = userdata;
	entry->next = env->vm->vm_finalizer_list;
	env->vm->vm_finalizer_list = entry;
	return true;
}

static bool
rt_commit_jit(struct rt_env *env)
{
	if (env->vm->config.jit_enable && env->vm->is_jit_dirty) {
		if (!jit_commit(env)) {
			rt_invalidate_jit_entries(env->vm);
			(void)jit_free(env);
			env->vm->is_jit_dirty = false;
			rt_error(env, N_TR("JIT memory protection failed."));
			rt_report_jit_lifecycle("publish", false);
			return false;
		}
		env->vm->is_jit_dirty = false;
		rt_report_jit_lifecycle("publish", true);
	}
	return true;
}

/*
 * Call
 */

/*
 * Call a function with a name.
 */
bool
rt_call_with_name(
	struct rt_env *env,
	const char *func_name,
	uint32_t arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	struct rt_value global;
	struct rt_func *func;
	bool func_ok;

	/* Search a function. */
	func_ok = false;
	do {
		if (!rt_check_global(env, func_name))
			break;

		if (!rt_get_global(env, func_name, &global))
			break;

		if (global.type != NOCT_VALUE_FUNC)
			break;

		func_ok = true;
	} while (0);

	if (!func_ok) {
		noct_error(env, N_TR("Cannot find function %s."), func_name);
		return false;
	}

	func = global.val.func;

	/* Call. */
	if (!rt_call(env, func, arg_count, arg, ret))
		return false;

	return true;
}

/*
 * Call a function.
 */
bool
rt_call(
	struct rt_env *env,
	struct rt_func *func,
	uint32_t arg_count,
	struct rt_value *arg,
	struct rt_value *ret)
{
	char old_file_name[256];
	uint32_t i;

	if (arg_count != func->param_count) {
		noct_error(env, N_TR("%s(): Function arguments not match."), func->name);
		return false;
	}

	/* Allocate a frame for this call. */
	if (!rt_enter_frame(env, func))
		return false;

	env->frame->arg_count = arg_count;

	/*
	 * Every exit below must pop the frame. Leaving it behind would
	 * keep its slots alive as GC roots after the values they refer
	 * to are gone, and would leave the frame index out of step with
	 * the real call depth.
	 */

	/* Pass the args. */
	for (i = 0; i < arg_count; i++)
		env->frame->tmpvar[i] = arg[i];

#if defined(NOCT_USE_MULTITHREAD)
	/* Make a safepoint. */
	om_safepoint(env);
#endif

	/* Validate a fast entry only after its arguments are rooted. */
	if (func->is_fast) {
		if (!rt_check_fast_call(env, func, arg_count)) {
			rt_leave_frame(env);
			return false;
		}
	}

	/* Run. */
	if (func->cfunc != NULL) {
		/*
		 * Call an intrinsic or an FFI function implemented in C.
		 */
		if (!func->cfunc(env)) {
			rt_leave_frame(env);
			return false;
		}
	} else if (func->cfunc_with_data != NULL) {
		/* Call a native function with its host-owned opaque context. */
		if (!func->cfunc_with_data(env, func->cfunc_userdata)) {
			rt_leave_frame(env);
			return false;
		}
	} else {
		/*
		 * Call a Noct world function.
		 */

		/* Backup the old file name from the env. */
		strncpy(old_file_name, env->file_name, sizeof(old_file_name) - 1);

		/* Copy the new file name to the env. */
		strncpy(env->file_name, env->frame->func->file_name, sizeof(env->file_name) - 1);

		if (func->jit_code != NULL) {
			/*
			 * The function has a JIT-generated code. Call it.
			 */
			if (getenv("NOCT_JIT_DEBUG") != NULL)
				fprintf(stderr, "noct-jit: %s: native-entry\n",
					func->name);
			if (!func->jit_code(env)) {
				/*
				 * Native code returned false.
				 * Restore the old file name and exit with false.
				 */
				strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
				rt_leave_frame(env);
				return false;
			}
		} else {
			/*
			 * No JIT-generated code. Call the bytecode interpreter.
			 */
			if (!rt_visit_bytecode(env, func)) {
				/*
				 * Interpreter returned false.
				 * Restore the old file name and exit with false.
				 */
				strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
				rt_leave_frame(env);
				return false;
			}
		}

		/* Restore the old file name. */
		strncpy(env->file_name, old_file_name, sizeof(env->file_name) - 1);
	}

	/* Get a return value. */
	if (ret != NULL)
		*ret = env->frame->tmpvar[0];

	/* Succeeded. */
	rt_leave_frame(env);

	return true;
}

/* Enter a new calling frame. */
static bool
rt_enter_frame(
	struct rt_env *env,
	struct rt_func *func)
{
	struct rt_frame *frame;

	/*
	 * Check before incrementing so the frame index stays valid when
	 * the stack is full: the caller's error path still unwinds
	 * against its own (unchanged) frame.
	 */
	if (env->cur_frame_index + 1 >= RT_FRAME_MAX) {
		rt_error(env, N_TR("Stack overflow."));
		return false;
	}
	env->cur_frame_index++;

	frame = &env->frame_alloc[env->cur_frame_index];
	env->frame = frame;
	frame->func = func;
	frame->tmpvar = &frame->tmpvar_alloc[0];
	frame->tmpvar_size = func->tmpvar_size;
	frame->pinned_count = 0;

	/* We can't remove this due to GC. */
	memset(frame->tmpvar, 0, sizeof(struct rt_value) * (size_t)frame->tmpvar_size);

	return true;
}

/* Leave the current calling frame. */
static void
rt_leave_frame(
	struct rt_env *env)
{
	if (--env->cur_frame_index < 0) {
		rt_error(env, N_TR("Stack underflow."));
		abort();
	}

	env->frame = &env->frame_alloc[env->cur_frame_index];
}

/* Validate an exact fast entry contract against rooted arguments. */
static bool
rt_check_fast_call(
	struct rt_env *env,
	struct rt_func *func,
	uint32_t arg_count)
{
	const struct fast_signature *signature;
	const struct fast_param_contract *contract;
	const struct fast_extent *extent;
	struct rt_value *arguments;
	struct rt_value *argument;
	struct rt_value *extent_argument;
	struct rt_packed *packed;
	uint64_t extent_value;
	size_t element_count;
	uint32_t i;
	uint32_t axis;

	assert(env != NULL);
	assert(env->frame != NULL);
	assert(func != NULL);

	signature = &func->fast_signature;
	arguments = env->frame->tmpvar;
	if (!signature->valid ||
	    signature->version != NOCT_FAST_SIGNATURE_VERSION ||
	    signature->param_count != arg_count ||
	    (arg_count > 0 && signature->param == NULL)) {
		rt_error(
			env,
			N_TR("Invalid __fast function signature for '%s'."),
			func->name);
		return false;
	}

	/* Validate every exact value tag and Packed element kind first. */
	for (i = 0; i < arg_count; i++) {
		contract = &signature->param[i];
		argument = &arguments[i];

		if (argument->type != contract->value_type) {
			rt_error(
				env,
				N_TR("__fast call '%s': argument %u has the wrong primitive type."),
				func->name,
				(unsigned int)i + 1);
			return false;
		}

		if (contract->value_type != NOCT_VALUE_PACKED)
			continue;

		packed = argument->val.packed;
		if (packed == NULL || packed->type != contract->packed_type) {
			rt_error(
				env,
				N_TR("__fast call '%s': argument %u has the wrong packed element type."),
				func->name,
				(unsigned int)i + 1);
			return false;
		}
	}

	/* Validate every Packed shape after all scalar tags are known valid. */
	for (i = 0; i < arg_count; i++) {
		contract = &signature->param[i];
		if (contract->value_type != NOCT_VALUE_PACKED)
			continue;

		if (contract->rank == 0 ||
		    contract->rank > NOCT_FAST_RANK_MAX ||
		    contract->extent == NULL) {
			rt_error(
				env,
				N_TR("Invalid __fast function signature for '%s'."),
				func->name);
			return false;
		}

		element_count = 1;

		/* Multiply every positive extent into the exact element count. */
		for (axis = 0; axis < contract->rank; axis++) {
			extent = &contract->extent[axis];
			if (extent->kind == FAST_EXTENT_CONST) {
				if (extent->value.constant <= 0) {
					rt_error(
						env,
						N_TR("__fast call '%s': shape extents must be positive."),
						func->name);
					return false;
				}

				extent_value = (uint64_t)extent->value.constant;
			} else if (extent->kind == FAST_EXTENT_PARAM) {
				if (extent->value.param_index >= arg_count) {
					rt_error(
						env,
						N_TR("Invalid __fast function signature for '%s'."),
						func->name);
					return false;
				}

				extent_argument =
					&arguments[extent->value.param_index];
				if (extent_argument->type == NOCT_VALUE_INT) {
					if (extent_argument->val.i <= 0) {
						rt_error(
							env,
							N_TR("__fast call '%s': shape extents must be positive."),
							func->name);
						return false;
					}

					extent_value =
						(uint64_t)(uint32_t)
							extent_argument->val.i;
				} else if (extent_argument->type ==
					   NOCT_VALUE_LONG) {
					if (extent_argument->val.l <= 0) {
						rt_error(
							env,
							N_TR("__fast call '%s': shape extents must be positive."),
							func->name);
						return false;
					}

					extent_value =
						(uint64_t)extent_argument->val.l;
				} else {
					rt_error(
						env,
						N_TR("Invalid __fast function signature for '%s'."),
						func->name);
					return false;
				}
			} else {
				rt_error(
					env,
					N_TR("Invalid __fast function signature for '%s'."),
					func->name);
				return false;
			}

			if (extent_value > (uint64_t)SIZE_MAX ||
			    element_count >
				SIZE_MAX / (size_t)extent_value) {
				rt_error(
					env,
					N_TR("__fast call '%s': shape element count overflow."),
					func->name);
				return false;
			}

			element_count *= (size_t)extent_value;
		}

		packed = arguments[i].val.packed;
		if (packed->elem_size != element_count) {
			rt_error(
				env,
				N_TR("__fast call '%s': argument %u does not match the exact shape."),
				func->name,
				(unsigned int)i + 1);
			return false;
		}
	}

	return true;
}

/*
 * String
 */

/*
 * Make a string value.
 */
bool
rt_make_string(
	struct rt_env *env,
	struct rt_value *val,
	const char *data)
{
	size_t len;
	uint32_t hash;

	len = strlen(data) + 1; /* Including NUL. */
	hash = 0;
	if (!rt_make_string_with_hash(env, val, data, len, hash))
		return false;

	return true;
}

/*
 * Make a string value. (hash version)
 */
bool
rt_make_string_with_hash(
	struct rt_env *env,
	struct rt_value *val,
	const char *data,
	size_t len,		/* Including NUL */
	uint32_t hash)
{
	struct rt_string *rts;

	/* Allocate a string. */
	rts = rt_gc_alloc_string(env, data, len, hash);
	if (rts == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/*
	 * Here, this thread is "in-flight" and GC won't be executed
	 * in other threads.
	 */

	/* Setup a value. */
	val->type = NOCT_VALUE_STRING;
	val->val.str = rts;

	return true;
}

/*
 * Cache the hash of a string.
 */
void
rt_cache_string_hash(
	struct rt_string *rts)
{
	if (rts->hash == 0)
		rts->hash = noct_string_hash(rts->data);
}

/*
 * Get a string hash. (FNV-1a)
 */
uint32_t
rt_string_hash(
	const char *s)
{
	uint32_t hash = 2166136261u;
	while (*s) {
		hash ^= (uint8_t)*s++;
		hash *= 16777619u;
	}
	return hash;
}

/*
 * Get a string hash and a length. (FNV-1a)
 */
void
rt_string_hash_and_len(
	const char *s,
	uint32_t *hash,
	uint32_t *len)
{
	*len = 0;
	*hash = 2166136261u;
	while (*s) {
		*hash ^= (uint8_t)*s++;
		*hash *= 16777619u;
		*len = *len + 1;
	}
}

/*
 * Arrays and Dictionaries
 */

/*
 * Make an empty array.
 */
bool
rt_make_empty_array(
	struct rt_env *env,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_make_array(env, val))
		return false;

	return true;
}

/*
 * Get the size of an array.
 */
bool
rt_get_array_size(
	struct rt_env *env,
	struct rt_value *arr,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_array_size(env, arr, size))
		return false;

	return true;
}

/*
 * Retrieves an array element.
 */
bool
rt_get_array_elem(
	struct rt_env *env,
	struct rt_value *arr,
	size_t index,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_array(env, arr, index, val))
		return false;

	return true;
}

/*
 * Stores an value to an array.
 */
bool
rt_set_array_elem(
	struct rt_env *env,
	struct rt_value *arr,
	size_t index,
	NoctValue *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_array(env, arr, index, val))
		return false;

	return true;
}

/*
 * Resizes an array.
 */
bool
rt_resize_array(
	struct rt_env *env,
	struct rt_value *arr,
	size_t size)
{
	/* Delegate to the object model implementation. */
	if (!om_resize_array(env, arr, size))
		return false;

	return true;
}

/*
 * Make a shallow copy of an array.
 */
bool
rt_make_array_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	/* Delegate to the object model implementation. */
	if (!om_copy_array(env, dst, src))
		return false;

	return true;
}

/*
 * Make an empty dictionary.
 */
bool
rt_make_empty_dict(
	struct rt_env *env,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_make_dict(env, val))
		return false;

	return true;
}

/*
 * Get the size of a dictionary.
 */
bool
rt_get_dict_size(
	struct rt_env *env,
	struct rt_value *dict,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_dict_size(env, dict, size))
		return false;

	return true;
}

/*
 * Get the allocation size of a dictionary.
 */
bool
rt_get_dict_alloc_size(
	struct rt_env *env,
	struct rt_value *dict,
	size_t *size)
{
	/* Delegate to the object model implementation. */
	if (!om_get_dict_alloc_size(env, dict, size))
		return false;

	return true;
}

/*
 * Checks if a key exists in a dictionary.
 */
bool
rt_check_dict_key(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	bool *ret)
{
	/* Delegate to the object model implementation. */
	if (!om_check_dict_key(env, dict, key, ret))
		return false;

	return true;
}

/*
 * Checks if a key exists in a dictionary.
 */
bool
rt_check_dict_key_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	bool *ret)
{
	struct rt_value key_val;

	key_val.type = NOCT_VALUE_INT;
	key_val.val.i = 0;
	if (env->frame != NULL)
		rt_pin_local(env, &key_val);
	else
		rt_pin_global(env, &key_val);

	if (!rt_make_string(env, &key_val, key))
		return false;

	/* Delegate to the object model implementation. */
	if (!om_check_dict_key(env, dict, &key_val, ret))
		return false;
		
	if (env->frame != NULL)
		rt_unpin_local(env, &key_val);
	else
		rt_unpin_global(env, &key_val);
	
	return true;
}

/*
 * Get a dictionary key by index.
 */
bool
rt_get_dict_by_index(
	struct rt_env *env,
	struct rt_value *dict,
	size_t index,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict_index(env, dict, index, key, val))
		return false;

	return true;
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict(env, dict, key, val))
		return false;

	return true;	
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	size_t len;

	/* Including NUL. */
	len = strlen(key) + 1;

	/* Delegate to the object model implementation. */
	if (!om_read_dict_with_hash(env,
				    dict,
				    key,
				    len,
				    rt_string_hash(key),
				    val))
		return false;
		
	return true;
}

/*
 * Retrieves the value by a key in a dictionary.
 */
bool
rt_get_dict_elem_with_hash(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_read_dict_with_hash(env, dict, key, len, hash, val))
		return false;

	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_dict(env, dict, key, val))
		return false;
		
	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	struct rt_value *val)
{
	size_t len;

	/* Including NUL. */
	len = strlen(key) + 1;

	/* Delegate to the object model implementation. */
	if (!om_write_dict_with_hash(env,
				     dict,
				     key,
				     len,
				     rt_string_hash(key),
				     val))
		return false;
	
	return true;
}

/*
 * Stores a key-value-pair to a dictionary.
 */
bool
rt_set_dict_elem_with_hash(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	/* Delegate to the object model implementation. */
	if (!om_write_dict_with_hash(env,
				     dict,
				     key,
				     len,
				     hash,
				     val))
		return false;
	
	return true;
}

/*
 * Remove a dictionary key.
 */
bool
rt_remove_dict_elem(
	struct rt_env *env,
	struct rt_value *dict,
	struct rt_value *key)
{
	/* Delegate to the object model implementation. */
	if (!om_erase_dict_entry(env, dict, key))
		return false;

	return true;
}

/*
 * Remove a dictionary key. (hash version)
 */
bool
rt_remove_dict_elem_cstr(
	struct rt_env *env,
	struct rt_value *dict,
	const char *key)
{
	struct rt_value key_val;

	key_val.type = NOCT_VALUE_INT;
	key_val.val.i = 0;
	if (env->frame != NULL)
		rt_pin_local(env, &key_val);
	else
		rt_pin_global(env, &key_val);

	if (!rt_make_string(env, &key_val, key))
		return false;
	
	/* Delegate to the object model implementation. */
	if (!om_erase_dict_entry(env, dict, &key_val)) {
		rt_unpin_global(env, &key_val);
		return false;
	}
		
	if (env->frame != NULL)
		rt_unpin_local(env, &key_val);
	else
		rt_unpin_global(env, &key_val);
	
	return true;
}

/*
 * Make a shallow copy of a dictionary.
 */
bool
rt_make_dict_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	/* Delegate to the object model implementation. */
	if (!om_copy_dict(env, dst, src))
		return false;

	return true;
}

/*
 * Merges a dictionary.
 */
bool
rt_merge_dict(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src1,
	struct rt_value *src2)
{
	/* Delegate to the object model implementation. */
	if (!om_merge_dict(env, dst, src1, src2))
		return false;

	return true;
}

static struct rt_dict *
rt_get_latest_dict(
	struct rt_env *env,
	struct rt_value *dict)
{
#if defined(NOCT_USE_MULTITHREAD)
	struct rt_dict *real_dict;
	struct rt_dict *next;

	UNUSED_PARAMETER(env);

	real_dict = atomic_load_acquire_ptr((void **)&dict->val.dict);
	while ((next = atomic_load_acquire_ptr((void **)&real_dict->newer)) != NULL)
		real_dict = next;

	return real_dict;
#else
	struct rt_dict *real_dict;
	struct rt_dict *next;

	UNUSED_PARAMETER(env);

	real_dict = dict->val.dict;
	while ((next = real_dict->newer) != NULL)
		real_dict = next;

	return real_dict;
#endif
}

/*
 * Sets the native pointers to a dictionary.
 */
bool
rt_set_dict_native_pointer(
	struct rt_env *env,
	struct rt_value *dict,
	void *native_pointer,
	void (*native_finalizer)(void *native_pointer))
{
	struct rt_dict *real_dict;

	real_dict = rt_get_latest_dict(env, dict);

	real_dict->native_pointer = native_pointer;
	real_dict->native_finalizer = native_finalizer;

	return true;
}

/*
 * Gets the native pointer from a dictionary.
 */
bool
rt_get_dict_native_pointer(
	struct rt_env *env,
	struct rt_value *dict,
	void **native_pointer,
	void (**native_finalizer)(void *native_pointer))
{
	struct rt_dict *real_dict;

	real_dict = rt_get_latest_dict(env, dict);

	*native_pointer = real_dict->native_pointer;
	*native_finalizer = real_dict->native_finalizer;

	return true;
}

/*
 * Make a packed.
 */
bool
rt_make_packed(
	struct rt_env *env,
	struct rt_value *val,
	int type,
	size_t size,
	size_t elem_size,
	void *preallocated,
	void *native_pointer,
	void (*native_finalizer)(void *native_pointer))
{
	struct rt_packed *packed;

	assert(env != NULL);
	assert(val != NULL);
	assert(size > 0);
	assert(elem_size > 0);
	assert((native_pointer == NULL) == (native_finalizer == NULL));
	assert(preallocated != NULL || native_pointer == NULL);

	/* Allocate an array. */
	packed = rt_gc_alloc_packed(env,
				    type,
				    size,
				    elem_size,
				    preallocated,
				    native_pointer,
				    native_finalizer);
	if (packed == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/*
	 * Here, this thread is "in-flight" and GC won't be executed
	 * in other threads.
	 */

	/* Setup a value. */
	val->type = NOCT_VALUE_PACKED;
	val->val.packed = packed;

	return true;
}

bool
rt_get_packed_native_pointer(
	struct rt_env *env,
	struct rt_value *packed,
	void **native_pointer,
	void (**native_finalizer)(void *native_pointer))
{
	UNUSED_PARAMETER(env);

	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(native_pointer != NULL);
	assert(native_finalizer != NULL);

	*native_pointer = packed->val.packed->native_pointer;
	*native_finalizer = packed->val.packed->native_finalizer;
	return true;
}

bool
rt_finalize_packed(
	struct rt_env *env,
	struct rt_value *packed)
{
	struct rt_packed *p;
	void *native_pointer;
	void (*native_finalizer)(void *native_pointer);

	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);

	p = packed->val.packed;
	assert(p != NULL);
	if (p->native_finalizer == NULL)
		return true;

	native_pointer = p->native_pointer;
	native_finalizer = p->native_finalizer;

	p->native_pointer = NULL;
	p->native_finalizer = NULL;
	p->packed_buffer = NULL;
	p->elem_size = 0;

	native_finalizer(native_pointer);

	return true;
}

/*
 * Get the element type of a packed.
 */
bool
rt_get_packed_type(
	struct rt_env *env,
	struct rt_value *packed,
	int *type)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(type != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	/* Get the type. */
	*type = packed->val.packed->type;

	return true;
}

/*
 * Get the element count of a packed.
 */
bool
rt_get_packed_size(
	struct rt_env *env,
	struct rt_value *packed,
	size_t *size)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(size != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	/* Get the type. */
	*size = packed->val.packed->elem_size;

	return true;
}

/*
 * Retrieves an int8 packed element.
 */
bool
rt_get_packed_elem(
	struct rt_env *env,
	struct rt_value *packed,
	size_t index,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);
	assert(val != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	if (index >= packed->val.packed->elem_size) {
		rt_error(env, N_TR("Packed index %ld is out-of-range."), index);
		return false;
	}

	switch (packed->val.packed->type) {
	case NOCT_PACKED_INT8:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int8_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT8:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((uint8_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT16:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int16_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT16:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((uint16_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT32:
		val->type = NOCT_VALUE_INT;
		val->val.i = *((int32_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT32:
		val->type = NOCT_VALUE_INT;
		val->val.i = (int32_t)*((uint32_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_INT64:
		val->type = NOCT_VALUE_LONG;
		val->val.l = (int64_t)*((int64_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_UINT64:
		val->type = NOCT_VALUE_LONG;
		val->val.l = (int64_t)*((uint64_t *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_FLOAT32:
		val->type = NOCT_VALUE_FLOAT;
		val->val.f = *((float *)(packed->val.packed->packed_buffer) + index);
		break;
	case NOCT_PACKED_FLOAT64:
		val->type = NOCT_VALUE_DOUBLE;
		val->val.lf = *((double *)(packed->val.packed->packed_buffer) + index);
		break;
	}

	return true;
}

/*
 * Stores an value to a packed.
 */
bool
rt_set_packed_elem(
	struct rt_env *env,
	struct rt_value *packed,
	size_t index,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(packed != NULL);
	assert(packed->type == NOCT_VALUE_PACKED);
	assert(packed->val.packed != NULL);

	assert(val != NULL);
	if (packed->val.packed->packed_buffer == NULL) {
		rt_error(env, N_TR("Packed is unmapped."));
		return false;
	}

	if (index >= packed->val.packed->elem_size) {
		rt_error(env, N_TR("Packed index %ld is out-of-range."), index);
		return false;
	}

	switch (packed->val.packed->type) {
	case NOCT_PACKED_INT8:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int8_t *)packed->val.packed->packed_buffer + index) = (int8_t)(uint8_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT8:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint8_t *)packed->val.packed->packed_buffer + index) = (uint8_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT16:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int16_t *)packed->val.packed->packed_buffer + index) = (int16_t)(uint16_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT16:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint16_t *)packed->val.packed->packed_buffer + index) = (uint16_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)(int)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int32_t *)packed->val.packed->packed_buffer + index) = (int32_t)(uint32_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)(int)val->val.f;
 			break;
		case NOCT_VALUE_DOUBLE:
			*((uint32_t *)packed->val.packed->packed_buffer + index) = (uint32_t)(int)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_INT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)(uint64_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)(uint64_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((int64_t *)packed->val.packed->packed_buffer + index) = (int64_t)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_UINT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(uint32_t)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(int64_t)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((uint64_t *)packed->val.packed->packed_buffer + index) = (uint64_t)(int64_t)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_FLOAT32:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((float *)packed->val.packed->packed_buffer + index) = (float)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	case NOCT_PACKED_FLOAT64:
		switch (val->type) {
		case NOCT_VALUE_INT:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.i;
			break;
		case NOCT_VALUE_LONG:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.l;
			break;
		case NOCT_VALUE_FLOAT:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.f;
			break;
		case NOCT_VALUE_DOUBLE:
			*((double *)packed->val.packed->packed_buffer + index) = (double)val->val.lf;
			break;
		default:
			rt_error(env, N_TR("Value is not a number."));
			return false;
		}
		break;
	default:
		assert(0);
		break;
	}

	return true;
}

/*
 * Make a copy of a packed.
 */
bool
rt_make_packed_copy(
	struct rt_env *env,
	struct rt_value *dst,
	struct rt_value *src)
{
	struct rt_packed *dst_packed;
	size_t size;

	assert(env != NULL);
	assert(dst != NULL);
	assert(dst->type == NOCT_VALUE_PACKED);
	assert(dst->val.packed != NULL);
	assert(dst->val.packed->packed_buffer != NULL);
	assert(src->type == NOCT_VALUE_PACKED);
	assert(src->val.packed != NULL);
	assert(src->val.packed->packed_buffer != NULL);

	/* Determine the byte size. */
	switch (src->val.packed->type) {
	case NOCT_PACKED_INT8:
	case NOCT_PACKED_UINT8:
		size = src->val.packed->elem_size;
		break;
	case NOCT_PACKED_INT16:
	case NOCT_PACKED_UINT16:
		size = src->val.packed->elem_size * 2;
		break;
	case NOCT_PACKED_INT32:
	case NOCT_PACKED_UINT32:
	case NOCT_PACKED_FLOAT32:
		size = src->val.packed->elem_size * 4;
		break;
	default:
		size = src->val.packed->elem_size * 8;
		break;
	}

	/* Allocate an array. */
	dst_packed = rt_gc_alloc_packed(env,
					 src->val.packed->type,
					 size,
					 src->val.packed->elem_size,
					 NULL,
					 NULL,
					 NULL);
	if (dst_packed == NULL)
		return false;

	/*
	 * In this section, it is guaranteed that GC is not executed
	 * in other threads because this thread is "in-flight" and
	 * a GC execution waits for all threads become not in-flight.
	 */

	memcpy(dst_packed->packed_buffer, src->val.packed->packed_buffer, size);

	dst->type = NOCT_VALUE_PACKED;
	dst->val.packed = dst_packed;

	return true;
}

/*
 * Global Variable
 */

#if !defined(NOCT_USE_MULTITHREAD)

#define ACQUIRE_GLOBAL()
#define RELEASE_GLOBAL()

#else

#define ACQUIRE_GLOBAL()								\
	do {										\
		while (1) {							\
			int old = atomic_fetch_add_acquire_int(			\
				&env->vm->global_var_counter, 1);			\
			if (old == 0)						\
				break;							\
			atomic_fetch_sub_release_int(				\
				&env->vm->global_var_counter, 1);			\
		}									\
	} while (0)

#define RELEASE_GLOBAL()								\
	do {										\
		atomic_fetch_sub_release_int(&env->vm->global_var_counter, 1);	\
	} while (0)

#endif

/* Initialize the global variables. */
static bool
rt_init_global(
	struct rt_env *env)
{
	const uint32_t START_SIZE = 2;

	assert(env->vm->global == NULL);

	/* Allocate the table. */
	env->vm->global = noct_calloc(sizeof(struct rt_bindglobal) * START_SIZE, 1);
	if (env->vm->global == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	env->vm->global_alloc_size = START_SIZE;
	env->vm->global_size = 0;

	return true;
}

/* Cleanup the global variables. */
static void
rt_cleanup_global(
	struct rt_env *env)
{
	uint32_t i;

	assert(env->vm->global != NULL);

	for (i = 0; i < env->vm->global_alloc_size; i++) {
		if (env->vm->global[i].name != NULL) {
			noct_free(env->vm->global[i].name);
			env->vm->global[i].name = NULL;
		}
	}
	noct_free(env->vm->global);
	env->vm->global = NULL;
}

/*
 * Check if a global variable exists.
 */
bool
rt_check_global(
	struct rt_env *env,
	const char *name)
{
	uint32_t index, i, len, hash;

	ACQUIRE_GLOBAL();

	rt_string_hash_and_len(name, &hash, &len);
	len++;	/* Including NUL. */

	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		if (env->vm->global[i].is_removed)
			continue;
		if (env->vm->global[i].name == NULL) {
			/* Not found. */
			RELEASE_GLOBAL();
			return false;
		}
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) != 0)
			continue;

		/* Found. */
		RELEASE_GLOBAL();
		return true;
	}

	/* Not found. */
	RELEASE_GLOBAL();
	return false;
}

/*
 * Get a global variable.
 */
bool
rt_get_global(
	struct rt_env *env,
	const char *name,
	struct rt_value *val)
{
	size_t len;
	uint32_t hash;

	len = strlen(name) + 1; /* Including NUL. */
	hash = rt_string_hash(name);

	if (!rt_get_global_with_hash(env, name, len, hash, val))
		return false;

	return true;
}

/*
 * Get a global variable. (hash version)
 */
bool
rt_get_global_with_hash(
	struct rt_env *env,
	const char *name,
	size_t len,
	uint32_t hash,
	struct rt_value *val)
{
	uint32_t index, i;

	ACQUIRE_GLOBAL();

	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		if (env->vm->global[i].is_removed)
			continue;
		if (env->vm->global[i].name == NULL)
			break;
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) != 0)
			continue;

		/* Found. */
		*val = env->vm->global[i].val;
		RELEASE_GLOBAL();
		return true;
	}

	/* Not found. */
	RELEASE_GLOBAL();
	rt_error(env, N_TR("Symbol \"%s\" not found."), name);
	return false;
}

/*
 * Set a global variable.
 */
bool
rt_set_global(
	struct rt_env *env,
	const char *name,
	struct rt_value *val)
{
	size_t len;
	uint32_t hash;

	len = strlen(name) + 1;	/* Including NUL. */
	hash = rt_string_hash(name);
	if (!rt_set_global_with_hash(env, name, len, hash, val))
		return false;

	return true;
}

/* Mark an already-registered global binding immutable. */
bool
rt_mark_global_const(
	struct rt_env *env,
	const char *name)
{
	uint32_t i;

	ACQUIRE_GLOBAL();
	for (i = 0; i < env->vm->global_alloc_size; i++) {
		if (env->vm->global[i].name == NULL ||
		    env->vm->global[i].is_removed)
			continue;
		if (strcmp(env->vm->global[i].name, name) == 0) {
			env->vm->global[i].is_const = true;
			RELEASE_GLOBAL();
			return true;
		}
	}
	RELEASE_GLOBAL();
	rt_error(env, N_TR("Symbol \"%s\" not found."), name);
	return false;
}

/*
 * Set a global variable.
 */
bool
rt_set_global_with_hash(
	struct rt_env *env,
	const char *name,
	size_t len,		/* Including NUL. */
	uint32_t hash,
	struct rt_value *val)
{
	uint32_t index, i;

	ACQUIRE_GLOBAL();

	/* Reisze if 75% is used. */
	if (env->vm->global_size >= env->vm->global_alloc_size / 4 * 3) {
		if (!rt_expand_global(env)) {
			RELEASE_GLOBAL();
			return false;
		}
	}

	/* Search a place to insert or overwrite. */
	index = hash & ((uint32_t)env->vm->global_alloc_size - 1) ;
	for (i = index;
	     i != ((index - 1 + env->vm->global_alloc_size) & (env->vm->global_alloc_size - 1));
	     i = (i + 1) & (env->vm->global_alloc_size - 1)) {
		/* If found an empty entry. */
		if (env->vm->global[i].is_removed ||
		    env->vm->global[i].name == NULL) {
			/* Insert a new entry. */
			env->vm->global[i].name = noct_strdup(name);
			if (env->vm->global[i].name == NULL) {
				RELEASE_GLOBAL();
				rt_out_of_memory(env);
				return false;
			}
			env->vm->global[i].name_len = (uint32_t)len;
			env->vm->global[i].name_hash = hash;
			env->vm->global[i].val = *val;
			env->vm->global_size++;
			RELEASE_GLOBAL();
			return true;
		}

		/* If found an existing entry. */
		if (env->vm->global[i].name_len != len)
			continue;
		if (env->vm->global[i].name_hash != hash)
			continue;
		if (strcmp(env->vm->global[i].name, name) == 0) {
			/* Reject assignment to a constant (let) binding. */
			if (env->vm->global[i].is_const) {
				RELEASE_GLOBAL();
				rt_error(env, N_TR("Cannot assign to constant \"%s\"."), name);
				return false;
			}
			/* Overwrite the existing entry value. */
			env->vm->global[i].val = *val;
			RELEASE_GLOBAL();
			return true;
		}
	}

	/* No empty entry. */
	assert(NEVER_COME_HERE);
	RELEASE_GLOBAL();
	return false;
}

/* Expand the global variable table. */
static bool
rt_expand_global(
	struct rt_env *env)
{
	struct rt_bindglobal *old_tbl,*new_tbl;
	uint32_t old_size, new_size, i, j, index;

	/* Allocate the new table. */
	old_size = env->vm->global_alloc_size;
	old_tbl = env->vm->global;
	new_size = old_size * 2;
	new_tbl = noct_calloc(sizeof(struct rt_bindglobal) * new_size, 1);
	if (new_tbl == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	/* Rehash (copy). */
	for (i = 0; i < old_size; i++) {
		if (old_tbl[i].name == NULL || old_tbl[i].is_removed)
			continue;
		index = rt_string_hash(old_tbl[i].name) & (new_size - 1) ;
		for (j = index;
		     j != ((index - 1 + new_size) & (new_size - 1));
		     j = (j + 1) & (new_size - 1)) {
			if (new_tbl[j].name == NULL) {
				new_tbl[j].name = old_tbl[i].name;
				new_tbl[j].name_len = old_tbl[i].name_len;
				new_tbl[j].name_hash = old_tbl[i].name_hash;
				new_tbl[j].val = old_tbl[i].val;
				new_tbl[j].is_const = old_tbl[i].is_const;
				break;
			}
		}
	}

	noct_free(old_tbl);
	env->vm->global = new_tbl;
	env->vm->global_alloc_size = new_size;

	return true;
}

/*
 * __fast func
 */

/*
 * Restores a generated __fast function's caller-side contract.
 */
bool
rt_mark_fast_func(
	struct rt_func *func,
	uint32_t tmpvar_size,
	int return_type,
	uint32_t param_count,
	const int *value_type,
	const int *packed_type,
	const int *restricted,
	const uint32_t *rank,
	const int *extent_kind,
	const int64_t *extent_value)
{
	struct fast_signature candidate;
	struct fast_param_contract *contract;
	struct fast_extent *extent;
	uint32_t extent_count;
	uint32_t param_index;
	uint32_t i;
	uint32_t axis;

	if (func == NULL)
		return false;
	if (param_count != func->param_count || param_count > NOCT_ARG_MAX)
		return false;
	if (tmpvar_size < param_count + 1 || tmpvar_size > RT_TMPVAR_MAX)
		return false;
	if (param_count > 0 &&
	    (value_type == NULL ||
	     packed_type == NULL ||
	     restricted == NULL ||
	     rank == NULL)) {
		return false;
	}

	extent_count = 0;

	/* Validate and total every exact-rank extent table. */
	for (i = 0; i < param_count; i++) {
		if (restricted[i] != 0 && restricted[i] != 1)
			return false;
		if (rank[i] > NOCT_FAST_RANK_MAX)
			return false;

		extent_count += rank[i];
	}

	if (extent_count > 0 &&
	    (extent_kind == NULL || extent_value == NULL)) {
		return false;
	}

	fast_signature_init(&candidate);
	candidate.valid = true;
	candidate.param_count = param_count;
	candidate.return_type = return_type;

	if (param_count > 0) {
		candidate.param = noct_calloc(
			(size_t)param_count,
			sizeof(*candidate.param));
		if (candidate.param == NULL)
			return false;
	}

	extent_count = 0;

	/* Restore every parameter and its sparse exact-rank extent table. */
	for (i = 0; i < param_count; i++) {
		contract = &candidate.param[i];
		contract->value_type = value_type[i];
		contract->packed_type = packed_type[i];
		contract->restricted = restricted[i] != 0;
		contract->rank = rank[i];

		if (rank[i] == 0)
			continue;

		contract->extent = noct_calloc(
			(size_t)rank[i],
			sizeof(*contract->extent));
		if (contract->extent == NULL) {
			fast_signature_free(&candidate);
			return false;
		}

		/* Decode this parameter's consecutive extent entries. */
		for (axis = 0; axis < rank[i]; axis++) {
			extent = &contract->extent[axis];
			extent->kind = extent_kind[extent_count];

			if (extent->kind == FAST_EXTENT_CONST) {
				extent->value.constant =
					extent_value[extent_count];
			} else if (extent->kind == FAST_EXTENT_PARAM) {
				if (extent_value[extent_count] < 0) {
					fast_signature_free(&candidate);
					return false;
				}

				param_index = (uint32_t)extent_value[extent_count];
				if ((int64_t)param_index !=
				    extent_value[extent_count]) {
					fast_signature_free(&candidate);
					return false;
				}
				extent->value.param_index = param_index;
			} else {
				fast_signature_free(&candidate);
				return false;
			}

			extent_count++;
		}
	}

	if (!fast_signature_valid(&candidate)) {
		fast_signature_free(&candidate);
		return false;
	}

	fast_signature_free(&func->fast_signature);
	func->fast_signature = candidate;
	func->is_fast = true;
	func->tmpvar_size = tmpvar_size;
	func->return_type = return_type;
	func->return_packed_type = -1;

	/* Mirror the contract in the ordinary runtime metadata. */
	for (i = 0; i < param_count; i++) {
		func->param_type[i] = value_type[i];
		func->param_packed_type[i] = packed_type[i];
		func->param_restricted[i] = restricted[i] != 0;
	}

	return true;
}

/*
 * Pinning Native APIs
 */

/*
 * Pins a C global variable.
 */
bool
rt_pin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_pin_global(env, val))
		return false;

	return true;
}

/*
 * Unpins a C global variable.
 */
bool
rt_unpin_global(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_unpin_global(env, val))
		return false;

	return true;
}

/*
 * Pin a C local variable.
 */
bool
rt_pin_local(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_pin_local(env, val))
		return false;

	return true;
}

/*
 * Unpin a C local variable.
 */
bool
rt_unpin_local(
	struct rt_env *env,
	struct rt_value *val)
{
	assert(env != NULL);
	assert(val != NULL);

	if (!rt_gc_unpin_local(env, val))
		return false;

	return true;
}

/*
 * Make a safepoint.
 */
bool
rt_safepoint(
	struct rt_env *env)
{
	om_safepoint(env);

	return true;
}

/*
 * Error Handling
 */

/*
 * Get an error message.
 */
const char *
rt_get_error_message(
	struct rt_env *env)
{
	return &env->error_message[0];
}

/*
 * Get an error file name.
 */
const char *
rt_get_error_file(
	struct rt_env *env)
{
	return &env->file_name[0];
}

/*
 * Get an error line number.
 */
int
rt_get_error_line(
	struct rt_env *env)
{
	return env->line;
}

/*
 * Output an error message.
 */
void
rt_error(
	struct rt_env *env,
	const char *msg,
	...)
{
	va_list ap;

	va_start(ap, msg);
	vsnprintf(env->error_message, sizeof(env->error_message) - 1, msg, ap);
	va_end(ap);
}

/*
 * Output an out-of-memory message.
 */
void
rt_out_of_memory(
	struct rt_env *env)
{
	noct_error(env, N_TR("Out of memory."));
}
