# Decisions, gaps and risks

## Accepted on 14 Nov 2026

Iain accepted all twelve proposals from the Implementation Plan. The PRD and the plan already reflect them.

| # | Item | Decided | Where it lands |
|---|---|---|---|
| 1 | Time added per right answer | Capped at the level time | Core scoring, M0 |
| 2 | Multiplier model | The run resets to 0 on a wrong answer and the step drops by one. A run of 3 sets the step to at least ×2, a run of 6 sets ×3 | Core scoring, M0 |
| 3 | QUIT on the pause screen | Ends the run as a game over. The score can enter the top 10 and coins stay | Pause screen, M0 |
| 4 | Minimum operand | 1 in the generator (the PRD range for intermediates and answers is 0 to the largest number) | Generator, M0 |
| 5 | Endless number range | The largest number stays at 1,000 | Generator, M0 |
| 6 | Game Center switch | In Settings on Apple devices only, off by default, behind the parental gate (board CP14) | Settings, services |
| 7 | Saved run | Save an interrupted run. CONTINUE resumes it once (boards CP18 and CP16) | Persistence, Title, Pause |
| 8 | Phone orientation | Portrait only | App layer |
| 9 | Desktop focus loss | Pause the run | Lifecycle |
| 10 | Distribution | `.dmg` (signed and notarised), zip and AppImage on GitHub Releases, and a static host for the web build | M7 |
| 11 | Repository | `iainchesworthlabs/heart-boom-maths` | M0 |
| 12 | Parental gate form | A number written in words, typed back (board CP17) | M4 |

## Confirmed at hand-over (14 Nov 2026)

| Item | Decided |
|---|---|
| Gaps G1 to G8 | All eight accepted as suggested below. M0 writes each into the PRD and pins it with a test |
| Repository layout | The organisation template's: `libs/core` for the rules and `apps/game` for the Qt and QML application, not the handover's five top-level folders |
| What is published | Neutral documentation is committed and published through GitHub Pages. The design spec, boards and handover notes stay local in `private/`, which Git ignores |

## Approved differences from the design boards (15 Nov 2026)

The boards are the reference for every screen. After a side-by-side review of the built screens, the project owner approved these differences, so they are not defects:

| Screen | Difference | Decision |
|---|---|---|
| Game over | A HOME button beside TOP 10, which the board does not show | Keep. HOME returns to Title, as `game-architecture.md` says |
| About | A fourth row, "Source code", for the repository link | Keep |
| Play, last 10 seconds | The hint button stays on screen during the alert | Keep. A hint is allowed in the alert zone |
| Play, wrong answer | The dimmed tiles look khaki, where the board shows cream | Keep |
| All screens | The halftone dots are smaller than on the board | Keep |
| Title, Level clear, Win, Game over, Name entry, Pause | The star bursts are regular shapes, not the board's hand-drawn polygons | Keep |
| Win | Spacing inside the coin card | Keep |

Everything else that differs from a board is treated as a defect and fixed, for example the width ratio of the gate and name-entry buttons, the height of the question digits and the layout of endless questions.

## Tracy in the core (15 Nov 2026)

The project owner allows Tracy in `libs/core`. The rule that the core includes only the C++ standard library has this one exception. Tracy is reached only through `libs/core/include/heart_boom_maths/profile.hpp`, whose macros are empty unless profiling is built in, so normal and release builds have no Tracy code and no overhead. Tracy is a profiler and takes no part in the rules: state, hashes and replays are unaffected, and a test holds that. Profiling is on in debug builds. Desktop release builds (Windows, Linux, macOS) carry Tracy dormant, as the organisation standard allows: it listens on the loopback interface only, collects nothing until a viewer connects, and starts only when a person asks at run time (an environment variable or a command line option, off by default), so a release can be profiled without rebuilding. The iOS, Android and WebAssembly builds are built without Tracy, so the shipped mobile and web apps have no profiler listener at all. Sanitizer, coverage and fuzz builds leave it off.

## Haptics (15 Nov 2026)

The project owner asked for a little touch feedback on phones: on a right or a wrong answer and on a few other actions, not overboard, with a preference to turn it off. It is on by default, a Settings switch that is shown only where the device can make haptics, and saved as the optional `haptics` field of the save (an older save reads as on; the schema version stays 1). It is never the only cue: every haptic goes with a visual cue that already exists, and the game plays the same without it. It needs no permission and sends nothing anywhere (no `VIBRATE` on Android, no change to the privacy manifest). The feel (which beats, how long, how close together) is a first guess to be judged on a device. The events, the platform notes and the device checks are in `haptics.md`.

