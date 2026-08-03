// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs_types.h"

namespace Eden::PS5 {

/// Keeps real title IDs unchanged and derives a path-independent cache namespace for homebrew.
[[nodiscard]] std::optional<u64> ResolveShaderCacheIdentity(u64 program_id,
                                                            const FileSys::VirtualFile& content);

} // namespace Eden::PS5
