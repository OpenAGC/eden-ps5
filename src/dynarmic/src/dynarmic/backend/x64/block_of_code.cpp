// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/x64/block_of_code.h"

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/mman.h>
#    include <unistd.h>
#endif

#if defined(__PROSPERO__)
#    include <ps5/kernel.h>
#endif

#ifdef __APPLE__
#    include <errno.h>
#    include <fmt/format.h>
#    include <sys/sysctl.h>
#endif

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>

#include "common/assert.h"
#include "dynarmic/backend/x64/a32_jitstate.h"
#include "dynarmic/backend/x64/abi.h"
#include "dynarmic/backend/x64/hostloc.h"
#include "dynarmic/backend/x64/perf_map.h"
#include "dynarmic/backend/x64/prospero_jit_vm_lock.h"
#include "dynarmic/backend/x64/stack_layout.h"
#include "dynarmic/backend/x64/xbyak.h"
#include "dynarmic/mcl/bit.hpp"

namespace Dynarmic::Backend::X64 {

const Xbyak::Reg64 BlockOfCode::ABI_JIT_PTR = HostLocToReg64(Dynarmic::Backend::X64::ABI_JIT_PTR);
#ifdef _WIN32
const Xbyak::Reg64 BlockOfCode::ABI_RETURN = HostLocToReg64(Dynarmic::Backend::X64::ABI_RETURN);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM1 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM1);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM2 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM2);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM3 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM3);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM4 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM4);
const std::array<Xbyak::Reg64, ABI_PARAM_COUNT> BlockOfCode::ABI_PARAMS = {BlockOfCode::ABI_PARAM1, BlockOfCode::ABI_PARAM2, BlockOfCode::ABI_PARAM3, BlockOfCode::ABI_PARAM4};
#else
const Xbyak::Reg64 BlockOfCode::ABI_RETURN = HostLocToReg64(Dynarmic::Backend::X64::ABI_RETURN);
const Xbyak::Reg64 BlockOfCode::ABI_RETURN2 = HostLocToReg64(Dynarmic::Backend::X64::ABI_RETURN2);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM1 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM1);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM2 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM2);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM3 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM3);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM4 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM4);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM5 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM5);
const Xbyak::Reg64 BlockOfCode::ABI_PARAM6 = HostLocToReg64(Dynarmic::Backend::X64::ABI_PARAM6);
const std::array<Xbyak::Reg64, ABI_PARAM_COUNT> BlockOfCode::ABI_PARAMS = {BlockOfCode::ABI_PARAM1, BlockOfCode::ABI_PARAM2, BlockOfCode::ABI_PARAM3, BlockOfCode::ABI_PARAM4, BlockOfCode::ABI_PARAM5, BlockOfCode::ABI_PARAM6};
#endif

