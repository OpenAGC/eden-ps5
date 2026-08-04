// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: 2021 Skyline Team and Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>

#include "common/logging.h"
#ifdef __PROSPERO__
#include "common/ps5_qualification_trace.h"
#endif
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_readable_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvdrv/nvdrv_interface.h"

namespace Service::Nvidia {

void NVDRV::Open(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NVDRV, "called");
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);

    if (!lifecycle_state.IsInitialized()) {
        rb.Push<DeviceFD>(0);
        rb.PushEnum(NvResult::NotInitialized);

        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    const auto& buffer = ctx.ReadBuffer();
    const std::string device_name(Common::StringFromBuffer(buffer));

    if (device_name == "/dev/nvhost-prof-gpu") {
        rb.Push<DeviceFD>(0);
        rb.PushEnum(NvResult::NotSupported);

        LOG_WARNING(Service_NVDRV, "/dev/nvhost-prof-gpu cannot be opened in production");
        return;
    }

    DeviceFD fd = nvdrv->Open(device_name, session_id);

    rb.Push<DeviceFD>(fd);
    rb.PushEnum(fd != INVALID_NVDRV_FD ? NvResult::Success : NvResult::FileOperationFailed);
}

void NVDRV::ServiceError(HLERequestContext& ctx, NvResult result) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(result);
}

