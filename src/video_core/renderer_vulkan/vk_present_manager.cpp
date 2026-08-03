// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <cstdio>

#include "common/settings.h"
#include "common/thread.h"
#include "core/frontend/emu_window.h"
#include "video_core/renderer_vulkan/ps5_qualification_trace.h"
#include "video_core/renderer_vulkan/vk_present_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_swapchain.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {


namespace {

bool CanBlitToSwapchain(const vk::PhysicalDevice& physical_device, VkFormat format) {
    const VkFormatProperties props{physical_device.GetFormatProperties(format)};
    return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);
}

[[nodiscard]] VkImageSubresourceLayers MakeImageSubresourceLayers() {
    return VkImageSubresourceLayers{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
}

[[nodiscard]] VkImageBlit MakeImageBlit(s32 frame_width, s32 frame_height, s32 swapchain_width,
                                        s32 swapchain_height) {
    return VkImageBlit{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffsets =
            {
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                {
                    .x = frame_width,
                    .y = frame_height,
                    .z = 1,
                },
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffsets =
            {
                {
                    .x = 0,
                    .y = 0,
                    .z = 0,
                },
                {
                    .x = swapchain_width,
                    .y = swapchain_height,
                    .z = 1,
                },
            },
    };
}

[[nodiscard]] VkImageCopy MakeImageCopy(u32 frame_width, u32 frame_height, u32 swapchain_width,
                                        u32 swapchain_height) {
    return VkImageCopy{
        .srcSubresource = MakeImageSubresourceLayers(),
        .srcOffset =
            {
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .dstSubresource = MakeImageSubresourceLayers(),
        .dstOffset =
            {
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .extent =
            {
                .width = (std::min)(frame_width, swapchain_width),
                .height = (std::min)(frame_height, swapchain_height),
                .depth = 1,
            },
    };
}

} // Anonymous namespace

PresentManager::PresentManager(const vk::Instance& instance_,
                               Core::Frontend::EmuWindow& render_window_,
                               const Device& device_,
                               MemoryAllocator& memory_allocator_,
                               Scheduler& scheduler_,
                               Swapchain& swapchain_,
                               vk::SurfaceKHR& surface_)
    : instance{instance_}
    , render_window{render_window_}
    , device{device_}
    , memory_allocator{memory_allocator_}
    , scheduler{scheduler_}
    , swapchain{swapchain_}
    , surface{surface_}
    , blit_supported{CanBlitToSwapchain(device.GetPhysical(), swapchain.GetImageViewFormat())}
    , use_present_thread{Settings::values.async_presentation.GetValue()}
{
    SetImageCount();

    auto& dld = device.GetLogical();
    cmdpool = dld.CreateCommandPool({
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = device.GetGraphicsFamily(),
    });
    auto cmdbuffers = cmdpool.Allocate(image_count);

    frames.resize(image_count);
    for (u32 i = 0; i < frames.size(); i++) {
        Frame& frame = frames[i];
        frame.cmdbuf = vk::CommandBuffer{cmdbuffers[i], device.GetDispatchLoader()};
        frame.render_ready = dld.CreateSemaphore({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        });
        frame.present_done = dld.CreateFence({
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        });
#ifdef __PROSPERO__
        constexpr VkDeviceSize QualificationReadbackBytes = 32u * sizeof(u32);
        frame.qualification_readback = memory_allocator.CreateBuffer(
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .size = QualificationReadbackBytes,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = nullptr,
            },
            MemoryUsage::Download);
        if (!frame.qualification_readback.IsHostVisible() ||
            frame.qualification_readback.Mapped().size() < QualificationReadbackBytes) {
            throw std::runtime_error{"PS5 qualification readback is not host visible"};
        }
#endif
        free_queue.push_back(&frame);
    }

    if (use_present_thread) {
        present_thread = std::jthread([this](std::stop_token token) { PresentThread(token); });
    }
#ifdef __PROSPERO__
    std::fputs("eden-ps5: INIT CHECKPOINT present-manager\n", stderr);
#endif
}

PresentManager::~PresentManager() = default;

Frame* PresentManager::GetRenderFrame() {
#ifdef __PROSPERO__
    static std::atomic<u32> get_frame_sequence{0};
    const u32 sequence = get_frame_sequence.fetch_add(1, std::memory_order_relaxed);
    const bool trace_qualification = ShouldTracePS5QualificationFrame(sequence);
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: get-render-frame sequence=%u stage=queue-wait-before\n",
                     sequence);
    }
#endif

    // Wait for free presentation frames
    std::unique_lock lock{free_mutex};
    free_cv.wait(lock, [this] { return !free_queue.empty(); });

    // Take the frame from the queue
    Frame* frame = free_queue.front();
    free_queue.pop_front();
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: get-render-frame sequence=%u stage=fence-wait-before\n",
                     sequence);
    }
