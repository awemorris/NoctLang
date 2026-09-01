/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Vulkan 1.2 accelerator backend.
 */

#include "accel_vulkan.h"
#include "accel_context.h"
#include "accel_mutex.h"
#include "accel_vulkan_shader.h"
#include "hir.h"
#include "objectmodel.h"
#include "runtime.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define ACCEL_VULKAN_WORKGROUP_SIZE	64U
#define ACCEL_VULKAN_SESSION_MAGIC	0x4e564b53U

enum accel_session_state {
	ACCEL_SESSION_RECORDING,
	ACCEL_SESSION_SUBMITTING,
	ACCEL_SESSION_COPY_READY,
	ACCEL_SESSION_FINISHED,
	ACCEL_SESSION_FAILED,
	ACCEL_SESSION_ORPHANED
};

struct accel_vulkan_backend {
	struct accel_vulkan_api api;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFloatControlsProperties float_controls;
	VkPhysicalDeviceMemoryProperties memory_properties;
	struct accel_mutex queue_mutex;
};

struct accel_vulkan_kernel {
	VkShaderModule shader_module;
	VkDescriptorSetLayout descriptor_layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
	bool uses_f32;
};

struct accel_vulkan_prepared {
	struct accel_program *program;
	struct accel_vulkan_kernel *kernel;
	uint32_t kernel_count;
};

struct accel_vulkan_buffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	void *mapped;
	VkDeviceSize logical_size;
	VkDeviceSize allocation_size;
	uint32_t memory_type;
	bool coherent;
	bool active;
	bool upload;
	bool download;
};

struct accel_vulkan_session {
	uint32_t magic;
	struct rt_vm *vm;
	struct accel_context *context;
	struct accel_live_session live;
	enum accel_session_state state;
	const struct accel_vulkan_prepared *prepared;
	uint32_t program_id;
	uint32_t buffer_count;
	uint32_t next_kernel;
	int64_t *scalar_value;
	uint32_t *scalar_word;
	uint32_t *kernel_start;
	uint32_t *kernel_trip;
	bool *kernel_active;
	struct accel_vulkan_buffer *buffer;
	void **snapshot;
	size_t *element_count;
	int *packed_type;
	struct accel_vulkan_buffer scalar_buffer;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet *descriptor_set;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	VkFence fence;
	bool command_started;
	bool has_commands;
};

