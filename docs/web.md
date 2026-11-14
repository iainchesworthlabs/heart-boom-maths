# The browser build

The game runs in a browser as a static site: a page, one script, Qt's loader and one WebAssembly module. There is no
server side, no network call and nothing to install. This page says how it is built, where the save lives, what happens
when the tab is hidden, what the download weighs, and which of those facts have been run rather than only read.

## How it is built

`cmake --preset wasm-release` builds `apps/game` with Qt's single-threaded WebAssembly kit (`wasm_singlethread`), and
leaves the finished site in `build/wasm-release/site`. Single-threaded because a multi-threaded module needs the
host to send cross-origin isolation headers, and the game runs on one thread. `docs/building.md` lists what the preset
needs in the environment.

| File | What it is |
|---|---|
| `index.html` | The page, from `apps/game/web/index.html.in` |
| `page.css`, `start.js` | The loading message, and the script that starts the module and reports a failure |
| `qtloader.js` | Qt's loader, copied from the kit |
| `heart-boom-maths.js`, `heart-boom-maths.wasm` | Emscripten's glue and the game with Qt linked in |
| `LICENSE.txt`, `notices.txt` | The licence, and the Qt, dependency and font notices About shows |

**Two Emscripten releases.** Qt 6.12.0's kit was built with Emscripten 5.0.5 and refuses to configure with any other
(`QT_EMCC_VERSION` in its `qconfig.h`). The `emscripten` pin of `.github/toolchain-versions.json` (6.0.6) is the
repository's current release, and builds the rules and the text harness, which need no Qt. The game build therefore
uses 5.0.5 until a Qt release moves; `wasm-build.yml` names it (`QT_EMSCRIPTEN_VERSION`) and checks it against the kit.

**The page.** Qt's own page is not used. It starts the game from a script inside the page, shows Qt's logo and has no
content security policy. The project's page has a loading message, a viewport for phones, a message when the module
cannot start, no pull-to-refresh (which would end a run), and a `Content-Security-Policy` that allows the page's own files
and nothing else: `default-src 'none'`, scripts and connections from `'self'`, `'wasm-unsafe-eval'` for the module and no
`unsafe-eval`. Fonts and sounds are application resources inside the module. `style-src` allows `'unsafe-inline'`
because Qt's browser platform adds a `<style>` element when it starts; no script is inline. The module is linked with
`-sDYNAMIC_EXECUTION=0`, because Embind (which Qt uses to talk to the page) otherwise builds its call stubs with
`new Function`, which the policy refuses. `tools/ci/check_web_site.py` checks the page and the built site for these rules
and for any address, font, script or beacon that leaves the origin; `ci.yml` runs it on the template on every pull
request.

A meta element cannot carry `frame-ancestors`. A host that can set headers should send the same policy as a header and
add `frame-ancestors 'none'`.

## Where the save lives

A browser has no file system that lasts: files written with `QFile` go to an in-memory file system and are gone at the
next load. In its browser build Qt makes `QSettings` use the page's `localStorage` (Qt 6.12,
`src/corelib/io/qsettings_wasm.cpp`), which holds about 5 MB for an origin. The save is a few kilobytes, so
`WebSaveStore` keeps the whole save text under one key, `save.json`, in a `KeyValueBackend` (`QSettingsBackend` in the
game). In `localStorage` the key is `qt-v0-iainchesworthlabs-heart--boom--maths-save.json`; the organisation name is
the namespace Qt gives its keys and names no person.

`SaveManager` in the core keeps the rules that are the same everywhere: a save from a newer build is never overwritten,
and a bad one starts the game on defaults. What differs on the web:

| Case | What happens |
|---|---|
| First visit | Nothing is stored; the game starts on defaults |
| Reload, or the tab closed and reopened | The save is read back: wallet, settings, top 10 and a paused run (CONTINUE) |
| A save that cannot be read | The text is copied to the key `save.json.bad` and removed from `save.json`, so the next write cannot destroy it. The browser has no file to rename, and only the latest damaged copy is kept. Clearing the site's data removes it |
| Storage blocked (cookies off, a private window, a sandboxed frame) | The page probes `localStorage` once at start. When that throws, the game keeps its save in memory (`SaveStorage::SESSION`): everything works, and nothing survives the next load. It does not crash, and the player is not told, because the game has no text for it yet |
| Quota full, or another write failure | `store()` returns false and the old text stays. The game goes on |
| Another browser or device | A separate save. There is no sync |

The probe is JavaScript in `src/platform/wasm/browser.cpp` (`EM_JS`, compiled into the script, not evaluated). Without
it, the exception of a refused `localStorage` would run through Qt's settings code, which has no handler for it. The
choice of store is `platform::save_storage()`, answered by each `src/platform/<os>/browser.cpp`, so shared code never
asks which system it is on.

Risk, not yet seen: Safari removes script-written storage of a site the person has not used for a while. A returning
child may find an empty save there. Confirm in Safari on a phone (see "Still to confirm").

## A hidden tab

