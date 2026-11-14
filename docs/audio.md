# Audio

The game plays the effects of the feedback table in [the specification](SPEC.md) and two music beds. The rules of
when a sound may play are plain C++ in `apps/game/src/services/`; only the last step, the call into the sound device,
uses Qt Multimedia, or AVFoundation on iPhone and iPad.

## The sound files are placeholders

Sound and music are not designed yet. The files in `apps/game/audio/` are placeholders, made by
`tools/assets/make_placeholder_sounds.py` from sine tones, harmonics and a seeded noise source. They contain no recorded
or sampled material, are the project's own work and are released under GPL-3.0-or-later. A sound designer replaces
them.

| File | Sound | Kind |
|---|---|---|
| `coin_chime.wav` | `COIN_CHIME`, a correct answer | effect |
| `soft_buzz.wav` | `SOFT_BUZZ`, a wrong answer | effect |
| `rising_tone.wav` | `RISING_TONE`, a streak step | effect |
| `ticking.wav` | `TICKING`, the alert zone. A half-second loop that plays until `stop_effect` | effect |
| `cartoon_pop.wav` | `CARTOON_POP`, the bomb goes off | effect |
| `fanfare.wav` | `FANFARE`, level complete | effect |
| `whoosh.wav` | `WHOOSH`, a hint | effect |
| `music_menu.wav` | `MENU` | music bed, loops |
| `music_play.wav` | `PLAY` | music bed, loops |

All files are 16-bit PCM, mono, 22.05 kHz, about 0.8 MB in all, so the web download stays small. Each file is a resource
(`:/heart_boom_maths/audio/<file>`), never read from disk or fetched. `audio_assets.cpp` maps each `SoundId` and
`MusicId` to its file, its looping and its volume.

The generator uses only the Python standard library. Of the floating-point operations it uses only `+ - * /` and
`floor`, which give the same result on every machine, so the files are byte-identical everywhere.

```bash
python3 tools/assets/make_placeholder_sounds.py            # rewrite apps/game/audio/*.wav
python3 tools/assets/make_placeholder_sounds.py --check    # what the test game.audio_placeholders runs
```

To replace a sound, drop the designer's file over the same name (or change the name in `audio_assets.cpp` and the
resource list in `apps/game/CMakeLists.txt`), annotate it in `REUSE.toml`, add its credit to `apps/game/audio/Sounds.txt`
(which About shows through the notices), and stop using the generator for that file. A file can be any format
`QSoundEffect` and `AVAudioPlayer` both read, which means PCM WAV (the iOS backend tells AVFoundation the type is WAVE).

## The service

