// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

namespace Eden::PS5 {

[[noreturn]] void TerminateApplication(int result);

} // namespace Eden::PS5

extern "C" [[noreturn]] void edenPs5TerminateApplicationFromJitFailure(
    const char* operation, const void* base, std::size_t size, int error);
