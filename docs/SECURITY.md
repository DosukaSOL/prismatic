<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Security & content-integrity notes

## Threat model & posture

PRISMATIC processes two kinds of untrusted input: **profile/preset JSON** and
**game-derived pixel data** from a backend. The tested core is written to be safe
against malformed input and to avoid the OWASP Top-10 classes that apply to a
local native application.

| Concern | Mitigation | Where |
|---|---|---|
| Unbounded recursion / stack exhaustion from crafted JSON | Parser enforces `kMaxDepth = 64` and rejects deeper nesting. | `core/include/prismatic/json.hpp` |
| Integer/buffer overflow in image handling | Pixel access is bounds-defined by `width*height`; sizes are `size_t`; no unchecked pointer arithmetic on external data. | `core/include/prismatic/types.hpp` |
| Malformed PNG output corrupting downstream tools | Encoder recomputes CRC32/Adler32; `test_png` re-validates every chunk. | `core/include/prismatic/png.hpp`, `tests/unit/test_png.cpp` |
| Supply-chain risk from vendored dependencies | The tested core vendors **no** third-party source; SHA-256/JSON/PNG are first-party. | `THIRD_PARTY_NOTICES.md` |
| Secrets in logs | No credentials are read or logged; the app requests no network permission. | `android/app/src/main/AndroidManifest.xml` |

## Content integrity (the core promise)

PRISMATIC must **never invent, replace, or fabricate game artwork**. This is both
an ethical/legal requirement and a correctness property:

- The renderer only ever consumes tiles, sprites and palettes that a backend
  exposes through the `EmulatorAdapter` API. It has no image-generation model and
  no bundled game art.
- Enhancement is *relighting and regrading* of those exact pixels (plus derived
  height/normal/depth fields computed from them) — never substitution.
- Profiles can be exported **copyright-safe**: `serializeProfile` strips the ROM
  SHA-256 and title so shared profiles carry no game-identifying material.
- The development **synthetic backend** contains only first-party, hand-authored
  placeholder art and explicitly approximates no copyrighted asset.

## ROMs, BIOS and firmware

PRISMATIC does not contain, download, or distribute ROMs, BIOS images, or
firmware, and never will. Real game graphics are obtained **only at runtime** from
the user's own dumped ROM through a real emulator backend the user supplies. See
[LEGAL_AND_LICENSING.md](LEGAL_AND_LICENSING.md).

## Reporting

This is a personal/engineering project without a formal disclosure process. If you
find a memory-safety or content-integrity issue, open an issue describing the
input and observed behavior (do not attach copyrighted ROM data).
