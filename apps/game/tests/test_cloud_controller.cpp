// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "heart_boom_maths/save_data.hpp"
#include "heart_boom_maths/save_merge.hpp"
#include "heart_boom_maths/save_store.hpp"
#include "parental_gate.hpp"
#include "services/clock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;
using State = GameController::State;

namespace
{

constexpr std::uint64_t GATE_SEED = 41;
constexpr std::uint64_t DEVICE_A_BITS = 0x00000000000000A1ULL;
constexpr std::uint64_t DEVICE_B_BITS = 0x00000000000000B2ULL;
constexpr std::string_view DEVICE_A_ID = "00000000000000a1";
constexpr std::string_view DEVICE_B_ID = "00000000000000b2";
constexpr std::string_view OTHER_DEVICE_ID = "00000000000000c3";

/// One device: its own save, its own recorded services, a controller, and the cloud store of the account it shares with
/// the other devices of the test.
struct Device
{
    explicit Device(std::shared_ptr<CloudAccount> account, std::uint64_t first_draw, std::uint64_t other_ids,
                    std::optional<std::string> saved_text = std::nullopt, bool cloud_available = true)
        : memory(saved_text.has_value() ? MemorySaveStore(*saved_text) : MemorySaveStore()),
          cloud(std::move(account), cloud_available), m_FirstDraw(first_draw), m_OtherIds(other_ids),
          controller(make_services(), make_options())
    {
    }

    GameServices make_services()
    {
        GameServices services;
        services.store = &memory;
        services.audio = &audio;
        services.scores = &scores;
        services.haptics = &haptics;
        services.links = &links;
        services.clock = &clock;
        services.cloud = &cloud;
        return services;
    }

    GameOptions make_options()
    {
        GameOptions options;
        options.session_seed = 20261115;
        options.tile_guard_ns = 0;
        options.gate_seed_source = [] { return GATE_SEED; };
        // The first draw is the device's id; every later one is another id.
        options.id_source = [this] { return m_Draws++ == 0 ? m_FirstDraw : m_OtherIds + m_Draws; };
        return options;
    }

    void turn_on()
    {
        controller.requestCloudSync(true);
        REQUIRE(controller.gate_active());
        REQUIRE(controller.submitGate(QString::number(make_gate_challenge(GATE_SEED).number)));
    }

    void answer_right()
    {
        controller.answer(controller.session_view().question.correct_tile);
    }

    /// Ten right answers: level 1 is cleared.
    void clear_level()
    {
        for (int i = 0; i < 10; ++i)
        {
            answer_right();
        }
        REQUIRE(controller.state() == State::LevelClear);
    }

    /// Pause, then quit: the game is over. The score may earn a name screen.
    void quit_from_play()
    {
        controller.pause();
        controller.quit();
        REQUIRE(controller.state() == State::GameOver);
    }

    /// A whole game that earns coins and ends. A player with no name meets the name screen, and SKIP drops the entry.
    void play_a_game()
    {
        controller.startGame();
        clear_level();
        controller.nextLevel();
        quit_from_play();
        if (controller.name_entry_pending())
        {
            controller.skipName();
        }
    }

    /// Lets the bomb go off: real time passes in whole ticks until the game is over.
    void run_out_of_time()
    {
        for (int i = 0; i < 2000 && controller.state() == State::Playing; ++i)
        {
            clock.advance_ms(50);
            controller.advance_time();
        }
        REQUIRE(controller.state() == State::GameOver);
    }

    [[nodiscard]] SaveData saved() const
    {
        REQUIRE(memory.text().has_value());
        const ParseResult parsed = parse_save(*memory.text());
        REQUIRE(parsed.status == LoadStatus::OK);
        return parsed.data;
    }

    [[nodiscard]] SharedData payload() const
    {
        REQUIRE(cloud.account->text.has_value());
        const SharedParseResult parsed = parse_shared(*cloud.account->text);
        REQUIRE(parsed.status == LoadStatus::OK);
        return parsed.data;
    }

