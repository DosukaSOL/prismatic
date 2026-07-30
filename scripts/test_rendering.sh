#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Compile PRISMATIC GLSL shaders to SPIR-V and validate them with glslc.
# Uses the glslc bundled with the Android NDK (shaderc). Fails on any error.
set -euo pipefail

GLSLC="${GLSLC:-$HOME/Library/Android/sdk/ndk/27.1.12297006/shader-tools/darwin-x86_64/glslc}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/shaders"
OUT="${1:-$ROOT/build/shaders}"

if [[ ! -x "$GLSLC" ]]; then
  echo "ERROR: glslc not found at $GLSLC" >&2
  echo "Set GLSLC=/path/to/glslc and re-run." >&2
  exit 2
fi

mkdir -p "$OUT"
echo "glslc: $("$GLSLC" --version | head -1)"

status=0
for shader in "$SRC"/*.vert "$SRC"/*.frag "$SRC"/*.comp; do
  [[ -e "$shader" ]] || continue
  name="$(basename "$shader")"
  if "$GLSLC" -O "$shader" -o "$OUT/$name.spv"; then
    bytes=$(wc -c < "$OUT/$name.spv")
    echo "  OK   $name -> $name.spv ($bytes bytes)"
  else
    echo "  FAIL $name" >&2
    status=1
  fi
done

exit $status
