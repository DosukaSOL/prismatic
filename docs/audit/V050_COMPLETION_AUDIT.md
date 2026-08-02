<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# v0.5.0 completion audit (vs. the full HGSS product spec)

Classification of every relevant v0.5.0 feature. Statuses: **EXECUTED**
(implemented + run with evidence), **BUILT** (compiles, not device-run),
**UI-BACKED** (UI wired to a real backend), **UI-ONLY** (no backend),
**EMULATOR** (works via melonDS), **BLOCKED-HW** (needs the AYN Thor).

| Feature | Status | Notes |
|---|---|---|
| Game library home + game cards | UI-BACKED, BUILT | drives GameStore JSON library |
| ROM importer (SAF, hash, identify, verdicts) | EXECUTED (desktop CLI), BUILT (Android) | real HG/SS dumps verified |
| Private installs, pristine source copy | EXECUTED | prism CLI E2E on real dumps |
| VCDIFF decoder | EXECUTED + TESTED | byte-identical to xdelta3, 8/8 real patches |
| Visual+ downloader + hash pinning | EXECUTED (desktop path), BUILT (Android download) | canonical release assets |
| Mod profiles (Vanilla/VisualOnly/Safe/Conservative/Full) | EXECUTED | hash-verified builds boot |
| Custom per-component Visual+ combination | **MISSING** | only fixed variants; no independent component selection |
| Mods page | UI-BACKED | profile switching has real backend |
| Camera page | **UI-ONLY beyond profile mapping** | no live camera backend in v0.5.0 |
| Performance page | **INFO-ONLY** | JIT state display real; no perf mode backend |
| Saves page | UI-BACKED | lists real files; pinned save path real |
| Save states page | UI-BACKED (quick slot) | adapter savestates real (desktop-verified) |
| "Native runtime: Not Available" labels | HONEST | correctly stated |
| Game execution | EMULATOR | melonDS; no native runtime exists in v0.5.0 |
| In-game HGSS Options-menu integration | **MISSING** | no game-side integration |
| High-refresh (60/90/120) presentation | **MISSING** | render loop is fixed ~60 Hz |
| Region/language adapters | PARTIAL | USA verified; others header-identified only |
| ROM extraction (NitroFS/NARC) | **MISSING** in v0.5.0 | added on this branch |
| Parity test harness | **MISSING** in v0.5.0 | added on this branch |
| Android Recents/exit fix | BUILT, BLOCKED-HW | device swipe test pending |
| Lower-screen logo / DS routing | BUILT, BLOCKED-HW | device test pending |
| Release safety scan | EXECUTED | in repo, run per release |

## Verdict

v0.5.0 is an honest emulator-backed game platform. The missing product pillars,
in dependency order: (1) local ROM extraction, (2) a runtime layer that is
game-aware beyond patch selection (live camera first), (3) parity harness,
(4) in-game options integration, (5) native execution. This branch attacks
them in that order; `docs/COMPLETION_MATRIX.md` tracks per-item evidence.
