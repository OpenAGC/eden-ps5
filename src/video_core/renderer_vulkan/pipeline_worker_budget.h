// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>

namespace Vulkan {

constexpr size_t ProsperoPipelineWorkerLimit = 1;

[[nodiscard]] constexpr size_t ResolvePipelineWorkerCount(size_t available, bool constrained) {
    return constrained ? std::min(available, ProsperoPipelineWorkerLimit) : available;
}

} // namespace Vulkan
