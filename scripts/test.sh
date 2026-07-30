#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build (if needed) and run the PRISMATIC test suite.
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v cmake >/dev/null 2>&1; then
  export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"
fi

cmake -S . -B build -G "Unix Makefiles" >/dev/null
cmake --build build -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null
ctest --test-dir build --output-on-failure
