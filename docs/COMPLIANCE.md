# Children's privacy, screen time and store rules

Source: PRD sections "Platform services and age rating" and "Screen time limits and children's privacy", from public platform and store pages read on 14 Nov 2026. This is a summary for developers and not legal advice. Platform rules change, so re-read the linked pages before each store submission.

## What v1 must stay

Heart Boom Maths targets the lowest age rating in each store: 4+ on the App Store, and the matching lowest rating on Google Play (PEGI 3, ESRB Everyone) from the questionnaire. These constraints keep it there, and each one is a code or release rule:

| Constraint | Consequence in the code |
|---|---|
| Cartoon content only | The bomb pops into a star burst. No frightening art or sound |
| No ads, no purchases | Coins are earned in play and have no money value. No store code |
| No chat, no accounts | No network code for either, and no sign-in or server of the game's own. iCloud sync (below) uses the iCloud account the person already has on the device |
| No third-party analytics or crash reporting | No such dependency may be added. CI and the release traffic capture check this |
| No location | No location permission |
| No notifications | Never request notification permission |
| No links out except behind a parental gate | One call opens a URL (`LinkOpener::open`), reached only through the gate. CI fails if another appears |
| Typed names stay on the family's own devices | Names are in the save file. Only if a grown-up turns iCloud sync on do they also go to that family's iCloud account, and nowhere else. They never reach the developer |
| iCloud sync is off by default and behind the parental gate | A Settings switch on iPhone and iPad only. With it off, or on any other platform, the game touches no cloud API: nothing reads, writes or listens to the store. The one cloud API in the code base is `NSUbiquitousKeyValueStore`, in `src/platform/ios/cloud_platform.mm` |
| No permissions beyond what a store lists for a game with no data | Haptics use `performHapticFeedback` on Android, which needs none (never `Vibrator.vibrate()` and its VIBRATE permission), the UIKit feedback generators on iOS and `navigator.vibrate` in a browser. They collect and send nothing. See [Haptics](haptics.md) |
| No listening socket in a shipped build unless asked | The desktop release builds carry the Tracy client dormant: it starts only for `--profile` or `HBM_PROFILE=1`, and then listens on the loopback interface only. The web, Android and iOS builds have no Tracy, and CI checks that. See [Profiling](profiling.md) |

Entering the App Store Kids Category is a separate decision. It adds the parental gate and no-third-party-analytics rules, which this list already follows.

## Data

v1 sends no personal data to the developer or to anyone outside the family. There are two optional data paths, both Apple devices only, both off by default and behind the parental gate: the Game Center score, which carries one number, and iCloud sync, which keeps the players and the top 10 in the family's own iCloud account.

| Topic | Platform behaviour for children | Heart Boom Maths |
|---|---|---|
| Public profile | On Game Center global leaderboards a player's nickname, avatar and score are visible to all users. Apple's page also says children's activity is shared only with friends, and does not say which applies to a leaderboard score | Submits a score only. Never reads or shows a nickname, avatar, player ID or friend list |
| Friends and multiplayer | Parents can block them, and the controls apply only to games using those features | None used, and no friends-list request |
| Chat | Children cannot send or receive typed text in Game Center, and in-game voice chat is off | No chat |
| Activity sharing | In the United States, under-13s share with friends only or nobody | Does not read or change it |
| Android | Supervised accounts have no Play Games followers feature. Games aimed at under-13s need not use Play Games Services | v1 Android does not use Play Games Services |

Game Center on Apple devices is a Settings switch, off by default, behind the parental gate. With the switch off the game never contacts Game Center and never shows a sign-in sheet. The working assumption is that Game Center is allowed at this age rating. Confirm it against Apple's Kids Category rules before submission. If it is not allowed, remove score submission (`NullScoreService` on every platform) and keep the local top 10.

### iCloud sync

| Topic | Heart Boom Maths |
|---|---|
| What is stored | One piece of text under one key of the iCloud key-value store: the players (names, coins, win mark, best score) and the top 10 (names and scores). Not the Settings switches, the saved run or which player is current |
| Whose iCloud | The Apple Account signed in on the device, and only that one. The game has no account, sign-in or server of its own. Apple's iCloud moves the key between the devices on that account |
| Who can read it | The family's devices. Nothing is sent to the developer, and the developer has no way to read an iCloud account |
| When it is used | Only while the Settings switch is on. A default install never reads, writes or listens to the store. Turning it on needs the parental gate. Turning it off needs none and leaves the copy in iCloud as it is |
| Children | The names are the nicknames typed into the game (A to Z, 12 letters). Apple's page on the key-value store says it keeps values on disk unencrypted and advises against sensitive data; the game keeps nicknames and scores, no password, address or identifier. How the store behaves on a child's Apple Account in Family Sharing is not confirmed (below) |
| Apple's privacy answers | Data kept in the person's own iCloud that the developer cannot reach is, on Apple's definition of "collect" (data transmitted off the device where you or your partners can access it), not collected. Apple's page does not name iCloud, so the owner confirms the App Privacy answers in App Store Connect (docs/ios.md) |
| Privacy manifest | `NSUbiquitousKeyValueStore` is not on Apple's list of required-reason APIs, so the manifest does not change (docs/ios.md) |

