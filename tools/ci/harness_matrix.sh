#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs the text harness through a fixed set of games and prints every result, so two builds of the rules (native and
# WebAssembly under Node) can be compared line for line:
#
#   tools/ci/harness_matrix.sh <replays directory> <harness command...>
#
#   tools/ci/harness_matrix.sh libs/core/tests/data/replays build/native/libs/core/heart_boom_maths_harness
#   tools/ci/harness_matrix.sh libs/core/tests/data/replays node build/wasm-core/libs/core/heart_boom_maths_harness.js
#
# The last line of a run carries the score, level and both state hashes (see the harness's usage text), so identical
# output means the same rules gave the same game, event for event. The golden replays are checked first; a run whose
# exit status is not 0 is printed with it and does not stop the matrix.
set -u

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <replays directory> <harness command...>" >&2
    exit 2
fi
replays="$1"
shift
harness=("$@")

# The header names the arguments only, so the output does not depend on how the harness is started.
run() {
    echo "== $*"
    "${harness[@]}" "$@" 2>&1
    echo "exit $?"
}

run --check "$replays"
run --auto perfect --seed 12 --quiet
run --auto perfect --seed 13 --endless-to 20 --quiet
run --auto always-wrong --seed 5 --quiet
run --auto hint-then-right --seed 3 --wallet 50 --quiet
run --auto stall 3 --seed 4 --quiet
run --auto random 7 --seed 99 --quiet
run --auto random 8 --seed 100 --think 12 --quiet
