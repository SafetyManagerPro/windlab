#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/windlab-smoke.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT
for test in smoke phrase-smoke midi-learn-smoke phrase-record-smoke; do
    BIN="$TMP_DIR/$test"
    c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I Source "tests/$test.cpp" -o "$BIN"
    "$BIN"
done
