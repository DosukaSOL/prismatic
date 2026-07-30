# Android Dual-Display Research

Primary sources (fetched 2026-07-30):
- `android.app.Presentation` — https://developer.android.com/reference/android/app/Presentation
- `android.hardware.display.DisplayManager` — https://developer.android.com/reference/android/hardware/display/DisplayManager

## Verified API facts

### Enumerating displays
- `DisplayManager` (`Context.DISPLAY_SERVICE`) provides:
  - `Display[] getDisplays()` — all logical displays.
  - `Display[] getDisplays(String category)` — filtered; results **sorted by
    preference** (first = most preferred).
  - `DISPLAY_CATEGORY_PRESENTATION` — secondary displays suitable for
    presentation (external/wireless historically).
  - `DISPLAY_CATEGORY_BUILT_IN_DISPLAYS` (**added v36.1**) — enumerate built-in
    displays specifically; note its `displayId` "is not guaranteed to be stable
    and may change when the display becomes active."
  - `getDisplay(int displayId)`, `getDisplayTopology()` (v36.1).

### Showing content on a second display
- `Presentation extends Dialog`, constructed with `(Context, Display[, theme])`.
- It **creates its own Context** from the target display and must load resources
  via the presentation's own `Context`/`Resources` (correct density/metrics).
- `show()` throws `WindowManager.InvalidDisplayException` if the display lacks
  `Display.FLAG_PRESENTATION` or a presentation is currently disallowed (e.g. it
  would occlude the app's own task).
- Auto-lifecycle: a `Presentation` is auto-cancelled when its display is removed
  or the app's task is removed; the app must pause/resume its content with the
  activity.

### CRITICAL device-dependent fact for the AYN Thor
> From the official `Presentation` docs: "starting from
> `Build.VERSION_CODES.BAKLAVA`, `Display.FLAG_PRESENTATION` can now also be
> attached to built-in **internal** displays… On earlier releases, internal
> displays were not suitable for presentations and attempting to show one would
> always result in an exception."

Implication: whether the Thor's **lower internal AMOLED** is drivable via
`Presentation` depends on Android version and the OEM's per-display flags. We
therefore **must not assume** a fixed approach. PRISMATIC implements a
**capability probe** and multiple layout strategies (see below), selecting at
runtime.

### Lifecycle / hotplug
- `registerDisplayListener(...)` with event filter bits
  `EVENT_TYPE_DISPLAY_ADDED/REMOVED/CHANGED/REFRESH_RATE/STATE` (API 36) or the
  legacy `(listener, handler)` form. Used to recover surfaces after sleep, lid
  events, activity recreation and display reconnect.
- `Display.getSupportedModes()` / `Display.Mode` expose width/height/refresh for
  per-display mode selection and frame pacing.

## Layout strategies PRISMATIC implements (chosen at runtime)

1. **Two physical displays present** (ideal Thor case): main activity renders
   the enhanced upper (gameplay) surface; a `Presentation` renders the native
   lower (touch) surface on the second display. Requires the lower display to be
   presentation-eligible on the device's Android build.
2. **Single logical display** (lower screen not exposed as a separate
   presentation display): render both DS screens into one surface using a
   configurable split (vertical stack / side-by-side / main+inset / PiP /
   hidden-with-toggle). This is the universal fallback and works everywhere.
3. **External display attached**: route the lower (or a duplicate) to the
   external presentation display.

The device probe records: display count, ids, sizes, refresh rates, density,
`FLAG_PRESENTATION` eligibility, HDR/wide-color where exposed. Everything is
logged into the diagnostics bundle (`docs/diagnostics`).

## Touch routing
- Touch on the lower surface must map surface-local coordinates → DS 256×192
  touch space. Because the lower surface may be a `Presentation` window or a
  sub-rect of one surface, the mapping is computed from the **actual** surface
  rect at layout time, never hard-coded. Implemented and unit-tested in the core
  (`TouchMapper`).

## Open device-specific unknowns (require the physical Thor)
- Whether the lower internal display is enumerated as a presentation display on
  the shipped firmware.
- The exact `displayId`s and their stability across sleep/lid.
- Controller `KeyEvent` codes and axis ranges (must be read via input probe).
These are listed in `../USER_ACTIONS_REQUIRED.md` for on-device confirmation.
