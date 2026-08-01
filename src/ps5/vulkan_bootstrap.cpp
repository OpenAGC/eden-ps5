// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <vulkan/vulkan.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <array>
#include <cstdint>
#include <cstdio>

#include "vulkan_quad_indexed_comp_spv.h"

namespace {

constexpr std::uint32_t ImageCount = 3;
constexpr std::uint32_t FrameCount = 600;
constexpr std::uint64_t WaitTimeoutNs = 2'000'000'000;
constexpr VkDeviceSize VmaProbeSize = 4096;

extern "C" int sceKernelUsleep(unsigned int microseconds);
extern "C" int sceSystemServiceGetAppStatus(void* status);
extern "C" int sceSystemServiceKillApp(int app_id, int how, int reason, int core_dump);

struct BootstrapState {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSemaphore complete = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, ImageCount> commands{};
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkBuffer upload_buffer = VK_NULL_HANDLE;
    VkBuffer device_buffer = VK_NULL_HANDLE;
    VkBuffer readback_buffer = VK_NULL_HANDLE;
    VmaAllocation upload_allocation = VK_NULL_HANDLE;
    VmaAllocation device_allocation = VK_NULL_HANDLE;
    VmaAllocation readback_allocation = VK_NULL_HANDLE;
    void* upload_mapping = nullptr;
    void* readback_mapping = nullptr;
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
    VkPipeline compute_pipeline = VK_NULL_HANDLE;
    bool fence_pending = false;
};

[[noreturn]] void TerminateApplication() {
    std::array<std::uint32_t, 0x100 / sizeof(std::uint32_t)> status{};
    const int status_result = sceSystemServiceGetAppStatus(status.data());
    std::uint32_t app_id = status[2];
    if (app_id < 0x10 || app_id == UINT32_MAX) {
        app_id = status[0];
    }
    if (status_result != 0 || app_id < 0x10 || app_id == UINT32_MAX) {
        std::printf("eden-ps5-bootstrap: cannot resolve app status=0x%x\n", status_result);
        std::fflush(nullptr);
        for (;;) {
            sceKernelUsleep(250'000);
        }
    }

    const int kill_result = sceSystemServiceKillApp(static_cast<int>(app_id), 0, 0, 0);
    std::printf("eden-ps5-bootstrap: system exit app=0x%x result=0x%x\n", app_id, kill_result);
    std::fflush(nullptr);
    for (;;) {
        sceKernelUsleep(250'000);
    }
}

#define VK_REQUIRE(call)                                                                           \
    do {                                                                                           \
        const VkResult required_result = (call);                                                   \
        if (required_result != VK_SUCCESS) {                                                       \
            std::printf("eden-ps5-bootstrap: %s failed (%d)\n", #call, required_result);           \
            return required_result;                                                                \
        }                                                                                          \
    } while (false)

VkResult CreateVmaBuffer(BootstrapState& state, VkBufferUsageFlags usage,
                         VmaMemoryUsage memory_usage, VmaAllocationCreateFlags flags,
                         VkBuffer& buffer, VmaAllocation& allocation, void** mapping) {
    const VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = VmaProbeSize,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    const VmaAllocationCreateInfo allocation_info{
        .flags = flags,
        .usage = memory_usage,
    };
    VmaAllocationInfo result_info{};
    const VkResult result = vmaCreateBuffer(state.allocator, &buffer_info, &allocation_info,
                                            &buffer, &allocation, &result_info);
    if (result == VK_SUCCESS && mapping != nullptr) {
        *mapping = result_info.pMappedData;
    }
    return result;
}

VkResult RunVmaProbe(BootstrapState& state) {
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    const VmaAllocatorCreateInfo allocator_info{
        .physicalDevice = VK_NULL_HANDLE,
        .device = state.device,
        .pVulkanFunctions = &functions,
        .instance = state.instance,
        .vulkanApiVersion = VK_API_VERSION_1_1,
    };

    std::uint32_t physical_count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VK_REQUIRE(vkEnumeratePhysicalDevices(state.instance, &physical_count, &physical));
    if (physical_count != 1 || physical == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VmaAllocatorCreateInfo resolved_allocator_info = allocator_info;
    resolved_allocator_info.physicalDevice = physical;
    VK_REQUIRE(vmaCreateAllocator(&resolved_allocator_info, &state.allocator));

    constexpr VmaAllocationCreateFlags upload_flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    constexpr VmaAllocationCreateFlags readback_flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    VK_REQUIRE(CreateVmaBuffer(state,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_HOST, upload_flags,
                               state.upload_buffer, state.upload_allocation,
                               &state.upload_mapping));
    VK_REQUIRE(CreateVmaBuffer(state,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, 0,
                               state.device_buffer, state.device_allocation, nullptr));
    VK_REQUIRE(CreateVmaBuffer(state, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_AUTO_PREFER_HOST, readback_flags,
                               state.readback_buffer, state.readback_allocation,
                               &state.readback_mapping));
    if (state.upload_mapping == nullptr || state.readback_mapping == nullptr) {
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    auto* upload_words = static_cast<std::uint32_t*>(state.upload_mapping);
    auto* readback_words = static_cast<std::uint32_t*>(state.readback_mapping);
    constexpr std::size_t WordCount = VmaProbeSize / sizeof(std::uint32_t);
    for (std::size_t index = 0; index < WordCount; ++index) {
        upload_words[index] = 0x51a70000U ^ static_cast<std::uint32_t>(index * 0x1021U);
        readback_words[index] = 0;
    }
    VK_REQUIRE(vmaFlushAllocation(state.allocator, state.upload_allocation, 0, VK_WHOLE_SIZE));

    VkCommandBuffer probe_command = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo command_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(state.device, &command_info, &probe_command));
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_REQUIRE(vkBeginCommandBuffer(probe_command, &begin_info));
    const VkBufferCopy copy{.size = VmaProbeSize};
    vkCmdCopyBuffer(probe_command, state.upload_buffer, state.device_buffer, 1, &copy);
    vkCmdCopyBuffer(probe_command, state.device_buffer, state.readback_buffer, 1, &copy);
    VK_REQUIRE(vkEndCommandBuffer(probe_command));
    VK_REQUIRE(vkResetFences(state.device, 1, &state.fence));
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &probe_command,
    };
    VK_REQUIRE(vkQueueSubmit(state.queue, 1, &submit_info, state.fence));
    VK_REQUIRE(vkWaitForFences(state.device, 1, &state.fence, VK_TRUE, WaitTimeoutNs));
    VK_REQUIRE(vmaInvalidateAllocation(state.allocator, state.readback_allocation, 0,
                                       VK_WHOLE_SIZE));
    for (std::size_t index = 0; index < WordCount; ++index) {
        if (readback_words[index] != upload_words[index]) {
            std::printf("eden-ps5-bootstrap: VMA mismatch word=%zu expected=%08x actual=%08x\n",
                        index, upload_words[index], readback_words[index]);
            return VK_ERROR_VALIDATION_FAILED_EXT;
        }
    }
    vkFreeCommandBuffers(state.device, state.command_pool, 1, &probe_command);
    std::printf("eden-ps5-bootstrap: VMA upload/device/readback verified bytes=%llu\n",
                static_cast<unsigned long long>(VmaProbeSize));
    return VK_SUCCESS;
}

VkResult CreateEdenComputePipeline(BootstrapState& state) {
    const std::array bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };
    const VkDescriptorSetLayoutCreateInfo descriptor_layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VK_REQUIRE(vkCreateDescriptorSetLayout(state.device, &descriptor_layout_info, nullptr,
                                           &state.descriptor_layout));

    const VkDescriptorPoolSize pool_size{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 2,
    };
    const VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };
    VK_REQUIRE(vkCreateDescriptorPool(state.device, &pool_info, nullptr,
                                      &state.descriptor_pool));
    const VkDescriptorSetAllocateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = state.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &state.descriptor_layout,
    };
    VK_REQUIRE(vkAllocateDescriptorSets(state.device, &set_info, &state.descriptor_set));

    const std::array buffer_infos{
        VkDescriptorBufferInfo{state.upload_buffer, 0, 4 * sizeof(std::uint32_t)},
        VkDescriptorBufferInfo{state.device_buffer, 0, 6 * sizeof(std::uint32_t)},
    };
    const std::array writes{
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = state.descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_infos[0],
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = state.descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &buffer_infos[1],
        },
    };
    vkUpdateDescriptorSets(state.device, static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);

