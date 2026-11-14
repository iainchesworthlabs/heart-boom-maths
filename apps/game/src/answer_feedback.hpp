// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game_controller.hpp"

#include <QtQml/qqmlregistration.h>

#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace heart_boom_maths::game
{

/// Holds the look of the last answer for a moment. The core deals the next question at once, so without this the
/// answered tiles would be gone before the child saw them go green or red. The core keeps running meanwhile: nothing
/// here pauses or delays it. The play screen draws the held question while `active`, and the shell keeps the play
/// screen up until the hold ends when the answer was the last of a level or the bomb went off.
///
/// Not final: the QML engine derives from it. It reads the controller's `answered` data and the answer signals, and
/// works out no rule.
class AnswerFeedback : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(heart_boom_maths::game::GameController* game READ game WRITE set_game NOTIFY gameChanged FINAL)

    Q_PROPERTY(Kind kind READ kind NOTIFY kindChanged FINAL)
    Q_PROPERTY(bool active READ active NOTIFY kindChanged FINAL)

    // The held answer.
    Q_PROPERTY(int questionNumber READ question_number NOTIFY heldChanged FINAL)
    Q_PROPERTY(QStringList parts READ parts NOTIFY heldChanged FINAL)
    Q_PROPERTY(QList<int> tiles READ tiles NOTIFY heldChanged FINAL)
    Q_PROPERTY(int removedTiles READ removed_tiles NOTIFY heldChanged FINAL)
    Q_PROPERTY(int chosenTile READ chosen_tile NOTIFY heldChanged FINAL)
    Q_PROPERTY(int correctTile READ correct_tile NOTIFY heldChanged FINAL)
    Q_PROPERTY(int correctValue READ correct_value NOTIFY heldChanged FINAL)

    // What the answer did, for the floating tags.
    Q_PROPERTY(int points READ points NOTIFY heldChanged FINAL)
    Q_PROPERTY(int secondsDelta READ seconds_delta NOTIFY heldChanged FINAL)
    Q_PROPERTY(int multiplierBefore READ multiplier_before NOTIFY heldChanged FINAL)
    Q_PROPERTY(int multiplierAfter READ multiplier_after NOTIFY heldChanged FINAL)
    Q_PROPERTY(bool multiplierRose READ multiplier_rose NOTIFY heldChanged FINAL)

    // How long each kind is held, in milliseconds. The wrong answer's second is the one the spec asks for. A test sets
    // them to a few milliseconds.
    Q_PROPERTY(int rightHoldMs READ right_hold_ms WRITE set_right_hold_ms NOTIFY holdTimesChanged FINAL)
    Q_PROPERTY(int wrongHoldMs READ wrong_hold_ms WRITE set_wrong_hold_ms NOTIFY holdTimesChanged FINAL)
    Q_PROPERTY(int boomHoldMs READ boom_hold_ms WRITE set_boom_hold_ms NOTIFY holdTimesChanged FINAL)

    // NOLINTBEGIN(readability-identifier-naming): QML sees these names, so signals, invokables, enumerators and signal
    // parameters are camelCase and PascalCase (CLAUDE.md, Conventions).
public:
    enum class Kind
    {
        None,
        Right,
        Wrong,
        Boom,
    };
    Q_ENUM(Kind)

    /// Presentation times. The core does not know them: they are not part of the rules or of a replay.
    static constexpr int DEFAULT_RIGHT_HOLD_MS = 450;
    static constexpr int DEFAULT_WRONG_HOLD_MS = 1000; // SPEC: the correct tile is shown for 1 s
    static constexpr int DEFAULT_BOOM_HOLD_MS = 1200;

    explicit AnswerFeedback(QObject* parent = nullptr);

    [[nodiscard]] GameController* game() const;
    void set_game(GameController* game);

    [[nodiscard]] Kind kind() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] int question_number() const;
    [[nodiscard]] QStringList parts() const;
    [[nodiscard]] QList<int> tiles() const;
    [[nodiscard]] int removed_tiles() const;
    [[nodiscard]] int chosen_tile() const;
    [[nodiscard]] int correct_tile() const;
    [[nodiscard]] int correct_value() const;
    [[nodiscard]] int points() const;
    [[nodiscard]] int seconds_delta() const;
    [[nodiscard]] int multiplier_before() const;
    [[nodiscard]] int multiplier_after() const;
    [[nodiscard]] bool multiplier_rose() const;
    [[nodiscard]] int right_hold_ms() const;
    [[nodiscard]] int wrong_hold_ms() const;
    [[nodiscard]] int boom_hold_ms() const;
    void set_right_hold_ms(int ms);
    void set_wrong_hold_ms(int ms);
    void set_boom_hold_ms(int ms);

    /// Ends the hold now.
    Q_INVOKABLE void clear();

signals:
    void gameChanged();
    void kindChanged();
    void heldChanged();
    void holdTimesChanged();
    // NOLINTEND(readability-identifier-naming)

private:
    void on_correct(int tile, int points, int coins, int streak_bonus_coins, int run, int multiplier,
                    bool multiplier_rose, int seconds_added);
    void on_wrong(int tile, int correct_tile, int seconds_lost, int multiplier);
    void on_game_over(GameController::EndReason reason);
    void on_baseline_event();
    void hold(Kind kind, int ms);
    void read_answered();

    GameController* m_Game = nullptr;
    QTimer m_Timer;
    Kind m_Kind = Kind::None;

    int m_QuestionNumber = 0;
    QStringList m_Parts;
    QList<int> m_Tiles;
    int m_Removed = 0;
    int m_Chosen = -1;
    int m_Correct = -1;
    int m_Points = 0;
    int m_SecondsDelta = 0;
    int m_MultiplierBefore = 1;
    int m_MultiplierAfter = 1;
    bool m_MultiplierRose = false;
    int m_Baseline = 1; // the step before the next answer

    int m_RightHoldMs = DEFAULT_RIGHT_HOLD_MS;
    int m_WrongHoldMs = DEFAULT_WRONG_HOLD_MS;
    int m_BoomHoldMs = DEFAULT_BOOM_HOLD_MS;
};

} // namespace heart_boom_maths::game
