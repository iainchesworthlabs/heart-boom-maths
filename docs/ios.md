# iOS and Game Center

The game builds for iPhone and iPad from the same C++ core and QML as every other platform. This page says how the iOS
build is made, how the Game Center score works, what has been checked and where, and what only the owner can do. It is
the written result of the iOS part of the M2 spike and the iOS half of M5 ([Milestones](MILESTONES.md)).

## How it builds

| Preset | What it builds | Where |
|---|---|---|
| `ios-release` | `apps/ios`, a one-screen stub that shows the name and version. It is the template of the signing and TestFlight path, and is no longer released | `ios-build.yml` |
| `ios-game-release` | The whole game (`apps/game`), linked statically against Qt for iOS. Unsigned, unless given the signing variables | `ios-game-build.yml` (unsigned), and the `ios` job of `release.yml` (signed) |

`ios-game-release` is a cross-build, so it needs two Qt kits of the pinned version (the `qt` key of
`.github/toolchain-versions.json`):

- the **iOS kit**, without the `qtmultimedia` module (sound comes from AVFoundation, [Audio](audio.md)), named by `HEART_BOOM_MATHS_QT_IOS_ROOT`. Its `qt.toolchain.cmake` is the
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

### Signed, for TestFlight

The `ios` job of `release.yml` builds, signs and sends the **game** (the stub of `apps/ios` stays in the repository as a
template and is no longer released). It runs only when the repository variable `SIGN_IOS_UPLOAD` is `true`. In order:

1. `import-ios-signing` puts the Apple Distribution certificate in a keychain made for the job and the App Store
   provisioning profile where Xcode looks for it (it checks the identifier and that the profile lists no device).
2. **Read the provisioning profile.** The job decodes the profile (`security cms -D`) and prints its name, application
   identifier, team, expiry, the number of devices and the names of its entitlements, never a secret. It stops with a
   sentence if the application identifier is not `TEAMID.com.iainchesworthlabs.heartboommaths`, or if the profile expires
   within 14 days.
3. The two Qt kits are installed exactly as in `ios-game-build.yml` (no Qt Multimedia, no vcpkg).
4. `cmake --preset ios-game-release` is given the signing variables on the command line (the team, the profile name, the
   build number, which is the run number, and the Game Center switch). `apps/game/ios/ios.cmake` then signs the
   application target alone, manually, with `Apple Distribution`; the preset's unsigned defaults stay for every other
   target.
5. `export-ios-ipa` archives the scheme `heart_boom_maths_game` and exports an App Store IPA.
6. `apps/game/ios/verify_game_app.sh --signed --store` opens the IPA as a device would, then Apple validates it
   (`altool --validate-app`), and a tag, or a manual run with `upload_testflight`, uploads it. Any other run stops after
   the validation. The IPA is the workflow artifact `ios-appstore` for 7 days and is never put on the release.

#### The Game Center rule

Xcode refuses to sign a bundle that asks for an entitlement its provisioning profile does not carry. So the Game Center
entitlement (`com.apple.developer.game-center`, in `heart-boom-maths.entitlements`) follows the profile: step 2 passes
`-DHEART_BOOM_MATHS_IOS_GAME_CENTER=ON` to CMake only when the profile's entitlements contain it, and `OFF` otherwise, and
prints which in the log and the job summary. `OFF` leaves out the entitlement and nothing else: the GameKit code is still
there, `Info.plist` still lists `gamekit`, and signing in to Game Center fails, which `GameCenterScoreService` treats as a
player who is not signed in (no score is sent). `verify_game_app.sh --game-center on|off` checks the state the job chose
against the signature (on: the signature carries the entitlement; off: it does not) and reports which. When the owner
turns the capability on for the App ID and makes the profile again, the next run builds with Game Center on, with no
change to the workflow. Locally the option is `-DHEART_BOOM_MATHS_IOS_GAME_CENTER=OFF` (default ON).

#### Running it

The release workflow runs from any branch by hand. It needs the secrets and the variable of [Releasing](releasing.md):

