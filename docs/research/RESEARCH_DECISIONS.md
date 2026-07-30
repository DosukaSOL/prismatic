# Research Decisions

Decisions taken from Gate 1 research, with rationale. These feed
`../ARCHITECTURE_DECISIONS.md`.

## D-R1 — Do not recreate games; emulate + enhance
gen1recomp proves per-game recreation is high-fidelity but **does not scale**
(hand-written per game). PRISMATIC's universal goal requires **accurate emulator
cores** + a **separate enhancement layer**. → adapter architecture.

## D-R2 — Backends: mGBA (GB/GBC/GBA) + melonDS (DS)
Both are mature, active, and expose (or can expose) the structured graphics and
dual-screen state we need. Licenses verified. → mGBA MPL-2.0 module; melonDS
GPL-3.0 module.

## D-R3 — Adopt runtime-extraction + no-ship-assets ethic
From gen1recomp: verify identity by hash, derive visuals from the user's ROM at
runtime, keep only a **private derived cache**, never ship/transmit assets. →
Fidelity Lock + copyright-safe export + cache keys.

## D-R4 — First-party code is GPL-3.0-or-later
Because a distributed binary linking melonDS is GPL-3.0, and to honor the
strongest upstream copyleft, PRISMATIC's own code is GPL-3.0-or-later. mGBA
(MPL-2.0) and any MIT deps keep their terms with notices. → root `LICENSE`,
`../LEGAL_AND_LICENSING.md`.

## D-R5 — Probe the device; hard-code nothing
Official Android docs confirm internal-display `Presentation` support is
version-dependent (Android 16 "Baklava") and built-in display ids may be
unstable. → runtime capability probes + multiple layout strategies; AYN Thor
profile only after a real report.

## D-R6 — Renderer: own Vulkan (1.1) + GLES 3.1 fallback, no big engine
Tight control of surfaces/pacing/pipeline-cache/passes is required for dual
display + emulator timing. → custom renderer; shaders compiled with NDK `glslc`.

## D-R7 — Validate now with synthetic fixtures + software renderer
Because no ROM, no Vulkan ICD, and no Thor are available here, the **entire
enhancement pipeline** is validated deterministically on the CPU with synthetic
DS-like fixtures, producing screenshots + an HTML report. Real-GPU and real-ROM
validation are explicitly device/ROM-blocked and documented.

## D-R8 — Keep the libretro ABI as a future option, not a dependency
The adapter is *inspired by* libretro's core/frontend split but is a native
capability-flagged C++ interface tuned for structured capture + dual display.
