<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Android: Recent-Apps dismissal and clean exit

## Symptom (reported on AYN Thor)

Prismatic could not be dismissed normally from the Recent Apps overview: the
task card would not leave the screen / the app appeared stuck.

## Root-cause analysis

Audit of `MainActivity` (branch baseline) found three contributing defects:

1. **`finishAffinity()` on Back-from-home.** This finishes activities but does
   not remove the *task*. On some launchers the dead task card lingers in
   Recents and behaves abnormally when swiped.
2. **No `onDestroy()` teardown at all.** The `AudioPlayer` thread, the
   secondary-display `Presentation` window token and the native emulator core
   were never explicitly released on task teardown, so a swipe could leave a
   half-alive process holding a window on the second display.
3. **No guaranteed save flush.** `onStop()` was not overridden; a Recents swipe
   (which does *not* guarantee `onDestroy`) could kill the process before
   battery saves reached disk.

## Fix (this branch)

- Back-on-home and the new **Exit Prismatic** actions call
  `exitPrismatic()`: flush save → stop audio → dismiss the lower-screen
  `Presentation` → persist prefs → **`finishAndRemoveTask()`**.
- `onStop()` now flushes battery saves and persists prefs — this is the last
  callback Android guarantees before the process may be killed.
- `onDestroy()` performs belt-and-braces teardown: audio stop, presentation
  dismissal, `nativeFlushSave()` + `nativeUnloadRom()`.
- `onPause()` continues to dismiss the presentation so no window token ever
  outlives the visible activity.

## Required behaviour (verify on device)

| Check | Expected | Status |
|---|---|---|
| Swipe away from Recents | card disappears normally | MANUAL REQUIRED (Thor) |
| Battery save after swipe | intact on relaunch | MANUAL REQUIRED (Thor) |
| Exit Prismatic menu item | task removed, clean relaunch | MANUAL REQUIRED (Thor) |
| Lower display after exit | released, no stale surface | MANUAL REQUIRED (Thor) |
| No zombie process | `ps` shows none after exit | MANUAL REQUIRED (Thor) |

Lifecycle logic compiles and is unit-reviewable; the five checks above need the
physical dual-screen device.