```bash
# Build, sign, check, and have Apple validate the IPA. Nothing is uploaded.
gh workflow run release.yml --ref <branch>

# The same, and upload the build to TestFlight.
gh workflow run release.yml --ref <branch> -f upload_testflight=true
```

A tag does the second without the flag. Apple processes an uploaded build for a few minutes before it shows in the
TestFlight app. The workflow also runs the other release jobs (the desktop packages, and Android when its variable is
set); open the `Package (iOS)` job.

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

- **Qt for iOS is static only.** There are no dynamic Qt libraries on iOS. The game, Qt, the QML module and the plugins end
  up in one executable. `qt_add_executable` makes the target an iOS bundle, and the static
  QML and platform plugins are imported by Qt's own finalisation (`qmlimportscanner`, from the host kit).
- **The game is GPL-3.0-or-later**, and Qt is available under the GPL, the LGPL and a commercial licence. The About screen
  already carries the Qt notice and the source address.
- **The App Store question is open and is not decided here.** `DECISIONS.md` records it: with Qt linked statically, the
  LGPL is a known problem on the App Store, and the GPL is reported to conflict with Apple's terms. The options recorded
  there are a commercial Qt licence for the iOS build, or iOS after v1. What an App Store build needs from the owner is
  written advice on the licence position (to be taken before any upload), and, if the answer is a commercial Qt licence,
  the licence itself and the way it is applied to the iOS build alone. The CI compile of this page proves the game builds;
  it says nothing about whether it may be distributed through the store. The release workflow can upload a build to
  **TestFlight** (internal testers, not the store) before the question is settled; releasing to the App Store waits for it.

## Safe area and the privacy manifest

- **Safe area.** Qt 6.9 and later draw a window edge to edge on iOS, so the cut-out of an iPhone, its rounded corners and the
  home indicator are the application's to keep clear. `apps/game/qml/SafeAreaFrame.qml` (used by `Main.qml`) reads
  `SafeArea.margins` and lays the application shell out inside them; the yellow halftone page runs under the unsafe edges,
  with its dots in step with the page inside. Desktop windows report zero margins. The tests stand in for a phone's
  margins with `SafeArea.additionalMargins`.
- **Privacy manifest.** `apps/game/ios/PrivacyInfo.xcprivacy` goes into the root of the bundle. It says the game does not
  track, has no tracking domains and collects no data type, and declares the required-reason APIs that the game and the
  statically linked Qt call: UserDefaults (CA92.1), FileTimestamp (C617.1), SystemBootTime (35F9.1) and DiskSpace (E174.1).
  `verify_game_app.sh` refuses a bundle without it, and one that says it tracks or collects. If Apple's upload check names
  another required-reason API after a Qt update or a new feature, add its category and reason to the file.

## Sound

The iPhone and iPad build has sound, from AVFoundation and not from Qt Multimedia, whose FFmpeg backend cannot link into
the static kit. `src/platform/ios/avf_audio_backend.mm` is a thin shim over `AVAudioSession` and `AVAudioPlayer`, and the
rules (what loads, when the session is activated, what an interruption stops and what comes back) are plain C++ in
`ClipAudioBackend`, tested on every desktop. The category is Ambient: the game follows the silent switch, keeps the player's
own music playing and records nothing, so Info.plist still holds no usage text. [Audio](audio.md) has the detail.

The sound files are Qt resources inside the executable. `verify_game_app.sh` checks that the executable links
AVFoundation and that the name of every file of `apps/game/audio` is in its resource table.

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
fails at Apple's end and is logged and swallowed. A TestFlight build made with Game Center off (see above) never gets as
far as a submission.

## What is checked, and where

