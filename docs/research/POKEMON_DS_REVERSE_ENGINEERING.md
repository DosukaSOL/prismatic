# Pokémon DS Reverse-Engineering Notes

Scope: only what PRISMATIC needs to build the **SoulSilver profile foundation**
(identity + environment tags + rule precedence), using **runtime inspection**
through the DS adapter. **No copyrighted data, offsets dumps, or extracted
assets are stored in this repository.** Detailed offsets belong in a user-local,
git-ignored profile derived on the user's own machine from their own ROM.

## Identity (implemented, testable without a ROM)
- DS ROM header exposes an **internal game code** (e.g. `IPG*` family for
  HGSS-era titles) and maker code at fixed header offsets, plus a title string.
- PRISMATIC identifies a game by **SHA-256 of the whole ROM** (primary) and
  cross-checks header game-code + region + revision. **Never** assume offsets
  from one revision apply to another — the profile stores per-(hash) rules.
- The identity/verification logic is implemented and unit-tested against
  **synthetic headers** (no real ROM needed).

## Data the DS adapter is expected to surface (via melonDS)
- 2D engine: BG control/scroll, tile maps, tile graphics, palettes, OAM, window,
  blend, priority — per engine (main/sub).
- 3D engine: geometry commands / captured color(+depth) for the overworld.
- Screen routing: which engine drives which physical screen (games swap this).

## Environment tagging model (implemented)
PRISMATIC does **not** need to decode NitroFS map formats to be useful. The
profile associates a **map identifier** (surfaced by the adapter or, for the
foundation, a synthetic map id) with an **environment tag** (Town/Route/Forest/
Cave/Interior/Water/Battle/Menu/…) and per-tag lighting/fog/camera policy. Map
identifiers and tile classifications for a specific ROM are authored into a
**user-local profile** — never shipped.

## GBATEK
GBATEK (https://problemkaputt.de/gbatek.htm) is the canonical hardware reference
for the GBA/DS 2D/3D engines, OAM/BG/affine/window/blend/priority semantics used
to interpret adapter-captured state. Used as **DOC**, not code.

## Explicitly out of scope here
- Shipping HGSS map tables, tile IDs, model IDs, or sprites.
- Any claim of SoulSilver compatibility without the acceptance matrix in
  `../SOULSILVER_VALIDATION.md` being executed against a user ROM.
