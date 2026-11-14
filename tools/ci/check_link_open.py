#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Allow at most one call that opens a URL in apps/game.

    python3 tools/ci/check_link_open.py [--root DIR]
    python3 tools/ci/check_link_open.py --self-test

The only way out of the game is `LinkOpener::open`, reached through the parental gate (docs/COMPLIANCE.md). The check
counts every place in apps/game that can open a URL: the qualified name `LinkOpener::open`, and the system calls that do
it (QDesktopServices::openUrl, openUrlExternally in C++ or QML, ShellExecute, openURL, an ACTION_VIEW intent). The total
must not exceed one. Zero passes while the game has no link to open.

So the one call is written once, in the LinkOpener implementation, as the system call; code elsewhere calls it through
the interface (`m_Links->open(url)`), which this check does not count. Spelling `LinkOpener::open` in code as well counts
as a second call. Comments, and the tests under apps/game/tests, are not scanned.

Exit status 0 when the count is zero or one, 1 when it is more, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

GAME = Path("apps/game")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".m", ".mm", ".qml", ".js", ".java", ".kt"}
SKIPPED_DIRECTORIES = {"tests"}

PATTERNS = [
    re.compile(r"\bLinkOpener::open\b"),
    re.compile(r"\bQDesktopServices::openUrl\b"),
    re.compile(r"\bopenUrlExternally\b"),
    re.compile(r"\bShellExecute(?:Ex)?[AW]?\b"),
    re.compile(r"\bopenURL\b"),
    re.compile(r"\bIntent\.ACTION_VIEW\b"),
]
LINE_COMMENT = re.compile(r"(?<!:)//.*$")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def code_lines(text: str) -> list[str]:
    text = BLOCK_COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return [LINE_COMMENT.sub("", line) for line in text.splitlines()]


def find_calls(root: Path) -> list[str]:
    """One "file:line: text" entry per place that can open a URL."""
    game = root / GAME
    found: list[str] = []
    if not game.is_dir():
        return found
    for path in sorted(game.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        if SKIPPED_DIRECTORIES & set(path.relative_to(game).parts[:-1]):
            continue
        for number, line in enumerate(code_lines(path.read_text(encoding="utf-8", errors="replace")), start=1):
            for pattern in PATTERNS:
                for match in pattern.finditer(line):
                    found.append(f"{path.relative_to(root).as_posix()}:{number}: {match.group(0)}")
    return found


def self_test() -> int:
    def count(files: dict[str, str]) -> int:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for rel, text in files.items():
                target = root / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(text, encoding="utf-8")
            return len(find_calls(root))

    cases = [
        ("no game directory", {}, 0),
        ("no calls", {"apps/game/src/main.cpp": "int main() { return 0; }\n"}, 0),
        ("one system call", {"apps/game/src/link.cpp": "QDesktopServices::openUrl(QUrl(url));\n"}, 1),
        ("one qualified call", {"apps/game/src/a.cpp": "LinkOpener::open(url);\n"}, 1),
        ("calls through the interface are not counted", {"apps/game/src/a.cpp": "m_Links->open(url);\n"}, 0),
        ("two calls in one file", {"apps/game/src/a.cpp": "LinkOpener::open(a);\nLinkOpener::open(b);\n"}, 2),
        ("one call in each of two files",
         {"apps/game/src/a.cpp": "LinkOpener::open(a);\n", "apps/game/src/b.cpp": "QDesktopServices::openUrl(u);\n"}, 2),
        ("QML call", {"apps/game/qml/A.qml": "onClicked: Qt.openUrlExternally(url)\n"}, 1),
        ("native calls", {"apps/game/src/platform/ios/a.mm": "[[UIApplication sharedApplication] openURL:u];\n",
                          "apps/game/src/platform/windows/a.cpp": "ShellExecuteW(nullptr, L\"open\", u, 0, 0, 1);\n"}, 2),
        ("comments are ignored", {"apps/game/src/a.cpp": "// LinkOpener::open(url)\n/* QDesktopServices::openUrl(u) */\n"}, 0),
        ("code after a URL string is still read",
         {"apps/game/src/a.cpp": 'auto s = "https://example.org"; LinkOpener::open(s);\n'}, 1),
        ("tests are not scanned", {"apps/game/tests/t.cpp": "LinkOpener::open(url);\nLinkOpener::open(url);\n"}, 0),
        ("other directories are not scanned", {"libs/core/a.cpp": "LinkOpener::open(url);\nLinkOpener::open(url);\n"}, 0),
        ("a longer name is not a match", {"apps/game/src/a.cpp": "SystemLinkOpener::open(url);\nopenUrlExternallyAll();\n"}, 0),
    ]
    results = []
    for name, files, expected in cases:
        got = count(files)
        ok = got == expected
        results.append(ok)
        print(f"{'ok  ' if ok else 'FAIL'} {name}" + ("" if ok else f"  expected {expected}, got {got}"))
    return 0 if all(results) else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=".", help="repository root (default: the current directory)")
    parser.add_argument("--self-test", action="store_true", help="run the checker's own tests and exit")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    calls = find_calls(Path(args.root).resolve())
    if len(calls) > 1:
        for call in calls:
            print(call, file=sys.stderr)
        print(f"{len(calls)} places open a URL; exactly one is allowed (LinkOpener::open, behind the parental gate).",
              file=sys.stderr)
        return 1
    print(f"apps/game opens a URL in {len(calls)} place(s); at most one is allowed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
