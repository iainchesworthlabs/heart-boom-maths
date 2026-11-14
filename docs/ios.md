# iOS and Game Center

The game builds for iPhone and iPad from the same C++ core and QML as every other platform. This page says how the iOS
build is made, how the Game Center score works, what has been checked and where, and what only the owner can do. It is
the written result of the iOS part of the M2 spike and the iOS half of M5 ([Milestones](MILESTONES.md)).

## How it builds

| Preset | What it builds | Where |
|---|---|---|
| `ios-release` | `apps/ios`, a one-screen stub that shows the name and version. It exists to prove signing and TestFlight | `ios-build.yml`, and the `ios` job of `release.yml` |
| `ios-game-release` | The whole game (`apps/game`), linked statically against Qt for iOS. Unsigned | `ios-game-build.yml` |

`ios-game-release` is a cross-build, so it needs two Qt kits of the pinned version (the `qt` key of
`.github/toolchain-versions.json`):

- the **iOS kit**, with the `qtmultimedia` module, named by `HEART_BOOM_MATHS_QT_IOS_ROOT`. Its `qt.toolchain.cmake` is the
  preset's toolchain file;
- the **macOS desktop kit**, named by `ICL_QT_ROOT`, which becomes `QT_HOST_PATH`: the tools that run on the Mac while
  building (`moc`, `rcc`, `qmlcachegen`, `qmlimportscanner`).

```bash
export HEART_BOOM_MATHS_QT_IOS_ROOT=~/Qt/6.12.0/ios
export ICL_QT_ROOT=~/Qt/6.12.0/macos
cmake --preset ios-game-release
cmake --build --preset ios-game-release
apps/game/ios/verify_game_app.sh build/ios-game-release/Release-iphoneos/heart-boom-maths.app \
    --bundle-id com.iainchesworthlabs.heartboommaths --executable heart-boom-maths \
    --entitlements apps/game/ios/heart-boom-maths.entitlements
```

No vcpkg is involved: the manifest has no dependency for iOS (Catch2 is for the tests, which are not built there). The
preset turns code signing off, so no Apple account is needed.

`ios-game-build.yml` does the same on a `macos-26` runner: the two `setup-qt` steps, configure, build, and then
`apps/game/ios/verify_game_app.sh` over the `.app`. It reports the size of the bundle and the executable in the job
summary, and keeps the configure and build logs for a week when it fails. It is not a required check ([CI](ci.md)).
Run it by hand with `gh workflow run ios-game-build.yml --ref <branch>`.

The release workflow still builds and signs the stub (`apps/ios`), on purpose: signing and uploading the game is the
owner's step, after the licence position below is settled.

### The bundle

The files are in `apps/game/ios/`, applied by `ios.cmake` to the game target.

| File | What it holds |
|---|---|
| `Info.plist.in` | The identifier `com.iainchesworthlabs.heartboommaths`, the icon and launch colour, portrait only on a phone and any way up on a tablet (full screen, no split view), `arm64` and `gamekit` as required capabilities, no status bar, `ITSAppUsesNonExemptEncryption` false, and **no usage text for any permission**: the game asks for none |
| `heart-boom-maths.entitlements` | One entitlement, `com.apple.developer.game-center`. An unsigned build does not embed it |
| `Assets.xcassets` | The launch colour and the app icon |
| `verify_game_app.sh` | The check of the built bundle (see above) |

The **app icon is a placeholder**. It is a flat heart with a fuse, drawn by `tools/assets/make_placeholder_icon.py` from a
formula with no outside art, and the test `game.icon_placeholder` keeps the file equal to the script. The real icon has not
been designed; a designer's 1024 x 1024 picture (no transparency) replaces `AppIcon-1024.png`, and the script and its test
are then removed.

## What the spike found

- **Qt for iOS is static only.** There are no dynamic Qt libraries on iOS. The game, Qt, the QML module, the Qt Multimedia
  backend and the plugins end up in one executable. `qt_add_executable` makes the target an iOS bundle, and the static
  QML and platform plugins are imported by Qt's own finalisation (`qmlimportscanner`, from the host kit).
- **The game is GPL-3.0-or-later**, and Qt is available under the GPL, the LGPL and a commercial licence. The About screen
  already carries the Qt notice and the source address.
- **The App Store question is open and is not decided here.** `DECISIONS.md` records it: with Qt linked statically, the
  LGPL is a known problem on the App Store, and the GPL is reported to conflict with Apple's terms. The options recorded
  there are a commercial Qt licence for the iOS build, or iOS after v1. What an App Store build needs from the owner is
  written advice on the licence position (to be taken before any upload), and, if the answer is a commercial Qt licence,
  the licence itself and the way it is applied to the iOS build alone. The CI compile of this page proves the game builds;
  it says nothing about whether it may be distributed through the store. Nothing in the repository uploads the game.

## Game Center

Game Center sends one number, the score of a finished game, to a leaderboard. It is on Apple devices only, behind the
Settings switch (off by default) and the parental gate. With the switch off the game never contacts Game Center and never
shows a sign-in sheet ([Compliance](COMPLIANCE.md)).

The code is in three layers, so the rules are tested on every desktop and the part that only an Apple device can run is
small:

