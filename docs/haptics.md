# Haptics

A little touch feedback on phones: a tick when a button is pressed, a crisp tap for a right answer, a buzz for a wrong one,
a bump for a streak step, and a few more. It is always an extra. Every haptic goes with a cue that already shows on screen
(and usually sounds), and the game plays the same with haptics off or on a device that has none. The player can turn them
off in Settings.

The core knows nothing about haptics, as it knows nothing about sound. The app layer maps core events to haptics in one
table, like the sounds.

## What is felt, and when

| Haptic | When | Feel |
|---|---|---|
| `TAP` | A button is pressed, a key of the parental-gate keypad or the name keyboard is pressed, or a switch is flipped on | Light selection tick |
| `RIGHT` | A right answer | Light and crisp |
| `MULTIPLIER_UP` | A right answer that lifts the streak step (x2, x3) | Medium bump, instead of `RIGHT` |
| `WRONG` | A wrong answer | Short error buzz |
| `HINT` | A hint is bought | Soft |
| `ALERT` | The timer enters the last 10 seconds | Warning, once. The ticking that follows is not felt, and coming back to the alert zone after a pause does not warn again |
| `LEVEL_CLEAR` | A level is cleared | Success, instead of `RIGHT` for the last answer |
| `WIN` | Level 12 is cleared | Three small beats about a tenth of a second apart: light, medium, success |
| `BOOM` | The bomb goes off | Heavy, once. QUIT and FINISH are not felt |

- A tap on an answer tile gets only its result (`RIGHT`, `MULTIPLIER_UP` or `WRONG`), never a `TAP` as well. The hint button
  gets only `HINT`.
- One command gives at most one haptic. The tenth right answer sends a right answer and a level clear together, and only the
  clear is felt; a wrong answer that sets off the bomb gives only the boom. When two want the same moment the stronger wins:
  `BOOM`, `WIN`, `LEVEL_CLEAR`, `WRONG`, `ALERT`, `MULTIPLIER_UP`, `RIGHT`, `HINT`, `TAP`.
- Nothing is felt on every tick.

The table is `apps/game/src/event_haptics.cpp` (`haptic_for_event`, `haptic_for_events`), tested in
`tests/test_haptics.cpp`. The beats of each haptic and the priorities are `services/haptic_patterns.cpp`.

## How it is wired

```
core Event --> event_haptics (one haptic per command) --> HapticsService --> HapticBackend --> the system
QML press  --> Haptics.tap() --> GameController::tap() ----^
```

| Piece | File | Does |
|---|---|---|
| `HapticsService` | `services/haptics_service.hpp` | The interface the controller uses: `available()`, `play(HapticId)`, `set_enabled(bool)`, `suspend()`, `resume()`. `NullHapticsService` for the desktop and for tests that do not look |
| `BackedHapticsService` | `services/backed_haptics_service.*` | The rules, in plain C++ over a `HapticBackend`, a `Clock` and a `HapticTimer`, so every desktop tests them with fakes |
| `HapticBackend` | `services/haptic_backend.hpp` | One beat at a time: `available()`, `fire(beat)`, `cancel()` |
| Platform tables | `services/haptic_maps.*` | What each system is asked for, for each haptic (plain C++, tested) |
| `QtHapticTimer` | `services/qt_haptic_timer.*` | The single-shot timer of the application |
| `make_haptic_backend()` | `platform/<os>/haptics_platform.*` | The translation to the system, chosen by CMake through the per-OS directory (no `#if`) |
| `event_haptics` | `event_haptics.*` | The table above |
| `Haptics` (QML singleton) | `qml/Haptics.qml` | The controls' way to `GameController::tap()`. `AppShell` gives it the controller; a control used alone finds none and does nothing |

`ComicButton` and the name keys ask for a `TAP` on a press, and `ComicSwitch` after the click. A button whose action has a
haptic of its own sets `hapticTap: false` (the hint). The screens write no rule: the service drops a tap when haptics are
off, paused, too soon after another beat, or unavailable.

### The rules of the service

- **Nothing** while the Settings switch is off, while the service is suspended, or on a device that cannot. A cue asked for
  then is dropped, not queued.
- **Suspended** means the game is paused or the app is in the background. The controller suspends the service when the
  session pauses or the app leaves the foreground, and resumes it when the game is running again and the app is back. A
  beat still to come, such as the rest of the win, is dropped at once, and the backend is asked to cancel.
  Turning the switch off does the same. So a press on RESUME, which happens in the pause, is not felt.
- **Spacing.** Two beats never come closer than 40 ms (`HAPTIC_MIN_GAP_MS`). A cue that arrives inside the gap waits it out,
  and a cue that arrives while another waits replaces it only if it is stronger.
- **`TAP`** is dropped, not queued, inside 120 ms (`HAPTIC_TAP_GAP_MS`) of the last beat or while a cue waits, so drumming on a
  button gives a tick now and then, and a tick never delays the cue that follows it.
