// SPDX-License-Identifier: GPL-3.0-or-later
// Performance tests of the app layer without a screen: what the controller costs per frame and per answer, what the
// save costs to write and read, and how long it takes to start. Label `perf`; built and run only by the perf presets.
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/session.hpp"
#include "perf_harness.hpp"
#include "services/file_save_store.hpp"
#include "ui_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <cstdint>
#include <memory>
#include <string>

using namespace hbm_perf;
using namespace hbm_test;
using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using State = GameController::State;

namespace
{

constexpr int REPETITIONS = 9;
constexpr std::int64_t FRAME_NS = 16'666'667; // 60 frames a second

volatile std::uint64_t g_Sink = 0;

void use(std::uint64_t value)
{
    g_Sink = g_Sink + value;
}

/// What a child's next tap needs: the controller on a screen that plays, whatever the last answer led to.
void keep_playing(GameController& c)
{
    switch (c.state())
    {
    case State::LevelClear:
        c.nextLevel();
        break;
    case State::Won:
        c.keepGoing();
        break;
    case State::GameOver:
        if (c.name_entry_pending())
        {
            c.skipName();
        }
        c.startGame();
        break;
    case State::Idle:
        c.startGame();
        break;
    case State::Paused:
        c.resume();
        break;
    case State::Playing:
        break;
    }
}

void answer_right(GameController& c)
{
    keep_playing(c);
    c.answer(c.session_view().question.correct_tile);
}

/// A save with a full top 10 and a run paused in the middle of a level, the biggest a real save gets.
std::string full_save_text()
{
    GameSession s(Tuning::defaults(), 5);
    s.start_game();
    for (int i = 0; i < 25; ++i)
    {
        if (s.view().state == SessionState::LEVEL_CLEAR)
        {
            s.next_level();
        }
        s.answer(s.view().question.correct_tile);
    }
    s.pause();
    SaveData data;
    data.device.id = "9f1c2a7b3d4e5f60";
    data.shared.players[0].won_once = true;
    data.shared.players[0].coins = {{data.device.id, 123456, 0}};
    for (int i = 0; i < 10; ++i)
    {
        data.shared.top10.push_back(
            {std::string("PLAYER ") + static_cast<char>('A' + i), 100000 - 1000 * i, data.device.id, i + 1});
    }
    data.device.next_seq = 11;
    data.device.run = s.snapshot();
    data.device.run_player = "first";
    return serialise(data).value();
}

/// A controller on a save file in a temporary folder, with the other services recorded.
struct FileRig
{
    explicit FileRig(const std::string& saved_text) : dir(), store(dir.path())
    {
        REQUIRE(dir.isValid());
        if (!saved_text.empty())
        {
            REQUIRE(store.store(saved_text));
        }
        GameServices services;
        services.store = &store;
        services.quarantine = &store;
        services.audio = &audio;
        services.scores = &scores;
        services.links = &links;
        services.clock = &clock;
        GameOptions options;
        options.session_seed = 20261114;
        options.tile_guard_ns = 0;
        controller = std::make_unique<GameController>(services, options);
    }

    QTemporaryDir dir;
    FileSaveStore store;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    std::unique_ptr<GameController> controller;
};

} // namespace

TEST_CASE("Advancing a playing level costs little per frame", "[perf][app]")
{
    constexpr int FRAMES = 20'000;
    constexpr int ANSWER_EVERY = 120; // a tap every two seconds
    UiRig rig;
    rig.controller.startGame();
    const auto samples = sample_ns_per_op(REPETITIONS,
                                          [&](Timer& timer)
                                          {
                                              timer.start();
                                              for (int frame = 0; frame < FRAMES; ++frame)
                                              {
                                                  rig.clock.advance_ns(FRAME_NS);
                                                  rig.controller.advance_time();
                                                  if (frame % ANSWER_EVERY == ANSWER_EVERY - 1)
                                                  {
                                                      answer_right(rig.controller);
                                                  }
                                              }
                                              timer.stop();
                                              return FRAMES;
                                          });
    REQUIRE(rig.controller.state() != State::Idle);
    check("app.controller.advance_time_per_frame", "ns_per_frame", "ns", stats_of(samples));
}

