// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens the built site in a headless Chrome and checks what only a browser can show:
//
//   node tools/ci/web_smoke.mjs <site directory> [--chrome PATH] [--screenshot FILE]
//
// 1. the game starts: the loading message goes away and the game's window appears, with no script error and no
//    Content-Security-Policy violation (the policy is in index.html);
// 2. the page asks for nothing but its own files (every request goes to the test server);
// 3. the game saves into the page's localStorage, and a run paused by a hidden tab (a visibilitychange event, which is
//    what the page's hook listens to) is still there after a reload.
//
// No dependencies: Node 22 or later (its WebSocket), and the Chrome that the runner has. Chrome is found from --chrome,
// the CHROME environment variable, then the usual names. The check is a smoke test, not a UI test: it taps PLAY at the
// position of the button in an 800 by 600 window, and does not read the canvas.

import { spawn } from "node:child_process";
import { createReadStream, existsSync, mkdtempSync, rmSync, statSync, writeFileSync } from "node:fs";
import http from "node:http";
import { tmpdir } from "node:os";
import path from "node:path";
import process from "node:process";

const WIDTH = 800;
const HEIGHT = 600;
// PLAY on the title screen of the game, in a window of the size above.
const PLAY = { x: 400, y: 465 };
const SAVE_KEY = "qt-v0-iainchesworthlabs-heart--boom--maths-save.json";
const LOAD_TIMEOUT_MS = 120_000;

const MIME = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".wasm": "application/wasm",
    ".txt": "text/plain; charset=utf-8",
};

function parseArguments(argv) {
    const options = { site: null, chrome: process.env.CHROME || null, screenshot: null };
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] === "--chrome") {
            options.chrome = argv[++i];
        } else if (argv[i] === "--screenshot") {
            options.screenshot = argv[++i];
        } else if (!options.site) {
            options.site = argv[i];
        } else {
            throw new Error(`unexpected argument ${argv[i]}`);
        }
    }
    if (!options.site) {
        throw new Error("usage: web_smoke.mjs <site directory> [--chrome PATH] [--screenshot FILE]");
    }
    return options;
}

function findChrome(given) {
    const candidates = [
        given,
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        "/usr/bin/google-chrome",
        "/usr/bin/google-chrome-stable",
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    ];
    const found = candidates.find((candidate) => candidate && existsSync(candidate));
    if (!found) {
        throw new Error("no Chrome found; pass --chrome PATH or set CHROME");
    }
    return found;
}

function startServer(site) {
    const requests = [];
    const server = http.createServer((request, response) => {
        const url = new URL(request.url, "http://localhost");
        requests.push(url.pathname);
        const relative = decodeURIComponent(url.pathname === "/" ? "/index.html" : url.pathname);
        const file = path.join(site, path.normalize(relative));
        if (!file.startsWith(path.resolve(site)) || !existsSync(file) || !statSync(file).isFile()) {
            response.writeHead(404);
            response.end();
            return;
        }
        response.writeHead(200, {
            "Content-Type": MIME[path.extname(file)] ?? "application/octet-stream",
            "Cache-Control": "no-store",
        });
        createReadStream(file).pipe(response);
    });
    return new Promise((resolve) => {
        server.listen(0, "127.0.0.1", () => resolve({ server, requests, port: server.address().port }));
    });
}

