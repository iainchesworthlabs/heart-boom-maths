# Releasing

1. Merge pull requests into `develop`. Each user-visible change brings a changelog fragment.
2. Assemble the changelog on `develop` with a pull request:
   `python3 <icl-common-infra>/scripts/changelog_tool.py assemble --version 1.2.0 --date YYYY-MM-DD`.
   A final release needs the `## [1.2.0]` section this creates, and the Release workflow fails at its
   first job without it. A pre-release (`v1.2.0-rc.1`) can skip this step: its notes are the fragments
   still in `changelog.d/`, which stay there for the final release. `changelog_tool.py notes --version 1.2.0
   --allow-pending` prints the notes a release would get.
3. Tag the merge commit: `git tag -s v1.2.0 && git push origin v1.2.0`. The tag must point at a
   commit on `develop` and must be semantic version form; a suffix such as `-rc.1` makes a pre-release.
   The version before the suffix must equal `VERSION` in `project()` in `CMakeLists.txt` and `version`
   in `vcpkg.json`; the Release workflow stops at its first job when it does not, so change them in the
   pull request that assembles the changelog.
4. The Release workflow builds the Linux, macOS and Windows packages. Windows binaries are handled
   according to the `SIGN_WINDOWS` repository variable: `false`, `test` or `production`. The Linux package must
   start on Ubuntu 24.04 (glibc 2.39): the `check-glibc-floor` step fails the job when something in it needs a newer
   glibc, which a build on the newest runner can do with a single function. A project that needs a higher floor passes
   `allowed` and says why here.
5. The publish job waits for approval in the `release` environment, then writes `SHA512SUMS`,
   detached GPG signatures, an SPDX SBOM and build provenance, creates the GitHub Release, and for a
   final release fast-forwards `main` to the tag.

`main` only ever moves by that fast-forward. The docs site is deployed from the push to `main`.

## Verifying a download

```bash
gpg --verify SHA512SUMS.asc SHA512SUMS
sha512sum --check SHA512SUMS
gh attestation verify <file> --repo iainchesworthlabs/<repository>
```

The public key is `docs/signing-key.asc`. Its fingerprint is published in `SECURITY.md` of the organisation `.github` repository.

## Windows test signing

Test-signed files carry `-testsigned` in the name and are accepted only on machines that trust the
personal CA. Until a production certificate exists they may go on any release, final or
pre-release, and the notes say so. See `docs/test-signing.md` in `icl-common-infra`.

## Phones and tablets

A project that builds for Android or iOS follows section 7.7 of the standard in `icl-common-infra`, and signs and
distributes an iOS build with the shared actions described in `docs/ios-signing.md` there. An ad-hoc IPA lists the devices'
identifiers and so belongs only in a private repository; neither a release asset nor a workflow artifact is private in a
public one. So the template ships the TestFlight way only.

### iOS and TestFlight

The `ios` job of `release.yml` builds the **game** (`apps/game`, the `ios-game-release` preset) and sends it to
TestFlight. `apps/ios`, a one-screen UIKit stub, and `ios-build.yml` stay as the template of the path (compile, sign,
check, validate with Apple, upload) that a new project copies; they are not released. `docs/ios.md` has the detail of the
job, of what it checks and of the Game Center rule below.

- **`ios-game-build.yml`** compiles the game unsigned on every change that the iOS build reads, and looks inside the
  bundle with `apps/game/ios/verify_game_app.sh`. It needs no secret. It is not a required check (see `docs/ci.md`).
  `ios-build.yml` does the same for the stub.
- **The `ios` job of `release.yml`** runs only when the repository variable `SIGN_IOS_UPLOAD` is `true`; with it unset the
  release is made without iOS. It imports the certificate and profile (`import-ios-signing`), reads the profile (it stops
  if the profile is for another identifier or expires within 14 days), installs the two Qt kits, configures
  `ios-game-release` signed, with the build number of the run, archives and exports an App Store IPA (`export-ios-ipa`),
  checks the IPA with `verify_game_app.sh --signed --store`, and hands it to `upload-testflight`. A tag, or a manual run
  with `upload_testflight`, uploads; any other run has Apple validate the IPA and stops. The IPA is a workflow artifact
  for 7 days under the name `ios-appstore`, and is never put on the release.
- **Game Center follows the profile.** The entitlement is requested only when the provisioning profile carries it
  (`HEART_BOOM_MATHS_IOS_GAME_CENTER`, set by the job from the profile). Without it the build is signed and goes to
  TestFlight, and signing in to Game Center fails quietly in the game. The job prints which it chose.
- **iCloud sync follows the profile, the same way.** The key-value store entitlement
  (`com.apple.developer.ubiquity-kvstore-identifier`) is requested (`HEART_BOOM_MATHS_IOS_ICLOUD`) only when the profile
  carries it with the value `TEAMID.*` (how a profile normally holds it) or `TEAMID.com.iainchesworthlabs.heartboommaths`;
  the app itself asks for the full value. Without it the build is signed and goes to
  TestFlight with no iCloud sync switch in Settings. The job prints which it chose.
- **Running it by hand.** `gh workflow run release.yml --ref <branch>` builds and signs the game and has Apple validate
  it; add `-f upload_testflight=true` to upload the build to TestFlight from a branch with no tag. The other release jobs
  run too; the iOS one is `Package (iOS)`.
- **One identifier.** `HEART_BOOM_MATHS_APPLICATION_ID` in `CMakeLists.txt` is the bundle identifier
  (`com.iainchesworthlabs.<app>`, the same string as the Android application id). The iOS workflows repeat it as
  `BUNDLE_ID`, and `verify_app.sh` fails when the bundle says otherwise. `tools/instantiate.py` sets all three.
