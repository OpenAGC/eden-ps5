// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "audio_core/sink/sink.h"
#include "audio_core/sink/sink_stream.h"

namespace Core {
class System;
} // namespace Core

namespace AudioCore::Sink {
class NullSinkStreamImpl final : public SinkStream {
public:
    explicit NullSinkStreamImpl(Core::System& system_, StreamType type_)
        : SinkStream{system_, type_} {}
    ~NullSinkStreamImpl() override {}
    void AppendBuffer(SinkBuffer&, std::span<s16>) override {}
    std::vector<s16> ReleaseBuffer(u64) override {
        return {};
    }
    void Start(bool = false) override {
        std::scoped_lock l{timing_lock};
        if (!paused) {
            return;
        }
        resume_time = std::chrono::steady_clock::now();
        paused = false;
    }
    void Stop() override {
        std::scoped_lock l{timing_lock};
        if (paused) {
            return;
        }
        played_frames += FramesSinceResume();
        paused = true;
    }
    u64 GetExpectedPlayedSampleCount() override {
        std::scoped_lock l{timing_lock};
        return played_frames + (paused ? 0 : FramesSinceResume());
    }

private:
    u64 FramesSinceResume() const {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - resume_time);
        return static_cast<u64>(elapsed.count()) * TargetSampleRate / 1'000'000'000;
    }

    std::mutex timing_lock;
    std::chrono::steady_clock::time_point resume_time{};
    u64 played_frames{};
};

/**
 * A no-op sink for when no audio out is wanted.
 */
class NullSink final : public Sink {
public:
    explicit NullSink(std::string_view) {}
    ~NullSink() override = default;

    SinkStream* AcquireSinkStream(Core::System& system, u32, const std::string&,
                                  StreamType type) override {
        if (null_sink == nullptr) {
            null_sink = std::make_unique<NullSinkStreamImpl>(system, type);
        }
        return null_sink.get();
    }

    void CloseStream(SinkStream*) override {}
    void CloseStreams() override {}
    f32 GetDeviceVolume() const override {
        return 1.0f;
    }
    void SetDeviceVolume(f32 volume) override {}
    void SetSystemVolume(f32 volume) override {}

private:
    SinkStreamPtr null_sink{};
};

} // namespace AudioCore::Sink
