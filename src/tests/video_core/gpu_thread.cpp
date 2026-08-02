// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

#include "video_core/gpu_thread.h"

TEST_CASE("GPU thread failure channel retains the first Vulkan error", "[video_core]") {
    VideoCommon::GPUThread::ThreadFailureState failure;

    const bool completed = VideoCommon::GPUThread::CaptureThreadFailure(
        failure, [] { throw std::runtime_error{"VK_ERROR_FORMAT_NOT_SUPPORTED"}; });

    REQUIRE_FALSE(completed);
    REQUIRE(failure.Failed());
    REQUIRE(failure.Message() == "VK_ERROR_FORMAT_NOT_SUPPORTED");
    REQUIRE_FALSE(failure.Report("replacement error"));
    REQUIRE(failure.Message() == "VK_ERROR_FORMAT_NOT_SUPPORTED");
}

TEST_CASE("GPU thread failure channel records non-standard exceptions", "[video_core]") {
    VideoCommon::GPUThread::ThreadFailureState failure;

    const bool completed = VideoCommon::GPUThread::CaptureThreadFailure(failure, [] { throw 1; });

    REQUIRE_FALSE(completed);
    REQUIRE(failure.Message() == "unknown exception");
}