- **Build number.** `CFBundleVersion` is `HEART_BOOM_MATHS_IOS_BUILD_NUMBER` (the run number in the release job), or the
  project version when empty. TestFlight refuses a number that is not higher than every earlier one for the same version.
- **Property list.** `ITSAppUsesNonExemptEncryption` is `false`, which is the owner's declaration to Apple that the
  application uses no encryption beyond the system's. Change it only if that stops being true. The check refuses usage
  text for a permission the application does not ask for. Safe areas are the application's: the UIKit sample lays out in
  `safeAreaLayoutGuide`; a Qt Quick application reads `SafeArea.margins` (Qt 6.9 or later).
- **Signing names.** The signing needs, by name only: the organisation secrets `IOS_DIST_CERT_P12_BASE64`,
  `IOS_DIST_CERT_PASSWORD`, `APPSTORE_API_KEY_ID`, `APPSTORE_API_ISSUER_ID` and `APPSTORE_API_PRIVATE_KEY` (shared with
  the repository), the repository secret `IOS_PROVISIONING_PROFILE_BASE64`, and the repository variable `SIGN_IOS_UPLOAD`.

What the owner does in Apple's developer portal and App Store Connect, once per application (details in
`docs/ios-signing.md` of `icl-common-infra`):

1. Identifiers: register an explicit App ID for the bundle identifier.
2. Profiles, Distribution: an **App Store Connect** profile for that App ID and the Apple Distribution certificate, with no
   devices. Its base64 is the secret `IOS_PROVISIONING_PROFILE_BASE64`.
3. App Store Connect, Apps, New App: iOS, the bundle identifier, a name that is unique across the App Store (the name
   under the icon comes from `CFBundleDisplayName`, which is `HEART_BOOM_MATHS_DISPLAY_NAME`), any SKU.
4. TestFlight tab: an Internal Testing group with the testers.
5. Add the repository to the selected repositories of the organisation secrets above, and set `SIGN_IOS_UPLOAD` to `true`.
6. For the game's Game Center score (not needed for a first TestFlight build): turn the **Game Center** capability on for the
   App ID, make the App Store profile again and set the secret again, create the leaderboard in App Store Connect and put
   its identifier into `GAME_CENTER_LEADERBOARD_ID`. Also still to come before the store, not TestFlight: the real app
   icon and a privacy policy URL.
7. For iCloud sync (not needed for a first TestFlight build): turn the **iCloud** capability on for the App ID with
   **Key-value storage** ticked (no iCloud Documents or CloudKit), make the App Store profile again (once, after Game Center
   and iCloud are both on) and set the secret again. Then the two-device and child-account tests of `docs/ios.md`.

The agent that works in the repository does none of these: secrets, variables and the Apple account are the owner's.

### Android and Google Play

`apps/game/android` holds the manifest and resources, and `cmake/HbmAndroid.cmake` the identity and version of the package
(`docs/android.md` says how it is built and what the checks prove).

- **`android-build.yml`** builds the unsigned release packages and a debug-signed APK on every change that the Android build
  reads, and looks inside them with `apps/game/android/verify_package.sh` (no permission, not debuggable in the release,
  one architecture). It needs no secret and is not a required check (see `docs/ci.md`).
- **The `android` job of `release.yml`** runs only when the repository variable `SIGN_ANDROID_UPLOAD` is `true`; with it
  unset the release is made without Android and no signing secret is read. It writes the upload key from the secrets to a
  file that only the job's user can read, builds with `QT_ANDROID_SIGN_AAB` and `QT_ANDROID_SIGN_APK` (Qt reads the key from
  `QT_ANDROID_KEYSTORE_PATH`, `_ALIAS`, `_STORE_PASS` and `_KEY_PASS`), checks the signed APK (not debuggable, no permission,
  not signed with Android's debug key) and the AAB (`jarsigner -verify`), removes the key, and keeps the packages as the
  workflow artifact `android-aab-signed` for 7 days. It does not upload to Google Play and does not put anything on the
  release (only an artifact named `release-*` goes there); the owner adds the AAB to the internal testing track by hand.
  The job has not run: it needs the owner's key, and the first run is the test of it. Run it on a branch first
  (`gh workflow run release.yml --ref <branch>`), which signs and checks but publishes nothing.
- **One identifier.** `HEART_BOOM_MATHS_APPLICATION_ID` in `CMakeLists.txt` is the application id, the same string as the
  iOS bundle identifier. `android-build.yml` does not repeat it; `verify_package.sh` holds the package to it.
- **Version code.** The number of the run (`ICL_BUILD_NUMBER`), which only grows, and Play refuses a number that is not
  higher than every earlier upload. Upload only builds of `release.yml`: each workflow has its own run number.
- **Signing names.** The signing needs, by name only: the organisation secrets `ANDROID_KEYSTORE_BASE64`,
  `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and `ANDROID_KEY_PASSWORD` (shared with the repository; the standard in
  `icl-common-infra` calls it one upload key for every application), and the repository variable `SIGN_ANDROID_UPLOAD`.

What the owner does, once per application (details in `docs/secrets.md` of `icl-common-infra`):

1. Play Console: create the application for the id `com.iainchesworthlabs.heartboommaths`, and complete the listing, the
   content rating, the target audience and the Data safety form (`docs/store/google-play.md`).
2. Choose Play App Signing, and register the upload key's certificate. The upload key is the one in the secrets above; its
   offline copy is the owner's.
3. Add the repository to the selected repositories of the four organisation secrets, and set `SIGN_ANDROID_UPLOAD` to `true`.
4. After a tag, download the `android-aab-signed` artifact of the run and add the AAB to a release on the internal testing
   track. Test on devices (`docs/android.md`, "What only a device shows") before production.

The agent that works in the repository does none of these either.