    const VkPushConstantRange push_range{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 3 * sizeof(std::uint32_t),
    };
    const VkPipelineLayoutCreateInfo pipeline_layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &state.descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range,
    };
    VK_REQUIRE(vkCreatePipelineLayout(state.device, &pipeline_layout_info, nullptr,
                                      &state.pipeline_layout));

    const VkShaderModuleCreateInfo shader_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(VULKAN_QUAD_INDEXED_COMP_SPV),
        .pCode = VULKAN_QUAD_INDEXED_COMP_SPV,
    };
    VK_REQUIRE(vkCreateShaderModule(state.device, &shader_info, nullptr,
                                    &state.shader_module));
    const VkPipelineCacheCreateInfo cache_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };
    VK_REQUIRE(vkCreatePipelineCache(state.device, &cache_info, nullptr,
                                     &state.pipeline_cache));
    const VkPipelineShaderStageCreateInfo stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = state.shader_module,
        .pName = "main",
    };
    const VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = state.pipeline_layout,
    };
    VK_REQUIRE(vkCreateComputePipelines(state.device, state.pipeline_cache, 1,
                                        &pipeline_info, nullptr,
                                        &state.compute_pipeline));
    return VK_SUCCESS;
}

VkResult DispatchEdenCompute(BootstrapState& state, std::uint32_t base_vertex) {
    constexpr std::array<std::uint32_t, 4> Input{3, 5, 7, 11};
    auto* upload_words = static_cast<std::uint32_t*>(state.upload_mapping);
    auto* readback_words = static_cast<std::uint32_t*>(state.readback_mapping);
    for (std::size_t index = 0; index < Input.size(); ++index) {
        upload_words[index] = Input[index];
    }
    for (std::size_t index = 0; index < 6; ++index) {
        readback_words[index] = 0;
    }
    VK_REQUIRE(vmaFlushAllocation(state.allocator, state.upload_allocation, 0,
                                  Input.size() * sizeof(std::uint32_t)));

    VkCommandBuffer command = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo command_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = state.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(state.device, &command_info, &command));
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_REQUIRE(vkBeginCommandBuffer(command, &begin_info));
    const std::array to_compute{
        VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = state.upload_buffer,
            .offset = 0,
            .size = 4 * sizeof(std::uint32_t),
        },
        VkBufferMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = state.device_buffer,
            .offset = 0,
            .size = 6 * sizeof(std::uint32_t),
        },
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                         static_cast<std::uint32_t>(to_compute.size()),
                         to_compute.data(), 0, nullptr);
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                      state.compute_pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            state.pipeline_layout, 0, 1, &state.descriptor_set,
                            0, nullptr);
    const std::array<std::uint32_t, 3> push_constants{base_vertex, 2, 0};
    vkCmdPushConstants(command, state.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push_constants), push_constants.data());
    vkCmdDispatch(command, 1, 1, 1);
    const VkBufferMemoryBarrier to_copy{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = state.device_buffer,
        .offset = 0,
        .size = 6 * sizeof(std::uint32_t),
    };
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                         &to_copy, 0, nullptr);
    const VkBufferCopy copy{.size = 6 * sizeof(std::uint32_t)};
    vkCmdCopyBuffer(command, state.device_buffer, state.readback_buffer, 1, &copy);
    VK_REQUIRE(vkEndCommandBuffer(command));
    VK_REQUIRE(vkResetFences(state.device, 1, &state.fence));
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command,
    };
    VK_REQUIRE(vkQueueSubmit(state.queue, 1, &submit_info, state.fence));
    VK_REQUIRE(vkWaitForFences(state.device, 1, &state.fence, VK_TRUE,
                               WaitTimeoutNs));
    VK_REQUIRE(vmaInvalidateAllocation(state.allocator, state.readback_allocation, 0,
                                       6 * sizeof(std::uint32_t)));
    constexpr std::array<std::uint32_t, 6> Swizzle{3, 5, 7, 3, 7, 11};
    for (std::size_t index = 0; index < Swizzle.size(); ++index) {
        const std::uint32_t expected = Swizzle[index] + base_vertex;
        if (readback_words[index] != expected) {
            std::printf(
                "eden-ps5-bootstrap: compute mismatch word=%zu expected=%u actual=%u\n",
                index, expected, readback_words[index]);
            return VK_ERROR_VALIDATION_FAILED_EXT;
        }
    }
    vkFreeCommandBuffers(state.device, state.command_pool, 1, &command);
    return VK_SUCCESS;
}

