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
| `PauseOverlay` | `game`, `showSettings` (false); signal `settingsRequested` | The pause screen over the play screen. The gear is hidden until a Settings screen exists |
| `Placeholder` | `title`, `subtitle`; children are the buttons | Marked in its own comment as a stand-in; each real screen replaces its use in `AppShell.qml` |

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
