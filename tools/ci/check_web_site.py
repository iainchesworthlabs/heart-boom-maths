#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that the browser build's page reaches nothing but its own files, and report what it weighs.

    python3 tools/ci/check_web_site.py [--root DIR]          check the page template, apps/game/web (no build needed)
    python3 tools/ci/check_web_site.py --site DIR [--budget-gzip-bytes N] [--summary FILE]
                                                              check a built site (build/wasm-release/site) and its size
    python3 tools/ci/check_web_site.py --self-test

The rules come from CLAUDE.md (no network calls, no analytics, fonts and assets from resources) and docs/web.md:

* the page carries a Content-Security-Policy whose every source is 'none', 'self', data:, blob:, and nothing that
  allows eval or another origin ('unsafe-inline' only for styles, 'wasm-unsafe-eval' only for scripts);
* every script is a file of the page, none is inline;
* no link, source, form action or style names another origin;
* the page's own script does not open a connection (fetch, XMLHttpRequest, WebSocket, sendBeacon), a window, or evaluate
  a string. Qt's loader (qtloader.js) and the compiled module are Qt's and Emscripten's code and are not read.

With --site, the files the page names and the licence and notices must exist, and the size of each file, raw and
gzipped, goes to the output and to --summary (the job summary of the web workflow). The total gzipped size is what a
first visit downloads when the host compresses, as GitHub Pages does; it must not exceed --budget-gzip-bytes.

Exit status 0 when everything holds, 1 when a rule is broken or the budget is exceeded, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import gzip
import re
import sys
import tempfile
from html.parser import HTMLParser
from pathlib import Path

WEB_DIR = Path("apps/game/web")
TEMPLATE = "index.html.in"
OWN_SCRIPT = "start.js"
OWN_STYLE = "page.css"
SITE_EXTRA_FILES = ["LICENSE.txt", "notices.txt", "qtloader.js", OWN_SCRIPT, OWN_STYLE]

ANY_SOURCE = {"'none'", "'self'", "data:", "blob:"}
URL_SCHEME = re.compile(r"^\s*(?:[a-z][a-z0-9+.-]*:|//)", re.IGNORECASE)
# What the page's own script must not do. The patterns are looked for outside comments.
FORBIDDEN_JS = [
    (re.compile(r"\bfetch\s*\("), "fetch"),
    (re.compile(r"\bXMLHttpRequest\b"), "XMLHttpRequest"),
    (re.compile(r"\bWebSocket\b"), "WebSocket"),
    (re.compile(r"\bEventSource\b"), "EventSource"),
    (re.compile(r"\bsendBeacon\b"), "sendBeacon"),
    (re.compile(r"\bwindow\.open\b|\bopen\s*\(\s*['\"]http"), "window.open"),
    (re.compile(r"\beval\s*\("), "eval"),
    (re.compile(r"\bnew\s+Function\b"), "new Function"),
    (re.compile(r"\bimportScripts\b"), "importScripts"),
    (re.compile(r"https?://", re.IGNORECASE), "an http(s) address"),
]
LINE_COMMENT = re.compile(r"(?<!:)//.*$")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def strip_js_comments(text: str) -> str:
    text = BLOCK_COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return "\n".join(LINE_COMMENT.sub("", line) for line in text.splitlines())


