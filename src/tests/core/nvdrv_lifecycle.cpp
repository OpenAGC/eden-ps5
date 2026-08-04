// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "core/hle/service/nvdrv/nvdrv_interface.h"

namespace Service::Nvidia {

TEST_CASE("NVDRV lifecycle rejects invalid process and ARUID identities",
          "[core][nvdrv][lifecycle]") {
    NVDRV::LifecycleState state;

    REQUIRE_FALSE(state.IsInitialized());
    REQUIRE(state.SetAruid(0x51, 0x51) == NvResult::NotInitialized);
    REQUIRE(state.Initialize(false) == NvResult::BadParameter);
    REQUIRE_FALSE(state.IsInitialized());

    REQUIRE(state.Initialize(true) == NvResult::Success);
    REQUIRE(state.IsInitialized());
    REQUIRE(state.Initialize(false) == NvResult::Success);

    REQUIRE(state.SetAruid(0, 0) == NvResult::AccessDenied);
    REQUIRE(state.SetAruid(0x51, 0x52) == NvResult::AccessDenied);
    REQUIRE(state.Aruid() == 0);

    REQUIRE(state.SetAruid(0x51, 0x51) == NvResult::Success);
    REQUIRE(state.Aruid() == 0x51);
    REQUIRE(state.SetAruid(0x51, 0x51) == NvResult::Success);

    REQUIRE_FALSE(state.IsGraphicsFirmwareMemoryMarginEnabled());
    state.SetGraphicsFirmwareMemoryMarginEnabled(1);
    REQUIRE(state.IsGraphicsFirmwareMemoryMarginEnabled());
    state.SetGraphicsFirmwareMemoryMarginEnabled(0);
    REQUIRE_FALSE(state.IsGraphicsFirmwareMemoryMarginEnabled());
}

} // namespace Service::Nvidia
