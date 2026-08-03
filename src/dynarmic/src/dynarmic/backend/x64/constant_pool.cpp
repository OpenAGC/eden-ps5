// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/x64/constant_pool.h"

#include <cstring>

#include "common/assert.h"
#include "dynarmic/backend/x64/block_of_code.h"

namespace Dynarmic::Backend::X64 {

ConstantPool::ConstantPool(BlockOfCode& code, size_t size)
        : code(code)
        , insertion_point(0) {
    code.EnsureMemoryCommitted(align_size + size);
    code.int3();
    code.align(align_size);
    auto* const executable_data = reinterpret_cast<ConstantT*>(code.AllocateFromCodeSpace(size));
    auto* const writable_data = reinterpret_cast<ConstantT*>(code.GetWritableAddress(executable_data));
    executable_pool = std::span<ConstantT>(executable_data, size / align_size);
    writable_pool = std::span<ConstantT>(writable_data, size / align_size);
}

Xbyak::Address ConstantPool::GetConstant(const Xbyak::AddressFrame& frame, u64 lower, u64 upper) {
    const auto constant = ConstantT(lower, upper);
    auto iter = constant_info.find(constant);
    if (iter == constant_info.end()) {
        ASSERT(insertion_point < executable_pool.size());
        writable_pool[insertion_point] = constant;
        iter = constant_info.insert({constant, &executable_pool[insertion_point]}).first;
        ++insertion_point;
    }
    return frame[code.rip + iter->second];
}

}  // namespace Dynarmic::Backend::X64
