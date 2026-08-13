/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

#ifndef NOCT_ACCEL_H
#define NOCT_ACCEL_H

#include <noct/noct.h>

/* Mutually exclusive source/runtime function kinds. */
enum noct_func_kind {
	NOCT_FUNC_NORMAL,
	NOCT_FUNC_ACCEL,
	NOCT_FUNC_GPU,
	NOCT_FUNC_FAST
};

/* How a parameter crosses the host/device boundary. */
enum accel_param_transport {
	ACCEL_TRANSPORT_SCALAR,
	ACCEL_TRANSPORT_COPY_IN,
	ACCEL_TRANSPORT_COPY_OUT,
	ACCEL_TRANSPORT_DEVICE_PTR
};

/* Effects are independent from transport (DEVICE_PTR may be read/write). */
enum accel_param_effect {
	ACCEL_EFFECT_NONE = 0,
	ACCEL_EFFECT_READ = 1,
	ACCEL_EFFECT_WRITE = 2
};

enum accel_range_status {
	ACCEL_RANGE_NOT_APPLICABLE,
	ACCEL_RANGE_COMPLETE,
	ACCEL_RANGE_UNAVAILABLE
};

enum accel_parallel_mode {
	ACCEL_PARALLEL_NOT_APPLICABLE,
	ACCEL_PARALLEL_DOALL,
	ACCEL_PARALLEL_SERIAL
};

/* For managed i in [0,count): required end is count + max_offset. */
struct accel_param_range {
	int status;
	bool has_access;
	int64_t min_offset;
	int64_t max_offset;
};

/* Compatibility names for the v1 backends while they migrate to transport. */
enum accel_access {
	ACCEL_ACCESS_NONE = ACCEL_TRANSPORT_SCALAR,
	ACCEL_ACCESS_IN = ACCEL_TRANSPORT_COPY_IN,
	ACCEL_ACCESS_OUT = ACCEL_TRANSPORT_COPY_OUT
};

/* Backend-neutral compiler/runtime descriptor. */
struct accel_kernel {
	uint32_t descriptor_version;
	int func_kind;
	bool eligible;
	int rejection_reason;
	int parallel_mode;
	char *name;
	char *source_name;
	int source_line;
	uint32_t param_count;
	int param_type[NOCT_ARG_MAX];
	int param_packed_type[NOCT_ARG_MAX];
	int param_access[NOCT_ARG_MAX];
	int param_transport[NOCT_ARG_MAX];
	unsigned int param_effect[NOCT_ARG_MAX];
	struct accel_param_range param_range[NOCT_ARG_MAX];
	int output_param;
	int dispatch_param;
	char *glsl;
	size_t glsl_size;
	char *hlsl;
	size_t hlsl_size;
	uint32_t content_hash;
	void *backend_data;
};

#define ACCEL_PROGRAM_VERSION 3
#define ACCEL_PROGRAM_MAX_EXPRS 256
#define ACCEL_PROGRAM_MAX_BUFFERS 64
#define ACCEL_PROGRAM_MAX_KERNELS 64
#define ACCEL_PROGRAM_MAX_STEPS 128

enum accel_expr_op {
	ACCEL_EXPR_CONST,
	ACCEL_EXPR_SCALAR_ARG,
	ACCEL_EXPR_BUFFER_LENGTH,
	ACCEL_EXPR_ADD_CONST,
	ACCEL_EXPR_MUL_CONST,
	ACCEL_EXPR_MIN,
	ACCEL_EXPR_MAX,
	ACCEL_EXPR_CEIL_DIV_CONST
};

struct accel_expr {
	int op;
	int left;
	int right;
	int ref;
	int64_t value;
};

enum accel_buffer_origin {
	ACCEL_BUFFER_HOST_IN,
	ACCEL_BUFFER_HOST_OUT,
	ACCEL_BUFFER_DEVICE_PTR,
	ACCEL_BUFFER_LOCAL,
	ACCEL_BUFFER_REDUCTION_RESULT,
	ACCEL_BUFFER_SCRATCH
};

struct accel_buffer_desc {
	int id;
	char *name;
	int source_line;
	int origin;
	int outer_param;
	int element_kind;
	int element_width;
	int length_expr;
	int read_start_expr;
	int read_end_expr;
	int write_start_expr;
	int write_end_expr;
	int first_step;
	int last_step;
	bool initially_defined;
	bool upload;
	bool download;
};

enum accel_binding_kind {
	ACCEL_BIND_BUFFER,
	ACCEL_BIND_SCALAR_EXPR
};

struct accel_binding {
	int kind;
	int kernel_param;
	int value;
};

enum accel_step_kind {
	ACCEL_STEP_DOALL_DISPATCH,
	ACCEL_STEP_DOSUM_REDUCTION,
	ACCEL_STEP_DEVICE_COPY
};

enum accel_reduction_operator {
	ACCEL_REDUCTION_NONE,
	ACCEL_REDUCTION_ADD
};

