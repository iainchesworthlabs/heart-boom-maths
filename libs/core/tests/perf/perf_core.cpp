// SPDX-License-Identifier: GPL-3.0-or-later
// Performance tests of the rules library: how fast a running session ticks, how fast questions are dealt, what a
// snapshot, a save and a whole replay cost, and how much the hot paths allocate. Label `perf`; they are built and run
// only by the perf presets (docs/testing.md, "Performance tests"). Every number is checked against
// tools/perf-budgets.json.
#include "../session_test_support.hpp"
#include "golden_replays.hpp"
#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/question_generator.hpp"
#include "heart_boom_maths/replay.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/session.hpp"
#include "heart_boom_maths/tuning.hpp"
#include "perf_alloc.hpp"
#include "perf_harness.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace heart_boom_maths;
using namespace hbm_perf;

namespace
{

// Nine repetitions after a warm-up: the median ignores a disturbed run or two, and the best shows the quiet machine.
constexpr int REPETITIONS = 9;

// Keeps the optimiser from throwing away work whose result nothing reads.
volatile std::uint64_t g_Sink = 0;

void use(std::uint64_t value)
{
    g_Sink = g_Sink + value;
}

// A child answering a question every two seconds: the timer never runs out, so the session ticks on through every
// level of the campaign and into endless play.
constexpr int TICKS_PER_ANSWER = 40;

struct PlayCounts
{
    std::int64_t ticks = 0;
    std::int64_t tick_allocations = 0;   // heap allocations made inside tick() calls only
    std::int64_t answer_allocations = 0; // inside answer() and take_events()
    std::int64_t answers = 0;
    int highest_level = 0;
};

/// Plays perfectly, every command a real session would get, until `ticks` ticks have been made. When
/// `count_allocations` is set the heap counter is read around the ticks and around the answers.
PlayCounts play_ticks(GameSession& s, std::int64_t ticks, bool count_allocations)
{
    PlayCounts counts;
    while (counts.ticks < ticks)
    {
        switch (s.view().state)
        {
        case SessionState::PLAYING:
        {
            const auto batch = static_cast<int>(std::min<std::int64_t>(TICKS_PER_ANSWER, ticks - counts.ticks));
            const std::int64_t before = count_allocations ? alloc_counters().allocations : 0;
            for (int i = 0; i < batch; ++i)
            {
                s.tick();
            }
            if (count_allocations)
            {
                counts.tick_allocations += alloc_counters().allocations - before;
            }
            counts.ticks += batch;
            if (counts.ticks < ticks && s.view().state == SessionState::PLAYING)
            {
                const std::int64_t before_answer = count_allocations ? alloc_counters().allocations : 0;
                s.answer(s.view().question.correct_tile);
                use(s.take_events().size());
                if (count_allocations)
                {
                    counts.answer_allocations += alloc_counters().allocations - before_answer;
                }
                ++counts.answers;
            }
            break;
        }
        case SessionState::LEVEL_CLEAR:
            s.next_level();
            use(s.take_events().size());
            break;
        case SessionState::WON:
            s.keep_going();
            use(s.take_events().size());
            break;
        case SessionState::PAUSED:
            s.resume();
            break;
        default: // IDLE, GAME_OVER
            s.start_game();
            use(s.take_events().size());
            break;
        }
        counts.highest_level = std::max(counts.highest_level, s.view().level);
    }
    return counts;
}

} // namespace

TEST_CASE("A running session ticks fast, with answers, level clears and endless play", "[perf][core]")
{
    constexpr std::int64_t TICKS = 1'000'000;
    PlayCounts last;
    const auto samples = sample_ns_per_op(REPETITIONS,
                                          [&](Timer& timer)
                                          {
                                              GameSession s(Tuning::defaults(), 12);
                                              s.start_game();
                                              timer.start();
                                              last = play_ticks(s, TICKS, false);
                                              timer.stop();
                                              return TICKS;
                                          });
    // The run must have gone through the campaign and into endless play, or it measured the wrong thing.
    REQUIRE(last.highest_level > 12);
    const Stats stats = stats_of(samples);
    check("core.session.tick_with_answers", "ns_per_tick", "ns", stats,
          {{"ticks_per_second", 1e9 / stats.median}, {"answers", static_cast<double>(last.answers)}});
}

