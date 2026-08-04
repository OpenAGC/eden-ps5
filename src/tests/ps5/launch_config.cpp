// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "ps5/launch_config.h"

TEST_CASE("PS5 launch configuration parses exact modes", "[ps5]") {
    Eden::PS5::LaunchConfig config{};

    REQUIRE(Eden::PS5::ParseLaunchConfig("init\n", config) == Eden::PS5::LaunchConfigError::None);
    REQUIRE(config.mode == Eden::PS5::LaunchMode::Init);
    REQUIRE(config.game_path.empty());

    REQUIRE(Eden::PS5::ParseLaunchConfig("game\n/data/homebrew/games/2048.nro\n", config) ==
            Eden::PS5::LaunchConfigError::None);
    REQUIRE(config.mode == Eden::PS5::LaunchMode::Game);
    REQUIRE(config.game_path == "/data/homebrew/games/2048.nro");
    REQUIRE(config.presented_frame_limit == 0);

    REQUIRE(Eden::PS5::ParseLaunchConfig("game\n/data/homebrew/games/2048.nro\nframes=600\n",
                                         config) == Eden::PS5::LaunchConfigError::None);
    REQUIRE(config.presented_frame_limit == 600);

    const std::string max_path = "/" + std::string(Eden::PS5::MaxGamePathBytes - 1, 'a');
    const std::string max_config = "game\n" + max_path + "\nframes=108000\n";
    REQUIRE(max_config.size() <= Eden::PS5::MaxLaunchConfigBytes);
    REQUIRE(Eden::PS5::ParseLaunchConfig(max_config, config) ==
            Eden::PS5::LaunchConfigError::None);
    REQUIRE(config.game_path == max_path);
    REQUIRE(config.presented_frame_limit == Eden::PS5::MaxPresentedFrameLimit);
}

TEST_CASE("PS5 launch configuration fails closed", "[ps5]") {
    Eden::PS5::LaunchConfig config{};
    for (const std::string_view malformed :
         {"", "init", "init\nextra\n", "game\n", "game\nrelative.nro\n", "game\n/data/game.nro\r\n",
          "game\n/data/game.nro\nframes=0\n", "game\n/data/game.nro\nframes=108001\n",
          "game\n/data/game.nro\nframes=60\nextra\n", "game\n/data/game.nro\ninput_cycle=1\n",
          "game\n/data/game.nro\ninput_cycle=1\nframes=60\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=0\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=10001\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=abc\n",
          "game\n/data/game.nro\nframes=60\ninput_profile=flappy\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=1\ninput_cycle=1\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=18\ninput_profile=generic\n",
          "game\n/data/game.nro\nframes=60\ninput_cycle=18\ninput_profile=flappy\nextra\n",
          "capture\n"}) {
        REQUIRE(Eden::PS5::ParseLaunchConfig(malformed, config) ==
                Eden::PS5::LaunchConfigError::Malformed);
    }

    std::string oversized(Eden::PS5::MaxLaunchConfigBytes + 1, 'x');
    REQUIRE(Eden::PS5::ParseLaunchConfig(oversized, config) ==
            Eden::PS5::LaunchConfigError::Oversized);
}

TEST_CASE("PS5 launch configuration file defaults and bounds", "[ps5]") {
    const auto path = std::filesystem::temp_directory_path() / "eden_ps5_launch_config_test";
    std::filesystem::remove(path);

    Eden::PS5::LaunchConfig config{};
    REQUIRE(Eden::PS5::ReadLaunchConfigFile(path.string(), config) ==
            Eden::PS5::LaunchConfigError::None);
    REQUIRE(config.mode == Eden::PS5::LaunchMode::Init);

    {
        std::ofstream file{path, std::ios::binary};
        file << "game\nrelative.nro\n";
    }
    REQUIRE(Eden::PS5::ReadLaunchConfigFile(path.string(), config) ==
            Eden::PS5::LaunchConfigError::Malformed);

    {
        std::ofstream file{path, std::ios::binary};
        file << std::string(Eden::PS5::MaxLaunchConfigBytes + 1, 'x');
    }
    REQUIRE(Eden::PS5::ReadLaunchConfigFile(path.string(), config) ==
            Eden::PS5::LaunchConfigError::Oversized);
    REQUIRE(std::filesystem::remove(path));
}
