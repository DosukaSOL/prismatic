# AYN Thor Research

> Honesty note: no AYN Thor Max device is attached to this build environment,
> and detailed vendor spec pages were **not** independently fetched this
> session. The characteristics below are taken from the project brief and
> general knowledge and are treated as **unverified assumptions to be confirmed
> on-device**. PRISMATIC therefore **probes everything at runtime** rather than
> hard-coding device facts (per the brief's explicit instruction).

## Assumed target characteristics (to verify on-device)
| Property | Assumed value | How PRISMATIC verifies |
|---|---|---|
| SoC | Snapdragon 8 Gen 2 | `Build`, `/proc/cpuinfo`, Vulkan `deviceName` |
| GPU | Adreno 740-class | Vulkan `VkPhysicalDeviceProperties.deviceName`, GL_RENDERER |
| RAM | 16 GB | `ActivityManager.MemoryInfo` |
| Upper display | ~1920×1080 AMOLED ≤120 Hz | `Display.getSupportedModes()` |
| Lower display | ~1240×1080 AMOLED 60 Hz touch | `DisplayManager.getDisplays()` + mode query |
| Controls | physical gamepad | `InputDevice` enumeration + probe |
| Android | 13 baseline or newer | `Build.VERSION.SDK_INT` |

## Runtime device-profile flow (implemented in the app layer, logic unit-tested)
1. On first launch, run **capability probes**: displays, input devices, GPU
   (Vulkan/GLES), memory, thermal/battery status.
2. Emit a `DeviceCapabilityReport` (JSON) into diagnostics.
3. If the report matches an **AYN Thor signature** (manufacturer/model +
   two-internal-display topology), select the Thor layout profile; otherwise a
   generic layout. The signature match is **data-driven**, created only after a
   real report is seen — no unverified hard-coding.

## Dual-display specifics
See `ANDROID_DUAL_DISPLAY_RESEARCH.md`. The lower internal display's
presentation eligibility is Android-version/OEM dependent (internal-display
`Presentation` requires Android 16 "Baklava" per official docs), so the app
supports both the two-display path and the single-surface split fallback.

## What still requires the physical device (see USER_ACTIONS_REQUIRED.md)
- Confirm display enumeration, ids, modes, and lower-screen presentation
  eligibility.
- Capture real controller key/axis codes for the mapping defaults.
- Measure sustained FPS, frame pacing, thermals, and battery under load.
