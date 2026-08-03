// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <vector>

#include "core/file_sys/vfs/vfs_vector.h"
#include "ps5/shader_cache_identity.h"

namespace {

class ShortReadVfsFile final : public FileSys::VectorVfsFile {
public:
    using VectorVfsFile::VectorVfsFile;

    std::size_t Read(u8* data, std::size_t length, std::size_t offset) const override {
        return VectorVfsFile::Read(data, std::min<std::size_t>(length, 1), offset);
    }
};

class StalledVfsFile final : public FileSys::VectorVfsFile {
public:
    using VectorVfsFile::VectorVfsFile;

    std::size_t Read(u8*, std::size_t, std::size_t) const override {
        return 0;
    }
};

} // namespace

TEST_CASE("PS5 shader cache identity preserves real title IDs", "[ps5]") {
    constexpr u64 program_id = 0x0100123456789000;
    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(program_id, nullptr) == program_id);
}

TEST_CASE("PS5 homebrew shader cache identity uses stable SHA-256 namespace", "[ps5]") {
    const auto empty = std::make_shared<FileSys::VectorVfsFile>(std::vector<u8>{}, "empty.nro");
    const auto abc =
        std::make_shared<FileSys::VectorVfsFile>(std::vector<u8>{'a', 'b', 'c'}, "first.nro");
    const auto renamed =
        std::make_shared<FileSys::VectorVfsFile>(std::vector<u8>{'a', 'b', 'c'}, "second.nro");
    const auto changed =
        std::make_shared<FileSys::VectorVfsFile>(std::vector<u8>{'a', 'b', 'd'}, "first.nro");

    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(0, empty) == 0xe3b0c44298fc1c14ULL);
    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(0, abc) == 0xba7816bf8f01cfeaULL);
    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(0, renamed) ==
            Eden::PS5::ResolveShaderCacheIdentity(0, abc));
    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(0, changed) !=
            Eden::PS5::ResolveShaderCacheIdentity(0, abc));
    REQUIRE((*Eden::PS5::ResolveShaderCacheIdentity(0, changed) & (1ULL << 63)) != 0);
}

TEST_CASE("PS5 homebrew shader cache identity rejects missing content", "[ps5]") {
    REQUIRE_FALSE(Eden::PS5::ResolveShaderCacheIdentity(0, nullptr));

    const auto stalled = std::make_shared<StalledVfsFile>(std::vector<u8>{1}, "stalled.nro");
    REQUIRE_FALSE(Eden::PS5::ResolveShaderCacheIdentity(0, stalled));
}

TEST_CASE("PS5 homebrew shader cache identity accepts short reads", "[ps5]") {
    const auto short_read =
        std::make_shared<ShortReadVfsFile>(std::vector<u8>{'a', 'b', 'c'}, "short.nro");
    REQUIRE(Eden::PS5::ResolveShaderCacheIdentity(0, short_read) == 0xba7816bf8f01cfeaULL);
}
