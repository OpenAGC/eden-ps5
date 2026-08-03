// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <bit>
#include <cstddef>
#include <span>
#include <utility>

#include <ankerl/unordered_dense.h>

#include "common/common_types.h"
#include "dynarmic/backend/x64/xbyak.h"

namespace Dynarmic::Backend::X64 {

class BlockOfCode;

/// ConstantPool allocates a block of memory from BlockOfCode.
/// It places constants into this block of memory, returning the address
/// of the memory location where the constant is placed. If the constant
/// already exists, its memory location is reused.
class ConstantPool final {
public:
    ConstantPool(BlockOfCode& code, size_t size);

    Xbyak::Address GetConstant(const Xbyak::AddressFrame& frame, u64 lower, u64 upper = 0);

private:
    static constexpr size_t align_size = 16;  // bytes

    using ConstantT = std::pair<u64, u64>;
    static_assert(sizeof(ConstantT) == align_size);

    struct ConstantHash {
        std::size_t operator()(const ConstantT& constant) const noexcept {
            return constant.first ^ std::rotl<u64>(constant.second, 1);
        }
    };

    // Xbyak uses executable_pool addresses to calculate RIP-relative
    // displacements. Values are initialized only through writable_pool: on
    // Prospero the two spans are separate aliases of the same JIT allocation.
    ankerl::unordered_dense::map<ConstantT, void*, ConstantHash> constant_info;
    std::span<ConstantT> executable_pool;
    std::span<ConstantT> writable_pool;
    BlockOfCode& code;
    std::size_t insertion_point;
};

}  // namespace Dynarmic::Backend::X64
