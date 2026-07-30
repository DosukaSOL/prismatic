# Licensing Matrix

Verified during Gate 1 (2026-07-30) by reading each project's `LICENSE`/README.
This drives the integration architecture in `../ARCHITECTURE_DECISIONS.md`.

## Core dependencies considered

| Component | License | Copyleft scope | Obligations if used | Integration decision |
|---|---|---|---|---|
| **mGBA** (GB/GBC/GBA) | MPL-2.0 | **File-level** | Publish source of any modified MPL files; include notice; may combine with proprietary/other-licensed files in the same binary | **Adapter over the core as a separate module.** MPL-2.0 is GPL-compatible, so it can also live in a GPL build. |
| **melonDS** (DS/DSi) | GPL-3.0 | **Whole-program** | Any binary that links melonDS must be GPL-3.0-compatible and offer complete corresponding source | **The DS adapter + any binary linking melonDS is GPL-3.0.** PRISMATIC ships as an open-source GPL-3.0 app for the DS path; alternatively the core may be isolated in a separate process. |
| **gen1recomp** | MIT | none | Attribution | Not vendored (concept only). |
| Dear ImGui (dev tools) | MIT | none | Attribution | OK to vendor for engineering UI. |
| SDL3 (optional) | Zlib | none | Attribution | OK if used. |
| stb_image / stb_image_write | MIT/Public Domain | none | none | OK (PNG I/O in tools). Currently PRISMATIC ships its **own** dependency-free PNG writer to avoid any third-party code in the tested slice. |

## License-compatibility conclusions

1. **MPL-2.0 (mGBA) + GPL-3.0 (melonDS)**: MPL-2.0 is explicitly compatible with
   GPL. A combined GPL-3.0 distribution may include both. The **effective
   license of a build that statically links melonDS is GPL-3.0**.
2. **PRISMATIC's own first-party code** in this repository is licensed
   permissively where it stands alone, but any distributed **binary that links
   melonDS becomes GPL-3.0**. This is acceptable because PRISMATIC is an
   open-source project. Documented in `../LEGAL_AND_LICENSING.md`.
3. **Subprocess option**: To keep a permissive frontend, the DS core can be run
   as a **separate GPL-3.0 executable** communicating over IPC. This is
   implemented as an architectural *option* (the adapter is process-agnostic)
   but not required for the open-source build.

## Hard prohibitions (enforced in code + CI)

Never included in the repository or any artifact:
- ROMs, BIOS, firmware, encryption keys.
- Extracted game assets (tiles, sprites, maps, music), official logos.
- Proprietary SDK files or leaked/Nintendo source.

CI runs `scripts/check_no_secrets_or_roms.*` to scan for `*.nds/.gba/.gb/.gbc`,
BIOS/firmware blobs, and secret-like strings (see `docs/SECURITY.md`).

## First-party license choice

This repository's original code is offered under **GPL-3.0-or-later** to remain
compatible with the DS core path and to honor the strongest upstream copyleft we
depend on. See root `LICENSE`. (mGBA's MPL-2.0 and ImGui's MIT remain under
their own terms with notices in `THIRD_PARTY_NOTICES.md`.)
