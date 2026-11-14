// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "parental_gate.hpp"
#include "services/clock.hpp"
#include "services/game_center_backend.hpp"
#include "services/game_center_score_service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;

namespace
{

constexpr std::uint64_t GATE_SEED = 31;

/// What the fake GameKit saw. It lives apart from the backend because the service owns the backend.
struct BackendLog
{
    int started = 0;
    int stopped = 0;
    std::vector<GameCenterBackend::StatusCallback> handlers;
    std::vector<std::pair<std::int64_t, std::string>> submitted;

    /// The platform answers the newest sign-in handler.
    void report(bool authenticated, bool restricted = false) const
    {
        REQUIRE_FALSE(handlers.empty());
        handlers.back()(GameCenterStatus{authenticated, restricted});
    }
};

class FakeBackend final : public GameCenterBackend
{
public:
    explicit FakeBackend(std::shared_ptr<BackendLog> log) : m_Log(std::move(log)) {}

    void start_authentication(StatusCallback on_status) override
    {
        ++m_Log->started;
        m_Log->handlers.push_back(std::move(on_status));
    }

    void stop_authentication() override
    {
        ++m_Log->stopped;
    }

    void submit_score(std::int64_t score, std::string_view leaderboard_id) override
    {
        m_Log->submitted.emplace_back(score, std::string(leaderboard_id));
    }

private:
    std::shared_ptr<BackendLog> m_Log;
};

std::unique_ptr<GameCenterScoreService> make_service(const std::shared_ptr<BackendLog>& log)
{
    return std::make_unique<GameCenterScoreService>(std::make_unique<FakeBackend>(log));
}

} // namespace

TEST_CASE("The Game Center service is available and idle until the switch is on", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    const auto service = make_service(log);

    CHECK(service->available());
    // A default install: the controller tells the service the switch is off, and GameKit is not touched.
    service->set_enabled(false);
    service->submit(100);
    CHECK(log->started == 0);
    CHECK(log->stopped == 0);
    CHECK(log->submitted.empty());
}

TEST_CASE("The sign-in handler is installed only when the switch is turned on, and once", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    const auto service = make_service(log);

    service->set_enabled(true);
    CHECK(log->started == 1);
    service->set_enabled(true); // already on
    CHECK(log->started == 1);

    service->set_enabled(false);
    CHECK(log->stopped == 1);
    service->set_enabled(false); // already off
    CHECK(log->stopped == 1);

    service->set_enabled(true);
    CHECK(log->started == 2);
}

TEST_CASE("A score is sent only for a signed-in, unrestricted player", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    const auto service = make_service(log);
    service->set_enabled(true);

    service->submit(50); // the sign-in has not answered yet
    CHECK(log->submitted.empty());

    log->report(false);
    service->submit(50); // not signed in
    CHECK(log->submitted.empty());

    log->report(false, true);
    service->submit(50); // parental controls
    CHECK(log->submitted.empty());

    log->report(true, true);
    service->submit(50); // signed in, but restricted
    CHECK(log->submitted.empty());

    log->report(true);
    service->submit(50);
    REQUIRE(log->submitted.size() == 1);
    CHECK(log->submitted.front().first == 50);
    CHECK(log->submitted.front().second == GAME_CENTER_LEADERBOARD_ID);

    log->report(false); // signed out in Settings
    service->submit(60);
    CHECK(log->submitted.size() == 1);
}

TEST_CASE("The service sends what it is given and does not filter repeats or drop a zero", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    const auto service = make_service(log);
    service->set_enabled(true);
    log->report(true);

    service->submit(0);
    service->submit(700);
    service->submit(700); // two games can end on the same score
    service->submit(-1);  // never a real score
    REQUIRE(log->submitted.size() == 3);
    CHECK(log->submitted[0].first == 0);
    CHECK(log->submitted[1].first == 700);
    CHECK(log->submitted[2].first == 700);
}

TEST_CASE("The leaderboard identifier is the one given, and a placeholder by default", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    GameCenterScoreService service(std::make_unique<FakeBackend>(log), "example.board");
    service.set_enabled(true);
    log->report(true);
    service.submit(5);
    REQUIRE(log->submitted.size() == 1);
    CHECK(log->submitted.front().second == "example.board");

    CHECK(GAME_CENTER_LEADERBOARD_ID.find("placeholder") != std::string_view::npos);
}

