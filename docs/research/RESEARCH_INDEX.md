# Research Index

Central catalog of primary sources consulted during Gate 1. Every entry was
fetched live during this session (dates are the fetch date, 2026-07-30, unless a
page stated its own "last updated"). Detail lives in the sibling documents.

## How to read this

Each reference is rated for **reuse**:
- **CODE** — license permits vendoring/linking into PRISMATIC.
- **CONCEPT** — study only; implement an original solution.
- **DOC** — normative documentation used to drive implementation.

## Emulator cores

| Project | URL | License | Verified | Reuse | Notes |
|---|---|---|---|---|---|
| mGBA | https://github.com/mgba-emu/mgba | MPL-2.0 | Yes (LICENSE, README) | CODE | GB/GBC/GBA; C; libretro core; graphics inspection; Lua scripting; CMake. Copyright © 2013–2026 Jeffrey Pfau. |
| melonDS | https://github.com/melonDS-emu/melonDS | GPL-3.0 | Yes (LICENSE, README) | CODE (copyleft) | DS/DSi; C++; OpenGL renderer; TODO lists "render screens to separate windows". |
| gen1recomp | https://github.com/bryanthaboi/gen1recomp | MIT | Yes (README, repo) | CONCEPT | LÖVE2D **engine re-creation** of Gen-1 Pokémon; decodes assets from a user ROM at runtime. See `GEN1RECOMP_ANALYSIS.md`. |

## Reverse-engineering / data references

| Project | URL | License | Reuse | Notes |
|---|---|---|---|---|
| pret/pokered | https://github.com/pret/pokered | (disassembly) | CONCEPT | Behavior/formula provenance; gen1recomp credits it. Not linked into PRISMATIC. |
| GBATEK | https://problemkaputt.de/gbatek.htm | doc | DOC | Canonical GBA/DS hardware reference (2D/3D engine, OAM, BG, blend, priority). |

## Android platform

| Reference | URL | Reuse | Key fact |
|---|---|---|---|
| `Presentation` | https://developer.android.com/reference/android/app/Presentation | DOC | Dialog attached to a `Display`; uses that display's own `Context`/`Resources`. Internal-display presentations only from Android 16 ("BAKLAVA"). |
| `DisplayManager` | https://developer.android.com/reference/android/hardware/display/DisplayManager | DOC | `getDisplays(DISPLAY_CATEGORY_PRESENTATION)`, `registerDisplayListener`, `DISPLAY_CATEGORY_BUILT_IN_DISPLAYS` (v36.1). |

## Renderer / shader references

See `EMULATOR_RENDERING_REFERENCES.md`, `SHADER_REFERENCE_CATALOG.md`,
`MOBILE_VULKAN_RESEARCH.md`.

## Decisions derived from this research

See `RESEARCH_DECISIONS.md` and `../ARCHITECTURE_DECISIONS.md`.
