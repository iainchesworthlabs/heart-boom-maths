// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/game_center_score_service.hpp"

#include <memory>
#include <utility>

namespace heart_boom_maths::game
{

GameCenterScoreService::GameCenterScoreService(std::unique_ptr<GameCenterBackend> backend,
                                               std::string_view leaderboard_id)
    : m_Backend(std::move(backend)), m_LeaderboardId(leaderboard_id)
{
}

bool GameCenterScoreService::available() const
{
    return true;
}

void GameCenterScoreService::set_enabled(bool enabled)
{
    if (enabled == m_State->enabled)
    {
        return;
    }
    m_State->enabled = enabled;
    // A new generation for every change of the switch, so a status that was already on its way when the switch moved is
    // not taken for an answer to the new position.
    const std::uint64_t generation = ++m_State->generation;
    m_State->status = {};

    if (!enabled)
    {
        m_Backend->stop_authentication();
        return;
    }
    m_Backend->start_authentication(
        [state = std::weak_ptr<State>(m_State), generation](GameCenterStatus status)
        {
            const std::shared_ptr<State> live = state.lock();
            if (live && live->enabled && live->generation == generation)
            {
                live->status = status;
            }
        });
}

void GameCenterScoreService::submit(std::int64_t score)
{
    const bool may_send = m_State->enabled && m_State->status.authenticated && !m_State->status.restricted;
    // No score is negative; the leaderboard would take one, but it could only come from a bug.
    if (!may_send || score < 0)
    {
        return;
    }
    m_Backend->submit_score(score, m_LeaderboardId);
}

} // namespace heart_boom_maths::game
