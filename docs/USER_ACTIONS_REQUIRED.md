<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# User actions required

Things only **you** can do — because they need physical hardware, copyrighted
files you must supply, or credentials — to move PRISMATIC past its current
blocked/deferred items.

## 1. Run the app on real hardware (unblocks on-device verification)

The APK is built but not run on a device here.

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.prismatic.app/.MainActivity
```

Please confirm, on an **AYN Thor Max** (or any Android 11+ arm64 device):

- The app launches and the top screen animates (player + follower walking).
- Preset/Time/Weather/Lantern controls change the image.
- On the Thor's **dual displays**, the DS bottom (touch UI) appears on the second
  panel; on a single display it appears as an inline overlay.
- Touch on the bottom screen registers.

Capture `adb logcat -s prismatic` output if anything fails.

## 2. Supply a legally-dumped ROM (unblocks real games / SoulSilver)

PRISMATIC ships **no** ROMs/BIOS and must never contain them. Once the real
melonDS/mGBA adapters are implemented, you will point PRISMATIC at:

- your own **DS ROM** (e.g. a cartridge you own, dumped by you), and
- any **DS BIOS/firmware** the emulator core requires, also dumped by you.

Place them under `local_data/` (git-ignored). Until you do, only the synthetic
backend runs. See [LEGAL_AND_LICENSING.md](LEGAL_AND_LICENSING.md).

## 3. Install missing SDK pieces (optional, to run an emulator here)

The reference host lacks `cmdline-tools` and any emulator system image. To run the
APK in an emulator instead of a device:

```bash
# via Android Studio SDK Manager, or sdkmanager once cmdline-tools is installed:
sdkmanager "platform-tools" "system-images;android-34;google_apis;arm64-v8a"
avdmanager create avd -n thor_test -k "system-images;android-34;google_apis;arm64-v8a"
emulator -avd thor_test
```

## 4. Provide a Vulkan-capable target (optional, for the GPU path)

The GLSL→SPIR-V shaders compile, but the runtime Vulkan presenter is not yet
implemented. Verifying it will require an Adreno device (the Thor) or a desktop
with a Vulkan ICD.

## What is NOT required from you for the current build

- No credentials, network access, or accounts are needed to build the core or the
  APK, or to run the desktop tests and the headless runner.
