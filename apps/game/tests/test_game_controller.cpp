// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "heart_boom_maths/version.hpp"
#include "services/clock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <memory>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;
using State = GameController::State;

namespace
{

constexpr std::uint64_t FIRST_GATE_SEED = 11;
constexpr std::uint64_t SECOND_GATE_SEED = 12;

/// One controller with every service recorded. The save store is passed in so a second rig can read what the first
/// one wrote, as a restart does.
struct Rig
{
    explicit Rig(MemorySaveStore& memory, bool score_service_available = false)
        : scores(score_service_available), controller(make_services(memory), make_options())
    {
    }

    GameServices make_services(MemorySaveStore& memory)
    {
        GameServices services;
        services.store = &memory;
        services.audio = &audio;
        services.scores = &scores;
        services.links = &links;
        services.clock = &clock;
        return services;
    }

    GameOptions make_options()
    {
        GameOptions options;
        options.session_seed = 20261114;
        options.gate_seed_source = [this] { return gate_seeds_used++ == 0 ? FIRST_GATE_SEED : SECOND_GATE_SEED; };
        return options;
    }

    /// Plays real time in 50 ms frames, the length of one tick.
    void play_for_ms(int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += 50)
        {
            clock.advance_ms(50);
            controller.advance_time();
        }
    }

    void answer_right()
    {
        controller.answer(controller.session_view().question.correct_tile);
    }

    void answer_wrong()
    {
        const SessionView& v = controller.session_view();
        for (int tile = 0; tile < 4; ++tile)
        {
            if (tile != v.question.correct_tile && ((v.removed_tiles >> tile) & 1) == 0)
            {
                controller.answer(tile);
                return;
            }
        }
    }

    void play_level_right()
    {
        for (int i = 0; i < 10; ++i)
        {
            answer_right();
        }
    }

    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    int gate_seeds_used = 0;
    GameController controller;
};

SaveData saved(const MemorySaveStore& memory)
{
    REQUIRE(memory.text().has_value());
    const ParseResult parsed = parse_save(*memory.text());
    REQUIRE(parsed.status == LoadStatus::OK);
    return parsed.data;
}

} // namespace

TEST_CASE("A fresh controller shows the title screen on defaults", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    const GameController& c = rig.controller;
    CHECK(c.state() == State::Idle);
    CHECK(c.wallet() == 0);
    CHECK(c.points() == 0);
    CHECK(c.best_score() == 0);
    CHECK_FALSE(c.has_saved_run());
    CHECK(c.sound_enabled());
    CHECK(c.music_enabled());
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(c.game_center_available());
    CHECK_FALSE(c.gate_active());
    CHECK_FALSE(c.save_blocked());
    CHECK(c.question_text().isEmpty());
    CHECK(c.tiles().isEmpty());
    CHECK(c.top10().isEmpty());
    CHECK(c.hint_cost() == Tuning::defaults().hint_cost_coins);
    CHECK(c.name_max_length() == Tuning::defaults().name_max_length);
    CHECK(c.version_text().startsWith(QStringLiteral("Heart Boom Maths ")));
    CHECK(c.build_info().value(QStringLiteral("version")).toString().toStdString() ==
          std::string(heart_boom_maths::VERSION_STRING));
    CHECK_FALSE(c.notices().isEmpty());
    CHECK(rig.audio.music == std::vector<MusicId>{MusicId::MENU});
    CHECK_FALSE(memory.text().has_value()); // nothing is written before there is something to keep
}

TEST_CASE("The settings and the wallet in the save reach the game and the services", "[integration][controller]")
{
    SaveData data;
    data.coins = 40;
    data.won_once = true;
    data.sound = false;
    data.music = false;
    data.game_center = true;
    data.top10 = {{"ADA", 900}, {"BEN", 500}};
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory, true);
    CHECK(rig.controller.wallet() == 40);
    CHECK(rig.controller.session_view().won_once);
    CHECK_FALSE(rig.controller.sound_enabled());
    CHECK_FALSE(rig.controller.music_enabled());
    CHECK_FALSE(rig.audio.effects_enabled);
    CHECK_FALSE(rig.audio.music_enabled);
    CHECK(rig.controller.game_center_enabled());
    CHECK(rig.scores.enabled);
    CHECK(rig.controller.best_score() == 900);
    CHECK(rig.controller.top10().size() == 2);
}