static bool accel_vulkan_api_valid(const struct accel_vulkan_api *api);
static void accel_vulkan_initialization_error(struct rt_env *env, const char *message);
static bool accel_vulkan_loader_version(struct rt_env *env, const struct accel_vulkan_api *api);
static bool accel_vulkan_create_instance(struct rt_env *env, struct accel_vulkan_backend *backend);
static bool accel_vulkan_resolve_instance_api(struct rt_env *env, struct accel_vulkan_backend *backend);
static bool accel_vulkan_select_device(struct rt_env *env, struct accel_vulkan_backend *backend, const char *gpu_name);
static bool accel_vulkan_device_candidate(struct accel_vulkan_backend *backend, VkPhysicalDevice device, const char *gpu_name, uint32_t *queue_family, uint32_t *score, bool *name_match);
static bool accel_vulkan_find_queue_family(struct accel_vulkan_backend *backend, VkPhysicalDevice device, uint32_t *queue_family);
static uint32_t accel_vulkan_device_score(const VkPhysicalDeviceProperties *properties);
static bool accel_vulkan_create_device(struct rt_env *env, struct accel_vulkan_backend *backend);
static enum accel_compile_status accel_vulkan_prepare_program(void *backend_state, const struct accel_program *program, struct accel_prepared_program *result);
static bool accel_vulkan_program_uses_f32(const struct accel_program *program);
static bool accel_vulkan_prepare_kernel(struct accel_vulkan_backend *backend, shaderc_compiler_t compiler, shaderc_compile_options_t options, const struct accel_program *program, uint32_t kernel_index, struct accel_vulkan_kernel *result);
static void accel_vulkan_destroy_kernel(struct accel_vulkan_backend *backend, struct accel_vulkan_kernel *kernel);
static void accel_vulkan_destroy_prepared_program(void *backend_state, struct accel_prepared_program *program);
static bool accel_vulkan_register_runtime(struct accel_context *context, struct rt_env *env);
static void accel_vulkan_destroy_backend_state(void *backend_state);
static bool accel_vulkan_begin(struct rt_env *env);
static bool accel_vulkan_dispatch(struct rt_env *env);
static bool accel_vulkan_finish(struct rt_env *env);
static bool accel_vulkan_begin_metadata(struct rt_env *env, struct rt_value *args, struct rt_value *element, struct accel_vulkan_session *session);
static bool accel_vulkan_begin_resources(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_allocate_session_metadata(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_read_scalars(struct rt_env *env, struct rt_value *args, struct rt_value *element, struct accel_vulkan_session *session, size_t args_size);
static bool accel_vulkan_evaluate_kernels(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_read_buffers(struct rt_env *env, struct rt_value *args, struct rt_value *element, struct accel_vulkan_session *session, size_t args_size);
static bool accel_vulkan_buffer_plan(struct rt_env *env, struct accel_vulkan_session *session, uint32_t buffer_index, size_t element_count);
static bool accel_vulkan_create_host_buffer(struct rt_env *env, struct accel_vulkan_backend *backend, VkDeviceSize logical_size, VkBufferUsageFlags usage, struct accel_vulkan_buffer *buffer);
static bool accel_vulkan_find_host_memory(const struct accel_vulkan_backend *backend, uint32_t memory_type_bits, uint32_t *memory_type, bool *coherent);
static bool accel_vulkan_round_allocation(struct rt_env *env, VkDeviceSize value, VkDeviceSize atom, VkDeviceSize *result);
static void accel_vulkan_destroy_buffer(struct accel_vulkan_backend *backend, struct accel_vulkan_buffer *buffer);
static bool accel_vulkan_create_descriptors(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_kernel_uses_buffer(const struct accel_ir_kernel *kernel, uint32_t buffer_index);
static bool accel_vulkan_record_begin(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_flush_session(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_invalidate_session(struct rt_env *env, struct accel_vulkan_session *session);
static bool accel_vulkan_copy_results(struct rt_env *env, struct rt_value *args, struct rt_value *element, struct accel_vulkan_session *session);
static bool accel_vulkan_fail_session_locked(struct rt_env *env, struct accel_vulkan_session *session, const char *message);
static void accel_vulkan_session_finalizer(void *native_pointer);
static void accel_vulkan_session_orphan_locked(struct accel_live_session *live);
static void accel_vulkan_session_close_locked(struct accel_vulkan_session *session);
static void accel_vulkan_session_destroy(struct accel_vulkan_session *session);
static struct accel_context *accel_vulkan_current_context(struct rt_env *env);
static bool accel_vulkan_get_session_argument(struct rt_env *env, struct rt_value *value, struct accel_vulkan_session **session);
static bool accel_vulkan_pin(struct rt_env *env, struct rt_value *value, uint32_t *count);
static bool accel_vulkan_unpin(struct rt_env *env, struct rt_value *value, uint32_t *count);

static const struct accel_backend_ops accel_vulkan_ops = {
	accel_vulkan_prepare_program,
	accel_vulkan_destroy_prepared_program,
	accel_vulkan_register_runtime,
	accel_vulkan_destroy_backend_state
};

static const struct accel_vulkan_api accel_vulkan_real_api = {
	vkGetInstanceProcAddr,
	vkCreateInstance,
	vkDestroyInstance,
	vkEnumeratePhysicalDevices,
	NULL,
	vkGetPhysicalDeviceQueueFamilyProperties,
	vkGetPhysicalDeviceMemoryProperties,
	vkCreateDevice,
	vkDestroyDevice,
	vkGetDeviceQueue,
	vkDeviceWaitIdle,
	vkCreateShaderModule,
	vkDestroyShaderModule,
	vkCreateDescriptorSetLayout,
	vkDestroyDescriptorSetLayout,
	vkCreatePipelineLayout,
	vkDestroyPipelineLayout,
	vkCreateComputePipelines,
	vkDestroyPipeline,
	vkCreateBuffer,
	vkDestroyBuffer,
	vkGetBufferMemoryRequirements,
	vkAllocateMemory,
	vkFreeMemory,
	vkBindBufferMemory,
	vkMapMemory,
	vkUnmapMemory,
	vkFlushMappedMemoryRanges,
	vkInvalidateMappedMemoryRanges,
	vkCreateDescriptorPool,
	vkDestroyDescriptorPool,
	vkAllocateDescriptorSets,
	vkUpdateDescriptorSets,
	vkCreateCommandPool,
	vkDestroyCommandPool,
	vkAllocateCommandBuffers,
	vkBeginCommandBuffer,
	vkEndCommandBuffer,
	vkCmdBindPipeline,
	vkCmdBindDescriptorSets,
	vkCmdCopyBuffer,
	vkCmdPipelineBarrier,
	vkCmdFillBuffer,
	vkCmdDispatch,
	vkCreateFence,
	vkDestroyFence,
	vkQueueSubmit,
	vkWaitForFences
};

static const char *accel_vulkan_begin_parameter[] = {
	"programId",
	"args"
};

static const char *accel_vulkan_dispatch_parameter[] = {
	"session",
	"kernelIndex"
};

static const char *accel_vulkan_finish_parameter[] = {
	"session",
	"args"
};

/*
 * Creates the production Vulkan backend with the process loader table.
 */
bool
accel_vulkan_create(
	struct rt_env *env,
	const char *gpu_name,
	const struct accel_backend_ops **ops,
	void **backend_state)
{
	return accel_vulkan_create_with_api(
		env,
		gpu_name,
		&accel_vulkan_real_api,
		ops,
		backend_state);
}

/*
 * Creates a Vulkan backend through an injected function table.
 *
 * The function table is copied into the returned backend state.  Tests may
 * therefore release the input table after this call returns.
 */
bool
accel_vulkan_create_with_api(
	struct rt_env *env,
	const char *gpu_name,
	const struct accel_vulkan_api *api,
	const struct accel_backend_ops **ops,
	void **backend_state)
{
	struct accel_vulkan_backend *backend;

	if (ops != NULL)
		*ops = NULL;
	if (backend_state != NULL)
		*backend_state = NULL;

	if (env == NULL || ops == NULL || backend_state == NULL)
		return false;
	if (!accel_vulkan_api_valid(api)) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Incomplete Vulkan function table."));
		return false;
	}

	backend = noct_calloc(1, sizeof(*backend));
	if (backend == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	backend->api = *api;
	backend->api.get_physical_device_properties2 = NULL;

	if (!accel_vulkan_loader_version(env, &backend->api))
		goto error;
	if (!accel_vulkan_create_instance(env, backend))
		goto error;
	if (!accel_vulkan_resolve_instance_api(env, backend))
		goto error;
	if (!accel_vulkan_select_device(env, backend, gpu_name))
		goto error;
	if (!accel_vulkan_create_device(env, backend))
		goto error;
	if (!accel_mutex_init(&backend->queue_mutex)) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Failed to initialize the Vulkan queue mutex."));
		goto error;
	}

	*ops = &accel_vulkan_ops;
	*backend_state = backend;

	return true;

error:
	accel_vulkan_destroy_backend_state(backend);

	return false;
}

/* Validate every required bootstrap and runtime Vulkan entry point. */
static bool
accel_vulkan_api_valid(
	const struct accel_vulkan_api *api)
{
	if (api == NULL)
		return false;
	if (api->get_instance_proc_addr == NULL)
		return false;
	if (api->create_instance == NULL)
		return false;
	if (api->destroy_instance == NULL)
		return false;
	if (api->enumerate_physical_devices == NULL)
		return false;
	if (api->get_physical_device_queue_family_properties == NULL)
		return false;
	if (api->get_physical_device_memory_properties == NULL)
		return false;
	if (api->create_device == NULL)
		return false;
	if (api->destroy_device == NULL)
		return false;
	if (api->get_device_queue == NULL)
		return false;
	if (api->device_wait_idle == NULL)
		return false;
	if (api->create_shader_module == NULL)
		return false;
	if (api->destroy_shader_module == NULL)
		return false;
	if (api->create_descriptor_set_layout == NULL)
		return false;
	if (api->destroy_descriptor_set_layout == NULL)
		return false;
	if (api->create_pipeline_layout == NULL)
		return false;
	if (api->destroy_pipeline_layout == NULL)
		return false;
	if (api->create_compute_pipelines == NULL)
		return false;
	if (api->destroy_pipeline == NULL)
		return false;
	if (api->create_buffer == NULL)
		return false;
	if (api->destroy_buffer == NULL)
		return false;
	if (api->get_buffer_memory_requirements == NULL)
		return false;
	if (api->allocate_memory == NULL)
		return false;
	if (api->free_memory == NULL)
		return false;
	if (api->bind_buffer_memory == NULL)
		return false;
	if (api->map_memory == NULL)
		return false;
	if (api->unmap_memory == NULL)
		return false;
	if (api->flush_mapped_memory_ranges == NULL)
		return false;
	if (api->invalidate_mapped_memory_ranges == NULL)
		return false;
	if (api->create_descriptor_pool == NULL)
		return false;
	if (api->destroy_descriptor_pool == NULL)
		return false;
	if (api->allocate_descriptor_sets == NULL)
		return false;
	if (api->update_descriptor_sets == NULL)
		return false;
	if (api->create_command_pool == NULL)
		return false;
	if (api->destroy_command_pool == NULL)
		return false;
	if (api->allocate_command_buffers == NULL)
		return false;
	if (api->begin_command_buffer == NULL)
		return false;
	if (api->end_command_buffer == NULL)
		return false;
	if (api->cmd_bind_pipeline == NULL)
		return false;
	if (api->cmd_bind_descriptor_sets == NULL)
		return false;
	if (api->cmd_copy_buffer == NULL)
		return false;
	if (api->cmd_pipeline_barrier == NULL)
		return false;
	if (api->cmd_fill_buffer == NULL)
		return false;
	if (api->cmd_dispatch == NULL)
		return false;
	if (api->create_fence == NULL)
		return false;
	if (api->destroy_fence == NULL)
		return false;
	if (api->queue_submit == NULL)
		return false;
	if (api->wait_for_fences == NULL)
		return false;

	return true;
}

/* Set one constant Vulkan initialization diagnostic. */
static void
accel_vulkan_initialization_error(
	struct rt_env *env,
	const char *message)
{
	rt_error(env, "%s", message);
}

/* Require a working Vulkan 1.2 loader before creating an instance. */
static bool
accel_vulkan_loader_version(
	struct rt_env *env,
	const struct accel_vulkan_api *api)
{
	PFN_vkEnumerateInstanceVersion enumerate_version;
	VkResult vk_result;
	uint32_t version;

	enumerate_version = (PFN_vkEnumerateInstanceVersion)
		api->get_instance_proc_addr(
			VK_NULL_HANDLE,
			"vkEnumerateInstanceVersion");
	if (enumerate_version == NULL) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Vulkan 1.2 is required, but the loader only exposes Vulkan 1.0."));
		return false;
	}

	version = 0;
	vk_result = enumerate_version(&version);
	if (vk_result != VK_SUCCESS) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Failed to query the Vulkan loader version."));
		return false;
	}
	if (version < VK_API_VERSION_1_2) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Vulkan 1.2 or newer is required."));
		return false;
	}

	return true;
}

/* Create the private Vulkan 1.2 instance. */
static bool
accel_vulkan_create_instance(
	struct rt_env *env,
	struct accel_vulkan_backend *backend)
{
	VkApplicationInfo application_info;
	VkInstanceCreateInfo create_info;
	VkResult result;

	memset(&application_info, 0, sizeof(application_info));
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pApplicationName = "Noct";
	application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.pEngineName = "Noct Accel";
	application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	application_info.apiVersion = VK_API_VERSION_1_2;

	memset(&create_info, 0, sizeof(create_info));
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &application_info;

	result = backend->api.create_instance(
		&create_info,
		NULL,
		&backend->instance);
	if (result != VK_SUCCESS) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Failed to create a Vulkan 1.2 instance."));
		return false;
	}

	return true;
}

/* Resolve the required Vulkan 1.2 physical-device property entry point. */
static bool
accel_vulkan_resolve_instance_api(
	struct rt_env *env,
	struct accel_vulkan_backend *backend)
{
	backend->api.get_physical_device_properties2 =
		(PFN_vkGetPhysicalDeviceProperties2)
		backend->api.get_instance_proc_addr(
			backend->instance,
			"vkGetPhysicalDeviceProperties2");
	if (backend->api.get_physical_device_properties2 == NULL) {
		accel_vulkan_initialization_error(
			env,
			N_TR("The Vulkan loader did not expose Vulkan 1.2 physical-device properties."));
		return false;
	}

	return true;
}

/* Select one exact or highest-scoring suitable compute device. */
static bool
accel_vulkan_select_device(
	struct rt_env *env,
	struct accel_vulkan_backend *backend,
	const char *gpu_name)
{
	VkPhysicalDevice *device;
	VkPhysicalDevice selected;
	VkResult result;
	uint32_t device_count;
	uint32_t queue_family;
	uint32_t selected_queue_family;
	uint32_t score;
	uint32_t selected_score;
	uint32_t match_count;
	uint32_t i;
	bool name_match;
	bool suitable;

	device_count = 0;
	result = backend->api.enumerate_physical_devices(
		backend->instance,
		&device_count,
		NULL);
	if (result != VK_SUCCESS || device_count == 0) {
		accel_vulkan_initialization_error(
			env,
			N_TR("No Vulkan physical device is available."));
		return false;
	}

	device = noct_malloc(sizeof(*device) * device_count);
	if (device == NULL) {
		rt_out_of_memory(env);
		return false;
	}

	result = backend->api.enumerate_physical_devices(
		backend->instance,
		&device_count,
		device);
	if (result != VK_SUCCESS) {
		noct_free(device);
		accel_vulkan_initialization_error(
			env,
			N_TR("Failed to enumerate Vulkan physical devices."));
		return false;
	}

	selected = VK_NULL_HANDLE;
	selected_score = 0;
	selected_queue_family = 0;
	match_count = 0;

	/* Evaluate every device without opening a logical device. */
	for (i = 0; i < device_count; i++) {
		suitable = accel_vulkan_device_candidate(
			backend,
			device[i],
			gpu_name,
			&queue_family,
			&score,
			&name_match);
		if (!suitable)
			continue;
		if (gpu_name != NULL && !name_match)
			continue;
		if (gpu_name != NULL)
			match_count++;

		if (selected == VK_NULL_HANDLE || score > selected_score) {
			selected = device[i];
			selected_score = score;
			selected_queue_family = queue_family;
		}
	}

	noct_free(device);

	if (gpu_name != NULL && match_count == 0) {
		rt_error(env, N_TR("Vulkan device '%s' was not found."), gpu_name);
		return false;
	}
	if (gpu_name != NULL && match_count > 1) {
		rt_error(env, N_TR("Vulkan device name '%s' is ambiguous."), gpu_name);
		return false;
	}
	if (selected == VK_NULL_HANDLE) {
		accel_vulkan_initialization_error(
			env,
			N_TR("No suitable Vulkan 1.2 compute device is available."));
		return false;
	}

	backend->physical_device = selected;
	backend->queue_family = selected_queue_family;
	memset(&backend->float_controls, 0, sizeof(backend->float_controls));
	backend->float_controls.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
	memset(&backend->properties, 0, sizeof(backend->properties));
	{
		VkPhysicalDeviceProperties2 properties;

		memset(&properties, 0, sizeof(properties));
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &backend->float_controls;
		backend->api.get_physical_device_properties2(
			backend->physical_device,
			&properties);
		backend->properties = properties.properties;
	}
	backend->api.get_physical_device_memory_properties(
		backend->physical_device,
		&backend->memory_properties);

	return true;
}

/* Check one physical device against immutable initialization requirements. */
static bool
accel_vulkan_device_candidate(
	struct accel_vulkan_backend *backend,
	VkPhysicalDevice device,
	const char *gpu_name,
	uint32_t *queue_family,
	uint32_t *score,
	bool *name_match)
{
	VkPhysicalDeviceProperties2 properties;
	VkPhysicalDeviceFloatControlsProperties float_controls;

	memset(&float_controls, 0, sizeof(float_controls));
	float_controls.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES;
	memset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &float_controls;
	backend->api.get_physical_device_properties2(device, &properties);

	*name_match = false;
	if (gpu_name != NULL && strcmp(properties.properties.deviceName, gpu_name) == 0)
		*name_match = true;

	if (properties.properties.apiVersion < VK_API_VERSION_1_2)
		return false;
	if (properties.properties.limits.maxComputeWorkGroupInvocations <
	    ACCEL_VULKAN_WORKGROUP_SIZE) {
		return false;
	}
	if (properties.properties.limits.maxComputeWorkGroupSize[0] <
	    ACCEL_VULKAN_WORKGROUP_SIZE) {
		return false;
	}
	if (!accel_vulkan_find_queue_family(
		backend,
		device,
		queue_family)) {
		return false;
	}

	*score = accel_vulkan_device_score(&properties.properties);

	return true;
}

/* Find the first compute-capable queue family for one physical device. */
static bool
accel_vulkan_find_queue_family(
	struct accel_vulkan_backend *backend,
	VkPhysicalDevice device,
	uint32_t *queue_family)
{
	VkQueueFamilyProperties *properties;
	uint32_t count;
	uint32_t i;
	bool found;

	count = 0;
	backend->api.get_physical_device_queue_family_properties(
		device,
		&count,
		NULL);
	if (count == 0)
		return false;

	properties = noct_malloc(sizeof(*properties) * count);
	if (properties == NULL)
		return false;

	backend->api.get_physical_device_queue_family_properties(
		device,
		&count,
		properties);
	found = false;

	/* Select the first family that can execute compute commands. */
	for (i = 0; i < count; i++) {
		if ((properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)
			continue;

		*queue_family = i;
		found = true;
		break;
	}

	noct_free(properties);

	return found;
}

/* Give deterministic preference to discrete and integrated GPUs. */
static uint32_t
accel_vulkan_device_score(
	const VkPhysicalDeviceProperties *properties)
{
	/* Rank broad Vulkan device classes before stable enumeration order. */
	switch (properties->deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return 500;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return 400;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return 300;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		return 200;
	default:
		return 100;
	}
}

/* Create the selected device and borrow its one compute queue. */
static bool
accel_vulkan_create_device(
	struct rt_env *env,
	struct accel_vulkan_backend *backend)
{
	VkDeviceQueueCreateInfo queue_info;
	VkDeviceCreateInfo create_info;
	VkResult result;
	float priority;

	priority = 1.0f;
	memset(&queue_info, 0, sizeof(queue_info));
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueFamilyIndex = backend->queue_family;
	queue_info.queueCount = 1;
	queue_info.pQueuePriorities = &priority;

	memset(&create_info, 0, sizeof(create_info));
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = 1;
	create_info.pQueueCreateInfos = &queue_info;

	result = backend->api.create_device(
		backend->physical_device,
		&create_info,
		NULL,
		&backend->device);
	if (result != VK_SUCCESS) {
		accel_vulkan_initialization_error(
			env,
			N_TR("Failed to create the selected Vulkan compute device."));
		return false;
	}

	backend->api.get_device_queue(
		backend->device,
		backend->queue_family,
		0,
		&backend->queue);
	if (backend->queue == VK_NULL_HANDLE) {
		accel_vulkan_initialization_error(
			env,
			N_TR("The selected Vulkan compute queue is unavailable."));
		return false;
	}

	return true;
}

/* Prepare every immutable shader pipeline for one accelerator program. */
static enum accel_compile_status
accel_vulkan_prepare_program(
	void *backend_state,
	const struct accel_program *program,
	struct accel_prepared_program *result)
{
	struct accel_vulkan_backend *backend;
	struct accel_vulkan_prepared *prepared;
	shaderc_compiler_t compiler;
	shaderc_compile_options_t options;
	char validation_error[128];
	uint32_t descriptor_count;
	uint32_t i;

	if (result == NULL)
		return ACCEL_COMPILE_ERROR;

	result->payload = NULL;
	backend = backend_state;
	if (backend == NULL || program == NULL) {
		hir_error(0, N_TR("Invalid Vulkan program preparation request."));
		return ACCEL_COMPILE_ERROR;
	}
	if (!accel_program_validate(
		program,
		validation_error,
		sizeof(validation_error))) {
		hir_error(
			program->source_line,
			N_TR("Invalid accelerator program reached the Vulkan backend."));
		return ACCEL_COMPILE_ERROR;
	}

	descriptor_count = program->buffer_count + 1;
	if (descriptor_count >
	    backend->properties.limits.maxPerStageDescriptorStorageBuffers) {
		return ACCEL_COMPILE_DECLINED;
	}
	if (descriptor_count >
	    backend->properties.limits.maxDescriptorSetStorageBuffers) {
		return ACCEL_COMPILE_DECLINED;
	}
	if (ACCEL_VULKAN_WORKGROUP_SIZE >
	    backend->properties.limits.maxComputeWorkGroupInvocations) {
		return ACCEL_COMPILE_DECLINED;
	}
	if (ACCEL_VULKAN_WORKGROUP_SIZE >
	    backend->properties.limits.maxComputeWorkGroupSize[0]) {
		return ACCEL_COMPILE_DECLINED;
	}
	if (accel_vulkan_program_uses_f32(program)) {
		if (!backend->float_controls.shaderSignedZeroInfNanPreserveFloat32)
			return ACCEL_COMPILE_DECLINED;
		if (!backend->float_controls.shaderDenormPreserveFloat32)
			return ACCEL_COMPILE_DECLINED;
		if (!backend->float_controls.shaderRoundingModeRTEFloat32)
			return ACCEL_COMPILE_DECLINED;
	}

	prepared = noct_calloc(1, sizeof(*prepared));
	if (prepared == NULL) {
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	prepared->kernel = noct_calloc(
		program->kernel_count,
		sizeof(*prepared->kernel));
	if (prepared->kernel == NULL) {
		noct_free(prepared);
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}
	prepared->kernel_count = program->kernel_count;

	prepared->program = accel_program_clone(program);
	if (prepared->program == NULL) {
		noct_free(prepared->kernel);
		noct_free(prepared);
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	compiler = shaderc_compiler_initialize();
	if (compiler == NULL) {
		result->payload = prepared;
		accel_vulkan_destroy_prepared_program(backend, result);
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	options = shaderc_compile_options_initialize();
	if (options == NULL) {
		shaderc_compiler_release(compiler);
		result->payload = prepared;
		accel_vulkan_destroy_prepared_program(backend, result);
		hir_out_of_memory();
		return ACCEL_COMPILE_ERROR;
	}

	shaderc_compile_options_set_target_env(
		options,
		shaderc_target_env_vulkan,
		shaderc_env_version_vulkan_1_2);
	shaderc_compile_options_set_target_spirv(
		options,
		shaderc_spirv_version_1_5);
	shaderc_compile_options_set_optimization_level(
		options,
		shaderc_optimization_level_zero);

	/* Create every kernel pipeline before publishing the prepared payload. */
	for (i = 0; i < prepared->kernel_count; i++) {
		if (!accel_vulkan_prepare_kernel(
			backend,
			compiler,
			options,
			program,
			i,
			&prepared->kernel[i])) {
			shaderc_compile_options_release(options);
			shaderc_compiler_release(compiler);
			result->payload = prepared;
			accel_vulkan_destroy_prepared_program(backend, result);
			return ACCEL_COMPILE_ERROR;
		}
	}

	shaderc_compile_options_release(options);
	shaderc_compiler_release(compiler);
	result->payload = prepared;

	return ACCEL_COMPILE_APPLIED;
}

/* Detect whether any kernel requires strict Float32 device controls. */
static bool
accel_vulkan_program_uses_f32(
	const struct accel_program *program)
{
	const struct accel_ir_kernel *kernel;
	uint32_t i;
	uint32_t j;

	/* Inspect every buffer and instruction type in every kernel. */
	for (i = 0; i < program->kernel_count; i++) {
		kernel = program->kernel[i].ir;
		for (j = 0; j < kernel->buffer_binding_count; j++) {
			if (kernel->buffer_value_type[j] == ACCEL_IR_F32)
				return true;
		}
		for (j = 0; j < kernel->instruction_count; j++) {
			if (kernel->instruction[j].result_type == ACCEL_IR_F32)
				return true;
		}
	}

	return false;
}

/* Compile and create one immutable Vulkan compute pipeline. */
static bool
accel_vulkan_prepare_kernel(
	struct accel_vulkan_backend *backend,
	shaderc_compiler_t compiler,
	shaderc_compile_options_t options,
	const struct accel_program *program,
	uint32_t kernel_index,
	struct accel_vulkan_kernel *result)
{
	struct accel_vulkan_spirv spirv;
	VkShaderModuleCreateInfo shader_info;
	VkDescriptorSetLayoutCreateInfo descriptor_info;
	VkDescriptorSetLayoutBinding *binding;
	VkPipelineLayoutCreateInfo layout_info;
	VkPipelineShaderStageCreateInfo stage_info;
	VkComputePipelineCreateInfo pipeline_info;
	VkResult vk_result;
	uint32_t descriptor_count;
	uint32_t i;
	enum accel_compile_status status;

	memset(&spirv, 0, sizeof(spirv));
	memset(result, 0, sizeof(*result));

	status = accel_vulkan_shader_compile(
		compiler,
		options,
		program,
		kernel_index,
		&spirv);
	if (status != ACCEL_COMPILE_APPLIED)
		return false;

	memset(&shader_info, 0, sizeof(shader_info));
	shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_info.codeSize = spirv.word_count * sizeof(uint32_t);
	shader_info.pCode = spirv.word;
	vk_result = backend->api.create_shader_module(
		backend->device,
		&shader_info,
		NULL,
		&result->shader_module);
	accel_vulkan_shader_cleanup(&spirv);
	if (vk_result != VK_SUCCESS) {
		hir_error(
			program->kernel[kernel_index].source_line,
			N_TR("Failed to create a Vulkan shader module."));
		return false;
	}

	descriptor_count = program->buffer_count + 1;
	binding = noct_calloc(descriptor_count, sizeof(*binding));
	if (binding == NULL) {
		hir_out_of_memory();
		accel_vulkan_destroy_kernel(backend, result);
		return false;
	}

	/* Describe each data buffer followed by the scalar input block. */
	for (i = 0; i < descriptor_count; i++) {
		binding[i].binding = i;
		binding[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		binding[i].descriptorCount = 1;
		binding[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}

	memset(&descriptor_info, 0, sizeof(descriptor_info));
	descriptor_info.sType =
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_info.bindingCount = descriptor_count;
	descriptor_info.pBindings = binding;
	vk_result = backend->api.create_descriptor_set_layout(
		backend->device,
		&descriptor_info,
		NULL,
		&result->descriptor_layout);
	noct_free(binding);
	if (vk_result != VK_SUCCESS) {
		hir_error(
			program->kernel[kernel_index].source_line,
			N_TR("Failed to create a Vulkan descriptor layout."));
		accel_vulkan_destroy_kernel(backend, result);
		return false;
	}

	memset(&layout_info, 0, sizeof(layout_info));
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &result->descriptor_layout;
	vk_result = backend->api.create_pipeline_layout(
		backend->device,
		&layout_info,
		NULL,
		&result->pipeline_layout);
	if (vk_result != VK_SUCCESS) {
		hir_error(
			program->kernel[kernel_index].source_line,
			N_TR("Failed to create a Vulkan pipeline layout."));
		accel_vulkan_destroy_kernel(backend, result);
		return false;
	}

	memset(&stage_info, 0, sizeof(stage_info));
	stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage_info.module = result->shader_module;
	stage_info.pName = "main";

	memset(&pipeline_info, 0, sizeof(pipeline_info));
	pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_info.stage = stage_info;
	pipeline_info.layout = result->pipeline_layout;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;
	vk_result = backend->api.create_compute_pipelines(
		backend->device,
		VK_NULL_HANDLE,
		1,
		&pipeline_info,
		NULL,
		&result->pipeline);
	if (vk_result != VK_SUCCESS) {
		hir_error(
			program->kernel[kernel_index].source_line,
			N_TR("Failed to create a Vulkan compute pipeline."));
		accel_vulkan_destroy_kernel(backend, result);
		return false;
	}

	result->uses_f32 = accel_vulkan_program_uses_f32(program);

	return true;
}

/* Destroy one partially or fully prepared kernel in reverse order. */
static void
accel_vulkan_destroy_kernel(
	struct accel_vulkan_backend *backend,
	struct accel_vulkan_kernel *kernel)
{
	if (kernel->pipeline != VK_NULL_HANDLE) {
		backend->api.destroy_pipeline(
			backend->device,
			kernel->pipeline,
			NULL);
	}
	if (kernel->pipeline_layout != VK_NULL_HANDLE) {
		backend->api.destroy_pipeline_layout(
			backend->device,
			kernel->pipeline_layout,
			NULL);
	}
	if (kernel->descriptor_layout != VK_NULL_HANDLE) {
		backend->api.destroy_descriptor_set_layout(
			backend->device,
			kernel->descriptor_layout,
			NULL);
	}
	if (kernel->shader_module != VK_NULL_HANDLE) {
		backend->api.destroy_shader_module(
			backend->device,
			kernel->shader_module,
			NULL);
	}

	memset(kernel, 0, sizeof(*kernel));
}

/* Destroy one backend-prepared program and clear its opaque slot. */
static void
accel_vulkan_destroy_prepared_program(
	void *backend_state,
	struct accel_prepared_program *program)
{
	struct accel_vulkan_backend *backend;
	struct accel_vulkan_prepared *prepared;
	uint32_t i;

	if (program == NULL)
		return;
	if (program->payload == NULL)
		return;

	backend = backend_state;
	prepared = program->payload;

	/* Release every immutable pipeline before its program metadata. */
	for (i = 0; i < prepared->kernel_count; i++)
		accel_vulkan_destroy_kernel(backend, &prepared->kernel[i]);

	accel_program_destroy(prepared->program);
	noct_free(prepared->kernel);
	noct_free(prepared);
	program->payload = NULL;
}

/* Register the selected backend's private runtime protocol. */
static bool
accel_vulkan_register_runtime(
	struct accel_context *context,
	struct rt_env *env)
{
	struct rt_func *begin;
	struct rt_func *dispatch;
	struct rt_func *finish;
	struct rt_value package;
	struct rt_value value;
	bool pinned;
	bool success;

	UNUSED_PARAMETER(context);

	memset(&package, 0, sizeof(package));
	memset(&value, 0, sizeof(value));
	pinned = false;
	success = false;

	if (!rt_pin_local(env, &package))
		return false;
	pinned = true;

	if (!rt_make_empty_dict(env, &package))
		goto cleanup;

	if (!rt_register_cfunc(
		env,
		"__Accel.begin",
		2,
		accel_vulkan_begin_parameter,
		accel_vulkan_begin,
		&begin)) {
		goto cleanup;
	}
	if (!rt_register_cfunc(
		env,
		"__Accel.dispatch",
		2,
		accel_vulkan_dispatch_parameter,
		accel_vulkan_dispatch,
		&dispatch)) {
		goto cleanup;
	}
	if (!rt_register_cfunc(
		env,
		"__Accel.finish",
		2,
		accel_vulkan_finish_parameter,
		accel_vulkan_finish,
		&finish)) {
		goto cleanup;
	}

	value.type = NOCT_VALUE_FUNC;
	value.val.func = begin;
	if (!rt_set_dict_elem_cstr(env, &package, "begin", &value))
		goto cleanup;

	value.val.func = dispatch;
	if (!rt_set_dict_elem_cstr(env, &package, "dispatch", &value))
		goto cleanup;

	value.val.func = finish;
	if (!rt_set_dict_elem_cstr(env, &package, "finish", &value))
		goto cleanup;

	if (!om_freeze_dict(env, &package))
		goto cleanup;
	if (!rt_set_global(env, "__Accel", &package))
		goto cleanup;
	if (!rt_mark_global_const(env, "__Accel"))
		goto cleanup;

	success = true;

cleanup:
	if (pinned) {
		if (!rt_unpin_local(env, &package))
			success = false;
	}

	return success;
}

/* Destroy the selected device and instance after all sessions and programs. */
static void
accel_vulkan_destroy_backend_state(
	void *backend_state)
{
	struct accel_vulkan_backend *backend;
	VkResult result;

	backend = backend_state;
	if (backend == NULL)
		return;

	if (backend->device != VK_NULL_HANDLE) {
		if (backend->queue_mutex.initialized) {
			accel_mutex_lock(&backend->queue_mutex);
			result = backend->api.device_wait_idle(backend->device);
			if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) {
				accel_mutex_unlock(&backend->queue_mutex);
				abort();
			}
			accel_mutex_unlock(&backend->queue_mutex);
		}

		accel_mutex_destroy(&backend->queue_mutex);
		backend->api.destroy_device(backend->device, NULL);
		backend->device = VK_NULL_HANDLE;
	}
	if (backend->instance != VK_NULL_HANDLE) {
		backend->api.destroy_instance(backend->instance, NULL);
		backend->instance = VK_NULL_HANDLE;
	}

	noct_free(backend);
}

/* Create and return one recording session for a published program. */
static bool
accel_vulkan_begin(
	struct rt_env *env)
{
	struct accel_context *context;
	const struct accel_prepared_program *entry;
	struct accel_vulkan_session *session;
	struct rt_value program_value;
	struct rt_value args;
	struct rt_value element;
	struct rt_value returned;
	size_t program_id;
	uint32_t pin_count;
	bool installed;
	bool linked;
	bool success;

	memset(&program_value, 0, sizeof(program_value));
	memset(&args, 0, sizeof(args));
	memset(&element, 0, sizeof(element));
	memset(&returned, 0, sizeof(returned));
	session = NULL;
	pin_count = 0;
	installed = false;
	linked = false;
	success = false;

	if (!accel_vulkan_pin(env, &program_value, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &args, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &element, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &returned, &pin_count))
		goto cleanup;

	if (!noct_get_arg_check_int_long(
		env,
		0,
		&program_value,
		&program_id)) {
		goto cleanup;
	}
	if (program_id == 0 || program_id > UINT32_MAX) {
		rt_error(env, N_TR("Invalid Vulkan accelerator program ID."));
		goto cleanup;
	}
	if (!noct_get_arg_check_array(env, 1, &args))
		goto cleanup;

	context = accel_vulkan_current_context(env);
	if (context == NULL) {
		rt_error(env, N_TR("Vulkan accelerator context is unavailable."));
		goto cleanup;
	}

	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (!accel_context_is_attached_locked(context)) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Vulkan accelerator context is detached."));
		goto cleanup;
	}

	entry = accel_context_lookup_program_locked(
		context,
		(uint32_t)program_id);
	if (entry == NULL || entry->payload == NULL) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Vulkan accelerator program is not published."));
		goto cleanup;
	}
	accel_context_state_unlock(context);
	noct_leave_blocking(env);

	session = noct_calloc(1, sizeof(*session));
	if (session == NULL) {
		rt_out_of_memory(env);
		goto cleanup;
	}

	session->magic = ACCEL_VULKAN_SESSION_MAGIC;
	session->vm = env->vm;
	session->context = context;
	session->state = ACCEL_SESSION_RECORDING;
	session->prepared = entry->payload;
	session->program_id = (uint32_t)program_id;
	session->buffer_count = session->prepared->program->buffer_count;
	session->live.orphan_locked = accel_vulkan_session_orphan_locked;

	if (!accel_vulkan_begin_metadata(
		env,
		&args,
		&element,
		session)) {
		goto cleanup;
	}

	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (!accel_context_is_attached_locked(context) ||
	    accel_context_lookup_program_locked(
		context,
		(uint32_t)program_id) != entry) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Vulkan accelerator program changed during begin."));
		goto cleanup;
	}
	if (!accel_vulkan_begin_resources(env, session)) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Failed to create Vulkan accelerator session resources."));
		goto cleanup;
	}
	accel_context_link_session_locked(context, &session->live);
	linked = true;
	accel_context_state_unlock(context);
	noct_leave_blocking(env);

	if (!rt_make_empty_dict(env, &returned))
		goto cleanup;
	if (!rt_set_dict_native_pointer(
		env,
		&returned,
		session,
		accel_vulkan_session_finalizer)) {
		goto cleanup;
	}
	installed = true;

	if (!noct_set_return(env, &returned)) {
		if (rt_set_dict_native_pointer(env, &returned, NULL, NULL))
			installed = false;
		goto cleanup;
	}

	success = true;

cleanup:
	if (!installed && session != NULL) {
		if (linked) {
			noct_enter_blocking(env);
			accel_context_state_lock(context);
			if (session->live.linked)
				accel_context_unlink_session_locked(context, &session->live);
			accel_vulkan_session_orphan_locked(&session->live);
			accel_context_state_unlock(context);
			noct_leave_blocking(env);
		} else if (context != NULL) {
			noct_enter_blocking(env);
			accel_context_state_lock(context);
			accel_vulkan_session_close_locked(session);
			accel_context_state_unlock(context);
			noct_leave_blocking(env);
		}
		accel_vulkan_session_destroy(session);
	}

	if (pin_count >= 4) {
		if (!accel_vulkan_unpin(env, &returned, &pin_count))
			success = false;
	}
	if (pin_count >= 3) {
		if (!accel_vulkan_unpin(env, &element, &pin_count))
			success = false;
	}
	if (pin_count >= 2) {
		if (!accel_vulkan_unpin(env, &args, &pin_count))
			success = false;
	}
	if (pin_count >= 1) {
		if (!accel_vulkan_unpin(env, &program_value, &pin_count))
			success = false;
	}

	return success;
}

/* Record one in-order kernel dispatch without submitting the session. */
static bool
accel_vulkan_dispatch(
	struct rt_env *env)
{
	struct accel_context *context;
	struct accel_vulkan_backend *backend;
	struct accel_vulkan_session *session;
	struct accel_vulkan_kernel *kernel;
	struct rt_value session_value;
	struct rt_value kernel_value;
	VkMemoryBarrier barrier;
	size_t kernel_index;
	uint32_t group_count;
	uint32_t pin_count;
	bool success;

	memset(&session_value, 0, sizeof(session_value));
	memset(&kernel_value, 0, sizeof(kernel_value));
	session = NULL;
	pin_count = 0;
	success = false;

	if (!accel_vulkan_pin(env, &session_value, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &kernel_value, &pin_count))
		goto cleanup;
	if (!accel_vulkan_get_session_argument(
		env,
		&session_value,
		&session)) {
		goto cleanup;
	}
	if (!noct_get_arg_check_int_long(
		env,
		1,
		&kernel_value,
		&kernel_index)) {
		goto cleanup;
	}
	if (kernel_index > UINT32_MAX) {
		rt_error(env, N_TR("Invalid Vulkan accelerator kernel index."));
		goto cleanup;
	}

	context = accel_vulkan_current_context(env);
	if (context == NULL) {
		rt_error(env, N_TR("Vulkan accelerator context is unavailable."));
		goto cleanup;
	}

	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (session->magic != ACCEL_VULKAN_SESSION_MAGIC ||
	    session->vm != env->vm ||
	    session->context != context ||
	    !session->live.linked ||
	    session->state != ACCEL_SESSION_RECORDING ||
	    session->next_kernel != (uint32_t)kernel_index ||
	    kernel_index >= session->prepared->kernel_count) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Invalid or out-of-order Vulkan accelerator dispatch."));
		goto cleanup;
	}

	if (session->kernel_active[kernel_index]) {
		backend = accel_context_get_backend_state(context);
		kernel = &session->prepared->kernel[kernel_index];
		if (session->has_commands) {
			memset(&barrier, 0, sizeof(barrier));
			barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask =
				VK_ACCESS_SHADER_READ_BIT |
				VK_ACCESS_SHADER_WRITE_BIT;
			backend->api.cmd_pipeline_barrier(
				session->command_buffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				1,
				&barrier,
				0,
				NULL,
				0,
				NULL);
		}

		backend->api.cmd_bind_pipeline(
			session->command_buffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			kernel->pipeline);
		backend->api.cmd_bind_descriptor_sets(
			session->command_buffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			kernel->pipeline_layout,
			0,
			1,
			&session->descriptor_set[kernel_index],
			0,
			NULL);

		group_count = (session->kernel_trip[kernel_index] +
			ACCEL_VULKAN_WORKGROUP_SIZE - 1) /
			ACCEL_VULKAN_WORKGROUP_SIZE;
		backend->api.cmd_dispatch(
			session->command_buffer,
			group_count,
			1,
			1);
		session->has_commands = true;
	}

	session->next_kernel++;
	accel_context_state_unlock(context);
	noct_leave_blocking(env);
	success = true;

cleanup:
	if (pin_count >= 2) {
		if (!accel_vulkan_unpin(env, &kernel_value, &pin_count))
			success = false;
	}
	if (pin_count >= 1) {
		if (!accel_vulkan_unpin(env, &session_value, &pin_count))
			success = false;
	}

	return success;
}

/* Submit one complete session, synchronize host outputs, and close resources. */
static bool
accel_vulkan_finish(
	struct rt_env *env)
{
	struct accel_context *context;
	struct accel_vulkan_backend *backend;
	struct accel_vulkan_session *session;
	struct rt_value session_value;
	struct rt_value args;
	struct rt_value element;
	struct rt_value publication;
	VkMemoryBarrier barrier;
	VkSubmitInfo submit_info;
	VkResult result;
	uint32_t pin_count;
	bool submitted;
	bool success;

	memset(&session_value, 0, sizeof(session_value));
	memset(&args, 0, sizeof(args));
	memset(&element, 0, sizeof(element));
	memset(&publication, 0, sizeof(publication));
	session = NULL;
	pin_count = 0;
	submitted = false;
	success = false;

	if (!accel_vulkan_pin(env, &session_value, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &args, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &element, &pin_count))
		goto cleanup;
	if (!accel_vulkan_pin(env, &publication, &pin_count))
		goto cleanup;
	if (!accel_vulkan_get_session_argument(
		env,
		&session_value,
		&session)) {
		goto cleanup;
	}
	if (!noct_get_arg_check_array(env, 1, &args))
		goto cleanup;

	context = accel_vulkan_current_context(env);
	if (context == NULL) {
		rt_error(env, N_TR("Vulkan accelerator context is unavailable."));
		goto cleanup;
	}

	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (session->magic != ACCEL_VULKAN_SESSION_MAGIC ||
	    session->vm != env->vm ||
	    session->context != context ||
	    !session->live.linked ||
	    session->state != ACCEL_SESSION_RECORDING ||
	    session->next_kernel != session->prepared->kernel_count) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Invalid or incomplete Vulkan accelerator finish."));
		goto cleanup;
	}

	backend = accel_context_get_backend_state(context);
	session->state = ACCEL_SESSION_SUBMITTING;
	if (session->has_commands) {
		memset(&barrier, 0, sizeof(barrier));
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		backend->api.cmd_pipeline_barrier(
			session->command_buffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_HOST_BIT,
			0,
			1,
			&barrier,
			0,
			NULL,
			0,
			NULL);

		result = backend->api.end_command_buffer(
			session->command_buffer);
		if (result != VK_SUCCESS) {
			accel_vulkan_fail_session_locked(
				env,
				session,
				N_TR("Failed to finish a Vulkan command buffer."));
			accel_context_state_unlock(context);
			noct_leave_blocking(env);
			rt_error(env, N_TR("Failed to finish a Vulkan command buffer."));
			goto cleanup;
		}
	}
	accel_context_state_unlock(context);

	if (session->has_commands) {
		memset(&submit_info, 0, sizeof(submit_info));
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &session->command_buffer;

		accel_mutex_lock(&backend->queue_mutex);
		result = backend->api.queue_submit(
			backend->queue,
			1,
			&submit_info,
			session->fence);
		if (result == VK_SUCCESS) {
			submitted = true;
			result = backend->api.wait_for_fences(
				backend->device,
				1,
				&session->fence,
				VK_TRUE,
				UINT64_MAX);
		}
		if (result != VK_SUCCESS && submitted)
			(void)backend->api.device_wait_idle(backend->device);
		accel_mutex_unlock(&backend->queue_mutex);
		if (result != VK_SUCCESS) {
			accel_context_state_lock(context);
			accel_vulkan_fail_session_locked(
				env,
				session,
				N_TR("Vulkan accelerator submission failed."));
			accel_context_state_unlock(context);
			noct_leave_blocking(env);
			rt_error(env, N_TR("Vulkan accelerator submission failed."));
			goto cleanup;
		}

		if (!accel_vulkan_invalidate_session(env, session)) {
			accel_context_state_lock(context);
			accel_vulkan_fail_session_locked(
				env,
				session,
				N_TR("Failed to invalidate Vulkan output memory."));
			accel_context_state_unlock(context);
			noct_leave_blocking(env);
			rt_error(env, N_TR("Failed to invalidate Vulkan output memory."));
			goto cleanup;
		}
	}

	accel_context_state_lock(context);
	session->state = ACCEL_SESSION_COPY_READY;
	accel_context_state_unlock(context);
	noct_leave_blocking(env);

	if (!accel_vulkan_copy_results(
		env,
		&args,
		&element,
		session)) {
		goto terminal_failure;
	}

	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (session->context != context ||
	    session->state != ACCEL_SESSION_COPY_READY ||
	    !session->live.linked) {
		accel_context_state_unlock(context);
		noct_leave_blocking(env);
		rt_error(env, N_TR("Vulkan accelerator session changed during finish."));
		goto cleanup;
	}

	accel_context_unlink_session_locked(context, &session->live);
	accel_vulkan_session_close_locked(session);
	session->state = ACCEL_SESSION_FINISHED;
	session->context = NULL;
	accel_context_state_unlock(context);
	noct_leave_blocking(env);
	success = true;
	goto cleanup;

terminal_failure:
	noct_enter_blocking(env);
	accel_context_state_lock(context);
	if (session->context == context && session->live.linked) {
		accel_context_unlink_session_locked(context, &session->live);
		accel_vulkan_session_close_locked(session);
		session->state = ACCEL_SESSION_FAILED;
		session->context = NULL;
	}
	accel_context_state_unlock(context);
	noct_leave_blocking(env);

cleanup:
	if (pin_count >= 4) {
		if (!accel_vulkan_unpin(env, &publication, &pin_count))
			success = false;
	}
	if (pin_count >= 3) {
		if (!accel_vulkan_unpin(env, &element, &pin_count))
			success = false;
	}
	if (pin_count >= 2) {
		if (!accel_vulkan_unpin(env, &args, &pin_count))
			success = false;
	}
	if (pin_count >= 1) {
		if (!accel_vulkan_unpin(env, &session_value, &pin_count))
			success = false;
	}

	return success;
}

