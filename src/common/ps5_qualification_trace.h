// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Common {

constexpr bool ShouldTracePS5QualificationSequence(u32 sequence) {
    if (sequence < 16u) {
        return true;
    }
    const u32 completed_events = sequence + 1u;
    return (completed_events & (completed_events - 1u)) == 0u;
}

} // namespace Common
