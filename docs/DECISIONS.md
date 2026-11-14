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
