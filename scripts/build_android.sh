#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the PRISMATIC Android debug APK using JDK 17 and the committed wrapper.
set -euo pipefail
cd "$(dirname "$0")/../android"

JH="$(/usr/libexec/java_home -v 17 2>/dev/null || echo "${JAVA_HOME:-}")"
if [[ -z "$JH" ]]; then
  echo "ERROR: JDK 17 not found. Set JAVA_HOME to a JDK 17 install." >&2
  exit 2
fi

JAVA_HOME="$JH" ANDROID_HOME="${ANDROID_HOME:-$HOME/Library/Android/sdk}" \
  ./gradlew :app:assembleDebug --console=plain "$@"

APK="app/build/outputs/apk/debug/app-debug.apk"
echo "APK: android/$APK"
shasum -a 256 "$APK" || true
