# Android

The game builds for Android phones and tablets from the same CMake project and the same `apps/game` code as the desktop
builds. Qt's own tooling makes the package: `qt_add_executable` makes the program a shared library (the activity loads it),
and `androiddeployqt` turns that library, Qt's libraries and plugins, the QML modules and the files of `apps/game/android`
into an APK or an AAB, with Gradle. v1 builds `arm64-v8a` only.

## Build

You need Qt's Android kit and a desktop kit of the same version (for the host tools and `androiddeployqt`), the Android SDK
with the NDK that kit was built with, platform `android-36` and build tools `36.0.0`, and a JDK 17 or later. The versions
are in the `android` block of `.github/toolchain-versions.json`; Qt is the `qt` key.

| Variable | Meaning |
|---|---|
| `HEART_BOOM_MATHS_QT_ANDROID_ROOT` | Qt's Android kit, for example `Qt/6.12.0/android_arm64_v8a`. The kit names the architecture |
| `ICL_QT_ROOT` | The desktop kit of the same Qt version |
| `ANDROID_SDK_ROOT`, `ANDROID_NDK_ROOT` | The SDK and the NDK in it. Qt 6.12's kit is built with NDK r27c (`27.2.12479018`), which CI uses |

```bash
cmake --preset android-arm64-release      # an unsigned APK and AAB
cmake --build --preset android-arm64-release

cmake --preset android-arm64-debug        # an APK signed with Android's debug key, for a phone
cmake --build --preset android-arm64-debug
```

The packages are under `build/<preset>/apps/game/android-build/build/outputs/`: `apk/release/*-unsigned.apk`,
`bundle/release/*.aab` and `apk/debug/*-debug.apk`. The `apk` and `aab` targets make one of them alone. No tests are built:
vcpkg is not used on Android, and nothing runs on a device here. To build for an emulator on a PC, point
`HEART_BOOM_MATHS_QT_ANDROID_ROOT` at the `android_x86_64` kit; the same presets work, and nothing else in the project
names an architecture.

`apps/game/android/verify_package.sh <package> [--version x.y.z] [--release]` reads a finished package and checks it (below).
It needs `aapt2` from the SDK's build tools, on `PATH` or through `ANDROID_SDK_ROOT`.

## Identity and version

| | Value | Where it comes from |
|---|---|---|
| Application id | `com.iainchesworthlabs.heartboommaths` | `HEART_BOOM_MATHS_APPLICATION_ID` in `CMakeLists.txt`, the same string as the iOS bundle identifier |
| Version name | The project version, for example `0.1.0` | `project(VERSION)` in `CMakeLists.txt` |
| Version code | The CI run number | `ICL_BUILD_NUMBER`, then `GITHUB_RUN_NUMBER`, and 1 off CI |
| Label | Heart Boom Maths | `HEART_BOOM_MATHS_DISPLAY_NAME` |
| Levels | minimum 28, target and compile 36 | `cmake/HbmAndroid.cmake` |

Google Play refuses an upload whose version code is not higher than every earlier one, so the code must only grow. A run
number does, but each workflow has its own counter: upload to Play only builds of one workflow (`release.yml`), never a mix
of it and `android-build.yml`. A build that must go out under another number passes
`-DHEART_BOOM_MATHS_ANDROID_VERSION_CODE=<number>`.

## What the package asks for

**Nothing.** The manifest has no permission: no network, no storage, no camera, no microphone, no location, no
notifications. The game keeps one small file in its own private folder and plays sound, which need none. Qt's manifest
template has a permissions placeholder that Qt's core and network modules put `INTERNET` and storage permissions at,
whatever the program does; `apps/game/android/AndroidManifest.xml` does not have the placeholder, so none is inserted.

The merged manifest has one `uses-permission` line all the same, which AndroidX adds to every application that uses it:
`<application id>.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION`. It is the application's own signature-level permission for
receivers registered at run time. It asks nothing of a person and no other application can hold it. The check allows that
one name and fails on any other.

A permission is added only with the code that needs it and a decision in `DECISIONS.md`; the check then changes with it.

## Manifest decisions

- **Backup is off** (`allowBackup="false"`). The save holds the names a child types, and those never leave the device;
  Google's cloud backup would copy the file to Google's servers. The cost is that a new phone starts with an empty Top 10.
- **Portrait.** The design is a phone held upright (`DECISIONS.md`, 8), so the activity is locked to portrait, which the
  phone sees from the first frame. A tablet (smallest side 600 dp or more) running Android 16 or later ignores the lock for
  an application targeted at level 36 and follows the device, which is what the regular size class is for. The manifest does
  not declare the game category (`appCategory`), because Android exempts an application that does from that rule (check
  it on a tablet). **On a tablet with Android 15 or
  earlier the game stays portrait.** `docs/game-architecture.md` says only that phones are locked; if tablets should turn on
  those versions too, the lock moves from the manifest to a call made at start, which needs a new function in
  `src/platform/<os>/` for every system. That is a decision for the owner.
