<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Building the PRISMATIC Android app

The Android app (`android/`) is a Kotlin frontend that renders the enhanced DS
screens on-device. Its native library **reuses the exact same first-party
`prismatic_core` and synthetic backend** that are built and tested on desktop,
compiled for `arm64-v8a` through the NDK. This means the on-device image is
produced by the identical, already-validated pipeline.

## Prerequisites (versions verified on the build host)

| Component | Version | How it is provided |
|---|---|---|
| JDK | 17 (Zulu 17) | `/usr/libexec/java_home -v 17` |
| Android Gradle Plugin | 8.7.2 | declared in `android/build.gradle.kts` |
| Gradle | 8.9 | via the committed wrapper (`./gradlew`) |
| Kotlin | 1.9.24 | declared in `android/build.gradle.kts` |
| Android SDK Platform | android-36 | `~/Library/Android/sdk/platforms/android-36` |
| Build-tools | 36.0.0 | `~/Library/Android/sdk/build-tools/36.0.0` |
| NDK | 27.1.12297006 | `~/Library/Android/sdk/ndk/27.1.12297006` |
| CMake (NDK) | 3.22.1 | `~/Library/Android/sdk/cmake/3.22.1` |

`android/local.properties` must point `sdk.dir` at your Android SDK. A sample is
committed for the reference host; edit it for your machine (it is git-ignored so
your edit will not be tracked).

## Build the debug APK

```bash
cd android
JAVA_HOME="$(/usr/libexec/java_home -v 17)" ./gradlew :app:assembleDebug
```

Output:

```
android/app/build/outputs/apk/debug/app-debug.apk
```

The build compiles the native `.so` (`buildCMakeDebug[arm64-v8a]`), the Kotlin
sources, and packages a launchable APK. The reference build produced a 4.08 MB
APK containing `lib/arm64-v8a/libprismaticnative.so` (a valid AArch64 ELF). See
[TEST_REPORT.md](TEST_REPORT.md) for the exact size and SHA-256.

## Install and run

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.prismatic.app/.MainActivity
```

### Controls

- **Tap left / right half of the top screen** — previous / next preset.
- **Preset ± / Time / Weather / Lantern buttons** — cycle enhancement settings.
- **Bottom screen (touch UI)** — forwarded to the backend as DS touch input.

### Dual display

On a device that exposes a second display (e.g. the AYN Thor Max's second panel),
the DS bottom screen is shown there via an `android.app.Presentation`. On a
single-display device the bottom screen is shown inline as a picture-in-picture
overlay.

## Honesty note

The APK is **built and packaged** on the reference host but has **not been run on
an AYN Thor Max or any physical device** in this environment (no hardware
attached, and no emulator system image is installed). The dual-display routing
and the Vulkan-optimised render path are **designed and compiled where possible
but not device-verified**. Confirm on real hardware. See
[USER_ACTIONS_REQUIRED.md](USER_ACTIONS_REQUIRED.md).

## Notes on host configuration

- Only the `android-36` platform is installed here, so `compileSdk = 36`; the
  advisory about an un-vetted compileSdk is silenced in `gradle.properties`
  (`android.suppressUnsupportedCompileSdk=36`). If you have `android-34`/`35`
  installed you may lower `compileSdk` accordingly.
- The app uses **only platform APIs** (no AndroidX/Jetpack), which keeps the
  dependency surface — and the build — minimal.