- **`WIN`** goes out as its three beats on a timer; only the bomb may replace it.

These numbers are app-layer comfort values, not game rules, and are to be judged on a device and adjusted there.

## The preference

Settings has a Haptics row after Music and before Game Center, a `ComicSwitch` like the others (the word ON or OFF as well
as the knob, and the accessible name "Haptics"). It is shown only where `hapticsAvailable` is true: hidden on a desktop,
shown on a phone, and on the web only on a touch screen whose browser has `navigator.vibrate`.

It is the optional `haptics` field of the save (boolean, default on). A save from before it existed reads as on, a field of the
wrong type damages the save as for `game_center`, and the writer always writes it. The schema version stays 1. Turning the
switch on is felt as one light tick, so the player knows what it does; turning it off is not.

The preference is kept even where haptics are unavailable (a save moved to a phone keeps what the player chose).

## Platforms

| Platform | `available()` | How a beat is made |
|---|---|---|
| iOS | `CHHapticEngine.capabilitiesForHardware.supportsHaptics` from CoreHaptics. True on an iPhone with a Taptic Engine, false on an iPad (which has none) and on the iPhones that predate it | `UISelectionFeedbackGenerator` for `TAP`; `UIImpactFeedbackGenerator` rigid for `RIGHT`, soft for `HINT`, medium for `MULTIPLIER_UP`, heavy for `BOOM`; `UINotificationFeedbackGenerator` error for `WRONG`, warning for `ALERT`, success for `LEVEL_CLEAR`. Each generator is prepared again after use. CoreHaptics is linked (`ios/ios.cmake`) for the check only |
| Android | The device has a vibrator (`Vibrator.hasVibrator()`, which needs no permission) | `View.performHapticFeedback(int)` on the window's decor view, on Android's UI thread (through `runOnAndroidMainThread`), with no flags. On Android 11 and later `RIGHT` is `CONFIRM` and `WRONG` is `REJECT`; below that (the minimum is 28) `KEYBOARD_TAP` and `LONG_PRESS`. `TAP` and `HINT` are `CLOCK_TICK`, `MULTIPLIER_UP` is `VIRTUAL_KEY`, `ALERT` and `BOOM` are `LONG_PRESS` |
| Web | `navigator.vibrate` exists and the main pointer is a finger (`(pointer: coarse)`), so a laptop browser shows no row | `navigator.vibrate` with a short pattern (8 to 160 ms, `WRONG` and `LEVEL_CLEAR` with a pause), in JavaScript inside `try` and `catch` |
| Windows, macOS, Linux | Never | Nothing (`NullHapticBackend`) |

- **No permission anywhere.** The Android manifest still asks for none, and `verify_package.sh` still fails on any. The game
  never calls `Vibrator.vibrate()`, which is the call that needs `android.permission.VIBRATE`; `performHapticFeedback` needs
  nothing. `hasVibrator()` is only a question. The iOS feedback generators call no API that Apple's privacy manifest lists, so
  `PrivacyInfo.xcprivacy` is unchanged.
- **Android's own setting decides.** No `FLAG_IGNORE_GLOBAL_SETTING` or `FLAG_IGNORE_VIEW_SETTING` is passed, so a phone
  with "Touch feedback" (or "Vibration feedback") off in the system settings stays still even with the game's switch on. The
  game's row cannot know it.
- **iOS and Low Power Mode or Silent mode** are Apple's to apply; the generators are used as the system intends.
- **The web** lets a page vibrate only after the player has touched it, which every cue here follows (a tap starts them). iOS
  Safari has no `navigator.vibrate`, so a web page on an iPhone shows no row.

## What a person must check on a device

None of this can be felt in CI. The rules and the tables are tested everywhere; the feel is not.

- [ ] iPhone: a press, a right answer, a wrong answer, a hint, a streak step, the alert, a level clear, the win (three beats)
      and the bomb each feel different and not overdone. The first beat after launch is not late.
- [ ] iPhone: the row shows in Settings. iPad: it does not.
- [ ] Android phone: the same, on a device with Android 11 or later (`CONFIRM` and `REJECT`) and on one below it. A tablet
      with no vibrator shows no row.
- [ ] Android: with the system's touch feedback off, the game is still, with the game's switch on. With it on, the game moves.
- [ ] Android: the installed package asks for no permission (`apps/game/android/verify_package.sh`).
- [ ] Phone browser (Android Chrome): the row shows, a tap vibrates, and a desktop browser shows no row.
- [ ] Turning the switch off at once ends a win that is playing, and a pause or leaving the app does the same. Nothing is
      felt on the pause screen or in the background.
- [ ] Turning the switch on gives one light tick. The setting is still there after a restart.
- [ ] Drumming on a button or the keypad gives an occasional tick, not a continuous buzz.
- [ ] The spacing and the length of the beats (`haptic_patterns.cpp`, `haptic_maps.cpp`, `backed_haptics_service.hpp`) are
      right for a child's hand. Adjust the numbers there.