## Rules by region

| Rule | What it asks | Heart Boom Maths |
|---|---|---|
| COPPA (United States) | Verifiable parental consent before collecting personal information from a child under 13 | Collects none, so no consent flow |
| GDPR (EU) | Age of digital consent runs from 13 to 16 by country | Same |
| Google Play Families policy | Declare target age group, add a privacy policy, disclose personal data collected from children, use only certified ad SDKs | Declares the age groups covering 5 to 12, links a privacy policy, has no ads |
| Australia, Children's Online Privacy Code | Must be registered by 10 December 2026, with no commencement date published yet. Covers social media, relevant electronic services (including online games with chat) and designated internet services children are likely to use | Single player, no chat, no data collected, so probably outside it. Re-read when registered |

## Screen time

Parents set limits in the operating system. The game cannot see or change them. Its job is to survive an interruption with nothing lost.

| Platform | What a parent can set |
|---|---|
| iPhone and iPad | Screen Time limits per app or category, and Downtime. A child at a limit can ask for more time |
| Android | Family Link daily limit, which locks the device after a warning, with bonus time from the parent. Digital Wellbeing app timers pause an app for the rest of the day |
| macOS | Screen Time through Family Sharing |
| Windows, Linux, browser | Not researched yet |

Rules for every platform (full list in `SPEC.md`): pause on any loss of focus or move to the background, save when pausing, resume to the pause screen with RESUME and QUIT, keep one saved run that resumes once, and never detect, warn about or work around system limits.

## Check before release

Each item is a task for M5 or M7, and the result goes in the PRD.

- [ ] iPhone with a child Apple Account: set an app limit, let it run out, and confirm the game pauses, saves and resumes.
- [ ] Same iPhone with Game Center multiplayer and adding friends set to Don't Allow: confirm score submission still behaves.
- [ ] Android with Family Link: test a daily limit, an app limit and bedtime, and confirm the game pauses and saves each time.
- [ ] Find out how a child's account appears on a global leaderboard before score submission is turned on.
- [ ] Research Windows and Linux parental controls, and test the pause on focus loss.
- [ ] Write the privacy policy: no data collected, no accounts, no ads. Link it from both stores and from About, behind the parental gate.
- [ ] Read the Children's Online Privacy Code once registered and confirm the game's scope.
- [ ] Traffic capture on a device: the app makes no network requests of its own (Game Center and iCloud sync switches off), and only the Game Center call with Game Center on.
- [ ] Traffic capture on a device with iCloud sync on: the game process makes no connection of its own; the only traffic is the system's own iCloud traffic to Apple, and nothing goes to any other host.
- [ ] A fresh install with iCloud sync off: nothing reads, writes or listens to the store (no iCloud sign-in or permission prompt, no traffic).
- [ ] Two devices on one Apple Account with sync on: a player added on each, coins earned on each, a rename, a removal and top 10 entries on each end up the same on both, and the coins are the sum of what each earned (none lost, none booked as spent).
- [ ] A child's Apple Account in Family Sharing: is the switch offered, does the key sync, what does the parent see in the iCloud settings. Record the result here and in `ios.md`.
- [ ] iCloud signed out, and iCloud Drive off while signed in: the row is not offered, nothing crashes, and signing back in resumes sync.
- [ ] Find where a parent can delete the game's iCloud data in the iCloud settings, and write the exact path into the privacy policy.
- [ ] On a desktop release package: with no flag and no `HBM_PROFILE`, nothing listens on port 8086 (`netstat`), and `TracyClient.dll` or the Tracy library is listed in the notices.
- [ ] Parental gate form checked against the current Apple and Google rules for children's apps.

## References

- Apple, Kids category: https://developer.apple.com/kids/
- Google Play, Families policy: https://support.google.com/googleplay/android-developer/answer/9893335
- Google Play, target audience and content: https://support.google.com/googleplay/android-developer/answer/9867159
- Apple, Game Center and privacy: https://www.apple.com/legal/privacy/data/en/game-center/
- Apple, Game Center parental controls: https://support.apple.com/en-au/HT201304
- Apple, the iCloud key-value store: https://developer.apple.com/documentation/foundation/nsubiquitouskeyvaluestore
- Apple, App Privacy details ("collect"): https://developer.apple.com/app-store/app-privacy-details/
- Google, Play Games for supervised accounts: https://support.google.com/googleplay/answer/3129346
- Android games guidelines: https://developer.android.com/games/guidelines
- Google, Family Link daily limit: https://support.google.com/families/answer/7103340
- OAIC, Children's Online Privacy Code: https://www.oaic.gov.au/privacy/privacy-registers/privacy-codes/childrens-online-privacy-code