/* Validate all Noct arguments and snapshot host inputs before blocking. */
static bool
accel_vulkan_begin_metadata(
	struct rt_env *env,
	struct rt_value *args,
	struct rt_value *element,
	struct accel_vulkan_session *session)
{
	size_t args_size;

	if (!noct_get_array_size(env, args, &args_size))
		return false;
	if (!accel_vulkan_allocate_session_metadata(env, session))
		return false;
	if (!accel_vulkan_read_scalars(
		env,
		args,
		element,
		session,
		args_size)) {
		return false;
	}
	if (!accel_vulkan_evaluate_kernels(env, session))
		return false;
	if (!accel_vulkan_read_buffers(
		env,
		args,
		element,
		session,
		args_size)) {
		return false;
	}

	return true;
}

/* Allocate fixed-size plain-C metadata owned by one session. */
static bool
accel_vulkan_allocate_session_metadata(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	uint32_t scalar_word_count;

	program = session->prepared->program;
	scalar_word_count = program->scalar_count + program->kernel_count * 2;

	if (program->scalar_count != 0) {
		session->scalar_value = noct_calloc(
			program->scalar_count,
			sizeof(*session->scalar_value));
		if (session->scalar_value == NULL)
			goto out_of_memory;
	}

	session->scalar_word = noct_calloc(
		scalar_word_count,
		sizeof(*session->scalar_word));
	if (session->scalar_word == NULL)
		goto out_of_memory;

	session->kernel_start = noct_calloc(
		program->kernel_count,
		sizeof(*session->kernel_start));
	if (session->kernel_start == NULL)
		goto out_of_memory;

	session->kernel_trip = noct_calloc(
		program->kernel_count,
		sizeof(*session->kernel_trip));
	if (session->kernel_trip == NULL)
		goto out_of_memory;

	session->kernel_active = noct_calloc(
		program->kernel_count,
		sizeof(*session->kernel_active));
	if (session->kernel_active == NULL)
		goto out_of_memory;

	if (program->buffer_count != 0) {
		session->buffer = noct_calloc(
			program->buffer_count,
			sizeof(*session->buffer));
		if (session->buffer == NULL)
			goto out_of_memory;

		session->snapshot = noct_calloc(
			program->buffer_count,
			sizeof(*session->snapshot));
		if (session->snapshot == NULL)
			goto out_of_memory;

		session->element_count = noct_calloc(
			program->buffer_count,
			sizeof(*session->element_count));
		if (session->element_count == NULL)
			goto out_of_memory;

		session->packed_type = noct_calloc(
			program->buffer_count,
			sizeof(*session->packed_type));
		if (session->packed_type == NULL)
			goto out_of_memory;
	}

	return true;

out_of_memory:
	rt_out_of_memory(env);

	return false;
}

