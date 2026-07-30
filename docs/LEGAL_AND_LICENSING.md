<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Legal & licensing

## PRISMATIC's license

PRISMATIC is licensed **GPL-3.0-or-later** (see [LICENSE](../LICENSE)). This choice
is deliberate and, in part, forced by the intended emulator integrations:

- **melonDS** (the DS backend PRISMATIC is designed to adapt) is **GPL-3.0**. Any
  distributed binary that links melonDS must itself be GPL-3.0-compatible, so the
  whole frontend is GPL-3.0-or-later.
- **mGBA** (GB/GBC/GBA) is **MPL-2.0**, which is compatible with GPL-3.0.

See [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) for full upstream notices.

## Current dependency reality

The code **as built in this repository** does not yet link melonDS or mGBA — those
adapters are designed but not implemented, and no third-party source is vendored.
The tested core is entirely first-party. The GPL-3.0-or-later license is adopted
up-front so that adding those GPL/MPL backends later requires no relicensing.

| Component | License | Linked today? |
|---|---|---|
| PRISMATIC core / app | GPL-3.0-or-later | — |
| melonDS (planned DS adapter) | GPL-3.0 | No (designed) |
| mGBA (planned GB/GBA adapter) | MPL-2.0 | No (designed) |
| Dear ImGui (potential tooling UI) | MIT | No |

## ROMs, BIOS, firmware and game assets

- PRISMATIC **does not** include, download, generate, or distribute ROMs, BIOS
  images, firmware, or any copyrighted game artwork.
- Real game graphics are only ever read **at runtime** from a ROM the **user**
  legally owns and supplies to a real emulator backend they configure.
- The enhancement never fabricates or substitutes artwork; it relights the game's
  own pixels. See [SECURITY.md](SECURITY.md).
- The bundled **synthetic backend** is 100% first-party placeholder art and
  intentionally resembles no real game.

## Trademarks

"Pokémon", "SoulSilver", "Nintendo", "Game Boy", "AYN", "Thor", "Snapdragon",
"Adreno", and all related names are trademarks of their respective owners. Any
reference in this project is descriptive/nominative only. PRISMATIC is not
affiliated with, endorsed by, or sponsored by any of them.

## Your responsibilities as a user

Dumping and using ROMs/BIOS is your responsibility and is subject to the laws of
your jurisdiction. Only use content you are legally entitled to use. See
[USER_ACTIONS_REQUIRED.md](USER_ACTIONS_REQUIRED.md).