#endif

    // Wait for the presentation to be finished so all frame resources are free
    frame->present_done.Wait();
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: get-render-frame sequence=%u stage=fence-wait-after\n",
                     sequence);
    }
#endif
#ifdef __PROSPERO__
    if (frame->qualification_readback_pending) {
        frame->qualification_readback.Invalidate();
        const std::span<const u8> samples = frame->qualification_readback.Mapped().first(128u);
        const auto report_samples = [sequence = frame->qualification_sequence](
                                        const char* stage, std::span<const u8> stage_samples) {
            u32 nonzero_bytes = 0;
            u64 hash = UINT64_C(1469598103934665603);
            for (const u8 byte : stage_samples) {
                nonzero_bytes += byte != 0;
                hash = (hash ^ byte) * UINT64_C(1099511628211);
            }
            std::fprintf(stderr,
                         "eden-ps5: %s samples sequence=%u nonzero_bytes=%u hash=%016llx "
                         "first=%02x%02x%02x%02x\n",
                         stage, sequence, nonzero_bytes, static_cast<unsigned long long>(hash),
                         stage_samples[0], stage_samples[1], stage_samples[2], stage_samples[3]);
        };
        report_samples("intermediate-frame", samples.first(64u));
        report_samples("swapchain-frame", samples.subspan(64u, 64u));
        frame->qualification_readback_pending = false;
    }
#endif
    frame->present_done.Reset();
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: get-render-frame sequence=%u stage=return\n", sequence);
    }
#endif

    return frame;
}

void PresentManager::Present(Frame* frame) {
    if (use_present_thread) {
        scheduler.Record([this, frame](vk::CommandBuffer) {
            std::unique_lock lock{queue_mutex};
            present_queue.push_back(frame);
            frame_cv.notify_one();
        });
    } else {
#ifdef __PROSPERO__
        static std::atomic<u32> present_sequence{0};
        const u32 sequence = present_sequence.fetch_add(1, std::memory_order_relaxed);
        const bool trace_qualification = ShouldTracePS5QualificationFrame(sequence);
        if (trace_qualification) {
            std::fprintf(stderr, "eden-ps5: present-manager sequence=%u stage=worker-wait-before\n",
                         sequence);
        }
#endif
        scheduler.WaitWorker();
#ifdef __PROSPERO__
        if (trace_qualification) {
            std::fprintf(stderr, "eden-ps5: present-manager sequence=%u stage=worker-wait-after\n",
                         sequence);
        }
#endif
        CopyToSwapchain(frame);
#ifdef __PROSPERO__
        if (trace_qualification) {
            std::fprintf(stderr, "eden-ps5: present-manager sequence=%u stage=copy-after\n",
                         sequence);
        }
#endif
        free_queue.push_back(frame);
    }
}

