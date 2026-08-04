// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ps5/qualification_input.h"

namespace Eden::PS5 {

constexpr std::size_t MaxLaunchConfigBytes = 1083;
constexpr std::size_t MaxGamePathBytes = 1024;
constexpr std::uint32_t MaxPresentedFrameLimit = 108000;
constexpr std::uint32_t MaxQualificationInputPressLimit = 10000;
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
    std::uint32_t presented_frame_limit = 0;
    bool qualification_input_cycle = false;
    std::uint32_t qualification_input_press_limit = 0;
    QualificationInputProfile qualification_input_profile = QualificationInputProfile::Generic;
};

LaunchConfigError ParseLaunchConfig(std::string_view text, LaunchConfig& config);
LaunchConfigError ReadLaunchConfigFile(std::string_view path, LaunchConfig& config);
const char* LaunchConfigErrorName(LaunchConfigError error);

} // namespace Eden::PS5
