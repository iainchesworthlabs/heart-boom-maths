// SPDX-License-Identifier: GPL-3.0-or-later
#include "answer_feedback.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QTest>

using namespace heart_boom_maths::game;
using namespace hbm_test;
using Kind = AnswerFeedback::Kind;

namespace
{

constexpr int SHORT_HOLD_MS = 30;
constexpr int WAIT_PAST_HOLD_MS = 120;

/// A rig with a started game and a feedback object watching it, holds shortened so a test can wait them out.
struct Held
{
    Held()
    {
        feedback.set_game(&rig.controller);
        feedback.set_right_hold_ms(SHORT_HOLD_MS);
        feedback.set_wrong_hold_ms(SHORT_HOLD_MS);
        feedback.set_boom_hold_ms(SHORT_HOLD_MS);
        rig.controller.startGame();
    }

    /// The question as it stood before the tap: what the held look must show.
    [[nodiscard]] QStringList parts_now() const
    {
        return rig.controller.question_parts();
    }

    UiRig rig;
    AnswerFeedback feedback;
};

} // namespace

TEST_CASE("A wrong answer is held with the question it was given to", "[unit][feedback]")
{
    Held h;
    const QStringList before = h.parts_now();
    const QList<int> tiles_before = h.rig.controller.tiles();
    const int correct = h.rig.controller.session_view().question.correct_tile;
    QSignalSpy kind_changed(&h.feedback, &AnswerFeedback::kindChanged);

    h.rig.answer_wrong();

    CHECK(h.feedback.kind() == Kind::Wrong);
    CHECK(h.feedback.active());
    CHECK(h.feedback.parts() == before);
    CHECK(h.feedback.tiles() == tiles_before);
    CHECK(h.feedback.correct_tile() == correct);
    CHECK(h.feedback.chosen_tile() != correct);
    CHECK(h.feedback.correct_value() == tiles_before.at(correct));
    CHECK(h.feedback.seconds_delta() == -h.rig.controller.seconds_per_wrong_answer());
    CHECK(h.feedback.points() == 0);
    CHECK(h.feedback.question_number() == 1);
    // The core has already moved on, and the hold does not hold it back.
    CHECK(h.rig.controller.question_number() == 2);
    CHECK(h.rig.controller.question_parts() != before);
    CHECK(kind_changed.count() == 1);
}

TEST_CASE("A hold ends by itself and the held data stays readable", "[unit][feedback]")
{
    Held h;
    h.rig.answer_wrong();
    REQUIRE(h.feedback.active());

    QTest::qWait(WAIT_PAST_HOLD_MS);

    CHECK(h.feedback.kind() == Kind::None);
    CHECK_FALSE(h.feedback.active());
}

TEST_CASE("A right answer is held with the tile that was tapped and its points", "[unit][feedback]")
{
    Held h;
    h.rig.play_for_ms(10000); // so the answer has room to add time
    const int correct = h.rig.controller.session_view().question.correct_tile;
    const QList<int> tiles_before = h.rig.controller.tiles();

    h.rig.answer_right();

    CHECK(h.feedback.kind() == Kind::Right);
    CHECK(h.feedback.chosen_tile() == correct);
    CHECK(h.feedback.correct_tile() == correct);
    CHECK(h.feedback.tiles() == tiles_before);
    CHECK(h.feedback.points() == 10);
    CHECK(h.feedback.seconds_delta() == h.rig.controller.seconds_per_right_answer());
    CHECK_FALSE(h.feedback.multiplier_rose());
    CHECK(h.feedback.multiplier_before() == 1);
    CHECK(h.feedback.multiplier_after() == 1);
}

TEST_CASE("The step rising and falling is reported with its value before the answer", "[unit][feedback]")
{
    Held h;
    h.rig.answer_right();
    h.rig.answer_right();
    h.rig.answer_right(); // a run of 3 lifts the step to 2
    CHECK(h.feedback.multiplier_rose());
    CHECK(h.feedback.multiplier_before() == 1);
    CHECK(h.feedback.multiplier_after() == 2);

    h.rig.answer_wrong();
    CHECK(h.feedback.kind() == Kind::Wrong);
    CHECK(h.feedback.multiplier_before() == 2);
    CHECK(h.feedback.multiplier_after() == 1);
    CHECK_FALSE(h.feedback.multiplier_rose());

    h.rig.answer_wrong(); // already at the bottom: no fall to show
    CHECK(h.feedback.multiplier_before() == 1);
    CHECK(h.feedback.multiplier_after() == 1);
}