Qt's browser platform does not report a hidden tab. Its code (`src/plugins/platforms/wasm` in Qt 6.12) has no
`visibilitychange` or `pagehide` handler and never reports a change of application state, so `applicationStateChanged`
does not fire when the tab goes to the background, the window is minimised or the phone locks. The game would run its
bomb on in the background.

`platform::watch_page_visibility` listens for the page's `visibilitychange` through Emscripten's own API
(`emscripten_set_visibilitychange_callback`, no script of the project's) and calls the same entry the desktop uses,
`GameController::on_application_state(false)` when the page is hidden and `(true)` when it is shown. That pauses a run,
writes the save and suspends the sound. Coming back resumes the sound and leaves the game on the pause screen. A page
opened in a background tab is reported once at the start. Off the web the function does nothing, because Qt reports the
state itself.

## Sound

Qt Multimedia is not part of the WebAssembly kit that aqtinstall installs for Qt 6.12.0 (the module list of that kit has
no `qtmultimedia`), so the browser build has no sound backend and starts silent: the log says
`audio: silent (no Qt Multimedia in this build, or no output device)`. The first-gesture filter that unlocks a browser's
sound on the first tap is in place for when a backend exists. The way forward is a decision for the owner: Qt Multimedia
built from source for the web, or a small backend over the browser's own audio, which the `AudioBackend` interface
allows without touching the rules.

## Size

The first measurement, of the build that includes this page (Qt 6.12.0, Emscripten 5.0.5, `-O3`, no sound backend):

| | Raw | Gzipped |
|---|---:|---:|
| `heart-boom-maths.wasm` | 27.2 MB | 9.3 MB |
| `heart-boom-maths.js` | 0.24 MB | 0.06 MB |
| The rest of the site | 0.07 MB | 0.02 MB |
| **Total** | **27.5 MB** | **9.4 MB** |

`wasm-build.yml` records these in the job summary of every run and fails when the gzipped total passes `WASM_GZIP_BUDGET_BYTES`,
12 MiB. That budget is a placeholder, to be confirmed by the owner. Most of the module is Qt (Quick and the QML engine,
statically linked). Options if it must shrink: `-Oz`, link-time optimisation, a Qt kit built without the features the
game does not use. A host must compress `.wasm` (GitHub Pages does); uncompressed it is 27 MB. Not measured: the time a
mid-range phone takes to show the first screen.

## Hosting

Open. The site is plain static files, so any host works. GitHub Pages is the default candidate: it is free, serves
`.wasm` with the right type and compresses it, and the repository is public. It sets no headers, so the policy stays in
the page. No deploy job exists yet; `wasm-build.yml` builds, checks and keeps the site as a run artifact for 14 days.

## What the CI checks

`.github/workflows/wasm-build.yml` is not a required check and runs when something the build reads changes, or by hand
(`gh workflow run wasm-build.yml --ref <branch>`).

| Job | Proves |
|---|---|
| WebAssembly rules (replays under Node) | The golden replays check out when the core is built with Emscripten and run under Node, and eight fixed games print the same scores and hashes as the native Linux build (`tools/ci/harness_matrix.sh`) |
| WebAssembly game (build, size) | The game builds for the browser, the site is complete and closed, and it is within the size budget |
| WebAssembly game (browser smoke test) | In the runner's Chrome the game starts with no script error or policy violation, requests only its own files, saves, and a hidden tab leaves a paused run that is still there after a reload (`tools/ci/web_smoke.mjs`) |

## Spike results (M2)

| Question | Answer | How it was found |
|---|---|---|
| Does a save written through `QSettings` survive a reload? | Yes, in Chromium (the app's browser pane) and headless Chrome | Run: the wallet and a paused run came back, and CONTINUE showed |
| With storage blocked? | The game starts on defaults, plays, pauses and "saves" in memory without an error; nothing survives the reload | Run in Chromium with `localStorage` replaced by an object that throws. Read: Qt's code (cookies off falls back to a temporary file; an exception from `localStorage` is not handled) |
| Does Qt report a hidden tab? | No | Read: Qt 6.12's wasm platform plugin. Run: the hook paused and saved the game when the page's `visibilitychange` fired (simulated by overriding `document.hidden` and dispatching the event) |
| Does audio play after the first tap? | Not answered: there is no sound backend in the web kit | Read: the kit's module list and `lib/cmake` hold no Qt Multimedia |
| Download size | 9.4 MB gzipped, 27.5 MB raw | Run: a build in WSL with Emscripten 5.0.5 |

## Still to confirm

- A real hidden tab (switching tabs, locking a phone) in Chrome, Firefox and Safari, on a phone and a desktop. The event is
  the standard one and the hook is proved with a simulated event.
- Persistence in Firefox and Safari, including Safari's removal of unused storage, and Safari's private window.
- The first-screen time on a mid-range phone.
- Touch input and the layout at phone sizes in a real mobile browser.
- That the CI jobs pass on the hosted runners (written and checked locally in WSL and on Windows, not yet run there).