function launchChrome(chrome, profile) {
    const args = [
        "--headless=new",
        "--remote-debugging-port=0",
        `--user-data-dir=${profile}`,
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-extensions",
        "--disable-background-networking",
        // Software WebGL, which Qt Quick needs and a runner has no GPU for.
        "--use-angle=swiftshader",
        "--enable-unsafe-swiftshader",
        "--ignore-gpu-blocklist",
        `--window-size=${WIDTH},${HEIGHT}`,
    ];
    if (process.platform === "linux") {
        // A runner's Chrome cannot make its sandbox without privileges; the page is the project's own.
        args.push("--no-sandbox");
    }
    args.push("about:blank");
    const child = spawn(chrome, args, { stdio: ["ignore", "ignore", "pipe"] });
    return new Promise((resolve, reject) => {
        let text = "";
        const timer = setTimeout(() => reject(new Error(`Chrome did not report a DevTools address:\n${text}`)), 30_000);
        child.stderr.on("data", (chunk) => {
            text += chunk.toString();
            const match = text.match(/DevTools listening on (ws:\/\/\S+)/);
            if (match) {
                clearTimeout(timer);
                resolve({ child, address: match[1] });
            }
        });
        child.on("exit", (code) => reject(new Error(`Chrome exited with ${code} before it was ready:\n${text}`)));
    });
}

class Devtools {
    constructor(address) {
        this.socket = new WebSocket(address);
        this.nextId = 1;
        this.pending = new Map();
        this.handlers = [];
        this.sessionId = null;
        this.ready = new Promise((resolve, reject) => {
            this.socket.addEventListener("open", resolve);
            this.socket.addEventListener("error", reject);
        });
        this.socket.addEventListener("message", (event) => {
            const message = JSON.parse(event.data);
            if (message.id && this.pending.has(message.id)) {
                const { resolve, reject } = this.pending.get(message.id);
                this.pending.delete(message.id);
                if (message.error) {
                    reject(new Error(`${message.error.message}`));
                } else {
                    resolve(message.result);
                }
            } else if (message.method) {
                this.handlers.forEach((handler) => handler(message.method, message.params));
            }
        });
    }

    send(method, params = {}, useSession = true) {
        const id = this.nextId++;
        const message = { id, method, params };
        if (useSession && this.sessionId) {
            message.sessionId = this.sessionId;
        }
        this.socket.send(JSON.stringify(message));
        return new Promise((resolve, reject) => this.pending.set(id, { resolve, reject }));
    }

    on(handler) {
        this.handlers.push(handler);
    }

    async evaluate(expression) {
        const { result, exceptionDetails } = await this.send("Runtime.evaluate", {
            expression,
            awaitPromise: true,
            returnByValue: true,
        });
        if (exceptionDetails) {
            throw new Error(`evaluating ${expression}: ${exceptionDetails.text}`);
        }
        return result.value;
    }
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function waitFor(description, condition, timeoutMs) {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
        if (await condition()) {
            return;
        }
        if (Date.now() > deadline) {
            throw new Error(`timed out waiting for ${description}`);
        }
        await sleep(250);
    }
}

