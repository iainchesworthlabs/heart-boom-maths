// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace heart_boom_maths::game
{

/// What the platform last said about the Game Center account of this device. Nothing about the player is carried: no
/// alias, no friends, no avatar, no identifier.
struct GameCenterStatus
{
    /// A Game Center player is signed in on this device.
    bool authenticated = false;
    /// Parental controls or Screen Time block Game Center for this account.
    bool restricted = false;
};

/// The thin layer over GameKit: it installs and removes the sign-in handler and sends one number, and knows nothing
/// about the Settings switch or when a score may go. GameCenterScoreService decides that; a backend only does it. The
/// Objective-C++ backend is the one real implementation (platform/ios), and the tests use a recording one, so the rules
/// need no Apple device.
class GameCenterBackend
{
public:
    using StatusCallback = std::function<void(GameCenterStatus)>;

    virtual ~GameCenterBackend() = default;

    /// Installs the sign-in handler. This is the only call that can make the system show a sign-in sheet, which is why
    /// the service makes it only after the Settings switch is turned on. The callback runs on the thread that owns the
    /// application's objects, once for every change of status, and may run after stop_authentication() returned.
    virtual void start_authentication(StatusCallback on_status) = 0;

    /// Removes the sign-in handler.
    virtual void stop_authentication() = 0;

    /// Sends the score to the leaderboard and shows nothing. A failure is logged and swallowed by the backend.
    virtual void submit_score(std::int64_t score, std::string_view leaderboard_id) = 0;
};

} // namespace heart_boom_maths::game