class PageReader(HTMLParser):
    """Collects what the checks need from the page; comments are not data."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.policies: list[str] = []
        self.scripts: list[dict[str, str | None]] = []
        self.references: list[tuple[str, str, str]] = []  # (tag, attribute, value)
        self.inline_script_text = False
        self._in_script = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = {name: value for name, value in attrs}
        if tag == "meta" and (attributes.get("http-equiv") or "").lower() == "content-security-policy":
            self.policies.append(attributes.get("content") or "")
        if tag == "script":
            self.scripts.append(attributes)
            self._in_script = True
        for attribute in ("src", "href", "action", "poster", "data", "srcset", "formaction"):
            if attributes.get(attribute) is not None:
                self.references.append((tag, attribute, attributes[attribute] or ""))

    def handle_endtag(self, tag: str) -> None:
        if tag == "script":
            self._in_script = False

    def handle_data(self, data: str) -> None:
        if self._in_script and data.strip():
            self.inline_script_text = True


def check_policy(policy: str) -> list[str]:
    problems = []
    directives: dict[str, list[str]] = {}
    for part in policy.split(";"):
        words = part.split()
        if words:
            directives[words[0].lower()] = words[1:]
    if "default-src" not in directives:
        problems.append("the policy has no default-src")
    elif directives["default-src"] != ["'none'"]:
        problems.append("default-src must be 'none', so a source that is not named is refused")
    for name, sources in directives.items():
        for source in sources:
            allowed = source in ANY_SOURCE
            allowed = allowed or (source == "'unsafe-inline'" and name == "style-src")
            allowed = allowed or (source == "'wasm-unsafe-eval'" and name == "script-src")
            if not allowed:
                problems.append(f"{name} allows {source}")
    for needed in ("script-src", "connect-src"):
        if needed not in directives:
            problems.append(f"the policy has no {needed}")
    if directives.get("connect-src", ["'self'"]) != ["'self'"]:
        problems.append("connect-src must be 'self' alone")
    if directives.get("script-src", []) and "'self'" not in directives["script-src"]:
        problems.append("script-src must allow 'self'")
    return problems


def check_page(text: str) -> list[str]:
    reader = PageReader()
    reader.feed(text)
    problems = []
    if len(reader.policies) != 1:
        problems.append(f"the page needs exactly one Content-Security-Policy meta element, it has {len(reader.policies)}")
    else:
        problems += check_policy(reader.policies[0])
    for script in reader.scripts:
        source = script.get("src")
        if not source:
            problems.append("a script is inline; every script must be a file (src=...)")
        elif URL_SCHEME.match(source):
            problems.append(f"a script comes from another origin: {source}")
    if reader.inline_script_text:
        problems.append("a script element has inline code")
    for tag, attribute, value in reader.references:
        if tag == "script":
            continue # read above
        if URL_SCHEME.match(value) and not value.strip().lower().startswith(("data:", "blob:")):
            problems.append(f"<{tag} {attribute}> names another origin: {value}")
    return problems


def check_style(text: str) -> list[str]:
    problems = []
    text = BLOCK_COMMENT.sub("", text)
    if re.search(r"@import\b", text):
        problems.append("the style sheet imports another file")
    for match in re.finditer(r"url\(\s*['\"]?([^'\")\s]+)", text, re.IGNORECASE):
        if URL_SCHEME.match(match.group(1)) and not match.group(1).lower().startswith(("data:", "blob:")):
            problems.append(f"the style sheet names another origin: {match.group(1)}")
    return problems


def check_script(text: str) -> list[str]:
    code = strip_js_comments(text)
    return [f"{OWN_SCRIPT} uses {label}" for pattern, label in FORBIDDEN_JS if pattern.search(code)]


def check_sources(web_dir: Path, page_name: str) -> list[str]:
    problems = []
    for name, check in ((page_name, check_page), (OWN_STYLE, check_style), (OWN_SCRIPT, check_script)):
        path = web_dir / name
        if not path.is_file():
            problems.append(f"{path} is missing")
            continue
        problems += [f"{name}: {problem}" for problem in check(path.read_text(encoding="utf-8"))]
    return problems


def gzip_size(path: Path) -> int:
    return len(gzip.compress(path.read_bytes(), compresslevel=9, mtime=0))


def format_size(count: int) -> str:
    return f"{count:,} ({count / 1_048_576:.2f} MiB)"


def check_site(site: Path, budget: int | None) -> tuple[list[str], str]:
    problems = check_sources(site, "index.html")
    summary = ["| File | Raw (bytes) | Gzipped (bytes) |", "|---|---:|---:|"]
    page = site / "index.html"
    modules: list[str] = []
    if page.is_file():
        reader = PageReader()
        reader.feed(page.read_text(encoding="utf-8"))
        modules = [str(s["src"]) for s in reader.scripts if s.get("src") and s["src"] not in ("qtloader.js", OWN_SCRIPT)]
        if len(modules) != 1:
            problems.append(f"index.html should load exactly one module script, it loads {modules}")
    required = ["index.html", *SITE_EXTRA_FILES]
    for module in modules:
        required += [module, module[: -len(".js")] + ".wasm"]
    total_raw = total_gzip = 0
    for name in required:
        path = site / name
        if not path.is_file():
            problems.append(f"{name} is missing from the site")
            continue
        raw, packed = path.stat().st_size, gzip_size(path)
        total_raw += raw
        total_gzip += packed
        summary.append(f"| `{name}` | {raw:,} | {packed:,} |")
    for path in sorted(site.iterdir()):
        if path.name not in required:
            problems.append(f"{path.name} is in the site and is not one of its files")
    summary.append(f"| **Total** | **{total_raw:,}** | **{total_gzip:,}** |")
    lines = ["\n".join(summary), "", f"Download on a first visit, gzipped: {format_size(total_gzip)}."]
    if budget is not None:
        within = total_gzip <= budget
        lines.append(f"Budget: {format_size(budget)}; {'within it' if within else 'EXCEEDED'}.")
        if not within:
            problems.append(f"the gzipped download is {total_gzip:,} bytes, over the budget of {budget:,}")
    return problems, "\n".join(lines)


def self_test() -> int:
    good_page = """<!doctype html><html><head>
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; base-uri 'none'">
<link rel="icon" href="data:,"><link rel="stylesheet" href="page.css"></head><body>
<script src="app.js" defer></script><script src="qtloader.js" defer></script><script src="start.js" defer></script>
</body></html>"""

    def page_problems(text: str) -> int:
        return len(check_page(text))

    def with_policy(policy: str) -> str:
        return re.sub(r'content="[^"]*"', f'content="{policy}"', good_page, count=1)

    cases = [
        ("a good page", lambda: page_problems(good_page), 0),
        ("no policy", lambda: page_problems(good_page.replace("Content-Security-Policy", "x-nothing")), 1),
        ("eval allowed", lambda: page_problems(with_policy("default-src 'none'; script-src 'self' 'unsafe-eval'; connect-src 'self'")), 1),
        ("another origin in a policy",
         lambda: page_problems(with_policy("default-src 'none'; script-src 'self'; connect-src 'self' https://x.example")), 2),
        ("default-src open", lambda: page_problems(with_policy("default-src 'self'; script-src 'self'; connect-src 'self'")), 1),
        ("no connect-src", lambda: page_problems(with_policy("default-src 'none'; script-src 'self'")), 1),
        ("inline script", lambda: page_problems(good_page.replace("</body>", "<script>alert(1)</script></body>")), 2),
        ("script from elsewhere",
         lambda: page_problems(good_page.replace('src="app.js"', 'src="https://cdn.example/app.js"')), 1),
        ("protocol-relative stylesheet",
         lambda: page_problems(good_page.replace('href="page.css"', 'href="//cdn.example/p.css"')), 1),
        ("a font link", lambda: page_problems(good_page.replace("</head>", '<link href="https://fonts.example/f.css" rel="stylesheet"></head>')), 1),
        ("a good style", lambda: len(check_style("body { background: url(data:image/png;base64,AAA); }")), 0),
        ("an imported style", lambda: len(check_style('@import "https://x.example/a.css";')), 1),
        ("a remote image in a style", lambda: len(check_style("a { background: url('https://x.example/a.png') }")), 1),
        ("a good script", lambda: len(check_script("// fetch( is fine in a comment\nqtLoad({});\n")), 0),
        ("fetch in a script", lambda: len(check_script("fetch('x');")), 1),
        ("a beacon and an address", lambda: len(check_script("navigator.sendBeacon('https://x.example');")), 2),
        ("eval in a script", lambda: len(check_script("eval('1');")), 1),
    ]

    def site_case(extra: dict[str, str], budget: int | None) -> int:
        with tempfile.TemporaryDirectory() as tmp:
            site = Path(tmp)
            files = {
                "index.html": good_page,
                "page.css": "body { margin: 0; }",
                "start.js": "qtLoad({});",
                "qtloader.js": "// loader",
                "app.js": "// module",
                "app.wasm": "wasm",
                "LICENSE.txt": "licence",
                "notices.txt": "notices",
            }
            files.update(extra)
            for name, text in files.items():
                if text is not None:
                    (site / name).write_text(text, encoding="utf-8")
            return len(check_site(site, budget)[0])

    cases += [
        ("a good site", lambda: site_case({}, 10_000), 0),
        ("a site missing its module", lambda: site_case({"app.wasm": None}, None), 1),
        ("a site with a stray file", lambda: site_case({"tracker.js": "x"}, None), 1),
        ("a site over budget", lambda: site_case({}, 10), 1),
    ]
    results = []
    for name, run, expected in cases:
        got = run()
        ok = got == expected
        results.append(ok)
        print(f"{'ok  ' if ok else 'FAIL'} {name}" + ("" if ok else f"  expected {expected} problem(s), got {got}"))
    return 0 if all(results) else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=".", help="repository root (default: the current directory)")
    parser.add_argument("--site", help="a built site folder to check and weigh, for example build/wasm-release/site")
    parser.add_argument("--budget-gzip-bytes", type=int, help="fail when the gzipped total is larger")
    parser.add_argument("--summary", help="write the size table here (the job summary file)")
    parser.add_argument("--self-test", action="store_true", help="run the checker's own tests and exit")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if args.site:
        site = Path(args.site)
        if not site.is_dir():
            print(f"{site} is not a folder", file=sys.stderr)
            return 2
        problems, report = check_site(site, args.budget_gzip_bytes)
        print(report)
        if args.summary:
            with open(args.summary, "a", encoding="utf-8") as handle:
                handle.write(report + "\n")
    else:
        web_dir = Path(args.root).resolve() / WEB_DIR
        if not web_dir.is_dir():
            print(f"{WEB_DIR} does not exist: no web page to check.")
            return 0
        problems = check_sources(web_dir, TEMPLATE)
    for problem in problems:
        print(f"FAIL {problem}", file=sys.stderr)
    if problems:
        return 1
    print("The page reaches nothing outside its own files." if not args.site else "The site is complete and closed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