async function main() {
    const options = parseArguments(process.argv.slice(2));
    const site = path.resolve(options.site);
    const failures = [];
    const fail = (text) => {
        failures.push(text);
        console.error(`FAIL ${text}`);
    };

    const { server, requests, port } = await startServer(site);
    const origin = `http://127.0.0.1:${port}`;
    const profile = mkdtempSync(path.join(tmpdir(), "hbm-smoke-"));
    const chromePath = findChrome(options.chrome);
    console.log(`Chrome: ${chromePath}`);
    const { child, address } = await launchChrome(chromePath, profile);
    const devtools = new Devtools(address);
    const problems = [];
    const urls = [];

    try {
        await devtools.ready;
        const { targetId } = await devtools.send("Target.createTarget", { url: "about:blank" }, false);
        const { sessionId } = await devtools.send("Target.attachToTarget", { targetId, flatten: true }, false);
        devtools.sessionId = sessionId;
        devtools.on((method, params) => {
            if (method === "Network.requestWillBeSent") {
                urls.push(params.request.url);
            } else if (method === "Network.loadingFailed") {
                problems.push(`a request failed: ${params.errorText} ${params.blockedReason ?? ""}`);
            } else if (method === "Runtime.exceptionThrown") {
                problems.push(`a script threw: ${params.exceptionDetails.exception?.description ?? params.exceptionDetails.text}`);
            } else if (method === "Runtime.consoleAPICalled" && params.type === "error") {
                problems.push(`console.error: ${params.args.map((a) => a.value ?? a.description).join(" ")}`);
            } else if (method === "Log.entryAdded" && params.entry.level === "error") {
                problems.push(`browser log: ${params.entry.text} ${params.entry.url ?? ""}`);
            } else if (method === "Page.javascriptDialogOpening") {
                problems.push(`a dialog opened: ${params.message}`);
            }
        });
        for (const domain of ["Page", "Runtime", "Network", "Log"]) {
            await devtools.send(`${domain}.enable`);
        }
        await devtools.send("Emulation.setDeviceMetricsOverride", {
            width: WIDTH,
            height: HEIGHT,
            deviceScaleFactor: 1,
            mobile: false,
        });

        const loaded = () =>
            devtools.evaluate(
                "document.getElementById('loading')?.style.display === 'none' && " +
                    "document.getElementById('screen')?.style.display === 'block'",
            );
        const savedText = () => devtools.evaluate(`window.localStorage.getItem(${JSON.stringify(SAVE_KEY)})`);

        // 1. The game starts.
        const started = Date.now();
        await devtools.send("Page.navigate", { url: `${origin}/index.html` });
        await waitFor("the game to start", loaded, LOAD_TIMEOUT_MS);
        console.log(`The game started in ${((Date.now() - started) / 1000).toFixed(1)} s.`);
        if (options.screenshot) {
            await sleep(500);
            const { data } = await devtools.send("Page.captureScreenshot", { format: "png" });
            writeFileSync(options.screenshot, Buffer.from(data, "base64"));
            console.log(`Screenshot: ${options.screenshot}`);
        }

        // 3. Play, hide the tab, and find the paused run in the save.
        for (const type of ["mousePressed", "mouseReleased"]) {
            await devtools.send("Input.dispatchMouseEvent", { type, x: PLAY.x, y: PLAY.y, button: "left", clickCount: 1 });
            await sleep(60);
        }
        await sleep(1500);
        await devtools.evaluate(
            "Object.defineProperty(document, 'hidden', {configurable: true, get: () => true});" +
                "Object.defineProperty(document, 'visibilityState', {configurable: true, get: () => 'hidden'});" +
                "document.dispatchEvent(new Event('visibilitychange')); true",
        );
        await waitFor("the hidden tab to save the paused run", async () => (await savedText())?.includes('"run"'), 10_000).catch(
            () => fail("a hidden tab did not leave a paused run in the save"),
        );

        // The save is still there, and the game starts from it, after a reload.
        await devtools.send("Page.reload", { ignoreCache: true });
        await waitFor("the game to start again", loaded, LOAD_TIMEOUT_MS);
        if (!(await savedText())?.includes('"run"')) {
            fail("the paused run was not in the save after a reload");
        }
    } catch (error) {
        fail(error.message);
    } finally {
        child.removeAllListeners("exit");
        child.kill();
        server.close();
        await sleep(500);
        rmSync(profile, { recursive: true, force: true, maxRetries: 5, retryDelay: 200 });
    }

    // 2. Nothing but the page's own files was asked for.
    const foreign = urls.filter((url) => !url.startsWith(origin) && !url.startsWith("data:") && !url.startsWith("blob:") && url !== "about:blank");
    foreign.forEach((url) => fail(`a request went outside the page's own origin: ${url}`));
    problems.forEach((problem) => fail(problem));
    console.log(`${requests.length} requests reached the server: ${[...new Set(requests)].join(" ")}`);

    if (failures.length > 0) {
        console.error(`${failures.length} problem(s).`);
        process.exit(1);
    }
    console.log("The site starts, asks for nothing outside itself, saves and restores a paused run.");
}

main().catch((error) => {
    console.error(error);
    process.exit(1);
});
