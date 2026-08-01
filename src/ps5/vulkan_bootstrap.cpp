// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint32_t ImageCount = 3;
constexpr std::uint32_t FrameCount = 600;
constexpr std::uint64_t WaitTimeoutNs = 2'000'000'000;

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
    if (state.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(state.device, state.command_pool, nullptr);
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
