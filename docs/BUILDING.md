<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Building PRISMATIC (desktop core)

The desktop build compiles the first-party C++20 core, the synthetic DS backend,
the headless validation runner, and the test suite. It has **no third-party
dependencies** and needs only a C++20 compiler and CMake ≥ 3.22.

## Prerequisites

| Tool | Version used to verify | Notes |
|---|---|---|
| C++ compiler | AppleClang 21 (any C++20 compiler) | GCC 11+/Clang 14+ expected to work. |
| CMake | 3.22.1 | The Android SDK ships one at `~/Library/Android/sdk/cmake/3.22.1/bin`. |
| Make/Ninja | GNU Make (default) | Any CMake generator works. |

If `cmake` is not on your `PATH`, use the SDK copy:

```bash
export PATH="$HOME/Library/Android/sdk/cmake/3.22.1/bin:$PATH"
```

## Configure, build, test

```bash
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected: `100% tests passed, 0 tests failed out of 7`.

## Run the headless validation runner

```bash
./build/tools/headless-runner/prismatic_headless
open local_data/validation_output/report.html      # macOS; use xdg-open on Linux
```

This drives the synthetic backend across 10 scenarios (presets, times of day,
weather, a night + lantern shot, the bottom touch UI, and motion) and writes, for
each, the native composite and the enhanced result as PNGs plus an HTML index.

## Shaders (GLSL → SPIR-V)

```bash
./scripts/test_rendering.sh                 # uses the NDK glslc by default
GLSLC=/path/to/glslc ./scripts/test_rendering.sh build/shaders   # override
```

`shader_compile` is also registered as a CTest test.

## Targets

| Target | Kind | Description |
|---|---|---|
| `prismatic_core` | static lib | The enhancement core. |
| `prismatic_synthetic` | static lib | First-party DS backend fixture. |
| `prismatic_headless` | executable | Validation runner. |
| `test_*` | executables | Unit/integration tests (run via `ctest`). |

## Troubleshooting

- **`cmake: command not found`** — export the SDK CMake path shown above.
- **Old compiler** — ensure C++20 (`-std=c++20`); the build sets this itself.
- **Warnings as errors** — the build uses `-Wall -Wextra` but does *not* treat
  warnings as errors.
