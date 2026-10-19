#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="$(mktemp "${TMPDIR:-/tmp}/windlab-smoke.XXXXXX")"
trap 'rm -f "$BIN"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I Source tests/smoke.cpp -o "$BIN"
"$BIN"
