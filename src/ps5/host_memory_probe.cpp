// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/host_memory.h"
#include "ps5/runtime.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>

namespace {

constexpr std::size_t GuestBackingSize = std::size_t{4} << 30;
constexpr std::size_t ChunkSize = std::size_t{64} << 20;
constexpr std::uint64_t MarkerSeed = 0x5053354d454d0000ULL;

bool ExerciseBacking(Common::HostMemory& memory) {
    auto* const base = memory.BackingBasePointer();
    if (!base || memory.VirtualBasePointer())
        return false;

    for (std::size_t offset = 0; offset < GuestBackingSize; offset += ChunkSize) {
        const std::uint64_t marker = MarkerSeed ^ offset;
        auto* const first = reinterpret_cast<std::uint64_t*>(base + offset);
        auto* const last = reinterpret_cast<std::uint64_t*>(base + offset + ChunkSize - sizeof(marker));
        *first = marker;
        *last = ~marker;
    }

    for (std::size_t offset = 0; offset < GuestBackingSize; offset += ChunkSize) {
        const std::uint64_t marker = MarkerSeed ^ offset;
        const auto* const first = reinterpret_cast<const std::uint64_t*>(base + offset);
        const auto* const last =
            reinterpret_cast<const std::uint64_t*>(base + offset + ChunkSize - sizeof(marker));
        if (*first != marker || *last != ~marker)
            return false;
    }
    return true;
}

} // namespace

int main() {
    bool passed = false;
    try {
        Common::HostMemory memory{GuestBackingSize, std::size_t{1} << 39};
        passed = ExerciseBacking(memory);
    } catch (const std::bad_alloc&) {
        std::printf("eden-ps5 host-memory probe: allocation failed\n");
    }

    if (passed) {
        std::printf("eden-ps5 host-memory probe: PASS bytes=0x%zx chunks=%zu\n",
                    GuestBackingSize, GuestBackingSize / ChunkSize);
    } else {
        std::printf("eden-ps5 host-memory probe: FAIL\n");
    }
    std::fflush(nullptr);
    Eden::PS5::TerminateApplication(passed ? 0 : 1);
}
