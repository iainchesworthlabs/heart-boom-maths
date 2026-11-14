#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep libs/core free of Qt and of anything that is not the C++ standard library.

    python3 tools/ci/check_core_includes.py [--root DIR]
    python3 tools/ci/check_core_includes.py --self-test

The core is the game's rules and must run with no Qt, file access, clock or threads (CLAUDE.md, "Rules that must not be
broken"). Four rules, each reported with file:line:

  qt          no Qt header (<Q...> or <Qt...>) in any file under libs/core, tests included
  not-std     an angle-bracket include must name a C++ standard library header; a quoted include must name a header of
              the core itself (heart_boom_maths/..., a generated header, or a file next to the includer)
  forbidden   no standard header that gives access to files, a clock, threads or the standard random distributions
  cmake       no Qt target or package in a CMake file under libs/core

The tests under libs/core/tests may also include Catch2, and the text harness there may print, so `not-std` accepts
<catch2/...> and `forbidden` does not apply to them. Files that CMake generates are not scanned: only the sources are.

Exit status 0 when clean, 1 on a violation, 2 on a usage error.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

CORE = Path("libs/core")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm", ".in"}
CMAKE_NAMES = {"CMakeLists.txt"}
CMAKE_SUFFIXES = {".cmake"}

# Names the core's own headers are reached by when they are generated at configure time.
GENERATED_HEADERS = {"tuning_data.hpp", "prd_tables.hpp", "version.hpp", "save_fixtures.hpp"}

STANDARD_HEADERS = frozenset("""
algorithm any array atomic barrier bit bitset charconv chrono codecvt compare complex concepts condition_variable
coroutine deque exception execution expected filesystem flat_map flat_set format forward_list fstream functional future
generator initializer_list iomanip ios iosfwd iostream istream iterator latch limits list locale map mdspan memory
memory_resource mutex new numbers numeric optional ostream print queue random ranges ratio regex scoped_allocator
semaphore set shared_mutex source_location span spanstream sstream stack stacktrace stdexcept stdfloat stop_token
streambuf string string_view strstream syncstream system_error thread tuple type_traits typeindex typeinfo
unordered_map unordered_set utility valarray variant vector version
cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp csignal cstdarg cstddef cstdint cstdio
cstdlib cstring ctime cuchar cwchar cwctype
""".split())

# Not allowed in the library itself: files, clocks, threads, and the distributions whose results differ between
# standard libraries and so would break replays.
FORBIDDEN_IN_LIBRARY = frozenset("""
filesystem fstream chrono ctime thread mutex shared_mutex condition_variable future stop_token semaphore latch barrier
random iostream cstdio
""".split())

INCLUDE = re.compile(r'^\s*#\s*include\s*(?P<open>[<"])(?P<name>[^>"]+)[>"]')
QT_NAME = re.compile(r"^(?:.*/)?Qt?[A-Z0-9]")
CMAKE_QT = re.compile(r"\bQt[56]?(?:::|\b)|find_package\s*\(\s*Qt", re.IGNORECASE)
LINE_COMMENT = re.compile(r"(?<!:)//.*$")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.DOTALL)


def strip_comments(text: str) -> list[str]:
    """The lines of text with comments blanked, so a comment about Qt is not a finding."""
    text = BLOCK_COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return [LINE_COMMENT.sub("", line) for line in text.splitlines()]


def is_test(path: Path, root: Path) -> bool:
    return (root / CORE / "tests") in path.parents


def check_source(path: Path, root: Path) -> list[str]:
    problems: list[str] = []
    rel = path.relative_to(root).as_posix()
    in_tests = is_test(path, root)
    core_dir = root / CORE
    for number, line in enumerate(strip_comments(path.read_text(encoding="utf-8", errors="replace")), start=1):
        match = INCLUDE.match(line)
        if not match:
            continue
        name = match.group("name").strip()
        where = f"{rel}:{number}"
        if QT_NAME.match(name):
            problems.append(f"{where}: qt: {name} is a Qt header; libs/core must not use Qt")
            continue
        if match.group("open") == "<":
            if name.startswith("catch2/") and in_tests:
                continue
            if name not in STANDARD_HEADERS:
                problems.append(f"{where}: not-std: <{name}> is not a C++ standard library header")
            elif name in FORBIDDEN_IN_LIBRARY and not in_tests:
                problems.append(f"{where}: forbidden: <{name}> gives the core a file, clock, thread or random "
                                "distribution; the app layer owns those")
        else:
            own = (
                name.startswith("heart_boom_maths/")
                or name in GENERATED_HEADERS
                or (path.parent / name).resolve().is_relative_to(core_dir.resolve()) and (path.parent / name).is_file()
            )
            if not own:
                problems.append(f'{where}: not-std: "{name}" is not a header of libs/core')
    return problems