## After v1: players, sync and four stages (15 Nov 2026)

The project owner accepted a plan for the game after v1 and the twelve decisions below. The PRD is the owner's to edit and follows; until it does, this table and `SPEC.md` are the record. Each stage's rules are written into `SPEC.md` when that stage starts.

Order of work: the foundation (players, save version 2, the merge, iCloud sync), then stage 2 (Dress up), stage 1 (It knows her), stage 3 (More maths), stage 4 (More ways to play).

| # | Part | Decided |
|---|---|---|
| 1 | Foundation | iCloud sync is allowed. The rules "no network calls or accounts", "names never leave the device" and "no cloud sync" become: nothing leaves the family's own devices and iCloud account, and nothing reaches the developer. The switch is off by default, behind the parental gate |
| 2 | Foundation | Players and the top 10 sync. Settings switches and the saved run stay on each device |
| 3 | Foundation | Up to four players. Adding is free, removing is behind the gate. A top 10 entry takes the player's name |
| 4 | Stage 2 | The place where coins are spent is called DRESS UP. "No store" in `SPEC.md` becomes "nothing costs real money" |
| 5 | Stage 2 | Diamonds bring animal friends at set totals. There is no second price list |
| 6 | Stage 2 | One bonus question per level carries a diamond. A hint is allowed, and missing it costs nothing extra |
| 7 | Stage 2 | Costumes are vector redraws of the designer's drawings, not scans |
| 8 | Stage 1 | A game may start at any level the player has cleared, and earns 300 points for each level skipped |
| 9 | Stage 1 | Look-again pictures of missed sums go on the level-clear and game-over cards, not into play. The clock rules do not change |
| 10 | Stage 3 | Worlds (Number Bonds, Times Tables, Sharing) are all open from the start, share coins, and each has its own top 10 |
| 11 | Stage 4 | Calm mode pays coins but has no points and no top 10 |
| 12 | Stage 4 | Two-player is for tablets and desktop. Most points in one level wins |

The mechanics of the party cat, the bonus animal and diamonds are new proposals: the PRD holds only their names.

Still to do for decision 1, with the sync work: `COMPLIANCE.md`, the privacy policy and the store forms, and a check of how Apple's privacy answers treat data kept in the user's own iCloud.

### Gaps in the players rules (15 Nov 2026)

Small points the twelve decisions do not state. The owner accepted all eight answers on 15 Nov 2026.

| # | Gap | Decided |
|---|---|---|
| P1 | What does a player with no name show as? | PLAYER 1, PLAYER 2 and so on, by place in the list |
| P2 | Can a player be named without reaching the top 10? | Yes. WHO'S PLAYING opens the name screen to name or rename a player at any time |
| P3 | What does NEW BEST! mean when players share one top 10? | The score beat the current player's own best. Every finished game counts towards it |
| P4 | A device has one saved run. What if another player plays? | The other player's run stays until the device has to save a run for the player now playing, which replaces it. Removing a player drops that player's run |
| P5 | When can the player be switched? | From the title only |
| P6 | Two devices each add players while apart, and the merge has more than four | All are kept. `max_players` limits adding only |
| P7 | A sync payload written by a later build | It is left alone and not merged, as a newer save file is |
| P8 | WHO'S PLAYING and the player chip have no board | Built from the existing components for the designer to review. A board follows |

### The questions felt too alike (15 Nov 2026)

The owner reported that the questions felt less random than they should. The generator had three causes. The owner decided to fix the dealing and the tiles, and to leave the small levels to stage 1.

| Cause | Detail | Decided |
|---|---|---|
| Small levels | Levels 1 and 2 have 10 and 15 possible sums, so every game opens with the same 25 | No change to the level table. Stage 1's start level lets a player skip them |
| Answer-first dealing | Larger levels chose the answer evenly and then split it, so sums with few splits came up far too often. On level 3, "1 + 1" was in about 4 games of 10, against 1 in 10 if every sum were equally likely | Every possible question of a kind has the same chance. The choice between kinds on a mixed level stays even |
| Tiles | The wrong tiles were mostly the answer ±1 and ±2, so the right answer was usually one of the middle values | How many wrong tiles sit below the answer is chosen evenly from what the range allows, so the answer's place in the order gives nothing away |

The rules are in `SPEC.md`, Question generation. Every golden replay is regenerated with the change.

## Earlier decisions (7 and 14 Nov)

