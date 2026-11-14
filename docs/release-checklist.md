# Release checklist

Done by hand before each release, and in full before `v1.0.0`. The rules behind it are in [Compliance](COMPLIANCE.md). The tag, changelog and signing mechanics are in [Releasing](releasing.md) and are not repeated here. Record each result (date, device, build number, pass or fail) in the pull request or release notes.

## 1. Before tagging

- [ ] CI is green on `develop` for the release commit, and `python3 tools/ci/precheck.py` passes.
- [ ] Replays and `tuning.json` are consistent: no replay changed without a deliberate rule or tuning change.
- [ ] The notices file builds, and every dependency has a licence file.
- [ ] The version in `CMakeLists.txt` and `vcpkg.json` matches the tag, and the changelog section exists (see [Releasing](releasing.md)).
- [ ] Store text matches the app: every statement in the [App Store](store/app-store.md) and [Google Play](store/google-play.md) drafts and in the [privacy policy](privacy-policy.md) is still true of this build.
- [ ] The privacy policy page is live at the address the About screen opens, and the address in each store listing is the same.

## 2. Device tests

Run on real devices, with the release build and no debugger. Compare with the [Compliance](COMPLIANCE.md) list.

### Screen time and parental controls

- [ ] iPhone, child Apple Account: set an app limit, let it run out, and confirm the game pauses, saves and resumes to the pause screen. Repeat with Downtime.
- [ ] Same iPhone with Game Center multiplayer and adding friends set to Don't Allow: confirm score submission still behaves, and that nothing is submitted while the Game Center switch is off.
- [ ] Android, Family Link child account: test a daily limit, an app limit and bedtime. Confirm the game pauses and saves each time. Repeat with a Digital Wellbeing app timer.
- [ ] macOS with Screen Time: confirm the same.
- [ ] Windows, Linux: the window losing focus pauses the run. Note any parental control found.
- [ ] Browser: a hidden tab pauses the run.
- [ ] After every case above, RESUME and QUIT work, the clock stays stopped until RESUME, and nothing is lost.
- [ ] The game shows no sign of the limit and has no ask-for-more-time button.

### No network traffic

- [ ] Capture the device's traffic during a full session: start, play two levels, pause, background, return, finish, view the top 10, open Settings and About. With Game Center off, the app makes no network requests of its own. On the web, only the initial load of the page and its files.
- [ ] iPhone and iPad with Game Center on: the only extra traffic is Game Center's, and only after a grown-up passes the gate.
- [ ] Read the merged Android manifest, the iOS property list and the desktop bundle. No permission or usage text beyond the [Google Play](store/google-play.md) and [App Store](store/app-store.md) pages. No location, notification, camera, microphone, contacts or photos entry.
- [ ] Confirm the build links no analytics or crash-reporting code.

### Parental gate and links

- [ ] Every link on About opens only after the gate is passed, and does nothing when it is failed. A wrong answer deals a new number.
- [ ] The Game Center switch cannot be turned on without the gate.
- [ ] No other control in the app opens a web address. (CI counts the calls; this is the by-hand check.)
- [ ] The gate form is checked against the current Apple and Google rules for children's apps (risk in [Decisions](DECISIONS.md)).

### Saved data

- [ ] Interrupt a game by every route (home button, lock, kill the app, a call), and CONTINUE resumes it once, then is gone.
- [ ] A damaged save file is moved to `save.json.bad` and the game starts on defaults without crashing.
- [ ] A save file from a newer version is left untouched and the game starts on defaults.
- [ ] Coins, settings and the top 10 survive a restart. A name typed in the top 10 appears nowhere outside the game.
- [ ] Uninstalling or clearing storage removes the data, as the privacy policy says. In the browser, blocked storage does not crash the game.

### Accessibility and look

- [ ] Every control has an accessible name and role, and text keeps 4.5:1 contrast.
- [ ] Muted, the game plays the same: every sound has a visual twin and no cue depends on colour alone.
- [ ] Both size classes: compact on a phone in portrait, regular on a tablet and in a desktop window. Phones do not rotate.
- [ ] Every screen is compared by hand with its board.
- [ ] Tap targets are comfortable for a small child: answer tiles at least 96 pt high on a phone.