- **Touch is not required.** A Chromebook or a tablet with a keyboard can install and play it.
- **The launch colour** is the page yellow (`Theme.background`), so a launch is not a flash of the system's default colour.
  The system's own launch screen on Android 12 and later is the icon on the same colour.
- **The icon is a placeholder**: a pink heart on yellow, drawn in vector XML (`res/drawable/ic_launcher_foreground.xml`).
  The real icon is not designed yet; replace the two files and add a monochrome layer for themed icons then.
- **System bars.** Android 15 and later draw the app edge to edge whatever the app asks for, and the theme gives the bars
  dark icons over the yellow page. `SafeAreaFrame.qml` lays the whole application out inside Qt's `SafeArea.margins`
  (Qt 6.9 and later), with the yellow page running under the bars, so a camera cut-out or a status bar does not cover the
  top bar. A desktop window has no margins and the frame changes nothing. Seeing it work needs a device.

## Lifecycle and the save

Nothing Android-specific is needed. Qt reports a phone going to the background, the lock screen, the notification shade and a
split-screen focus change as `QGuiApplication::applicationStateChanged` leaving `Active`, and `main.cpp` already connects
that to `GameController::on_application_state(false)`, which pauses a running level, remembers the run, suspends sound and
saves, in that order, before the process can be stopped. The tick driver runs only while playing, so the long gap of a
background does not become ticks. This is what a Family Link limit or a Digital Wellbeing timer needs: they send the game to
the background or lock the device.

The save is `FileSaveStore` in `QStandardPaths::AppDataLocation`, which on Android is the application's private folder.
Writes are atomic (`QSaveFile`), the folder needs no permission and is removed with the application.

## Checks

`.github/workflows/android-build.yml` builds both presets on a hosted Linux runner and runs `verify_package.sh` on each
package. It is not a required check and not in the merge queue: it runs when something the Android build reads changes, and
by hand (`gh workflow run android-build.yml --ref <branch>`). It needs no secret. The check fails on:

- an application id, version name or label that is not the expected one, a version code that is not the run number;
- a minimum level under 28 or a target level under 35 (the one Google Play asks for);
- **any permission** other than the AndroidX line above, in the APK's merged manifest, and in the AAB's manifest read as text;
- `allowBackup` not false, an activity that is not locked to portrait;
- a release APK that is debuggable;
- native code for an architecture other than `arm64-v8a`, a missing program library, a missing launcher icon or activity.

The sizes of the packages are printed in the log and in the run's summary. The unsigned release packages and the
debug-signed APK are uploaded as artifacts (seven and fourteen days). The debug-signed APK installs on a phone that allows
apps from outside the store.

## What was and was not proved

Built and checked on a Windows PC with Qt 6.12.0's Android kit and NDK r28c, in `Debug` and `Release`: the packages build, the
package checks pass, the merged manifest has no permission but the one above, the version code is the number the build
was given, and the AAB (about 20 MB) is less than half the size of the APK (about 49 MB), which keeps Qt's libraries uncompressed so
that Android can map them. That kit has no Qt Multimedia, so the sound backend was not part of it.

Only CI or a device can show:

- the build with Qt Multimedia, and that it adds no permission (the check above fails the run if it does);
- the build with the pinned NDK (r27c) and the runner's JDK 17;
- the signed build of `release.yml`, which needs the owner's key.

## What only a device shows

Nothing counts as working until it ran on a device, and the project does not run the game on an emulator in CI.

- The game starts, draws the title screen and plays a level on a phone and a tablet, with sound.
- The launcher icon, the launch screen and the label on a real launcher; the system bars over the yellow page.
- The pause on every way of leaving: Home, the recent-apps screen, the lock button, an incoming call, split screen. The saved
  run resumes to the pause screen.
- **Family Link and Digital Wellbeing** (`COMPLIANCE.md`): a daily limit, an app limit and bedtime each pause and save the
  game, and a Play Store install on a child's supervised account works.
- The back gesture and the Back button (`ScreenFrame` handles `Keys.onBackPressed`).
- A traffic capture: the game makes no network request of its own.
- Rotation of a tablet on Android 16 or later, and a keyboard.

## What remains for the owner

- A Google Play Console account, the app record for `com.iainchesworthlabs.heartboommaths`, the listing, the content
  rating, the target-audience and Data safety forms (`store/google-play.md`), and the privacy policy address.
- The upload key: its secrets, the repository variable `SIGN_ANDROID_UPLOAD`, and Play App Signing. See
  [Releasing](releasing.md), "Android and Google Play".
- The device tests above, with the results recorded in `COMPLIANCE.md`.
- A decision on tablets with Android 15 or earlier (above), and the real icon.
