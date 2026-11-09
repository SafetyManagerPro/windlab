#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -G Xcode -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0
cmake --build build --config Release --parallel 4
ctest --test-dir build --build-config Release --output-on-failure
printf '\nBuild products are in build/WindLab_artefacts/Release/\n'
find build/WindLab_artefacts/Release -maxdepth 2 \( -name '*.vst3' -o -name '*.component' -o -name '*.app' \) -print