namespace {

constexpr size_t CONSTANT_POOL_SIZE = 2 * 1024 * 1024;
constexpr size_t PRELUDE_COMMIT_SIZE = 16 * 1024 * 1024;

#if defined(__PROSPERO__)
constexpr size_t PROSPERO_PAGE_SIZE = 0x4000;
constexpr int PROSPERO_JIT_MAX_PROTECTION = PROT_READ | PROT_WRITE | PROT_EXEC;
constexpr size_t PROSPERO_JIT_ALLOCATION_SLOTS = 32;

struct ProsperoJitAllocation {
    size_t mapping_size;
    u8* executable_code;
    u8* writable_mapping;
    u8* executable_mapping;
    int writable_descriptor;
    int executable_descriptor;
};

std::mutex s_prospero_jit_allocations_mutex;
std::array<std::optional<ProsperoJitAllocation>, PROSPERO_JIT_ALLOCATION_SLOTS>
    s_prospero_jit_allocations;

extern "C" [[noreturn]] void edenPs5TerminateApplicationFromJitFailure(
    const char* operation, const void* base, size_t size, int error);
extern "C" int sceKernelJitCreateSharedMemory(int flags, size_t size, int protection, int* descriptor);
extern "C" int sceKernelJitCreateAliasOfSharedMemory(int descriptor, int protection, int* alias_descriptor);
extern "C" int sceKernelJitMapSharedMemory(int descriptor, int protection, void** address);
extern "C" int sceKernelMunmap(void* address, size_t size);

constexpr size_t AlignUpProspero(size_t value) noexcept {
    return (value + PROSPERO_PAGE_SIZE - 1) / PROSPERO_PAGE_SIZE * PROSPERO_PAGE_SIZE;
}

[[noreturn]] void FailProsperoJitOperation(const char* operation, const void* base, size_t size, int error) {
    edenPs5TerminateApplicationFromJitFailure(operation, base, size, error);
}

bool RegisterProsperoJitAllocation(const ProsperoJitAllocation& allocation) {
    std::scoped_lock lock{s_prospero_jit_allocations_mutex};
    for (auto& slot : s_prospero_jit_allocations) {
        if (!slot.has_value()) {
            slot = allocation;
            return true;
        }
    }
    return false;
}

std::optional<ProsperoJitAllocation> TakeProsperoJitAllocation(const u8* executable_code) {
    std::scoped_lock lock{s_prospero_jit_allocations_mutex};
    for (auto& slot : s_prospero_jit_allocations) {
        if (slot.has_value() && slot->executable_code == executable_code) {
            auto allocation = slot;
            slot.reset();
            return allocation;
        }
    }
    return std::nullopt;
}

u8* FindProsperoWritableCode(const u8* executable_code) {
    std::scoped_lock lock{s_prospero_jit_allocations_mutex};
    for (const auto& slot : s_prospero_jit_allocations) {
        if (slot.has_value() && slot->executable_code == executable_code) {
            return slot->writable_mapping + PROSPERO_PAGE_SIZE;
        }
    }
    return nullptr;
}
#endif

class CustomXbyakAllocator : public Xbyak::Allocator {
public:
#ifdef _WIN32
    uint8_t* alloc(size_t size) override {
        void* p = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
        if (p == nullptr) {
            using Xbyak::Error;
            XBYAK_THROW(Xbyak::ERR_CANT_ALLOC);
        }
        return static_cast<uint8_t*>(p);
    }

    void free(uint8_t* p) override {
        VirtualFree(static_cast<void*>(p), 0, MEM_RELEASE);
    }

    bool useProtect() const override { return false; }
#else
#    if defined(__PROSPERO__)
    static constexpr size_t DYNARMIC_PAGE_SIZE = PROSPERO_PAGE_SIZE;
#    else
    static constexpr size_t DYNARMIC_PAGE_SIZE = 4096;
#    endif