    MemorySaveStore memory;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingHaptics haptics;
    RecordingLinks links;
    FakeCloudStore cloud;

private:
    std::uint64_t m_FirstDraw;
    std::uint64_t m_OtherIds;
    std::uint64_t m_Draws = 0;

public:
    GameController controller;
};

/// A save with the switch already on, for a device that was set up before.
std::string saved_with_sync_on()
{
    SaveData data;
    data.device.cloud_sync = true;
    return *serialise(data);
}

/// What another device would have put in the cloud: the first player with this many coins earned on that device.
std::string payload_with_coins(std::int64_t coins, std::string_view device_id = OTHER_DEVICE_ID)
{
    SaveData other;
    REQUIRE(assign_device_id(other, std::string(device_id)));
    give_wallet(other, coins);
    return *serialise_shared(other.shared);
}

std::int64_t wallet_in(const SharedData& shared, std::string_view player_id)
{
    const Player* player = find_player(shared, player_id);
    REQUIRE(player != nullptr);
    return player_wallet(*player);
}

/// The coins this device has taken off a player.
std::int64_t spent_by(const SaveData& data, std::string_view player_id, std::string_view device_id)
{
    const Player* player = find_player(data.shared, player_id);
    REQUIRE(player != nullptr);
    for (const CoinEntry& entry : player->coins)
    {
        if (entry.device == device_id)
        {
            return entry.spent;
        }
    }
    return 0;
}

std::int64_t spent_in_total(const SaveData& data, std::string_view player_id)
{
    const Player* player = find_player(data.shared, player_id);
    REQUIRE(player != nullptr);
    std::int64_t total = 0;
    for (const CoinEntry& entry : player->coins)
    {
        total += entry.spent;
    }
    return total;
}

} // namespace

TEST_CASE("The switch is off by default, and the store is not touched until it is on", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000);
    GameController& c = device.controller;

    CHECK(c.cloud_sync_available());
    CHECK_FALSE(c.cloud_sync_enabled());
    // A whole game, a setting, a rename, and leaving and coming back: with the switch off none of it reaches the store.
    device.play_a_game();
    c.setSoundEnabled(false);
    CHECK(c.renamePlayer(c.player_id(), QStringLiteral("AMY")));
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK(device.cloud.calls() == 0);
    CHECK_FALSE(account->text.has_value());
    CHECK_FALSE(device.saved().device.cloud_sync);
}

TEST_CASE("Turning sync on opens the parental gate and sets the switch only when it is passed", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000);
    GameController& c = device.controller;
    QSignalSpy settings_spy(&c, &GameController::settingsChanged);

    c.requestCloudSync(true);
    REQUIRE(c.gate_active());
    CHECK(c.gate_purpose() == GameController::GatePurpose::CloudSync);
    CHECK_FALSE(c.cloud_sync_enabled());
    CHECK(device.cloud.calls() == 0); // not while the gate is open

    CHECK_FALSE(c.submitGate(QString::number(make_gate_challenge(GATE_SEED).number + 1)));
    CHECK_FALSE(c.cloud_sync_enabled());
    c.cancelGate();
    CHECK_FALSE(c.gate_active());
    CHECK_FALSE(c.cloud_sync_enabled());
    CHECK(device.cloud.calls() == 0);
    CHECK(settings_spy.count() == 0);

    device.turn_on();
    CHECK(c.cloud_sync_enabled());
    CHECK(settings_spy.count() >= 1);
    CHECK(device.saved().device.cloud_sync);
    // The switch being turned on is a trigger: the store is listened to and one sync ran (nothing in the cloud, so this
    // device's part was written).
    CHECK(device.cloud.handlers_installed == 1);
    CHECK(device.cloud.reads == 1);
    CHECK(device.cloud.writes == 1);
    CHECK(device.payload() == device.saved().shared);
}

