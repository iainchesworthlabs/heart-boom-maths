# Heart Boom Maths: architecture

Source: Implementation Plan v1, 14 Nov 2026, with all twelve decisions accepted. Where this file and `SPEC.md` disagree about a game rule, `SPEC.md` wins. This file owns how the rules are built.

v1 is one deterministic C++23 game core inside a Qt 6 and QML shell. The build order is spec, then core, then screens.

## Repository

```
heart-boom-maths
  libs/core/             game rules, standard library only (include, src, tests, data)
  apps/game/             Qt and QML application
    qml/                 screens
    src/                 view-models, GameController, tick driver
    src/services/        service interfaces and shared code (save, audio, scores, links)
    src/platform/<os>/   the per-OS implementations
    fonts/               Bangers and Lilita One with their licence texts
  docs/                  this documentation (not a code layer)
  private/               not committed: design spec and boards
```

The handover called these `libs/core`, `app/`, `platform/`, `tuning/` and `tests/`. The layers below keep those names as concepts: the core layer is `libs/core`, the platform layer is `apps/game/src/services` plus `src/platform/<os>`, and the app layer is the rest of `apps/game`.

| Item | Choice |
|---|---|
| Repository | `iainchesworthlabs/heart-boom-maths` |
| Language | C++23 |
| UI | Qt 6 with QML |
| Build | CMake presets. vcpkg (the organisation's fork) for non-Qt dependencies. Qt installed with aqtinstall in CI |
| Licence | GPL-3.0-or-later (see the Qt and App Store risk in `DECISIONS.md`) |
| Branches | `develop` is the fast trunk. `main` carries releases only |
| CI | Staged tiers, actions pinned by commit SHA, ThreadSanitizer run required |
| Naming | PascalCase types, snake_case functions and locals, `m_PascalCase` members, UPPER_SNAKE constants, camelCase for names QML sees, Allman braces |

Fonts (Bangers and Lilita One, SIL OFL 1.1) are bundled in `apps/game/fonts/` with their licence text. Third-party licences and notices are collected into one generated file that About reads.

## Layers

Dependencies point down: the app layer uses the platform and core layers, the platform layer uses core types, and the core layer uses nothing.

1. `libs/core` includes the C++ standard library and nothing else. No Qt, file access, clock, threads or global state.
2. The platform layer declares the service interfaces and implements them once per OS. It may use Qt and core types.
3. The app layer may use both. It is the only place that holds a real-time clock, a `QObject` or a QML type.
4. The tests in `libs/core/tests` link `libs/core` alone, so the whole rule set runs without Qt or a display.

CMake enforces rule 1: the core target links no Qt target, and CI fails if a core file includes a Qt header. This keeps rules testable in milliseconds, makes replays repeat exactly, and lets the v2 ESP32-P4 handheld reuse `libs/core` unchanged.

## Game core

`GameSession` takes commands and returns events. It reads no clock and draws nothing, so one seed and one list of commands always produce the same run.

```cpp
class GameSession
{
public:
    GameSession(const Tuning& tuning, std::uint64_t seed);

    void start_game();
    void tick();
    void answer(int tile_index);
    void use_hint();
    void pause();
    void resume();
    void quit();        // QUIT on the pause screen
    void next_level();  // leaves the level-clear card
    void keep_going();  // win screen: into level 13
    void finish();      // win screen: end the run
    void set_wallet(std::int64_t coins);   // from the save, before a game starts
    void set_won_once(bool won_once);      // from the save, before a game starts

    const SessionView& view() const;
    std::vector<Event> take_events();
    std::vector<std::int64_t> snapshot() const;
    bool restore(std::span<const std::int64_t> values);
    std::uint64_t state_hash() const;
};
```

Events: `level_started`, `question_shown`, `answer_correct`, `answer_wrong`, `hint_used`, `alert_entered`, `level_cleared`, `game_won`, `game_over`. Each event carries the numbers the screens and sounds need (points, coins, streak, seconds added), so the app never recomputes a rule. The app reads `view()` every frame for what to draw.

The session is in one of six states (idle, playing, paused, level clear, won, game over), and a command the state does not accept is ignored. Pause and resume work from the Playing state only. Clearing level 12 pays the 1,000-coin bonus once and waits in the won state, where `keep_going()` starts level 13, the first endless level. The win screen is an app-layer choice between KEEP GOING and FINISH (see `SPEC.md`, Ending a game).

### Time

The core counts integer ticks. The first build uses 20 ticks per second, set by one constant in `tuning.json`:

| Quantity | Seconds | Ticks |
|---|---|---|
| Level time | 60 | 1,200 |
| Alert zone | 10 | 200 |
| Right answer | +7 | +140 |
| Wrong answer | −3 | −60 |

The app layer owns the real clock. It turns elapsed time into whole ticks and calls `tick()` that many times. Pausing means it stops calling. After a stall such as a window drag, catch-up is limited to a few ticks per frame.

### Random numbers

The core has its own small generator and its own bounded-integer function. It must not use `std::uniform_int_distribution` or other standard distributions, because their results differ between standard libraries and would break replays across platforms. The generator state is a few integers and is part of the saved run.

### State

All core state is integers: ticks, points, coins, run, step, counters. No floating point, so every platform produces the same state hash.

## Question generator

`level_spec(level)` returns the PRD level table row for levels 1 to 12. Endless levels use three small functions of k (level 13 is k = 1):

```cpp
constexpr int endless_level_seconds(int k)
{
    return std::max(60 - 2 * k, 30);
}

constexpr int endless_bonus_seconds(int k)
{
    return std::max(7 - (k - 1) / 3, 3);
}

constexpr int endless_multi_term_percent(int k)
{
    return std::min(40 + (k - 1) * 60 / 7, 100);
}
```

A test checks every row of the PRD endless table against these.

Dealing a question:

1. Pick the shape: from the level table for levels 1 to 12, and in endless by the multi-term percentage. Endless keeps level 12's range of 1,000.
2. Build operands from the result backwards: choose the result in range, then split it, so every intermediate value stays between 0 and the level's largest number and no retry loop is needed. Operands are at least 1.
3. Make three wrong tiles from the answer ±1, ±2, ±10 and a digit swap, kept in range and distinct. When fewer than three qualify, top up with the nearest unused numbers.
4. Place the four tiles with a Fisher–Yates shuffle on the session generator.
5. Avoid repeats within a level. When a level has 50 or fewer possible questions, list them all, shuffle and deal ten. Larger levels redraw on a repeat.

### Tuning

Every number lives in a `Tuning` struct in `libs/core`. `Tuning::defaults()` returns the release values. Developer builds can read an optional `tuning.json` in the app layer to override them, so playtests change numbers without a rebuild. A test pins `Tuning::defaults()` to `libs/core/tests/data/prd-tables.json`, so a change to either fails CI until the other follows. `libs/core/data/tuning.json` is the starting data.

## Persistence

One small JSON document per device. `libs/core` owns the schema. Platforms store and return the text, so every platform reads and writes the same file.

| Field | Holds | Written when |
|---|---|---|
| `version` | Schema number, starting at 1 | Every save |
| `coins` | The wallet | Level clear, game over, pause |
| `won_once` | Whether the 1,000-coin win bonus was paid | Level 12 cleared |
| `sound`, `music` | The two Settings switches | When changed |
| `game_center` | The Game Center switch (Apple only, default off) | When changed |
| `top10` | Up to 10 entries of name and score | After name entry |
| `run` | Saved run snapshot | On pause. Removed when resumed or ended |

The plan's field table does not list `game_center`. The PRD and decision 6 require the switch to persist, so add it and note it in the schema.

Best score is the top entry of `top10`, so it is not stored twice.

```cpp
class SaveStore
{
public:
    virtual ~SaveStore() = default;
    virtual std::optional<std::string> load() = 0;
    virtual bool store(std::string_view text) = 0;
};
```

Implementations: `FileSaveStore` (iOS, Android, desktop), `WebSaveStore` (browser), `MemorySaveStore` (tests).

Rules:

- Atomic writes: write a temporary file and rename it over the old one.
- A file that fails to parse is renamed to `save.json.bad` and the game starts on defaults. It never crashes on a save.
- A file with a higher `version` than the build knows is left alone. The game runs on defaults for that session and does not overwrite it.
- Each schema step is one function from version n to n + 1 in `libs/core`, with a fixture file in `libs/core/tests/` for every old version.
- `libs/core` carries a small JSON reader and writer limited to objects, arrays, strings, integers and booleans. Round-trip and random-input tests cover it.
- Names and scores never leave the device. No cloud sync.

### Browser storage

On WebAssembly, files written with `QFile` go to an in-memory file system by default, and standard data folders may not exist. `QSettings` has a local-storage backend with a 5 MB limit. The save is a few kilobytes, so `WebSaveStore` keeps the whole text under one `QSettings` key. The M2 web spike confirms this on the pinned Qt version, including what happens when the browser blocks storage. Fallback: a small JavaScript storage bridge.

## App layer

QML screens driven by one view-model each. The QML is the same on every platform and the layout adapts to the window. One `GameController` owns the `GameSession`, the tick driver, the save data and the platform services. Screens bind to read-only properties of small view-models and send commands as method calls. The tick driver lives in the controller because the app layer is the only layer allowed a clock.

| Screen | Board | Shows |
|---|---|---|
| Title | CP1, CP18 | Best score, coins, CONTINUE when a run is saved |
| Play | CP2 to CP5, CP13 | Question, four tiles, timer, multiplier, streak, hint button |
| Level clear | CP6 | Points breakdown, coins earned, wallet |
| Win | CP8 | The 1,000-coin bonus, KEEP GOING (endless) or FINISH |
| Name entry | CP11 | Rank, score, name field, SAVE and SKIP |
| Top 10 | CP12 | Rank, name and score per entry |
| Game over | CP7 | Score, level reached, coins, best score |
| Settings | CP14 | Sound and music switches, and a Game Center switch on Apple devices |
| About | CP15 | Build information, font licences, notices |
| Pause | CP16 | RESUME and QUIT |
| Parental gate | CP17 | A number in words for a grown-up to type back, before a link opens or Game Center turns on |

Navigation: PLAY AGAIN on Game over starts level 1 directly, HOME returns to Title. Top 10, Settings and About open from Title, and Top 10 also opens from Game over. Pause opens from Play.

- Two size classes: compact (phone portrait) and regular (tablets, desktop windows), matching CP9 and CP10. Phones are locked to portrait.
- Bangers and Lilita One ship as application resources and load with `FontLoader`, never from the network.
- Every button has an accessible name and role. Text keeps 4.5:1 contrast. No cue relies on colour alone. All sounds have a visual twin. Full screen-reader play of a timed game is out of scope for v1.

## Platform services

The platform layer holds everything that touches the operating system. Each service is an interface with a real implementation per platform and a null or in-memory one for tests. `GameController` receives the services at start-up. Screens never reach them directly.

| Service | Implementations | Notes |
|---|---|---|
| `SaveStore` | `FileSaveStore`, `WebSaveStore`, `MemorySaveStore` | See Persistence |
| `ScoreService` | `GameCenterScoreService` on iOS and iPadOS, `NullScoreService` elsewhere | Submits a finished score. The local top 10 does not depend on it |
| `AudioService` | Qt Multimedia implementation, `NullAudioService` for tests | Effects and music, with the two Settings switches |
| `LinkOpener` | System browser | Reached only through the parental gate |
| `BuildInfo` | Generated at configure time | Version, build number, commit and Qt version |
| Notices | Generated at build time | Third-party licence texts for the About screen |

```cpp
class ScoreService
{
public:
    virtual ~ScoreService() = default;
    virtual bool available() const = 0;
    virtual void submit(std::int64_t score) = 0;
};

class AudioService
{
public:
    virtual ~AudioService() = default;
    virtual void play_effect(SoundId id) = 0;
    virtual void play_music(MusicId id) = 0;
    virtual void set_enabled(bool effects, bool music) = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
};

class LinkOpener
{
public:
    virtual ~LinkOpener() = default;
    virtual void open(const std::string& url) = 0;
};
```

### Score service

The local top 10 is part of the save data and the core ranks it, so it works with the service absent. `GameController` calls `ScoreService::submit` once when a run ends, with the final score. The call returns nothing. A failure is logged and never shown to the child.

`GameCenterScoreService` is Objective-C++, compiled only for the iOS target. It sends the score and nothing else, and never reads the player's alias, friends or avatar. It stays idle until the Settings switch is on. The switch is off by default and sits behind the parental gate. The sign-in handler is installed only after the switch is turned on, so a default install never shows a Game Center sign-in sheet. If the player is not signed in, or Screen Time restricts Game Center, `submit` does nothing. Android uses `NullScoreService` in v1.

### Lifecycle and screen time

The app observes `QGuiApplication::applicationStateChanged`. When the state leaves Active, `GameController` calls `pause()`, stops the tick driver, suspends audio and writes the save. When the app is active again, a paused run opens the pause screen with RESUME and QUIT, and the tick driver stays stopped until RESUME.

The game has no code for screen-time settings. A Screen Time limit, Downtime, a Family Link lock or a Digital Wellbeing timer takes the app out of the foreground, and that is the only signal needed. Device tests confirm each produces a state change in Qt.

Desktop windows pause when they lose focus. In the browser the same path runs from page visibility. The spike confirms Qt reports a hidden tab, and a small JavaScript hook calls `pause()` if it does not.

### Audio

One table in the app layer maps core events to sounds, using the feedback table in `SPEC.md`. The core knows nothing about audio. Browsers block audio until the first tap, so on the web the first tap on Title starts the sound. Whether Qt Multimedia is enough on WebAssembly, and the audio format, are settled by the M2 spike.

The built service, its placeholder sounds and how to replace them are in [Audio](audio.md).

### Parental gate

One screen in the app layer. `GameController::requestLink(url)` (QML-facing names are camelCase; only an https address is accepted) opens the gate, `submitGate(typed)` checks the number, and it calls `LinkOpener::open` only when it is passed. The gate also guards the Game Center switch. The only links v1 needs are the privacy policy and the source repository, both on About. The code base has exactly one call that opens a URL, and a CI check fails if another appears. The form is a number written in words that the adult types back. It is checked against the current Apple and Google rules for children's apps before release.

### Build information and notices

`BuildInfo` is generated by CMake from the project version and the CI environment: version, build number from the CI run, short commit hash and Qt version. The Settings footer shows "Heart Boom Maths 1.0.0 (build 42)" and About shows the rest. Local builds show build 0 and "dev".

A CMake step gathers the licence file of each vcpkg dependency, the Qt notice and the OFL text for the two fonts into one notices file, which ships as a resource. CI fails if a dependency has no licence file.

v1 ships no analytics, crash reporting or advertising code, and the app makes no network requests of its own. The release checklist includes a traffic capture on a device.

### As built (appcore)

Deviations from the sketches above, all small:

- `ScoreService` gains `set_enabled(bool)`, called when the Game Center switch changes, so the controller always calls `submit` once and the service stays idle while the switch is off. `available()` is false on a platform without a leaderboard, and the switch is then ignored.
- `AudioService` gains `stop_effect(SoundId)` so the ticking can end when the timer leaves the alert zone, or the level stops running. `SoundId` and `MusicId` are in `services/audio_service.hpp`; `MusicId` is `NONE`, `MENU` and `PLAY` (a placeholder until music is chosen).
- `AudioService` also gains `unlock()` (empty by default), called on the first tap so a browser lets sound start. The Qt Multimedia service is `BackedAudioService` over a thin `AudioBackend`; see [Audio](audio.md).
- `SaveStore` only loads and stores, so moving a damaged file aside is a second interface, `SaveQuarantine`, that `FileSaveStore` also implements. The controller moves a damaged file to `save.json.bad` before its first write. A newer file stays untouched because `SaveManager` refuses every save after it.
- The controller is built from `GameServices` (pointers) and `GameOptions` (tuning, session seed, gate seed source, build information). The clock is the `Clock` interface (`SteadyClock`, and `ManualClock` for tests), and `TickDriver` is plain C++ with integer arithmetic only.
- The gate asks for a three-digit number in English words (`parental_gate.hpp`). A wrong answer deals a new number. Only the two link purposes and the Game Center switch use it.

## Tests

Nearly all behaviour lives in `libs/core`, so nearly all tests run there, without Qt, a window or a clock.

| Test | Covers | Runs |
|---|---|---|
| Core unit | Scoring, timer, levels, generator, PRNG, save serialisation | Tier 1 |
| Tuning | `Tuning::defaults()` equals the PRD tables | Tier 1 |
| Golden replay | Whole runs from a seed and a command list | Tier 1 on Linux. Tier 2 compares results on macOS, Windows and WebAssembly under Node |
| Property | Generator and session invariants over many seeds | Tier 1 with a fixed seed count. Tier 3 with many more |
| Sanitizers | ASan and UBSan on all tests, TSan on the app-layer tests | Tier 2 |
| Persistence | Round trip, migrations, damaged and newer files, each real store on its own OS | Tier 1 for the logic. Tier 2 for the stores |
| App layer | Each screen and view-model loads with no QML warnings, at both size classes | Tier 2, offscreen |
| Browser | Scripted run in the WebAssembly build, reload, save restored | Tier 3 |
| Device | The release checklist in `COMPLIANCE.md` and playtests | By hand before each release |

The test framework is chosen in M0 and pinned in vcpkg. A coverage report for `libs/core` is published with each pull request. The threshold is set after the core exists.

### Golden replays

A replay is a seed, a tuning name and a list of commands with the tick each arrives on. The test plays it through a `GameSession` and compares the events and the final state hash with the stored expectation. The hash is a 64-bit FNV-1a over the serialised session, defined in `libs/core`.

```json
{
  "format": 1,
  "seed": 20261114,
  "tuning": "defaults",
  "commands": [
    { "tick": 0, "do": "start_game" },
    { "tick": 38, "do": "answer", "tile": 2 },
    { "tick": 91, "do": "pause" },
    { "tick": 91, "do": "resume" }
  ],
  "expect": { "events_hash": "<hash>", "final_hash": "<hash>", "score": 0 }
}
```

A command's `tick` is the number of `tick()` calls made before it, and the runner makes them between commands. Optional fields: `wallet` and `won_once` (the starting save values), `end_tick` (keep ticking to here after the last command, so a run can end in the bomb), and `note` and `recipe` (for readers and for the harness, which rewrites a file from its recipe). Hashes are 16 lowercase hex digits. The code is `replay.hpp` in `libs/core`.

The set covers a perfect level, a level with wrong answers and hints, a time-out in the alert zone, the 12-level win, and endless mode to level 20. A deliberate rule or tuning change regenerates the affected replays in the same commit, and the diff is reviewed. A replay that changes without a deliberate rule change is a bug.

### Property tests

Over many seeds and every level, including endless:

- Each question has exactly one correct tile, and the four tile values are distinct.
- The shown expression evaluates to the correct tile, and operands stay inside the level's range.
- No question repeats within a level, and levels 1 and 2 deal each of their 10 and 15 possibilities once before any repeat.
- Time left is never negative, never exceeds the cap, and the bomb goes off when it reaches zero.
- Points and level counters never decrease, the wallet never goes negative, and a hint is refused when the wallet holds fewer than 2 coins.
- A random sequence of commands, including pause and resume at any tick and commands after game over, never crashes and never breaks the rules above.

There are no pixel comparisons in v1. Each milestone with screens is checked by hand against the boards in `private/design/boards/` (kept out of the public repository). Sound and feel are judged in playtests.

## Build, CI and release

Every target builds from one CMake project through presets, named as in the template and `snake` (`linux-debug`, `linux-release`, `linux-asan-ubsan`, `linux-tsan`, `linux-coverage`, `macos-release`, `windows-msvc-release`, `windows-msvc-asan`, `android-arm64-release`, `ios-release`, and a new `wasm-release`). A single versions file pins the Qt version and the Emscripten version it needs, and aqtinstall and vcpkg read from it, so a local build and a CI build use the same toolchain. GitHub Actions are pinned by commit SHA.

### CI tiers

| Tier | Runs on | Jobs |
|---|---|---|
| 1 | Every push | One Linux job: format check, core build with no Qt on the include path, core, tuning, replay, persistence and property tests with a fixed seed count |
| 2 | Every pull request | Desktop matrix on Linux (GCC and Clang), macOS and Windows (MSVC) with the full app, replay hash comparison across platforms, file store tests and offscreen app tests. A Linux ASan and UBSan job and a TSan job. A WebAssembly build with the replays run under Node. Unsigned Android and iOS builds to catch breakage |
| 3 | Nightly on `develop`, and on a release tag | Long property runs, the browser test, and signed release builds of every platform |

`develop` is the fast trunk. `main` receives a merge at each release, and a version tag on `main` starts the release build. Pull requests from forks get no secrets. Signing keys are available only to the tag build.

### Release per platform

| Platform | Build host | Output | Distribution |
|---|---|---|---|
| iPhone and iPad | macOS runner with Xcode, Qt for iOS linked statically | Signed `.ipa` | TestFlight, then App Store |
| Android | Linux runner with the NDK, `arm64-v8a` and `x86_64` | Signed `.aab` | Play internal testing, then Google Play |
| Browser | Linux runner with Emscripten, single-threaded Qt build | Static `.html`, `.js` and `.wasm` files | Static host, chosen when the web build is ready. GitHub Pages is the default candidate |
| macOS | macOS runner | Signed and notarised `.dmg` | GitHub Release |
| Windows | Windows runner with MSVC and `windeployqt` | Zip archive | GitHub Release |
| Linux | Linux runner | AppImage | GitHub Release |

The browser build is single-threaded because a multi-threaded WebAssembly build needs cross-origin isolation headers from the host, and the core runs on one thread anyway. A Windows installer and code signing, Flatpak, and desktop store listings are for later.

The version is the CMake project version, for example 1.0.0. The build number is the CI run number, and it feeds the iOS build number and the Android version code, which only ever go up.

Store listings are done by hand for v1: the Apple age rating questionnaire, App Privacy answers, the Google Play target-age declaration and data safety form, the privacy policy address, and screenshots from the app.
