# iOS, Game Center and iCloud sync

The game builds for iPhone and iPad from the same C++ core and QML as every other platform. This page says how the iOS
build is made, how the Game Center score and iCloud sync work, what has been checked and where, and what only the owner
can do. It is the written result of the iOS part of the M2 spike and the iOS half of M5 ([Milestones](MILESTONES.md)),
and of the iCloud sync work after v1.

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
    --entitlements build/ios-game-release/heart-boom-maths.entitlements
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
   build number, which is the run number, and the Game Center and iCloud switches). `apps/game/ios/ios.cmake` then signs the
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

iCloud sync follows the same rule with `HEART_BOOM_MATHS_IOS_ICLOUD` (see [iCloud sync](#icloud-sync)), and the job
reads the profile for both in the same step. The entitlements file is no longer a fixed file: CMake writes it for the two
options, and `verify_game_app.sh` is given the written file.

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
| `heart-boom-maths.entitlements.in` | The template of the entitlements file, which CMake writes to the build directory with `com.apple.developer.game-center` and `com.apple.developer.ubiquity-kvstore-identifier` each only when its option is ON. An unsigned build does not embed it |
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

- **Safe area.** Qt 6.9 and later keep an iOS window inside the safe area unless it asks to be expanded: `Main.qml` sets
  `Qt.ExpandedClientAreaHint` on iOS, and without it the cut-out and home-indicator bars stay black. With the hint the
  cut-out of an iPhone, its rounded corners and the
  home indicator are the application's to keep clear. `apps/game/qml/SafeAreaFrame.qml` (used by `Main.qml`) reads
  `SafeArea.margins` and lays the application shell out inside them; the yellow halftone page runs under the unsafe edges,
  with its dots in step with the page inside. Desktop windows report zero margins. The tests stand in for a phone's
  margins with `SafeArea.additionalMargins`.
- **The halftone page** is a repeated picture of one dot (`apps/game/images/halftone/`, drawn by
  `tools/assets/make_halftone_tiles.py`), not a Canvas. A Canvas fills in after the screen appears, and on a phone that was
  a flash of black on every screen change.
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

## iCloud sync

iCloud sync keeps the players and the top 10 (the `shared` part of the save, `game-architecture.md`) the same on every
device signed in to one Apple Account. It is on Apple devices only, behind the Settings switch (off by default) and the
parental gate. The game has no account, sign-in or server of its own: it uses the iCloud key-value store of the
device's own Apple Account and nothing else. With the switch off the game never reads, writes or listens to the store
([Compliance](COMPLIANCE.md)).

| Layer | File | Holds |
|---|---|---|
| The interface | `src/services/cloud_store.hpp` | `CloudStore`: `available()`, `read()`, `write()`, `set_change_handler()`. `NullCloudStore` is never available |
| The rules | `src/services/cloud_sync.{hpp,cpp}` | `sync_with_cloud`, plain C++ over a `CloudStore`: read, `parse_shared`, `merge`, `adopt_shared`, write back only when the text differs |
| The controller | `src/game_controller.cpp` | The switch and its gate, when a sync may run, and what the screens are told |
| The shim | `src/platform/ios/cloud_platform.mm` | `NSUbiquitousKeyValueStore`: one key, one notification, no decisions |

`src/platform/cloud_platform.hpp` declares `make_cloud_store()`, and every directory under `src/platform/<os>/` defines
it: the iOS one returns the key-value store, every other one `NullCloudStore`. CMake compiles the directory of the target,
so no `#if` is involved. `NSUbiquitousKeyValueStore` is in Foundation, so no framework is added.

### How the store is used

- **One key**, `hbm.shared.v2`, holds the text `serialise_shared` writes (`{"version": 2, "shared": {...}}`). A few
  kilobytes for a family. The game refuses to write more than 900,000 bytes (`CLOUD_PAYLOAD_LIMIT_BYTES`).
- **Nothing is touched until the game asks.** Making the store, and `available()`, read no key. `available()` is true when
  the bundle was built with the entitlement (`HBM_ICLOUD_ENTITLED`, from `HEART_BOOM_MATHS_IOS_ICLOUD`) and
  `NSFileManager.ubiquityIdentityToken` is not nil (an iCloud account is signed in). `read()`, `write()` and the first
  non-empty `set_change_handler()` are called only while the switch is on. The handler registration adds the
  `NSUbiquitousKeyValueStoreDidChangeExternallyNotification` observer and calls `synchronize` once, as Apple's guide
  advises at launch.
- **The notice reaches the main thread.** The observer is added with the main operation queue, so the handler runs on the
  application's main (GUI) thread whatever thread iCloud's notice comes from. A server change, the first sync and a change
  of account all call it; a quota violation is only logged. Nothing blocks: the store keeps values in memory and iCloud
  uploads them later.
- **The rules never trust the cloud copy.** A payload of a later version is left alone (decision P7). One that cannot be
  read is replaced by this device's part. A merge that would break a format limit keeps the local data and writes
  nothing. The local save is never damaged by what is in the cloud.

### When a sync runs

The controller syncs only while the switch is on and the store is available, and only on the title (the core idle, or the
game over with no name waiting) or on the pause screen: the moments `GameSession::set_wallet` takes a wallet. The triggers
are the switch being turned on, start-up after the save loads, the app becoming active, the store's change handler and a
save. A trigger at any other moment (during play, on the level-clear or win card, while a name is waiting) only marks a
sync as owed, and it runs at the next such moment. After a sync that changed the local data the session takes the player's
wallet and win mark again and the screens are told.

This is what keeps `record_wallet` from destroying coins. It counts a fall in the session's wallet as coins spent. Coins
another device earned, merged in while the session still held the old wallet, would be booked as spent by the next save.
`Coins earned on another device are never booked as spent here` (`test_cloud_controller.cpp`) plays that out.

### The entitlement rule

The key-value store needs the entitlement `com.apple.developer.ubiquity-kvstore-identifier` (type string, value
`$(TeamIdentifierPrefix)$(CFBundleIdentifier)` as Xcode writes it), which follows the provisioning profile exactly as the
Game Center rule above does. Apple says an app must request it to use the store; what the store does without it is not
documented on the pages read, so the game does not offer the switch without it. And Xcode refuses to sign a bundle that
asks for an entitlement the profile lacks, so:

1. `ios.cmake` writes `heart-boom-maths.entitlements` in the build directory from `heart-boom-maths.entitlements.in`,
   with the Game Center entry when `HEART_BOOM_MATHS_IOS_GAME_CENTER` is ON and the key-value store entry when
   `HEART_BOOM_MATHS_IOS_ICLOUD` is ON. Both default to ON locally. With both OFF no entitlements file is given to Xcode.
2. Step 2 of the signed build reads the profile and passes `-DHEART_BOOM_MATHS_IOS_ICLOUD=ON` only when the profile's
   entitlements hold the key with the value `TEAMID.com.iainchesworthlabs.heartboommaths`, and `OFF` otherwise (a
   wildcard or another value counts as not carrying it), and prints which in the log and the job summary.
3. `OFF` leaves out the entitlement, and the target is built with `HBM_ICLOUD_ENTITLED=0`, so `available()` is false and
   the Settings row is not offered. Nothing else changes. The unsigned build (`ios-game-build.yml`) works with either.
4. `verify_game_app.sh --icloud on|off` checks the state it was given: the generated file holds exactly the entries of
   that state, and for a signed bundle the signature carries the key with the value `<team>.<bundle id>` (on) or does not
   carry it (off). It also fails on a `CloudKit` framework: `NSUbiquitousKeyValueStore` is the only cloud API.

When the owner turns the iCloud capability on for the App ID and makes the profile again, the next run builds with sync
on, with no change to the workflow.

### Not compiled, not run

**`cloud_platform.mm` has not been compiled.** It was written on a Windows machine with Apple's documentation open and no
SDK. Neither have the changes to `ios.cmake`, `verify_game_app.sh` and the two workflows been run: they need a Mac. The
entitlements file that CMake would write was checked for all four option states on Windows (a `cmake -P` script and a
property list reader); nothing else of the Apple side was. Expect the first macOS run to be where a mistake shows (see
the end of this page). Everything above the shim (the rules, the controller, the Settings row) is compiled and tested on
every desktop.

### What Apple's documentation says, and what is unconfirmed

Read on 15 Nov 2026. Apple's documentation site is built by script, so the pages marked (data) were read through the
JSON that the page itself loads (`developer.apple.com/tutorials/data/documentation/...`), which holds the same text.

| Question | Answer | Page |
|---|---|---|
| Limits of the store | At most 1,024 keys; 1 MB of storage for all values; 1 MB for one value (a 1 MB value uses the whole quota); a key of at most 128 characters in UTF-16, which does not count against the quota. A write over a limit fails and posts the change notification with the quota-violation reason; a key that is too long raises an exception. The older design guide says a key is at most 64 bytes in UTF-8: the game's key is 13 bytes, which meets both | [NSUbiquitousKeyValueStore](https://developer.apple.com/documentation/foundation/nsubiquitouskeyvaluestore) (data); [Design for key-value data in iCloud](https://developer.apple.com/library/archive/documentation/General/Conceptual/iCloudDesignGuide/Chapters/DesigningForKey-ValueDataIniCloud.html) |
| The notification | `NSUbiquitousKeyValueStoreDidChangeExternallyNotification`; register shortly after launch with the default store as the object. Reasons: server change, initial sync, quota violation, account change | [didChangeExternallyNotification](https://developer.apple.com/documentation/foundation/nsubiquitouskeyvaluestore/didchangeexternallynotification) (data); the design guide |
| `synchronize` | The guide registers the observer and calls `synchronize` once at launch, and says it need not be called again unless the app needs the fastest possible upload. Whether it can block is not stated | The design guide |
| No account | "If the device doesn't have an active Apple account, the changes remain only on the current device", and iCloud reconciles them at sign-in | NSUbiquitousKeyValueStore (data) |
| The entitlement | `com.apple.developer.ubiquity-kvstore-identifier`, a string, added by enabling the iCloud capability and the "Key-value storage" service | [iCloud Key-Value Store Entitlement](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.developer.ubiquity-kvstore-identifier) (data); [iCloud fundamentals](https://developer.apple.com/library/archive/documentation/General/Conceptual/iCloudDesignGuide/Chapters/iCloudFundametals.html) (name only) |
| Privacy manifest | `NSUbiquitousKeyValueStore` is not on the required-reason list: the categories are file timestamp, system boot time, disk space, active keyboards and user defaults, and the user-defaults category lists `UserDefaults` alone. The manifest is unchanged | [NSPrivacyAccessedAPIType](https://developer.apple.com/documentation/bundleresources/app-privacy-configuration/nsprivacyaccessedapitypes/nsprivacyaccessedapitype) (data); [Describing use of required reason API](https://developer.apple.com/documentation/bundleresources/describing-use-of-required-reason-api) (data) |
| Signed in | `ubiquityIdentityToken` is an opaque token for "the current user's iCloud Drive Documents identity", nil when iCloud is unavailable or no user is logged in; reading it is fast and may be done on the main thread | [ubiquityIdentityToken](https://developer.apple.com/documentation/foundation/filemanager/ubiquityidentitytoken) (data) |
| What "collect" means | Transmitting data off the device where you or your partners can access it; data processed only on the device is not collected. iCloud and the key-value store are not named | [App Privacy details](https://developer.apple.com/app-store/app-privacy-details/) |
| Sensitive data | The store's page says it keeps information on disk unencrypted and advises the Keychain for sensitive information | NSUbiquitousKeyValueStore (data) |

**Unconfirmed**, to settle on a device or with Apple:

- The **value** of the entitlement. `$(TeamIdentifierPrefix)$(CFBundleIdentifier)` is what Xcode writes when the capability is
  switched on, but none of the pages read gives the value. The release job therefore accepts the profile's key only when
  it equals `TEAMID.com.iainchesworthlabs.heartboommaths`, and `verify_game_app.sh` checks the signed value the same way.
- Whether **`ubiquityIdentityToken` is nil with iCloud Drive off** while an account is signed in. The page ties the token
  to iCloud Drive, and the store itself works with no Drive. If it is nil there, the row is hidden for those people (a
  safe failure). No better public way to ask "is an account signed in" without CloudKit was found, and CloudKit is not
  used.
- How the store behaves for a **child's Apple Account in Family Sharing**: whether iCloud and the key-value store are on,
  whether a parent can turn them off, and what a parent sees. Apple's Family Sharing pages
  ([How Family Sharing works](https://support.apple.com/en-us/105062), [parental controls overview](https://support.apple.com/en-us/119854))
  say nothing about iCloud for a child.
- Whether the game's key shows in **Settings, your name, iCloud, Manage Account Storage** and can be deleted there. Apple's
  page on managing iCloud storage ([Manage your iCloud storage](https://support.apple.com/en-us/108922)) shows the path and
  a "Delete Data from iCloud" button for some apps, and does not say whether a third-party key-value store is listed.
- Whether the **store is thread-safe** and whether `synchronize` can block: the pages are silent. The game uses the store
  from the main thread only and calls `synchronize` once.
- Whether Apple's **App Privacy answers** count data in the person's own iCloud as collected. By Apple's definition it is
  not (the developer cannot access it), but the page does not say so. The owner decides the answers.

## What is checked, and where

| What | Where | How |
|---|---|---|
| The Game Center rules: idle until the switch, handler installed once and only then, no send when signed out or restricted, a late answer ignored, the identifier passed through | Every desktop, in CI | `game.*` tests in `tests/test_game_center.cpp`, over a fake backend |
| The Settings path: the gate must be passed before the service is enabled, and a finished game then sends one score | Every desktop, in CI | The same tests, with the real `GameController` |
| `NullScoreService` elsewhere, the Settings row hidden | Every desktop, in CI | Existing controller and UI tests |
| The iCloud sync rules: no payload, a later version, an unreadable payload, a merge that is refused, a write the store does not take, a made-up player, and two devices settling on the same data (players, coins, a rename, a removal, top 10 entries) | Every desktop, in CI | `game.*` tests in `tests/test_cloud_sync.cpp`, over a fake store |
| The sync in the controller: the gate to turn on, no gate to turn off, no store call of any kind while the switch is off, each trigger, an owed sync running at the next allowed moment, a name pending, the current player removed elsewhere, the coins of another device never booked as spent | Every desktop, in CI | `game.*` tests in `tests/test_cloud_controller.cpp` |
| The Settings row of iCloud sync and its gate, at both size classes | Every desktop, in CI | `ui.*` tests in `tests/test_ui_cloud.cpp` |
| The icon equals its script | Every desktop, in CI | `game.icon_placeholder` |
| The game compiles and links for a device against static Qt, and the bundle has the right keys, entitlement file, GameKit and none of the forbidden frameworks | A `macos-26` runner | `ios-game-build.yml` |
| The sound rules: the clips loaded at start-up, volumes, looping, the session activated by the first sound, interruption and reset, a clip that fails to load | Every desktop, in CI | `game.*` tests in `tests/test_clip_audio.cpp`, over a fake host and fake players |
| The sound files are in the executable, and AVFoundation is linked | A `macos-26` runner | `verify_game_app.sh` |
| The Objective-C++ shims (GameKit, AVFoundation, UIKit and CoreHaptics for [haptics](haptics.md), and the iCloud key-value store) | A `macos-26` runner (compile only) | `ios-game-build.yml`. Not run by any test. **`cloud_platform.mm` has not been compiled by anyone yet** |
| The signed IPA: the signature, an App Store profile for the identifier, the Game Center and iCloud entitlements in the state the profile allowed, Apple's own validation | A `macos-26` runner, with the signing secrets | The `ios` job of `release.yml`. Only the runner can show it: nothing of Xcode runs on a Windows or Linux machine |
| The game makes sound on a device, goes quiet with the silent switch, and recovers after a phone call, Siri and a Screen Time lock | **Not done**: needs a device | Owner |
| The game starts on a device, plays, and pauses and saves when it leaves the foreground | **Not done**: needs a device | Owner |
| Game Center sign-in, a real submission, a child account, Screen Time | **Not done**: needs a device and the leaderboard | Owner (below) |
| iCloud sync between two devices, a child's account, signed out, iCloud Drive off, the traffic capture | **Not done**: needs two devices and an Apple Account | Owner (below) |

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
5. For **iCloud sync**: turn the **iCloud capability with Key-value storage** on for the same App ID, make the App Store
   profile again (one profile can hold Game Center and iCloud, so do both before making it) and set the secret again.
   Until then builds are signed without the entitlement and the Settings row is not offered. Then **review the App
   Privacy answers** in App Store Connect (see "Unconfirmed" above: the game keeps the players and the top 10 only in the
   family's own iCloud, which the developer cannot read) and the iCloud wording of the
   [privacy policy](privacy-policy.md), and decide whether nicknames kept unencrypted in the key-value store (Apple's
   page advises against sensitive data there) are acceptable. Run the **device tests** of the iCloud items in
   [Compliance](COMPLIANCE.md): two devices on one account, a child's account, iCloud signed out and iCloud Drive off, and a
   traffic capture with the switch on and off. Record the results there, and settle each "Unconfirmed" item above.
6. **Device tests** from [Compliance](COMPLIANCE.md), with the results recorded there:
   - the app limit and Downtime on an iPhone with a child Apple Account: the game pauses, saves and resumes;
   - Game Center multiplayer and adding friends set to Don't Allow: a score is still sent, or not, as intended (the game
     treats only `GKErrorParentalControlsBlocked` as a restriction, which this test confirms or changes);
   - a fresh install with the switch off shows **no sign-in sheet** and makes **no network request** (traffic capture),
     and with the switch on only Game Center's own traffic appears;
   - how a child's account appears on a global leaderboard, before score submission is turned on;
   - what the sign-in sheet looks like when the switch is turned on while signed out.
7. The **age rating**, privacy, Game Center and iCloud answers in App Store Connect.

## Things the first macOS run may show

The Mac side has not been run by the author of this page, so these are where a first red run would most likely be (for the
signed release job: the archive of a CMake project that holds many static-library targets, the scheme name, Apple's
validation of the property list and the privacy declarations, and a profile that is not as the job expects; each prints
what it found before it stops):
Qt's own handling of the launch screen when the target has its own `Info.plist.in` (the property list gives
`UILaunchScreen` itself); the static plugin import of `qmlimportscanner`; the Objective-C++ of the shims, which no compiler has seen against the
real SDK yet (`avf_audio_backend.mm` was only checked against hand-written stubs of the AVFoundation types, and
`cloud_platform.mm` against nothing: the block that captures a `std::shared_ptr`, the `id<NSObject>` member under ARC and the
`HBM_ICLOUD_ENTITLED` definition are the places to look); the entitlements file that CMake writes, whose
`$(TeamIdentifierPrefix)$(CFBundleIdentifier)` Xcode must expand, and the profile test of the release job that expects the
value `TEAMID.com.iainchesworthlabs.heartboommaths`; and the
name search of `verify_game_app.sh` for the sound files in the executable. The workflow prints the kits it found and the errors of the
build first, to make each of them quick to read.
