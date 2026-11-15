# Heart Boom Maths: game specification

Status: PRD v1 draft, revision 68, 14 Nov 2026. This file is a developer's copy of the rules in the PRD. If this file and the PRD disagree, the PRD wins and this file is wrong. Numbers are machine-readable in `libs/core/tests/data/prd-tables.json` (what the PRD says) and `libs/core/data/tuning.json` (what the build uses).

All timing and reward values are playtest defaults. They live in one `Tuning` struct, so a balance change is a data change.

## What the game is

A mental-arithmetic game for ages 5 to 12. Each question is an adding or taking-away number sentence with four answer tiles. The player answers 10 questions per level against a bomb timer. The bomb is a heart, and when it goes off it pops into a star burst. The rules come from the game's designer unless the PRD marks them as a proposal.

v1 is tap only. The core accepts an answer as a tile index, so Say and Write modes can be added later without a rule change.

## Level loop

1. A level starts with a clear play area and a full timer (60 s up to level 12).
2. A question appears, for example `8 + ? = 13`. Four answer tiles appear, exactly one correct.
3. A correct answer scores points at the current multiplier, earns 1 coin, and adds time (7 s up to level 12), never taking the timer above the level's time.
4. A wrong answer scores nothing, removes 3 s, and lowers the multiplier step by one. The correct tile is shown for 1 s.
5. A wrong answer counts as one of the 10 questions. After 10 answers the level is complete.
6. At 10 s remaining the alert state starts. The alert gives no extra time. A correct answer can lift the timer back out of it.
7. At zero the bomb goes off and the game is over. The timer never shows a negative value, and a wrong answer that takes it to zero or below ends the game at once.
8. On level complete, whole seconds left convert to points and the next level starts with a fresh timer.
9. Clearing level 12 wins the game, pays 1,000 coins once, and offers endless mode (level 13 onward).

A new game always starts at level 1. Coins are kept when the bomb goes off.

## Levels 1 to 12

Difficulty moves on three dials: number size, operation mix and question shape. The PRD ladder changes one dial per step.

| Level | Shape | Operations | Largest number |
|---|---|---|---|
| 1 | a + b | Add | 5 |
| 2 | a − b | Take away | 5 |
| 3 | a ± b | Mixed | 10 |
| 4 | a ± b | Mixed | 20 |
| 5 | a + ? = c | Add | 20 |
| 6 | a + b + c | Add | 20 |
| 7 | a ± b | Mixed | 50 |
| 8 | a + b − c | Mixed | 50 |
| 9 | a ± b | Mixed | 100 |
| 10 | a + b + c − d | Mixed | 100 |
| 11 | a ± b | Mixed | 500 |
| 12 | a ± b | Mixed | 1,000 |

## Endless mode

Endless levels keep level 12's largest number (1,000) and draw shapes from the full set. Each level still has 10 questions. Let k be the endless level number (level 13 is k = 1).

| Quantity | Rule |
|---|---|
| Level time | `max(60 − 2k, 30)` seconds |
| Time per right answer | `max(7 − floor((k − 1) / 3), 3)` seconds |
| Share of three- and four-term questions | `min(40 + (k − 1) × 60 / 7, 100)` percent, integer arithmetic |
| Wrong answer | still −3 s |
| Alert zone | still the last 10 s |

| Level | Level time | Per right answer | Three- and four-term | Alert share of fuse |
|---|---|---|---|---|
| 13 | 58 s | +7 s | 40% | 17% |
| 16 | 52 s | +6 s | about 65% | 19% |
| 20 | 44 s | +5 s | 100% | 23% |
| 25 | 34 s | +3 s | 100% | 29% |
| 27 and up | 30 s | +3 s | 100% | 33% |

From level 25 a right answer adds only what a wrong one removes, so every game ends. Scoring is unchanged. Endless levels count towards the level reached and the top 10.

## Question generation

- Every operand is between 1 and the level's largest number. Every intermediate result and the answer is between 0 and the largest number. (The PRD allowed 0 as an operand. Decision 4 makes the minimum 1.)
- Wrong tiles are near misses: answer ±1, ±2, ±10, or a digit swap. They are distinct, in range, and different from the answer.
- The number of wrong tiles below the answer (none, one, two or three) is chosen with equal chance from the numbers the range allows, and the rest are above it. So the answer is as likely to be the smallest or the largest tile as a middle one. A side with too few near misses in range is topped up with the nearest unused numbers on that side.
- The correct tile's position is random. No question repeats within a level.
- Generation is seeded, so any level can be replayed exactly.
- Every possible question of a kind (a + b, a − b, a + ? = c and so on) has the same chance of being dealt. On a level that mixes kinds the choice between kinds is even, and endless keeps its own split.
- Build operands from the result backwards so no retry loop is needed: choose the result with a chance in proportion to the number of ways to split it, then choose one of those ways with equal chance.
- For a level with 50 or fewer possible questions, list them all, shuffle and deal 10. Level 1 has exactly 10 and level 2 has 15.

