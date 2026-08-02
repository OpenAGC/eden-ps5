// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/page_table.h"
#include "common/virtual_buffer.h"
#include "ps5/runtime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace {

constexpr std::size_t AddressSpaceBits = 39;
constexpr std::size_t GuestPageBits = 12;
constexpr std::size_t EntryCount = std::size_t{1} << (AddressSpaceBits - GuestPageBits);
constexpr std::size_t FlexibleBytes = std::size_t{64} << 20;

bool ExerciseSparsePageTable() {
    Common::PageTable table;
    table.Resize(AddressSpaceBits, GuestPageBits);
    if (table.entries.size() != EntryCount || table.fastmem_arena != nullptr)
        return false;

    constexpr std::array<std::size_t, 7> Indices{
        0, 2047, 2048, 65536, EntryCount / 2, EntryCount - 2049, EntryCount - 1,
    };
    for (std::size_t slot = 0; slot < Indices.size(); ++slot) {
        auto& entry = table.entries[Indices[slot]];
        const std::uintptr_t pointer = 0x100000 + slot * 0x4000;
        entry.ptr.Store(pointer, Common::PageType::Memory);
        entry.block = 0x200000 + slot * 0x10000;
        entry.addr = 0x300000 + slot * 0x10000;
    }

    for (std::size_t slot = 0; slot < Indices.size(); ++slot) {
        const auto& entry = table.entries[Indices[slot]];
        const std::uintptr_t pointer = 0x100000 + slot * 0x4000;
        if (entry.ptr.Pointer() != pointer || entry.ptr.Type() != Common::PageType::Memory ||
            entry.block != 0x200000 + slot * 0x10000 || entry.addr != 0x300000 + slot * 0x10000) {
            return false;
        }
    }

    const auto& untouched = table.entries[4096];
    return untouched.ptr.Raw() == 0 && untouched.block == 0 && untouched.addr == 0;
}

bool ExerciseFlexibleVirtualBuffer() {
    constexpr std::size_t Count = FlexibleBytes / sizeof(std::uint32_t);
    Common::VirtualBuffer<std::uint32_t> buffer{Count};
    buffer[0] = 0x11223344;
    buffer[Count / 2] = 0x55667788;
    buffer[Count - 1] = 0x99aabbcc;
    if (buffer[0] != 0x11223344 || buffer[Count / 2] != 0x55667788 ||
        buffer[Count - 1] != 0x99aabbcc) {
        return false;
    }

    Common::VirtualBuffer<std::uint32_t> moved{std::move(buffer)};
    if (buffer.data() != nullptr || moved.size() != Count || moved[Count - 1] != 0x99aabbcc)
        return false;

    moved.resize(0x10000 / sizeof(std::uint32_t));
    moved[0] = 0xa5a5a5a5;
    moved[moved.size() - 1] = 0x5a5a5a5a;
    return moved[0] == 0xa5a5a5a5 && moved[moved.size() - 1] == 0x5a5a5a5a;
}

} // namespace

int main() {
    const bool passed = ExerciseSparsePageTable() && ExerciseFlexibleVirtualBuffer();
    std::printf("eden-ps5 virtual-buffer probe: %s entries=0x%zx flexible=0x%zx\n",
                passed ? "PASS" : "FAIL", EntryCount, FlexibleBytes);
    std::fflush(nullptr);
    Eden::PS5::TerminateApplication(passed ? 0 : 1);
}
