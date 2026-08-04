// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel::Svc {

TEST_CASE("Initial process id range exposes the kernel allocation bounds", "[core][kernel][svc]") {
    const auto minimum = GetInitialProcessIdRangeValue(
        static_cast<u64>(InitialProcessIdRangeInfo::Minimum));
    const auto maximum = GetInitialProcessIdRangeValue(
        static_cast<u64>(InitialProcessIdRangeInfo::Maximum));

    REQUIRE(minimum == InitialProcessIdMin);
    REQUIRE(maximum == InitialProcessIdMax);
    REQUIRE(*minimum == KProcess::InitialProcessIdMin);
    REQUIRE(*maximum == KProcess::InitialProcessIdMax);
    REQUIRE_FALSE(GetInitialProcessIdRangeValue(2).has_value());
    REQUIRE_FALSE(GetInitialProcessIdRangeValue(~u64{0}).has_value());
}

} // namespace Kernel::Svc
