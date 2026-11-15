// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Recording stand-ins for the platform services, so a test reads what the controller asked for.

#include "heart_boom_maths/save_data.hpp"
#include "services/audio_service.hpp"
#include "services/backed_haptics_service.hpp"
#include "services/clock.hpp"
#include "services/haptic_backend.hpp"
#include "services/haptic_timer.hpp"
#include "services/haptics_service.hpp"
#include "services/link_opener.hpp"
#include "services/score_service.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
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

/// The HapticsService as a test sees it from the controller: `asked` is every request, `played` only the ones a real
/// service would let through (the switch is on and the game is not paused or in the background).
class RecordingHaptics final : public HapticsService
{
public:
    explicit RecordingHaptics(bool is_available = true) : m_Available(is_available) {}

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void play(HapticId id) override
    {
        asked.push_back(id);
        if (enabled && !suspended)
        {
            played.push_back(id);
        }
    }

    void set_enabled(bool on) override
    {
        enabled = on;
    }

    void suspend() override
    {
        suspended = true;
        ++suspends;
    }

    void resume() override
    {
        suspended = false;
        ++resumes;
    }

    [[nodiscard]] int count(HapticId id) const
    {
        return static_cast<int>(std::count(played.begin(), played.end(), id));
    }

    std::vector<HapticId> asked;
    std::vector<HapticId> played;
    bool enabled = true;
    bool suspended = false;
    int suspends = 0;
    int resumes = 0;

private:
    bool m_Available;
};

/// One beat that reached the device, and when (milliseconds on the test clock).
struct FiredBeat
{
    HapticId id;
    std::int64_t at_ms;

    friend bool operator==(const FiredBeat&, const FiredBeat&) = default;
};

class RecordingHapticBackend final : public HapticBackend
{
public:
    explicit RecordingHapticBackend(const Clock& clock, bool is_available = true)
        : m_Clock(clock), m_Available(is_available)
    {
    }

    [[nodiscard]] bool available() const override
    {
        return m_Available;
    }

    void fire(HapticId beat) override
    {
        fired.push_back({beat, m_Clock.now_ns() / 1'000'000});
    }

    void cancel() override
    {
        ++cancels;
    }

    [[nodiscard]] std::vector<HapticId> ids() const
    {
        std::vector<HapticId> out;
        for (const FiredBeat& beat : fired)
        {
            out.push_back(beat.id);
        }
        return out;
    }

    std::vector<FiredBeat> fired;
    int cancels = 0;

private:
    const Clock& m_Clock;
    bool m_Available;
};

/// A single-shot timer that fires when the test moves the clock past it (see HapticRig::advance_ms).
class ManualHapticTimer final : public HapticTimer
{
public:
    explicit ManualHapticTimer(const Clock& clock) : m_Clock(clock) {}

    void start(int delay_ms, std::function<void()> on_fire) override
    {
        m_Callback = std::move(on_fire);
        m_DueMs = m_Clock.now_ns() / 1'000'000 + delay_ms;
        ++starts;
    }

    void stop() override
    {
        m_Callback = nullptr;
    }

    [[nodiscard]] bool armed() const
    {
        return static_cast<bool>(m_Callback);
    }

    [[nodiscard]] std::int64_t due_ms() const
    {
        return m_DueMs;
    }

    /// Fires what is pending, once. The callback may arm the timer again.
    void fire()
    {
        auto callback = std::move(m_Callback);
        m_Callback = nullptr;
        if (callback)
        {
            callback();
        }
    }

    int starts = 0;

private:
    const Clock& m_Clock;
    std::function<void()> m_Callback;
    std::int64_t m_DueMs = 0;
};

/// The real haptic rules over a recording device, a hand-driven timer and a clock that moves only when told to.
struct HapticRig
{
    explicit HapticRig(bool device_can = true)
        : backend(clock, device_can), timer(clock), service(backend, clock, timer)
    {
    }

    /// Moves time on, firing the timer at each moment it falls due on the way.
    void advance_ms(std::int64_t milliseconds)
    {
        const std::int64_t target = clock.now_ns() / 1'000'000 + milliseconds;
        while (timer.armed() && timer.due_ms() <= target)
        {
            const std::int64_t now = clock.now_ns() / 1'000'000;
            if (timer.due_ms() > now)
            {
                clock.advance_ms(timer.due_ms() - now);
            }
            timer.fire();
        }
        const std::int64_t now = clock.now_ns() / 1'000'000;
        if (target > now)
        {
            clock.advance_ms(target - now);
        }
    }

    ManualClock clock;
    RecordingHapticBackend backend;
    ManualHapticTimer timer;
    BackedHapticsService service;
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

// ---- Save data the way the game keeps it ----------------------------------------------------------------------------
// The tests build a first-run save and give it what they need. The coins go under the empty device, as a version 1 save
// had them, and the controller gives them to its own device id when it loads.

/// The wallet of the current player.
inline std::int64_t wallet_of(const heart_boom_maths::SaveData& data)
{
    const heart_boom_maths::Player* player = heart_boom_maths::current_player(data);
    return player != nullptr ? heart_boom_maths::player_wallet(*player) : 0;
}

/// Sets the wallet of the current player.
inline void give_wallet(heart_boom_maths::SaveData& data, std::int64_t coins)
{
    heart_boom_maths::Player* player = heart_boom_maths::current_player(data);
    if (player != nullptr)
    {
        (void)heart_boom_maths::record_wallet(*player, data.device.id, coins);
    }
}

/// Puts a top 10 in the save, numbered in rank order, and the best score of the current player to match.
inline void give_top10(heart_boom_maths::SaveData& data, std::vector<heart_boom_maths::ScoreEntry> entries)
{
    std::int64_t seq = 1;
    for (heart_boom_maths::ScoreEntry& entry : entries)
    {
        entry.device = data.device.id;
        entry.seq = seq++;
    }
    data.device.next_seq = seq;
    data.shared.top10 = std::move(entries);
    if (heart_boom_maths::Player* player = heart_boom_maths::current_player(data); player != nullptr)
    {
        player->best = heart_boom_maths::best_score(data.shared.top10);
    }
}

/// A saved run, paused by the current player.
inline void give_run(heart_boom_maths::SaveData& data, std::vector<std::int64_t> values)
{
    data.device.run = std::move(values);
    data.device.run_player = data.device.current_player;
}

} // namespace hbm_test
