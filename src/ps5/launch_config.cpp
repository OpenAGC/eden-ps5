// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ps5/launch_config.h"

#include <array>
#include <cerrno>
#include <cstdio>

namespace Eden::PS5 {

LaunchConfigError ParseLaunchConfig(std::string_view text, LaunchConfig& config) {
    config = {};
    if (text.size() > MaxLaunchConfigBytes) {
        return LaunchConfigError::Oversized;
    }
    if (text == "init\n") {
        return LaunchConfigError::None;
    }
    constexpr std::string_view GamePrefix = "game\n";
    if (!text.starts_with(GamePrefix) || text.size() <= GamePrefix.size() + 1 ||
        text.back() != '\n') {
        return LaunchConfigError::Malformed;
    }

    const std::string_view path =
        text.substr(GamePrefix.size(), text.size() - GamePrefix.size() - 1);
    if (path.empty() || path.size() > MaxGamePathBytes || path.front() != '/') {
        return LaunchConfigError::Malformed;
    }
    for (const unsigned char byte : path) {
        if (byte < 0x20 || byte == 0x7f) {
            return LaunchConfigError::Malformed;
        }
    }

    config.mode = LaunchMode::Game;
    config.game_path.assign(path);
    return LaunchConfigError::None;
}

LaunchConfigError ReadLaunchConfigFile(std::string_view path, LaunchConfig& config) {
    config = {};
    const std::string path_string{path};
    std::FILE* file = std::fopen(path_string.c_str(), "rb");
    if (file == nullptr) {
        return errno == ENOENT ? LaunchConfigError::None : LaunchConfigError::Io;
    }

    std::array<char, MaxLaunchConfigBytes + 1> bytes{};
    const std::size_t size = std::fread(bytes.data(), 1, bytes.size(), file);
    const bool failed = std::ferror(file) != 0;
    const bool close_failed = std::fclose(file) != 0;
    if (failed || close_failed) {
        return LaunchConfigError::Io;
    }
    if (size > MaxLaunchConfigBytes) {
        return LaunchConfigError::Oversized;
    }
    return ParseLaunchConfig(std::string_view{bytes.data(), size}, config);
}

const char* LaunchConfigErrorName(LaunchConfigError error) {
    switch (error) {
    case LaunchConfigError::None:
        return "none";
    case LaunchConfigError::Io:
        return "io";
    case LaunchConfigError::Oversized:
        return "oversized";
    case LaunchConfigError::Malformed:
        return "malformed";
    }
    return "unknown";
}

} // namespace Eden::PS5