void NVDRV::Ioctl1(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl={:#08x}", fd, command.raw);

    if (!lifecycle_state.IsInitialized()) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    // Check device
    output_buffer.resize_destructive(ctx.GetWriteBufferSize(0));
    const auto input_buffer = ctx.ReadBuffer(0);

    const auto nv_result = nvdrv->Ioctl1(fd, command, input_buffer, output_buffer);
    if (command.is_out != 0) {
        ctx.WriteBuffer(output_buffer);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(nv_result);
}

void NVDRV::Ioctl2(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
#ifdef __PROSPERO__
    static std::atomic<u32> ioctl2_sequence{0};
    const u32 sequence = ioctl2_sequence.fetch_add(1, std::memory_order_relaxed);
    const bool trace_qualification = Common::ShouldTracePS5QualificationSequence(sequence);
    if (trace_qualification) {
        LOG_INFO(Service_NVDRV,
                 "PS5 NVDRV ioctl2: sequence={} stage=entry fd={} command={:#x}", sequence, fd,
                 command.raw);
    }
#endif
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl={:#08x}", fd, command.raw);

    if (!lifecycle_state.IsInitialized()) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    const auto input_buffer = ctx.ReadBuffer(0);
    const auto input_inlined_buffer = ctx.ReadBuffer(1);
    output_buffer.resize_destructive(ctx.GetWriteBufferSize(0));

    const auto nv_result =
        nvdrv->Ioctl2(fd, command, input_buffer, input_inlined_buffer, output_buffer);
#ifdef __PROSPERO__
    if (trace_qualification) {
        LOG_INFO(Service_NVDRV,
                 "PS5 NVDRV ioctl2: sequence={} stage=device-return result={:#x} output_size={}",
                 sequence, static_cast<u32>(nv_result), output_buffer.size());
    }
#endif
    if (command.is_out != 0) {
        ctx.WriteBuffer(output_buffer);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(nv_result);
#ifdef __PROSPERO__
    if (trace_qualification) {
        LOG_INFO(Service_NVDRV, "PS5 NVDRV ioctl2: sequence={} stage=response-ready", sequence);
    }
    if (command.group == 'H' && command.cmd == 0x1b && nv_result == NvResult::Success) {
        Common::MarkPS5QualificationGuestSubmitResponseReady();
    }
#endif
}

void NVDRV::Ioctl3(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto command = rp.PopRaw<Ioctl>();
    LOG_DEBUG(Service_NVDRV, "called fd={}, ioctl={:#08x}", fd, command.raw);

    if (!lifecycle_state.IsInitialized()) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    const auto input_buffer = ctx.ReadBuffer(0);
    output_buffer.resize_destructive(ctx.GetWriteBufferSize(0));
    inline_output_buffer.resize_destructive(ctx.GetWriteBufferSize(1));

    const auto nv_result =
        nvdrv->Ioctl3(fd, command, input_buffer, output_buffer, inline_output_buffer);
    if (command.is_out != 0) {
        ctx.WriteBuffer(output_buffer, 0);
        ctx.WriteBuffer(inline_output_buffer, 1);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(nv_result);
}

void NVDRV::Close(HLERequestContext& ctx) {
    LOG_DEBUG(Service_NVDRV, "called");

    if (!lifecycle_state.IsInitialized()) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto result = nvdrv->Close(fd);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(result);
}

void NVDRV::Initialize(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);

    if (lifecycle_state.IsInitialized()) {
        LOG_DEBUG(Service_NVDRV, "called on an initialized session");
        rb.PushEnum(NvResult::Success);
        return;
    }

    IPC::RequestParser rp{ctx};
    const auto process_handle{ctx.GetCopyHandle(0)};
    // The transfer memory is lent to nvdrv as a work buffer since nvdrv is
    // unable to allocate as much memory on its own. For HLE it's unnecessary to handle it
    [[maybe_unused]] const auto transfer_memory_handle{ctx.GetCopyHandle(1)};
    [[maybe_unused]] const auto transfer_memory_size = rp.Pop<u32>();

    auto& container = nvdrv->GetContainer();
    auto process = ctx.GetObjectFromHandle<Kernel::KProcess>(process_handle);
    const NvResult validation_result = lifecycle_state.Initialize(process.IsNotNull());
    if (validation_result != NvResult::Success) {
        LOG_ERROR(Service_NVDRV, "Initialize rejected invalid process handle={:#x}",
                  process_handle);
        rb.PushEnum(validation_result);
        return;
    }

    session_id = container.OpenSession(process.GetPointerUnsafe());
    LOG_DEBUG(Service_NVDRV, "initialized process_id={:#x}, transfer_size={:#x}",
              process->GetProcessId(), transfer_memory_size);
    rb.PushEnum(NvResult::Success);
}

void NVDRV::QueryEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto fd = rp.Pop<DeviceFD>();
    const auto event_id = rp.Pop<u32>();

    if (!lifecycle_state.IsInitialized()) {
        ServiceError(ctx, NvResult::NotInitialized);
        LOG_ERROR(Service_NVDRV, "NvServices is not initialized!");
        return;
    }

    Kernel::KEvent* event = nullptr;
    NvResult result = nvdrv->QueryEvent(fd, event_id, event);

    if (result == NvResult::Success) {
        IPC::ResponseBuilder rb{ctx, 3, 1};
        rb.Push(ResultSuccess);
        auto& readable_event = event->GetReadableEvent();
        rb.PushCopyObjects(ctx, readable_event);
        rb.PushEnum(NvResult::Success);
    } else {
        LOG_ERROR(Service_NVDRV, "Invalid event request!");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.PushEnum(result);
    }
}

void NVDRV::SetAruid(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 requested_aruid = rp.Pop<u64>();
    const u64 caller_pid = ctx.GetPID();
    const NvResult result = lifecycle_state.SetAruid(caller_pid, requested_aruid);

    if (result == NvResult::Success) {
        LOG_DEBUG(Service_NVDRV, "called, aruid={:#x}", requested_aruid);
    } else {
        LOG_ERROR(Service_NVDRV,
                  "SetAruid rejected caller_pid={:#x}, requested_aruid={:#x}, result={:#x}",
                  caller_pid, requested_aruid, static_cast<u32>(result));
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(result);
}

void NVDRV::SetGraphicsFirmwareMemoryMarginEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const u64 value = rp.Pop<u64>();
    lifecycle_state.SetGraphicsFirmwareMemoryMarginEnabled(value);
    LOG_DEBUG(Service_NVDRV, "called, enabled={}",
              lifecycle_state.IsGraphicsFirmwareMemoryMarginEnabled());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void NVDRV::GetStatus(HLERequestContext& ctx) {
    LOG_WARNING(Service_NVDRV, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(NvResult::Success);
}

void NVDRV::DumpGraphicsMemoryInfo(HLERequestContext& ctx) {
    // According to SwitchBrew, this has no inputs and no outputs, so effectively does nothing on
    // retail hardware.
    LOG_DEBUG(Service_NVDRV, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

NVDRV::NVDRV(Core::System& system_, std::shared_ptr<Module> nvdrv_, const char* name)
    : ServiceFramework{system_, name}, nvdrv{std::move(nvdrv_)} {
    static const FunctionInfo functions[] = {
        {0, &NVDRV::Open, "Open"},
        {1, &NVDRV::Ioctl1, "Ioctl"},
        {2, &NVDRV::Close, "Close"},
        {3, &NVDRV::Initialize, "Initialize"},
        {4, &NVDRV::QueryEvent, "QueryEvent"},
        {5, nullptr, "MapSharedMem"},
        {6, &NVDRV::GetStatus, "GetStatus"},
        {7, nullptr, "SetAruidForTest"},
        {8, &NVDRV::SetAruid, "SetAruid"},
        {9, &NVDRV::DumpGraphicsMemoryInfo, "DumpGraphicsMemoryInfo"},
        {10, nullptr, "InitializeDevtools"},
        {11, &NVDRV::Ioctl2, "Ioctl2"},
        {12, &NVDRV::Ioctl3, "Ioctl3"},
        {13, &NVDRV::SetGraphicsFirmwareMemoryMarginEnabled,
         "SetGraphicsFirmwareMemoryMarginEnabled"},
    };
    RegisterHandlers(functions);
}

NVDRV::~NVDRV() {
    if (lifecycle_state.IsInitialized()) {
        auto& container = nvdrv->GetContainer();
        container.CloseSession(session_id);
    }
}

} // namespace Service::Nvidia