| Piece | Job |
|---|---|
| `AudioService` | The interface the controller calls: `play_effect`, `stop_effect`, `play_music`, `set_enabled`, `suspend`, `resume` and `unlock` |
| `BackedAudioService` | The rules, with no Qt. Effects and music honour the two Settings switches. `suspend()` stops everything, and `resume()` starts the music bed again. An effect asked for while sound is not allowed is dropped, not queued. A value that is no sound is ignored. `stop_effect` always goes through |
| `AudioBackend` | The thin layer that starts and stops a clip. The tests use a recording one |
| `qt_audio_backend.cpp` | The backend over `QSoundEffect`, one voice per clip, loaded when the game starts. Looping effects and beds use the loop count of the voice, so no media decoder is needed |
| `ClipAudioBackend` | The backend for a native player that takes a sound file from memory (iOS). Plain C++ over a `PlayerHost` and a `ClipPlayer`, so its rules are tested on every desktop. See [iOS](#ios-avfoundation) |
| `no_audio_backend.cpp` | Used instead of the Qt backend when the build has no Qt Multimedia and is not for iOS. It makes no backend, so the game is silent |
| `make_audio_service()` | Chooses the service at start-up (below) |
| `FirstGestureFilter` | An event filter on the application that calls `unlock()` on the first click, touch or key press |

The factory gives `NullAudioService` for a `--smoke-test` run, for the `offscreen` and `minimal` Qt platforms (a CI
machine, which has no sound device), and when there is no backend or no output device. It never fails and never waits for
a device. The log line at start-up says which it chose.

Browsers start no sound before the player's first gesture. On the web the service is made with `requires_unlock`, holds
back every effect and the music until `unlock()`, and then starts the music bed that was asked for last. On every other
platform `unlock()` changes nothing. What the web build can do with `QSoundEffect` is checked by the web spike (M2).

## Qt Multimedia in the build

Qt Multimedia is an add-on module of the Qt kit. The configure looks for the `Qt6Multimedia` package; when it is found
the backend is built and `Qt6::Multimedia` is linked, and otherwise a status line says so and the game builds silent. iOS
does not look for the module at all (below). The
CI job that installs Qt with aqtinstall needs `modules: qtmultimedia` (for the WebAssembly kit as well) for the checked-in
sounds to be heard in those builds.

## iOS (AVFoundation)

Qt Multimedia cannot link into the static iOS build: its backend there is FFmpeg, and the kit has no FFmpeg libraries.
The iOS preset therefore builds without it (`HEART_BOOM_MATHS_USE_QT_MULTIMEDIA=OFF`, and the configure ignores the
option on iOS anyway), and iPhone and iPad take their sound from AVFoundation. `make_device_audio_backend()` is the one
seam: `apps/game/src/platform/ios/avf_audio_backend.mm` defines it on iOS, and CMake adds neither `qt_audio_backend.cpp`
nor `no_audio_backend.cpp` there.

The work is split so that only translation is left to the Objective-C++:

| Piece | Holds |
|---|---|
| `ClipAudioBackend` (`clip_audio_backend.{hpp,cpp}`) | Every decision: which clips load at start-up, the volume of each (`player_volume`: the percentage of `audio_assets.cpp`, held to 0 to 100), when the session is activated, what an interruption does, what a reset does. Tested over a fake host and fake players |
| `read_audio_clip` (`audio_resource.{hpp,cpp}`) | Reads a clip from `:/heart_boom_maths/audio/` with `QFile`. AVFoundation cannot open a `qrc:` path, so the bytes are handed over as memory |
| `avf_audio_backend.mm` | `AvfHost`: `AVAudioSession` calls and `AVAudioPlayer` creation from `NSData`. `AvfPlayer`: play, stop, rewind. `AvfAudioBackend`: the observers of the session notifications, which call the three methods of `ClipAudioBackend` |

What the backend does:

- **One `AVAudioPlayer` per clip**, all nine made when the backend is made, so the first effect is not late. Looping clips
  (the ticking and the two beds) have `numberOfLoops = -1`; the others play once. A clip that cannot be read or decoded is
  logged (its file name only) and skipped; playing it does nothing, and the game never waits for or crashes on it. When no
  clip loads at all the factory gives no backend, and the game is silent.
- **The session category is Ambient.** Sound follows the silent switch, mixes with the music the player has on and does
  not stop it, and nothing is recorded, so there is no microphone use and no usage text. The category is set when the
  backend is made and set again with each activation. The session is **activated by the first sound that is asked for**,
  not at start-up, so a game with both switches off never touches the audio route. iOS needs no first-gesture unlock.
- **Starting and stopping** have the meaning of `QtAudioBackend`: an effect starts from its beginning, `stop_effect`
  always stops, a bed replaces the bed that sounds. Suspend and resume come from `BackedAudioService` as on every platform.
- **An interruption** (a phone call, Siri, an alarm, Screen Time) reaches the backend as `AVAudioSessionInterruptionNotification`
  on the main queue. When it begins, every player stops and the session counts as inactive. When it ends, the session is
  activated again with the next sound, and the music bed that was sounding starts again from its beginning if the system
  sets `shouldResume`, unless the app stopped the bed meanwhile. Effects are not brought back. The ticking of a bomb that is
  already in its alert zone is therefore silent until the next one; the red fuse still shows it. A sound asked for while the
  session cannot be activated is dropped, and the next sound tries again, so a missing "ended" notification cannot mute the
  game for good.
- **A reset of the media services** (`AVAudioSessionMediaServicesWereResetNotification`) makes every player invalid, so all
  nine are made again and the bed starts again.

## WebAssembly

The aqt kit has no Qt Multimedia module, so the browser build is silent. A WebAudio backend or a source-built module is the
choice.
