// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ps5/runtime.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace Eden::PS5 {

namespace {

constexpr unsigned int TerminationGraceUs = 2'000'000;

extern "C" int sceKernelUsleep(unsigned int microseconds);
extern "C" int sceSystemServiceGetAppStatus(void* status);
extern "C" int sceSystemServiceKillApp(int app_id, int how, int reason, int core_dump);

} // namespace

[[noreturn]] void TerminateApplication(int result) {
    std::array<std::uint32_t, 0x100 / sizeof(std::uint32_t)> status{};
    const int status_result = sceSystemServiceGetAppStatus(status.data());
    std::uint32_t app_id = status[2];
    if (app_id < 0x10 || app_id == UINT32_MAX) {
        app_id = status[0];
    }
    if (status_result != 0 || app_id < 0x10 || app_id == UINT32_MAX) {
        std::printf("eden-ps5: cannot resolve app status=0x%x result=%d\n", status_result, result);
        std::fflush(nullptr);
        std::_Exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    const int kill_result = sceSystemServiceKillApp(static_cast<int>(app_id), 0, 0, 0);
    std::printf("eden-ps5: system exit app=0x%x result=0x%x status=%d\n", app_id, kill_result,
                result);
    std::fflush(nullptr);
    if (kill_result == 0) {
        sceKernelUsleep(TerminationGraceUs);
    }
    std::_Exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

} // namespace Eden::PS5

extern "C" [[noreturn]] void edenPs5TerminateApplicationFromJitFailure(
    const char* operation, const void* base, std::size_t size, int error) {
    std::fprintf(stderr,
                 "eden-ps5 dynarmic %s failed: base=%p size=0x%zx errno=%d; terminating "
                 "without executing an invalid JIT mapping\n",
                 operation, base, size, error);
    std::fflush(stderr);
    Eden::PS5::TerminateApplication(EXIT_FAILURE);
}
