// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "core/hle/service/nvdrv/core/nvmap.h"

namespace Service::Nvidia::NvCore {

TEST_CASE("NVMap pin ownership remains balanced", "[core][nvdrv][nvmap]") {
    NvMap::PinState pins;

    REQUIRE(pins.Count() == 0);
    REQUIRE(pins.Release() == NvMap::PinState::ReleaseResult::Unbalanced);
    REQUIRE(pins.Count() == 0);

    pins.Add();
    pins.Add();
    REQUIRE(pins.Count() == 2);
    REQUIRE(pins.Release() == NvMap::PinState::ReleaseResult::StillPinned);
    REQUIRE(pins.Count() == 1);
    REQUIRE(pins.Release() == NvMap::PinState::ReleaseResult::LastPin);
    REQUIRE(pins.Count() == 0);

    REQUIRE(pins.Release() == NvMap::PinState::ReleaseResult::Unbalanced);
    REQUIRE(pins.Count() == 0);
}

} // namespace Service::Nvidia::NvCore