/* Read every scalar argument into checked size values and raw shader words. */
static bool
accel_vulkan_read_scalars(
	struct rt_env *env,
	struct rt_value *args,
	struct rt_value *element,
	struct accel_vulkan_session *session,
	size_t args_size)
{
	const struct accel_program *program;
	const struct accel_scalar_binding *binding;
	float float_value;
	int int_value;
	uint32_t raw;
	uint32_t i;

	program = session->prepared->program;

	/* Copy each immutable source scalar in argument order. */
	for (i = 0; i < program->scalar_count; i++) {
		binding = &program->scalar[i];
		if (binding->args_slot >= args_size) {
			rt_error(env, N_TR("Vulkan accelerator scalar argument is missing."));
			return false;
		}
		if (!noct_get_array_elem(
			env,
			args,
			binding->args_slot,
			element)) {
			return false;
		}

		if (binding->value_type == ACCEL_IR_I32) {
			if (!noct_get_int(env, element, &int_value))
				return false;
			session->scalar_value[i] = int_value;
			session->scalar_word[i] = (uint32_t)(int32_t)int_value;
		} else if (binding->value_type == ACCEL_IR_F32) {
			if (!noct_get_float(env, element, &float_value))
				return false;
			memcpy(&raw, &float_value, sizeof(raw));
			session->scalar_value[i] = 0;
			session->scalar_word[i] = raw;
		} else {
			rt_error(env, N_TR("Invalid Vulkan accelerator scalar type."));
			return false;
		}
	}

	return true;
}

