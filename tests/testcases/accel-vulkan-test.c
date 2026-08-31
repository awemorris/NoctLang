/* -*- coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*- */

/*
 * Noct Programming Language
 * Copyright (c) 2025, 2026, Awe Morris
 */

/*
 * Device-independent Vulkan accelerator contract tests.
 */

#include "accel_vulkan.h"
#include "runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum mock_mode {
	MOCK_SUCCESS,
	MOCK_VERSION_MISSING,
	MOCK_VERSION_FAILURE,
	MOCK_VERSION_1_1,
	MOCK_PROPERTIES_MISSING
};

struct mock_state {
	int mode;
	uint32_t get_proc_count;
	uint32_t create_instance_count;
	uint32_t destroy_instance_count;
	uint32_t enumerate_device_count;
	uint32_t create_device_count;
	uint32_t destroy_device_count;
};

static struct mock_state mock;

static PFN_vkVoidFunction VKAPI_PTR mock_get_instance_proc_addr(VkInstance instance, const char *name);
static VkResult VKAPI_PTR mock_enumerate_instance_version(uint32_t *version);
static VkResult VKAPI_PTR mock_create_instance(const VkInstanceCreateInfo *create_info, const VkAllocationCallbacks *allocator, VkInstance *instance);
static void VKAPI_PTR mock_destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator);
static VkResult VKAPI_PTR mock_enumerate_physical_devices(VkInstance instance, uint32_t *count, VkPhysicalDevice *device);
static void VKAPI_PTR mock_get_physical_device_properties2(VkPhysicalDevice device, VkPhysicalDeviceProperties2 *properties);
static void VKAPI_PTR mock_get_queue_properties(VkPhysicalDevice device, uint32_t *count, VkQueueFamilyProperties *properties);
static void VKAPI_PTR mock_get_memory_properties(VkPhysicalDevice device, VkPhysicalDeviceMemoryProperties *properties);
static VkResult VKAPI_PTR mock_create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *create_info, const VkAllocationCallbacks *allocator, VkDevice *device);
static void VKAPI_PTR mock_destroy_device(VkDevice device, const VkAllocationCallbacks *allocator);
static void VKAPI_PTR mock_get_device_queue(VkDevice device, uint32_t family, uint32_t index, VkQueue *queue);
static VkResult VKAPI_PTR mock_device_wait_idle(VkDevice device);
static void mock_reset(int mode);
static void mock_make_api(struct accel_vulkan_api *api);
static bool test_missing_required_function(struct rt_env *env);
static bool test_version_cases(struct rt_env *env);
static bool test_properties_resolution(struct rt_env *env);
static bool test_device_selection(struct rt_env *env);
static bool expect_failure(struct rt_env *env, struct accel_vulkan_api *api, const char *gpu_name);

/* Run the device-independent Vulkan initialization contract tests. */
int
main(
	int argc,
	char *argv[])
{
	struct rt_vm *vm;
	struct rt_env *env;
	struct rt_config config;
	bool success;

	UNUSED_PARAMETER(argc);
	UNUSED_PARAMETER(argv);

	noct_set_default_config(&config);
	config.jit_enable = 0;
	if (!noct_create_vm(&vm, &env, &config)) {
		fprintf(stderr, "failed to create test VM\n");
		return 1;
	}

	success = test_missing_required_function(env);
	if (success)
		success = test_version_cases(env);
	if (success)
		success = test_properties_resolution(env);
	if (success)
		success = test_device_selection(env);

	if (!noct_destroy_vm(vm))
		success = false;

	if (!success)
		return 1;

	printf("Vulkan accelerator plan tests passed.\n");

	return 0;
}

/* Return one injected loader or instance function. */
static PFN_vkVoidFunction VKAPI_PTR
mock_get_instance_proc_addr(
	VkInstance instance,
	const char *name)
{
	mock.get_proc_count++;
	if (instance == VK_NULL_HANDLE &&
	    strcmp(name, "vkEnumerateInstanceVersion") == 0) {
		if (mock.mode == MOCK_VERSION_MISSING)
			return NULL;

		return (PFN_vkVoidFunction)mock_enumerate_instance_version;
	}
	if (instance != VK_NULL_HANDLE &&
	    strcmp(name, "vkGetPhysicalDeviceProperties2") == 0) {
		if (mock.mode == MOCK_PROPERTIES_MISSING)
			return NULL;

		return (PFN_vkVoidFunction)mock_get_physical_device_properties2;
	}

	return NULL;
}

