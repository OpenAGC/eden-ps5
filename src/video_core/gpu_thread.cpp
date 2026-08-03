// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>

#include "common/assert.h"
#ifdef __PROSPERO__
#include "common/ps5_qualification_trace.h"
#endif
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/frontend/graphics_context.h"
#include "video_core/control/scheduler.h"
#include "video_core/dma_pusher.h"
#include "video_core/gpu.h"
#include "video_core/gpu_thread.h"
#include "video_core/host1x/host1x.h"
#include "video_core/renderer_base.h"

namespace VideoCommon::GPUThread {

ThreadManager::ThreadManager(Core::System& system_) : system{system_} {}

ThreadManager::~ThreadManager() = default;

void ThreadManager::StartThread(VideoCore::RendererBase& renderer,
                                Core::Frontend::GraphicsContext& context,
                                Tegra::Control::Scheduler& scheduler) {
    rasterizer = renderer.ReadRasterizer();
    thread = std::jthread([&](std::stop_token stop_token) {
        Common::SetCurrentThreadName("GPU");
        Common::SetCurrentThreadPriority(Common::ThreadPriority::Critical);
        system.RegisterHostThread();

        const bool completed = CaptureThreadFailure(state.failure, [&] {
            auto current_context = context.Acquire();
            CommandDataContainer next;
            while (!stop_token.stop_requested()) {
                state.queue.PopWait(next, stop_token);
                if (stop_token.stop_requested()) {
                    break;
                }
                if (auto* submit_list = std::get_if<SubmitListCommand>(&next.data)) {
#ifdef __PROSPERO__
                    static std::atomic<u32> dispatch_sequence{0};
                    const u32 sequence = dispatch_sequence.fetch_add(1, std::memory_order_relaxed);
                    const bool trace_qualification =
                        Common::ShouldTracePS5QualificationSequence(sequence);
                    if (trace_qualification) {
                        LOG_INFO(HW_GPU,
                                 "PS5 GPU thread submit: sequence={} stage=dispatch-entry "
                                 "channel={} headers={} prefetched={}",
                                 sequence, submit_list->channel,
                                 submit_list->entries.command_lists.size(),
                                 submit_list->entries.prefetch_command_list.size());
                    }
#endif
                    scheduler.Push(system.GPU(), submit_list->channel,
                                   std::move(submit_list->entries));
#ifdef __PROSPERO__
                    if (trace_qualification) {
                        LOG_INFO(HW_GPU,
                                 "PS5 GPU thread submit: sequence={} stage=dispatch-complete",
                                 sequence);
                    }
#endif
                } else if (std::holds_alternative<GPUTickCommand>(next.data)) {
                    system.GPU().TickWork();
                } else if (const auto* flush = std::get_if<FlushRegionCommand>(&next.data)) {
                    renderer.ReadRasterizer()->FlushRegion(flush->addr, flush->size);
                } else if (const auto* invalidate =
                               std::get_if<InvalidateRegionCommand>(&next.data)) {
                    renderer.ReadRasterizer()->OnCacheInvalidation(invalidate->addr,
                                                                   invalidate->size);
                } else {
                    ASSERT(false);
                }
                state.signaled_fence.store(next.fence);
                if (next.block) {
                    // We have to lock the write_lock to ensure that the condition_variable wait not
                    // get a race between the check and the lock itself.
                    std::scoped_lock lk{state.write_lock};
                    state.cv.notify_all();
                }
            }
        });
        if (completed) {
            return;
        }

        const std::string message = state.failure.Message().value_or("unknown exception");
        LOG_CRITICAL(HW_GPU, "GPU thread stopped after an exception: {}", message);
        {
            std::scoped_lock lock{state.write_lock};
            state.signaled_fence.store(state.last_fence, std::memory_order_release);
        }
        state.cv.notify_all();
        system.Exit();
    });
}

void ThreadManager::SubmitList(s32 channel, Tegra::CommandList&& entries, bool is_async) {
    PushCommand(SubmitListCommand(channel, std::move(entries)), false, is_async);
}

void ThreadManager::FlushRegion(DAddr addr, u64 size, bool is_async) {
    if (!is_async) {
        // Always flush with synchronous GPU mode
        PushCommand(FlushRegionCommand(addr, size), false, is_async);
    }
}

void ThreadManager::TickGPU(bool is_async) {
    PushCommand(GPUTickCommand(), false, is_async);
}

void ThreadManager::InvalidateRegion(DAddr addr, u64 size) {
    rasterizer->OnCacheInvalidation(addr, size);
}

void ThreadManager::FlushAndInvalidateRegion(DAddr addr, u64 size, bool is_async) {
    if (Settings::IsGPULevelHigh()) {
        if (!is_async) {
            PushCommand(FlushRegionCommand(addr, size), false, is_async);
        } else {
            auto& gpu = system.GPU();
            const u64 fence = gpu.RequestFlush(addr, size);
            TickGPU(is_async);
            gpu.WaitForSyncOperation(fence);
        }
    }
    rasterizer->OnCacheInvalidation(addr, size);
}

u64 ThreadManager::PushCommand(CommandData&& command_data, bool block, bool is_async) {
    if (!is_async) {
        // In synchronous GPU mode, block the caller until the command has executed
        block = true;
    }

    std::unique_lock lk(state.write_lock);
    if (state.failure.Failed()) {
        return state.signaled_fence.load(std::memory_order_acquire);
    }
    const u64 fence{++state.last_fence};
    state.queue.EmplaceWait(std::move(command_data), fence, block);

    if (block) {
        state.cv.wait(lk, thread.get_stop_token(), [this, fence] {
            return state.failure.Failed() ||
                   fence <= state.signaled_fence.load(std::memory_order_acquire);
        });
    }

    return fence;
}

std::optional<std::string> ThreadManager::GetThreadFailure() const {
    return state.failure.Message();
}

} // namespace VideoCommon::GPUThread
