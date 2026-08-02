// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <sys/mman.h>
#endif

#include "common/assert.h"
#include "common/virtual_buffer.h"

namespace Common {

#if defined(__PROSPERO__)
namespace {

constexpr std::size_t ProsperoPageSize = 0x4000;
constexpr int ProsperoCpuReadWrite = 0x03;

constexpr std::size_t AlignUp(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) / alignment * alignment;
}

} // namespace

extern "C" {
int sceKernelMapFlexibleMemory(void** address, std::size_t length, int protection, int flags);
int sceKernelReleaseFlexibleMemory(void* address, std::size_t length);
int sceKernelMunmap(void* address, std::size_t length);
}
#endif

void* AllocateMemoryPages(std::size_t size) noexcept {
#ifdef _WIN32
    void* base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (base == nullptr) {
        // Probably failing to reserve is less likely than failing to commit
        base = VirtualAlloc(nullptr, size, MEM_COMMIT, PAGE_READWRITE);
    }
#elif defined(__PROSPERO__)
    if (size == 0 || size > SIZE_MAX - (ProsperoPageSize - 1))
        return nullptr;
    const std::size_t aligned_size = AlignUp(size, ProsperoPageSize);
    void* base = nullptr;
    const int result =
        sceKernelMapFlexibleMemory(&base, aligned_size, ProsperoCpuReadWrite, 0);
    if (result != 0) {
        std::fprintf(stderr,
                     "eden-ps5 virtual buffer allocation failed: requested=0x%zx "
                     "aligned=0x%zx result=0x%x\n",
                     size, aligned_size, static_cast<unsigned int>(result));
        base = nullptr;
    }
#else
    void* base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (base == MAP_FAILED)
        base = nullptr;
#endif
    ASSERT(base);
    return base;
}

void FreeMemoryPages(void* base, [[maybe_unused]] std::size_t size) noexcept {
    if (!base)
        return;
#ifdef _WIN32
    ASSERT(VirtualFree(base, 0, MEM_RELEASE));
#elif defined(__PROSPERO__)
    ASSERT(size <= SIZE_MAX - (ProsperoPageSize - 1));
    const std::size_t aligned_size = AlignUp(size, ProsperoPageSize);
    const int release_result = sceKernelReleaseFlexibleMemory(base, aligned_size);
    ASSERT(release_result == 0 || sceKernelMunmap(base, aligned_size) == 0);
#else
    ASSERT(munmap(base, size) == 0);
#endif
}

} // namespace Common