TEST_CASE("Turning sync off needs no gate, stops all store use and leaves the cloud copy as it is",
          "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000);
    GameController& c = device.controller;
    device.turn_on();
    const std::string in_cloud = *account->text;

    c.requestCloudSync(false);
    CHECK_FALSE(c.gate_active());
    CHECK_FALSE(c.cloud_sync_enabled());
    CHECK_FALSE(device.saved().device.cloud_sync);
    CHECK(device.cloud.handlers_removed == 1);

    const int calls = device.cloud.calls();
    device.play_a_game();
    CHECK(c.renamePlayer(c.player_id(), QStringLiteral("AMY")));
    c.on_application_state(false);
    c.on_application_state(true);
    device.cloud.report_change(); // the handler is gone: nothing listens
    CHECK(device.cloud.calls() == calls);
    CHECK(*account->text == in_cloud);

    // Asking to turn it off again does nothing, and turning it on again needs the gate again.
    c.requestCloudSync(false);
    CHECK(device.cloud.calls() == calls);
    c.requestCloudSync(true);
    CHECK(c.gate_active());
}

TEST_CASE("Where the store cannot sync the switch is not offered and the store is never used", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on(), false);
    GameController& c = device.controller;

    CHECK_FALSE(c.cloud_sync_available());
    CHECK_FALSE(c.cloud_sync_enabled()); // the save says on, but nothing can run
    c.requestCloudSync(true);
    CHECK_FALSE(c.gate_active());
    device.play_a_game();
    CHECK(device.cloud.calls() == 0);
}

TEST_CASE("A device with no cloud store at all is the same as one that cannot sync", "[integration][cloud]")
{
    MemorySaveStore memory;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    GameServices services;
    services.store = &memory;
    services.audio = &audio;
    services.scores = &scores;
    services.links = &links;
    services.clock = &clock; // no cloud
    GameController controller(services);
    CHECK_FALSE(controller.cloud_sync_available());
    CHECK_FALSE(controller.cloud_sync_enabled());
    controller.requestCloudSync(true);
    CHECK_FALSE(controller.gate_active());
}

TEST_CASE("A save from a newer build keeps the switch out of reach", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, std::string(R"({"version":99})"));
    GameController& c = device.controller;
    REQUIRE(c.save_blocked());
    c.requestCloudSync(true);
    CHECK_FALSE(c.gate_active());
    CHECK_FALSE(c.cloud_sync_enabled());
    CHECK(device.cloud.calls() == 0);
}

TEST_CASE("Start-up with the switch on listens and takes what another device sent", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    account->text = payload_with_coins(55);
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;

    CHECK(c.cloud_sync_enabled());
    CHECK(device.cloud.handlers_installed == 1);
    CHECK(device.cloud.reads == 1);
    CHECK(c.wallet() == 55);
    CHECK(wallet_in(device.saved().shared, FIRST_PLAYER_ID) == 55); // and the save was written
}

TEST_CASE("The app becoming active is a trigger", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    c.on_application_state(false); // leaving saves, which is a trigger of its own
    const int reads = device.cloud.reads;

    account->text = payload_with_coins(30); // another device wrote while this one was away
    CHECK(c.wallet() == 0);
    c.on_application_state(true);
    CHECK(device.cloud.reads == reads + 1);
    CHECK(c.wallet() == 30);
}

TEST_CASE("The store's change handler is a trigger", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    const int reads = device.cloud.reads;

    account->text = payload_with_coins(12);
    device.cloud.report_change();
    CHECK(device.cloud.reads == reads + 1);
    CHECK(c.wallet() == 12);
}

TEST_CASE("A save at the title is a trigger, and sends what changed", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    const int reads = device.cloud.reads;

    CHECK(c.renamePlayer(c.player_id(), QStringLiteral("AMY")));
    CHECK(device.cloud.reads == reads + 1);
    CHECK(find_player(device.payload(), FIRST_PLAYER_ID)->name == "AMY");
}