TEST_CASE("Turning the switch off ends submission, and a late answer does not undo it", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    const auto service = make_service(log);
    service->set_enabled(true);
    log->report(true);
    service->submit(1);
    REQUIRE(log->submitted.size() == 1);

    service->set_enabled(false);
    service->submit(2);
    CHECK(log->submitted.size() == 1);

    log->report(true); // the platform answers after the switch went off
    service->submit(3);
    CHECK(log->submitted.size() == 1);

    // On again: the old handler's answer means nothing, the new one's does.
    service->set_enabled(true);
    REQUIRE(log->handlers.size() == 2);
    log->handlers.front()(GameCenterStatus{true, false});
    service->submit(4);
    CHECK(log->submitted.size() == 1);
    log->report(true);
    service->submit(5);
    REQUIRE(log->submitted.size() == 2);
    CHECK(log->submitted.back().first == 5);
}

TEST_CASE("An answer that arrives after the service is gone is harmless", "[unit][game_center]")
{
    const auto log = std::make_shared<BackendLog>();
    {
        const auto service = make_service(log);
        service->set_enabled(true);
    }
    log->report(true);
    CHECK(log->submitted.empty());
}

namespace
{

/// The controller over the real Game Center service and the fake GameKit, with the other services recorded.
struct CenterRig
{
    CenterRig()
        : log(std::make_shared<BackendLog>()), service(make_service(log)), controller(make_services(), make_options())
    {
    }

    GameServices make_services()
    {
        GameServices services;
        services.store = &memory;
        services.audio = &audio;
        services.scores = service.get();
        services.links = &links;
        services.clock = &clock;
        return services;
    }

    static GameOptions make_options()
    {
        GameOptions options;
        options.session_seed = 20261114;
        options.gate_seed_source = [] { return GATE_SEED; };
        return options;
    }

    void turn_on_behind_the_gate()
    {
        controller.requestGameCenter(true);
        REQUIRE(controller.gate_active());
        REQUIRE(controller.submitGate(QString::number(make_gate_challenge(GATE_SEED).number)));
    }

    void play_one_game_and_quit()
    {
        controller.startGame();
        controller.answer(controller.session_view().question.correct_tile);
        controller.pause();
        controller.quit();
    }

    std::shared_ptr<BackendLog> log;
    std::unique_ptr<GameCenterScoreService> service;
    MemorySaveStore memory;
    ManualClock clock;
    RecordingAudio audio;
    RecordingLinks links;
    GameController controller;
};

} // namespace

TEST_CASE("The Settings row turns the service on only after the gate", "[integration][game_center]")
{
    CenterRig rig;
    GameController& c = rig.controller;
    CHECK(c.game_center_available());
    CHECK_FALSE(c.game_center_enabled());
    CHECK(rig.log->started == 0); // a default install never installs the sign-in handler

    c.requestGameCenter(true);
    REQUIRE(c.gate_active());
    CHECK(rig.log->started == 0); // not while the gate is open
    CHECK_FALSE(c.submitGate(QString::number(make_gate_challenge(GATE_SEED).number + 1)));
    CHECK(rig.log->started == 0);
    c.cancelGate();
    CHECK(rig.log->started == 0);

    rig.turn_on_behind_the_gate();
    CHECK(c.game_center_enabled());
    CHECK(rig.log->started == 1);

    c.requestGameCenter(false);
    CHECK_FALSE(c.game_center_enabled());
    CHECK(rig.log->stopped == 1);
}

TEST_CASE("A finished game sends its score once, only when the switch is on and the player is signed in",
          "[integration][game_center]")
{
    CenterRig off;
    off.play_one_game_and_quit();
    CHECK(off.log->submitted.empty()); // the switch is off: the game ended and nothing was sent

    CenterRig rig;
    rig.turn_on_behind_the_gate();
    rig.log->report(true);
    rig.play_one_game_and_quit();
    REQUIRE(rig.log->submitted.size() == 1);
    CHECK(rig.log->submitted.front().first == rig.controller.final_score());

    // Left, and entered again: still the one score of the one game.
    rig.controller.on_application_state(false);
    rig.controller.on_application_state(true);
    CHECK(rig.log->submitted.size() == 1);
}
