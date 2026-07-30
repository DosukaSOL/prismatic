#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Configure and build the PRISMATIC desktop core + tests + runner.
set -euo pipefail
cd "$(dirname "$0")/.."

# Use the Android SDK's CMake if a system cmake is not on PATH.
if ! command -v cmake >/dev/null 2>&1; then
  export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"
fi

cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
echo "Build complete. Binaries in ./build."
