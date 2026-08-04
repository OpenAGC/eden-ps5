// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Eden::PS5 {

enum class QualificationInputProfile {
    Generic,
    Flappy,
};

enum class QualificationInputKey {
    A,
    B,
    Left,
    Up,
    Right,
    Down,
};

[[nodiscard]] constexpr const char* QualificationInputProfileName(
    QualificationInputProfile profile) noexcept {
    switch (profile) {
    case QualificationInputProfile::Generic:
        return "generic";
    case QualificationInputProfile::Flappy:
        return "flappy";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::uint64_t QualificationInputStepIntervalMs(
    QualificationInputProfile profile) noexcept {
    // A complete press/release takes two steps. Flappy's 450 ms steps create
    // a 900 ms flap cadence, safely longer than its 700 ms jump window.
    return profile == QualificationInputProfile::Flappy ? 450 : 250;
}

[[nodiscard]] constexpr QualificationInputKey QualificationInputKeyForPress(
    QualificationInputProfile profile, std::size_t press_index) noexcept {
    if (profile == QualificationInputProfile::Flappy) {
        return QualificationInputKey::A;
    }
    constexpr std::array GenericKeys{
        QualificationInputKey::A,  QualificationInputKey::A,     QualificationInputKey::A,
        QualificationInputKey::A,  QualificationInputKey::B,     QualificationInputKey::Left,
        QualificationInputKey::Up, QualificationInputKey::Right, QualificationInputKey::Down,
    };
    return GenericKeys[press_index % GenericKeys.size()];
}

[[nodiscard]] constexpr const char* QualificationInputKeyName(QualificationInputKey key) noexcept {
    switch (key) {
    case QualificationInputKey::A:
        return "A";
    case QualificationInputKey::B:
        return "B";
    case QualificationInputKey::Left:
        return "Left";
    case QualificationInputKey::Up:
        return "Up";
    case QualificationInputKey::Right:
        return "Right";
    case QualificationInputKey::Down:
        return "Down";
    }
    return "Unknown";
}

} // namespace Eden::PS5