VkResult RunEdenComputeProbe(BootstrapState& state) {
    VK_REQUIRE(CreateEdenComputePipeline(state));
    VK_REQUIRE(DispatchEdenCompute(state, 100));

    std::array<std::uint8_t, 256> cache_data{};
    std::size_t cache_size = cache_data.size();
    VK_REQUIRE(vkGetPipelineCacheData(state.device, state.pipeline_cache,
                                      &cache_size, cache_data.data()));
    if (cache_size < sizeof(VkPipelineCacheHeaderVersionOne)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkDestroyPipeline(state.device, state.compute_pipeline, nullptr);
    state.compute_pipeline = VK_NULL_HANDLE;
    vkDestroyPipelineCache(state.device, state.pipeline_cache, nullptr);
    state.pipeline_cache = VK_NULL_HANDLE;

    const VkPipelineCacheCreateInfo cache_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .initialDataSize = cache_size,
        .pInitialData = cache_data.data(),
    };
    VK_REQUIRE(vkCreatePipelineCache(state.device, &cache_info, nullptr,
                                     &state.pipeline_cache));
    const VkPipelineShaderStageCreateInfo stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = state.shader_module,
        .pName = "main",
    };
    const VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage_info,
        .layout = state.pipeline_layout,
    };
    VK_REQUIRE(vkCreateComputePipelines(state.device, state.pipeline_cache, 1,
                                        &pipeline_info, nullptr,
                                        &state.compute_pipeline));
    VK_REQUIRE(DispatchEdenCompute(state, 200));
    std::printf(
        "eden-ps5-bootstrap: Eden quad-index compute verified cache_bytes=%zu reload=1\n",
        cache_size);
    return VK_SUCCESS;
}

} // namespace

