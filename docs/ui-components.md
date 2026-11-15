# UI components

The screens are QML in `apps/game/qml/`, one static QML module (`HeartBoomMaths`) built into the `heart_boom_maths_app`
library, so the program and the `ui` tests load the same files. The controller is the module's one C++ type that screens
read (`GameController`, uncreatable); a screen declares it as `required property GameController game` and the shell hands
it down. `AnswerFeedback` is the second C++ type. Navigation is in `AppShell.qml` only.

Every component below has an `objectName` on the parts a test reads. Sizes are in logical pixels. Colours, outline widths,
radii, the hard offset shadow and the two fonts come from the `Theme` singleton and nowhere else.

## Foundation

| Component | Properties | Notes |
|---|---|---|
| `Theme` (singleton) | Colours (`background`, `halftone`, `ink`, `red`, `blue`, `green`, `orange`, `heartPink`, `white`, `metal`, `rope`, `coin`, `disabledFill`, `disabledText`), `outlineSmall`, `outlineLarge`, `radiusSmall`, `radiusLarge`, `shadowSmall`, `shadowLarge`, `displayFont`, `bodyFont`, `fontsReady`, `crossedSeven`, `compactMaxWidth` | `sizeClassFor(width)` returns `compact` below 600 and `regular` from 600. `formatTime`, `formatNumber`, `operatorGlyph`, `countWord` and `bitCount` are the text helpers. The fonts load from `qrc:/heart_boom_maths/fonts/` and `fontsReady` reports it. `crossedSeven` (false) swaps both fonts for the HBM Display and HBM Body copies whose 7 has a crossbar; no screen needs to change |
| `HalftoneBackground` | `gridSize` (12), `dotRadius` (1.6) | The yellow page with its dots. Draws once per size |
| `ComicCard` | `fillColor`, `borderColor`, `borderWidth` (5), `radius` (22), `shadowOffset` (8; 0 for none), `dashed`; children go inside the outline | White card with the ink outline and hard shadow |
| `DashedBox` | `lineColor`, `lineWidth`, `radius`, `fillColor`, `dashPattern` | Rounded box with a dashed outline inside its bounds. The `?` slot and a removed tile's ghost |
| `ComicButton` | `text`, `accessibleName`, `variant` (`primary` blue, `danger` red, `secondary` white), `pixelSize`, `letterSpacing`, `displayLabel`, `borderWidth`, `radius`, `shadowOffset`, `horizontalPadding`, `fillColor`, `textColor`; signal `clicked`; children are drawn inside (an icon) | Tap, click, Return, Enter and Space. Takes keyboard focus with a ring. Accessible role button with a name. Implicit size is never below 44; a disabled button turns grey. A press asks `Haptics.tap()` for a light tick unless `hapticTap` is false (the hint) |
| `Badge` | `text`, `fillColor`, `textColor`, `borderColor`, `borderWidth` (0 for none), `radius`, `dashed`, `shadowOffset`, `pixelSize`, `letterSpacing`, `horizontalPadding`, `verticalPadding`; children are drawn inside | The multiplier, ENDLESS, `-3 s`, `+5 s` |
| `CoinWallet` | `value`, `delta` (signed, 0 hides it), `coinSize`, `pixelSize`, `deltaPixelSize`, `borderWidth`, `radius`, `plain` (false) | The coin count; a change shows beside it with its sign. `plain` leaves out the pill (fill and outline) for a card that has its own: the title's player card |
| `StarBurst` | `text`, `subText`, `outerColor`, `innerColor`, `textColor`, `subTextColor`, `textSize`, `subTextSize`, `letterSpacing`, `padding` (4), `spikes`, `valleyDepth`, `innerShare`; read-only `fitScale`, `fitsInside` | Spiky outer polygon and white inner polygon; the text shrinks to stay inside the inner polygon with the padding and never grows past its nominal size |
| `SparkStar` | `fillColor`, `strokeColor`, `strokeWidth`, `width` | The star at the burning end of the fuse |
| `ArrowIcon`, `PauseIcon`, `GearIcon` | `color`, size | Drawn because the fonts have no such glyphs |

