// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/dynamic_library.h"
#include "core/frontend/graphics_context.h"
#include "video_core/vulkan_common/vulkan.h"

namespace Vulkan {

std::shared_ptr<Common::DynamicLibrary> OpenLibrary(
    [[maybe_unused]] Core::Frontend::GraphicsContext* context = nullptr);

/// Resolve the Vulkan loader entrypoint from the platform's supported linkage model.
[[nodiscard]] bool LoadGetInstanceProcAddr(const Common::DynamicLibrary& library,
                                           PFN_vkGetInstanceProcAddr* proc) noexcept;

} // namespace Vulkan
