// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/scratch_buffer.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/service.h"

namespace Service::Nvidia {

class NVDRV final : public ServiceFramework<NVDRV> {
public:
    class LifecycleState {
    public:
        [[nodiscard]] NvResult Initialize(bool has_valid_process) noexcept {
            if (initialized) {
                return NvResult::Success;
            }
            if (!has_valid_process) {
                return NvResult::BadParameter;
            }
            initialized = true;
            return NvResult::Success;
        }

        [[nodiscard]] NvResult SetAruid(u64 caller_pid, u64 requested_aruid) noexcept {
            if (!initialized) {
                return NvResult::NotInitialized;
            }
            if (caller_pid == 0 || caller_pid != requested_aruid) {
                return NvResult::AccessDenied;
            }
            aruid = requested_aruid;
            return NvResult::Success;
        }

        void SetGraphicsFirmwareMemoryMarginEnabled(u64 value) noexcept {
            graphics_firmware_memory_margin_enabled = value != 0;
        }

        [[nodiscard]] bool IsInitialized() const noexcept {
            return initialized;
        }

        [[nodiscard]] u64 Aruid() const noexcept {
            return aruid;
        }

        [[nodiscard]] bool IsGraphicsFirmwareMemoryMarginEnabled() const noexcept {
            return graphics_firmware_memory_margin_enabled;
        }

    private:
        bool initialized{};
        u64 aruid{};
        bool graphics_firmware_memory_margin_enabled{};
    };

    explicit NVDRV(Core::System& system_, std::shared_ptr<Module> nvdrv_, const char* name);
    ~NVDRV() override;

    std::shared_ptr<Module> GetModule() const {
        return nvdrv;
    }

private:
    void Open(HLERequestContext& ctx);
    void Ioctl1(HLERequestContext& ctx);
    void Ioctl2(HLERequestContext& ctx);
    void Ioctl3(HLERequestContext& ctx);
    void Close(HLERequestContext& ctx);
    void Initialize(HLERequestContext& ctx);
    void QueryEvent(HLERequestContext& ctx);
    void SetAruid(HLERequestContext& ctx);
    void SetGraphicsFirmwareMemoryMarginEnabled(HLERequestContext& ctx);
    void GetStatus(HLERequestContext& ctx);
    void DumpGraphicsMemoryInfo(HLERequestContext& ctx);

    void ServiceError(HLERequestContext& ctx, NvResult result);

    std::shared_ptr<Module> nvdrv;

    LifecycleState lifecycle_state{};
    NvCore::SessionId session_id{};
    Common::ScratchBuffer<u8> output_buffer;
    Common::ScratchBuffer<u8> inline_output_buffer;
};

} // namespace Service::Nvidia