TEST_CASE("A hint's removed tiles are held with the answer", "[unit][feedback]")
{
    Held h;
    h.rig.answer_right();
    h.rig.answer_right(); // two coins
    REQUIRE(h.rig.controller.hint_available());
    h.rig.controller.useHint();
    const int removed = h.rig.controller.removed_tiles();
    REQUIRE(removed != 0);

    h.rig.answer_right();

    CHECK(h.feedback.removed_tiles() == removed);
    CHECK(h.rig.controller.removed_tiles() == 0); // the next question has none gone
}

TEST_CASE("The bomb is held as a boom, QUIT is not", "[unit][feedback]")
{
    SECTION("the bomb")
    {
        Held h;
        h.rig.play_for_ms(61000);
        REQUIRE(h.rig.controller.state() == GameController::State::GameOver);
        CHECK(h.feedback.kind() == Kind::Boom);
        QTest::qWait(WAIT_PAST_HOLD_MS);
        CHECK(h.feedback.kind() == Kind::None);
    }
    SECTION("quit")
    {
        Held h;
        h.rig.controller.pause();
        h.rig.controller.quit();
        REQUIRE(h.rig.controller.state() == GameController::State::GameOver);
        CHECK(h.feedback.kind() == Kind::None);
    }
}

TEST_CASE("QUIT during the hold of an answer ends the hold at once", "[unit][feedback][journey]")
{
    Held h;
    h.feedback.set_wrong_hold_ms(60000);
    h.rig.answer_wrong();
    REQUIRE(h.feedback.kind() == Kind::Wrong);

    // The pause button is tapped while the wrong tile is still shown, then QUIT: the name or game-over screen must not
    // wait for a hold that belongs to a game that has ended.
    h.rig.controller.pause();
    CHECK(h.feedback.kind() == Kind::Wrong); // pausing alone changes nothing
    h.rig.controller.quit();
    REQUIRE(h.rig.controller.state() == GameController::State::GameOver);
    CHECK(h.feedback.kind() == Kind::None);
}

TEST_CASE("The last answer of a level is held after the state has moved on", "[unit][feedback]")
{
    Held h;
    h.rig.play_level_right();
    REQUIRE(h.rig.controller.state() == GameController::State::LevelClear);
    CHECK(h.feedback.kind() == Kind::Right);
    CHECK(h.feedback.question_number() == 10);
}

TEST_CASE("A new game ends a hold", "[unit][feedback]")
{
    Held h;
    h.feedback.set_boom_hold_ms(5000);
    h.rig.play_for_ms(61000);
    REQUIRE(h.feedback.kind() == Kind::Boom);

    h.rig.controller.startGame();

    CHECK(h.feedback.kind() == Kind::None);
}

TEST_CASE("The default hold times are the presentation times the spec names", "[unit][feedback]")
{
    AnswerFeedback feedback;
    CHECK(feedback.wrong_hold_ms() == 1000); // SPEC: the correct tile is shown for 1 s
    CHECK(feedback.right_hold_ms() > 0);
    CHECK(feedback.right_hold_ms() < feedback.wrong_hold_ms());
    CHECK(feedback.boom_hold_ms() > 0);
    CHECK(feedback.game() == nullptr);
}

TEST_CASE("The controller keeps the question an answer was given to", "[unit][controller]")
{
    UiRig rig;
    rig.controller.startGame();
    CHECK(rig.controller.answered().isEmpty());
    const QStringList parts = rig.controller.question_parts();
    const QList<int> tiles = rig.controller.tiles();
    QSignalSpy changed(&rig.controller, &GameController::answeredChanged);

    rig.answer_wrong();

    const QVariantMap answered = rig.controller.answered();
    CHECK(changed.count() == 1);
    CHECK(answered.value(QStringLiteral("parts")).toStringList() == parts);
    CHECK(answered.value(QStringLiteral("questionNumber")).toInt() == 1);
    CHECK_FALSE(answered.value(QStringLiteral("correct")).toBool());
    CHECK(answered.value(QStringLiteral("tiles")).toList().size() == tiles.size());
}

TEST_CASE("The alert zone and the tick rate reach the screens as numbers", "[unit][controller]")
{
    UiRig rig;
    rig.controller.startGame();
    CHECK(rig.controller.ticks_per_second() == 20);
    CHECK(rig.controller.alert_ticks() == 10 * 20);
    CHECK(rig.controller.alert_ticks() < rig.controller.ticks_total());
}