    // Can't subclass Xbyak::MmapAllocator because it is not a pure interface
    // and doesn't expose its construtor
    uint8_t* alloc(size_t size) override {
        // Waste a page to store the size
#    if defined(__PROSPERO__)
        const auto vm_lock = LockProsperoJitVm();
        if (size > std::numeric_limits<size_t>::max() - DYNARMIC_PAGE_SIZE - (PROSPERO_PAGE_SIZE - 1)) {
            FailProsperoJitOperation("code-cache size overflow", nullptr, size, EOVERFLOW);
        }
        size = AlignUpProspero(size + DYNARMIC_PAGE_SIZE);

        int executable_descriptor = -1;
        int writable_descriptor = -1;
        void* writable_mapping = MAP_FAILED;
        void* executable_mapping = MAP_FAILED;
        auto cleanup_failed_allocation = [&] {
            if (executable_mapping != MAP_FAILED && executable_mapping != writable_mapping) {
                kernel_vm_operation_lock();
                sceKernelMunmap(executable_mapping, size);
                kernel_vm_operation_unlock();
                executable_mapping = MAP_FAILED;
            }
            if (writable_mapping != MAP_FAILED) {
                kernel_vm_operation_lock();
                sceKernelMunmap(writable_mapping, size);
                kernel_vm_operation_unlock();
                writable_mapping = MAP_FAILED;
            }
            if (writable_descriptor >= 0) {
                const int descriptor = writable_descriptor;
                close(descriptor);
                writable_descriptor = -1;
                if (executable_descriptor == descriptor) {
                    executable_descriptor = -1;
                }
            }
            if (executable_descriptor >= 0) {
                close(executable_descriptor);
                executable_descriptor = -1;
            }
        };

        errno = 0;
        int result = sceKernelJitCreateSharedMemory(0, size, PROSPERO_JIT_MAX_PROTECTION,
                                                    &executable_descriptor);
        if (result != 0 || executable_descriptor < 0) {
            const int error = errno != 0 ? errno : (result != 0 ? result : EINVAL);
            cleanup_failed_allocation();
            FailProsperoJitOperation("create executable JIT shared memory", nullptr, size,
                                     error);
        }
        errno = 0;
        result = sceKernelJitCreateAliasOfSharedMemory(
            executable_descriptor, PROT_READ | PROT_WRITE, &writable_descriptor);
        if (result != 0 || writable_descriptor < 0 || writable_descriptor == executable_descriptor) {
            const int error = errno != 0 ? errno : (result != 0 ? result : EINVAL);
            cleanup_failed_allocation();
            FailProsperoJitOperation("create writable JIT shared-memory alias", nullptr, size,
                                     error);
        }

        errno = 0;
        writable_mapping = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, writable_descriptor, 0);
        if (writable_mapping == MAP_FAILED) {
            const int error = errno;
            cleanup_failed_allocation();
            FailProsperoJitOperation("map writable JIT shared-memory alias", nullptr, size,
                                     error);
        }
        errno = 0;
        void* executable_candidate = nullptr;
        kernel_vm_operation_lock();
        result = sceKernelJitMapSharedMemory(executable_descriptor, PROT_READ | PROT_EXEC,
                                             &executable_candidate);
        const int executable_map_error = result == 0 ? 0 : errno;
        kernel_vm_operation_unlock();
        const bool executable_candidate_valid = executable_candidate != nullptr && executable_candidate != MAP_FAILED && reinterpret_cast<uintptr_t>(executable_candidate) % PROSPERO_PAGE_SIZE == 0;
        if (result == 0 && executable_candidate_valid) {
            executable_mapping = executable_candidate;
        }
        if (result != 0 || !executable_candidate_valid || executable_mapping == writable_mapping) {
            const int error = result != 0 ? executable_map_error : EINVAL;
            cleanup_failed_allocation();
            FailProsperoJitOperation("map distinct executable JIT shared-memory alias", nullptr,
                                     size, error);
        }

        const ProsperoJitAllocation allocation{
            .mapping_size = size,
            .executable_code = static_cast<u8*>(executable_mapping) + DYNARMIC_PAGE_SIZE,
            .writable_mapping = static_cast<u8*>(writable_mapping),
            .executable_mapping = static_cast<u8*>(executable_mapping),
            .writable_descriptor = writable_descriptor,
            .executable_descriptor = executable_descriptor,
        };
        if (!RegisterProsperoJitAllocation(allocation)) {
            cleanup_failed_allocation();
            FailProsperoJitOperation("register dual-alias code cache", executable_mapping, size,
                                     ENOSPC);
        }
        std::fprintf(stderr,
                     "eden-ps5 dynarmic dual-alias code cache: writable=%p executable=%p "
                     "size=0x%zx\n",
                     static_cast<void*>(static_cast<u8*>(writable_mapping) + DYNARMIC_PAGE_SIZE),
                     static_cast<void*>(static_cast<u8*>(executable_mapping) + DYNARMIC_PAGE_SIZE),
                     size - DYNARMIC_PAGE_SIZE);
        return static_cast<u8*>(executable_mapping) + DYNARMIC_PAGE_SIZE;
#    else
        size += DYNARMIC_PAGE_SIZE;

