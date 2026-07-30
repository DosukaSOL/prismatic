# PRISMATIC — Environment Report (Gate 0)

Generated during the autonomous Gate 0 environment audit. All values were read
directly from the running machine; nothing here is assumed.

## Host

| Property | Value |
|---|---|
| OS | macOS 26.5.2 (build 25F84), Darwin 25.5.0 |
| Architecture | arm64 (Apple Silicon) |
| Logical CPUs | 14 |
| RAM | 24 GiB (25,769,803,776 bytes) |
| Free disk (Desktop volume) | ~329 GiB |
| Shell | /bin/zsh |
| Git | 2.50.1 (Apple Git-155) |

## Toolchains present

| Tool | Version / Location | Notes |
|---|---|---|
| Apple clang | 21.0.0 (arm64-apple-darwin) | Used for the desktop core + test harness |
| CMake (Android SDK) | 3.22.1 at `~/Library/Android/sdk/cmake/3.22.1` | No standalone `cmake` on PATH; SDK copy is used |
| JDK | OpenJDK 17.0.18 (Zulu17.64) | Satisfies Android Gradle Plugin 8.x |
| Android SDK | `~/Library/Android/sdk` | build-tools 35/36/36.1; platforms android-36, android-36.1 |
| Android NDK | 27.1.12297006 | arm64 native builds |
| glslc (shaderc) | v2022.3 in NDK `shader-tools/darwin-x86_64` | GLSL → SPIR-V compilation |
| adb | 1.0.41 | Device deploy/diagnostics |
| Python | 3.9.6 | Report tooling / scripts |
| Node | 24.13.1 | Optional tooling |
| Homebrew | 5.1.12 | Package installs if needed |

## Toolchains NOT present (worked around)

| Missing | Impact | Mitigation |
|---|---|---|
| `cmake` on PATH | — | Use the SDK-bundled CMake 3.22.1 (scripts add it to PATH) |
| `gradle` on PATH | — | Use the Gradle **wrapper** pinned in-repo |
| `ninja` | — | CMake uses the "Unix Makefiles" generator on desktop |
| Vulkan SDK / `glslangValidator` on PATH | Cannot run Vulkan on desktop | Desktop uses a deterministic **software rasterizer**; Vulkan code targets Android and is compiled by the NDK. Shaders are validated with the NDK `glslc`. |
| `android-33/34/35` platforms | — | Compile against android-36; `minSdk` set lower for device reach |
| `cmdline-tools` / `sdkmanager` | Cannot auto-accept new licenses | Required platforms are already installed |

## Network

| Endpoint | Result |
|---|---|
| github.com | 200 (reachable) |
| services.gradle.org/distributions | 200 (Gradle wrapper can download) |
| dl.google.com maven root | 404 on directory index (normal; artifacts still resolve) |

Network is available, so Gradle/AGP dependency resolution for the Android APK is
feasible (subject to download time).

## Consequences for this build

1. The **C++ core, software renderer, headless runner, fixtures and tests** are
   fully buildable and runnable **on this machine** with clang + SDK CMake, and
   are the primary verifiable deliverables.
2. **Vulkan** cannot execute on this desktop (no ICD); Vulkan render code is
   written for Android and compiled by the NDK. Its **shaders are compiled to
   SPIR-V** here via `glslc` as a validation gate.
3. An **Android debug APK** assemble is attempted; it depends on Gradle/AGP
   downloads succeeding.
4. There is **no AYN Thor device** attached, so all AYN-Thor-specific behavior
   (dual physical displays, controller codes, thermals) is implemented against
   the documented Android APIs and validated by logic/unit tests, then flagged
   as requiring on-device confirmation in `docs/USER_ACTIONS_REQUIRED.md`.