def check_cmake(path: Path, root: Path) -> list[str]:
    problems = []
    for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        code = line.split("#", 1)[0]
        if CMAKE_QT.search(code):
            problems.append(f"{path.relative_to(root).as_posix()}:{number}: cmake: libs/core must not link or find Qt")
    return problems


def find_problems(root: Path) -> list[str]:
    core = root / CORE
    if not core.is_dir():
        return [f"{CORE.as_posix()}: directory not found under {root}"]
    problems: list[str] = []
    for path in sorted(core.rglob("*")):
        if not path.is_file():
            continue
        if path.suffix in SOURCE_SUFFIXES:
            problems += check_source(path, root)
        elif path.name in CMAKE_NAMES or path.suffix in CMAKE_SUFFIXES:
            problems += check_cmake(path, root)
    return problems


def self_test() -> int:
    def write(root: Path, rel: str, text: str) -> None:
        target = root / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def run(files: dict[str, str]) -> list[str]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / CORE).mkdir(parents=True)
            for rel, text in files.items():
                write(root, rel, text)
            return find_problems(root)

    def expect(name: str, files: dict[str, str], *kinds: str) -> bool:
        problems = run(files)
        found = sorted(kind for kind in ("qt", "not-std", "forbidden", "cmake") if any(f": {kind}:" in p for p in problems))
        ok = found == sorted(kinds)
        print(f"{'ok  ' if ok else 'FAIL'} {name}" + ("" if ok else f"  expected {list(kinds)}, got {problems}"))
        return ok

    inc = "libs/core/include/heart_boom_maths/a.hpp"
    src = "libs/core/src/a.cpp"
    results = [
        expect("clean core", {inc: "#include <cstdint>\n#include <span>\n", src: '#include "heart_boom_maths/a.hpp"\n'}),
        expect("generated header", {src: '#include "tuning_data.hpp"\n#include "heart_boom_maths/tuning_data.hpp"\n'}),
        expect("local header", {src: '#include "detail.hpp"\n', "libs/core/src/detail.hpp": "#pragma once\n"}),
        expect("Qt header", {src: "#include <QString>\n"}, "qt"),
        expect("Qt module header", {src: "#include <QtCore/QObject>\n"}, "qt"),
        expect("Qt header in quotes", {src: '#include "QString"\n'}, "qt"),
        expect("Qt header in a test", {"libs/core/tests/t.cpp": "#include <QTest>\n"}, "qt"),
        expect("Qt in a comment", {src: "// #include <QString> is not allowed here\n/* #include <QString> */\n"}),
        expect("third-party header", {src: "#include <fmt/format.h>\n"}, "not-std"),
        expect("header outside the core", {src: '#include "../../../apps/game/x.hpp"\n'}, "not-std"),
        expect("missing local header", {src: '#include "nowhere.hpp"\n'}, "not-std"),
        expect("Catch2 in a test", {"libs/core/tests/t.cpp": "#include <catch2/catch_test_macros.hpp>\n"}),
        expect("Catch2 in the library", {src: "#include <catch2/catch_test_macros.hpp>\n"}, "not-std"),
        expect("file access in the library", {src: "#include <fstream>\n"}, "forbidden"),
        expect("clock in the library", {inc: "#include <chrono>\n"}, "forbidden"),
        expect("random in the library", {src: "#include <random>\n"}, "forbidden"),
        expect("iostream in a test", {"libs/core/tests/t.cpp": "#include <iostream>\n"}),
        expect("Qt in CMake", {"libs/core/CMakeLists.txt": "target_link_libraries(core PRIVATE Qt6::Core)\n"}, "cmake"),
        expect("find_package Qt", {"libs/core/CMakeLists.txt": "find_package(Qt6 REQUIRED)\n"}, "cmake"),
        expect("Qt in a CMake comment", {"libs/core/CMakeLists.txt": "# no Qt6 here\nadd_library(core)\n"}),
        expect("Qt word in a path", {"libs/core/CMakeLists.txt": "add_library(core src/quest.cpp)\n"}),
    ]
    missing = find_problems(Path(tempfile.gettempdir()) / "check-core-includes-no-such-root")
    results.append(bool(missing))
    print(f"{'ok  ' if missing else 'FAIL'} missing libs/core is reported")
    return 0 if all(results) else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=".", help="repository root (default: the current directory)")
    parser.add_argument("--self-test", action="store_true", help="run the checker's own tests and exit")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    root = Path(args.root).resolve()
    problems = find_problems(root)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{len(problems)} problem(s): libs/core must include the C++ standard library and nothing else.",
              file=sys.stderr)
        return 1
    print("libs/core includes only the C++ standard library and its own headers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
