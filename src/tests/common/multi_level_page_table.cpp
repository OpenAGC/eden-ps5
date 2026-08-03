// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "common/multi_level_page_table.h"

namespace {

constexpr std::size_t AddressSpaceBits = 20;
constexpr std::size_t FirstLevelBits = 4;
constexpr std::size_t PageBits = 12;
constexpr std::size_t EntriesPerLevel = 1u << (AddressSpaceBits - FirstLevelBits - PageBits);

} // namespace

TEST_CASE("MultiLevelPageTable: entries remain isolated across levels", "[common]") {
    Common::MultiLevelPageTable<u32> table(AddressSpaceBits, FirstLevelBits, PageBits);

    table.ReserveRange(0, 1u << (AddressSpaceBits - FirstLevelBits));
    table[0] = 0x12345678;
    table[EntriesPerLevel - 1] = 0xabcdef01;
    table[EntriesPerLevel] = 0x13572468;

    const auto& const_table = table;
    REQUIRE(const_table[0] == 0x12345678);
    REQUIRE(const_table[EntriesPerLevel - 1] == 0xabcdef01);
    REQUIRE(const_table[EntriesPerLevel] == 0x13572468);
    REQUIRE(const_table[EntriesPerLevel + 1] == 0);
}

TEST_CASE("MultiLevelPageTable: move transfers allocation ownership", "[common]") {
    Common::MultiLevelPageTable<u32> source(AddressSpaceBits, FirstLevelBits, PageBits);
    source[EntriesPerLevel * 2] = 0x24681357;

    Common::MultiLevelPageTable<u32> moved(std::move(source));
    REQUIRE(moved[EntriesPerLevel * 2] == 0x24681357);

    Common::MultiLevelPageTable<u32> assigned(AddressSpaceBits, FirstLevelBits, PageBits);
    assigned[0] = 1;
    assigned = std::move(moved);
    REQUIRE(assigned[EntriesPerLevel * 2] == 0x24681357);
}

TEST_CASE("MultiLevelPageTable: zero-size reserve is a no-op", "[common]") {
    Common::MultiLevelPageTable<u32> table(AddressSpaceBits, FirstLevelBits, PageBits);
    table.ReserveRange(1u << (AddressSpaceBits - FirstLevelBits), 0);
    const auto& const_table = table;
    REQUIRE(const_table[EntriesPerLevel] == 0);
}