TEST_CASE("A trigger during play only marks a sync as owed, and it runs when the pause screen opens",
          "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    c.startGame();
    device.answer_right();
    const int reads = device.cloud.reads;
    const std::int64_t wallet = c.wallet();

    account->text = payload_with_coins(40);
    device.cloud.report_change();
    c.on_application_state(true);
    device.answer_right(); // a persist at a moment the session cannot take a wallet is no trigger that runs
    CHECK(device.cloud.reads == reads);
    CHECK(c.wallet() == wallet + 1);

    c.pause();
    CHECK(c.state() == State::Paused);
    CHECK(device.cloud.reads == reads + 1);
    CHECK(c.wallet() == wallet + 1 + 40); // the pause screen takes the changed wallet
    c.resume();
    CHECK(c.wallet() == wallet + 1 + 40);
}

TEST_CASE("A trigger on the level-clear card waits, and a sync runs when the game ends and no name is waiting",
          "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    CHECK(c.renamePlayer(c.player_id(), QStringLiteral("AMY"))); // a named player: the score is saved at once
    c.startGame();
    device.clear_level();
    const int reads = device.cloud.reads;

    account->text = payload_with_coins(40);
    device.cloud.report_change();
    CHECK(device.cloud.reads == reads); // the card is no place to change the wallet
    c.nextLevel();
    CHECK(device.cloud.reads == reads);

    device.run_out_of_time(); // the bomb goes off: game over, nothing waiting for a name
    CHECK(c.state() == State::GameOver);
    CHECK_FALSE(c.name_entry_pending());
    CHECK(device.cloud.reads == reads + 1);
    CHECK(wallet_in(device.saved().shared, FIRST_PLAYER_ID) == c.wallet());
    CHECK(c.wallet() >= 40);
}

TEST_CASE("A change that arrives while a name is pending waits for the name", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    // Another device has played and named the player.
    Device other(account, DEVICE_B_BITS, 0x2000);
    other.turn_on();
    REQUIRE(other.controller.renamePlayer(other.controller.player_id(), QStringLiteral("BEN")));
    other.play_a_game();
    const std::int64_t other_score = other.controller.best_score();
    REQUIRE(other_score > 0);

    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    // The start-up sync took the name and the score; the player here has a name now, so make a new, unnamed player.
    const QString second = c.addPlayer();
    REQUIRE_FALSE(second.isEmpty());
    c.startGame();
    device.clear_level();
    c.nextLevel();
    device.quit_from_play();
    REQUIRE(c.name_entry_pending());
    const int rank = c.name_entry_rank();
    const int reads = device.cloud.reads;

    other.play_a_game(); // the other device ends another game, and the store tells this one
    device.cloud.report_change();
    CHECK(device.cloud.reads == reads); // not while the name screen is up
    CHECK(c.name_entry_pending());
    CHECK(c.name_entry_rank() == rank);

    CHECK(c.saveName(QStringLiteral("zed")) == rank);
    CHECK(device.cloud.reads == reads + 1); // the name was saved, and the owed sync ran after it
    CHECK(c.top10().size() >= 2);
}

TEST_CASE("The other device's changes reach every property a screen binds to", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device other(account, DEVICE_B_BITS, 0x2000);
    other.turn_on();
    REQUIRE(other.controller.renamePlayer(other.controller.player_id(), QStringLiteral("BEN")));
    other.play_a_game();

    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    // Start-up already took it; ask for the change once more after the other device moves on.
    const qint64 wallet_then = c.wallet();
    other.play_a_game();
    QSignalSpy players_spy(&c, &GameController::playersChanged);
    QSignalSpy wallet_spy(&c, &GameController::walletChanged);
    QSignalSpy top_spy(&c, &GameController::topScoresChanged);
    QSignalSpy run_spy(&c, &GameController::savedRunChanged);

    device.cloud.report_change();
    CHECK(c.wallet() > wallet_then);
    CHECK(c.wallet() == other.controller.wallet());
    CHECK(players_spy.count() >= 1);
    CHECK(wallet_spy.count() == 1);
    CHECK(top_spy.count() >= 1);
    CHECK(run_spy.count() >= 1);
    CHECK(c.player_name() == QStringLiteral("BEN"));
    CHECK(c.best_score() == other.controller.best_score());
    CHECK(c.top10().size() == other.controller.top10().size());
    CHECK(c.players().first().toMap().value(QStringLiteral("coins")).toLongLong() == c.wallet());
}

