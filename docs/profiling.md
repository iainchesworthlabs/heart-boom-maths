# Profiling

The game is instrumented with [Tracy](https://github.com/wolfpld/tracy). The marks go through the rules library
(`libs/core`), the app layer, the save, the sound and the scene graph's frame, so a trace shows where time goes from
a command to a presented picture. The numbers measured on one machine are at the end of this page.

The Tracy client is **built into the desktop builds, dormant**. It starts only when a person asks for it when the
program starts. The web, Android and iOS builds, and the sanitizer, coverage and fuzz builds, have no Tracy at all.

| Build | Tracy is | It starts |
|---|---|---|
| `windows-msvc-release`, `linux-release`, `macos-release` | built in, dormant | only for `--profile` or `HBM_PROFILE=1` |
| `linux-debug` (and any Debug build with `ICL_ENABLE_TRACY=ON`) | built in, dormant | only for `--profile` or `HBM_PROFILE=1` |
| `windows-msvc-profile`, `linux-profile` | built in | at launch; `--no-profile` stops it |
| `windows-msvc-asan`, `linux-asan-ubsan`, `linux-tsan`, `linux-coverage`, fuzz | not built | never |
| `wasm-release`, `android-*`, `ios-*` | not built | never; the configure stops if it is asked for |

A Debug build follows the same switch as a release, which is the cleaner rule: one way to start the profiler
everywhere, and a debug test run opens no socket. (The profile presets exist to take a trace, so they start by
default.)

## What is listening, when, and how to be sure it is not

A running Tracy client opens one TCP port (8086; `TRACY_PORT` in the environment changes it) and answers UDP
discovery broadcasts. Three settings of the client library, made by the overlay port in `vcpkg-overlay-ports/tracy`,
decide when and to whom:

- **Manual lifetime** (`TRACY_MANUAL_LIFETIME`): Tracy would normally start its worker thread and open its socket in a
  static initialiser, before `main`, in every process that links it. Here the client does not exist until the program
  calls `tracy::StartupProfiler()`, which happens in one place, `profile::start()`, called by `main()` only for the
  switch below. A process that is not asked has no Tracy thread, no socket, no broadcast and no allocation.
- **Only localhost** (`TRACY_ONLY_LOCALHOST`): the listener and the broadcast are bound to the loopback interface
  (127.0.0.1). The machine's network address does not answer; to profile from another machine, tunnel the port
  (`ssh -L 8086:127.0.0.1:8086 host`).
- **On demand** (`TRACY_ON_DEMAND`): once started, nothing is collected until a viewer connects.

Until `main()` starts the client, every mark of the game is one relaxed atomic load and a branch that is not taken.

The switch is read once, at launch, by `main()`, in this order (`apps/game/src/profiling/profile_switch.hpp`):

1. the command line: `--profile` starts it, `--no-profile` does not, `--profile-wait` starts it and waits (at most 30 s)
   for a viewer before the window opens, so that the start-up is in the trace; the last one wins;
2. the environment: `HBM_PROFILE` is `1`, `true`, `on` or `yes` (in any case) to start, `wait` to start and wait,
   `0`, `false`, `off` or `no` not to; anything else, or empty, is ignored, so a typo never opens or closes a profiler;
3. the build's default: off, except in the profile presets.

Nothing else starts it. When it starts, the program prints one line to the error stream:
`Tracy profiler started: connect a Tracy 0.14 profiler to 127.0.0.1:8086`.

**To be sure it is not listening**, in a build that has Tracy: the line above is absent, and
`netstat -an | findstr 8086` (Windows) or `ss -ltn | grep 8086` (Linux) shows nothing while the game runs. The test
suite checks the same, for the program as built:

- `game.no_listener_when_not_asked` starts the program with the switch unset and connects to the profiler's port on the
  loopback interface for the whole of its run: nothing accepts, and the program never says it started the profiler;
- `game.loopback_listener_when_asked` starts it with `--profile`: the loopback port accepts a connection, the machine's own
  network address does not, and the program says it started the profiler exactly once;
- `game.profiler_dormant`, `game.profiler_declined`, `game.profiler_asked_by_flag` and `game.profiler_asked_by_environment`
  check the same by what the program prints, including that the command line beats the environment;
- `game.tracy_binary` and `game.tracy_in_core` check that the program and the rules library carry Tracy exactly when the
  build has it (`tools/ci/check_binary_tracy.py`), and a build without it has none;
- `tracy.refused_for_EMSCRIPTEN`, `tracy.refused_for_ANDROID` and `tracy.refused_for_CMAKE_SYSTEM_NAME` run the configure check (`tools/ci/tracy_platform_check.cmake`) as a WebAssembly, an Android and an iOS build would meet it: Tracy is refused, and accepted only for a desktop or when off;
- `tools/ci/check_presets_tracy.py` (a project rule in CI) checks that no web, Android, iOS, sanitizer, coverage or fuzz
  preset has Tracy on, and the configure of such a build stops if `ICL_ENABLE_TRACY` is on (`cmake/HbmProfiling.cmake`).

The `[profile]` tests of the core check that a replay gives the same events and hashes with the profiler never started,
started and stopped: a mark only observes.

## Profiling a release

No rebuild. With a release build of the game (the one you ship, or a CI artifact):

```sh
HBM_PROFILE=1 ./heart-boom-maths          # Linux, macOS (set it in the launching environment)
heart-boom-maths.exe --profile            # Windows
```

then start the Tracy viewer, press Connect and enter `127.0.0.1`. For the start-up too, use `--profile-wait` and start
the viewer (or `tracy-capture`) within 30 seconds: the program waits before it builds anything.

Use a viewer of the **same version** as the client (0.14.x); the wire protocol changes between releases.

## Building

`ICL_ENABLE_TRACY` is on in the desktop release and debug presets (they set the vcpkg feature `profiling` too) and off
everywhere else. The feature supplies the Tracy client from `vcpkg-overlay-ports/tracy`, an overlay of the registry's
port that adds the two compile-time options the registry's lacks (`manual-lifetime`, `only-localhost`). A new Tracy
version means re-copying the registry's port at the new baseline and re-applying those lines.

With it off, every mark is empty and Tracy need not be installed.

For a trace you want the command-line tools. `windows-msvc-profile` builds them (the `profiling-tools` feature, built for
the host) and starts profiling at launch:

```sh
cmake --preset windows-msvc-profile
cmake --build --preset windows-msvc-profile
```

`linux-profile` is the same without the tools (add `profiling-tools` to `VCPKG_MANIFEST_FEATURES` to get them). The
preset is RelWithDebInfo: optimised, with symbols, the right shape for a profiler. Take numbers from it or from a
release; a Debug build is for finding where something happens, not how long it takes.

## Capturing a trace

```sh
TOOLS=build/windows-msvc-profile/vcpkg_installed/x64-windows/tools/tracy

# start the capture first; it waits for the program to appear
$TOOLS/tracy-capture -o game.tracy -a 127.0.0.1 -f &
build/windows-msvc-profile/apps/game/heart-boom-maths.exe --profile-run 90 --profile-summary summary.json

$TOOLS/tracy-csvexport game.tracy > zones.csv         # per-zone statistics: count, total, mean, min, max
$TOOLS/tracy-csvexport -e game.tracy > self.csv       # the same by self time
$TOOLS/tracy-csvexport -u game.tracy > events.csv     # every zone event
$TOOLS/tracy-csvexport -m game.tracy > messages.csv   # the messages (phases, backend, scene graph life)
$TOOLS/tracy-csvexport -u -p game.tracy > plots.csv   # zone events, then the plot samples
```

The capture ends when the program exits. Interactively, run the `tracy-profiler` GUI (0.14.x) and press Connect.

In the GUI: the frame set `UI` is the presentation cadence (one frame per presented picture); threads `main`
(the GUI thread) and the scene graph's render thread carry the zones; plots are listed below; messages mark the phases of
a scripted run and the graphics backend.

## What is instrumented

Zone names are `<layer>.<thing>`, lower case, dotted. They are what the CSV and the viewer show, so they are the stable
interface: rename one deliberately.

| Layer | Zones |
|---|---|
| core: session | `session.start_game`, `.begin_level`, `.deal_question`, `.tick`, `.answer`, `.use_hint`, `.next_level`, `.snapshot`, `.restore`, `.state_hash` |
| core: question generator | `generator.begin_level` (the deck build), `generator.next` (the deal), `generator.snapshot` |
| core: data | `json.parse`, `json.write`, `save.parse`, `save.serialise`, `save_manager.load`, `save_manager.save`, `replay.parse`, `replay.write`, `replay.run` |
| controller | `controller.construct`, `.start_game`, `.continue_game`, `.answer`, `.use_hint`, `.pause`, `.next_level`, `.save_name`, `.submit_gate`, `.on_application_state`, `.advance_time`, `.process`, `.refresh_view`, `.handle_event`, `.remember_answer`, `.finish_game`, `.sync_driver`, `.update_music`, `.persist`, `.remember_run`, `.apply_settings` |
| tick driver, feedback, gate | `tick_driver.ticks_due`, `feedback.on_correct`, `.on_wrong`, `.on_game_over`, `.hold`, `.clear`, `.read_answered`, `gate.make_challenge`, `gate.answer_matches` |
| services | `save_store.load`, `.store`, `.set_damaged_aside`; `audio.play_effect`, `.play_music`, `.set_enabled`, `.refresh_music`, `.load_players`, `.backend_play_effect`; `notices.read`; `build_info.current` |
| start-up | `main`, `startup.load_qml` |
| scene graph (render thread) | `render.sync` (the GUI thread blocked while the scene graph copies the QML scene), `render.rhi_frame` (the whole RHI frame: begin, record, submit, present), `render.frame` (the scene graph's render), `render.pass` (recording the main pass), `render.gpu` (the GPU executing the pass; Direct3D 11 only) |

Plots: `session.ticks_left`, `controller.ticks_due`, `render.frame_ms` (time between presentations), `GPU frame time (ms)`
(from the RHI's timestamps, any backend that has them), `startup.first_frame_ms`, `profile.phase` (a scripted run's phase),
and, with `--profile-sg-stats`, `qsg.nodes`, `qsg.batches`, `qsg.frame_ms`, `qsg.sync_ms`, `qsg.render_ms`, `qsg.swap_ms`
and `qsg.renderer_*_ms`. Messages: `session.alert_entered`, `graphics backend: ...`, the phases of a scripted run,
`render.gpu_zone_unavailable`. The frame set is `UI`.

### The scene graph, CPU and GPU

`RenderProfiler` (`apps/game/src/profiling/tracy/render_profiler.cpp`) connects to the window's signals
(`beforeSynchronizing` to `afterSynchronizing`, `beforeFrameBegin` to `afterFrameEnd`, `beforeRendering` to
`afterRendering`, `beforeRenderPassRecording` to `afterRenderPassRecording`, `frameSwapped`) and draws the zones above on the
render thread, which is where a GPU context must live. It is installed only while the profiler runs, or for a scripted run.

- **GPU zone.** On Direct3D 11, Qt's default on Windows, Tracy's D3D11 context is created on the device and immediate
  context Qt exposes (`QSGRendererInterface`), and timestamps are issued around the main render pass between
  `beginExternalCommands()` and `endExternalCommands()`, which replay what Qt has recorded first so that the timestamps land
  in order. Other backends get no GPU zone, only the plot. The Tracy D3D12, Metal, OpenGL and Vulkan contexts need command
  lists or descriptors that Qt does not hand out, or code for each API; they are not built, and the trace says
  `render.gpu_zone_unavailable`.
- **GPU frame time plot.** `QRhiCommandBuffer::lastCompletedGpuTime()` (Qt 6.6 and later; the RHI must be made with timestamps,
  which the profiling code asks for with `QSG_RHI_PROFILE=1`; `QRhi::Timestamps` is supported on D3D11, Metal, Vulkan and OpenGL
  with timer queries) is plotted as "GPU frame time (ms)". The RHI's header is semi-public, so a build with Tracy links Qt's
  `GuiPrivate` target.
- **Batches and nodes.** Qt exposes no API for these. `--profile-sg-stats` turns on the scene graph's own log
  (`QSG_RENDERER_DEBUG=render` and `qt.scenegraph.time.*`) and reads it (`scene_graph_log.cpp`): nodes and batches of each
  frame, and the phases of the frame, as plots and per-phase counts in the summary. The format is Qt's and not a contract,
  and the logging costs frame time, so take it in a run of its own.

## The scripted run

`--profile-run <seconds>` plays scripted games through the real controller and the real window for a fixed time, with the
real clock, and quits. `--profile-summary <file>` writes the per-phase statistics as JSON (to the output otherwise).
It needs a build with Tracy (the profile, release or debug presets) and does not start the profiler by itself: add
`--profile` for a trace. It plays on an in-memory save, so the player's save is never touched, and the sound is real.

The seconds are split between phases in proportion to their weights: the title; a level on a phone window (390 by 844);
the alert (wrong answers take time off the bomb until it is on, then the child stops); a level on a tablet window
(820 by 1180) and on a desktop window (1280 by 800); endless play at a fast pace (until level 26 or so); the top 10 on the
phone and the tablet. There are two size classes in the game (`Theme.compactMaxWidth` is 600), so the tablet and the
desktop are two shapes of the same one. The summary has, per phase: the frames presented, the idle gaps (gaps over 250 ms,
a screen that had nothing to move), the interval mean, p95, p99 and max, the dropped frames (gaps over 33.3 ms), the render
thread's work per frame (`beforeRendering` to `afterRendering`) and how often it took more than one refresh, the GPU frame
time, the visible items of the QML scene, and with `--profile-sg-stats` the nodes and batches.

## What it costs

Measured on the machine described under [The first profile](#the-first-profile), with the release preset
(`windows-msvc-release`, optimised, Tracy dormant) against the same source built with `-DICL_ENABLE_TRACY=OFF`
(no Tracy at all), 20 interleaved runs of each, medians.

| | No Tracy | Tracy, dormant (switch unset) | Tracy started, no viewer | Viewer capturing |
|---|---|---|---|---|
| One `HBM_ZONE`, ns | 0 (the macro is empty) | +0.65 | +1.7 | about 25 (Tracy's figure; not measured here) |
| One `HBM_PLOT`, ns | 0 | +0.43 | +1.3 | |
| Start-up: `--smoke-test`, process start to exit, ms | 693 | 699 (+6, inside the 32 ms spread of the runs) | 1060 (+370: starting the client and stopping it at exit) | |
| Idle at the title: working set, MB | 98.8 | 100.1 | 278.8 (Tracy's queues) | |
| Threads of the process | 114 | 114 | 117 | |
| Program size, KB | 4438 | 4511 (+73), plus `TracyClient.dll` 137 KB | | |
| A scripted 90 s run: frame interval p95 (play, tablet) ms | not available | 16.0 | 15.3 | 15.5 |

The per-mark costs are a loop of a no-inline function, best of nine runs of 50 million calls, minus the same loop without the
mark. The scripted runs differ from one another by less than the run-to-run noise (the render thread's work per frame is
0.3 ms on all three, the first frame is 0.87 to 0.95 s on all three). The game makes about 400 marks a second while it is
played (37 000 zone events in 90 s), so the dormant client costs a few hundred nanoseconds a second.

A started client with no viewer costs memory and the thread; use `--no-profile` (the default) when you are not profiling.

## The first profile

This is what a capture of the game found. It was taken once, on one machine, with other builds running on it, so read the
orders of magnitude and the ratios, not the last digit.

- Machine: AMD Ryzen 9 5950X, 96 GB, NVIDIA GeForce RTX 4090, Windows 11 Pro for Workstations 10.0.26200, one Acer X34 P
  (3440 by 1440; Qt reports a 10 ms refresh period; Qt's animation driver runs at 60 Hz, 16.7 ms).
- Renderer: Direct3D 11 through Qt's RHI, the threaded render loop.
- Build: `windows-msvc-profile` (RelWithDebInfo, MSVC 14.51), Qt 6.12.0, Tracy 0.14.1 (port version 2).
- Run: `heart-boom-maths.exe --profile-run 90`, captured with `tracy-capture` and read with `tracy-csvexport`; the scene graph's
  nodes and batches from a second run with `--profile-sg-stats`.

### Frames

Interval is the time between presentations. A screen with nothing to move draws nothing, so a pause of a quarter of a second
or more is an idle gap and is not counted in the interval; dropped is a gap of more than 33.3 ms (a missed 60 fps frame, or a
pause while the screen held still, such as the answer feedback). Work is the render thread's time recording the frame
(`beforeRendering` to `afterRendering`); GPU is `lastCompletedGpuTime()`.

| Phase | Window | s | Frames | Interval mean / p95 / p99 / max ms | Dropped | Work mean / p99 / max ms | GPU mean / p95 / max ms |
|---|---|---|---|---|---|---|---|
| Title | 390 x 844 | 3.9 | 1 | (idle) | 0 | 1.9 (the first frame) | |
| Play, level 1 to 2 | 390 x 844 | 11.7 | 641 | 18.2 / 20.8 / 51.0 / 247 | 20 | 0.32 / 0.98 / 1.4 | 0.16 / 0.21 / 2.9 |
| Alert | 390 x 844 | 5.9 | 344 | 17.1 / 20.2 / 40.2 / 70.0 | 4 | 0.34 / 0.62 / 0.74 | 0.16 / 0.16 / 2.9 |
| Play, level 3 to 5 | 820 x 1180 | 11.8 | 791 | 14.1 / 15.5 / 92.9 / 101 | 23 | 0.30 / 0.64 / 0.97 | 0.21 / 0.21 / 3.1 |
| Play, level 5 to 6 | 1280 x 800 | 11.8 | 836 | 13.7 / 14.6 / 93.1 / 116 | 21 | 0.28 / 0.59 / 1.3 | 0.21 / 0.27 / 2.8 |
| Endless, level 6 to 27 | 1280 x 800 | 39.1 | 2744 | 14.3 / 15.2 / 92.8 / 95.1 | 80 | 0.29 / 0.55 / 1.1 | 0.19 / 0.21 / 0.52 |
| Top 10 | 390 x 844 | 2.9 | 2 | (idle) | 0 | 0.43 | |
| Top 10 | 820 x 1180 | 2.9 | 1 | (idle) | 0 | 0.05 | |

The 90 ms gaps (p99 on the tablet and desktop) are the pauses between answers, when nothing animates; the work behind every
frame is under 1.5 ms of CPU and about 0.2 ms of GPU, a tenth of a 60 fps frame. The 250 ms gap in the first play phase is the
window resize and the first frame of the play screen (the RHI frame of the first draw after a resize takes 75 to 130 ms; the very first
frame of the program 130 to 230 ms: the pipelines are made then). The visible items of the QML scene: 68 on the title, 141 to 150 on
the play screen, 135 on the alert, 45 on the top 10.

### Scene graph

From `--profile-sg-stats`: the nodes and batches the renderer made for the last frame of each phase, opaque and alpha, and the
most of either in the phase.

| Screen | Opaque nodes / batches | Alpha nodes / batches | Most nodes / batches |
|---|---|---|---|
| Title | 2 / 2 | 46 / 35 | 48 / 37 |
| Play (phone) | 4 / 4 | 52 / 37 | 69 / 51 |
| Play (tablet) | 4 / 4 | 56 / 42 | 69 / 51 |
| Play (desktop) | 4 / 4 | 59 / 40 | 72 / 53 |
| Alert (phone) | 5 / 4 | 61 / 46 | 69 / 51 |
| Endless (desktop) | 4 / 4 | 65 / 44 | 69 / 48 |
| Top 10 (phone) | 2 / 2 | 16 / 11 | 31 / 15 |
| Top 10 (tablet) | 2 / 2 | 16 / 11 | 18 / 13 |

### Zones

Over the 90 s run, by total time (inclusive) and by self time, with per-call time. Times in milliseconds.

| Zone | Calls | Total | Self | Mean per call | Max |
|---|---|---|---|---|---|
| `render.rhi_frame` (render thread) | 5367 | 8936 | 7111 | 1.66 | 129 |
| `render.frame` (record the frame) | 5367 | 1609 | 858 | 0.30 | 7.8 |
| `controller.process` | 1749 | 1194 | 2 | 0.68 | 27 |
| `controller.answer` | 261 | 890 | 0.3 | 3.41 | 27 |
| `render.pass` | 5367 | 733 | 733 | 0.14 | 1.7 |
| `controller.handle_event` | 579 | 724 | 1.3 | 1.25 | 24 |
| `feedback.on_correct` | 242 | 616 | 455 | 2.54 (1.88 self) | 24 |
| `controller.refresh_view` | 1749 | 467 | 467 | 0.27 | 21 |
| `startup.load_qml` | 1 | 262 | 262 | 262 | 262 |
| `render.sync` (GUI thread blocked) | 5367 | 216 | 216 | 0.04 | 5.3 |
| `feedback.hold`, `feedback.clear` | 260, 70 | 173, 142 | 173, 142 | 0.67, 2.0 | 24, 3.6 |
| `controller.next_level` | 25 | 119 | 0 | 4.7 | 21 |

The rules library, for the same run: `session.answer` 7.9 microseconds a call (260 calls), `session.tick` 1.0 (1457),
`session.deal_question` 4.6, `generator.next` 3.1, `generator.begin_level` (the deck) 14, `session.begin_level` 20,
`save.serialise` 9 and `json.write` 2.6 per save: under 10 ms in all, in 90 s. The save to disk (`save_store.store`) happens at
the pause and the end, and is a file write of a few kilobytes.

### The three biggest costs, and what they suggest

Nothing was changed for this page; these are findings.

1. **Answering costs 3.4 ms on the GUI thread, 27 ms at worst** (`controller.answer`, 889 ms in the run): the rules take 8 microseconds of
   it, `feedback.on_correct` 2.5 ms (1.9 ms of that its own, which is the QML bindings that react to its signals), the rest the
   signals of `handle_event`. 27 ms is more than a frame at 60 fps, once at the end of a level (`controller.next_level`
   4.7 ms, 21 ms at worst). The cost is the QML side of the signals, not the game's rules; fewer or batched property signals per
   answer, or less to re-evaluate in the feedback and level-clear bindings, is where to look.
2. **Every tick refreshes the view for 0.27 ms, 21 ms at worst** (`controller.refresh_view`, 467 ms; 20 times a second, plus once per
   command): the timer and the score change signals reach QML 20 times a second. It is under 2% of a frame but it is the largest
   steady cost of the app layer; emitting only what changed (the controller already compares) and keeping the bindings that read
   `timeChanged` cheap would take most of it away.
3. **Start-up and the first draw: 0.87 to 0.95 s to the first frame** (`startup.load_qml` 262 ms of it, the QML engine and its
   components; the first frame's pipelines 130 to 230 ms; a window resize 75 to 130 ms on the next frame). On a phone the same
   work is slower. Pre-warming the pipelines, or less QML on the title screen, is the lever; the rest of the second is Qt's own start and the
   audio start (`audio.load_players`).

Not a cost: the rules library (microseconds), the render thread (0.3 ms of CPU and 0.2 ms of GPU per frame), the save. The
time in `render.rhi_frame` that is not recording (7.1 of its 8.9 seconds) is the wait for the display.

Also worth knowing: the title screen draws nothing while it is still, which is right; a started profiler with no viewer holds 180 MB
more than the same program without (Tracy's queues), so it is for profiling and not left on.

## Adding instrumentation

Include `heart_boom_maths/profile.hpp` (the core's, which the app layer uses too) and use the macros. Nothing else of
Tracy's API is used outside it, on purpose: the macros are what compile to nothing, and `tools/ci/check_core_includes.py`
fails the build if another file of `libs/core` includes Tracy.

| Macro | Use |
|---|---|
| `HBM_ZONE("layer.name")` | a zone for the rest of the C++ scope; the default choice. `HBM_ZONE_FN()` names it after the function |
| `HBM_ZONE_VALUE(n)` | attaches a number to the zone of the enclosing `HBM_ZONE` |
| `HBM_PLOT("name", n)` | a number on a timeline, integer or floating point |
| `HBM_MESSAGE("text")`, `HBM_MESSAGE_DYNAMIC(view)` | a discrete event; the first needs a literal, the second copies a string built at run time |
| `HBM_FRAME_MARK("name")` | a frame boundary of a named set |
| `HBM_THREAD_NAME("name")` | names the calling thread, once per thread |
| `HBM_INFO(view)` | text for the trace's information window |

Rules:

- **Names are string literals.** Tracy keeps the pointer.
- **Arguments are not evaluated in a build without Tracy.** Keep them free of side effects, and never let a mark be the only
  use of a variable (an unused-variable warning is an error in exactly the build nobody profiles).
- **Put a zone where there is work, not in an inner loop.** The cost is below. In the core, a zone belongs to a command,
  a level, a parse; a per-tick zone is there only because it measured as cheap.
- **A mark never decides anything.** The core's state is integers, and nothing a mark does may reach it.

## Why it is built this way

- **CMake chooses the header, not the preprocessor.** `libs/core/variants/profile-on` and `profile-off` each hold a
  `heart_boom_maths/profile.hpp`; one is put on the include path (the same way the platform directories are chosen), so no
  source asks which it got and the project's no-`#if` rule holds. The empty variant makes every macro vanish without evaluating
  its arguments; the app layer's `src/profiling/tracy` and `src/profiling/null` work the same way.
- **Built in and dormant, not a second build.** A profiler that can be turned on in the build you ship, with no rebuild and
  nothing running until then, is the only way to look at a problem on a person's machine. The price is the client library in
  the package and a flag test at each mark.
- **Loopback only.** A profiler that a shipped binary can be made to open must not be reachable from the network.
- **The core may include Tracy through one header.** It cannot read a clock or hold state, but a mark needs neither; the one
  piece of state in the header is the flag that says whether the profiler started, which no rule reads.
- **Tracy's crash handler is off** in the overlay's feature set: on Windows it replaces the unhandled-exception filter when a
  viewer connects, and does not chain to what it replaced.
- **WebAssembly, Android and iOS cannot host a listener usefully** (a page cannot open one; a phone app must not), so they
  have none; the configure refuses it.

## Licence

Tracy is BSD-3-Clause. A build that has it ships it (`TracyClient.dll` beside the program on Windows, installed with it), so
its licence text is in the notices the About screen shows and in `THIRD-PARTY-NOTICES.txt`.
