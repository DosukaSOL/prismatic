<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android: lower physical screen behaviour

The AYN Thor's second AMOLED panel is owned by `SecondaryPresentation` and has
exactly two modes:

| App state | Lower screen shows |
|---|---|
| Home screen / game pages / library / remap / studio | **Official full Prismatic logo** |
| DS game running (no overlay open) | **DS bottom touchscreen** (256×192 touch-mapped) |
| Lower display absent | nothing — inline single-screen layout is used |

## Logo screen

- Asset: the existing official logo `res/drawable-nodpi/prismatic_logo.png`
  (`R.drawable.prismatic_logo`) — the same asset used on the home screen. No
  new logo was generated.
- Rendering: `FIT_CENTER` (aspect preserved, never stretched or cropped),
  centred, 12 %-of-min-dimension padding, near-black `#04050A` background,
  85 % alpha — AMOLED-safe for long menu sessions.
- The logo view is not a touch surface; touches are ignored in logo mode.

## Mode switching

`MainActivity.updateLowerScreenMode()` derives the mode from UI state
(`gameLoaded && !homeVisible && no management overlay open`) and is invoked
from `updatePaused()`, which every state transition already calls — so the
lower screen can never show stale emulator output on a menu.

The presentation is dismissed in `onPause`/`onDestroy`/`exitPrismatic` and
recreated in `onResume` only when a secondary display is actually present
(no hardcoded display IDs).

## Device checks (manual, AYN Thor)

- logo on home + game pages; DS touch UI in gameplay — MANUAL REQUIRED
- rotation/reconnect of the lower display — MANUAL REQUIRED
- surface released after exit — MANUAL REQUIRED
