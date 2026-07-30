<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Third-party notices

PRISMATIC's enhancement **core** vendors no third-party source — SHA-256, JSON,
and PNG are implemented from scratch. The optional **Nintendo DS backend** links
one external project, included as a pinned git **submodule** (not copied into
this tree):

## melonDS

- **Project:** melonDS — a Nintendo DS emulator
- **Upstream:** https://github.com/melonDS-emu/melonDS
- **Location here:** `third_party/melonDS` (git submodule)
- **Pinned commit:** `b3dd9880` (0.8.1-1802-gb3dd9880)
- **License:** GNU General Public License v3.0 or later (GPL-3.0-or-later)

melonDS bundles its own dependencies (teakra, xxhash, fatfs, tiny-AES-c,
blip-buf, and Dolphin-derived helpers); their licenses apply as distributed by
melonDS upstream. See the submodule's `LICENSE` and source headers.

PRISMATIC is licensed GPL-3.0-or-later, compatible with melonDS.

### Fetching the submodule

The submodule is **not** part of a plain source download. After cloning:

```sh
git submodule update --init --recursive
```

The DS backend is built only when `-DPRISMATIC_ENABLE_MELONDS=ON` (desktop) or as
part of the Android app build. No BIOS, firmware, or ROM is bundled; melonDS runs
on its built-in FreeBIOS + generated firmware, and the user supplies their own
`.nds` files at runtime.
