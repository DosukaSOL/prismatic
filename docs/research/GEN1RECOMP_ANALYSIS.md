# gen1recomp — Technical Analysis

Source: https://github.com/bryanthaboi/gen1recomp (branch `dev`), fetched
2026-07-30. License: **MIT**. Languages reported by GitHub: C 53.4%, C++ 22.5%,
Lua 11.9%, Shell 2.5%, HTML 1.9%, Makefile 1.8%. ~883 stars, active (releases
minutes apart at time of review; latest `v0.1.43`).

## 1. What it actually is

gen1recomp is **not** an emulator, **not** an assembly transpiler, and it does
**not** ship or download a disassembly at runtime. Quoting the project README:

> "This project does not include a ROM, emulate the Game Boy, transpile
> assembly, or download a disassembly. A canonical US Poke Red or Blue ROM is
> the only game content input. The ROM is verified, used during import, and then
> released from memory. It is not copied into the cache."

It is a **hand-written engine re-creation** in Lua running on **LÖVE 11.x**
(love2d). The gameplay logic, map behavior and battle formulas are
re-implemented by hand (guided by the `pret/pokered` disassembly for
provenance), while **graphics and game data are decoded from the player's own
ROM at import time** into a private, per-user generated cache
(`data/generated`, `assets/generated`).

Classification: **hybrid engine-recreation + runtime asset extraction**, tightly
specialized to Pokémon Gen 1.

## 2. How it validates the ROM

- Accepts only the canonical 1 MiB US Red/Blue ROMs.
- Verifies **SHA-1** before creating any game data:
  - Red: `ea9bcae617fdf159b045185467ae58b2e4a48b9a`
  - Blue: `d7037c83e1ae5b39bde3c30787637ba1d4c48ce2`
- The ROM is read during import, then released; not persisted to the cache.

## 3. What it extracts vs. re-creates

| Concern | Approach |
|---|---|
| Tile/sprite graphics | **Extracted** from the verified ROM at import, cached privately |
| Audio (music/SFX/cries) | Synthesized at runtime from compact channel programs copied out of the verified ROM |
| Maps | Editable via a custom Tiled build; exported as mods |
| Game logic / battle math | **Hand-written Lua**, with selectable rulesets (`gen1_faithful` vs `modern_clean`) reproducing or removing original quirks (e.g. `oneIn256Miss`, crit-uses-base-speed) |
| 3D/voxel view | Optional presentation mods: `TILT`, `ZOOM`, `COLORS`, `GBC FX`, `VOID FILL` — a **2.5D/voxel projection of the original tiles**, covered by Polygon/Kotaku/Digital Foundry/XDA |

## 4. Reusable **architectural ideas** (not code)

1. **Runtime asset derivation + private cache** is the correct copyright-safe
   pattern: verify identity by hash, extract from the user's ROM at runtime,
   never ship or transmit assets, keep a private derived cache keyed to that
   user. **PRISMATIC adopts this exact policy** (see Fidelity Lock, cache keys).
2. **Hash-gated identity** (SHA-1/SHA-256 whitelist) before doing anything
   game-specific — mirrors PRISMATIC's profile/ROM identity requirement.
3. **Selectable behavior rulesets** decoupled from the data — analogous to
   PRISMATIC's separation of *emulation correctness* from *enhancement
   profiles*.
4. **Its voxel/TILT mode proves demand and feasibility** of a 2.5D projection
   built purely from original Gen-1 tiles — direct moral support for
   PRISMATIC's "derive geometry from original tiles" thesis.

## 5. Reusable **licensed code**

MIT permits reuse with attribution. However, the code is **Lua for LÖVE2D** and
**Gen-1-specific**; it is not architecturally compatible with PRISMATIC's
C++/Vulkan, multi-system, emulator-frontend design. **Decision: reuse the
ideas, not the code.** No gen1recomp source is vendored.

## 6. Pokémon-Red-specific implementation (does NOT generalize)

- Hand-written per-map logic and battle formulas are specific to Gen 1 and would
  have to be rewritten for every other title. This is the fundamental reason
  **engine re-creation does not scale** to "universal GB/GBC/GBA/DS support".
- The importer's hard-coded SHA-1 whitelist and ROM layout knowledge are
  Red/Blue-only.

## 7. Concepts unsuitable for PRISMATIC

- **Per-game hand-written engines**: PRISMATIC must run *arbitrary* legally
  owned games through *accurate emulator cores*, then enhance rendering. A
  recreation approach would mean re-authoring every game — the opposite of a
  universal frontend.
- **Lua/LÖVE runtime**: unsuitable for a mobile Vulkan renderer with tight frame
  pacing and NDK integration.

## 8. Lessons applicable to PRISMATIC

- Keep **emulation** (accuracy) and **enhancement** (visuals) strictly
  separate — PRISMATIC does this at the adapter boundary.
- Treat the ROM as an **identity-verified, runtime-only** asset source.
- Expose **faithful vs. stylized** toggles (their rulesets ↔ our Fidelity Lock +
  presets).

## 9. Mistakes PRISMATIC should avoid

- Do **not** describe PRISMATIC as "an extension of gen1recomp." Different
  language, license posture, scope and technique. The only shared DNA is the
  runtime-extraction/no-ship-assets ethic.
- Do **not** hard-code a single game's offsets as if universal — gen1recomp can
  because it targets exactly two ROMs; PRISMATIC must key everything to verified
  identity + versioned profiles.

## 10. Verdict

gen1recomp is an **inspirational CONCEPT reference** confirming (a) the
copyright-safe runtime-extraction model and (b) that a 2.5D projection of
original pixel tiles is compelling and feasible. Its code is not reused.