namespace {

VkResult RunBootstrap(BootstrapState& state) {
    auto& instance = state.instance;
    auto& surface = state.surface;
    auto& device = state.device;
    auto& swapchain = state.swapchain;
    auto& command_pool = state.command_pool;
    auto& acquired = state.acquired;
    auto& complete = state.complete;
    auto& fence = state.fence;
    auto& queue = state.queue;
    auto& commands = state.commands;

    std::printf("eden-ps5-bootstrap: Vulkan initialization begin\n");

    const VkApplicationInfo application{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "eden-ps5-vulkan-bootstrap",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "Eden",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };
    const std::array instance_extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
    };
    const VkInstanceCreateInfo instance_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
        .enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size()),
        .ppEnabledExtensionNames = instance_extensions.data(),
    };
    VK_REQUIRE(vkCreateInstance(&instance_info, nullptr, &instance));
    std::printf("eden-ps5-bootstrap: instance created\n");

    const VkHeadlessSurfaceCreateInfoEXT surface_info{
        .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
    };
    VK_REQUIRE(vkCreateHeadlessSurfaceEXT(instance, &surface_info, nullptr, &surface));
    std::printf("eden-ps5-bootstrap: surface created\n");

    std::uint32_t physical_count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VK_REQUIRE(vkEnumeratePhysicalDevices(instance, &physical_count, &physical));
    if (physical_count != 1 || physical == VK_NULL_HANDLE) {
        std::printf("eden-ps5-bootstrap: expected one physical device\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkBool32 present_supported = VK_FALSE;
    VK_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(physical, 0, surface, &present_supported));
    if (!present_supported) {
        std::printf("eden-ps5-bootstrap: universal queue cannot present\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    VK_REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities));
    std::uint32_t format_count = 0;
    VK_REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, nullptr));
    std::array<VkSurfaceFormatKHR, 4> formats{};
    if (format_count == 0 || format_count > formats.size()) {
        std::printf("eden-ps5-bootstrap: unsupported surface format count %u\n", format_count);
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    VK_REQUIRE(
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, formats.data()));

    const float priority = 1.0F;
    const VkDeviceQueueCreateInfo queue_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char* device_extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    const VkDeviceCreateInfo device_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &device_extension,
    };
    VK_REQUIRE(vkCreateDevice(physical, &device_info, nullptr, &device));
    std::printf("eden-ps5-bootstrap: device created\n");
    vkGetDeviceQueue(device, 0, 0, &queue);

    const VkSwapchainCreateInfoKHR swapchain_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = ImageCount,
        .imageFormat = formats[0].format,
        .imageColorSpace = formats[0].colorSpace,
        .imageExtent = capabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VK_REQUIRE(vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));
    std::printf("eden-ps5-bootstrap: swapchain created\n");

    std::uint32_t image_count = ImageCount;
    std::array<VkImage, ImageCount> images{};
    VK_REQUIRE(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data()));
    if (image_count != ImageCount) {
        std::printf("eden-ps5-bootstrap: expected three swapchain images\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };
    VK_REQUIRE(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool));
    const VkCommandBufferAllocateInfo allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = ImageCount,
    };
    VK_REQUIRE(vkAllocateCommandBuffers(device, &allocate_info, commands.data()));

    for (std::uint32_t index = 0; index < ImageCount; ++index) {
        const VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        };
        VK_REQUIRE(vkBeginCommandBuffer(commands[index], &begin_info));
        const VkImageMemoryBarrier to_clear{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[index],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        vkCmdPipelineBarrier(commands[index], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_clear);
        const VkClearColorValue color{{0.03F, 0.18F, 0.65F, 1.0F}};
        const VkImageSubresourceRange color_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(commands[index], images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &color, 1, &color_range);
        const VkImageMemoryBarrier to_present{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[index],
            .subresourceRange = color_range,
        };
        vkCmdPipelineBarrier(commands[index], VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &to_present);
        VK_REQUIRE(vkEndCommandBuffer(commands[index]));
    }
    std::printf("eden-ps5-bootstrap: command buffers recorded\n");

    const VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VK_REQUIRE(vkCreateSemaphore(device, &semaphore_info, nullptr, &acquired));
    VK_REQUIRE(vkCreateSemaphore(device, &semaphore_info, nullptr, &complete));
    const VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_REQUIRE(vkCreateFence(device, &fence_info, nullptr, &fence));
    std::printf("eden-ps5-bootstrap: synchronization created\n");
    VK_REQUIRE(RunVmaProbe(state));
    VK_REQUIRE(RunEdenComputeProbe(state));

    for (std::uint32_t frame = 0; frame < FrameCount; ++frame) {
        VK_REQUIRE(vkWaitForFences(device, 1, &fence, VK_TRUE, WaitTimeoutNs));
        state.fence_pending = false;
        VK_REQUIRE(vkResetFences(device, 1, &fence));
        std::uint32_t image_index = 0;
        VK_REQUIRE(vkAcquireNextImageKHR(device, swapchain, WaitTimeoutNs, acquired, VK_NULL_HANDLE,
                                         &image_index));
        const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &acquired,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &commands[image_index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &complete,
        };
        VK_REQUIRE(vkQueueSubmit(queue, 1, &submit_info, fence));
        state.fence_pending = true;
        const VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &complete,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index,
        };
        VK_REQUIRE(vkQueuePresentKHR(queue, &present_info));
        if ((frame + 1) % 100 == 0) {
            std::printf("eden-ps5-bootstrap: %u/%u frames\n", frame + 1, FrameCount);
        }
    }
    VK_REQUIRE(vkWaitForFences(device, 1, &fence, VK_TRUE, WaitTimeoutNs));
    state.fence_pending = false;
    return VK_SUCCESS;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    BootstrapState state;
    VkResult result = RunBootstrap(state);
    std::printf("eden-ps5-bootstrap: cleanup begin result=%d\n", result);
    if (state.device != VK_NULL_HANDLE && state.fence != VK_NULL_HANDLE && state.fence_pending) {
        const VkResult wait_result =
            vkWaitForFences(state.device, 1, &state.fence, VK_TRUE, WaitTimeoutNs);
        if (result == VK_SUCCESS && wait_result != VK_SUCCESS) {
            result = wait_result;
        }
    }
    if (state.fence != VK_NULL_HANDLE) {
        vkDestroyFence(state.device, state.fence, nullptr);
    }
    if (state.complete != VK_NULL_HANDLE) {
        vkDestroySemaphore(state.device, state.complete, nullptr);
    }
    if (state.acquired != VK_NULL_HANDLE) {
        vkDestroySemaphore(state.device, state.acquired, nullptr);
    }
    if (state.compute_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(state.device, state.compute_pipeline, nullptr);
    }
    if (state.pipeline_cache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(state.device, state.pipeline_cache, nullptr);
    }
    if (state.shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(state.device, state.shader_module, nullptr);
    }
    if (state.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(state.device, state.pipeline_layout, nullptr);
    }
    if (state.descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(state.device, state.descriptor_pool, nullptr);
    }
    if (state.descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(state.device, state.descriptor_layout, nullptr);
    }
    if (state.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(state.device, state.command_pool, nullptr);
    }
    if (state.allocator != VK_NULL_HANDLE) {
        if (state.readback_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(state.allocator, state.readback_buffer,
                             state.readback_allocation);
            state.readback_buffer = VK_NULL_HANDLE;
            state.readback_allocation = VK_NULL_HANDLE;
        }
        if (state.device_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(state.allocator, state.device_buffer,
                             state.device_allocation);
            state.device_buffer = VK_NULL_HANDLE;
            state.device_allocation = VK_NULL_HANDLE;
        }
        if (state.upload_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(state.allocator, state.upload_buffer,
                             state.upload_allocation);
            state.upload_buffer = VK_NULL_HANDLE;
            state.upload_allocation = VK_NULL_HANDLE;
        }
        VmaTotalStatistics statistics{};
        vmaCalculateStatistics(state.allocator, &statistics);
        std::printf("eden-ps5-bootstrap: VMA teardown allocations=%u bytes=%llu\n",
                    statistics.total.statistics.allocationCount,
                    static_cast<unsigned long long>(
                        statistics.total.statistics.allocationBytes));
        if (result == VK_SUCCESS &&
            (statistics.total.statistics.allocationCount != 0 ||
             statistics.total.statistics.allocationBytes != 0)) {
            result = VK_ERROR_UNKNOWN;
        }
        vmaDestroyAllocator(state.allocator);
        state.allocator = VK_NULL_HANDLE;
    }
    if (state.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(state.device, state.swapchain, nullptr);
    }
    if (state.device != VK_NULL_HANDLE) {
        vkDestroyDevice(state.device, nullptr);
    }
    if (state.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
    }
    if (state.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(state.instance, nullptr);
    }
    if (result == VK_SUCCESS) {
        std::printf("eden-ps5-bootstrap: PASS %u frames\n", FrameCount);
    } else {
        std::printf("eden-ps5-bootstrap: FAIL result=%d\n", result);
    }
    std::fflush(nullptr);
    TerminateApplication();
}