/* Return the selected mock loader version. */
static VkResult VKAPI_PTR
mock_enumerate_instance_version(
	uint32_t *version)
{
	if (mock.mode == MOCK_VERSION_FAILURE)
		return VK_ERROR_INITIALIZATION_FAILED;
	if (mock.mode == MOCK_VERSION_1_1)
		*version = VK_API_VERSION_1_1;
	else
		*version = VK_API_VERSION_1_2;

	return VK_SUCCESS;
}

/* Create one opaque mock instance. */
static VkResult VKAPI_PTR
mock_create_instance(
	const VkInstanceCreateInfo *create_info,
	const VkAllocationCallbacks *allocator,
	VkInstance *instance)
{
	UNUSED_PARAMETER(allocator);

	if (create_info->pApplicationInfo->apiVersion != VK_API_VERSION_1_2)
		return VK_ERROR_INITIALIZATION_FAILED;

	mock.create_instance_count++;
	*instance = (VkInstance)(uintptr_t)1;

	return VK_SUCCESS;
}

/* Destroy one opaque mock instance. */
static void VKAPI_PTR
mock_destroy_instance(
	VkInstance instance,
	const VkAllocationCallbacks *allocator)
{
	UNUSED_PARAMETER(instance);
	UNUSED_PARAMETER(allocator);

	mock.destroy_instance_count++;
}

/* Enumerate one opaque mock physical device. */
static VkResult VKAPI_PTR
mock_enumerate_physical_devices(
	VkInstance instance,
	uint32_t *count,
	VkPhysicalDevice *device)
{
	UNUSED_PARAMETER(instance);

	mock.enumerate_device_count++;
	if (device == NULL) {
		*count = 1;
		return VK_SUCCESS;
	}

	if (*count == 0)
		return VK_INCOMPLETE;

	device[0] = (VkPhysicalDevice)(uintptr_t)2;
	*count = 1;

	return VK_SUCCESS;
}

/* Fill Vulkan 1.2 limits and strict Float32 properties. */
static void VKAPI_PTR
mock_get_physical_device_properties2(
	VkPhysicalDevice device,
	VkPhysicalDeviceProperties2 *properties)
{
	VkPhysicalDeviceFloatControlsProperties *float_controls;

	UNUSED_PARAMETER(device);

	properties->properties.apiVersion = VK_API_VERSION_1_2;
	properties->properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
	properties->properties.limits.maxComputeWorkGroupInvocations = 256;
	properties->properties.limits.maxComputeWorkGroupSize[0] = 256;
	properties->properties.limits.maxComputeWorkGroupCount[0] = 65535;
	properties->properties.limits.maxPerStageDescriptorStorageBuffers = 64;
	properties->properties.limits.maxDescriptorSetStorageBuffers = 64;
	properties->properties.limits.maxStorageBufferRange = 1U << 24;
	properties->properties.limits.nonCoherentAtomSize = 64;
	(void)strcpy(properties->properties.deviceName, "Mock GPU");

	float_controls = properties->pNext;
	if (float_controls != NULL) {
		float_controls->shaderSignedZeroInfNanPreserveFloat32 = VK_TRUE;
		float_controls->shaderDenormPreserveFloat32 = VK_TRUE;
		float_controls->shaderRoundingModeRTEFloat32 = VK_TRUE;
	}
}

/* Expose one compute-capable queue family. */
static void VKAPI_PTR
mock_get_queue_properties(
	VkPhysicalDevice device,
	uint32_t *count,
	VkQueueFamilyProperties *properties)
{
	UNUSED_PARAMETER(device);

	if (properties == NULL) {
		*count = 1;
		return;
	}

	memset(&properties[0], 0, sizeof(properties[0]));
	properties[0].queueFlags = VK_QUEUE_COMPUTE_BIT;
	properties[0].queueCount = 1;
	*count = 1;
}