/* Evaluate every dynamic start and trip count before creating resources. */
static bool
accel_vulkan_evaluate_kernels(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	const struct accel_kernel_plan *kernel;
	struct accel_vulkan_backend *backend;
	int64_t start;
	int64_t trip;
	uint64_t group_count;
	uint32_t word_index;
	uint32_t i;

	program = session->prepared->program;
	backend = accel_context_get_backend_state(session->context);

	/* Resolve all dispatch dimensions before making any Vulkan object. */
	for (i = 0; i < program->kernel_count; i++) {
		kernel = &program->kernel[i];
		if (!accel_program_evaluate_size(
			program,
			kernel->start_expression,
			program->scalar_count,
			session->scalar_value,
			&start)) {
			rt_error(env, N_TR("Vulkan accelerator loop start overflowed."));
			return false;
		}
		if (!accel_program_evaluate_size(
			program,
			kernel->trip_expression,
			program->scalar_count,
			session->scalar_value,
			&trip)) {
			rt_error(env, N_TR("Vulkan accelerator trip count overflowed."));
			return false;
		}
		if (start < 0 || start > INT32_MAX || trip < 0 || trip > UINT32_MAX) {
			rt_error(env, N_TR("Vulkan accelerator dispatch range is too large."));
			return false;
		}

		group_count = ((uint64_t)trip +
			ACCEL_VULKAN_WORKGROUP_SIZE - 1) /
			ACCEL_VULKAN_WORKGROUP_SIZE;
		if (group_count >
		    backend->properties.limits.maxComputeWorkGroupCount[0]) {
			rt_error(env, N_TR("Vulkan accelerator dispatch count exceeds the device limit."));
			return false;
		}

		session->kernel_start[i] = (uint32_t)start;
		session->kernel_trip[i] = (uint32_t)trip;
		session->kernel_active[i] = trip != 0;
		word_index = program->scalar_count + i * 2;
		session->scalar_word[word_index] = (uint32_t)start;
		session->scalar_word[word_index + 1] = (uint32_t)trip;
	}

	return true;
}

