<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Testing

PRISMATIC uses a tiny first-party test harness (`tests/test_util.hpp`, macros
`CHECK`/`CHECK_EQ`/`CHECK_NEAR`) so the tested core has **no third-party
dependency**, not even a test framework. Tests are registered with CTest.

## Running

```bash
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Test inventory

| Test | Kind | What it verifies |
|---|---|---|
| `test_hash` | unit | SHA-256 against FIPS-180-4 known vectors (`""`, `"abc"`, the pangram, 1M × `a`). |
| `test_json` | unit | Parse/serialize round-trip, malformed-input rejection, recursion-depth bound (anti-DoS). |
| `test_png` | unit | Emitted PNG chunk structure + CRCs recomputed; large-image DEFLATE block splitting. |
| `test_materials` | unit | Content-driven material classification (water/foliage/path/emissive) + cache hit/override. |
| `test_profile` | unit | 10 presets present; preset & profile JSON round-trip; copyright-safe stripping; rule precedence; validation; day > night lighting; gameplay-safe camera clamp. |
| `test_pipeline` | integration | Synthetic backend structure (2 screens, ≥2 backgrounds, ≥2 sprites, framebuffer == composite); determinism; full render (upscale dims, non-trivial output, debug views); render determinism. |
| `shader_compile` | tool | Every GLSL shader compiles to SPIR-V via `glslc`. |

## Determinism

The pipeline is deterministic: `test_pipeline` renders the same frame twice and
asserts byte-identical output, and asserts the backend's framebuffer equals the
independent native compositor. This is what lets enhancement be validated without
a GPU and compared to ground truth.

## What is *not* covered by automated tests

- On-device execution (Android runtime, dual display, touch) — requires hardware.
- Real emulator adapters (mGBA/melonDS) — require user ROM/BIOS.
- GPU Vulkan rendering output — shaders are compile-checked, not pixel-diffed on a
  device GPU in this environment.

See [TEST_REPORT.md](TEST_REPORT.md) for the latest executed results with evidence
labels, and [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md).
