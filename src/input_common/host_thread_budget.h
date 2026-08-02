// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace InputCommon {

struct HostInputWorkerPolicy {
    bool enable_custom_hid{true};
    bool enable_udp{true};
};

[[nodiscard]] constexpr HostInputWorkerPolicy ResolveHostInputWorkerPolicy(
    bool constrained_host_threads) {
    if (constrained_host_threads) {
        return {
            .enable_custom_hid = false,
            .enable_udp = false,
        };
    }
    return {};
}

} // namespace InputCommon
