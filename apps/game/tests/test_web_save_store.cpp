// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "services/app_save_store.hpp"
#include "services/clock.hpp"
#include "services/file_save_store.hpp"
#include "services/key_value_backend.hpp"
#include "services/qsettings_backend.hpp"
#include "services/web_save_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;

namespace
{

/// The state of a browser's storage, which outlives the page: a reload makes a new store over the same Storage.
struct FakeStorage
{
    std::map<std::string, std::string, std::less<>> values;
    bool blocked = false; // every access fails, as with cookies off or a sandboxed frame
    bool full = false;    // reads work and writes fail, as with a full quota
    bool remove_fails = false;
    int reads = 0;
    int writes = 0;
};

class FakeBackend final : public KeyValueBackend
{
public:
    explicit FakeBackend(FakeStorage& storage) : m_Storage(storage) {}

    std::optional<std::string> get(std::string_view key) override
    {
        ++m_Storage.reads;
        const auto found = m_Storage.values.find(key);
        if (m_Storage.blocked || found == m_Storage.values.end())
        {
            return std::nullopt;
        }
        return found->second;
    }

    bool set(std::string_view key, std::string_view value) override
    {
        ++m_Storage.writes;
        if (m_Storage.blocked || m_Storage.full)
        {
            return false;
        }
        m_Storage.values.insert_or_assign(std::string(key), std::string(value));
        return true;
    }

    bool remove(std::string_view key) override
    {
        if (m_Storage.blocked || m_Storage.remove_fails)
        {
            return false;
        }
        const auto found = m_Storage.values.find(key);
        if (found != m_Storage.values.end())
        {
            m_Storage.values.erase(found);
        }
        return true;
    }

private:
    FakeStorage& m_Storage;
};

WebSaveStore make_store(FakeStorage& storage)
{
    return WebSaveStore(std::make_unique<FakeBackend>(storage));
}

constexpr auto DAMAGED_TEXT = "{ this is not a save";

/// A controller on a browser store, with every other service recorded.
struct WebRig
{
    explicit WebRig(FakeStorage& storage)
        : store(make_store(storage)), controller(
                                          [this]
                                          {
                                              GameServices services;
                                              services.store = &store;
                                              services.quarantine = &store;
                                              services.audio = &audio;
                                              services.scores = &scores;
                                              services.links = &links;
                                              services.clock = &clock;
                                              return services;
                                          }(),
                                          []
                                          {
                                              GameOptions options;
                                              options.tile_guard_ns = 0; // taps straight after a level starts
                                              return options;
                                          }())
    {
    }

    WebSaveStore store;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    GameController controller;
};

} // namespace

TEST_CASE("The browser store reads nothing when nothing was saved", "[unit][web_store]")
{
    FakeStorage storage;
    WebSaveStore store = make_store(storage);
    CHECK_FALSE(store.load().has_value());
}

TEST_CASE("The browser store keeps the whole save under one key and replaces it", "[unit][web_store]")
{
    FakeStorage storage;
    WebSaveStore store = make_store(storage);
    const std::string text = "{\n  \"name\": \"Zo\xC3\xAB, \\\"Q\\\"\",\n  \"list\": [1, 2, 3]\n}\n";

    REQUIRE(store.store(text));
    CHECK(store.load() == text);
    REQUIRE(storage.values.size() == 1);
    CHECK(storage.values.begin()->first == WebSaveStore::SAVE_KEY);

    REQUIRE(store.store("{}"));
    CHECK(store.load() == "{}");
    CHECK(storage.values.size() == 1);

    // A reload is a new store over the same storage.
    WebSaveStore reloaded = make_store(storage);
    CHECK(reloaded.load() == "{}");
}

TEST_CASE("Blocked storage reads as a first run and refuses a write without a crash", "[unit][web_store]")
{
    FakeStorage storage;
    storage.blocked = true;
    WebSaveStore store = make_store(storage);
    CHECK_FALSE(store.load().has_value());
    CHECK_FALSE(store.store("{}"));
    CHECK_FALSE(store.load().has_value());
    // Nothing to put aside either, and asking does not fail.
    CHECK(store.set_damaged_aside());
}

