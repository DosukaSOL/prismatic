# Third-Party Notices

PRISMATIC includes or is designed to integrate the following third-party
components. Each retains its own license. This file is updated as dependencies
are actually vendored/linked.

## Currently in the repository (tested slice)
PRISMATIC's tested desktop core has **no vendored third-party source**. It uses
only the C++ standard library and a first-party, dependency-free PNG writer and
JSON parser written for this project. This keeps the verifiable slice free of
external license entanglements.

## Designed integrations (added when wired; not yet in-tree)

### mGBA — GB/GBC/GBA core
- Copyright © 2013–2026 Jeffrey Pfau and contributors.
- License: Mozilla Public License 2.0 (MPL-2.0).
- Source: https://github.com/mgba-emu/mgba
- Obligation: modified MPL files' source must be made available; notice
  preserved. Combining with other-licensed files in one binary is permitted.
- mGBA itself bundles: inih (BSD-3), LZMA SDK (public domain), MurmurHash3
  (public domain), getopt-for-MSVC (public domain), SQLite3 (public domain).

### melonDS — Nintendo DS/DSi core
- Copyright © melonDS contributors.
- License: GNU General Public License v3.0 (GPL-3.0).
- Source: https://github.com/melonDS-emu/melonDS
- Obligation: any distributed binary linking melonDS is GPL-3.0; complete
  corresponding source must be offered.

### Dear ImGui — engineering/dev tools UI (optional)
- Copyright © Omar Cornut and contributors. License: MIT.
- Source: https://github.com/ocornut/imgui

## Documentation references (not distributed code)
- Android SDK documentation — Google, under the Android Content License.
- GBATEK — Martin Korth, used as reference documentation only.

## Never included
No ROMs, BIOS, firmware, encryption keys, extracted game assets, official logos,
or proprietary SDK files are part of this project or its artifacts.