## Points and coins

| Event | Points | Coins |
|---|---|---|
| Correct answer | 10 × multiplier | 1 |
| Every 5th correct in a row | none | 1 bonus |
| Wrong answer | 0 | 0 |
| Perfect level (10 of 10) | 100 bonus | 5 |
| Level complete | 10 per whole second left | none |
| Win (clear level 12), once | none | 1,000 |
| Hint | none | costs 2 |

Coins are saved in a wallet on the device, kept when the bomb goes off, and spent only on hints. There are no purchases and coins have no money value. Diamonds are parked.

### Multiplier

The session keeps two numbers: the run (correct answers in a row) and the step (×1, ×2 or ×3).

- A run of 3 sets the step to at least ×2. A run of 6 sets it to ×3, the cap.
- A wrong answer sets the run to 0 and lowers the step by one, never below ×1. After a drop the ladder climbs again at 3 and at 6 in a row.
- Every fifth answer of a run pays a bonus coin.
- The run and step belong to the game, so they carry across levels and reset on a new game.

### Hints

A hint costs 2 coins and removes two of the three wrong tiles, leaving the right answer and one wrong tile. The session generator chooses which. One hint per question. The button is disabled when the wallet holds fewer than 2 coins.

## Leaderboard

The device keeps a top 10 of final scores. A final score is the total for one game, from level 1 until the game ends, including endless levels.

