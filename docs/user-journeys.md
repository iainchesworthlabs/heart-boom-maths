# User journeys and lifecycle

This page maps every screen of the game as built, every way between them, what is saved at each step, and what happens when the app is interrupted. It was written from the code (`AppShell.qml`, `game_controller.*`, `answer_feedback.*`, `tick_driver.*`, `parental_gate.*`, `main.cpp` and the session states in `libs/core`) and checked against [`SPEC.md`](SPEC.md), [`game-architecture.md`](game-architecture.md) and [`COMPLIANCE.md`](COMPLIANCE.md). The journeys are walked by the `ui` tests in `apps/game/tests/test_ui_journeys.cpp`, which assert the screen after every step.

Vocabulary: the **state** is the core session's state as the controller shows it (Idle, Playing, Paused, LevelClear, Won, GameOver). The **screen** is what `AppShell.qml` shows for it. Two things sit outside the state: a **page** (Settings, About, Top 10, WHO'S PLAYING and the name screen of a player, stacked over the screen) and the **parental gate** (over everything while the controller has one open).

## Open decisions

These are cases the documents do not settle. Each needs an answer from the project owner and the game's designer. None is changed in code.

| # | Situation | Options | Recommendation |
|---|---|---|---|
| D1 | The app is killed (not just sent to the background) while the level-clear card or the win screen is up. Leaving the foreground there writes the wallet and the win mark, but no run: the core only pauses from Playing, so the next launch starts on the title and the levels cleared so far, and the score they earned, are gone. SPEC says pausing writes the run and does not mention these two cards. | A. Keep it: the loss is one game. B. Also write a snapshot while a card is up. The core already restores a level-clear and a win snapshot, so CONTINUE could return to the card (the level-clear card already copes with missing sums, `complete` is false). C. Write a snapshot of the start of the next level. | B. A card is on screen for a good part of a game, and a screen-time lock is exactly when a kill is likely. It needs a line in SPEC (a saved run may be a card as well as a paused level) and a decision about the caption under CONTINUE. |
| D2 | The app is killed on the name screen. The score that earned a place is in memory only until SAVE, so it is lost. A background and return keeps it. | A. Keep it. B. Write the pending score into the save at game over, and offer the name screen again on the next launch (a new save field). C. Save it under a default name. | A for v1. B adds a schema field and a new first screen on launch for a small, rare loss. Revisit if playtests show it. |
| D3 | A button on a card screen sits over the answer tiles of the next screen, and a second tap on it (a double tap) lands on a tile. On a 390 by 844 phone (measured in the `ui` tests' offscreen window), PLAY AGAIN spans y 579 to 651 and its centre is inside the tile grid of the next game (y 600 to 808); NEXT LEVEL spans y 561 to 633. RESUME and the other buttons low on a screen should be measured too. The second tap is an answer the player did not mean, which costs 3 seconds and the multiplier step. | A. Do nothing. B. The tiles ignore a tap for about a third of a second after a level starts or the pause screen closes. It is an input guard in the app layer and no rule changes. | B, with a short guard (a presentation constant next to the hold times). Existing `ui` tests that tap a tile straight after `startGame()` would wait for it. |
| D4 | NEW GAME on the title, and PLAY from the Top 10 while a saved run waits, drop that run with one tap and no question. QUIT on the pause screen ends the game with one tap. SPEC has no confirmation for either. | A. Keep it. B. Ask first (a new screen or card). | A until a playtest shows it going wrong: a confirmation is a new screen and a design decision. The labels were made consistent (see below). |
| D5 | The back key on the cards. It is implemented as: play pauses, pause resumes, Top 10 goes back, game over goes home, and the level-clear, win and name screens ignore it so a stray press cannot close the app or drop a score. SPEC is silent. | A. As built. B. Name entry's back key means SKIP. C. Let the system close the app everywhere. | A. Confirm it is what the designer wants. |
| D6 | Decided: SPEC says the saved run is "deleted as soon as it loads". The persistence table in `game-architecture.md` now says the same. The code already followed SPEC: CONTINUE deletes the run, and leaving the app again writes it again. A crash on the restored pause screen, before the app leaves the foreground, loses it. | Settled | Settled |
| D7 | Coins earned in a level are written at the next pause, level clear or game over. A hard kill with no warning (battery, crash) loses the coins of the level being played and any hint spent in it. SPEC lists those save moments, so this is as specified. | A. Keep it. B. Also write after each answer. | A. |
| D8 | The device keeps one saved run (SPEC, Players). QUIT is reached through the pause screen, and pausing saves a run for the player now playing, which replaces another player's run. So when a second player quits a game, the first player's saved run is gone, though the game that player paused was never touched. A bomb or a game that is never paused leaves it alone. | A. As built: the rule is the one in SPEC, and the run is replaced when the device has to save one for the player now playing. B. Keep a run for each player (a save change). | A for now. B needs a schema change and is the owner's call. |
| D9 | A removed player stays in the list as a mark, so a merge cannot bring the player back, and the list holds 64 of them. A family that adds and removes players many times (the 64th place) can never add another: ADD PLAYER is hidden as if the list were full. | A. Keep it. B. Let a build forget removed players that every device has seen (a sync rule). | A: it takes about 60 add and remove cycles to meet. |

## Gaps closed in this pass

| Gap | Fix | Test |
|---|---|---|
| The Android back key did nothing on About (the keyboard sat on the root item, the frame that handles the key is its child), and on the play, pause, Top 10, level-clear, win, name and game-over screens it was not handled at all, so the system closed the app. | `AppShell.qml` handles the back key on the screens above (see [Keys](#keys)). About is a focus scope that passes the keyboard to its frame, or to the licence text while one is open. | `Keys: ...` in `test_ui_journeys.cpp` |
| A run in the save file that is not a running or paused level (a level-clear card, a win, a finished game) was restored by CONTINUE, which showed a card with no numbers on it. | `continueGame()` and the caption both check a scratch session first. Such a run counts as one that cannot be restored: the run is spent and the title says so calmly. | `A saved run that is not a paused level is refused and spent` |
| The parental gate stayed open when the app left the foreground, so whoever picked the device up next found it open. | The controller closes the gate when the app leaves the Active state. | `The parental gate is closed when the app leaves the foreground`, and the Settings journey |
| Pausing inside the second a wrong answer is held, then QUIT, left the play screen up until the hold ran out. | `AnswerFeedback` ends the hold on any game over that is not the bomb. | `QUIT during the hold of an answer ends the hold at once` and the hold case of the lifecycle test |
| The Top 10 opened from the title said PLAY AGAIN after HOME on the game-over card, and PLAY beside a saved run, which starting a game discards. | The button says PLAY AGAIN straight after a game, NEW GAME while a saved run waits, and PLAY otherwise. | `Journey: pause, Settings from the pause screen, ...` and `Journey: a score that does not rank, ...` |

## Screens

Every state of the core has exactly one screen, and every screen can be left by a button in the window. A page or the gate may sit over any of them.

| Screen | Shown when | Leaves by |
|---|---|---|
| Title | state Idle, or GameOver after HOME | PLAY or NEW GAME (starts level 1), CONTINUE (restores the saved run to Paused, only while it is the current player's), TOP 10, the gear and the player card (pages) |
| Play | Playing, Paused (with the pause overlay), and for a hold after the last answer of a level or the bomb | the pause button, the last answer (level clear, win), the bomb (game over) |
| Pause overlay | Paused | RESUME, QUIT, the gear (Settings page) |
| Level clear | LevelClear, once the hold of the last answer has ended | NEXT LEVEL |
| Win | Won, once the hold has ended | KEEP GOING (level 13), FINISH (game over) |
| Name entry | GameOver when the score earned a place and the player has no name | SAVE (names the player; the Top 10 page, then the game-over card), SKIP (the game-over card) |
| Game over | GameOver | PLAY AGAIN, TOP 10 (page), HOME |
| Top 10 (page) | opened from Title, the game-over card, or SAVE | the back arrow, PLAY or PLAY AGAIN or NEW GAME |
| Settings (page) | from the title's gear or the pause overlay's gear | the back arrow; ABOUT |
| About (page) | from Settings | the back arrow (to Settings); the licence and notices texts open over it and close with their own back arrow |
| WHO'S PLAYING (page) | from the player card on the title | the back arrow; a player's card (makes that player current); NAME or RENAME (the player's name screen); ADD PLAYER (the name screen of the new player); REMOVE (the gate) |
| Player name (page) | from WHO'S PLAYING | SAVE (gives the name), SKIP for a player just added or CANCEL for a rename, Escape and the back key (the same as SKIP or CANCEL); each goes back to WHO'S PLAYING |
| Parental gate | the controller has one open: About's two links, the Game Center switch, the iCloud sync switch, REMOVE on WHO'S PLAYING | CANCEL, the back arrow, or the right number |

## State and transition table

The controller states, the trigger of each move, and what is written to the save at the moment of the move. "Disk" means `save.json` through `SaveManager`; nothing is written while the save is blocked by a newer file.

| From | Trigger | To | Disk |
|---|---|---|---|
| (launch) | the controller reads the save | Idle | nothing is written until the first action |
| Idle, GameOver | PLAY, NEW GAME, PLAY AGAIN | Playing, level 1 | wallet and settings; the current player's own saved run is dropped, another player's stays |
| Idle | CONTINUE with a restorable run of the current player | Paused | the run is deleted at once; the wallet is the player's as it is now |
| Idle | CONTINUE that cannot restore | Idle | the run is deleted; a calm line on the title |
| Playing | pause button, Escape, back key, the app leaving the foreground | Paused | wallet, settings, top 10 and the run |
| Paused | RESUME, Escape, back key | Playing | the run is deleted |
| Paused | QUIT | GameOver (reason Quit) | wallet; the player's run is deleted |
| Playing | the last of the 10 answers, level below 12 | LevelClear | wallet |
| Playing | the last answer of level 12 | Won | wallet and `won_once` (the bonus is paid once) |
| Playing | time reaches 0, or a wrong answer takes it to 0 | GameOver (reason Bomb) | wallet; no run |
| LevelClear | NEXT LEVEL | Playing, next level | none |
| Won | KEEP GOING | Playing, level 13 | none |
| Won | FINISH | GameOver (reason Finish) | wallet |
| GameOver | SAVE on the name screen | GameOver | top 10, the player's name |
| GameOver | SKIP | GameOver | none; the score is not in the list and the player stays unnamed |
| Playing, Paused, Won | the game ends with a place in the top 10, for a player who has a name | GameOver | top 10 at once, under the name; no name screen; the Top 10 page opens next |
| Idle, GameOver | a player on WHO'S PLAYING | same | the old player's wallet, then the choice of current player |
| Idle, GameOver | ADD PLAYER | same | the new player, who is current |
| any | SAVE on a player's name screen | same | the name |
| Idle, GameOver | REMOVE and the right number | same | the player is marked removed; that player's saved run is deleted |
| GameOver | HOME | GameOver, title overlay | none |
| any | a setting changes | same | the setting |
| Idle, Paused, GameOver with no name waiting | iCloud sync is on and a trigger arrives (the switch turned on, start-up, the app active, the store reporting a change, a save) | same | the cloud copy, and when the other devices had more, the local save and the player's wallet |
| Playing, LevelClear, Won, GameOver with a name waiting | the same trigger | same | nothing: a sync is owed and runs at the next moment above |
| any | the app leaves the foreground | same, or Paused from Playing | everything above, as in the lifecycle table |

Every game over, whatever the reason, submits the final score to the score service once (a no-op unless Game Center is on and available) and offers the name screen when the score qualifies, as SPEC says for QUIT and FINISH.

## Journeys

Each step names the screen and the controller state, then what is saved. The `ui` test of the same name asserts them.

### 1. A new player's first game

Test: `Journey: a new player plays a first game, uses a hint, loses, and is ranked`.

1. First launch, no save file. Screen Title, state Idle, wallet 0, best score 0, PLAY (no CONTINUE). Nothing is on disk.
2. PLAY. Play, Playing, level 1, question 1 of 10. The defaults are written (wallet 0, no run).
3. With 0 or 1 coin the hint button is off. After the second right answer the wallet is 2, exactly the price, and it is on.
4. HINT. The wallet is 0, two wrong tiles are gone, and the button is off again. A tap on a disabled hint does nothing, so the wallet never goes below 0.
5. A wrong tile. It turns red, the right one turns green, and the multiplier step drops. A second tap on the same tile, or on another one, while the look is held, does nothing (the tiles are not tappable while held).
6. The tenth answer. The play screen stays up for the hold, then Level clear, state LevelClear. The wallet is written.
7. NEXT LEVEL. Play, level 2, a fresh timer.
8. The timer reaches 0. The heart pops on the play screen for its hold, then, because the score earned a place, Name entry (state GameOver). The wallet is written; the list is not changed yet.
9. A name and SAVE. The Top 10 page with the new row marked, state GameOver. The list is written. Back goes to the game-over card, which shows NEW BEST.
10. PLAY AGAIN. Play, level 1, 0 points. The wallet kept its coins.

### 2. Dying and ranking

Test: `Journey: a score that does not rank, then one that does, then SKIP and SAVE`.

A save with a full list. A score of 0 goes straight to the game-over card. TOP 10 from the card shows the list unchanged, nothing marked, with PLAY AGAIN. HOME shows the title with the game still over underneath, and PLAY there starts a new game. A score that ties the seventh place ranks below the older entry (8th). SKIP lands on the card with no NEW BEST and the list unchanged. SAVE shows the list with the new row marked and the tenth entry gone. From the title after HOME, the list marks nothing.

### 3. Winning level 12 and going on

Test: `Journey: clearing level 12 pays the bonus once, KEEP GOING into endless, QUIT, and a second win`.

1. Level 12 is cleared. Win, state Won. The card says +1,000 COINS. The wallet and `won_once` are written.
2. KEEP GOING. Play, level 13, the endless badge. QUIT from the pause screen ends the game as a game over with no BOOM (the run on disk is removed), then the name screen, SAVE, the list, the card.
3. PLAY AGAIN and a second win in the same install. The win card says BONUS WON BEFORE and shows no payment, and the wallet rises by less than 1,000.
4. FINISH ends the game like QUIT. After a restart the third win pays nothing either.

### 4. Pause, leave, be killed, continue

Test: `Journey: pause, Settings from the pause screen, leave the app, be killed, then CONTINUE`.

1. The pause button. Play with the pause overlay, state Paused, the clock stopped. The run is written.
2. The gear on the overlay. Settings opens over the pause screen. The run stays paused. ABOUT and back, back returns to the pause overlay.
3. RESUME. Playing. The run is deleted. The app leaves the foreground: Paused at once, the audio is suspended and the run is written. Time passing while the app is away counts for nothing.
4. The app returns. Still the pause overlay, still stopped.
5. The app is killed and started again. Title with CONTINUE beside NEW GAME and the caption "Saved game: level 1, ... left". The Top 10 button there says NEW GAME.
6. CONTINUE. The pause overlay with the same level, points, time and question. The run is deleted from the save. RESUME plays on from the same tick.
7. The app leaves the foreground again and is started again. NEW GAME starts level 1 and drops the run.

Also covered by `Journey: a saved run that cannot be opened, and a save from a newer version`: a run the core refuses leaves the title calm with PLAY working and the wallet kept; a save with a higher version is played on defaults, nothing of its run is offered, and the file is byte-for-byte the same after a whole session.

### 5. Settings, the gate and About

Test: `Journey: Settings switches, the gate for Game Center and for a link, and a restart`.

1. The gear. Settings page. Sound and Music off: each is applied to the audio service and written at once.
   Haptics (shown only where the device can make them) works the same way, and turning it on gives one light tick (`tests/test_ui_haptics.cpp`, [Haptics](haptics.md)).
2. Game Center (shown only where the platform has a leaderboard). The switch opens the gate. A wrong number deals a new one. CANCEL leaves it off and writes nothing. The right number and OPEN turn it on, tell the score service, and write it.
3. ABOUT, then a link. The gate opens. Escape closes it and opens nothing. A wrong number opens nothing. The right number opens the address once through `LinkOpener`, and the gate is spent. While the gate is open the page under it takes no taps.
4. A gate left open when the app leaves the foreground is closed, and nothing opens when the app returns.
5. Back out through Settings to the title.
6. After a restart both switches are as they were left, and the services are told before the first screen shows.

### 6. A family: who is playing

Tests: `Journey: the chip on the title opens WHO'S PLAYING; add, switch, rename and remove players`, `Journey: the title, its CONTINUE and its numbers follow the player who is chosen`, `Journey: a named player's high score goes straight to the Top 10 page` and the `players` cases of `test_game_controller.cpp`.

1. The title shows one card across the top: the player's name (PLAYER 1 for a player with no name, by place in the list) with a down chevron, and under it their best score and their coins. The whole card is a button and opens WHO'S PLAYING.
2. The page lists the players with the best score and the coins. The one playing has a tick and PLAYING. With one player there is no REMOVE; with four there is no ADD PLAYER.
3. ADD PLAYER makes a new player current at once and opens the name screen: SAVE names them, SKIP leaves them unnamed. A card makes that player current; the wallet is recorded for the old player first, and the title's best score, coins and CONTINUE are the new player's.
4. NAME or RENAME opens the same name screen with the name they have. CANCEL leaves it.
5. REMOVE asks a grown-up (the gate: ASK A GROWN-UP, the number in words, REMOVE). A wrong number or CANCEL removes no one. The player and any run that player paused go.
6. The saved run stays with the player who paused it. CONTINUE and its caption show only for that player. Another player's new game, or game over, leaves it; their own pause replaces it (D8).
7. Players are switched from the title only. While a game is played, paused, on a card, on the win screen or waiting for a name, the controller refuses; the card is not on those screens.
8. A player with a name who earns a place in the top 10 sees no name screen: the entry is saved at once under their name and the Top 10 page opens with it marked, then the game-over card. A player with no name is asked once, and SAVE names the player. Every finished game counts towards the player's best, so NEW BEST! on the card means a better score than that player's own best.

### 7. iCloud sync between a family's devices

Tests: `test_cloud_controller.cpp` (the controller), `test_cloud_sync.cpp` (the rules) and `test_ui_cloud.cpp` (the row and the gate). Only on an Apple device with an iCloud account; elsewhere there is no row and nothing below happens.

1. Settings shows iCloud sync under Game Center (and under Haptics), OFF. Nothing has been read, written or listened to: a default install never contacts iCloud.
2. The switch opens the gate (ASK A GROWN-UP, "To turn on iCloud sync", TURN ON). A wrong number or CANCEL leaves it off and the store untouched. The right number turns it on, and the first sync runs: the players and the top 10 go to the family's iCloud if it holds nothing, or are joined with what another device put there.
3. While it is on, a change another device made reaches this one when the store says so, when the app comes back, and at each save. It is taken only on the title or the pause screen. During play, on the level-clear card, on the win screen or while a name is waiting, it waits and is taken at the next such moment, so a level never changes the coins under the player. The title, WHO'S PLAYING and the top 10 then show the other device's players, names, coins and scores. If the player here was removed on the other device, another player becomes current.
4. Coins earned on two devices are added, not replaced: a player who earned 50 on the phone and 30 on the tablet has 80 on both.
5. The switch turns off at once, with no gate. The copy in iCloud stays, and nothing more is read or written until it is turned on again (with the gate).
6. If the person signs out of iCloud, the row goes away and sync stops; the choice is kept, and sync resumes when an account returns. An iCloud copy written by a later build of the game is left alone.

## Leaving the foreground

Tests: the `Lifecycle:` cases in `test_ui_journeys.cpp`, and `Leaving the foreground in every state keeps what the save promises` in `test_game_controller.cpp`. The app calls `GameController::on_application_state(false)` when Qt reports a state other than Active (a lock screen, a call, an app switch, a window losing focus, a hidden browser tab). It pauses a running game, stops the tick driver and the ticking sound, suspends audio, closes the gate, and writes the save. Coming back resumes audio only.

| Showing | State | After leaving and returning | On disk |
|---|---|---|---|
| Title | Idle | the title | defaults or the current save |
| Play | Playing | Paused, the pause overlay, the clock stopped | run, wallet |
| Pause overlay | Paused | the same | run, wallet |
| Level clear | LevelClear | the same card | wallet; no run (D1) |
| Win | Won | the same screen | wallet, `won_once`; no run (D1) |
| Name entry | GameOver | the same screen, nothing typed is lost | wallet; the score is not in the list (D2) |
| Game over | GameOver | the same card | wallet |
| Settings, About, Top 10 | as underneath | the same page | as underneath |
| Gate open | as underneath | the page, with the gate closed | as underneath |
| A held right or wrong look | Playing | the pause overlay; the hold ends by itself and RESUME finds the tiles live | run, wallet |
| The last answer's hold (a card is next) | LevelClear or Won | the card, after the hold | wallet |
| The bomb's hold | GameOver | the name screen or the card, after the hold | wallet |

The ticking sound stops when the game pauses, when it leaves the alert zone and when the app leaves the foreground, and starts again on RESUME if the game is still in the alert zone.

In the alert zone a red frame pulses round the window, and a white flash crosses it when the multiplier steps up. Both are drawn over the whole window, edge to edge under a phone's cut-out and home indicator (`AlertFrame`, laid over the safe area by `AppRoot`); the play screen's content stays inside the safe area. On a phone each side of the frame is its 8 pixel line plus the margin the device keeps clear there, so it still reads at the rounded corners. Pausing ends the frame. Tests: `The alert frame and the streak flash cover the whole window while the play screen stays in the safe area`.

## When the save is written

| Moment | What |
|---|---|
| PLAY, NEW GAME, PLAY AGAIN | wallet, settings, top 10; a waiting run is removed |
| Pause (the button or leaving the foreground) | the run, with the wallet, settings and top 10 |
| RESUME | the run is removed |
| CONTINUE | the run is removed as soon as it is read, restorable or not |
| Level clear | wallet |
| Level 12 cleared | wallet and `won_once` |
| A switch of player | the old player's wallet, then the choice of current player |
| ADD PLAYER, a player's name, REMOVE with the right number | the player list (and the run, when its player is removed) |
| Game over (bomb, QUIT or FINISH) | wallet and the player's best; the player's own run is removed |
| SAVE on the name screen | top 10, and the player's name |
| A game that ends with a place in the top 10, for a named player | top 10 and the player's best, with the wallet |
| A setting changes, or the gate passes for Game Center or iCloud sync | the setting |
| An iCloud sync that changed the local data | the player list, the wallets and the top 10 (the device's own settings, run and current player stay) |
| Any other time the app leaves the foreground | everything that is current |

Writes are atomic (a temporary file renamed over the old one). A damaged file is moved to `save.json.bad` before the first write and the game starts on defaults. A file with a higher version is read as defaults, no save is written for the whole session, and the file is never touched (`A save from a newer version is never overwritten`, and the newer-save journey above). A saved run is never offered from either kind of file.

## Keys

The Escape key and the Android back key. A page or the gate always takes the key before the screen under it.

| Where | Escape | Back key |
|---|---|---|
| Title | nothing | the system's own (closes the app) |
| Play | pause | pause |
| Pause overlay | RESUME | RESUME |
| Level clear, Win, Name entry | nothing | nothing (see D5) |
| Game over | nothing | HOME |
| Top 10 | back | back |
| WHO'S PLAYING | one step back | one step back |
| Name screen of a player | SKIP or CANCEL | SKIP or CANCEL |
| Settings, About, a licence text | one step back | one step back |
| Parental gate | CANCEL | CANCEL |

Test: `Keys: the Android back key and Escape have a way back on every screen`. The back key is tested as a `Key_Back` event on the offscreen platform; it has not been tried on an Android device.

## Cases checked

OK means the code does it and a test covers it. The tests are in `apps/game/tests`.

| Case | Result | Where |
|---|---|---|
| First launch, no save | OK | `A fresh controller shows the title screen on defaults`; journey 1 |
| Launch with a top 10 and a wallet | OK | `The settings and the wallet in the save reach the game`; journey 2 |
| A full level, wrong answers, the alert zone | OK | `test_ui_play.cpp`, journey 1, `Leaving the alert zone stops the ticking...` |
| A hint with enough coins, with too few, at exactly 2 | OK | journey 1; `A hint costs coins, removes tiles and whooshes` |
| The bomb, then the name screen, qualifying or not, then game over, PLAY AGAIN, HOME, TOP 10 | OK | journeys 1 and 2 |
| Level clear, then NEXT LEVEL | OK | journey 1; `The level-clear card shows the numbers the core gave...` |
| Level 12: bonus once, `won_once` saved, KEEP GOING, FINISH | OK | journey 3; `Clearing level 12 wins, pays once...` |
| Endless to game over | OK | journey 3; `Winning level 12 offers KEEP GOING...` |
| Pause, RESUME, QUIT, Settings from the pause overlay and back | OK | journey 4; `The pause screen opens Settings and goes back...` |
| Leaving the foreground in every state | OK, with D1 and D2 | `Lifecycle:` cases |
| Killed while paused, CONTINUE, a failed restore, NEW GAME | OK | journey 4; `CONTINUE restores the paused run once` |
| A damaged or newer save file | OK | `A damaged save is moved aside...`, `A save from a newer version is never overwritten`, the newer-save journey |
| A run that is not a paused level | fixed | `A saved run that is not a paused level is refused and spent` |
| Killed on the name screen, on level clear, on win | decision | D2, D1 |
| The Top 10 from the title, from game over, after SAVE | OK | journeys 1 and 2; `The top 10 opens from the title and from the game-over card...` |
| Settings saved and applied; Game Center only where available and behind the gate | OK | journey 5; `Game Center turns on only behind the gate and off at once` |
| iCloud sync only where available, behind the gate, off at once, no store use while off | OK | journey 7; `Turning iCloud sync on goes through the parental gate and leaves it on`, `The switch is off by default, and the store is not touched until it is on` |
| A change from another device during play, on a card, or while a name waits | OK: owed, taken at the next title or pause | journey 7 step 3; `A trigger during play only marks a sync as owed...` |
| About links: cancel, wrong number, right number | OK | journey 5; `Opening a link never reaches the link opener until the gate is passed...` |
| The gate when the app leaves the foreground | fixed | journey 5 step 4 |
| Escape and back on every screen | fixed, with D5 | `Keys:` cases |
| Resize and rotation in every state | OK | `Lifecycle: changing the window's shape in every state...`; phones are locked to portrait, so rotation means a resize on a tablet or a desktop |
| Two quick taps on a tile | OK (the held look is not tappable) | journey 1 step 5 |
| Two quick taps on a button, or on a button over the tiles | decision | D3. A repeated PLAY, NEXT LEVEL, KEEP GOING, FINISH, RESUME or SAVE is ignored by the core's state check |
| Taps while a page or the gate is open | OK (the screen under is hidden and the page under the gate is disabled) | journey 5 step 3 |
| Coins never negative | OK | journey 1 step 4; the core's property tests |
| Wallet and top 10 after QUIT or FINISH | OK | `The wallet, settings and top 10 survive a restart through the file`; journey 3 |
| QUIT and FINISH: no BOOM, score ranked | OK | `QUIT on the pause screen ends the game and offers the name entry once`; journey 3 |
| Audio and the ticking stop when paused and when the app leaves | OK | `Lifecycle: the ticking and the clock stop...` |
| A page or the gate left behind by a state change | OK | `A change of the game's state closes any page`; the gate is also closed on leaving the foreground |
| Locale | no runtime switch exists: every string goes through `qsTr`, and a language change needs a restart | |

## Not verified here

Everything above runs offscreen with a manual clock. The iCloud store itself (the Apple calls) was not run: it is the one part that needs two Apple devices ([iOS](ios.md)). Not tried: Qt's application-state events on a real iOS, Android or browser build (the release checklist covers them), the Android back key on a device, Windows and Linux parental controls, and a hard kill of the real process (the restart in the tests is a second controller on the first one's save text).
