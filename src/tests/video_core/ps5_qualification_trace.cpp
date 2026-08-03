// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "video_core/renderer_vulkan/ps5_qualification_trace.h"

TEST_CASE("PS5 qualification traces the capture boundary and sparse progress", "[video_core]") {
    for (u32 sequence = 0; sequence < 16; ++sequence) {
        REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(sequence));
    }
    REQUIRE_FALSE(Vulkan::ShouldTracePS5QualificationFrame(16));
    REQUIRE_FALSE(Vulkan::ShouldTracePS5QualificationFrame(30));
    REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(31));
    REQUIRE_FALSE(Vulkan::ShouldTracePS5QualificationFrame(32));
    REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(63));
    REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(127));
    REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(255));
    REQUIRE(Vulkan::ShouldTracePS5QualificationFrame(511));
}

TEST_CASE("PS5 qualification readback stops without gating normal presentation", "[video_core]") {
    for (u32 sequence = 0; sequence < 8; ++sequence) {
        REQUIRE(Vulkan::ShouldCapturePS5QualificationReadback(sequence, 1280, 720, 1920, 1080));
    }
    for (u32 sequence = 8; sequence < 600; ++sequence) {
        REQUIRE_FALSE(
            Vulkan::ShouldCapturePS5QualificationReadback(sequence, 1280, 720, 1920, 1080));
    }
    REQUIRE_FALSE(Vulkan::ShouldCapturePS5QualificationReadback(0, 0, 720, 1920, 1080));
    REQUIRE_FALSE(Vulkan::ShouldCapturePS5QualificationReadback(0, 1280, 720, 0, 1080));
}