- Best score is the top entry. It shows on the title screen and the game-over card, where a new best reads NEW BEST!
- A score qualifies if the list has fewer than 10 entries or the score beats the 10th. A score of 0 never qualifies. On equal scores the older entry ranks first.
- Name entry: a name screen shows the rank and score, a text field, SAVE and SKIP. A name is up to 12 characters, A to Z and space, shown in capitals. SAVE needs at least one character. SKIP drops the entry.
- The Top 10 screen shows rank, name and score, with a new entry highlighted. It opens from Title and from Game over.
- The list and the names stay on the device unless a grown-up turns sync on (see Sync between a family's devices). They never reach the developer or anyone outside the family's own iCloud account.
- With players (see Players), an entry takes the current player's name, and the name screen appears only for a player who has no name yet.

## Players

Decided on 15 Nov 2026 (`DECISIONS.md`, "After v1"). The PRD follows.

- The save holds up to four players (`max_players`). A player has a name, a wallet, the win-bonus mark and a best score. A name follows the top 10 name rules and may be empty until it is first typed.
- One player is the current player. A game is played as that player: its coins, the win bonus and its best score belong to that player. The best score on the title and the game-over card is the current player's.
- A first run has one player with no name. A save from before players existed becomes that one player, with its coins and win-bonus mark.
- The title shows who is playing. WHO'S PLAYING lists the players, picks one and adds one. Anyone may add a player while there is room. Removing a player needs the parental gate.
- The top 10 is one list shared by all players. An entry takes the player's name. A player with no name gets the name screen once: SAVE names the player and saves the entry, SKIP drops the entry and leaves the player unnamed.
- The saved run belongs to the player who paused it. CONTINUE is offered only while that player is the current one.
- When a run resumes, its wallet is the player's wallet as it is now, not as it was at the pause.

## Sync between a family's devices

Decided on 15 Nov 2026. Sync uses the iCloud key-value store of the Apple Account signed in on the device. The game has no account, sign-in or server of its own, and nothing reaches the developer or anyone outside that account.

- A Settings switch on Apple devices, off by default, behind the parental gate. With the switch off, or iCloud unavailable, the game behaves exactly as it does without sync. Other platforms have no sync.
- The players and the top 10 sync. The Settings switches, the saved run and the choice of current player stay on each device.
- Two devices that were played apart are merged, and nothing a player earned or was given is lost:

| Data | Merge rule |
|---|---|
| Players | Both sets together, matched by player id. A removed player stays removed |
| Name | The later change wins, by a change counter and not a clock |
| Coins | Each device counts what was earned and what was spent on it. The merge keeps the larger count for each device. The wallet is earned less spent, never below 0 |
| Win-bonus mark | Set if either has it |
| Best score | The larger |
| Top 10 | Both lists together without duplicates, best ten kept |

The merge reads no clock. Merging in either order gives the same result, and merging the same data again changes nothing.

## Ending a game

| How | What happens |
|---|---|
| Bomb goes off | Game over. Score can enter the top 10. |
| QUIT on the pause screen | Treated as a game over. Score can enter the top 10. Coins stay. |
| FINISH on the win screen | Same as QUIT (board CP8). |
| KEEP GOING on the win screen | Continues into level 13. |

## Interruptions and the saved run

The same rules apply on every platform.

- When the app loses focus or goes to the background, it pauses at once and the bomb timer stops. A screen-time limit, a lock screen, a call, an app switch and a desktop window losing focus all count. In a browser, a hidden tab counts.
- Pausing writes the wallet, settings, top 10 and the current run (level, points, run and step, ticks left, current question and tile order, questions already asked this level, hint flag, random generator state).
- On return the game shows the pause screen with RESUME and QUIT. The clock stays stopped until RESUME.
- The title screen shows CONTINUE beside NEW GAME when a run is saved. A saved run resumes once, and is deleted as soon as it loads.
- The game does not read, detect or try to extend system limits. It has no ask-for-more-time button.

## No pressure to keep playing

No notifications or notification permission. No daily rewards, login streaks or real-world timers. No ads, purchases or store. The game-over card offers PLAY AGAIN and HOME with equal weight and no guilt wording. A level takes about a minute.

## Feedback cues

No cue depends on colour alone. Every sound has a visual twin, so a muted game plays the same.

| Event | Visual | Sound |
|---|---|---|
| Correct answer | Tile turns green, pops into the `?` slot, +points floats up | Coin chime |
| Wrong answer | Tile turns red and shakes, correct tile pulses green for 1 s, −3 s flashes on the timer | Soft buzz |
| Streak step (×2, ×3) | Flame badge grows, short screen flash | Rising tone |
| Last 10 s alert | Timer and fuse turn red, fuse sparks, play area edge pulses | Ticking |
| Bomb goes boom | Cartoon puff, screen shake, then the game-over card | Cartoon pop, nothing frightening |
| Level complete | Leftover seconds count up into points | Fanfare |
| Hint used | Removed tiles fade and drop away | Whoosh |

Sound and music assets must be original or licensed for this use, and each one is listed in the notices.

## Settings and About

Settings opens from a gear button on Title and in the pause menu. Values are saved at once.

| Setting | Default | Notes |
|---|---|---|
| Sound effects | On | Chimes, buzzes, ticking, boom, fanfare |
| Music | On | Separate from sound effects |
| Haptics | On | Only where the device can make them (a phone, a browser on a touch screen). A little touch feedback for answers and presses, never the only cue. See `haptics.md` |
| Game Center | Off | Apple devices only. Needs the parental gate to turn on |
| iCloud sync | Off | Apple devices only. Needs the parental gate to turn on. See Sync between a family's devices |

About shows version and build number, short commit hash, branch or tag and build date ("dev" for local builds), the game's own licence (GPL-3.0-or-later), third-party notices generated at build time, the font licences for Bangers and Lilita One, and a privacy statement with a link to the privacy policy. Every link that leaves the app opens behind the parental gate, where an adult types back a number written in words.

## Platforms and layout

v1 runs on iPhone, iPad, Android, WebAssembly in a browser, and native macOS, Windows and Linux. Phones are portrait only. The game core is identical everywhere. Each platform supplies screen, input, sound, storage and score service. The ESP32-P4 handheld is v2.

- Four answer tiles in a 2 × 2 grid at the bottom of a phone, in the bottom third of the screen, each at least 96 pt high. iPad scales the same layout up.
- On desktop and in a browser the stats move to a left panel and the four tiles sit in one row.
- Two size classes: compact (phone portrait) and regular (tablet and desktop). Boards CP9 and CP10 show regular.

## Parked or out of v1

Diamonds, the party cat, the bonus animal, Say and Write input, skip-ahead for returning players, an online leaderboard, accounts, ads, purchases, notifications, Play Games Services on Android, and the ESP32-P4 handheld.

## Open questions from the PRD

| Question | Working answer |
|---|---|
| Is Game Center allowed at the target age rating? | Assume yes. Confirm against Apple's Kids Category rules before submission. If not, remove score submission. |
| Is the endless ramp right? | Playtest (M6) |
| Is the 7 s and 3 s balance right for a 5-year-old on level 1? | Playtest (M6) |
| Skip-ahead for returning players | Later. No design. |