## Play

| Component | Properties | Notes |
|---|---|---|
| `HeartBomb` | `secondsLeft`, `alert`, `boom`, `size` (120); read-only `stateName` | Calm: pink, black numerals. Alert: red, white numerals, tilt and shake, speed lines, bigger yellow spark. Boom: a star burst pops. Drawn on a 120 grid |
| `FuseBar` | `fraction`, `alertFraction`, `alert`, `lostFraction`, `secondsLeft`, `borderWidth`, `zoneBorderWidth`, `stripePeriod`, `starSize` | Shrinks with the time; the alert zone is striped; the alert turns it red. `alertFraction` comes from the controller's `alertTicks` and `ticksTotal`, so endless levels get their own share. Accessible progress bar |
| `QuestionCard` | `parts`, `slotValue`, `fontSize`, `multiLineFontSize`, `slotWidth`, `slotHeight`, `slotBorder`, `slotRadius`, `partSpacing`, `rowSpacing`; read-only `multiLine`, `contentHeight`, `spoken` | Operators in blue, dashed red `?` slot. More than five parts read on two lines. A line that is too wide shrinks. `slotValue` fills the slot green |
| `AnswerTile` | `value`, `look` (`normal`, `wrong`, `right`, `dimmed`, `gone`), `interactive`, `fontSize`, `borderWidth`, `radius`, `shadowOffset`, `badgeSize`; signal `tapped` | Wrong: red, tilted, shakes, cross badge. Right: green, bigger, pulses, tick badge. Gone: fades, drops, dashed ghost. Only `normal` takes taps |
| `AnswerGrid` | `values`, `looks`, `interactive`, `metrics`; signal `answered(position)` | Four tiles in 2 by 2 or one row |
| `FeedbackTag` | `text`, `tone` (`wrong`, `right`, `alert`), `pixelSize`, `borderWidth`, `shadowOffset` | The tilted caption: NOT QUITE, +10, HURRY |
| `PlayHeart`, `PlayFuse` | Heart and fuse with the `-3 s` flash and the `+5 s` tag | |
| `PlayStats` | `points`, `multiplier`, `multiplierBefore`, `wallet`, `walletDelta`, `plainPoints` | Points, multiplier badge (red `x2!`, plain `x1`, dashed `x2 -> x1` while falling) and wallet |
| `PlayTopBar` | `level`, `endless`, `questionNumber`, `questionsPerLevel`; signal `pauseClicked` | |
| `PlayHint` | `cost`, `used`, `removedCount` | A `ComicButton` that reads `HINT - 2 COINS`, goes grey, and after use says `HINT USED - TWO TILES GONE` |
| `PlayMetrics` | `mode` (`compact`, `portrait`, `landscape`) | Every measurement of the three arrangements |
| `PlayScreen` | `game`, `feedback`; read-only `mode`, `alertShown`; signal `streakStepped` | The play screen. Draws held feedback from `AnswerFeedback` and everything else from the controller. It does not draw the red alert frame or the streak flash: it says `alertShown` and `streakStepped`, and `AlertFrame` draws them over the whole window |
| `AlertFrame` | `alert`, `unsafeLeft`, `unsafeTop`, `unsafeRight`, `unsafeBottom`, `lineWidth` (8); function `flash()` | The red frame that pulses in the last 10 seconds (`alertEdge`, four non-overlapping bars `alertEdgeTop`, `alertEdgeBottom`, `alertEdgeLeft`, `alertEdgeRight`) and the white flash of a multiplier step (`streakFlash`). It fills the window, outside the safe area, so on a phone both reach the real edge, under the cut-out and the home indicator, while the play screen stays inside the safe area. Each side of the frame is `lineWidth` plus the margin the device keeps clear on that side, so the frame still reads at a rounded corner and beside a notch; with no margins (a desktop window) it is the plain 8 pixel line of board CP3. Neither item is enabled or takes a tap |
| `AppRoot` | `game` | What the window shows: `SafeAreaFrame` with `AppShell` inside, and `AlertFrame` over it, driven by the shell's `alertShown` and `streakStepped`. `Main.qml` is only the `Window` around it, so the `ui` tests load the same composition (`app_root()` in `ui_support`, with `SafeArea.additionalMargins` standing in for a phone's margins) |
| `PauseOverlay` | `game`, `showSettings` (false); signal `settingsRequested` | The pause screen over the play screen. The shell turns the gear on and opens Settings from it |
| `Placeholder` | `title`, `subtitle`; children are the buttons | Marked in its own comment as a stand-in; each real screen replaces its use in `AppShell.qml` |

