// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ps5/shader_cache_identity.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <openssl/evp.h>

#include "core/file_sys/vfs/vfs.h"

namespace Eden::PS5 {
namespace {

constexpr std::size_t HashChunkSize = 1024 * 1024;
constexpr u64 HomebrewNamespace = 1ULL << 63;

struct EvpContextDeleter {
    void operator()(EVP_MD_CTX* context) const noexcept {
        EVP_MD_CTX_free(context);
    }
};

} // namespace

std::optional<u64> ResolveShaderCacheIdentity(u64 program_id, const FileSys::VirtualFile& content) {
    if (program_id != 0) {
        return program_id;
    }
    if (!content || !content->IsReadable()) {
        return std::nullopt;
    }

    const std::unique_ptr<EVP_MD_CTX, EvpContextDeleter> context{EVP_MD_CTX_new()};
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        return std::nullopt;
    }

    std::vector<u8> chunk(HashChunkSize);
    const std::size_t content_size = content->GetSize();
    std::size_t offset = 0;
    while (offset < content_size) {
        const std::size_t requested = std::min(chunk.size(), content_size - offset);
        const std::size_t received = content->Read(chunk.data(), requested, offset);
        if (received == 0 || received > requested ||
            EVP_DigestUpdate(context.get(), chunk.data(), received) != 1) {
            return std::nullopt;
        }
        offset += received;
    }

    std::array<u8, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1 || digest_size != 32) {
        return std::nullopt;
    }

    u64 identity = 0;
    for (std::size_t index = 0; index < sizeof(identity); ++index) {
        identity = (identity << 8) | digest[index];
    }
    return (identity & ~HomebrewNamespace) | HomebrewNamespace;
}

} // namespace Eden::PS5