/* Validate every Packed argument and snapshot each required host upload. */
static bool
accel_vulkan_read_buffers(
	struct rt_env *env,
	struct rt_value *args,
	struct rt_value *element,
	struct accel_vulkan_session *session,
	size_t args_size)
{
	const struct accel_program *program;
	const struct accel_buffer_binding *binding;
	void *pointer;
	size_t element_count;
	size_t byte_count;
	int packed_type;
	uint32_t i;

	program = session->prepared->program;

	/* Check each buffer descriptor even when all of its kernels are empty. */
	for (i = 0; i < program->buffer_count; i++) {
		binding = &program->buffer[i];
		if (binding->args_slot >= args_size) {
			rt_error(env, N_TR("Vulkan accelerator Packed argument is missing."));
			return false;
		}
		if (!noct_get_array_elem(
			env,
			args,
			binding->args_slot,
			element)) {
			return false;
		}
		if (!noct_get_packed_type(env, element, &packed_type))
			return false;
		if (packed_type != binding->element_kind) {
			rt_error(env, N_TR("Vulkan accelerator Packed element type does not match."));
			return false;
		}
		if (!noct_get_packed_size(env, element, &element_count))
			return false;

		session->element_count[i] = element_count;
		session->packed_type[i] = packed_type;
		if (!accel_vulkan_buffer_plan(
			env,
			session,
			i,
			element_count)) {
			return false;
		}
		if (!session->buffer[i].upload)
			continue;

		if (element_count > (size_t)-1 / binding->element_width) {
			rt_error(env, N_TR("Vulkan accelerator buffer size overflowed."));
			return false;
		}
		byte_count = element_count * binding->element_width;
		if (byte_count == 0)
			continue;

		session->snapshot[i] = noct_malloc(byte_count);
		if (session->snapshot[i] == NULL) {
			rt_out_of_memory(env);
			return false;
		}
		if (!noct_get_packed_pointer(env, element, &pointer))
			return false;
		memcpy(session->snapshot[i], pointer, byte_count);
	}

	return true;
}

/* Fold active effects and validate one buffer's runtime access range. */
static bool
accel_vulkan_buffer_plan(
	struct rt_env *env,
	struct accel_vulkan_session *session,
	uint32_t buffer_index,
	size_t element_count)
{
	const struct accel_program *program;
	const struct accel_buffer_binding *binding;
	const struct accel_buffer_effect *effect;
	int64_t first;
	int64_t end;
	uint32_t i;
	bool device_defined;
	bool any_use;

	program = session->prepared->program;
	binding = &program->buffer[buffer_index];
	device_defined = false;
	any_use = false;

	/* Fold only nonempty kernels in source order. */
	for (i = 0; i < program->kernel_count; i++) {
		if (!session->kernel_active[i])
			continue;

		effect = &binding->effect[i];
		if (!effect->read && !effect->write)
			continue;
		any_use = true;
		if (effect->read && !device_defined)
			session->buffer[buffer_index].upload = true;
		if (effect->read_before_write && !device_defined)
			session->buffer[buffer_index].upload = true;
		if (effect->write) {
			if (!effect->full_overwrite && !device_defined)
				session->buffer[buffer_index].upload = true;
			session->buffer[buffer_index].download = true;
			device_defined = true;
		}

		if (!accel_program_evaluate_size(
			program,
			binding->kernel_required_first_expression[i],
			program->scalar_count,
			session->scalar_value,
			&first)) {
			rt_error(env, N_TR("Vulkan accelerator buffer range overflowed."));
			return false;
		}
		if (!accel_program_evaluate_size(
			program,
			binding->kernel_required_end_expression[i],
			program->scalar_count,
			session->scalar_value,
			&end)) {
			rt_error(env, N_TR("Vulkan accelerator buffer range overflowed."));
			return false;
		}
		if (first < 0 || end < first || (uint64_t)end > element_count) {
			rt_error(env, N_TR("Vulkan accelerator buffer access is out of range."));
			return false;
		}
	}

	session->buffer[buffer_index].active = any_use;
	if (!any_use)
		return true;

	/* Preserve host contents conservatively until full-extent proof is local. */
	session->buffer[buffer_index].upload = true;

	return true;
}

/* Create mapped storage, descriptors, and one recording command buffer. */
static bool
accel_vulkan_begin_resources(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	struct accel_vulkan_backend *backend;
	VkDeviceSize byte_count;
	uint32_t scalar_word_count;
	uint32_t i;
	bool any_active;

	UNUSED_PARAMETER(env);

	program = session->prepared->program;
	backend = accel_context_get_backend_state(session->context);
	any_active = false;

	/* Determine whether this session needs any device resource at all. */
	for (i = 0; i < program->kernel_count; i++) {
		if (session->kernel_active[i]) {
			any_active = true;
			break;
		}
	}
	if (!any_active)
		return true;

	/* Allocate one mapped storage buffer for each active Packed binding. */
	for (i = 0; i < program->buffer_count; i++) {
		if (!session->buffer[i].active)
			continue;

		if (session->element_count[i] >
		    UINT64_MAX / program->buffer[i].element_width) {
			return false;
		}
		byte_count = (VkDeviceSize)session->element_count[i] *
			program->buffer[i].element_width;
		if (byte_count == 0)
			return false;
		if (byte_count > backend->properties.limits.maxStorageBufferRange)
			return false;

		if (!accel_vulkan_create_host_buffer(
			env,
			backend,
			byte_count,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			&session->buffer[i])) {
			return false;
		}
		if (session->buffer[i].upload) {
			memcpy(
				session->buffer[i].mapped,
				session->snapshot[i],
				(size_t)byte_count);
		}
	}

	scalar_word_count = program->scalar_count + program->kernel_count * 2;
	byte_count = (VkDeviceSize)scalar_word_count * sizeof(uint32_t);
	if (byte_count > backend->properties.limits.maxStorageBufferRange)
		return false;
	if (!accel_vulkan_create_host_buffer(
		env,
		backend,
		byte_count,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		&session->scalar_buffer)) {
		return false;
	}
	session->scalar_buffer.active = true;
	session->scalar_buffer.upload = true;
	memcpy(
		session->scalar_buffer.mapped,
		session->scalar_word,
		(size_t)byte_count);

	if (!accel_vulkan_flush_session(env, session))
		return false;
	if (!accel_vulkan_create_descriptors(env, session))
		return false;
	if (!accel_vulkan_record_begin(env, session))
		return false;

	/* Snapshots are dead once their bytes reside in mapped storage. */
	for (i = 0; i < program->buffer_count; i++) {
		noct_free(session->snapshot[i]);
		session->snapshot[i] = NULL;
	}

	return true;
}

