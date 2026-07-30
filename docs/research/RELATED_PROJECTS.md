# Related Projects

Catalog of prior work relevant to PRISMATIC. Entries marked **[verified
2026-07-30]** were fetched live this session; entries marked **[general
knowledge — verify before reuse]** are well-known projects noted from domain
knowledge and **must be re-verified** (license, activity, API) before any code
is reused. This honesty is required by the source-evaluation rules.

## Emulator cores
- **mGBA** — GB/GBC/GBA, MPL-2.0, C. **[verified]** libretro core, Lua
  scripting, debugger, graphics inspection. Primary GB/GBC/GBA backend candidate.
- **melonDS** — DS/DSi, GPL-3.0, C++. **[verified]** OpenGL renderer; roadmap
  includes "render screens to separate windows". Primary DS backend candidate.
- **DeSmuME** — DS, GPL-2.0. **[general knowledge — verify]** Known for strong
  graphics debugging (tile/OAM/map viewers). Study as a *concept* reference for
  structured-graphics inspection.

## Emulator frontends / shader systems
- **RetroArch / libretro** — frontend + core API; **slang** shader pipeline.
  **[general knowledge — verify]** Concept reference for the adapter API and the
  preset/parameter shader model. PRISMATIC defines its **own** adapter (does not
  adopt the libretro ABI in the tested slice).
- **Mesen** — NES/SNES, HD-pack tile replacement. **[general knowledge —
  verify]** Concept reference for tile-hash-keyed replacement/override.
- **Dolphin / PPSSPP / PCSX2 / Citra** — texture replacement + graphics
  debugging. **[general knowledge — verify]** Concept references for
  content-hash texture override and layer inspection. Citra is treated as
  historical only.

## Recreation / recompilation
- **gen1recomp** — MIT, LÖVE2D engine recreation. **[verified]** See
  `GEN1RECOMP_ANALYSIS.md`. Concept reference for runtime asset extraction.
- **pret/pokered, pret/pokeheartgold** — disassemblies. **[general knowledge —
  verify]** Provenance/behavior references only; **not** linked into PRISMATIC.

## Pokémon DS tooling (for the SoulSilver profile foundation)
- Community DS tools exist for map/tile/sprite/NitroFS inspection. **[general
  knowledge — verify each project's license before use]** PRISMATIC does not
  bundle any; the profile foundation relies on runtime inspection through the DS
  adapter, not third-party editors.

## Why PRISMATIC still writes original code
The reusable cores (mGBA, melonDS) cover *emulation*. The **enhancement stack**
(structured capture → scene reconstruction → materials → lighting → renderer →
profiles) has no drop-in open-source equivalent for this exact product, so it is
implemented originally here and validated with synthetic fixtures.