void PresentManager::RecreateFrame(Frame* frame, u32 width, u32 height, VkFormat image_view_format,
                                   VkRenderPass rd) {
    auto& dld = device.GetLogical();

    frame->width = width;
    frame->height = height;

    frame->image = memory_allocator.CreateImage({
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = swapchain.GetImageFormat(),
        .extent =
            {
                .width = width,
                .height = height,
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    });

    frame->image_view = dld.CreateImageView({
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = *frame->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_view_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    });

    const VkImageView image_view{*frame->image_view};
    frame->framebuffer = dld.CreateFramebuffer({
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = rd,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = width,
        .height = height,
        .layers = 1,
    });
}

void PresentManager::WaitPresent() {
    if (!use_present_thread) {
        return;
    }

    // Wait for the present queue to be empty
    {
        std::unique_lock queue_lock{queue_mutex};
        frame_cv.wait(queue_lock, [this] { return present_queue.empty(); });
    }

    // The above condition will be satisfied when the last frame is taken from the queue.
    // To ensure that frame has been presented as well take hold of the swapchain
    // mutex.
    std::scoped_lock swapchain_lock{swapchain_mutex};
}

void PresentManager::PresentThread(std::stop_token token) {
    Common::SetCurrentThreadName("VulkanPresent");
    while (!token.stop_requested()) {
        std::unique_lock lock{queue_mutex};
        // Wait for presentation frames
        frame_cv.wait(lock, token, [this] { return !present_queue.empty(); });
        if (!token.stop_requested()) {
            // Take the frame and notify anyone waiting
            Frame* frame = present_queue.front();
            present_queue.pop_front();
            frame_cv.notify_one();

            // By exchanging the lock ownership we take the swapchain lock
            // before the queue lock goes out of scope. This way the swapchain
            // lock in WaitPresent is guaranteed to occur after here.
            std::exchange(lock, std::unique_lock{swapchain_mutex});
            CopyToSwapchain(frame);

            // Free the frame for reuse
            std::scoped_lock fl{free_mutex};
            free_queue.push_back(frame);
            free_cv.notify_one();
        }
    }
}

void PresentManager::RecreateSwapchain(Frame* frame) {
    swapchain.Create(*surface, frame->width, frame->height); // Pass raw pointer
    SetImageCount();
}

void PresentManager::SetImageCount() {
    // We cannot have more than 7 images in flight at any given time.
    // FRAMES_IN_FLIGHT is 8, and the cache TICKS_TO_DESTROY is 8.
    // Mali drivers will give us 6.
    image_count = std::min<size_t>(swapchain.GetImageCount(), 7);
}

void PresentManager::CopyToSwapchain(Frame* frame) {
    bool requires_recreation = false;

    while (true) {
        try {
            // Recreate surface and swapchain if needed.
            if (requires_recreation) {
#ifdef __ANDROID__
                surface = CreateSurface(instance, render_window.GetWindowInfo());
#endif
                RecreateSwapchain(frame);
            }

            // Draw to swapchain.
            return CopyToSwapchainImpl(frame);
        } catch (const vk::Exception& except) {
            if (except.GetResult() != VK_ERROR_SURFACE_LOST_KHR) {
                throw;
            }

            requires_recreation = true;
        }
    }
}

void PresentManager::CopyToSwapchainImpl(Frame* frame) {
#ifdef __PROSPERO__
    static std::atomic<u32> qualification_sequence{0};
    const u32 sequence = qualification_sequence.fetch_add(1, std::memory_order_relaxed);
    const bool trace_qualification = ShouldTracePS5QualificationFrame(sequence);
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: copy-to-swapchain sequence=%u stage=acquire-before\n",
                     sequence);
    }
#endif

    // If the size of the incoming frames has changed, recreate the swapchain
    // to account for that.
    const bool is_suboptimal = swapchain.NeedsRecreation();
    const bool size_changed =
        swapchain.GetWidth() != frame->width || swapchain.GetHeight() != frame->height;
    if (is_suboptimal || size_changed) {
        RecreateSwapchain(frame);
    }

    while (swapchain.AcquireNextImage()) {
        RecreateSwapchain(frame);
    }
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: copy-to-swapchain sequence=%u stage=acquire-after\n",
                     sequence);
    }
#endif

    const vk::CommandBuffer cmdbuf{frame->cmdbuf};
    cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });

    const VkImage image{swapchain.CurrentImage()};
    const VkExtent2D extent = swapchain.GetExtent();
    const std::array pre_barriers{
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = *frame->image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
    };
    std::array post_barriers{
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
        VkImageMemoryBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = *frame->image,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        },
    };

    cmdbuf.PipelineBarrier(vk::PIPELINE_STAGE_GRAPHICS_COMPUTE_TRANSFER, VK_PIPELINE_STAGE_TRANSFER_BIT, {},
                           {}, {}, pre_barriers);