TEST_CASE("Coins earned on another device are never booked as spent here", "[integration][cloud]")
{
    // THE TRAP. save record_wallet() counts a fall of the session's wallet as coins spent. If coins from another device
    // were merged in while the session still held the old wallet, the next save would book them as spent and destroy
    // them. A sync therefore runs only where the session can take the wallet, and takes it at once.
    const auto account = std::make_shared<CloudAccount>();
    Device a(account, DEVICE_A_BITS, 0x1000);
    Device b(account, DEVICE_B_BITS, 0x2000);
    a.turn_on();
    b.turn_on();

    // Device B earns coins and ends its game; they reach the cloud.
    b.play_a_game();
    const std::int64_t b_earned = b.controller.wallet();
    REQUIRE(b_earned > 0);
    REQUIRE(wallet_in(b.payload(), FIRST_PLAYER_ID) == b_earned);

    // Device A is in the middle of a level when the change is reported.
    GameController& c = a.controller;
    c.startGame();
    for (int i = 0; i < 3; ++i)
    {
        a.answer_right();
    }
    const int reads = a.cloud.reads;
    a.cloud.report_change();
    CHECK(a.cloud.reads == reads);
    CHECK(c.wallet() == 3); // still its own

    // A finishes the level (the clear card is no moment either), plays on, and pauses.
    for (int i = 0; i < 7; ++i)
    {
        a.answer_right();
    }
    REQUIRE(c.state() == State::LevelClear);
    CHECK(a.cloud.reads == reads);
    c.nextLevel();
    a.answer_right();
    const std::int64_t a_earned_so_far = c.wallet();
    CHECK(a.cloud.reads == reads);
    c.pause();
    CHECK(a.cloud.reads == reads + 1);
    CHECK(c.wallet() == a_earned_so_far + b_earned);

    // It plays on and ends the game. Nothing is booked as spent, and the wallet is both devices' earnings.
    c.resume();
    a.answer_right();
    const std::int64_t at_the_end = c.wallet();
    CHECK(at_the_end == a_earned_so_far + b_earned + 1);
    a.quit_from_play();
    if (c.name_entry_pending())
    {
        c.skipName();
    }
    CHECK(c.wallet() == at_the_end);

    const SaveData saved = a.saved();
    CHECK(wallet_in(saved.shared, FIRST_PLAYER_ID) == at_the_end);
    CHECK(spent_by(saved, FIRST_PLAYER_ID, DEVICE_A_ID) == 0);
    CHECK(spent_by(saved, FIRST_PLAYER_ID, DEVICE_B_ID) == 0);
    CHECK(spent_in_total(saved, FIRST_PLAYER_ID) == 0);
    CHECK(wallet_in(a.payload(), FIRST_PLAYER_ID) == at_the_end);

    // And B, when it syncs again, has all of it as well.
    b.cloud.report_change();
    CHECK(b.controller.wallet() == at_the_end);
}

TEST_CASE("A saved wallet that a fall would book as spent shows why the session must take the merged one",
          "[unit][cloud]")
{
    SaveData here;
    REQUIRE(assign_device_id(here, std::string(DEVICE_A_ID)));
    give_wallet(here, 10);
    SaveData other;
    REQUIRE(assign_device_id(other, std::string(DEVICE_B_ID)));
    give_wallet(other, 25);

    const auto merged = merge(here.shared, other.shared);
    REQUIRE(merged.has_value());
    REQUIRE(adopt_shared(here, *merged, new_player_id(*merged, 1)));
    Player* player = current_player(here);
    REQUIRE(player != nullptr);
    REQUIRE(player_wallet(*player) == 35);

    // A session that still held 10 would write that back: a fall of 25, which is booked as coins spent.
    REQUIRE(record_wallet(*player, here.device.id, 10));
    CHECK(player_wallet(*player) == 10);
    CHECK(spent_by(here, FIRST_PLAYER_ID, DEVICE_A_ID) == 25);
}