## Title, Settings, About and the parental gate

These four are the screens a child (or a grown-up) uses around the game. Each reads the controller and says nothing it did not
get from it: the saved run's caption, the build line and the number the gate asks for all come from `GameController`.

| Component | Properties | Notes |
|---|---|---|
| `TitleScreen` | `game`; signals `top10Requested`, `settingsRequested`, `playersRequested` | The player card, the heart bomb in its burst, the name card, the tagline, PLAY (or CONTINUE and NEW GAME when `hasSavedRun`), TOP 10 and the gear. The player card (`playerChip`, replacing board CP1's BEST and coin pills) is one white button across the top with the outline and hard shadow of the other cards: `playerName` and a down chevron on the first line, BEST with the best score (`bestScoreValue`) and the coin with the coins (`walletPill`, a plain `CoinWallet`) under them. It says `playersRequested`, and its accessible name gives who is playing, the best score and the coins, and says that it chooses who is playing. The card is 24 taller than the pills were; the column takes that from its top margin and from the gap above the hero, so the burst and everything under it stay where board CP1 has them. The line under the buttons is the saved run (`Saved game: level 5, 0:31 left`, from `savedRun`), the calm line shown after a failed CONTINUE, or the plain promise. The tagline's numbers are `firstLevelQuestions` and `firstLevelSeconds` |
| `ScreenFrame` | `title`, `titleSize`, `titleSpacing`, `backName`, `designHeight`, `columnSpacing`, `escapeEnabled`; read-only `unit`, `contentWidth`; children go in the body; signal `backClicked` | The page that opens over another: dotted background, back button with the title, body column. The back button, the Escape key and the Android back key all say `backClicked`. `unit` scales the boards' 342 wide column to the window (up to 480 wide) and shrinks it when the window is short |
| `SettingsScreen` | `game`; signal `aboutRequested` | `SettingRow` for Sound effects and Music; Haptics only when `hapticsAvailable`; Game Center only when `gameCenterAvailable`; ABOUT; the build line (`versionText`) |
| `SettingRow` | `title`, `hint`, `checked`, `switchObjectName`, `unit`; the icon is the child; signal `toggleRequested` | A white card with an icon, a title, a line under it and a `ComicSwitch` |
| `Haptics` (singleton) | `controller`; function `tap()` | The controls' way to `GameController::tap()`; `AppShell` sets the controller. See [Haptics](haptics.md) |
| `HapticsIcon` | | A phone with a buzz on each side, the mark of the Haptics row |
| `ComicSwitch` | `checked`, `accessibleName`; signal `clicked` | Shows the state three ways: the word ON or OFF, the side the knob is on, and the track colour. It does not flip itself: the screen decides, because Game Center waits for the gate. Accessible check box with its state. Space, Return and Enter work |
| `RowButton` | `label`, `hint`, `arrow`, `hintPixelSize`, `unit` (and the `ComicButton` properties) | A button with its label at the left and a small line or an arrow at the right |
| `AboutScreen` | `game`; signal `backClicked` | Version, revision, branch, build date, the font licences, and four rows: Licence and Third-party notices open a `TextViewer` over About; Privacy policy and Source code call `requestLink` with `privacyPolicyUrl` and `repositoryUrl`, so the gate comes first |
| `TextViewer` | `text` (a `ScreenFrame`) | A white card with a scrolling text: finger, wheel, arrow keys, Page Up and Down, Home and End. The bar shows where the reading is |
| `PlayersScreen` | `game`; signal `nameRequested(playerId, isNew)`; (a `ScreenFrame`) | WHO'S PLAYING. One card per entry of `players`: a tick in a ring (empty for the others), the name, the best score and the coins, and for the player who is playing a blue card with a PLAYING tag. The card is a `ComicButton` that calls `selectPlayer`; NAME or RENAME (`nameRequested`) and REMOVE (`requestRemovePlayer`, hidden when `canRemovePlayer` is false) are drawn over its lower edge. ADD PLAYER (`addPlayer`, then `nameRequested`) shows while `canAddPlayer`. The layout is made for `maxPlayers` cards. Object names: `playerCard`, `playerSelect`, `playerTick`, `playerName`, `playingTag`, `playerBest`, `playerCoins`, `renameButton` and `removeButton`, each with the place in the list, and `addPlayerButton`. There is no board for it or for the chip (DECISIONS, P8) |
| `ParentalGateScreen` | `game`; (a `ScreenFrame`) | The number in words, a display, a keypad (1 to 9, 0, DEL), CANCEL and OPEN (TURN ON for Game Center, REMOVE for a player, whose name the prompt gives from `gatePlayerName`). OPEN is grey until `gateMatches(typed)`. Three digits that are not the number are a wrong answer at once (the keypad cannot be used to find the number a digit at a time), and so is Return on a wrong three digits: a mark and the words `Not that number. Here is a new one.` show and the controller deals a new number. Return on fewer digits does nothing. A hardware keyboard types, deletes (Backspace) and submits (Return) |

Navigation for these is in `AppShell.qml`: `pages` is a stack (`settings`, `about`, `top10`, `players`, `playerName`), `showSettings()`, `showAbout()`,
`showTop10()`, `showPlayers()`, `showPlayerName(id, isNew)` and `back()` change it, and any change of the game's state empties it, so a page never outlives the game it was
opened in. The pause screen's gear opens Settings and returns to the pause screen. The gate sits over everything while
`gateActive`. `top10Page` in the shell is a stand-in for the leaderboard screen.

`HeartBomb` has two more properties for these screens: `showSeconds` (false on the title and About, where it is a picture)
and `face` (the eyes and smile of the title). `CoinWallet.displayNumerals` sets the count in the heading font.
## Result and score screens

All of them stand on `ScorePage`, whose content column is 342 wide at `unit` 1 (the phone boards). `unit` grows with the
window up to a column of `maxWidth` and shrinks when the window is short, so every size inside a screen is a number times
`unit` and nothing is cut off. None of them works out a rule: they show what the controller gives.

| Component | Properties | Notes |
|---|---|---|
| `ScorePage` | `designHeight` (the content's height at unit 1), `maxWidth` (480), `topMax`, `reservedBottom`, `columnSpacing`; read-only `unit`, `columnWidth`, `compact`; children go in the column | Halftone page and one centred column. A phone's content starts `topMax` from the top, a wider window centres it |
| `StatRow` | `label`, `value`, `textColor`, `pixelSize`, `display` (Bangers for a total); children sit between label and number | A line of a score card. Its two texts are `<objectName>Label` and `<objectName>Value` |
| `CoinCard` | `text`, `note`, `coinSize`, `pixelSize`, `notePixelSize`, `borderWidth`, `radius`, `shadowOffset`, `centered` | The white card with a coin: `+17 COINS` and the wallet, `+1,000 COINS`, `WALLET 61 saved` |
| `LevelClearScreen` | `game` | CP6. Rows from `game.levelClear`: answer points, a PERFECT LEVEL row only when perfect, time bonus, LEVEL POINTS; then the coins card, NEXT LEVEL and the next-up line. When `levelClear.complete` is false (the level came back from a saved run part way) the answer points are left out and the total row reads SCORE |
| `WinScreen` | `game` | CP8. The bonus card reads `+1,000 COINS` from `game.winBonus`, or `BONUS WON BEFORE` when the core paid nothing; KEEP GOING and FINISH |
| `GameOverScreen` | `game`; signals `top10Requested`, `homeRequested` | CP7. The BOOM! burst shows only for `endReason` Bomb. NEW BEST! tags the best-score row after a saved best; COINS EARNED is left out after CONTINUE. PLAY AGAIN calls `startGame()` |
| `NameEntryScreen` | `game`, `name`, `keyboardMode` (`auto`, `on`, `off`), `forPlayer`, `playerId`, `initialName`, `burstText`, `skipText`; read-only `keyboardShown`, `canSave`; signals `saveStarted`, `finished` | CP11. The same screen names a player (`forPlayer`, from WHO'S PLAYING, objectName `playerNameScreen` in the shell): no score card, the field starts with `initialName`, SAVE calls `renamePlayer(playerId, name)` and the second button (`skipText`: SKIP or CANCEL) leaves the name as it was; both say `finished` and the shell goes back.  A drawn field fed by a hardware keyboard and `NameKeyboard`; every character goes through `game.normaliseName` and SAVE through `game.canSaveName`, so the screen holds no letter set and no length. `auto` shows the keyboard in a compact window, on iOS and Android and after a touch |
| `NameKeyboard` | `lettersEnabled`, `rows`; signals `letterTyped`, `deleteTyped` | A to Z, SPACE and DEL in the grey area the board reserves for a keyboard |
| `Top10Screen` | `game`, `highlightRank`, `playText`; signals `backRequested`, `playRequested` | CP12. One `ScoreRow` per entry, an empty-list card, a back arrow and the play button |
| `ScoreRow` | `rank`, `name`, `scoreText`, `highlighted`, `unit` | Medal for the first three, a plain number after. The highlighted row is blue with white text and a NEW tag |
| `ChevronIcon` | `color`, `strokeWidth` | The back arrow, drawn |

`StarBurst` also takes `textRotation` (the lean of the title in degrees) and centres a title of more than one line.

### Navigation of these screens

`AppShell` shows the name screen when the core is in game over with `nameEntryPending`, and the game-over card once it is
dealt with (SAVE or SKIP). Two screens are not core states, so the shell keeps an `overlay`, cleared whenever the core's
state changes: the Top 10, and Title after HOME on the game-over card (the core has no state for that). The functions the
screens call are `showTop10()` (from the Title's TOP 10 button, the game-over card, and after SAVE), `closeTop10()` and
`showTitle()`. The Top 10 marks the new entry only while it follows the game that earned it. When the controller saves a score at once (a player with a name has no name screen, `scoreSavedAtOnce`), the shell sets `top10Due` and opens the Top 10 page when the game-over card is due, which is after the boom's hold, the same place SAVE leads to.

## AnswerFeedback

The core deals the next question the moment an answer is given, so the right or wrong look is held in the app layer.
`GameController.answered` keeps the question as it stood when the child tapped (parts, tiles, removed tiles, the tapped and
the correct tile). `AnswerFeedback` (`kind`: `None`, `Right`, `Wrong`, `Boom`) holds it for `rightHoldMs`, `wrongHoldMs`
(1 s) or `boomHoldMs`, then clears. It never pauses the core, and the play screen ignores taps while a look is held. When
the held answer was the last of a level, or the bomb went off, the shell keeps the play screen up until the hold ends.

## Size classes and arrangements

`compact` is a phone in portrait: a 2 by 2 grid of tiles at least 96 high. `regular` is wider than 600: `portrait` (a
tablet) scales the same arrangement up, `landscape` (width at least height) moves the stats into a left panel and puts the
four tiles in one row. The class follows the window, so a resized desktop window changes arrangement while playing.
