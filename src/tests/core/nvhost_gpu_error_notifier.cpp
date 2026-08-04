// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "core/hle/service/nvdrv/devices/nvhost_gpu.h"

namespace Service::Nvidia::Devices {

TEST_CASE("NVHOST GPU error notifier follows the Horizon enable lifecycle",
          "[core][nvdrv][nvhost-gpu]") {
    nvhost_gpu::ErrorNotifierState notifier;

    REQUIRE_FALSE(notifier.IsEnabled());
    REQUIRE(notifier.MemoryHandle() == 0);

    notifier.Set(0x10);
    REQUIRE(notifier.IsEnabled());
    REQUIRE(notifier.MemoryHandle() == 0x10);

    notifier.Set(0x24);
    REQUIRE(notifier.IsEnabled());
    REQUIRE(notifier.MemoryHandle() == 0x24);

    notifier.Set(0);
    REQUIRE_FALSE(notifier.IsEnabled());
    REQUIRE(notifier.MemoryHandle() == 0);
}

} // namespace Service::Nvidia::Devices
