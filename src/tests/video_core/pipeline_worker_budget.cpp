// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "video_core/renderer_vulkan/pipeline_worker_budget.h"

TEST_CASE("Pipeline worker budget preserves unconstrained capacity", "[video_core]") {
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(0, false) == 0);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(1, false) == 1);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(4, false) == 4);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(std::numeric_limits<size_t>::max(), false) ==
            std::numeric_limits<size_t>::max());
}

TEST_CASE("Prospero pipeline worker budget reserves service capacity", "[video_core]") {
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(0, true) == 0);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(1, true) == 1);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(2, true) == 1);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(4, true) == 1);
    REQUIRE(Vulkan::ResolvePipelineWorkerCount(std::numeric_limits<size_t>::max(), true) == 1);
}