TEST_CASE("A full storage keeps the old save", "[unit][web_store]")
{
    FakeStorage storage;
    WebSaveStore store = make_store(storage);
    REQUIRE(store.store("old"));
    storage.full = true;
    CHECK_FALSE(store.store("new"));
    CHECK(store.load() == "old");
}

TEST_CASE("A damaged save is kept under a second key before it can be replaced", "[unit][web_store]")
{
    FakeStorage storage;
    WebSaveStore store = make_store(storage);
    REQUIRE(store.store(DAMAGED_TEXT));

    REQUIRE(store.set_damaged_aside());
    CHECK_FALSE(store.load().has_value());
    REQUIRE(storage.values.count(std::string(WebSaveStore::DAMAGED_KEY)) == 1);
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == DAMAGED_TEXT);

    // The next save does not touch the copy.
    REQUIRE(store.store("{}"));
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == DAMAGED_TEXT);

    // Only the latest damaged save is kept.
    REQUIRE(store.store("{ also broken"));
    REQUIRE(store.set_damaged_aside());
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == "{ also broken");
}

TEST_CASE("A damaged save that cannot be copied stays where it is", "[unit][web_store]")
{
    FakeStorage storage;
    WebSaveStore store = make_store(storage);
    REQUIRE(store.store(DAMAGED_TEXT));
    storage.full = true;
    CHECK_FALSE(store.set_damaged_aside());
    CHECK(store.load() == DAMAGED_TEXT);
    CHECK(storage.values.count(std::string(WebSaveStore::DAMAGED_KEY)) == 0);

    // The copy is made but the original cannot be removed: the text is safe, and the answer says it was not moved.
    storage.full = false;
    storage.remove_fails = true;
    CHECK_FALSE(store.set_damaged_aside());
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == DAMAGED_TEXT);
}

TEST_CASE("The game runs on blocked storage and loses only the save", "[integration][web_store]")
{
    FakeStorage storage;
    storage.blocked = true;
    WebRig rig(storage);
    GameController& c = rig.controller;
    CHECK(c.wallet() == 0);

    // A whole session: play, answer, leave the page, come back, quit. Every write fails and nothing else does.
    c.startGame();
    c.answer(c.session_view().question.correct_tile);
    c.on_application_state(false);
    c.on_application_state(true);
    CHECK(c.wallet() == 1);
    c.pause();
    c.quit();
    CHECK(storage.writes > 0);
    CHECK(storage.values.empty());
}

TEST_CASE("The wallet and the paused run survive a reload through the browser store", "[integration][web_store]")
{
    FakeStorage storage;
    {
        WebRig rig(storage);
        rig.controller.setSoundEnabled(false);
        rig.controller.startGame();
        for (int i = 0; i < 3; ++i)
        {
            rig.controller.answer(rig.controller.session_view().question.correct_tile);
        }
        // A hidden tab arrives as the application leaving the foreground (see watch_page_visibility): pause and save.
        rig.controller.on_application_state(false);
        CHECK(rig.controller.state() == GameController::State::Paused);
        CHECK(rig.audio.suspends == 1);
    }
    WebRig again(storage);
    CHECK_FALSE(again.controller.sound_enabled());
    CHECK(again.controller.wallet() == 3);
    CHECK(again.controller.has_saved_run());
}

TEST_CASE("A damaged save in the browser store is kept and the game starts on defaults", "[integration][web_store]")
{
    FakeStorage storage;
    storage.values[std::string(WebSaveStore::SAVE_KEY)] = DAMAGED_TEXT;

    WebRig rig(storage);
    CHECK(rig.controller.wallet() == 0);
    CHECK_FALSE(rig.controller.save_blocked());
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == DAMAGED_TEXT);
    CHECK(storage.values.count(std::string(WebSaveStore::SAVE_KEY)) == 0);

    // The first save after that is a good one and leaves the copy alone.
    rig.controller.setMusicEnabled(false);
    rig.controller.on_application_state(false);
    CHECK(storage.values.count(std::string(WebSaveStore::SAVE_KEY)) == 1);
    CHECK(storage.values.at(std::string(WebSaveStore::DAMAGED_KEY)) == DAMAGED_TEXT);
}