        int mode = MAP_PRIVATE;
#        if defined(MAP_ANONYMOUS)
        mode |= MAP_ANONYMOUS;
#        elif defined(MAP_ANON)
        mode |= MAP_ANON;
#        else
#            error "not supported"
#        endif
#        ifdef MAP_JIT
        mode |= MAP_JIT;
#        endif
        int prot = PROT_READ | PROT_WRITE;
#        ifdef PROT_MPROTECT
        // https://man.netbsd.org/mprotect.2 specifies that an mprotect() that is LESS
        // restrictive than the original mapping MUST fail
        prot |= PROT_MPROTECT(PROT_READ) | PROT_MPROTECT(PROT_WRITE) | PROT_MPROTECT(PROT_EXEC);
#        endif
        void* p = mmap(nullptr, size, prot, mode, -1, 0);
        if (p == MAP_FAILED) {
            using Xbyak::Error;
            XBYAK_THROW(Xbyak::ERR_CANT_ALLOC);
        }
        std::memcpy(p, &size, sizeof(size_t));
        return static_cast<uint8_t*>(p) + DYNARMIC_PAGE_SIZE;
#    endif
    }

    void free(uint8_t* p) override {
#    if defined(__PROSPERO__)
        const auto vm_lock = LockProsperoJitVm();
        const auto allocation = TakeProsperoJitAllocation(p);
        if (!allocation.has_value()) {
            FailProsperoJitOperation("unknown dual-alias code cache", p, 0, EINVAL);
        }

        const char* failed_operation = nullptr;
        int failed_error = 0;
        auto record_failure = [&](const char* operation, int result) {
            if (failed_operation == nullptr) {
                failed_operation = operation;
                failed_error = errno != 0 ? errno : result;
            }
        };
        errno = 0;
        kernel_vm_operation_lock();
        int result = sceKernelMunmap(allocation->executable_mapping, allocation->mapping_size);
        const int executable_unmap_error = result == 0 ? 0 : errno;
        kernel_vm_operation_unlock();
        if (result != 0) {
            errno = executable_unmap_error;
            record_failure("unmap executable JIT shared-memory alias", result);
        }
        errno = 0;
        kernel_vm_operation_lock();
        result = sceKernelMunmap(allocation->writable_mapping, allocation->mapping_size);
        const int writable_unmap_error = result == 0 ? 0 : errno;
        kernel_vm_operation_unlock();
        if (result != 0) {
            errno = writable_unmap_error;
            record_failure("unmap writable JIT shared-memory alias", result);
        }
        errno = 0;
        result = close(allocation->writable_descriptor);
        if (result != 0) {
            record_failure("close writable JIT shared-memory descriptor", result);
        }
        errno = 0;
        result = close(allocation->executable_descriptor);
        if (result != 0) {
            record_failure("close executable JIT shared-memory descriptor", result);
        }
        if (failed_operation != nullptr) {
            FailProsperoJitOperation(failed_operation, allocation->executable_mapping,
                                     allocation->mapping_size, failed_error);
        }
#    else
        size_t size;
        std::memcpy(&size, p - DYNARMIC_PAGE_SIZE, sizeof(size_t));
        munmap(p - DYNARMIC_PAGE_SIZE, size);
#    endif
    }

#    if defined(__PROSPERO__)
    uint8_t* getWritableAddress(uint8_t* executable) const override {
        u8* const writable = FindProsperoWritableCode(executable);
        if (writable == nullptr) {
            FailProsperoJitOperation("unknown executable code-cache address", executable, 0,
                                     EINVAL);
        }
        return writable;
    }
#    endif

#    ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
    bool useProtect() const override { return false; }
#    endif
#    if defined(__PROSPERO__) && !defined(DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT)
    bool useProtect() const override { return false; }
#    endif
#endif
};

// The allocator's fixed registry serializes split-alias lookup and cleanup.
CustomXbyakAllocator s_allocator;

