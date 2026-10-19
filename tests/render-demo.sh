#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
BIN="$(mktemp "${TMPDIR:-/tmp}/windlab-demo.XXXXXX")"
trap 'rm -f "$BIN"' EXIT
c++ -std=c++17 -O2 -Wall -Wextra -pedantic -I Source tests/render-demo.cpp -o "$BIN"
mkdir -p Preview
"$BIN" Preview/WindLab_0.2_demo.wav
