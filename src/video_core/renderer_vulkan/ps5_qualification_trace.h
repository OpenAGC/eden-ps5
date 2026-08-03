// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Vulkan {

constexpr bool ShouldTracePS5QualificationFrame(u32 sequence) {
    if (sequence < 16u) {
        return true;
    }
    const u32 completed_frames = sequence + 1u;
    return (completed_frames & (completed_frames - 1u)) == 0u;
}

constexpr bool ShouldCapturePS5QualificationReadback(u32 sequence, u32 width, u32 height,
                                                     u32 extent_width, u32 extent_height) {
    return sequence < 8u && width > 0u && height > 0u && extent_width > 0u && extent_height > 0u;
}

} // namespace Vulkan