#if defined(DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT) && !defined(__PROSPERO__)
void ProtectMemory(const void* base, size_t size, bool is_executable) {
#    ifdef _WIN32
    DWORD oldProtect = 0;
    VirtualProtect(const_cast<void*>(base), size, is_executable ? PAGE_EXECUTE_READ : PAGE_READWRITE, &oldProtect);
#    else
    static const size_t pageSize = sysconf(_SC_PAGESIZE);
    const size_t iaddr = reinterpret_cast<size_t>(base);
    const size_t roundAddr = iaddr & ~(pageSize - static_cast<size_t>(1));
    const int mode = is_executable ? (PROT_READ | PROT_EXEC) : (PROT_READ | PROT_WRITE);
    const size_t protectAddr = roundAddr;
    const size_t protect_size = size + (iaddr - roundAddr);
    const int result = mprotect(reinterpret_cast<void*>(protectAddr), protect_size, mode);
    ASSERT(result == 0);
#    endif
}
#endif

HostFeature GetHostFeatures() {
    HostFeature features = {};

#ifdef DYNARMIC_ENABLE_CPU_FEATURE_DETECTION
    using Cpu = Xbyak::util::Cpu;
    Xbyak::util::Cpu cpu_info;

    if (cpu_info.has(Cpu::tSSSE3))
        features |= HostFeature::SSSE3;
    if (cpu_info.has(Cpu::tSSE41))
        features |= HostFeature::SSE41;
    if (cpu_info.has(Cpu::tSSE42))
        features |= HostFeature::SSE42;
    if (cpu_info.has(Cpu::tAVX))
        features |= HostFeature::AVX;
    if (cpu_info.has(Cpu::tAVX2))
        features |= HostFeature::AVX2;
    if (cpu_info.has(Cpu::tAVX512F))
        features |= HostFeature::AVX512F;
    if (cpu_info.has(Cpu::tAVX512CD))
        features |= HostFeature::AVX512CD;
    if (cpu_info.has(Cpu::tAVX512VL))
        features |= HostFeature::AVX512VL;
    if (cpu_info.has(Cpu::tAVX512BW))
        features |= HostFeature::AVX512BW;
    if (cpu_info.has(Cpu::tAVX512DQ))
        features |= HostFeature::AVX512DQ;
    if (cpu_info.has(Cpu::tAVX512_BITALG))
        features |= HostFeature::AVX512BITALG;
    if (cpu_info.has(Cpu::tAVX512VBMI))
        features |= HostFeature::AVX512VBMI;
    if (cpu_info.has(Cpu::tPCLMULQDQ))
        features |= HostFeature::PCLMULQDQ;
    if (cpu_info.has(Cpu::tF16C))
        features |= HostFeature::F16C;
    if (cpu_info.has(Cpu::tFMA))
        features |= HostFeature::FMA;
    if (cpu_info.has(Cpu::tAESNI))
        features |= HostFeature::AES;
    if (cpu_info.has(Cpu::tSHA))
        features |= HostFeature::SHA;
    if (cpu_info.has(Cpu::tPOPCNT))
        features |= HostFeature::POPCNT;
    if (cpu_info.has(Cpu::tBMI1))
        features |= HostFeature::BMI1;
    if (cpu_info.has(Cpu::tBMI2))
        features |= HostFeature::BMI2;
    if (cpu_info.has(Cpu::tLZCNT))
        features |= HostFeature::LZCNT;
    if (cpu_info.has(Cpu::tGFNI))
        features |= HostFeature::GFNI;
    if (cpu_info.has(Cpu::tWAITPKG))
        features |= HostFeature::WAITPKG;

    if (cpu_info.has(Cpu::tBMI2)) {
        // BMI2 instructions such as pdep and pext have been very slow up until Zen 3.
        // Check for Zen 3 or newer by its family (0x19).
        // See also: https://en.wikichip.org/wiki/amd/cpuid
        if (cpu_info.has(Cpu::tAMD)) {
            std::array<u32, 4> data{};
            cpu_info.getCpuid(1, data.data());
            const u32 family_base = mcl::bit::get_bits<8, 11>(data[0]);
            const u32 family_extended = mcl::bit::get_bits<20, 27>(data[0]);
            const u32 family = family_base + family_extended;
            if (family >= 0x19)
                features |= HostFeature::FastBMI2;
        } else {
            features |= HostFeature::FastBMI2;
        }
    }
#endif

    return features;
}

