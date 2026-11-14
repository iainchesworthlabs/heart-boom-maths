// SPDX-License-Identifier: GPL-3.0-or-later
#include "answer_feedback.hpp"

#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

namespace heart_boom_maths::game
{

AnswerFeedback::AnswerFeedback(QObject* parent) : QObject(parent)
{
    m_Timer.setSingleShot(true);
    m_Timer.setTimerType(Qt::PreciseTimer);
    connect(&m_Timer, &QTimer::timeout, this, &AnswerFeedback::clear);
}

GameController* AnswerFeedback::game() const
{
    return m_Game;
}

void AnswerFeedback::set_game(GameController* game)
{
    if (game == m_Game)
    {
        return;
    }
    if (m_Game != nullptr)
    {
        m_Game->disconnect(this);
    }
    clear();
    m_Game = game;
    if (m_Game != nullptr)
    {
        connect(m_Game, &GameController::answerCorrect, this, &AnswerFeedback::on_correct);
        connect(m_Game, &GameController::answerWrong, this, &AnswerFeedback::on_wrong);
        connect(m_Game, &GameController::gameOver, this,
                [this](GameController::EndReason reason, qint64, bool, int) { on_game_over(reason); });
        // A new level or game and a restored run start from the step on screen, and a new level ends any hold.
        connect(m_Game, &GameController::levelStarted, this,
                [this](int)
                {
                    clear();
                    on_baseline_event();
                });
        connect(m_Game, &GameController::stateChanged, this, &AnswerFeedback::on_baseline_event);
        on_baseline_event();
    }
    emit gameChanged();
}

AnswerFeedback::Kind AnswerFeedback::kind() const
{
    return m_Kind;
}

bool AnswerFeedback::active() const
{
    return m_Kind != Kind::None;
}

int AnswerFeedback::question_number() const
{
    return m_QuestionNumber;
}

QStringList AnswerFeedback::parts() const
{
    return m_Parts;
}

QList<int> AnswerFeedback::tiles() const
{
    return m_Tiles;
}

int AnswerFeedback::removed_tiles() const
{
    return m_Removed;
}

int AnswerFeedback::chosen_tile() const
{
    return m_Chosen;
}

int AnswerFeedback::correct_tile() const
{
    return m_Correct;
}

int AnswerFeedback::correct_value() const
{
    return m_Correct >= 0 && m_Correct < m_Tiles.size() ? m_Tiles.at(m_Correct) : 0;
}

int AnswerFeedback::points() const
{
    return m_Points;
}

int AnswerFeedback::seconds_delta() const
{
    return m_SecondsDelta;
}

int AnswerFeedback::multiplier_before() const
{
    return m_MultiplierBefore;
}

int AnswerFeedback::multiplier_after() const
{
    return m_MultiplierAfter;
}

bool AnswerFeedback::multiplier_rose() const
{
    return m_MultiplierRose;
}

int AnswerFeedback::right_hold_ms() const
{
    return m_RightHoldMs;
}

int AnswerFeedback::wrong_hold_ms() const
{
    return m_WrongHoldMs;
}

int AnswerFeedback::boom_hold_ms() const
{
    return m_BoomHoldMs;
}

void AnswerFeedback::set_right_hold_ms(int ms)
{
    if (ms != m_RightHoldMs)
    {
        m_RightHoldMs = ms;
        emit holdTimesChanged();
    }
}

void AnswerFeedback::set_wrong_hold_ms(int ms)
{
    if (ms != m_WrongHoldMs)
    {
        m_WrongHoldMs = ms;
        emit holdTimesChanged();
    }
}

void AnswerFeedback::set_boom_hold_ms(int ms)
{
    if (ms != m_BoomHoldMs)
    {
        m_BoomHoldMs = ms;
        emit holdTimesChanged();
    }
}

void AnswerFeedback::clear()
{
    m_Timer.stop();
    if (m_Kind == Kind::None)
    {
        return;
    }
    m_Kind = Kind::None;
    emit kindChanged();
}

void AnswerFeedback::on_correct(int tile, int points, int /*coins*/, int /*streak_bonus_coins*/, int /*run*/,
                                int multiplier, bool multiplier_rose, int seconds_added)
{
    read_answered();
    m_Chosen = tile;
    m_Correct = tile;
    m_Points = points;
    m_SecondsDelta = seconds_added;
    m_MultiplierBefore = m_Baseline;
    m_MultiplierAfter = multiplier;
    m_MultiplierRose = multiplier_rose;
    m_Baseline = multiplier;
    emit heldChanged();
    hold(Kind::Right, m_RightHoldMs);
}

void AnswerFeedback::on_wrong(int tile, int correct_tile, int seconds_lost, int multiplier)
{
    read_answered();
    m_Chosen = tile;
    m_Correct = correct_tile;
    m_Points = 0;
    m_SecondsDelta = -seconds_lost;
    m_MultiplierBefore = m_Baseline;
    m_MultiplierAfter = multiplier;
    m_MultiplierRose = false;
    m_Baseline = multiplier;
    emit heldChanged();
    hold(Kind::Wrong, m_WrongHoldMs);
}

void AnswerFeedback::on_game_over(GameController::EndReason reason)
{
    // Only the bomb gets its pop; QUIT and FINISH go straight to the game-over card, so the look of an answer given
    // just before the pause must not keep the play screen up.
    if (reason == GameController::EndReason::Bomb)
    {
        hold(Kind::Boom, m_BoomHoldMs);
    }
    else
    {
        clear();
    }
}

void AnswerFeedback::on_baseline_event()
{
    if (m_Game != nullptr)
    {
        m_Baseline = m_Game->multiplier();
    }
}

void AnswerFeedback::hold(Kind kind, int ms)
{
    const bool changed = kind != m_Kind;
    m_Kind = kind;
    m_Timer.start(ms);
    if (changed)
    {
        emit kindChanged();
    }
}

void AnswerFeedback::read_answered()
{
    const QVariantMap held = m_Game->answered();
    m_QuestionNumber = held.value(QStringLiteral("questionNumber")).toInt();
    m_Parts = held.value(QStringLiteral("parts")).toStringList();
    m_Tiles.clear();
    const QVariantList tiles = held.value(QStringLiteral("tiles")).toList();
    for (const QVariant& tile : tiles)
    {
        m_Tiles.append(tile.toInt());
    }
    m_Removed = held.value(QStringLiteral("removedTiles")).toInt();
}

} // namespace heart_boom_maths::game