TEST_CASE("A tick allocates nothing, and an answer little", "[perf][core][alloc]")
{
    GameSession s(Tuning::defaults(), 12);
    s.start_game();
    (void)play_ticks(s, 20'000, false); // warm: vectors reach their working capacity
    const PlayCounts counts = play_ticks(s, 200'000, true);
    REQUIRE(counts.answers > 0);
    check("core.alloc.per_tick", "allocations_per_tick", "milli-allocs/op",
          exact(milli_allocations(counts.tick_allocations, counts.ticks)));
    check("core.alloc.per_answer", "allocations_per_answer", "milli-allocs/op",
          exact(milli_allocations(counts.answer_allocations, counts.answers)));
}

TEST_CASE("Questions are dealt fast at every kind of level", "[perf][core]")
{
    constexpr int LEVELS_PER_REPETITION = 2000;
    constexpr int QUESTIONS_PER_LEVEL = 10;
    constexpr std::int64_t QUESTIONS = static_cast<std::int64_t>(LEVELS_PER_REPETITION) * QUESTIONS_PER_LEVEL;
    for (const int level : {1, 2, 5, 12, 13, 20, 100})
    {
        const std::string name = "core.deal.level_" + std::to_string(level);
        QuestionGenerator generator(Tuning::defaults());
        Rng rng(7);
        const auto deal_levels = [&]
        {
            for (int i = 0; i < LEVELS_PER_REPETITION; ++i)
            {
                generator.begin_level(level, rng);
                for (int q = 0; q < QUESTIONS_PER_LEVEL; ++q)
                {
                    use(static_cast<std::uint64_t>(generator.next(rng).answer));
                }
            }
        };
        const auto samples = sample_ns_per_op(REPETITIONS,
                                              [&](Timer& timer)
                                              {
                                                  timer.start();
                                                  deal_levels();
                                                  timer.stop();
                                                  return QUESTIONS;
                                              });
        const Stats stats = stats_of(samples);
        check(name, "ns_per_question", "ns", stats, {{"questions_per_second", 1e9 / stats.median}});

        const std::int64_t before = alloc_counters().allocations;
        deal_levels();
        check(name + ".allocs", "allocations_per_question", "milli-allocs/op",
              exact(milli_allocations(alloc_counters().allocations - before, QUESTIONS)));
    }
}

TEST_CASE("Snapshot, restore and state hash cost little at campaign and endless levels", "[perf][core]")
{
    constexpr int CALLS = 20'000;
    for (const int level : {5, 20})
    {
        GameSession s(Tuning::defaults(), 3);
        hbm_test::reach_level(s, level);
        hbm_test::answer_right(s);
        hbm_test::answer_right(s);
        const std::vector<std::int64_t> snapshot = s.snapshot();
        const std::string suffix = ".level_" + std::to_string(level);

        auto measure = [&](const std::string& name, const char* metric, auto&& call)
        {
            const auto samples = sample_ns_per_op(REPETITIONS,
                                                  [&](Timer& timer)
                                                  {
                                                      timer.start();
                                                      for (int i = 0; i < CALLS; ++i)
                                                      {
                                                          call();
                                                      }
                                                      timer.stop();
                                                      return CALLS;
                                                  });
            check(name + suffix, metric, "ns", stats_of(samples));
        };
        measure("core.session.snapshot", "ns_per_call", [&] { use(s.snapshot().size()); });
        measure("core.session.restore", "ns_per_call",
                [&]
                {
                    const bool restored = s.restore(snapshot);
                    use(restored ? 1 : 0);
                });
        measure("core.session.state_hash", "ns_per_call", [&] { use(s.state_hash()); });
        REQUIRE(s.restore(snapshot)); // the loop above must have been restoring a state the session accepts
    }
}

namespace
{

SaveData full_save()
{
    GameSession s(Tuning::defaults(), 5);
    hbm_test::reach_level(s, 20);
    hbm_test::answer_right(s);
    SaveData data;
    data.coins = 123456;
    data.won_once = true;
    for (int i = 0; i < 10; ++i)
    {
        data.top10.push_back({std::string("PLAYER ") + static_cast<char>('A' + i), 100000 - 1000 * i});
    }
    data.run = s.snapshot();
    return data;
}

/// What `body` does `calls` times per repetition, as nanoseconds per call.
template <typename Body>
void measure_calls(const std::string& name, const char* metric, int calls, Body&& body)
{
    const auto samples = sample_ns_per_op(REPETITIONS,
                                          [&](Timer& timer)
                                          {
                                              timer.start();
                                              for (int i = 0; i < calls; ++i)
                                              {
                                                  body();
                                              }
                                              timer.stop();
                                              return calls;
                                          });
    check(name, metric, "ns", stats_of(samples));
}

} // namespace

TEST_CASE("A full save is written and read fast", "[perf][core][save]")
{
    const SaveData data = full_save();
    const std::string text = serialise(data).value();
    REQUIRE(parse_save(text).status == LoadStatus::OK);
    const auto document = json::parse(text);
    REQUIRE(document.has_value());

    measure_calls("core.save.serialise", "ns_per_call", 2000, [&] { use(serialise(data).value().size()); });
    measure_calls("core.save.parse", "ns_per_call", 2000, [&] { use(parse_save(text).data.top10.size()); });
    measure_calls("core.json.parse_full_save", "ns_per_call", 2000,
                  [&] { use(json::parse(text)->kind() == json::Kind::OBJECT); });
    measure_calls("core.json.write_full_save", "ns_per_call", 2000, [&] { use(json::write(*document).size()); });
}

TEST_CASE("Hostile input of the largest size the reader accepts is dealt with fast", "[perf][core][save]")
{
    const std::size_t LIMIT = json::Limits{}.max_bytes;

    // The widest document: an array of one-digit numbers up to the byte limit, 2 bytes each.
    std::string wide = "[";
    while (wide.size() + 3 < LIMIT)
    {
        wide += "1,";
    }
    wide += "1]";
    REQUIRE(json::parse(wide).has_value());

    // The most members: an object of distinct short keys, sorted on reading.
    std::string members = "{";
    for (int i = 0; members.size() + 16 < LIMIT; ++i)
    {
        members += "\"k" + std::to_string(99999 - i) + "\":1,";
    }
    members += "\"z\":1}";
    REQUIRE(json::parse(members).has_value());

    const std::string deep(LIMIT, '['); // refused at the depth limit, long before the end
    const std::string too_large(LIMIT + 1, ' ');
    std::string long_string = "\"" + std::string(LIMIT - 2, 'a') + "\""; // refused at the string limit

    measure_calls("core.json.hostile_wide_array", "ns_per_call", 5, [&] { use(json::parse(wide).has_value()); });
    measure_calls("core.json.hostile_many_members", "ns_per_call", 5, [&] { use(json::parse(members).has_value()); });
    measure_calls("core.json.hostile_deep_nesting", "ns_per_call", 200, [&] { use(json::parse(deep).has_value()); });
    measure_calls("core.json.hostile_too_large", "ns_per_call", 200, [&] { use(json::parse(too_large).has_value()); });
    measure_calls("core.json.hostile_long_string", "ns_per_call", 200,
                  [&] { use(json::parse(long_string).has_value()); });
    measure_calls("core.save.hostile_wide_array", "ns_per_call", 5,
                  [&] { use(static_cast<std::uint64_t>(parse_save(wide).status)); });
}

TEST_CASE("Every golden replay parses and plays fast", "[perf][core][replay]")
{
    for (const GoldenReplay& golden : GOLDEN_REPLAYS)
    {
        const std::string name = "core.replay." + std::string(golden.name);
        const auto replay = parse_replay(golden.text);
        REQUIRE(replay.has_value());
        const auto first = run_replay(*replay);
        REQUIRE(first.has_value());
        // The speed is of a run that is still the recorded one.
        REQUIRE(replay->expect.has_value());
        REQUIRE(first->events_hash == replay->expect->events_hash);
        REQUIRE(first->final_hash == replay->expect->final_hash);

        measure_calls(name + ".parse", "ns_per_call", 50, [&] { use(parse_replay(golden.text)->commands.size()); });
        measure_calls(name + ".run", "ns_per_call", 20,
                      [&] { use(static_cast<std::uint64_t>(run_replay(*replay)->score)); });
    }
}