| Layer | File | Holds |
|---|---|---|
| The interface | `src/services/score_service.hpp` | `ScoreService`: `available()`, `set_enabled()`, `submit()`. The controller calls `submit` once per finished game |
| The rules | `src/services/game_center_score_service.{hpp,cpp}` | `GameCenterScoreService`, plain C++ over a `GameCenterBackend` |
| The GameKit shim | `src/platform/ios/score_platform.mm` | `GameKitBackend`: three GameKit calls, no decisions |

`src/platform/score_platform.hpp` declares `make_score_service()`, and every directory under `src/platform/<os>/` defines
it: the iOS one returns `GameCenterScoreService` over `GameKitBackend`, every other one returns `NullScoreService`. CMake
compiles the directory of the target, so no `#if` is involved.

What `GameCenterScoreService` does:

- `available()` is true, so the Settings row shows (the row is also guarded by the gate in the controller). On every other
  platform `NullScoreService::available()` is false and the row is not there.
- It is idle until `set_enabled(true)`. Nothing is installed or contacted at start-up with the switch off.
- `set_enabled(true)` installs the sign-in handler (`GKLocalPlayer.authenticateHandler`) **only then**, once.
  `set_enabled(false)` removes it, and a status the platform reports late is ignored (each position of the switch has its
  own generation).
- `submit(score)` sends the score only when the switch is on, a player is signed in and the account is not restricted.
  Otherwise the score is dropped, not queued: it must not leave the device later than its game. It does not filter
  repeats, because two games can end on the same score, and the controller already submits exactly once per game.
- The backend sends with `+[GKLeaderboard submitScore:context:player:leaderboardIDs:completionHandler:]`, shows nothing
  when it finishes, and only logs the error code of a failure.
- "Restricted" is an authentication error of `GKErrorParentalControlsBlocked`. The game reads no other property of the
  player: no alias, no friends, no avatar, no player identifier. If GameKit hands the handler a sign-in sheet, it is
  presented, which is possible only while the switch is on.

### The leaderboard identifier

`GAME_CENTER_LEADERBOARD_ID` in `game_center_score_service.hpp` is the one place the identifier is written. It is a
**placeholder** (`...highscore.placeholder`): no such leaderboard exists, so until the owner replaces it a submission
fails at Apple's end and is logged and swallowed.

## What is checked, and where

| What | Where | How |
|---|---|---|
| The Game Center rules: idle until the switch, handler installed once and only then, no send when signed out or restricted, a late answer ignored, the identifier passed through | Every desktop, in CI | `game.*` tests in `tests/test_game_center.cpp`, over a fake backend |
| The Settings path: the gate must be passed before the service is enabled, and a finished game then sends one score | Every desktop, in CI | The same tests, with the real `GameController` |
| `NullScoreService` elsewhere, the Settings row hidden | Every desktop, in CI | Existing controller and UI tests |
| The icon equals its script | Every desktop, in CI | `game.icon_placeholder` |
| The game compiles and links for a device against static Qt, and the bundle has the right keys, entitlement file, GameKit and none of the forbidden frameworks | A `macos-26` runner | `ios-game-build.yml` |
| The Objective-C++ shim | A `macos-26` runner (compile only) | `ios-game-build.yml`. Not run by any test |
| The game starts on a device, plays, and pauses and saves when it leaves the foreground | **Not done**: needs a device | Owner |
| Game Center sign-in, a real submission, a child account, Screen Time | **Not done**: needs a device and the leaderboard | Owner (below) |

## What remains for the owner

1. **Decide the licence position** for the App Store ([Decisions](DECISIONS.md)), on written advice.
2. An **Apple Developer account**, and the **App ID** `com.iainchesworthlabs.heartboommaths` with the **Game Center**
   capability turned on. The provisioning profile is made after that, so that it carries the entitlement. The signing
   secrets and the variable `SIGN_IOS_UPLOAD` are in [Releasing](releasing.md).
3. The **App Store Connect record**, and the **leaderboard** (Game Center, Leaderboards: a single score, highest first,
   the best score). Put its identifier into `GAME_CENTER_LEADERBOARD_ID`.
4. Point the `ios` job of `release.yml` at the game (`ios-game-release`, `apps/game/ios/verify_game_app.sh`) instead of
   the stub, and send a build to **TestFlight**.
5. **Device tests** from [Compliance](COMPLIANCE.md), with the results recorded there:
   - the app limit and Downtime on an iPhone with a child Apple Account: the game pauses, saves and resumes;
   - Game Center multiplayer and adding friends set to Don't Allow: a score is still sent, or not, as intended (the game
     treats only `GKErrorParentalControlsBlocked` as a restriction, which this test confirms or changes);
   - a fresh install with the switch off shows **no sign-in sheet** and makes **no network request** (traffic capture),
     and with the switch on only Game Center's own traffic appears;
   - how a child's account appears on a global leaderboard, before score submission is turned on;
   - what the sign-in sheet looks like when the switch is turned on while signed out.
6. The real **app icon**, and **age rating**, privacy and Game Center answers in App Store Connect.

## Things the first macOS run may show

The Mac side has not been run by the author of this page, so these are where a first red run would most likely be:
Qt's own handling of the launch screen when the target has its own `Info.plist.in` (the property list gives
`UILaunchScreen` itself); the static plugin import of `qmlimportscanner` and the Qt Multimedia plugin for iOS; and the
`modules: qtmultimedia` install for the iOS kit by `aqtinstall`. The workflow prints the kits it found and the errors of the
build first, to make each of them quick to read.
