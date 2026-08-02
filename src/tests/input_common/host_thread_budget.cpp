// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "input_common/host_thread_budget.h"

TEST_CASE("Host input workers remain enabled on unconstrained platforms",
          "[input_common][ps5]") {
    constexpr auto policy = InputCommon::ResolveHostInputWorkerPolicy(false);
    STATIC_REQUIRE(policy.enable_custom_hid);
    STATIC_REQUIRE(policy.enable_udp);
}

TEST_CASE("Prospero reserves host threads while retaining the SDL input backend",
          "[input_common][ps5]") {
    constexpr auto policy = InputCommon::ResolveHostInputWorkerPolicy(true);
    STATIC_REQUIRE_FALSE(policy.enable_custom_hid);
    STATIC_REQUIRE_FALSE(policy.enable_udp);
}
