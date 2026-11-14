// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.hpp"
#include "game_controller.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "services/clock.hpp"
#include "services/file_save_store.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

using namespace heart_boom_maths;
using namespace heart_boom_maths::game;
using namespace hbm_test;

namespace
{

QByteArray read_file(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

void write_file(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

/// A controller on a file store, with every other service recorded.
struct FileRig
{
    explicit FileRig(const QString& directory)
        : store(directory), controller(
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
                                }())
    {
    }

    FileSaveStore store;
    ManualClock clock;
    RecordingAudio audio;
    RecordingScores scores;
    RecordingLinks links;
    GameController controller;
};

} // namespace

TEST_CASE("The file store reads nothing when there is no file", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FileSaveStore store(directory.path());
    CHECK_FALSE(store.load().has_value());
}

TEST_CASE("The file store writes atomically and reads back", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FileSaveStore store(directory.path());

    REQUIRE(store.store("first"));
    CHECK(store.load() == std::optional<std::string>("first"));
    REQUIRE(store.store("second, longer text"));
    CHECK(store.load() == std::optional<std::string>("second, longer text"));
    REQUIRE(store.store(""));
    CHECK(store.load() == std::optional<std::string>(""));

    // The temporary file of the atomic write is gone: only the save is left.
    CHECK(QDir(directory.path()).entryList(QDir::Files) == QStringList{QStringLiteral("save.json")});
}

TEST_CASE("The file store creates its folder", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FileSaveStore store(directory.filePath(QStringLiteral("a/b/c")));
    REQUIRE(store.store("text"));
    CHECK(QFile::exists(store.file_path()));
}

TEST_CASE("The file store reports a write it cannot make and keeps the old text", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    // The folder is a file, so nothing can be written under it.
    const QString blocker = directory.filePath(QStringLiteral("blocker"));
    write_file(blocker, "x");
    FileSaveStore blocked(blocker);
    CHECK_FALSE(blocked.store("text"));

    // A target that is a directory cannot be replaced either.
    FileSaveStore store(directory.path());
    REQUIRE(QDir().mkdir(store.file_path()));
    CHECK_FALSE(store.store("text"));
}

TEST_CASE("Setting a damaged file aside keeps its bytes and frees the name", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FileSaveStore store(directory.path());
    write_file(store.file_path(), "garbage one");
    REQUIRE(store.set_damaged_aside());
    CHECK_FALSE(QFile::exists(store.file_path()));
    CHECK(read_file(store.damaged_file_path()) == "garbage one");

    // A second damaged file replaces the first .bad.
    write_file(store.file_path(), "garbage two");
    REQUIRE(store.set_damaged_aside());
    CHECK(read_file(store.damaged_file_path()) == "garbage two");

    CHECK_FALSE(store.set_damaged_aside()); // nothing left to move
}

TEST_CASE("The default folder is a per-user data folder", "[integration][file_store]")
{
    CHECK_FALSE(FileSaveStore::default_directory().isEmpty());
}

TEST_CASE("A damaged save is moved aside and the game starts on defaults", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("save.json"));
    write_file(path, "{ this is not json");

    FileRig rig(directory.path());
    CHECK(rig.controller.wallet() == 0);
    CHECK_FALSE(rig.controller.save_blocked());
    CHECK_FALSE(QFile::exists(path));
    CHECK(read_file(path + QStringLiteral(".bad")) == "{ this is not json");

    // The next save writes a fresh, valid file and leaves the .bad file alone.
    rig.controller.setSoundEnabled(false);
    const ParseResult parsed = parse_save(read_file(path).toStdString());
    CHECK(parsed.status == LoadStatus::OK);
    CHECK_FALSE(parsed.data.sound);
    CHECK(read_file(path + QStringLiteral(".bad")) == "{ this is not json");
}

TEST_CASE("A save from a newer version is never overwritten", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = QDir(directory.path()).filePath(QStringLiteral("save.json"));
    const QByteArray newer = R"({"version": 99, "coins": 5, "something_new": [1, 2, 3]})";
    write_file(path, newer);

    FileRig rig(directory.path());
    CHECK(rig.controller.save_blocked());
    CHECK(rig.controller.wallet() == 0);

    // A whole session on defaults: play, pause, settings, quit, the app leaving the foreground.
    rig.controller.setMusicEnabled(false);
    rig.controller.startGame();
    rig.controller.answer(rig.controller.session_view().question.correct_tile);
    rig.controller.pause();
    rig.controller.on_application_state(false);
    rig.controller.quit();

    CHECK(read_file(path) == newer);
    CHECK_FALSE(QFile::exists(path + QStringLiteral(".bad")));
}

TEST_CASE("The wallet, settings and top 10 survive a restart through the file", "[integration][file_store]")
{
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    {
        FileRig rig(directory.path());
        rig.controller.setSoundEnabled(false);
        rig.controller.startGame();
        for (int i = 0; i < 3; ++i)
        {
            rig.controller.answer(rig.controller.session_view().question.correct_tile);
        }
        rig.controller.pause();
        rig.controller.quit();
        REQUIRE(rig.controller.name_entry_pending());
        CHECK(rig.controller.saveName(QStringLiteral("zed")) == 1);
    }
    FileRig again(directory.path());
    CHECK_FALSE(again.controller.sound_enabled());
    CHECK(again.controller.wallet() == 3); // a coin for each right answer; the streak bonus starts at five
    REQUIRE(again.controller.top10().size() == 1);
    CHECK(again.controller.best_score() == again.controller.top10().first().toMap().value("score").toLongLong());
    CHECK(again.controller.top10().first().toMap().value("name").toString() == QStringLiteral("ZED"));
    CHECK_FALSE(again.controller.has_saved_run());
}
