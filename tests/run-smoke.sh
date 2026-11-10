#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/windlab-smoke.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT
for test in smoke acoustic-expression-smoke expression-smoke phrase-smoke midi-learn-smoke phrase-record-smoke phrase-midi-smoke phrase-grid-smoke breath-calibration-smoke midi-input-monitor-smoke wind-controller-profiles-smoke hybrid-midi-smoke virtual-fingering-smoke expression-097-smoke phrase-sync-smoke parameter-cc-map-smoke rooms-section-mic-smoke; do
    BIN="$TMP_DIR/$test"
    c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I Source "tests/$test.cpp" -o "$BIN"
    "$BIN"
done