/* Expose one host-visible coherent memory type. */
static void VKAPI_PTR
mock_get_memory_properties(
	VkPhysicalDevice device,
	VkPhysicalDeviceMemoryProperties *properties)
{
	UNUSED_PARAMETER(device);

	memset(properties, 0, sizeof(*properties));
	properties->memoryHeapCount = 1;
	properties->memoryHeaps[0].size = 1U << 24;
	properties->memoryTypeCount = 1;
	properties->memoryTypes[0].heapIndex = 0;
	properties->memoryTypes[0].propertyFlags =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

/* Create one opaque mock logical device. */
static VkResult VKAPI_PTR
mock_create_device(
	VkPhysicalDevice physical_device,
	const VkDeviceCreateInfo *create_info,
	const VkAllocationCallbacks *allocator,
	VkDevice *device)
{
	UNUSED_PARAMETER(physical_device);
	UNUSED_PARAMETER(create_info);
	UNUSED_PARAMETER(allocator);

	mock.create_device_count++;
	*device = (VkDevice)(uintptr_t)3;

	return VK_SUCCESS;
}

/* Destroy one opaque mock logical device. */
static void VKAPI_PTR
mock_destroy_device(
	VkDevice device,
	const VkAllocationCallbacks *allocator)
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(allocator);

	mock.destroy_device_count++;
}

/* Return one opaque mock compute queue. */
static void VKAPI_PTR
mock_get_device_queue(
	VkDevice device,
	uint32_t family,
	uint32_t index,
	VkQueue *queue)
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(family);
	UNUSED_PARAMETER(index);

	*queue = (VkQueue)(uintptr_t)4;
}

/* Complete mock device cleanup without waiting on hardware. */
static VkResult VKAPI_PTR
mock_device_wait_idle(
	VkDevice device)
{
	UNUSED_PARAMETER(device);

	return VK_SUCCESS;
}

/* Reset all call counters and select one injected behavior. */
static void
mock_reset(
	int mode)
{
	memset(&mock, 0, sizeof(mock));
	mock.mode = mode;
}

/* Fill one complete table while replacing only bootstrap calls with mocks. */
static void
mock_make_api(
	struct accel_vulkan_api *api)
{
	memset(api, 0, sizeof(*api));
	api->get_instance_proc_addr = mock_get_instance_proc_addr;
	api->create_instance = mock_create_instance;
	api->destroy_instance = mock_destroy_instance;
	api->enumerate_physical_devices = mock_enumerate_physical_devices;
	api->get_physical_device_properties2 = mock_get_physical_device_properties2;
	api->get_physical_device_queue_family_properties = mock_get_queue_properties;
	api->get_physical_device_memory_properties = mock_get_memory_properties;
	api->create_device = mock_create_device;
	api->destroy_device = mock_destroy_device;
	api->get_device_queue = mock_get_device_queue;
	api->device_wait_idle = mock_device_wait_idle;
	api->create_shader_module = vkCreateShaderModule;
	api->destroy_shader_module = vkDestroyShaderModule;
	api->create_descriptor_set_layout = vkCreateDescriptorSetLayout;
	api->destroy_descriptor_set_layout = vkDestroyDescriptorSetLayout;
	api->create_pipeline_layout = vkCreatePipelineLayout;
	api->destroy_pipeline_layout = vkDestroyPipelineLayout;
	api->create_compute_pipelines = vkCreateComputePipelines;
	api->destroy_pipeline = vkDestroyPipeline;
	api->create_buffer = vkCreateBuffer;
	api->destroy_buffer = vkDestroyBuffer;
	api->get_buffer_memory_requirements = vkGetBufferMemoryRequirements;
	api->allocate_memory = vkAllocateMemory;
	api->free_memory = vkFreeMemory;
	api->bind_buffer_memory = vkBindBufferMemory;
	api->map_memory = vkMapMemory;
	api->unmap_memory = vkUnmapMemory;
	api->flush_mapped_memory_ranges = vkFlushMappedMemoryRanges;
	api->invalidate_mapped_memory_ranges = vkInvalidateMappedMemoryRanges;
	api->create_descriptor_pool = vkCreateDescriptorPool;
	api->destroy_descriptor_pool = vkDestroyDescriptorPool;
	api->allocate_descriptor_sets = vkAllocateDescriptorSets;
	api->update_descriptor_sets = vkUpdateDescriptorSets;
	api->create_command_pool = vkCreateCommandPool;
	api->destroy_command_pool = vkDestroyCommandPool;
	api->allocate_command_buffers = vkAllocateCommandBuffers;
	api->begin_command_buffer = vkBeginCommandBuffer;
	api->end_command_buffer = vkEndCommandBuffer;
	api->cmd_bind_pipeline = vkCmdBindPipeline;
	api->cmd_bind_descriptor_sets = vkCmdBindDescriptorSets;
	api->cmd_copy_buffer = vkCmdCopyBuffer;
	api->cmd_pipeline_barrier = vkCmdPipelineBarrier;
	api->cmd_fill_buffer = vkCmdFillBuffer;
	api->cmd_dispatch = vkCmdDispatch;
	api->create_fence = vkCreateFence;
	api->destroy_fence = vkDestroyFence;
	api->queue_submit = vkQueueSubmit;
	api->wait_for_fences = vkWaitForFences;
}

