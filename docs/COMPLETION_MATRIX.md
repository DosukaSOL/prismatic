<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Completion matrix — full-HGSS-product spec vs. reality

Statuses: **DONE** (implemented + executed, evidence cited) · **PARTIAL** ·
**NOT STARTED** · **BLOCKED-HW** (needs AYN Thor). Updated only with evidence.

| # | Requirement | Status | Evidence |
|---|---|---|---|
| 1 | Clean-ROM import, identity, verdicts | DONE | `prism identify/import`, test_game_platform |
| 2 | Private installs, pristine source | DONE | E2E on real HG/SS dumps |
| 3 | Local ROM extraction (NitroFS/NARC) | DONE | 384 files + 308 NARCs → 56,689 subfiles from real HG in 4.8 s; synthetic-fixture tests |
| 4 | Extraction → normalized runtime formats | NOT STARTED | raw extraction only; no format interpreters yet |
| 5 | Native HGSS runtime (game systems) | NOT STARTED | melonDS remains the only execution path; honest "Not Available" labels retained |
| 6 | Full-game functional parity / progression | NOT STARTED | depends on #5 |
| 7 | Region/language adapter coverage | PARTIAL | USA HG+SS verified; other retail sets header-identified in DB |
| 8 | Visual+ package (variant profiles) | DONE | hash-pinned builds, canonical release, boots |
| 9 | Visual+ independent per-component custom combo | NOT STARTED | requires patch decomposition or native hooks |
| 10 | In-game HGSS Options-menu integration | NOT STARTED | needs game-side code injection or native runtime |
| 11 | Live camera (pitch/zoom/height, presets, live sliders) | DONE (display-level) | GPU3D view-space hook; numeric + visual verification (Desktop captures); Android editor wired end-to-end |
| 12 | Per-map camera safety database | NOT STARTED | display-level adjust is uniform; safety DB pending |
| 13 | Performance Mode (no quality loss) | NOT STARTED | JIT toggle exists; no dedicated mode with equivalence proof |
| 14 | 60/90/120 Hz decoupled presentation | NOT STARTED | render loop fixed ~60 Hz |
| 15 | Saves: pinned per-install, flush guarantees | DONE | savePathOverride + onStop flush |
| 16 | Save states (slots, quick save/load) | DONE (single quick slot) | adapter savestates + JNI + page UI; multi-slot/thumbnails pending |
| 17 | Emulation fallback labeling | DONE | UI states "Emulation (melonDS)" honestly |
| 18 | Parity harness (differential, scripted, probes) | DONE | JIT-vs-interpreter: 6 checkpoints MATCH; vanilla-vs-Visual+: divergence correctly detected |
| 19 | Full-game automation | NOT STARTED | harness + pilot scripting exist as the substrate |
| 20 | Android lifecycle / Recents / lower screen | BUILT + BLOCKED-HW | device checks documented |
| 21 | Release safety | DONE | scan green every commit |

## Genuine blockers (external)

- **AYN Thor device**: all on-device validation.
- **Native runtime scale**: a genuine portable HGSS runtime (battles, scripts,
  full progression) is a large multi-milestone engineering program. The honest
  path implemented here: extraction (#3, done) → format interpreters (#4) →
  subsystem-by-subsystem runtime with the parity harness (#18, done) as the
  oracle. No milestone will be labeled done without executing evidence.
