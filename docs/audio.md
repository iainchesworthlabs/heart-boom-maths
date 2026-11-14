# Audio

The game plays the effects of the feedback table in [the specification](SPEC.md) and two music beds. The rules of
when a sound may play are plain C++ in `apps/game/src/services/`; only the last step, the call into the sound device,
uses Qt Multimedia.

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
`QSoundEffect` reads, which on every platform means uncompressed PCM WAV.

## The service

| Piece | Job |
|---|---|
| `AudioService` | The interface the controller calls: `play_effect`, `stop_effect`, `play_music`, `set_enabled`, `suspend`, `resume` and `unlock` |
| `BackedAudioService` | The rules, with no Qt. Effects and music honour the two Settings switches. `suspend()` stops everything, and `resume()` starts the music bed again. An effect asked for while sound is not allowed is dropped, not queued. A value that is no sound is ignored. `stop_effect` always goes through |
| `AudioBackend` | The thin layer that starts and stops a clip. The tests use a recording one |
| `qt_audio_backend.cpp` | The backend over `QSoundEffect`, one voice per clip, loaded when the game starts. Looping effects and beds use the loop count of the voice, so no media decoder is needed |
| `no_audio_backend.cpp` | Used instead of the file above when the build has no Qt Multimedia. It makes no backend, so the game is silent |
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
the backend is built and `Qt6::Multimedia` is linked, and otherwise a status line says so and the game builds silent. The
CI job that installs Qt with aqtinstall needs `modules: qtmultimedia` (for the WebAssembly kit as well) for the checked-in
sounds to be heard in those builds.
