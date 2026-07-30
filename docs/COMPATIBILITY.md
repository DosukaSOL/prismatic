<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Prismatic — compatible games

A living list of Nintendo DS titles verified to run with Prismatic's HD-2D
presentation. This mirrors the in-app **Compatible Games** view
(`android/app/src/main/assets/games.json`).

> Prismatic ships **no ROMs or BIOS**. Bring your own legally-obtained dump. The
> `code` column is the 4-character cartridge code stored in the ROM header
> (offset `0x0C`); Prismatic uses it to auto-apply the recommended look.

## Nintendo DS

| Game | Region | Code | Status | Recommended look |
| --- | --- | --- | --- | --- |
| Pokémon HeartGold | USA | `IPKE` | Playable | HD-2D (2.5D + shader) |
| Pokémon SoulSilver | USA | `IPGE` | Playable | HD-2D (2.5D + shader) |
| Pokémon HeartGold | EUR | `IPKP` | Playable | HD-2D (2.5D + shader) |
| Pokémon SoulSilver | EUR | `IPGP` | Playable | HD-2D (2.5D + shader) |
| Pokémon HeartGold | JPN | `IPKJ` | Playable | HD-2D (2.5D + shader) |
| Pokémon SoulSilver | JPN | `IPGJ` | Playable | HD-2D (2.5D + shader) |

### Notes

- **"Playable"** here means the title boots and runs through the Prismatic /
  melonDS path with the recommended HD-2D profile. It is **not** a claim of a
  full, hardware-certified playthrough — see
  [Known limitations](KNOWN_LIMITATIONS.md).
- Loading a listed game auto-applies its recommended look **unless** you have a
  saved custom look active (Shader Studio → Load). Your own look always wins.
- Want a different feel? Open **Shader Studio**, tweak the 13 parameters + depth
  tilt, and **save** your look — it will re-apply after a reboot.

## Adding a game

Add an entry to `android/app/src/main/assets/games.json`:

```json
{
  "code": "IPKE",
  "title": "Pokémon HeartGold",
  "region": "USA",
  "status": "Playable",
  "preset": "HD-2D",
  "enable25D": true,
  "enableShader": true,
  "notes": "Full HD-2D pass."
}
```

`preset` must match a built-in preset name (**HD-2D**, **CRT**, **LCD**,
**Night**, **Vivid**).
