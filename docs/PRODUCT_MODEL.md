<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Prismatic product model

## What Prismatic is

A game-focused launcher, emulator and mod platform for user-owned classic
handheld games. The home screen is the library; every installed game gets its
own management page (Play · Mods · Camera · Performance · Saves · Save States ·
Installation · Compatibility · Diagnostics).

First supported family: **Pokémon HeartGold / SoulSilver** (one shared family,
one shared architecture).

## The gen1recomp-style workflow

```
clean ROM  +  selected mods/packages
        │ (import: hash → identify → verify)
        ▼
private generated installation   ← the clean ROM is NEVER modified
        │ (per-profile builds, each SHA-256-verified)
        ▼
launch with mods active automatically
```

Users never operate XDelta/UniPatcher manually for the Prismatic experience.
Traditional patches remain available from the canonical mod repositories for
emulator/flashcart users.

## Repository ownership

| Repository | Owns |
|---|---|
| **DosukaSOL/prismatic** | library, launcher, ROM verification, private installs, mod installer/manager, saves/states, camera & performance settings, Android app, emulator fallback, compatibility DB, diagnostics |
| **DosukaSOL/pokemon-hgss-visual-mod** | canonical Visual+ patch: XDelta/BPS releases, patch generation, camera variants, Prismatic-compatible package manifest |
| **DosukaSOL/Foldscape** | 2D→2.5D/3D game transformation and reconstruction experiments; Prismatic only *loads* Foldscape packages |
| future `pokemon-<game>-runtime-and-mods` | per-family runtime integration, mods, compatibility data |

## Package model

| Extension | Purpose |
|---|---|
| `.prismgame` | game runtime-integration package (recognition, extraction rules, adapters) |
| `.prismod` | optional game modification (manifest + per-edition artifacts, hash-pinned) |
| `.prismpatch` | binary/declarative patch applied privately to a working installation |
| `.prismtexture` | texture replacement package |
| `.foldscape` | external Foldscape scene/conversion package |

Rules: packages never contain a commercial ROM; artifacts and outputs are
SHA-256-pinned; unauthorized asset redistribution is replaced by local
extraction/derivation from the user's own ROM.

## Save integrity invariants

- One battery save per install (`saves/main.sav`), pinned across every mod
  build of that install — switching mods never forks or loses a save.
- Saves flush on `onStop` (guaranteed before process death), Save & Close and
  exit.
- Save states are separate machine snapshots under `states/`.

## Execution modes

- **Emulation (melonDS core)** — current mode for HGSS; JIT or interpreter.
- **Native runtime** — roadmap (see `docs/roadmap/`); the UI reports it
  honestly as *Not Available* until it is real.