TEST_CASE("The current player removed on another device is replaced when the change arrives", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device a(account, DEVICE_A_BITS, 0x1000);
    Device b(account, DEVICE_B_BITS, 0x2000);
    a.turn_on();
    const QString second = a.controller.addPlayer();
    REQUIRE_FALSE(second.isEmpty());
    REQUIRE(a.controller.player_id() == second);
    b.turn_on(); // takes the second player from the cloud

    REQUIRE(b.controller.requestRemovePlayer(second));
    REQUIRE(b.controller.submitGate(QString::number(make_gate_challenge(GATE_SEED).number)));
    REQUIRE(find_player(b.payload(), second.toStdString())->removed);

    QSignalSpy players_spy(&a.controller, &GameController::playersChanged);
    a.cloud.report_change();
    CHECK(a.controller.player_id() == QStringLiteral("first"));
    CHECK(a.controller.players().size() == 1);
    CHECK(players_spy.count() >= 1);
    CHECK(a.controller.can_remove_player() == false);
    CHECK(a.saved().device.current_player == FIRST_PLAYER_ID);
}

TEST_CASE("A payload from a later build is left alone and the game plays on", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    account->text = std::string(R"({"version":99,"shared":{"from":"a later build"}})");
    Device device(account, DEVICE_A_BITS, 0x1000);
    device.turn_on();
    device.play_a_game();
    CHECK(device.cloud.writes == 0);
    CHECK(*account->text == R"({"version":99,"shared":{"from":"a later build"}})");
    CHECK(device.controller.wallet() > 0);
}

TEST_CASE("A payload that cannot be read is replaced, and never damages the local save", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    account->text = std::string("}{ not a payload");
    Device device(account, DEVICE_A_BITS, 0x1000);
    device.turn_on();
    CHECK(device.payload() == device.saved().shared);
    CHECK(device.controller.cloud_sync_enabled());
}

TEST_CASE("A store that refuses a write does not stop the game, and the next sync tries again", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000);
    device.cloud.fail_writes = true;
    device.turn_on();
    device.play_a_game();
    CHECK_FALSE(account->text.has_value());
    CHECK(device.controller.wallet() > 0);

    device.cloud.fail_writes = false;
    device.cloud.report_change();
    CHECK(device.payload() == device.saved().shared);
}

TEST_CASE("Signing out of iCloud takes the row away, and signing back in resumes", "[integration][cloud]")
{
    const auto account = std::make_shared<CloudAccount>();
    Device device(account, DEVICE_A_BITS, 0x1000, saved_with_sync_on());
    GameController& c = device.controller;
    REQUIRE(c.cloud_sync_enabled());
    QSignalSpy settings_spy(&c, &GameController::settingsChanged);

    device.cloud.set_available(false);
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK_FALSE(c.cloud_sync_available());
    CHECK_FALSE(c.cloud_sync_enabled());
    CHECK(settings_spy.count() >= 1);
    CHECK(device.cloud.handlers_removed == 1);
    CHECK(device.saved().device.cloud_sync); // the person's choice is kept

    device.cloud.set_available(true);
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK(c.cloud_sync_available());
    CHECK(c.cloud_sync_enabled());
    CHECK(device.cloud.handlers_installed == 2);
}

TEST_CASE("The controller stops listening when it goes away", "[integration][cloud]")
{
    MemorySaveStore memory(saved_with_sync_on());
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    FakeCloudStore cloud;
    GameServices services;
    services.store = &memory;
    services.audio = &audio;
    services.scores = &scores;
    services.links = &links;
    services.clock = &clock;
    services.cloud = &cloud;
    {
        GameController controller(services);
        REQUIRE(cloud.handlers_installed == 1);
        REQUIRE(static_cast<bool>(cloud.handler));
    }
    CHECK(cloud.handlers_removed == 1);
    CHECK_FALSE(static_cast<bool>(cloud.handler));
    cloud.report_change(); // nothing is left to call
}
