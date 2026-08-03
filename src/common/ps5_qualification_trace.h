// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>

#include "common/common_types.h"

namespace Common {

constexpr bool ShouldTracePS5QualificationSequence(u32 sequence) {
    if (sequence < 16u) {
        return true;
    }
    const u32 completed_events = sequence + 1u;
    return (completed_events & (completed_events - 1u)) == 0u;
}

#ifdef __PROSPERO__
inline std::atomic<bool> ps5_qualification_guest_submit_response_ready{false};

inline void MarkPS5QualificationGuestSubmitResponseReady() {
    ps5_qualification_guest_submit_response_ready.store(true, std::memory_order_release);
}

inline bool IsPS5QualificationGuestSubmitResponseReady() {
    return ps5_qualification_guest_submit_response_ready.load(std::memory_order_acquire);
}
#endif

} // namespace Common
