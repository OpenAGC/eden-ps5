// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Eden::PS5 {

constexpr std::size_t MaxLaunchConfigBytes = 1032;
constexpr std::size_t MaxGamePathBytes = 1024;
constexpr std::string_view DefaultLaunchConfigPath = "/data/homebrew/eden_ps5/eden.launch";

enum class LaunchMode {
    Init,
    Game,
};

enum class LaunchConfigError {
    None,
    Io,
    Oversized,
    Malformed,
};

struct LaunchConfig {
    LaunchMode mode = LaunchMode::Init;
    std::string game_path;
};

LaunchConfigError ParseLaunchConfig(std::string_view text, LaunchConfig& config);
LaunchConfigError ReadLaunchConfigFile(std::string_view path, LaunchConfig& config);
const char* LaunchConfigErrorName(LaunchConfigError error);

} // namespace Eden::PS5