/* Create and map one host-visible storage buffer. */
static bool
accel_vulkan_create_host_buffer(
	struct rt_env *env,
	struct accel_vulkan_backend *backend,
	VkDeviceSize logical_size,
	VkBufferUsageFlags usage,
	struct accel_vulkan_buffer *buffer)
{
	VkBufferCreateInfo buffer_info;
	VkMemoryRequirements requirements;
	VkMemoryAllocateInfo allocate_info;
	VkDeviceSize allocation_size;
	VkDeviceSize atom;
	VkResult result;
	uint32_t memory_type;
	bool coherent;

	UNUSED_PARAMETER(env);

	if (logical_size == 0)
		return false;

	memset(&buffer_info, 0, sizeof(buffer_info));
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = logical_size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	result = backend->api.create_buffer(
		backend->device,
		&buffer_info,
		NULL,
		&buffer->buffer);
	if (result != VK_SUCCESS)
		return false;

	memset(&requirements, 0, sizeof(requirements));
	backend->api.get_buffer_memory_requirements(
		backend->device,
		buffer->buffer,
		&requirements);
	if (!accel_vulkan_find_host_memory(
		backend,
		requirements.memoryTypeBits,
		&memory_type,
		&coherent)) {
		return false;
	}

	allocation_size = requirements.size;
	if (allocation_size < logical_size)
		allocation_size = logical_size;
	atom = backend->properties.limits.nonCoherentAtomSize;
	if (!coherent) {
		if (!accel_vulkan_round_allocation(
			env,
			allocation_size,
			atom,
			&allocation_size)) {
			return false;
		}
	}
	if (allocation_size >
	    backend->memory_properties.memoryHeaps[
		backend->memory_properties.memoryTypes[memory_type].heapIndex].size) {
		return false;
	}

	memset(&allocate_info, 0, sizeof(allocate_info));
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = allocation_size;
	allocate_info.memoryTypeIndex = memory_type;
	result = backend->api.allocate_memory(
		backend->device,
		&allocate_info,
		NULL,
		&buffer->memory);
	if (result != VK_SUCCESS)
		return false;

	result = backend->api.bind_buffer_memory(
		backend->device,
		buffer->buffer,
		buffer->memory,
		0);
	if (result != VK_SUCCESS)
		return false;

	result = backend->api.map_memory(
		backend->device,
		buffer->memory,
		0,
		allocation_size,
		0,
		&buffer->mapped);
	if (result != VK_SUCCESS)
		return false;

	buffer->logical_size = logical_size;
	buffer->allocation_size = allocation_size;
	buffer->memory_type = memory_type;
	buffer->coherent = coherent;
	buffer->active = true;

	return true;
}

/* Select a host-visible memory type, preferring coherent memory. */
static bool
accel_vulkan_find_host_memory(
	const struct accel_vulkan_backend *backend,
	uint32_t memory_type_bits,
	uint32_t *memory_type,
	bool *coherent)
{
	VkMemoryPropertyFlags flags;
	uint32_t fallback;
	uint32_t i;

	fallback = UINT32_MAX;

	/* Prefer a directly coherent mapping among compatible memory types. */
	for (i = 0; i < backend->memory_properties.memoryTypeCount; i++) {
		if ((memory_type_bits & (1U << i)) == 0)
			continue;

		flags = backend->memory_properties.memoryTypes[i].propertyFlags;
		if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
			continue;
		if ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
			*memory_type = i;
			*coherent = true;
			return true;
		}
		if (fallback == UINT32_MAX)
			fallback = i;
	}

	if (fallback == UINT32_MAX)
		return false;

	*memory_type = fallback;
	*coherent = false;

	return true;
}

/* Round a non-coherent mapped allocation to the device atom size. */
static bool
accel_vulkan_round_allocation(
	struct rt_env *env,
	VkDeviceSize value,
	VkDeviceSize atom,
	VkDeviceSize *result)
{
	VkDeviceSize remainder;

	UNUSED_PARAMETER(env);

	if (atom == 0)
		return false;

	remainder = value % atom;
	if (remainder == 0) {
		*result = value;
		return true;
	}
	if (value > UINT64_MAX - (atom - remainder))
		return false;

	*result = value + atom - remainder;

	return true;
}

/* Destroy one mapped buffer and its dedicated allocation. */
static void
accel_vulkan_destroy_buffer(
	struct accel_vulkan_backend *backend,
	struct accel_vulkan_buffer *buffer)
{
	if (buffer->mapped != NULL && buffer->memory != VK_NULL_HANDLE)
		backend->api.unmap_memory(backend->device, buffer->memory);
	if (buffer->memory != VK_NULL_HANDLE)
		backend->api.free_memory(backend->device, buffer->memory, NULL);
	if (buffer->buffer != VK_NULL_HANDLE)
		backend->api.destroy_buffer(backend->device, buffer->buffer, NULL);

	memset(buffer, 0, sizeof(*buffer));
}

/* Allocate and populate one descriptor set for each program kernel. */
static bool
accel_vulkan_create_descriptors(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	struct accel_vulkan_backend *backend;
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_info;
	VkDescriptorSetLayout *layout;
	VkDescriptorSetAllocateInfo allocate_info;
	VkDescriptorBufferInfo *buffer_info;
	VkWriteDescriptorSet *write;
	VkResult result;
	uint32_t write_count;
	uint32_t i;
	uint32_t j;

	UNUSED_PARAMETER(env);

	program = session->prepared->program;
	backend = accel_context_get_backend_state(session->context);

	memset(&pool_size, 0, sizeof(pool_size));
	pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_size.descriptorCount =
		(program->buffer_count + 1) * program->kernel_count;

	memset(&pool_info, 0, sizeof(pool_info));
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.maxSets = program->kernel_count;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	result = backend->api.create_descriptor_pool(
		backend->device,
		&pool_info,
		NULL,
		&session->descriptor_pool);
	if (result != VK_SUCCESS)
		return false;

	layout = noct_malloc(sizeof(*layout) * program->kernel_count);
	if (layout == NULL)
		return false;

	/* Borrow each immutable per-kernel descriptor layout. */
	for (i = 0; i < program->kernel_count; i++)
		layout[i] = session->prepared->kernel[i].descriptor_layout;

	session->descriptor_set = noct_calloc(
		program->kernel_count,
		sizeof(*session->descriptor_set));
	if (session->descriptor_set == NULL) {
		noct_free(layout);
		return false;
	}

	memset(&allocate_info, 0, sizeof(allocate_info));
	allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocate_info.descriptorPool = session->descriptor_pool;
	allocate_info.descriptorSetCount = program->kernel_count;
	allocate_info.pSetLayouts = layout;
	result = backend->api.allocate_descriptor_sets(
		backend->device,
		&allocate_info,
		session->descriptor_set);
	noct_free(layout);
	if (result != VK_SUCCESS)
		return false;

	buffer_info = noct_calloc(
		program->buffer_count + 1,
		sizeof(*buffer_info));
	if (buffer_info == NULL)
		return false;

	write = noct_calloc(
		program->buffer_count + 1,
		sizeof(*write));
	if (write == NULL) {
		noct_free(buffer_info);
		return false;
	}

	/* Populate only bindings statically used by each active kernel. */
	for (i = 0; i < program->kernel_count; i++) {
		if (!session->kernel_active[i])
			continue;

		write_count = 0;
		for (j = 0; j < program->buffer_count; j++) {
			if (!accel_vulkan_kernel_uses_buffer(
				program->kernel[i].ir,
				j)) {
				continue;
			}

			buffer_info[write_count].buffer = session->buffer[j].buffer;
			buffer_info[write_count].offset = 0;
			buffer_info[write_count].range =
				session->buffer[j].logical_size;
			memset(&write[write_count], 0, sizeof(write[write_count]));
			write[write_count].sType =
				VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write[write_count].dstSet = session->descriptor_set[i];
			write[write_count].dstBinding = j;
			write[write_count].descriptorCount = 1;
			write[write_count].descriptorType =
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write[write_count].pBufferInfo = &buffer_info[write_count];
			write_count++;
		}

		buffer_info[write_count].buffer = session->scalar_buffer.buffer;
		buffer_info[write_count].offset = 0;
		buffer_info[write_count].range =
			session->scalar_buffer.logical_size;
		memset(&write[write_count], 0, sizeof(write[write_count]));
		write[write_count].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write[write_count].dstSet = session->descriptor_set[i];
		write[write_count].dstBinding = program->buffer_count;
		write[write_count].descriptorCount = 1;
		write[write_count].descriptorType =
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write[write_count].pBufferInfo = &buffer_info[write_count];
		write_count++;

		backend->api.update_descriptor_sets(
			backend->device,
			write_count,
			write,
			0,
			NULL);
	}

	noct_free(write);
	noct_free(buffer_info);

	return true;
}

/* Report whether one typed kernel statically references a buffer binding. */
static bool
accel_vulkan_kernel_uses_buffer(
	const struct accel_ir_kernel *kernel,
	uint32_t buffer_index)
{
	const struct accel_ir_instruction *instruction;
	uint32_t i;

	/* Inspect every load and store reference. */
	for (i = 0; i < kernel->instruction_count; i++) {
		instruction = &kernel->instruction[i];
		if (instruction->opcode != ACCEL_IR_BUFFER_LOAD &&
		    instruction->opcode != ACCEL_IR_BUFFER_STORE) {
			continue;
		}
		if (instruction->reference == buffer_index)
			return true;
	}

	return false;
}