#ifdef __APPLE__
bool IsUnderRosetta() {
    int result = 0;
    size_t result_size = sizeof(result);
    if (sysctlbyname("sysctl.proc_translated", &result, &result_size, nullptr, 0) == -1) {
        if (errno != ENOENT)
            fmt::print("IsUnderRosetta: Failed to detect Rosetta state, assuming not under Rosetta");
        return false;
    }
    return result != 0;
}
#endif

}  // anonymous namespace

#ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
static const auto default_cg_mode = Xbyak::DontSetProtectRWE;
#else
static const auto default_cg_mode = nullptr;  // Allow RWE
#endif

BlockOfCode::BlockOfCode(RunCodeCallbacks cb, JitStateInfo jsi, size_t total_code_size, std::function<void(BlockOfCode&)> rcp)
        : Xbyak::CodeGenerator(total_code_size, default_cg_mode, &s_allocator)
        , cb(std::move(cb))
        , jsi(jsi)
        , constant_pool(*this, CONSTANT_POOL_SIZE)
        , host_features(GetHostFeatures()) {
    EnableWriting();
    EnsureMemoryCommitted(PRELUDE_COMMIT_SIZE);
    GenRunCode(rcp);
}

void BlockOfCode::PreludeComplete() {
    prelude_complete = true;
    code_begin = getCurr();
    ClearCache();
    DisableWriting();
}

void BlockOfCode::EnableWriting() {
#ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
#    if !defined(__PROSPERO__)
#        ifdef _WIN32
    ProtectMemory(getCode(), committed_size, false);
#        else
    ProtectMemory(getCode(), maxSize_, false);
#        endif
#    endif
#endif
}

void BlockOfCode::DisableWriting() {
#ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
#    if !defined(__PROSPERO__)
#        ifdef _WIN32
    ProtectMemory(getCode(), committed_size, true);
#        else
    ProtectMemory(getCode(), maxSize_, true);
#        endif
#    endif
#endif
}

void BlockOfCode::ClearCache() {
    ASSERT(prelude_complete);
    SetCodePtr(code_begin);
}

size_t BlockOfCode::SpaceRemaining() const {
    ASSERT(prelude_complete);
    const u8* current_ptr = getCurr<const u8*>();
    if (current_ptr >= &top_[maxSize_])
        return 0;
    return &top_[maxSize_] - current_ptr;
}

