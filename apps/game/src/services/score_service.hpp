// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace heart_boom_maths::game
{

/// Sends a finished score to a platform leaderboard. The local top 10 never depends on it. A failure is logged by the
/// implementation and never shown to the child.
class ScoreService
{
public:
    virtual ~ScoreService() = default;

    /// True when this platform has a leaderboard at all. The Game Center switch in Settings exists only then.
    [[nodiscard]] virtual bool available() const = 0;

    /// Follows the Game Center switch. An implementation stays idle (no sign-in sheet, no contact) while it is off,
    /// which is why the controller always calls submit() and does not decide this itself.
    virtual void set_enabled(bool enabled) = 0;

    /// One number, once per finished game. Nothing else about the player is ever sent.
    virtual void submit(std::int64_t score) = 0;
};

/// The service on every platform without a leaderboard.
class NullScoreService final : public ScoreService
{
public:
    [[nodiscard]] bool available() const override
    {
        return false;
    }

    void set_enabled(bool) override {}

    void submit(std::int64_t) override {}
};

} // namespace heart_boom_maths::game