#ifdef __PROSPERO__
    const bool capture_qualification_frames = ShouldCapturePS5QualificationReadback(
        sequence, frame->width, frame->height, extent.width, extent.height);
    if (capture_qualification_frames) {
        std::array<VkBufferImageCopy, 16> sample_regions{};
        for (u32 i = 0; i < sample_regions.size(); ++i) {
            VkBufferImageCopy& region = sample_regions[i];
            region.bufferOffset = i * sizeof(u32);
            region.imageSubresource = MakeImageSubresourceLayers();
            region.imageOffset = {
                .x = static_cast<s32>((i % 4u) * (frame->width - 1u) / 3u),
                .y = static_cast<s32>((i / 4u) * (frame->height - 1u) / 3u),
                .z = 0,
            };
            region.imageExtent = {.width = 1, .height = 1, .depth = 1};
        }
        cmdbuf.CopyImageToBuffer(*frame->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 *frame->qualification_readback, sample_regions);
    }
#endif

    if (blit_supported) {
        cmdbuf.BlitImage(*frame->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         MakeImageBlit(frame->width, frame->height, extent.width, extent.height),
                         VK_FILTER_LINEAR);
    } else {
        cmdbuf.CopyImage(*frame->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         MakeImageCopy(frame->width, frame->height, extent.width, extent.height));
    }

#ifdef __PROSPERO__
    if (capture_qualification_frames) {
        const VkImageMemoryBarrier swapchain_read_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = post_barriers[0].subresourceRange,
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                               swapchain_read_barrier);

        std::array<VkBufferImageCopy, 16> sample_regions{};
        for (u32 i = 0; i < sample_regions.size(); ++i) {
            VkBufferImageCopy& region = sample_regions[i];
            region.bufferOffset = 16u * sizeof(u32) + i * sizeof(u32);
            region.imageSubresource = MakeImageSubresourceLayers();
            region.imageOffset = {
                .x = static_cast<s32>((i % 4u) * (extent.width - 1u) / 3u),
                .y = static_cast<s32>((i / 4u) * (extent.height - 1u) / 3u),
                .z = 0,
            };
            region.imageExtent = {.width = 1, .height = 1, .depth = 1};
        }
        cmdbuf.CopyImageToBuffer(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 *frame->qualification_readback, sample_regions);
        const VkBufferMemoryBarrier host_read_barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = *frame->qualification_readback,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                               host_read_barrier);
        post_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        post_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        frame->qualification_sequence = sequence;
        frame->qualification_readback_pending = true;
    }
#endif

    cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, {},
                           {}, {}, post_barriers);

    cmdbuf.End();
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr, "eden-ps5: copy-to-swapchain sequence=%u stage=command-end\n",
                     sequence);
    }
#endif

    const VkSemaphore present_semaphore = swapchain.CurrentPresentSemaphore();
    const VkSemaphore render_semaphore = swapchain.CurrentRenderSemaphore();
    const std::array wait_semaphores = {present_semaphore, *frame->render_ready};

    static constexpr std::array<VkPipelineStageFlags, 2> wait_stage_masks{
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
    };

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 2U,
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = cmdbuf.address(),
        .signalSemaphoreCount = 1U,
        .pSignalSemaphores = &render_semaphore,
    };

    // Submit the image copy/blit to the swapchain
    {
        std::scoped_lock submit_lock{scheduler.submit_mutex};
#ifdef __PROSPERO__
        if (trace_qualification) {
            std::fprintf(stderr, "eden-ps5: copy-to-swapchain sequence=%u stage=submit-before\n",
                         sequence);
        }
#endif
        switch (const VkResult result =
                    device.GetGraphicsQueue().Submit(submit_info, *frame->present_done)) {
        case VK_SUCCESS:
            break;
        case VK_ERROR_DEVICE_LOST:
            device.ReportLoss();
            [[fallthrough]];
        default:
            vk::Check(result);
            break;
        }
#ifdef __PROSPERO__
        if (trace_qualification) {
            std::fprintf(stderr, "eden-ps5: copy-to-swapchain sequence=%u stage=submit-after\n",
                         sequence);
        }
#endif
    }

    // Present
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr,
                     "eden-ps5: copy-to-swapchain sequence=%u stage=native-present-before\n",
                     sequence);
    }
#endif
    swapchain.Present(render_semaphore);
#ifdef __PROSPERO__
    if (trace_qualification) {
        std::fprintf(stderr,
                     "eden-ps5: copy-to-swapchain sequence=%u stage=native-present-after\n",
                     sequence);
    }
#endif
}

} // namespace Vulkan