TEST_CASE("An answer, with the view refreshed and the signals sent, costs little", "[perf][app]")
{
    constexpr int ANSWERS = 3000;
    UiRig rig;
    GameController& c = rig.controller;
    // A screen listens to most of these; counting them makes the emissions real work.
    std::int64_t emitted = 0;
    const auto count = [&emitted] { ++emitted; };
    QObject::connect(&c, &GameController::stateChanged, count);
    QObject::connect(&c, &GameController::levelChanged, count);
    QObject::connect(&c, &GameController::questionChanged, count);
    QObject::connect(&c, &GameController::answeredChanged, count);
    QObject::connect(&c, &GameController::correctAnswersChanged, count);
    QObject::connect(&c, &GameController::timeChanged, count);
    QObject::connect(&c, &GameController::pointsChanged, count);
    QObject::connect(&c, &GameController::walletChanged, count);
    QObject::connect(&c, &GameController::runChanged, count);
    QObject::connect(&c, &GameController::multiplierChanged, count);
    QObject::connect(&c, &GameController::levelClearChanged, count);
    QObject::connect(&c, &GameController::answerCorrect, count);
    QObject::connect(&c, &GameController::questionShown, count);
    QObject::connect(&c, &GameController::levelCleared, count);
    c.startGame();
    const std::int64_t emitted_before = emitted;
    const auto samples = sample_ns_per_op(REPETITIONS,
                                          [&](Timer& timer)
                                          {
                                              timer.start();
                                              for (int i = 0; i < ANSWERS; ++i)
                                              {
                                                  answer_right(c);
                                              }
                                              timer.stop();
                                              return ANSWERS;
                                          });
    const double per_answer = static_cast<double>(emitted - emitted_before) / (ANSWERS * (REPETITIONS + 1.0));
    REQUIRE(per_answer > 1);
    check("app.controller.answer", "ns_per_answer", "ns", stats_of(samples), {{"signals_per_answer", per_answer}});
}

TEST_CASE("The save is written and read fast, and persisting costs little", "[perf][app][save]")
{
    const std::string text = full_save_text();
    {
        FileRig rig(text);
        constexpr int WRITES = 100;
        const auto write_samples = sample_ns_per_op(REPETITIONS,
                                                    [&](Timer& timer)
                                                    {
                                                        timer.start();
                                                        for (int i = 0; i < WRITES; ++i)
                                                        {
                                                            use(rig.store.store(text) ? 1 : 0);
                                                        }
                                                        timer.stop();
                                                        return WRITES;
                                                    });
        check("app.save.file_write", "ns_per_write", "ns", stats_of(write_samples),
              {{"bytes", static_cast<double>(text.size())}});

        constexpr int READS = 300;
        const auto read_samples = sample_ns_per_op(REPETITIONS,
                                                   [&](Timer& timer)
                                                   {
                                                       timer.start();
                                                       for (int i = 0; i < READS; ++i)
                                                       {
                                                           use(rig.store.load()->size());
                                                       }
                                                       timer.stop();
                                                       return READS;
                                                   });
        check("app.save.file_read", "ns_per_read", "ns", stats_of(read_samples));

        // persist() through the controller: a settings change saves, so toggling the sound writes the file each time.
        constexpr int TOGGLES = 100;
        bool on = rig.controller->sound_enabled();
        const auto persist_samples = sample_ns_per_op(REPETITIONS,
                                                      [&](Timer& timer)
                                                      {
                                                          timer.start();
                                                          for (int i = 0; i < TOGGLES; ++i)
                                                          {
                                                              on = !on;
                                                              rig.controller->setSoundEnabled(on);
                                                          }
                                                          timer.stop();
                                                          return TOGGLES;
                                                      });
        check("app.save.persist_to_file", "ns_per_persist", "ns", stats_of(persist_samples));
    }

    // The same without the disk: the cost of building and checking the save text in the controller.
    UiRig memory_rig(20261114, false, text);
    constexpr int TOGGLES = 1000;
    bool on = memory_rig.controller.sound_enabled();
    const auto memory_samples = sample_ns_per_op(REPETITIONS,
                                                 [&](Timer& timer)
                                                 {
                                                     timer.start();
                                                     for (int i = 0; i < TOGGLES; ++i)
                                                     {
                                                         on = !on;
                                                         memory_rig.controller.setSoundEnabled(on);
                                                     }
                                                     timer.stop();
                                                     return TOGGLES;
                                                 });
    check("app.save.persist_to_memory", "ns_per_persist", "ns", stats_of(memory_samples));
}

TEST_CASE("The controller starts fast: from nothing to the first view, with a full save to read", "[perf][app]")
{
    const std::string text = full_save_text();
    constexpr int STARTS = 20;
    const auto samples = sample_ns_per_op(REPETITIONS,
                                          [&](Timer& timer)
                                          {
                                              for (int i = 0; i < STARTS; ++i)
                                              {
                                                  timer.start();
                                                  auto rig = std::make_unique<UiRig>(20261114, false, text);
                                                  // What the title screen reads first.
                                                  use(static_cast<std::uint64_t>(rig->controller.state()));
                                                  use(static_cast<std::uint64_t>(rig->controller.best_score()));
                                                  use(static_cast<std::uint64_t>(rig->controller.top10().size()));
                                                  use(rig->controller.has_saved_run() ? 1 : 0);
                                                  timer.stop();
                                              }
                                              return STARTS;
                                          });
    check("app.controller.startup", "ns_per_start", "ns", stats_of(samples));
}