void BlockOfCode::EnsureMemoryCommitted([[maybe_unused]] size_t codesize) {
#ifdef _WIN32
    if (committed_size < size_ + codesize) {
        committed_size = std::min<size_t>(maxSize_, committed_size + codesize);
#    ifdef DYNARMIC_ENABLE_NO_EXECUTE_SUPPORT
        VirtualAlloc(top_, committed_size, MEM_COMMIT, PAGE_READWRITE);
#    else
        VirtualAlloc(top_, committed_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#    endif
    }
#endif
}

HaltReason BlockOfCode::RunCode(void* jit_state, CodePtr code_ptr) const {
    return run_code(jit_state, code_ptr);
}

HaltReason BlockOfCode::StepCode(void* jit_state, CodePtr code_ptr) const {
    return step_code(jit_state, code_ptr);
}

void BlockOfCode::ReturnFromRunCode(bool mxcsr_already_exited) {
    size_t index = 0;
    if (mxcsr_already_exited)
        index |= MXCSR_ALREADY_EXITED;
    jmp(return_from_run_code[index]);
}

void BlockOfCode::ForceReturnFromRunCode(bool mxcsr_already_exited) {
    size_t index = FORCE_RETURN;
    if (mxcsr_already_exited)
        index |= MXCSR_ALREADY_EXITED;
    jmp(return_from_run_code[index]);
}

void BlockOfCode::GenRunCode(std::function<void(BlockOfCode&)> rcp) {
    Xbyak::Label return_to_caller, return_to_caller_mxcsr_already_exited;

    align();
    run_code = getCurr<RunCodeFuncType>();

    // This serves two purposes:
    // 1. It saves all the registers we as a callee need to save.
    // 2. It aligns the stack so that the code the JIT emits can assume
    //    that the stack is appropriately aligned for CALLs.
    ABI_PushCalleeSaveRegistersAndAdjustStack(*this, sizeof(StackLayout));

    mov(ABI_JIT_PTR, ABI_PARAM1);
    mov(rbx, ABI_PARAM2);  // save temporarily in non-volatile register

    if (cb.enable_cycle_counting) {
        cb.GetTicksRemaining->EmitCall(*this);
        mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_to_run)], ABI_RETURN);
        mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)], ABI_RETURN);
    }

    // r14 = page table
    // r13 = fastmem pointer
    rcp(*this);

    cmp(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], 0);
    jne(return_to_caller_mxcsr_already_exited, T_NEAR);

    SwitchMxcsrOnEntry();
    jmp(rbx);

    align();
    step_code = getCurr<RunCodeFuncType>();

    ABI_PushCalleeSaveRegistersAndAdjustStack(*this, sizeof(StackLayout));

    mov(ABI_JIT_PTR, ABI_PARAM1);

    if (cb.enable_cycle_counting) {
        mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_to_run)], 1);
        mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)], 1);
    }

    rcp(*this);

    cmp(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], 0);
    jne(return_to_caller_mxcsr_already_exited, T_NEAR);
    lock();
    or_(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], u32(HaltReason::Step));

    SwitchMxcsrOnEntry();
    jmp(ABI_PARAM2);

    // Dispatcher loop

    align();
    return_from_run_code[0] = getCurr<const void*>();

    cmp(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], 0);
    jne(return_to_caller);
    if (cb.enable_cycle_counting) {
        cmp(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)], 0);
        jng(return_to_caller);
    }
    cb.LookupBlock->EmitCall(*this);
    jmp(ABI_RETURN);

    align();
    return_from_run_code[MXCSR_ALREADY_EXITED] = getCurr<const void*>();

    cmp(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], 0);
    jne(return_to_caller_mxcsr_already_exited);
    if (cb.enable_cycle_counting) {
        cmp(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)], 0);
        jng(return_to_caller_mxcsr_already_exited);
    }
    SwitchMxcsrOnEntry();
    cb.LookupBlock->EmitCall(*this);
    jmp(ABI_RETURN);

    align();
    return_from_run_code[FORCE_RETURN] = getCurr<const void*>();
    L(return_to_caller);

    SwitchMxcsrOnExit();
    // fallthrough

    return_from_run_code[MXCSR_ALREADY_EXITED | FORCE_RETURN] = getCurr<const void*>();
    L(return_to_caller_mxcsr_already_exited);

    if (cb.enable_cycle_counting) {
        cb.AddTicks->EmitCall(*this, [this](RegList param) {
            mov(param[0], qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_to_run)]);
            sub(param[0], qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)]);
        });
    }

    xor_(eax, eax);
    xchg(dword[ABI_JIT_PTR + jsi.offsetof_halt_reason], eax);

    ABI_PopCalleeSaveRegistersAndAdjustStack(*this, sizeof(StackLayout));
    ret();

    PerfMapRegister(run_code, getCurr(), "dynarmic_dispatcher");
}

void BlockOfCode::SwitchMxcsrOnEntry() {
    stmxcsr(dword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, save_host_MXCSR)]);
    ldmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_guest_MXCSR]);
}

void BlockOfCode::SwitchMxcsrOnExit() {
    stmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_guest_MXCSR]);
    ldmxcsr(dword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, save_host_MXCSR)]);
}

