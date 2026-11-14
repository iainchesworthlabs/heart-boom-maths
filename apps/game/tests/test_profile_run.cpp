// SPDX-License-Identifier: GPL-3.0-or-later
// What only a profiling build contains: the arithmetic of the scripted run and the reader of the scene graph's log.
#include "profile_run.hpp"
#include "scene_graph_log.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string_view>
#include <vector>

using namespace heart_boom_maths::game::profiling;
using Catch::Approx;

TEST_CASE("A percentile is the nearest rank of the sorted values", "[unit][profiling]")
{
    const std::vector<double> values{10, 1, 5, 3, 2, 4, 9, 8, 7, 6};
    CHECK(percentile(values, 50.0) == Approx(5.0));
    CHECK(percentile(values, 90.0) == Approx(9.0));
    CHECK(percentile(values, 95.0) == Approx(10.0));
    CHECK(percentile(values, 100.0) == Approx(10.0));
    CHECK(percentile(values, 0.0) == Approx(1.0));
    CHECK(percentile({}, 95.0) == 0.0);
    CHECK(percentile({4.0}, 99.0) == Approx(4.0));
}

TEST_CASE("Frame statistics count the slow frames and leave out a GPU time that is not there", "[unit][profiling]")
{
    constexpr double REFRESH_MS = 1000.0 / 60.0;
    std::vector<PhaseFrame> frames(98, PhaseFrame{16.0, 4.0, 2.0});
    frames.push_back({40.0, 30.0, -1.0}); // a missed refresh, with no GPU time and work that did not fit in a refresh
    frames.push_back({100.0, 5.0, 4.0});
    frames.push_back({2000.0, 1.0, 1.0}); // a screen that stood still, not a late frame
    const FrameStatistics stats = summarise_frames(frames, REFRESH_MS);
    CHECK(stats.frames == 100);
    CHECK(stats.idle_gaps == 1);
    CHECK(stats.dropped == 2);
    CHECK(stats.over_budget == 1);
    CHECK(stats.work_max_ms == Approx(30.0));
    CHECK(stats.work_mean_ms == Approx((98 * 4.0 + 30.0 + 5.0) / 100.0));
    CHECK(stats.max_ms == Approx(100.0));
    CHECK(stats.p95_ms == Approx(16.0));
    CHECK(stats.p99_ms == Approx(40.0));
    CHECK(stats.mean_ms == Approx((98 * 16.0 + 40.0 + 100.0) / 100.0));
    CHECK(stats.gpu_frames == 99);
    CHECK(stats.gpu_max_ms == Approx(4.0));
    CHECK(stats.gpu_mean_ms == Approx((98 * 2.0 + 4.0) / 99.0));
}

TEST_CASE("No frames give no statistics", "[unit][profiling]")
{
    const FrameStatistics stats = summarise_frames({}, 16.0);
    CHECK(stats.frames == 0);
    CHECK(stats.dropped == 0);
    CHECK(stats.gpu_frames == 0);
    CHECK(stats.mean_ms == 0.0);
}

TEST_CASE("The run options are read from the command line", "[unit][profiling]")
{
    RunOptions options;
    CHECK_FALSE(parse_run_options(std::vector<std::string_view>{"--smoke-test"}, options));

    const std::vector<std::string_view> both{"--profile-run", "45", "--profile-summary", "out.json"};
    REQUIRE(parse_run_options(both, options));
    CHECK(options.seconds == 45);
    CHECK(options.summary_path == QStringLiteral("out.json"));

    // A length that is not a number leaves the default.
    RunOptions fallback;
    REQUIRE(parse_run_options(std::vector<std::string_view>{"--profile-run", "soon"}, fallback));
    CHECK(fallback.seconds == 90);
}

TEST_CASE("The reader of the scene graph's log finds the numbers Qt prints", "[unit][profiling]")
{
    SECTION("the opaque and alpha passes, which Qt prints in one message")
    {
        const SceneGraphLine passes = parse_scene_graph_line(
            "Rendering:\n -> Opaque: 4 nodes in 3 batches...\n -> Alpha: 52 nodes in 37 batches...\n - 0x1 [upload]");
        CHECK(passes.kind == SceneGraphLine::Kind::PASSES);
        CHECK(passes.a == 4);
        CHECK(passes.b == 3);
        CHECK(passes.c == 52);
        CHECK(passes.d == 37);
    }
    SECTION("the opaque pass alone")
    {
        const SceneGraphLine opaque = parse_scene_graph_line(" -> Opaque: 4 nodes in 3 batches...");
        CHECK(opaque.kind == SceneGraphLine::Kind::PASSES);
        CHECK(opaque.a == 4);
        CHECK(opaque.c == 0);
    }
    SECTION("the renderer's time")
    {
        const SceneGraphLine renderer =
            parse_scene_graph_line("time in renderer: total=17ms, preprocess=3, updates=1, rendering=14");
        CHECK(renderer.kind == SceneGraphLine::Kind::RENDERER);
        CHECK(renderer.a == 17);
        CHECK(renderer.b == 3);
        CHECK(renderer.c == 1);
        CHECK(renderer.d == 14);
    }
    SECTION("the frame's time")
    {
        const SceneGraphLine frame = parse_scene_graph_line(
            "[window 0x1][render thread 0x2] syncAndRender: frame rendered in 6ms, sync=1, render=4, swap=1");
        CHECK(frame.kind == SceneGraphLine::Kind::FRAME);
        CHECK(frame.a == 6);
        CHECK(frame.b == 1);
        CHECK(frame.c == 4);
        CHECK(frame.d == 1);
    }
    SECTION("a line it does not know")
    {
        CHECK(parse_scene_graph_line("qt.rhi.general: Adapter 0").kind == SceneGraphLine::Kind::NONE);
        CHECK(parse_scene_graph_line("").kind == SceneGraphLine::Kind::NONE);
    }
}
