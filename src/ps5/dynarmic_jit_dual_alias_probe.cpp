// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ps5/runtime.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
int sceKernelJitCreateSharedMemory(int flags, std::size_t size, int protection, int* handle);
int sceKernelJitCreateAliasOfSharedMemory(int handle, int protection, int* alias_handle);
int sceKernelJitMapSharedMemory(int handle, int protection, void** address);
int sceKernelMunmap(void* address, std::size_t size);
}

namespace {

constexpr std::size_t PageSize = 0x4000;
constexpr int JitFlags = 0;
// This is the shared object's maximum protection. The two mapped aliases below
// remain strictly RW and RX, respectively.
constexpr int JitMaximumProtection = PROT_READ | PROT_WRITE | PROT_EXEC;
constexpr std::array<std::uint8_t, 6> KnownReturnStub{
    0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3, // mov eax, 42; ret
};
constexpr int ExpectedReturn = 42;

static_assert(PageSize % 0x4000 == 0);
static_assert(KnownReturnStub.size() <= PageSize);

void LogOperation(const char* operation, const void* address, int result, int error) {
    std::printf("eden-ps5 dynarmic-jit-dual-alias probe: op=%s address=%p size=0x%zx "
                "result=%d errno=%d\n",
                operation, address, PageSize, result, error);
}

class DualAliasMapping {
public:
    DualAliasMapping() = default;
    DualAliasMapping(const DualAliasMapping&) = delete;
    DualAliasMapping& operator=(const DualAliasMapping&) = delete;

    ~DualAliasMapping() {
        Cleanup();
    }

    bool Create() {
        int handle = -1;
        errno = 0;
        int result =
            sceKernelJitCreateSharedMemory(JitFlags, PageSize, JitMaximumProtection, &handle);
        LogOperation("create-shared-RWX-maximum", nullptr, result, result == 0 ? 0 : errno);
        if (result != 0 || handle < 0) {
            return false;
        }
        writer_handle = handle;

        handle = -1;
        errno = 0;
        result =
            sceKernelJitCreateAliasOfSharedMemory(writer_handle, PROT_READ | PROT_EXEC, &handle);
        LogOperation("create-alias-RX", nullptr, result, result == 0 ? 0 : errno);
        if (result != 0 || handle < 0) {
            return false;
        }
        executor_handle = handle;

        void* address = nullptr;
        errno = 0;
        result = sceKernelJitMapSharedMemory(writer_handle, PROT_READ | PROT_WRITE, &address);
        LogOperation("map-writer-RW", address, result, result == 0 ? 0 : errno);
        if (result != 0 || address == nullptr) {
            return false;
        }
        writer = address;

        address = nullptr;
        errno = 0;
        result = sceKernelJitMapSharedMemory(executor_handle, PROT_READ | PROT_EXEC, &address);
        LogOperation("map-executor-RX", address, result, result == 0 ? 0 : errno);
        if (result != 0 || address == nullptr || address == writer) {
            return false;
        }
        executor = address;
        return true;
    }

    bool ExecuteKnownReturn() const {
        if (writer == nullptr || executor == nullptr || writer == executor) {
            LogOperation("alias-state", nullptr, -1, EINVAL);
            return false;
        }

        std::memcpy(writer, KnownReturnStub.data(), KnownReturnStub.size());
        __builtin___clear_cache(static_cast<char*>(writer),
                                static_cast<char*>(writer) + KnownReturnStub.size());

        using StubFunction = int (*)();
        static_assert(sizeof(StubFunction) == sizeof(executor));
        StubFunction stub{};
        std::memcpy(&stub, &executor, sizeof(stub));
        const int actual_return = stub();
        const int result = actual_return == ExpectedReturn ? 0 : -1;
        LogOperation("execute-RX-known-return", executor, result, result == 0 ? 0 : EPROTO);
        return result == 0;
    }

    bool Cleanup() {
        if (cleanup_attempted) {
            return cleanup_succeeded;
        }
        cleanup_attempted = true;

        cleanup_succeeded = Unmap(executor, "unmap-executor-RX") && cleanup_succeeded;
        cleanup_succeeded = Unmap(writer, "unmap-writer-RW") && cleanup_succeeded;
        cleanup_succeeded = Close(executor_handle, "close-alias-RX") && cleanup_succeeded;
        cleanup_succeeded = Close(writer_handle, "close-shared-RW") && cleanup_succeeded;
        return cleanup_succeeded;
    }

private:
    static bool Unmap(void*& address, const char* operation) {
        if (address == nullptr) {
            return true;
        }
        errno = 0;
        const int result = sceKernelMunmap(address, PageSize);
        LogOperation(operation, address, result, result == 0 ? 0 : errno);
        if (result == 0) {
            address = nullptr;
            return true;
        }
        return false;
    }

    static bool Close(int& handle, const char* operation) {
        if (handle < 0) {
            return true;
        }
        errno = 0;
        const int result = close(handle);
        LogOperation(operation, nullptr, result, result == 0 ? 0 : errno);
        if (result == 0) {
            handle = -1;
            return true;
        }
        return false;
    }

    int writer_handle = -1;
    int executor_handle = -1;
    void* writer = nullptr;
    void* executor = nullptr;
    bool cleanup_attempted = false;
    bool cleanup_succeeded = true;
};

bool RunProbe() {
    DualAliasMapping mapping;
    const bool created = mapping.Create();
    const bool executed = created && mapping.ExecuteKnownReturn();
    return mapping.Cleanup() && executed;
}

} // namespace

int main() {
    const bool passed = RunProbe();
    std::printf("eden-ps5 dynarmic-jit-dual-alias probe: %s size=0x%zx aliases=2\n",
                passed ? "PASS" : "FAIL", PageSize);
    std::fflush(nullptr);
    Eden::PS5::TerminateApplication(passed ? 0 : 1);
}
