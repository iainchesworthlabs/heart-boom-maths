// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "services/game_center_backend.hpp"
#include "services/score_service.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace heart_boom_maths::game
{

/// The identifier of the leaderboard the score goes to. A PLACEHOLDER: no leaderboard of this name exists. The owner
/// creates the leaderboard in App Store Connect (Game Center, Leaderboards) and replaces this one constant with its ID.
/// Until then a submission fails at Apple's end and the backend logs and swallows it (docs/ios.md).
inline constexpr std::string_view GAME_CENTER_LEADERBOARD_ID =
    "com.iainchesworthlabs.heartboommaths.highscore.placeholder";

/// The score service of an Apple device. The rules live here, in plain C++; GameKit sits behind GameCenterBackend.
///
///  - It is idle until the Settings switch is on: no sign-in handler is installed, so a default install never shows a
///    sign-in sheet and never contacts Game Center.
///  - Turning the switch on installs the handler once; turning it off removes it, and anything the platform reports
///    afterwards is ignored.
///  - submit() sends one number when the switch is on, a player is signed in and the account is not restricted. A score
///    asked for at any other time is dropped, not queued: a score must not leave the device later than the game over it
///    belongs to. The controller calls submit() once per finished game, so this class does not filter duplicates (two
///    games can end on the same score).
class GameCenterScoreService final : public ScoreService
{
public:
    explicit GameCenterScoreService(std::unique_ptr<GameCenterBackend> backend,
                                    std::string_view leaderboard_id = GAME_CENTER_LEADERBOARD_ID);

    [[nodiscard]] bool available() const override;
    void set_enabled(bool enabled) override;
    void submit(std::int64_t score) override;

private:
    /// Shared with the callback given to the backend, which may outlive the switch position it was made for.
    struct State
    {
        bool enabled = false;
        std::uint64_t generation = 0;
        GameCenterStatus status;
    };

    std::unique_ptr<GameCenterBackend> m_Backend;
    std::string m_LeaderboardId;
    std::shared_ptr<State> m_State = std::make_shared<State>();
};

} // namespace heart_boom_maths::game
