# Children's privacy, screen time and store rules

Source: PRD sections "Platform services and age rating" and "Screen time limits and children's privacy", from public platform and store pages read on 14 Nov 2026. This is a summary for developers and not legal advice. Platform rules change, so re-read the linked pages before each store submission.

## What v1 must stay

Heart Boom Maths targets the lowest age rating in each store: 4+ on the App Store, and the matching lowest rating on Google Play (PEGI 3, ESRB Everyone) from the questionnaire. These constraints keep it there, and each one is a code or release rule:

| Constraint | Consequence in the code |
|---|---|
| Cartoon content only | The bomb pops into a star burst. No frightening art or sound |
| No ads, no purchases | Coins are earned in play and have no money value. No store code |
| No chat, no accounts | No network code for either |
| No third-party analytics or crash reporting | No such dependency may be added. CI and the release traffic capture check this |
| No location | No location permission |
| No notifications | Never request notification permission |
| No links out except behind a parental gate | One call opens a URL (`LinkOpener::open`), reached only through the gate. CI fails if another appears |
| Typed names stay on the device | Names are in the save file only |

Entering the App Store Kids Category is a separate decision. It adds the parental gate and no-third-party-analytics rules, which this list already follows.

## Data

v1 sends no personal data anywhere. The only data path is the optional Game Center score, which carries one number.

| Topic | Platform behaviour for children | Heart Boom Maths |
|---|---|---|
| Public profile | On Game Center global leaderboards a player's nickname, avatar and score are visible to all users. Apple's page also says children's activity is shared only with friends, and does not say which applies to a leaderboard score | Submits a score only. Never reads or shows a nickname, avatar, player ID or friend list |
| Friends and multiplayer | Parents can block them, and the controls apply only to games using those features | None used, and no friends-list request |
| Chat | Children cannot send or receive typed text in Game Center, and in-game voice chat is off | No chat |
| Activity sharing | In the United States, under-13s share with friends only or nobody | Does not read or change it |
| Android | Supervised accounts have no Play Games followers feature. Games aimed at under-13s need not use Play Games Services | v1 Android does not use Play Games Services |

Game Center on Apple devices is a Settings switch, off by default, behind the parental gate. With the switch off the game never contacts Game Center and never shows a sign-in sheet. The working assumption is that Game Center is allowed at this age rating. Confirm it against Apple's Kids Category rules before submission. If it is not allowed, remove score submission (`NullScoreService` on every platform) and keep the local top 10.

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
- [ ] Traffic capture on a device: the app makes no network requests of its own (Game Center switch off), and only the Game Center call with it on.
- [ ] Parental gate form checked against the current Apple and Google rules for children's apps.

## References

- Apple, Kids category: https://developer.apple.com/kids/
- Google Play, Families policy: https://support.google.com/googleplay/android-developer/answer/9893335
- Google Play, target audience and content: https://support.google.com/googleplay/android-developer/answer/9867159
- Apple, Game Center and privacy: https://www.apple.com/legal/privacy/data/en/game-center/
- Apple, Game Center parental controls: https://support.apple.com/en-au/HT201304
- Google, Play Games for supervised accounts: https://support.google.com/googleplay/answer/3129346
- Android games guidelines: https://developer.android.com/games/guidelines
- Google, Family Link daily limit: https://support.google.com/families/answer/7103340
- OAIC, Children's Online Privacy Code: https://www.oaic.gov.au/privacy/privacy-registers/privacy-codes/childrens-online-privacy-code