TEST_CASE("A save from a newer build in the browser store is never overwritten", "[integration][web_store]")
{
    FakeStorage storage;
    const std::string newer = R"({"version": 99, "coins": 5, "something_new": [1, 2, 3]})";
    storage.values[std::string(WebSaveStore::SAVE_KEY)] = newer;

    WebRig rig(storage);
    CHECK(rig.controller.save_blocked());
    rig.controller.setMusicEnabled(false);
    rig.controller.startGame();
    rig.controller.answer(rig.controller.session_view().question.correct_tile);
    rig.controller.pause();
    rig.controller.on_application_state(false);
    rig.controller.quit();

    CHECK(storage.values.at(std::string(WebSaveStore::SAVE_KEY)) == newer);
    CHECK(storage.values.count(std::string(WebSaveStore::DAMAGED_KEY)) == 0);
}

TEST_CASE("The factory picks the store the platform names", "[unit][web_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    const auto file = make_save_store(platform::SaveStorage::LOCAL_FILE, directory.path());
    CHECK(dynamic_cast<FileSaveStore*>(file.get()) != nullptr);

    // The browser store goes to the backend it is given.
    FakeStorage storage;
    const auto browser = make_save_store(platform::SaveStorage::BROWSER, directory.path(),
                                         [&storage] { return std::make_unique<FakeBackend>(storage); });
    CHECK(dynamic_cast<WebSaveStore*>(browser.get()) != nullptr);
    REQUIRE(browser->store("kept"));
    CHECK(storage.values.at(std::string(WebSaveStore::SAVE_KEY)) == "kept");

    // A session-only store keeps the save for the life of the store, and a new one starts empty, as a reload does. The
    // backend is not asked for, so no real settings are touched.
    bool asked = false;
    const auto session = make_save_store(platform::SaveStorage::SESSION, directory.path(),
                                         [&asked]() -> std::unique_ptr<KeyValueBackend>
                                         {
                                             asked = true;
                                             return nullptr;
                                         });
    CHECK_FALSE(asked);
    REQUIRE(session->store("for now"));
    CHECK(session->load() == "for now");
    CHECK(session->set_damaged_aside());
    CHECK_FALSE(session->load().has_value());
    const auto next_session = make_save_store(platform::SaveStorage::SESSION, directory.path());
    CHECK_FALSE(next_session->load().has_value());
}

TEST_CASE("The memory backend keeps, replaces and forgets text", "[unit][web_store]")
{
    MemoryKeyValueBackend backend;
    CHECK_FALSE(backend.get("a").has_value());
    CHECK(backend.set("a", "1"));
    CHECK(backend.set("a", "2"));
    CHECK(backend.get("a") == "2");
    CHECK(backend.remove("a"));
    CHECK(backend.remove("a")); // not there: still removed
    CHECK_FALSE(backend.get("a").has_value());
}

TEST_CASE("QSettings keeps the save text exactly, across a reload", "[integration][web_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("settings.ini"));
    // The characters that QSettings treats as syntax in some formats, and Unicode.
    const std::string text =
        "{\n\t\"a,b\": [1, 2, 3],\n\t\"q\": \"say \\\"hi\\\"; @x = 100% \\\\ end\",\n\t\"u\": \"Zo\xC3\xAB\"\n}\n";
    {
        QSettingsBackend backend(std::make_unique<QSettings>(path, QSettings::IniFormat));
        CHECK_FALSE(backend.get("save.json").has_value());
        REQUIRE(backend.set("save.json", text));
        CHECK(backend.get("save.json") == text);
        REQUIRE(backend.set("save.json.bad", "{ broken"));
    }
    {
        QSettingsBackend reloaded(std::make_unique<QSettings>(path, QSettings::IniFormat));
        CHECK(reloaded.get("save.json") == text);
        CHECK(reloaded.get("save.json.bad") == "{ broken");
        REQUIRE(reloaded.remove("save.json"));
        CHECK_FALSE(reloaded.get("save.json").has_value());
        CHECK(reloaded.get("save.json.bad") == "{ broken");
    }
}

TEST_CASE("QSettings that cannot be written refuse the write", "[integration][web_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    // A file where the settings folder should be: nothing can be created under it.
    const QString blocker = QDir(directory.path()).filePath(QStringLiteral("blocker"));
    {
        QFile file(blocker);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
    QSettingsBackend backend(
        std::make_unique<QSettings>(QDir(blocker).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat));
    CHECK_FALSE(backend.set("save.json", "{}"));
    CHECK_FALSE(backend.remove("save.json"));
}
