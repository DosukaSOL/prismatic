<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Visual+ integration

Canonical repository: **[DosukaSOL/pokemon-hgss-visual-mod](https://github.com/DosukaSOL/pokemon-hgss-visual-mod)** —
Prismatic consumes it and must never become a competing patch repository.

## How Prismatic consumes Visual+

1. The canonical v1.0.0 release ships, alongside the original LZMA-compressed
   xdelta3 patches, **portable variants** (`*-portable.xdelta`, pure RFC 3284,
   `xdelta3 -S none`) plus `visual-plus-hgss-1.0.0.prismod.json`.
2. Prismatic's dependency-free VCDIFF decoder (`core/src/vcdiff.cpp`,
   clean-room RFC 3284) applies them on any platform, including Android —
   validated **byte-identical to xdelta3 on all 8 artifacts** (HG + SS ×
   visual-only / safe / full / conservative-camera).
3. Every artifact is triple-pinned: patch SHA-256, required clean-ROM SHA-256,
   expected output SHA-256. A build that fails any pin is discarded.

## Profiles surfaced in the app

| Profile | Variant | Components |
|---|---|---|
| Vanilla | — | original game |
| Visual+ (Full) | `full` | backgrounds + full camera + fast HP |
| Visual+ (Conservative) | `conservative-camera` | backgrounds + subtle camera |
| Visual+ (Visuals only) | `visual-only` | backgrounds only |
| Visual+ (Safe) | `safe` | most compatibility-cautious set |

Camera choice in the Camera page maps to these variants until the native
runtime provides free camera control.

## Requirements

- Verified clean **USA** dumps only (hashes in
  `compatibility/hgss-rom-database.json`); other regions are rejected with a
  clear explanation, not silently mis-patched.
- Downloads happen on demand, are cached in app storage and re-verified before
  every use. Prismatic never downloads ROMs.
