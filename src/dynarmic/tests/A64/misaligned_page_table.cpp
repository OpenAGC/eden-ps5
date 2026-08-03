// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "dynarmic/tests/A64/testenv.h"
#include "dynarmic/tests/native/testenv.h"
#include "dynarmic/interface/A64/a64.h"

#if defined(ARCHITECTURE_x86_64)
namespace {

struct SparsePageEntry {
    std::uintptr_t raw{};
    std::array<std::uint64_t, 3> metadata{};
};
static_assert(sizeof(SparsePageEntry) == 32);

struct alignas(0x1000) SparsePage {
    std::array<std::uint8_t, 0x1000> bytes{};
};

class SparsePageTableTestEnv final : public A64TestEnv {
public:
    std::size_t read32_callbacks{};

    std::uint32_t MemoryRead32(std::uint64_t vaddr) override {
        ++read32_callbacks;
        return 0xa5a50000U | std::uint32_t(vaddr & 0xffff);
    }
};

struct SparsePageTableFixture {
    static constexpr std::size_t AddressSpaceBits = 16;
    static constexpr std::size_t LeafBits = 2;
    static constexpr std::size_t LeafEntries = 1U << LeafBits;
    static constexpr std::size_t RootEntries = 1U << (AddressSpaceBits - 12 - LeafBits);

    std::array<SparsePageEntry*, RootEntries> root{};
    std::array<std::array<SparsePageEntry, LeafEntries>, RootEntries> leaves{};

    void Map(std::uint64_t vaddr, SparsePage& page) {
        const std::size_t page_index = vaddr >> 12;
        const std::size_t root_index = page_index >> LeafBits;
        const std::size_t leaf_index = page_index & (LeafEntries - 1);
        root[root_index] = leaves[root_index].data();
        leaves[root_index][leaf_index].raw =
            (reinterpret_cast<std::uintptr_t>(page.bytes.data()) - (vaddr & ~0xfffULL)) | 1;
    }

    Dynarmic::A64::UserConfig Config(SparsePageTableTestEnv& env) {
        Dynarmic::A64::UserConfig conf{};
        conf.callbacks = &env;
        conf.page_table = reinterpret_cast<void**>(root.data());
        conf.page_table_address_space_bits = AddressSpaceBits;
        conf.page_table_pointer_mask_bits = 2;
        conf.page_table_log2_stride = 5;
        conf.sparse_page_table_leaf_bits = LeafBits;
        conf.absolute_offset_page_table = true;
        conf.detect_misaligned_access_via_page_table = 16 | 32 | 64 | 128;
        conf.only_detect_misalignment_via_page_table_on_page_boundary = true;
        return conf;
    }
};

void RunSparseLoadStore(SparsePageTableTestEnv& env, Dynarmic::A64::UserConfig conf,
                        std::uint64_t source, std::uint64_t destination) {
    Dynarmic::A64::Jit jit{conf};
    env.code_mem = {
        0xb9400001, // LDR W1, [X0]
        0xb9000041, // STR W1, [X2]
        0x14000000, // B .
    };
    jit.SetPC(0);
    jit.SetRegister(0, source);
    jit.SetRegister(2, destination);
    env.ticks_left = 3;
    CheckedRun([&] { jit.Run(); });
}

} // Anonymous namespace
#endif

TEST_CASE("misaligned load/store do not use page_table when detect_misaligned_access_via_page_table is set", "[a64]") {
    A64TestEnv env;
    Dynarmic::A64::UserConfig conf{};
    conf.callbacks = &env;
    conf.page_table = nullptr;
    conf.detect_misaligned_access_via_page_table = 128;
    conf.only_detect_misalignment_via_page_table_on_page_boundary = true;
    Dynarmic::A64::Jit jit{conf};

    env.code_mem.emplace_back(0x3c800400);  // STR Q0, [X0], #0
    env.code_mem.emplace_back(0x14000000);  // B .

    jit.SetPC(0);
    jit.SetRegister(0, 0x000000000b0afff8);

    env.ticks_left = 2;
    CheckedRun([&]() { jit.Run(); });

    // If we don't crash we're fine.
}

#if defined(ARCHITECTURE_x86_64)
TEST_CASE("A64 sparse page table resolves normal pages and falls back safely", "[a64]") {
    constexpr std::uint64_t SourcePage = 0x2000;
    constexpr std::uint64_t DestinationPage = 0x5000;
    constexpr std::uint64_t Source = SourcePage + 8;
    constexpr std::uint64_t Destination = DestinationPage + 12;

    SparsePageTableTestEnv env;
    SparsePageTableFixture table;
    SparsePage source_page;
    SparsePage destination_page;
    table.Map(SourcePage, source_page);
    table.Map(DestinationPage, destination_page);

    SECTION("mapped root and entry bypass callbacks") {
        constexpr std::uint32_t Expected = 0x1234abcd;
        std::memcpy(source_page.bytes.data() + 8, &Expected, sizeof(Expected));
        RunSparseLoadStore(env, table.Config(env), Source, Destination);

        std::uint32_t actual{};
        std::memcpy(&actual, destination_page.bytes.data() + 12, sizeof(actual));
        REQUIRE(actual == Expected);
        REQUIRE(env.read32_callbacks == 0);
    }

    SECTION("null root falls back to the read callback") {
        constexpr std::uint64_t Missing = 0x9004;
        RunSparseLoadStore(env, table.Config(env), Missing, Destination);

        std::uint32_t actual{};
        std::memcpy(&actual, destination_page.bytes.data() + 12, sizeof(actual));
        REQUIRE(actual == (0xa5a50000U | std::uint32_t(Missing)));
        REQUIRE(env.read32_callbacks == 1);
    }

    SECTION("null leaf entry falls back to the read callback") {
        constexpr std::uint64_t Missing = 0x3004;
        table.root[0] = table.leaves[0].data();
        RunSparseLoadStore(env, table.Config(env), Missing, Destination);

        std::uint32_t actual{};
        std::memcpy(&actual, destination_page.bytes.data() + 12, sizeof(actual));
        REQUIRE(actual == (0xa5a50000U | std::uint32_t(Missing)));
        REQUIRE(env.read32_callbacks == 1);
    }

    SECTION("special leaf entry falls back to the read callback") {
        constexpr std::uint64_t Special = 0x3004;
        table.root[0] = table.leaves[0].data();
        table.leaves[0][3].raw = 2;
        RunSparseLoadStore(env, table.Config(env), Special, Destination);

        std::uint32_t actual{};
        std::memcpy(&actual, destination_page.bytes.data() + 12, sizeof(actual));
        REQUIRE(actual == (0xa5a50000U | std::uint32_t(Special)));
        REQUIRE(env.read32_callbacks == 1);
    }

    SECTION("cross-page access falls back to the read callback") {
        constexpr std::uint64_t Boundary = SourcePage + 0xfff;
        RunSparseLoadStore(env, table.Config(env), Boundary, Destination);

        std::uint32_t actual{};
        std::memcpy(&actual, destination_page.bytes.data() + 12, sizeof(actual));
        REQUIRE(actual == (0xa5a50000U | std::uint32_t(Boundary)));
        REQUIRE(env.read32_callbacks == 1);
    }
}
#endif