Alert is the last 10 s of the 60 and gives no extra time. Correct answers keep adding time during the alert. Coins survive the bomb. Wrong answers cost time with clear feedback. When the bomb goes off the game is over, and a new game starts at level 1. 12 levels, then endless mode. Four answer tiles on every level, and a hint removes two wrong ones (removing one is the alternative to playtest). Look is Comic Pop. Name is Heart Boom Maths (first chosen as Star Bomb, then changed because Star Boom was already taken). The bomb is a pink heart (option A on the design canvas) and the explosion is a star burst. The game-over card shows a best score, and a high score earns a typed name on a top 10 that stays on the device. Settings (sound, music) and About exist. Game Center is integrated on Apple devices and assumed allowed at this rating. Target age rating is the lowest in each store. Diamonds, characters and the bonus animal are parked. The ESP32-P4 handheld is v2.

## Gaps found while writing this pack

These are small points the PRD and plan do not state. Settle each in M0 and write the answer into the PRD, then add a test. A suggested answer is given so work is not blocked.

| # | Gap | Suggested answer |
|---|---|---|
| G1 | Is the answer that completes a run of 3 or 6 scored at the new step or the old one? | New step. The flame badge grows and the flash plays on that same answer |
| G2 | Does the run (correct in a row) carry across levels? | Yes. It belongs to the game, and resets on a new game. The saved run already includes run and step |
| G3 | A wrong answer when time left is 3 s or less | Time clamps at 0 and the bomb goes off at once. The wrong-answer feedback is skipped and the boom plays |
| G4 | What does FINISH on the win screen do? | Same as QUIT: the run ends, the score can enter the top 10 |
| G5 | In endless, how are the non-multi-term questions and the three- versus four-term questions split? | Even split inside each group. Record the numbers in `tuning.json` so they are not hidden in code |
| G6 | The plan's save table lacks a field for the Game Center switch | Add `game_center` (boolean, default false). Decision 6 needs it to persist |
| G7 | How many ticks does leftover time convert from? | Whole seconds left, rounded down, times 10 points |
| G8 | Does a hint change the streak? | No. A hint only costs coins and removes tiles |

Two small drawing gaps in the plan and canvas: the navigation diagram has no Game over to Play edge (the text covers PLAY AGAIN), and the layers diagram does not list `LinkOpener` or the notices. Neither blocks the work.

## Open risks

| Risk | Response | Settled in |
|---|---|---|
| WebAssembly storage: the default file system is in memory, and the `QSettings` backend needs cookies and is limited to 5 MB | One `QSettings` key, proved in the web spike, with a small JavaScript storage bridge as the fallback | M2 |
| Qt on the App Store under GPL-3.0-or-later: Qt for iOS links statically, LGPL is a known problem on the App Store, and GPL is reported to conflict with Apple's terms | Unresolved. Get written advice. Options: a commercial Qt licence for the iOS build, or iOS after v1. Not legal advice | M2, then a final check before M7 |
| Cross-platform differences in the core | Integer-only state, the core's own PRNG, and the state hash compared on every platform in Tier 2 | M1, M3 |
| Game Center on a child account: behaviour and how a child appears on global leaderboards are unverified | Switch off by default and behind the gate. If device tests are unsatisfactory, ship `NullScoreService` everywhere | M5 |
| Screen-time signals: not yet confirmed that each limit moves the app out of the Active state | Device tests on child accounts. Add platform hooks if a case is missed | M5 |
| Audio: Qt Multimedia on the web is unconfirmed, and no sound assets exist yet | Web spike. Assets are original or licensed and are listed in the notices | M2, M4 |
| Parental gate form: Apple or Google may judge it too easy | Check current guidelines at M4 and adjust before M7 | M4, M7 |
| Store name: availability of "Heart Boom Maths" is unchecked, and an earlier name was taken | Search both stores and the trademark registers before reserving the name | Before M5 |
| Parental controls on Windows and Linux are not researched | Research and record. The pause path already covers the general case | M5 |
| Australian Children's Online Privacy Code: registration due by 10 Dec 2026 | Re-read the Code once registered and update the PRD | M7 |

## Spike questions

Web spike (M2): does a save written through `QSettings` survive a reload in Chrome, Safari and Firefox, and what happens with cookies blocked? Does Qt report a hidden tab as a state change? Does audio play after the first tap? How large is the release download, and how long does a mid-range phone take to show the first screen? It passes when the first three answers are yes with cookies allowed. Behaviour with storage blocked and a download size limit are decided from the results.

iOS spike (M2): does a release build with Qt linked statically run on a device? Can the Objective-C++ stub reach GameKit from the C++ layer with no sign-in sheet while the switch is off? Is GPL-3.0-or-later code with Qt acceptable on the App Store, according to written advice? It passes on yes to all three, and a no to the third changes the iOS plan.