/* Create and begin the session command buffer and completion fence. */
static bool
accel_vulkan_record_begin(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	struct accel_vulkan_backend *backend;
	VkCommandPoolCreateInfo pool_info;
	VkCommandBufferAllocateInfo allocate_info;
	VkCommandBufferBeginInfo begin_info;
	VkFenceCreateInfo fence_info;
	VkMemoryBarrier barrier;
	VkResult result;

	UNUSED_PARAMETER(env);

	backend = accel_context_get_backend_state(session->context);
	memset(&pool_info, 0, sizeof(pool_info));
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = backend->queue_family;
	result = backend->api.create_command_pool(
		backend->device,
		&pool_info,
		NULL,
		&session->command_pool);
	if (result != VK_SUCCESS)
		return false;

	memset(&allocate_info, 0, sizeof(allocate_info));
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = session->command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;
	result = backend->api.allocate_command_buffers(
		backend->device,
		&allocate_info,
		&session->command_buffer);
	if (result != VK_SUCCESS)
		return false;

	memset(&begin_info, 0, sizeof(begin_info));
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = backend->api.begin_command_buffer(
		session->command_buffer,
		&begin_info);
	if (result != VK_SUCCESS)
		return false;
	session->command_started = true;

	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask =
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT;
	backend->api.cmd_pipeline_barrier(
		session->command_buffer,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		1,
		&barrier,
		0,
		NULL,
		0,
		NULL);

	memset(&fence_info, 0, sizeof(fence_info));
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	result = backend->api.create_fence(
		backend->device,
		&fence_info,
		NULL,
		&session->fence);
	if (result != VK_SUCCESS)
		return false;

	return true;
}

/* Flush every non-coherent host write before command submission. */
static bool
accel_vulkan_flush_session(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	struct accel_vulkan_backend *backend;
	VkMappedMemoryRange range;
	VkResult result;
	uint32_t i;

	UNUSED_PARAMETER(env);

	program = session->prepared->program;
	backend = accel_context_get_backend_state(session->context);

	/* Flush each mapped data buffer that received a host snapshot. */
	for (i = 0; i < program->buffer_count; i++) {
		if (!session->buffer[i].active ||
		    !session->buffer[i].upload ||
		    session->buffer[i].coherent) {
			continue;
		}

		memset(&range, 0, sizeof(range));
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = session->buffer[i].memory;
		range.offset = 0;
		range.size = session->buffer[i].allocation_size;
		result = backend->api.flush_mapped_memory_ranges(
			backend->device,
			1,
			&range);
		if (result != VK_SUCCESS)
			return false;
	}

	if (!session->scalar_buffer.coherent) {
		memset(&range, 0, sizeof(range));
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = session->scalar_buffer.memory;
		range.offset = 0;
		range.size = session->scalar_buffer.allocation_size;
		result = backend->api.flush_mapped_memory_ranges(
			backend->device,
			1,
			&range);
		if (result != VK_SUCCESS)
			return false;
	}

	return true;
}

/* Invalidate every non-coherent output mapping after fence completion. */
static bool
accel_vulkan_invalidate_session(
	struct rt_env *env,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	struct accel_vulkan_backend *backend;
	VkMappedMemoryRange range;
	VkResult result;
	uint32_t i;

	UNUSED_PARAMETER(env);

	program = session->prepared->program;
	backend = accel_context_get_backend_state(session->context);

	/* Invalidate each mapped buffer that will be copied back to Noct. */
	for (i = 0; i < program->buffer_count; i++) {
		if (!session->buffer[i].active ||
		    !session->buffer[i].download ||
		    session->buffer[i].coherent) {
			continue;
		}

		memset(&range, 0, sizeof(range));
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.memory = session->buffer[i].memory;
		range.offset = 0;
		range.size = session->buffer[i].allocation_size;
		result = backend->api.invalidate_mapped_memory_ranges(
			backend->device,
			1,
			&range);
		if (result != VK_SUCCESS)
			return false;
	}

	return true;
}

/* Revalidate Packed arguments and copy every completed output to the host. */
static bool
accel_vulkan_copy_results(
	struct rt_env *env,
	struct rt_value *args,
	struct rt_value *element,
	struct accel_vulkan_session *session)
{
	const struct accel_program *program;
	void *pointer;
	size_t args_size;
	size_t element_count;
	size_t byte_count;
	int packed_type;
	uint32_t i;

	program = session->prepared->program;
	if (!noct_get_array_size(env, args, &args_size))
		return false;

	/* Reacquire each output pointer only for its immediate host copy. */
	for (i = 0; i < program->buffer_count; i++) {
		if (!session->buffer[i].download)
			continue;
		if (program->buffer[i].args_slot >= args_size) {
			rt_error(env, N_TR("Vulkan accelerator output argument is missing."));
			return false;
		}
		if (!noct_get_array_elem(
			env,
			args,
			program->buffer[i].args_slot,
			element)) {
			return false;
		}
		if (!noct_get_packed_type(env, element, &packed_type))
			return false;
		if (packed_type != session->packed_type[i]) {
			rt_error(env, N_TR("Vulkan accelerator output type changed."));
			return false;
		}
		if (!noct_get_packed_size(env, element, &element_count))
			return false;
		if (element_count != session->element_count[i]) {
			rt_error(env, N_TR("Vulkan accelerator output size changed."));
			return false;
		}
		if (!noct_get_packed_pointer(env, element, &pointer))
			return false;

		byte_count = element_count * program->buffer[i].element_width;
		memcpy(pointer, session->buffer[i].mapped, byte_count);
	}

	return true;
}

/* Close and unlink one terminally failed session under the state mutex. */
static bool
accel_vulkan_fail_session_locked(
	struct rt_env *env,
	struct accel_vulkan_session *session,
	const char *message)
{
	UNUSED_PARAMETER(env);
	UNUSED_PARAMETER(message);

	if (session->live.linked)
		accel_context_unlink_session_locked(session->context, &session->live);
	accel_vulkan_session_close_locked(session);
	session->state = ACCEL_SESSION_FAILED;
	session->context = NULL;

	return false;
}

/* Free a detached session wrapper after its native dictionary dies. */
static void
accel_vulkan_session_finalizer(
	void *native_pointer)
{
	struct accel_context *context;
	struct accel_vulkan_session *session;

	session = native_pointer;
	if (session == NULL)
		return;
	if (session->magic != ACCEL_VULKAN_SESSION_MAGIC) {
		noct_free(session);
		return;
	}

	context = session->context;
	if (context != NULL) {
		accel_context_state_lock(context);
		if (session->context == context && session->live.linked) {
			accel_context_unlink_session_locked(context, &session->live);
			accel_vulkan_session_orphan_locked(&session->live);
		}
		accel_context_state_unlock(context);
	}

	accel_vulkan_session_destroy(session);
}

/* Close one context-owned session while the state mutex is held. */
static void
accel_vulkan_session_orphan_locked(
	struct accel_live_session *live)
{
	struct accel_vulkan_session *session;

	session = (struct accel_vulkan_session *)((char *)live -
		offsetof(struct accel_vulkan_session, live));
	accel_vulkan_session_close_locked(session);
	session->state = ACCEL_SESSION_ORPHANED;
	session->context = NULL;
}

/* Release all session Vulkan handles under the context state mutex. */
static void
accel_vulkan_session_close_locked(
	struct accel_vulkan_session *session)
{
	struct accel_vulkan_backend *backend;
	const struct accel_program *program;
	uint32_t i;

	if (session->context == NULL)
		return;

	backend = accel_context_get_backend_state(session->context);
	program = session->prepared->program;

	if (session->fence != VK_NULL_HANDLE) {
		backend->api.destroy_fence(
			backend->device,
			session->fence,
			NULL);
		session->fence = VK_NULL_HANDLE;
	}
	if (session->descriptor_pool != VK_NULL_HANDLE) {
		backend->api.destroy_descriptor_pool(
			backend->device,
			session->descriptor_pool,
			NULL);
		session->descriptor_pool = VK_NULL_HANDLE;
	}
	if (session->command_pool != VK_NULL_HANDLE) {
		backend->api.destroy_command_pool(
			backend->device,
			session->command_pool,
			NULL);
		session->command_pool = VK_NULL_HANDLE;
		session->command_buffer = VK_NULL_HANDLE;
	}

	if (session->buffer != NULL) {
		/* Destroy every dedicated data-buffer allocation. */
		for (i = 0; i < program->buffer_count; i++)
			accel_vulkan_destroy_buffer(backend, &session->buffer[i]);
	}

	accel_vulkan_destroy_buffer(backend, &session->scalar_buffer);
	session->prepared = NULL;
}

/* Release one already unlinked session wrapper and plain-C metadata. */
static void
accel_vulkan_session_destroy(
	struct accel_vulkan_session *session)
{
	uint32_t i;

	if (session == NULL)
		return;

	if (session->snapshot != NULL) {
		/* Release every snapshot not consumed by resource creation. */
		for (i = 0; i < session->buffer_count; i++)
			noct_free(session->snapshot[i]);
	}

	noct_free(session->packed_type);
	noct_free(session->element_count);
	noct_free(session->snapshot);
	noct_free(session->descriptor_set);
	noct_free(session->buffer);
	noct_free(session->kernel_active);
	noct_free(session->kernel_trip);
	noct_free(session->kernel_start);
	noct_free(session->scalar_word);
	noct_free(session->scalar_value);
	session->magic = 0;
	noct_free(session);
}

/* Return the context attached through the VM optimizer callback userdata. */
static struct accel_context *
accel_vulkan_current_context(
	struct rt_env *env)
{
	if (env == NULL || env->vm == NULL)
		return NULL;

	return env->vm->accel_optimize_userdata;
}

/* Retrieve and validate one private session dictionary argument. */
static bool
accel_vulkan_get_session_argument(
	struct rt_env *env,
	struct rt_value *value,
	struct accel_vulkan_session **session)
{
	void *native_pointer;
	void (*native_finalizer)(void *native_pointer);

	*session = NULL;
	if (!noct_get_arg_check_dict(env, 0, value))
		return false;
	if (!rt_get_dict_native_pointer(
		env,
		value,
		&native_pointer,
		&native_finalizer)) {
		return false;
	}
	if (native_pointer == NULL ||
	    native_finalizer != accel_vulkan_session_finalizer) {
		rt_error(env, N_TR("Invalid Vulkan accelerator session."));
		return false;
	}

	*session = native_pointer;

	return true;
}

/* Pin one native local and record exact cleanup ownership. */
static bool
accel_vulkan_pin(
	struct rt_env *env,
	struct rt_value *value,
	uint32_t *count)
{
	if (!noct_pin_local(env, 1, value))
		return false;

	(*count)++;

	return true;
}

/* Unpin one native local in reverse source order. */
static bool
accel_vulkan_unpin(
	struct rt_env *env,
	struct rt_value *value,
	uint32_t *count)
{
	if (*count == 0)
		return true;
	if (!noct_unpin_local(env, 1, value))
		return false;

	(*count)--;

	return true;
}
