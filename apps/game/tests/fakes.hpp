// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Recording stand-ins for the platform services, so a test reads what the controller asked for.

#include "services/audio_service.hpp"
#include "services/link_opener.hpp"
#include "services/score_service.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace hbm_test
{

using namespace heart_boom_maths::game;

class RecordingAudio final : public AudioService
{
public:
    void play_effect(SoundId id) override
    {
        effects.push_back(id);
    }

    void stop_effect(SoundId id) override
    {
        stopped.push_back(id);
    }

    void play_music(MusicId id) override
    {
        music.push_back(id);
    }

    void set_enabled(bool effects_on, bool music_on) override
    {
        effects_enabled = effects_on;
        music_enabled = music_on;
    }

    void suspend() override
    {
        ++suspends;
    }

    void resume() override
    {
        ++resumes;
    }

    void unlock() override
    {
        ++unlocks;
    }

    [[nodiscard]] int count(SoundId id) const
    {
        return static_cast<int>(std::count(effects.begin(), effects.end(), id));
    }

    std::vector<SoundId> effects;
    std::vector<SoundId> stopped;
    std::vector<MusicId> music;
    bool effects_enabled = true;
    bool music_enabled = true;
    int suspends = 0;
    int resumes = 0;
    int unlocks = 0;
};

class RecordingLinks final : public LinkOpener
{
public:
    void open(const std::string& url) override
    {
        opened.push_back(url);
    }

    std::vector<std::string> opened;
};

class RecordingScores final : public ScoreService
{
public:
    explicit RecordingScores(bool is_available = false) : m_Available(is_available) {}

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void set_enabled(bool on) override
    {
        enabled = on;
    }

    void submit(std::int64_t score) override
    {
        submitted.push_back(score);
    }

    bool enabled = false;
    std::vector<std::int64_t> submitted;

private:
    bool m_Available;
};

} // namespace hbm_test
