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
| `Theme` (singleton) | Colours (`background`, `halftone`, `ink`, `red`, `blue`, `green`, `orange`, `heartPink`, `white`, `metal`, `rope`, `coin`, `disabledFill`, `disabledText`), `outlineSmall`, `outlineLarge`, `radiusSmall`, `radiusLarge`, `shadowSmall`, `shadowLarge`, `displayFont`, `bodyFont`, `fontsReady`, `compactMaxWidth` | `sizeClassFor(width)` returns `compact` below 600 and `regular` from 600. `formatTime`, `formatNumber`, `operatorGlyph`, `countWord` and `bitCount` are the text helpers. The fonts load from `qrc:/heart_boom_maths/fonts/` and `fontsReady` reports it |
| `HalftoneBackground` | `gridSize` (12), `dotRadius` (1.6) | The yellow page with its dots. Draws once per size |
| `ComicCard` | `fillColor`, `borderColor`, `borderWidth` (5), `radius` (22), `shadowOffset` (8; 0 for none), `dashed`; children go inside the outline | White card with the ink outline and hard shadow |
| `DashedBox` | `lineColor`, `lineWidth`, `radius`, `fillColor`, `dashPattern` | Rounded box with a dashed outline inside its bounds. The `?` slot and a removed tile's ghost |
| `ComicButton` | `text`, `accessibleName`, `variant` (`primary` blue, `danger` red, `secondary` white), `pixelSize`, `letterSpacing`, `displayLabel`, `borderWidth`, `radius`, `shadowOffset`, `horizontalPadding`, `fillColor`, `textColor`; signal `clicked`; children are drawn inside (an icon) | Tap, click, Return, Enter and Space. Takes keyboard focus with a ring. Accessible role button with a name. Implicit size is never below 44; a disabled button turns grey |
| `Badge` | `text`, `fillColor`, `textColor`, `borderColor`, `borderWidth` (0 for none), `radius`, `dashed`, `shadowOffset`, `pixelSize`, `letterSpacing`, `horizontalPadding`, `verticalPadding`; children are drawn inside | The multiplier, ENDLESS, `-3 s`, `+5 s` |
| `CoinWallet` | `value`, `delta` (signed, 0 hides it), `coinSize`, `pixelSize`, `deltaPixelSize`, `borderWidth`, `radius` | The coin count; a change shows beside it with its sign |
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
| `PlayScreen` | `game`, `feedback`; read-only `mode` | The play screen. Draws held feedback from `AnswerFeedback` and everything else from the controller |
| `PauseOverlay` | `game`, `showSettings` (false); signal `settingsRequested` | The pause screen over the play screen. The shell turns the gear on and opens Settings from it |
| `Placeholder` | `title`, `subtitle`; children are the buttons | Marked in its own comment as a stand-in; each real screen replaces its use in `AppShell.qml` |

## Title, Settings, About and the parental gate

These four are the screens a child (or a grown-up) uses around the game. Each reads the controller and says nothing it did not
get from it: the saved run's caption, the build line and the number the gate asks for all come from `GameController`.

| Component | Properties | Notes |
|---|---|---|
| `TitleScreen` | `game`; signals `top10Requested`, `settingsRequested` | Best score, coins, the heart bomb in its burst, the name card, the tagline, PLAY (or CONTINUE and NEW GAME when `hasSavedRun`), TOP 10 and the gear. The line under the buttons is the saved run (`Saved game: level 5, 0:31 left`, from `savedRun`), the calm line shown after a failed CONTINUE, or the plain promise. The tagline's numbers are `firstLevelQuestions` and `firstLevelSeconds` |
| `ScreenFrame` | `title`, `titleSize`, `titleSpacing`, `backName`, `designHeight`, `columnSpacing`, `escapeEnabled`; read-only `unit`, `contentWidth`; children go in the body; signal `backClicked` | The page that opens over another: dotted background, back button with the title, body column. The back button, the Escape key and the Android back key all say `backClicked`. `unit` scales the boards' 342 wide column to the window (up to 480 wide) and shrinks it when the window is short |
| `SettingsScreen` | `game`; signal `aboutRequested` | `SettingRow` for Sound effects and Music; Game Center only when `gameCenterAvailable`; ABOUT; the build line (`versionText`) |
| `SettingRow` | `title`, `hint`, `checked`, `switchObjectName`, `unit`; the icon is the child; signal `toggleRequested` | A white card with an icon, a title, a line under it and a `ComicSwitch` |
| `ComicSwitch` | `checked`, `accessibleName`; signal `clicked` | Shows the state three ways: the word ON or OFF, the side the knob is on, and the track colour. It does not flip itself: the screen decides, because Game Center waits for the gate. Accessible check box with its state. Space, Return and Enter work |
| `RowButton` | `label`, `hint`, `arrow`, `hintPixelSize`, `unit` (and the `ComicButton` properties) | A button with its label at the left and a small line or an arrow at the right |
| `AboutScreen` | `game`; signal `backClicked` | Version, revision, branch, build date, the font licences, and four rows: Licence and Third-party notices open a `TextViewer` over About; Privacy policy and Source code call `requestLink` with `privacyPolicyUrl` and `repositoryUrl`, so the gate comes first |
| `TextViewer` | `text` (a `ScreenFrame`) | A white card with a scrolling text: finger, wheel, arrow keys, Page Up and Down, Home and End. The bar shows where the reading is |
| `ParentalGateScreen` | `game`; (a `ScreenFrame`) | The number in words, a display, a keypad (1 to 9, 0, DEL), CANCEL and OPEN (TURN ON for Game Center). OPEN is grey until `gateMatches(typed)`. Three digits that are not the number are a wrong answer at once (the keypad cannot be used to find the number a digit at a time), and so is Return on a wrong three digits: a mark and the words `Not that number. Here is a new one.` show and the controller deals a new number. Return on fewer digits does nothing. A hardware keyboard types, deletes (Backspace) and submits (Return) |

Navigation for these is in `AppShell.qml`: `pages` is a stack (`settings`, `about`, `top10`), `showSettings()`, `showAbout()`,
`showTop10()` and `back()` change it, and any change of the game's state empties it, so a page never outlives the game it was
opened in. The pause screen's gear opens Settings and returns to the pause screen. The gate sits over everything while
`gateActive`. `top10Page` in the shell is a stand-in for the leaderboard screen.

`HeartBomb` has two more properties for these screens: `showSeconds` (false on the title and About, where it is a picture)
and `face` (the eyes and smile of the title). `CoinWallet.displayNumerals` sets the count in the heading font.

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
