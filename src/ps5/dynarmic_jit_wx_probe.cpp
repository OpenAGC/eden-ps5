// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ps5/runtime.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sys/mman.h>
#include <sys/thr.h>
#include <unistd.h>

namespace {

constexpr std::size_t PageSize = 0x4000;
constexpr std::size_t MetadataSize = PageSize;
constexpr std::size_t MappingSize = 0x2004000;
constexpr std::size_t CacheCount = 4;
constexpr std::size_t CycleCount = 4;
constexpr std::size_t CodeOffset = MetadataSize;
constexpr std::array<std::uint8_t, 6> KnownReturnStub{
    0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3,  // mov eax, 42; ret
};
constexpr int ExpectedReturn = 42;

static_assert(CacheCount == 4);
static_assert(MappingSize == 0x2000000 + MetadataSize);
static_assert(MappingSize % PageSize == 0);
static_assert(CodeOffset + KnownReturnStub.size() <= MappingSize);

#if defined(MAP_ANONYMOUS)
constexpr int AnonymousMapFlag = MAP_ANONYMOUS;
#elif defined(MAP_ANON)
constexpr int AnonymousMapFlag = MAP_ANON;
#else
#error "Prospero JIT W^X probe requires anonymous mmap support"
#endif

struct ProcessInfo {
    long pid = -1;
    long tid = -1;
    int tid_result = -1;
};

ProcessInfo GetProcessInfo() {
    ProcessInfo info{};
    info.pid = static_cast<long>(getpid());
    info.tid_result = thr_self(&info.tid);
    return info;
}

void LogOperation(const ProcessInfo& info, std::size_t cache, const char* iteration,
                  const char* operation, const void* base, int result, int error) {
    std::printf("eden-ps5 dynarmic-jit-wx probe: pid=%ld tid=%ld tid_result=%d cache=%zu "
                "iteration=%s op=%s base=%p size=0x%zx result=%d errno=%d\n",
                info.pid, info.tid, info.tid_result, cache, iteration, operation, base,
                MappingSize, result, error);
}

class ProbeMappings {
public:
    explicit ProbeMappings(ProcessInfo info) : info{info} {}

    ProbeMappings(const ProbeMappings&) = delete;
    ProbeMappings& operator=(const ProbeMappings&) = delete;

    ~ProbeMappings() {
        Cleanup();
    }

    bool CreateAll() {
        for (std::size_t cache = 0; cache < CacheCount; ++cache) {
            errno = 0;
            void* const mapping = mmap(nullptr, MappingSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                                       MAP_PRIVATE | AnonymousMapFlag, -1, 0);
            const int map_error = mapping == MAP_FAILED ? errno : 0;
            LogOperation(info, cache, "setup", "mmap-RWX-eligibility",
                         mapping == MAP_FAILED ? nullptr : mapping,
                         mapping == MAP_FAILED ? -1 : 0, map_error);
            if (mapping == MAP_FAILED) {
                return false;
            }

            mappings[cache] = mapping;
            errno = 0;
            const int demote_result = mprotect(mapping, MappingSize, PROT_READ | PROT_WRITE);
            const int demote_error = demote_result == 0 ? 0 : errno;
            LogOperation(info, cache, "setup", "initial-RW-demotion", mapping, demote_result,
                         demote_error);
            if (demote_result != 0) {
                return false;
            }
        }
        return true;
    }

    bool ExerciseAll() {
        for (std::size_t cache = 0; cache < CacheCount; ++cache) {
            for (std::size_t iteration = 0; iteration < CycleCount; ++iteration) {
                if (!ExerciseCycle(cache, iteration)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool Cleanup() {
        if (cleanup_attempted) {
            return cleanup_succeeded;
        }
        cleanup_attempted = true;

        bool clean = true;
        for (std::size_t cache = 0; cache < CacheCount; ++cache) {
            void*& mapping = mappings[cache];
            if (mapping == nullptr) {
                continue;
            }

            errno = 0;
            const int unmap_result = munmap(mapping, MappingSize);
            const int unmap_error = unmap_result == 0 ? 0 : errno;
            LogOperation(info, cache, "cleanup", "munmap", mapping, unmap_result, unmap_error);
            if (unmap_result == 0) {
                mapping = nullptr;
            } else {
                clean = false;
            }
        }
        cleanup_succeeded = clean;
        return cleanup_succeeded;
    }

private:
    bool ExerciseCycle(std::size_t cache, std::size_t iteration) {
        void* const mapping = mappings[cache];
        if (mapping == nullptr) {
            LogOperation(info, cache, "missing", "cache-state", nullptr, -1, EINVAL);
            return false;
        }

        char iteration_text[std::numeric_limits<std::size_t>::digits10 + 2]{};
        std::snprintf(iteration_text, sizeof(iteration_text), "%zu", iteration);

        auto* const entry = static_cast<std::uint8_t*>(mapping) + CodeOffset;
        std::memcpy(entry, KnownReturnStub.data(), KnownReturnStub.size());
        __builtin___clear_cache(reinterpret_cast<char*>(entry),
                                reinterpret_cast<char*>(entry + KnownReturnStub.size()));

        errno = 0;
        const int rx_result = mprotect(mapping, MappingSize, PROT_READ | PROT_EXEC);
        const int rx_error = rx_result == 0 ? 0 : errno;
        LogOperation(info, cache, iteration_text, "RW-to-RX", mapping, rx_result, rx_error);
        if (rx_result != 0) {
            return false;
        }

        using StubFunction = int (*)();
        static_assert(sizeof(StubFunction) == sizeof(entry));
        StubFunction stub{};
        std::memcpy(&stub, &entry, sizeof(stub));
        const int actual_return = stub();
        const int execute_result = actual_return == ExpectedReturn ? 0 : -1;
        const int execute_error = execute_result == 0 ? 0 : EPROTO;
        LogOperation(info, cache, iteration_text, "execute-known-return", mapping,
                     execute_result, execute_error);
        if (execute_result != 0) {
            return false;
        }

        errno = 0;
        const int rw_result = mprotect(mapping, MappingSize, PROT_READ | PROT_WRITE);
        const int rw_error = rw_result == 0 ? 0 : errno;
        LogOperation(info, cache, iteration_text, "RX-to-RW", mapping, rw_result, rw_error);
        return rw_result == 0;
    }

    ProcessInfo info;
    std::array<void*, CacheCount> mappings{};
    bool cleanup_attempted = false;
    bool cleanup_succeeded = true;
};

bool RunProbe() {
    ProbeMappings mappings{GetProcessInfo()};
    const bool passed = mappings.CreateAll() && mappings.ExerciseAll();
    return mappings.Cleanup() && passed;
}

} // namespace

int main() {
    const bool passed = RunProbe();
    std::printf("eden-ps5 dynarmic-jit-wx probe: %s caches=%zu size=0x%zx cycles=%zu\n",
                passed ? "PASS" : "FAIL", CacheCount, MappingSize, CycleCount);
    std::fflush(nullptr);
    Eden::PS5::TerminateApplication(passed ? 0 : 1);
}