| What | Where | How |
|---|---|---|
| The Game Center rules: idle until the switch, handler installed once and only then, no send when signed out or restricted, a late answer ignored, the identifier passed through | Every desktop, in CI | `game.*` tests in `tests/test_game_center.cpp`, over a fake backend |
| The Settings path: the gate must be passed before the service is enabled, and a finished game then sends one score | Every desktop, in CI | The same tests, with the real `GameController` |
| `NullScoreService` elsewhere, the Settings row hidden | Every desktop, in CI | Existing controller and UI tests |
| The icon equals its script | Every desktop, in CI | `game.icon_placeholder` |
| The game compiles and links for a device against static Qt, and the bundle has the right keys, entitlement file, GameKit and none of the forbidden frameworks | A `macos-26` runner | `ios-game-build.yml` |
| The sound rules: the clips loaded at start-up, volumes, looping, the session activated by the first sound, interruption and reset, a clip that fails to load | Every desktop, in CI | `game.*` tests in `tests/test_clip_audio.cpp`, over a fake host and fake players |
| The sound files are in the executable, and AVFoundation is linked | A `macos-26` runner | `verify_game_app.sh` |
| The Objective-C++ shims (GameKit, AVFoundation) | A `macos-26` runner (compile only) | `ios-game-build.yml`. Not run by any test |
| The signed IPA: the signature, an App Store profile for the identifier, the Game Center entitlement in the state the profile allowed, Apple's own validation | A `macos-26` runner, with the signing secrets | The `ios` job of `release.yml`. Only the runner can show it: nothing of Xcode runs on a Windows or Linux machine |
| The game makes sound on a device, goes quiet with the silent switch, and recovers after a phone call, Siri and a Screen Time lock | **Not done**: needs a device | Owner |
| The game starts on a device, plays, and pauses and saves when it leaves the foreground | **Not done**: needs a device | Owner |
| Game Center sign-in, a real submission, a child account, Screen Time | **Not done**: needs a device and the leaderboard | Owner (below) |

## What remains for the owner

1. **Decide the licence position** for the App Store ([Decisions](DECISIONS.md)), on written advice.
2. The **Game Center capability** turned on for the App ID `com.iainchesworthlabs.heartboommaths` (Apple Developer
   account, Identifiers), then the **App Store provisioning profile made again** from that App ID, and its base64 put
   into the secret `IOS_PROVISIONING_PROFILE_BASE64`. Until then TestFlight builds are signed without the Game Center
   entitlement (above) and Game Center sign-in does not work in them. The signing secrets and the variable
   `SIGN_IOS_UPLOAD` are in [Releasing](releasing.md).
3. The **leaderboard** (App Store Connect, the app, Game Center, Leaderboards: a single score, highest first, the best
   score). Put its identifier into `GAME_CENTER_LEADERBOARD_ID`.
4. The real **app icon** (a 1024 x 1024 picture, no transparency, replacing `AppIcon-1024.png`) and a **privacy policy
   URL** for App Store Connect (the game collects nothing, and the page says so: [Compliance](COMPLIANCE.md)).
5. **Device tests** from [Compliance](COMPLIANCE.md), with the results recorded there:
   - the app limit and Downtime on an iPhone with a child Apple Account: the game pauses, saves and resumes;
   - Game Center multiplayer and adding friends set to Don't Allow: a score is still sent, or not, as intended (the game
     treats only `GKErrorParentalControlsBlocked` as a restriction, which this test confirms or changes);
   - a fresh install with the switch off shows **no sign-in sheet** and makes **no network request** (traffic capture),
     and with the switch on only Game Center's own traffic appears;
   - how a child's account appears on a global leaderboard, before score submission is turned on;
   - what the sign-in sheet looks like when the switch is turned on while signed out.
6. The **age rating**, privacy and Game Center answers in App Store Connect.

## Things the first macOS run may show

The Mac side has not been run by the author of this page, so these are where a first red run would most likely be (for the
signed release job: the archive of a CMake project that holds many static-library targets, the scheme name, Apple's
validation of the property list and the privacy declarations, and a profile that is not as the job expects; each prints
what it found before it stops):
Qt's own handling of the launch screen when the target has its own `Info.plist.in` (the property list gives
`UILaunchScreen` itself); the static plugin import of `qmlimportscanner`; the Objective-C++ of the two shims, which no compiler has seen against the
real SDK yet (`avf_audio_backend.mm` was only checked against hand-written stubs of the AVFoundation types); and the
name search of `verify_game_app.sh` for the sound files in the executable. The workflow prints the kits it found and the errors of the
build first, to make each of them quick to read.