TEST_CASE("Starting a game shows the first question as the child reads it", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    int level_started = 0;
    int questions = 0;
    QObject::connect(&c, &GameController::levelStarted, [&](int level) { level_started += level; });
    QObject::connect(&c, &GameController::questionShown, [&](int) { ++questions; });
    QSignalSpy state_spy(&c, &GameController::stateChanged);

    c.startGame();
    CHECK(c.state() == State::Playing);
    CHECK(state_spy.count() == 1);
    CHECK(c.level() == 1);
    CHECK_FALSE(c.endless());
    CHECK(c.questions_per_level() == 10);
    CHECK(c.question_number() == 1);
    CHECK(c.ticks_left() == 1200);
    CHECK(c.ticks_total() == 1200);
    CHECK(c.seconds_left() == 60);
    CHECK(c.seconds_per_right_answer() == 7);
    CHECK(c.seconds_per_wrong_answer() == 3);
    CHECK(c.multiplier() == 1);
    CHECK(c.tiles().size() == 4);
    CHECK(level_started == 1);
    CHECK(questions == 1);
    CHECK(rig.audio.music.back() == MusicId::PLAY);

    // Level 1 is "a + b = ?" with the numbers from the question on screen.
    const Question& q = c.session_view().question;
    CHECK(c.question_text() == QStringLiteral("%1 + %2 = ?").arg(q.terms[0]).arg(q.terms[1]));
    CHECK(c.question_parts() == QStringList{QString::number(q.terms[0]), QStringLiteral("+"),
                                            QString::number(q.terms[1]), QStringLiteral("="), QStringLiteral("?")});
    CHECK(c.tiles()[q.correct_tile] == q.answer);

    c.startGame(); // ignored while a game is on
    CHECK(state_spy.count() == 1);
}

TEST_CASE("Question text covers every shape", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();

    // Levels 5 (missing addend), 8 (a + b - c) and 10 (four terms) by right answers; the timer never runs.
    for (int level = 1; level <= 10; ++level)
    {
        REQUIRE(c.level() == level);
        const Question& q = c.session_view().question;
        const QStringList parts = c.question_parts();
        switch (q.shape)
        {
        case QuestionShape::TWO_TERM:
            CHECK(parts.size() == 5);
            break;
        case QuestionShape::MISSING_ADDEND:
            CHECK(c.question_text() == QStringLiteral("%1 + ? = %2").arg(q.terms[0]).arg(q.terms[1]));
            break;
        case QuestionShape::THREE_TERM:
            CHECK(parts.size() == 7);
            break;
        case QuestionShape::FOUR_TERM:
            CHECK(parts.size() == 9);
            CHECK(parts.last() == QStringLiteral("?"));
            break;
        }
        rig.play_level_right();
        c.nextLevel();
    }
}

