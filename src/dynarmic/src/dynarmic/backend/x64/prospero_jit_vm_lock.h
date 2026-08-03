// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#if defined(__PROSPERO__)

#include <mutex>

namespace Dynarmic::Backend::X64 {

// Serializes all Dynarmic executable-VM operations in this process.  This is
// intentionally an outer operation guard rather than an mprotect() wrapper:
// the payload SDK implements mmap(PROT_EXEC) by calling kernel_mprotect()
// internally, so interposing and re-locking mprotect() would deadlock.
[[nodiscard]] inline std::unique_lock<std::mutex> LockProsperoJitVm() {
    // SpinLockImpl tears down Xbyak JIT memory during static destruction. Keep
    // this guard alive through process teardown rather than depending on
    // cross-translation-unit destruction order.
    static std::mutex& mutex = *new std::mutex;
    return std::unique_lock<std::mutex>{mutex};
}

}  // namespace Dynarmic::Backend::X64

#endif