## 3. Signing and secrets

The owner holds every secret. No one who works in the repository reads, prints or creates a secret value. The names below are the ones the release workflow and `docs/secrets.md` in `icl-common-infra` expect (verify against that file before each release).

| Needed for | Name | Scope |
|---|---|---|
| GPG signatures on the release | `REPO_GPG_PASSPHRASE`, and the GPG key secret (named in `docs/secrets.md`) | Organisation |
| Moving `main` to the tag | `RELEASE_BOT_APP_ID`, and the release bot's key secret (named in `docs/secrets.md`) | Organisation |
| Windows signing (test or production) | `WIN_TESTSIGN_PFX_BASE64`, `WIN_TESTSIGN_PFX_PASSWORD`, and the repository variable `SIGN_WINDOWS` (`false`, `test` or `production`) | Organisation, variable on the repository |
| iOS App Store build | `IOS_DIST_CERT_P12_BASE64`, `IOS_DIST_CERT_PASSWORD`, `APPSTORE_API_KEY_ID`, `APPSTORE_API_ISSUER_ID`, and the App Store Connect API key secret (named in `docs/secrets.md`) | Organisation |
| iOS provisioning for this app | `IOS_PROVISIONING_PROFILE_BASE64`, and the repository variable `SIGN_IOS_UPLOAD` set to `true` | Repository |
| macOS signing and notarisation | The macOS and notary names in `docs/secrets.md` | Organisation |
| Android signing | `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`, and the repository variable `SIGN_ANDROID_UPLOAD` set to `true` | Organisation, variable on the repository |

- [ ] The repository is in the selected repositories of each organisation secret above.
- [ ] The Apple developer account, the App ID for `com.iainchesworthlabs.heartboommaths` and the App Store Connect app record exist (steps in [Releasing](releasing.md)).
- [ ] A Google Play Console account and an app record exist, and the upload key is registered.
- [ ] Today's `release.yml` packages Linux, macOS and Windows and, when `SIGN_IOS_UPLOAD` is `true`, iOS, and, when `SIGN_ANDROID_UPLOAD` is `true`, a signed Android AAB (never run yet: try it on a branch first). It has no macOS notarisation job (verify). Add or run that before the release, or release without it and say so.
- [ ] The `release` environment approval is available to the owner.

## 4. Store submission by hand

Nothing here is automated, and nothing is filed from the repository.

- [ ] Store names are checked in both stores and in the trademark registers, before the name is reserved.
- [ ] Apple: App Information, age rating, App Privacy, export compliance, Kids Category decision, review notes, screenshots, build selected ([App Store draft](store/app-store.md)).
- [ ] Google: store listing, content rating, target audience, Data safety, app access, ads declaration, internal testing then production ([Google Play draft](store/google-play.md)).
- [ ] Both stores' children's rules are re-read on the day, and the "verify" lines in the drafts are resolved.
- [ ] Australian Children's Online Privacy Code: re-read once registered and confirm the game's scope (due 10 December 2026, see [Compliance](COMPLIANCE.md)).

## 5. Decisions only the owner can make

- [ ] Qt on the App Store under GPL-3.0-or-later: get written advice. Options are a commercial Qt licence for the iOS build, or iOS after v1 ([Decisions](DECISIONS.md), open risks).
- [ ] Whether to enter the Apple Kids Category, and the age band and Google target age groups to declare.
- [ ] Whether Game Center ships at this age rating. If not, `NullScoreService` everywhere.
- [ ] The final name, once checked.
- [ ] Price, store accounts and fees, and the public contact details and trader status each store asks for.
- [ ] The privacy contact email, the effective date, the web host for the browser build, and the final privacy page address.
- [ ] Whether the privacy page must go live before the first final release. The docs site deploys on a push to `main`, and `main` moves only on a final release tag ([Releasing](releasing.md)).
- [ ] Tagging `v1.0.0`.

## 6. After the tag

- [ ] Verify the downloads as described in [Releasing](releasing.md).
- [ ] The docs site shows the privacy page at the address the app opens.
- [ ] Install the store build on a clean device and run section 2's no-traffic and gate checks once more.