TEST_CASE("Real time becomes whole ticks", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    QSignalSpy time_spy(&c, &GameController::timeChanged);

    rig.clock.advance_ms(49);
    c.advance_time();
    CHECK(c.ticks_left() == 1200);
    rig.clock.advance_ms(1);
    c.advance_time();
    CHECK(c.ticks_left() == 1199);

    rig.play_for_ms(1000);
    CHECK(c.ticks_left() == 1179);
    CHECK(c.seconds_left() == 58);
    CHECK(time_spy.count() > 0);

    // A stall of ten seconds costs only the catch-up limit.
    rig.clock.advance_ms(10'000);
    c.advance_time();
    CHECK(c.ticks_left() == 1179 - MAX_CATCH_UP_TICKS);
}

TEST_CASE("Pausing stops time and resuming does not count the pause", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(500);
    const int before = c.ticks_left();

    c.pause();
    CHECK(c.state() == State::Paused);
    rig.play_for_ms(30'000);
    CHECK(c.ticks_left() == before);
    CHECK(c.has_saved_run());

    c.resume();
    CHECK(c.state() == State::Playing);
    CHECK_FALSE(c.has_saved_run());
    rig.play_for_ms(100);
    CHECK(c.ticks_left() == before - 2);
    CHECK_FALSE(saved(memory).run.has_value());
}

TEST_CASE("A right and a wrong answer move the score, the timer and the sounds", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();

    int right_points = 0;
    int wrong_lost = 0;
    QObject::connect(&c, &GameController::answerCorrect,
                     [&](int, int points, int, int, int, int, bool, int) { right_points = points; });
    QObject::connect(&c, &GameController::answerWrong, [&](int, int, int lost, int) { wrong_lost = lost; });
    QSignalSpy points_spy(&c, &GameController::pointsChanged);
    QSignalSpy question_spy(&c, &GameController::questionChanged);

    rig.answer_right();
    CHECK(right_points == 10);
    CHECK(c.points() == 10);
    CHECK(c.wallet() == 1);
    CHECK(c.run() == 1);
    CHECK(c.correct_answers() == 1);
    CHECK(c.question_number() == 2);
    CHECK(points_spy.count() == 1);
    CHECK(question_spy.count() == 1);
    CHECK(rig.audio.count(SoundId::COIN_CHIME) == 1);

    rig.answer_wrong();
    CHECK(wrong_lost == 3);
    CHECK(c.run() == 0);
    CHECK(c.points() == 10);
    CHECK(c.question_number() == 3);
    CHECK(rig.audio.count(SoundId::SOFT_BUZZ) == 1);
    CHECK(c.ticks_left() == 1200 - 60 + 0); // the right answer was capped at the full timer, the wrong one cost 3 s
}

TEST_CASE("The rising tone plays with the chime when the multiplier rises", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    bool rose = false;
    QObject::connect(&c, &GameController::answerCorrect,
                     [&](int, int, int, int, int, int, bool multiplier_rose, int) { rose = rose || multiplier_rose; });
    rig.answer_right();
    rig.answer_right();
    CHECK(rig.audio.count(SoundId::RISING_TONE) == 0);
    rig.answer_right(); // a run of 3 lifts the step to x2
    CHECK(rose);
    CHECK(c.multiplier() == 2);
    CHECK(rig.audio.count(SoundId::RISING_TONE) == 1);
    CHECK(rig.audio.count(SoundId::COIN_CHIME) == 3);
}

TEST_CASE("A hint costs coins, removes tiles and whooshes", "[integration][controller]")
{
    SaveData data;
    data.coins = 10;
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    CHECK(c.hint_available());
    int removed = 0;
    int cost = 0;
    QObject::connect(&c, &GameController::hintWasUsed,
                     [&](int removed_tiles, int price)
                     {
                         removed = removed_tiles;
                         cost = price;
                     });
    QSignalSpy removed_spy(&c, &GameController::removedTilesChanged);
    c.useHint();
    CHECK(c.wallet() == 8);
    CHECK(cost == 2);
    CHECK(c.hint_used());
    CHECK_FALSE(c.hint_available());
    CHECK(c.removed_tiles() == removed);
    CHECK(removed_spy.count() == 1);
    CHECK(rig.audio.count(SoundId::WHOOSH) == 1);
}

TEST_CASE("A cleared level shows its card and saves the wallet", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    int cleared = 0;
    QObject::connect(&c, &GameController::levelCleared, [&] { ++cleared; });
    QSignalSpy card_spy(&c, &GameController::levelClearChanged);

    rig.play_level_right();
    CHECK(c.state() == State::LevelClear);
    CHECK(cleared == 1);
    CHECK(card_spy.count() == 1);
    const QVariantMap card = c.level_clear();
    CHECK(card.value("level").toInt() == 1);
    CHECK(card.value("perfect").toBool());
    CHECK(card.value("secondsLeft").toInt() == 60);
    CHECK(card.value("timeBonusPoints").toInt() == 600);
    CHECK(card.value("perfectBonusPoints").toInt() == 100);
    CHECK(card.value("coins").toInt() == 5);
    CHECK(rig.audio.count(SoundId::FANFARE) == 1);

    // 10 right answers (10 coins and one streak bonus of 1 at the fifth and tenth) and 5 for the perfect level.
    CHECK(c.wallet() == saved(memory).coins);
    CHECK(c.wallet() == 10 + 2 + 5);

    rig.play_for_ms(5000); // the card has no clock
    CHECK(c.ticks_left() == 1200);
    c.nextLevel();
    CHECK(c.state() == State::Playing);
    CHECK(c.level() == 2);
}

TEST_CASE("Clearing level 12 wins, pays once and either keeps going or finishes", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    int bonus = -1;
    QObject::connect(&c, &GameController::gameWon, [&](int coins) { bonus = coins; });
    for (int level = 1; level <= 12; ++level)
    {
        rig.play_level_right();
        if (level < 12)
        {
            c.nextLevel();
        }
    }
    CHECK(c.state() == State::Won);
    CHECK(bonus == 1000);
    CHECK(rig.audio.count(SoundId::FANFARE) == 12); // the win sends GAME_WON as well and adds no second fanfare
    CHECK(saved(memory).won_once);
    CHECK(saved(memory).coins >= 1000);

    c.keepGoing();
    CHECK(c.state() == State::Playing);
    CHECK(c.level() == 13);
    CHECK(c.endless());

    // A second win elsewhere would pay nothing; here FINISH from level 12 is shown through a fresh rig.
    MemorySaveStore other;
    Rig again(other);
    again.controller.startGame();
    for (int level = 1; level <= 12; ++level)
    {
        again.play_level_right();
        if (level < 12)
        {
            again.controller.nextLevel();
        }
    }
    again.controller.finish();
    CHECK(again.controller.state() == State::GameOver);
    CHECK(again.controller.end_reason() == GameController::EndReason::Finish);
    CHECK(again.scores.submitted.size() == 1);
    CHECK(again.scores.submitted.front() == again.controller.final_score());
}

TEST_CASE("A whole scripted game runs to the bomb with the manual clock", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    rig.answer_right();
    int alerts = 0;
    int over_signals = 0;
    GameController::EndReason reason = GameController::EndReason::Quit;
    QObject::connect(&c, &GameController::alertEntered, [&] { ++alerts; });
    QObject::connect(&c, &GameController::gameOver,
                     [&](GameController::EndReason r, qint64, bool, int)
                     {
                         ++over_signals;
                         reason = r;
                     });

    int frames = 0;
    while (c.state() == State::Playing && frames < 2000)
    {
        rig.play_for_ms(50);
        ++frames;
    }
    CHECK(c.state() == State::GameOver);
    // 1200 ticks at one a frame, less the 7 s the first two answers could not add (the timer was full).
    CHECK(frames == 1200);
    CHECK(alerts == 1);
    CHECK(over_signals == 1);
    CHECK(reason == GameController::EndReason::Bomb);
    CHECK(c.ticks_left() == 0);
    CHECK(c.final_score() == 20);
    CHECK(c.final_level() == 1);
    CHECK(c.new_best());
    CHECK(c.name_entry_pending());
    CHECK(c.name_entry_rank() == 1);

    CHECK(rig.audio.count(SoundId::TICKING) == 1);
    CHECK(rig.audio.count(SoundId::CARTOON_POP) == 1);
    CHECK(rig.audio.stopped.size() == 1); // the ticking ends with the game
    CHECK(rig.audio.music.back() == MusicId::MENU);

    // Once, with the final score, and the driver is stopped for good.
    REQUIRE(rig.scores.submitted.size() == 1);
    CHECK(rig.scores.submitted.front() == 20);
    rig.play_for_ms(5000);
    CHECK(rig.scores.submitted.size() == 1);
    CHECK(c.ticks_left() == 0);
    CHECK(saved(memory).coins == 2);
}

TEST_CASE("Leaving the alert zone stops the ticking, and a pause in it starts it again", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.play_for_ms(51'000);
    REQUIRE(c.alert());
    CHECK(rig.audio.count(SoundId::TICKING) == 1);

    c.pause();
    CHECK(rig.audio.stopped.size() == 1);
    c.resume();
    CHECK(rig.audio.count(SoundId::TICKING) == 2);

    rig.answer_right(); // +7 s lifts the timer out of the last 10
    CHECK_FALSE(c.alert());
    CHECK(rig.audio.stopped.size() == 2);
}

TEST_CASE("QUIT on the pause screen ends the game and offers the name entry once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    CHECK(c.state() == State::GameOver);
    CHECK(c.end_reason() == GameController::EndReason::Quit);
    CHECK(c.final_score() == 10);
    CHECK_FALSE(c.has_saved_run());
    CHECK_FALSE(saved(memory).run.has_value());
    CHECK(rig.scores.submitted == std::vector<std::int64_t>{10});
    CHECK(rig.audio.count(SoundId::CARTOON_POP) == 1);

    REQUIRE(c.name_entry_pending());
    QSignalSpy top_spy(&c, &GameController::topScoresChanged);
    CHECK(c.normaliseName(QStringLiteral("ada lovelace 1!")) == QStringLiteral("ADA LOVELACE"));
    CHECK(c.saveName(QString()) == 0); // an empty name does not save
    CHECK(c.name_entry_pending());
    CHECK(c.saveName(QStringLiteral("ada lovelace 1!")) == 1);
    CHECK_FALSE(c.name_entry_pending());
    CHECK(top_spy.count() == 1);
    CHECK(c.highlight_rank() == 1);
    CHECK(c.best_score() == 10);
    CHECK(c.top10().first().toMap().value("name").toString() == QStringLiteral("ADA LOVELACE"));
    CHECK(saved(memory).top10 == std::vector<ScoreEntry>{{"ADA LOVELACE", 10}});
    CHECK(c.saveName(QStringLiteral("again")) == 0); // already saved

    // Starting again clears the leftovers of the last game.
    c.startGame();
    CHECK(c.highlight_rank() == 0);
    CHECK_FALSE(c.name_entry_pending());
}

TEST_CASE("SKIP drops the entry, and a score of nothing never qualifies", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    REQUIRE(c.name_entry_pending());
    c.skipName();
    CHECK_FALSE(c.name_entry_pending());
    CHECK(c.top10().isEmpty());
    c.skipName(); // nothing pending: no effect

    c.startGame();
    c.pause();
    c.quit();
    CHECK(c.final_score() == 0);
    CHECK_FALSE(c.name_entry_pending());
    CHECK_FALSE(c.new_best());
    CHECK(rig.scores.submitted.size() == 2);
}

TEST_CASE("Leaving the foreground pauses, suspends audio and writes the save", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    rig.play_for_ms(500);
    const int before = c.ticks_left();
    const int stores = memory.store_count();

    c.on_application_state(false);
    CHECK(c.state() == State::Paused);
    CHECK(rig.audio.suspends == 1);
    CHECK(memory.store_count() > stores);
    CHECK(saved(memory).run.has_value());
    CHECK(saved(memory).coins == 1);

    rig.play_for_ms(10'000);
    CHECK(c.ticks_left() == before);

    // Coming back resumes the sound only: the game stays on the pause screen with the clock stopped.
    c.on_application_state(true);
    CHECK(rig.audio.resumes == 1);
    CHECK(c.state() == State::Paused);
    rig.play_for_ms(10'000);
    CHECK(c.ticks_left() == before);

    c.resume();
    rig.play_for_ms(100);
    CHECK(c.ticks_left() == before - 2);
}

TEST_CASE("Leaving the foreground on the title screen or a card writes the save and pauses nothing",
          "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.on_application_state(false);
    CHECK(c.state() == State::Idle);
    CHECK(memory.text().has_value());
    c.on_application_state(true);

    c.startGame();
    rig.play_level_right();
    c.on_application_state(false);
    CHECK(c.state() == State::LevelClear);
    CHECK_FALSE(saved(memory).run.has_value());
    c.on_application_state(true);
    c.nextLevel();
    CHECK(c.state() == State::Playing);
}

TEST_CASE("CONTINUE restores the paused run once", "[integration][controller]")
{
    MemorySaveStore memory;
    std::int64_t points = 0;
    int ticks = 0;
    int level = 0;
    QStringList parts;
    QList<int> tiles;
    {
        Rig first(memory);
        first.controller.startGame();
        first.play_level_right();
        first.controller.nextLevel();
        first.answer_right();
        first.answer_wrong();
        first.play_for_ms(2000);
        first.controller.pause();
        points = first.controller.points();
        ticks = first.controller.ticks_left();
        level = first.controller.level();
        parts = first.controller.question_parts();
        tiles = first.controller.tiles();
    }
    REQUIRE(saved(memory).run.has_value());

    Rig second(memory);
    GameController& c = second.controller;
    CHECK(c.state() == State::Idle);
    CHECK(c.has_saved_run());
    QSignalSpy run_spy(&c, &GameController::savedRunChanged);

    REQUIRE(c.continueGame());
    CHECK(c.state() == State::Paused); // the pause screen: nothing runs until RESUME
    CHECK(c.points() == points);
    CHECK(c.ticks_left() == ticks);
    CHECK(c.level() == level);
    CHECK(c.question_parts() == parts);
    CHECK(c.tiles() == tiles);
    CHECK_FALSE(c.has_saved_run());
    CHECK(run_spy.count() == 1);
    CHECK_FALSE(saved(memory).run.has_value()); // deleted as soon as it loads
    second.play_for_ms(5000);
    CHECK(c.ticks_left() == ticks);

    CHECK_FALSE(c.continueGame()); // resumes once

    c.resume();
    second.play_for_ms(100);
    CHECK(c.ticks_left() == ticks - 2);
}

TEST_CASE("A paused run that is paused again after CONTINUE is saved again on leaving", "[integration][controller]")
{
    MemorySaveStore memory;
    {
        Rig first(memory);
        first.controller.startGame();
        first.controller.pause();
    }
    Rig second(memory);
    REQUIRE(second.controller.continueGame());
    second.controller.on_application_state(false);
    CHECK(saved(memory).run.has_value());
    CHECK(second.controller.has_saved_run());
}

TEST_CASE("A saved run that cannot be restored starts clean", "[integration][controller]")
{
    SaveData data;
    data.coins = 3;
    data.run = std::vector<std::int64_t>{1, 2, 3};
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.has_saved_run());
    int failed = 0;
    QObject::connect(&c, &GameController::continueFailed, [&] { ++failed; });

    CHECK_FALSE(c.continueGame());
    CHECK(failed == 1);
    CHECK(c.state() == State::Idle);
    CHECK_FALSE(c.has_saved_run());
    CHECK(c.wallet() == 3);
    CHECK_FALSE(saved(memory).run.has_value());

    c.startGame(); // the game is fine
    CHECK(c.state() == State::Playing);
}

TEST_CASE("CONTINUE does nothing without a run, and NEW GAME discards one", "[integration][controller]")
{
    MemorySaveStore memory;
    {
        Rig first(memory);
        first.controller.startGame();
        first.controller.pause();
    }
    Rig rig(memory);
    REQUIRE(rig.controller.has_saved_run());
    rig.controller.startGame();
    CHECK_FALSE(rig.controller.has_saved_run());
    CHECK_FALSE(saved(memory).run.has_value());
    CHECK_FALSE(rig.controller.continueGame());
    CHECK(rig.controller.state() == State::Playing);
}

TEST_CASE("The settings save at once and reach the audio service", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    QSignalSpy spy(&c, &GameController::settingsChanged);

    c.setSoundEnabled(false);
    CHECK_FALSE(rig.audio.effects_enabled);
    CHECK(rig.audio.music_enabled);
    CHECK_FALSE(saved(memory).sound);
    c.setMusicEnabled(false);
    CHECK_FALSE(rig.audio.music_enabled);
    CHECK_FALSE(saved(memory).music);
    CHECK(spy.count() == 2);

    c.setSoundEnabled(false); // no change, no signal
    CHECK(spy.count() == 2);
}

TEST_CASE("A link opens only after the grown-up types the number back", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    QSignalSpy gate_spy(&c, &GameController::gateChanged);
    QSignalSpy failed_spy(&c, &GameController::gateFailed);
    QSignalSpy passed_spy(&c, &GameController::gatePassed);

    CHECK_FALSE(c.requestLink(QStringLiteral("http://example.org/plain")));
    CHECK_FALSE(c.requestLink(QStringLiteral("file:///etc/passwd")));
    CHECK_FALSE(c.gate_active());
    CHECK(c.submitGate(QStringLiteral("1")) == false); // no gate open

    REQUIRE(c.requestLink(QStringLiteral("https://example.org/privacy")));
    CHECK(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::Link);
    const GateChallenge first = make_gate_challenge(FIRST_GATE_SEED);
    CHECK(c.gate_words().toStdString() == first.words);
    CHECK(rig.links.opened.empty());

    // A wrong answer opens nothing and deals a new number.
    CHECK_FALSE(c.submitGate(QString::number(first.number + 1)));
    CHECK(rig.links.opened.empty());
    CHECK(c.gate_active());
    CHECK(failed_spy.count() == 1);
    const GateChallenge second = make_gate_challenge(SECOND_GATE_SEED);
    CHECK(c.gate_words().toStdString() == second.words);
    CHECK_FALSE(c.submitGate(QString::number(first.number))); // the old number no longer works
    CHECK(rig.links.opened.empty());

    CHECK(c.submitGate(QString::number(second.number)));
    CHECK(rig.links.opened == std::vector<std::string>{"https://example.org/privacy"});
    CHECK_FALSE(c.gate_active());
    CHECK(c.gate_words().isEmpty());
    CHECK(passed_spy.count() == 1);
    CHECK(gate_spy.count() >= 3);

    // The gate is spent: the same answer opens nothing a second time.
    CHECK_FALSE(c.submitGate(QString::number(second.number)));
    CHECK(rig.links.opened.size() == 1);
}

TEST_CASE("Cancelling the gate opens nothing", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    REQUIRE(c.requestLink(QStringLiteral("HTTPS://example.org")));
    c.cancelGate();
    CHECK_FALSE(c.gate_active());
    CHECK(rig.links.opened.empty());
    c.cancelGate(); // already closed
}

TEST_CASE("Game Center is unavailable without a leaderboard", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    rig.controller.requestGameCenter(true);
    CHECK_FALSE(rig.controller.gate_active());
    CHECK_FALSE(rig.controller.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);
}

TEST_CASE("Game Center turns on only behind the gate and off at once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory, true);
    GameController& c = rig.controller;
    REQUIRE(c.game_center_available());
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);

    c.requestGameCenter(false); // already off: nothing happens
    CHECK_FALSE(c.gate_active());

    c.requestGameCenter(true);
    REQUIRE(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::GameCenter);
    CHECK_FALSE(c.game_center_enabled());
    const GateChallenge challenge = make_gate_challenge(FIRST_GATE_SEED);

    CHECK_FALSE(c.submitGate(QString::number(challenge.number + 1)));
    CHECK_FALSE(c.game_center_enabled());
    c.cancelGate();
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(memory.text().has_value()); // nothing was turned on, so nothing was written

    c.requestGameCenter(true);
    CHECK(c.submitGate(QString::number(make_gate_challenge(SECOND_GATE_SEED).number)));
    CHECK(c.game_center_enabled());
    CHECK(rig.scores.enabled);
    CHECK(saved(memory).game_center);
    CHECK(rig.links.opened.empty());

    c.requestGameCenter(true); // already on: no gate
    CHECK_FALSE(c.gate_active());

    c.requestGameCenter(false);
    CHECK_FALSE(c.game_center_enabled());
    CHECK_FALSE(rig.scores.enabled);
    CHECK_FALSE(saved(memory).game_center);
}

TEST_CASE("A store that cannot write is survived", "[integration][controller]")
{
    MemorySaveStore memory;
    memory.set_store_fails(true);
    Rig rig(memory);
    GameController& c = rig.controller;
    c.startGame();
    rig.answer_right();
    c.pause();
    c.quit();
    CHECK(c.state() == State::GameOver);
    CHECK(c.saveName(QStringLiteral("kit")) == 1); // the list is kept in memory for this session
    CHECK(c.top10().size() == 1);
}

TEST_CASE("Commands in the wrong state do nothing", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    c.answer(0);
    c.useHint();
    c.pause();
    c.resume();
    c.quit();
    c.nextLevel();
    c.keepGoing();
    c.finish();
    c.advance_time();
    CHECK(c.state() == State::Idle);
    CHECK_FALSE(memory.text().has_value());
    CHECK_FALSE(c.continueGame());
}

TEST_CASE("The controller registers its types and properties with Qt", "[integration][controller]")
{
    const QMetaObject& meta = GameController::staticMetaObject;
    for (const char* name : {"state",
                             "level",
                             "questionText",
                             "questionParts",
                             "tiles",
                             "removedTiles",
                             "secondsLeft",
                             "alert",
                             "points",
                             "wallet",
                             "run",
                             "multiplier",
                             "hintAvailable",
                             "top10",
                             "bestScore",
                             "hasSavedRun",
                             "gateActive",
                             "gateWords",
                             "versionText",
                             "noticesText",
                             "levelClear",
                             "savedRun",
                             "gateUrl",
                             "gateDigits",
                             "privacyPolicyUrl",
                             "repositoryUrl",
                             "firstLevelQuestions",
                             "firstLevelSeconds"})
    {
        INFO(name);
        CHECK(meta.indexOfProperty(name) >= 0);
    }
    for (const char* name :
         {"startGame()", "continueGame()", "answer(int)", "useHint()", "pause()", "resume()", "quit()", "nextLevel()",
          "keepGoing()", "finish()", "requestLink(QString)", "submitGate(QString)", "saveName(QString)", "skipName()",
          "gateMatches(QString)", "cancelGate()"})
    {
        INFO(name);
        CHECK(meta.indexOfMethod(name) >= 0);
    }
}

TEST_CASE("The saved run says its level and the time left for the title screen", "[integration][controller]")
{
    MemorySaveStore memory;
    int level = 0;
    int seconds = 0;
    {
        Rig first(memory);
        CHECK(first.controller.saved_run().isEmpty());
        first.controller.startGame();
        first.play_level_right();
        first.controller.nextLevel();
        first.answer_right();
        first.play_for_ms(3000);
        first.controller.pause();
        level = first.controller.level();
        seconds = first.controller.seconds_left();
        const QVariantMap now = first.controller.saved_run();
        CHECK(now.value(QStringLiteral("level")).toInt() == level);
        CHECK(now.value(QStringLiteral("secondsLeft")).toInt() == seconds);
    }
    CHECK(level == 2);

    Rig second(memory);
    QSignalSpy spy(&second.controller, &GameController::savedRunChanged);
    const QVariantMap saved_run = second.controller.saved_run();
    CHECK(saved_run.value(QStringLiteral("level")).toInt() == level);
    CHECK(saved_run.value(QStringLiteral("secondsLeft")).toInt() == seconds);

    // Reading it changes nothing in the controller's own game.
    CHECK(second.controller.state() == State::Idle);
    CHECK(second.controller.has_saved_run());

    // A new game spends the run, and the caption goes with it.
    second.controller.startGame();
    CHECK(second.controller.saved_run().isEmpty());
    CHECK(spy.count() >= 1);
}

TEST_CASE("A saved run the core would refuse shows no caption", "[integration][controller]")
{
    SaveData data;
    data.run = std::vector<std::int64_t>{1, 2, 3};
    MemorySaveStore memory(*serialise(data));
    Rig rig(memory);
    CHECK(rig.controller.has_saved_run());
    CHECK(rig.controller.saved_run().isEmpty());
}

TEST_CASE("The gate says what it opens and checks the typed number without telling it", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK_FALSE(c.gateMatches(QStringLiteral("123"))); // no gate open
    CHECK(c.gate_url().isEmpty());
    CHECK(c.gate_digits() == 3);

    REQUIRE(c.requestLink(c.privacy_policy_url()));
    CHECK(c.gate_url() == c.privacy_policy_url());
    const GateChallenge first = make_gate_challenge(FIRST_GATE_SEED);
    const QString right = QString::number(first.number);
    CHECK(c.gateMatches(right));
    CHECK(c.gateMatches(QStringLiteral(" ") + right + QStringLiteral(" ")));
    CHECK_FALSE(c.gateMatches(QString::number(first.number + 1)));
    CHECK_FALSE(c.gateMatches(right.left(2)));
    CHECK_FALSE(c.gateMatches(QString()));
    CHECK(rig.links.opened.empty()); // asking never opens

    c.cancelGate();
    CHECK(c.gate_url().isEmpty());
    CHECK_FALSE(c.gateMatches(right));
}

TEST_CASE("The two addresses About opens are https and written once", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    GameController& c = rig.controller;
    CHECK(c.privacy_policy_url() ==
          QStringLiteral("https://iainchesworthlabs.github.io/heart-boom-maths/privacy-policy/"));
    CHECK(c.repository_url() == QStringLiteral("https://github.com/iainchesworthlabs/heart-boom-maths"));
    // Each is accepted by requestLink, which needs https.
    CHECK(c.requestLink(c.privacy_policy_url()));
    c.cancelGate();
    CHECK(c.requestLink(c.repository_url()));
    c.cancelGate();
    CHECK(rig.links.opened.empty());
}

TEST_CASE("The title tagline numbers come from the tuning", "[integration][controller]")
{
    MemorySaveStore memory;
    Rig rig(memory);
    CHECK(rig.controller.first_level_questions() == Tuning::defaults().questions_per_level);
    CHECK(rig.controller.first_level_seconds() == Tuning::defaults().level_seconds);
}