struct accel_program_step {
	int kind;
	int source_line;
	int kernel;
	int fold_kernel;
	int trip_expr;
	uint32_t block_size;
	int result_buffer;
	int scratch_buffer;
	int scratch_buffer2;
	int reduction_operator;
	int reduction_type;
	uint32_t binding_count;
	struct accel_binding binding[NOCT_ARG_MAX];
};

struct accel_program {
	uint32_t descriptor_version;
	char *name;
	char *source_name;
	int source_line;
	int rejection_reason;
	uint32_t outer_param_count;
	unsigned int outer_param_effect[NOCT_ARG_MAX];
	struct accel_param_range outer_param_range[NOCT_ARG_MAX];
	uint32_t expr_count;
	struct accel_expr *expr;
	uint32_t buffer_count;
	struct accel_buffer_desc *buffer;
	uint32_t kernel_count;
	struct accel_kernel **kernel;
	uint32_t step_count;
	struct accel_program_step *step;
};

struct accel_kernel *accel_kernel_clone(const struct accel_kernel *src);
void accel_kernel_free(struct accel_kernel *kernel);
struct accel_program *accel_program_clone(const struct accel_program *src);
void accel_program_free(struct accel_program *program);
bool accel_program_validate(const struct accel_program *program,
			    char *error, size_t error_size);
bool accel_expr_evaluate(const struct accel_program *program,
			 int expr_index,
			 uint32_t arg_count,
			 const int64_t *scalar_arg,
			 const int64_t *buffer_length,
			 int64_t *result);
bool rt_register_accel_intrinsics(struct rt_env *env);

enum accel_dispatch_result {
	ACCEL_DISPATCH_ERROR = -1,
	ACCEL_DISPATCH_FALLBACK = 0,
	ACCEL_DISPATCH_OK = 1
};

#define ACCEL_EVENT_MAX 64
enum accel_event_state {
	ACCEL_EVENT_FREE,
	ACCEL_EVENT_RESERVED,
	ACCEL_EVENT_SUBMITTED,
	ACCEL_EVENT_COMPLETE,
	ACCEL_EVENT_JOINED
};

struct accel_event {
	uint32_t generation;
	int state;
	struct rt_value output;
	bool output_pinned;
	uint32_t retained_count;
	struct rt_value retained[NOCT_ARG_MAX + 1];
	void *backend_data;
};

int accel_vulkan_dispatch(struct rt_env *env, struct rt_func *func,
			  uint32_t arg_count, struct rt_value *arg);
bool accel_vulkan_list_devices(void);
int accel_vulkan_copy_to(struct rt_env *env, struct rt_packed *resource,
			 size_t offset, size_t size);
int accel_vulkan_copy_from(struct rt_env *env, struct rt_packed *resource,
			   size_t offset, size_t size);
void accel_vulkan_cleanup(struct rt_vm *vm);
int accel_dx12_dispatch(struct rt_env *env, struct rt_func *func,
			uint32_t arg_count, struct rt_value *arg);
int accel_dx12_dispatch_raw(struct rt_env *env, struct rt_func *func,
			    uint32_t grid_size, uint32_t block_size,
			    uint32_t arg_count, struct rt_value *arg,
			    struct accel_event *event);
bool accel_dx12_join(struct rt_env *env, struct accel_event *event);
bool accel_dx12_list_devices(void);
int accel_dx12_copy_async(struct rt_env *env, bool to_accel,
			  struct rt_packed *source, size_t source_offset,
			  struct rt_packed *destination,
			  size_t destination_offset, size_t size,
			  struct accel_event *event);
int accel_dx12_copy_to(struct rt_env *env, struct rt_packed *resource,
		       size_t offset, size_t size);
int accel_dx12_copy_from(struct rt_env *env, struct rt_packed *resource,
			 size_t offset, size_t size);
void accel_dx12_cleanup(struct rt_vm *vm);
int accel_opengl_dispatch(struct rt_env *env, struct rt_func *func,
			  uint32_t arg_count, struct rt_value *arg);
bool accel_opengl_list_devices(void);
int accel_opengl_dispatch_async(struct rt_env *env, struct rt_func *func,
				uint32_t arg_count, struct rt_value *arg,
				struct accel_event *event);
int accel_opengl_dispatch_raw_async(struct rt_env *env, struct rt_func *func,
				    uint32_t grid_size, uint32_t block_size,
				    uint32_t arg_count, struct rt_value *arg,
				    struct accel_event *event);
bool accel_opengl_join(struct rt_env *env, struct accel_event *event);
int accel_opengl_copy_to(struct rt_env *env, struct rt_packed *resource,
			 size_t offset, size_t size);
int accel_opengl_copy_from(struct rt_env *env, struct rt_packed *resource,
			   size_t offset, size_t size);
int accel_opengl_copy_async(struct rt_env *env, bool to_accel,
			    struct rt_packed *source, size_t source_offset,
			    struct rt_packed *destination,
			    size_t destination_offset, size_t size,
			    struct accel_event *event);
bool accel_opengl_sync_cpu(struct rt_env *env, struct rt_func *func,
			   uint32_t arg_count, struct rt_value *arg,
			   bool before_call);
void accel_opengl_cleanup(struct rt_vm *vm);
void accel_runtime_cleanup(struct rt_vm *vm);

#endif