/* Reject an incomplete table before making the first Vulkan call. */
static bool
test_missing_required_function(
	struct rt_env *env)
{
	struct accel_vulkan_api api;

	mock_reset(MOCK_SUCCESS);
	mock_make_api(&api);
	api.create_buffer = NULL;
	if (!expect_failure(env, &api, NULL))
		return false;
	if (mock.get_proc_count != 0 || mock.create_instance_count != 0)
		return false;

	return true;
}

/* Reject missing, failed, and pre-1.2 loader version queries. */
static bool
test_version_cases(
	struct rt_env *env)
{
	struct accel_vulkan_api api;

	mock_reset(MOCK_VERSION_MISSING);
	mock_make_api(&api);
	if (!expect_failure(env, &api, NULL))
		return false;
	if (mock.create_instance_count != 0)
		return false;

	mock_reset(MOCK_VERSION_FAILURE);
	mock_make_api(&api);
	if (!expect_failure(env, &api, NULL))
		return false;
	if (mock.create_instance_count != 0)
		return false;

	mock_reset(MOCK_VERSION_1_1);
	mock_make_api(&api);
	if (!expect_failure(env, &api, NULL))
		return false;
	if (mock.create_instance_count != 0)
		return false;

	return true;
}

/* Resolve properties through the instance and clean up on lookup failure. */
static bool
test_properties_resolution(
	struct rt_env *env)
{
	struct accel_vulkan_api api;

	mock_reset(MOCK_PROPERTIES_MISSING);
	mock_make_api(&api);
	if (!expect_failure(env, &api, NULL))
		return false;
	if (mock.create_instance_count != 1)
		return false;
	if (mock.destroy_instance_count != 1)
		return false;
	if (mock.enumerate_device_count != 0)
		return false;

	return true;
}

/* Select an exact mock device and release all owned backend resources. */
static bool
test_device_selection(
	struct rt_env *env)
{
	struct accel_vulkan_api api;
	const struct accel_backend_ops *ops;
	void *backend_state;

	mock_reset(MOCK_SUCCESS);
	mock_make_api(&api);
	if (!expect_failure(env, &api, "Other GPU"))
		return false;
	if (mock.create_device_count != 0)
		return false;

	mock_reset(MOCK_SUCCESS);
	mock_make_api(&api);
	ops = NULL;
	backend_state = NULL;
	if (!accel_vulkan_create_with_api(
		env,
		"Mock GPU",
		&api,
		&ops,
		&backend_state)) {
		return false;
	}
	if (ops == NULL || backend_state == NULL)
		return false;
	if (mock.create_device_count != 1)
		return false;

	ops->destroy_backend_state(backend_state);
	if (mock.destroy_device_count != 1)
		return false;
	if (mock.destroy_instance_count != 1)
		return false;

	return true;
}

/* Require a failed create call to leave both ownership outputs clear. */
static bool
expect_failure(
	struct rt_env *env,
	struct accel_vulkan_api *api,
	const char *gpu_name)
{
	const struct accel_backend_ops *ops;
	void *backend_state;

	ops = (const struct accel_backend_ops *)(uintptr_t)1;
	backend_state = (void *)(uintptr_t)1;
	env->error_message[0] = '\0';
	if (accel_vulkan_create_with_api(
		env,
		gpu_name,
		api,
		&ops,
		&backend_state)) {
		return false;
	}
	if (ops != NULL || backend_state != NULL)
		return false;
	if (env->error_message[0] == '\0')
		return false;

	return true;
}