void BlockOfCode::EnterStandardASIMD() {
    stmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_guest_MXCSR]);
    ldmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_asimd_MXCSR]);
}

void BlockOfCode::LeaveStandardASIMD() {
    stmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_asimd_MXCSR]);
    ldmxcsr(dword[ABI_JIT_PTR + jsi.offsetof_guest_MXCSR]);
}

void BlockOfCode::UpdateTicks() {
    if (!cb.enable_cycle_counting) {
        return;
    }

    cb.AddTicks->EmitCall(*this, [this](RegList param) {
        mov(param[0], qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_to_run)]);
        sub(param[0], qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)]);
    });

    cb.GetTicksRemaining->EmitCall(*this);
    mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_to_run)], ABI_RETURN);
    mov(qword[rsp + ABI_SHADOW_SPACE + offsetof(StackLayout, cycles_remaining)], ABI_RETURN);
}

void BlockOfCode::LookupBlock() {
    cb.LookupBlock->EmitCall(*this);
}

void BlockOfCode::LoadRequiredFlagsForCondFromRax(IR::Cond cond) {
#ifdef __APPLE__
    static const bool is_rosetta = IsUnderRosetta();
#endif

    // sahf restores SF, ZF, CF
    // add al, 0x7F restores OF

    switch (cond) {
    case IR::Cond::EQ:  // z
    case IR::Cond::NE:  // !z
    case IR::Cond::CS:  // c
    case IR::Cond::CC:  // !c
    case IR::Cond::MI:  // n
    case IR::Cond::PL:  // !n
        sahf();
        break;
    case IR::Cond::VS:  // v
    case IR::Cond::VC:  // !v
        cmp(al, 0x81);
        break;
    case IR::Cond::HI:  // c & !z
    case IR::Cond::LS:  // !c | z
        sahf();
        cmc();
        break;
    case IR::Cond::GE:  // n == v
    case IR::Cond::LT:  // n != v
    case IR::Cond::GT:  // !z & (n == v)
    case IR::Cond::LE:  // z | (n != v)
#ifdef __APPLE__
        if (is_rosetta) {
            shl(al, 3);
            xchg(al, ah);
            push(rax);
            popf();
            break;
        }
#endif
        cmp(al, 0x81);
        sahf();
        break;
    case IR::Cond::AL:
    case IR::Cond::NV:
        break;
    default:
        UNREACHABLE();
    }
}

Xbyak::Address BlockOfCode::Const(const Xbyak::AddressFrame& frame, u64 lower, u64 upper) {
    return constant_pool.GetConstant(frame, lower, upper);
}

CodePtr BlockOfCode::GetCodeBegin() const {
    return code_begin;
}

size_t BlockOfCode::GetTotalCodeSize() const {
    return maxSize_;
}

void* BlockOfCode::AllocateFromCodeSpace(size_t alloc_size) {
    if (size_ + alloc_size >= maxSize_) {
        using Xbyak::Error;
        XBYAK_THROW(Xbyak::ERR_CODE_IS_TOO_BIG);
    }

    EnsureMemoryCommitted(alloc_size);

    void* const ret = getCurr<void*>();
    void* const writable_ret = GetWritableAddress(ret);
    size_ += alloc_size;
    memset(writable_ret, 0, alloc_size);
    return ret;
}

void* BlockOfCode::GetWritableAddress(const void* executable_address) const {
    return Xbyak::CodeArray::getWritableAddress(executable_address);
}

void BlockOfCode::SetCodePtr(CodePtr code_ptr) {
    // The "size" defines where top_, the insertion point, is.
    size_t required_size = reinterpret_cast<const u8*>(code_ptr) - getCode();
    setSize(required_size);
}

void BlockOfCode::EnsurePatchLocationSize(CodePtr begin, size_t size) {
    size_t current_size = getCurr<const u8*>() - reinterpret_cast<const u8*>(begin);
    ASSERT(current_size <= size);
    nop(size - current_size);
}

}  // namespace Dynarmic::Backend::X64
