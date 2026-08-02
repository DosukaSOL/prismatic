#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Release-safety scan: fails if any ROM/game-derived data is tracked by git.
# Run in CI and before every release/packaging step.
set -euo pipefail
cd "$(dirname "$0")/.."

fail=0

# 1) Prohibited extensions tracked anywhere.
prohibited='\.(nds|srl|gba|gbc|gb|sav|dsv|state|bios|firmware|srm|rtc|xdelta|bps|ips)$'
if git ls-files | grep -iE "$prohibited"; then
    echo "FAIL: prohibited game-data files are tracked (above)." >&2
    fail=1
fi

# 2) Prohibited directories tracked.
for dir in local_data roms private extracted generated_game_data \
           private_reports private_screenshots private_saves private_states; do
    if git ls-files -- "$dir" 2>/dev/null | grep -q .; then
        echo "FAIL: tracked files inside prohibited directory '$dir/'." >&2
        fail=1
    fi
done

# 3) Large binary blobs that could be disguised game data (>8 MB tracked file).
while IFS= read -r f; do
    sz=$(wc -c < "$f" 2>/dev/null || echo 0)
    if [ "$sz" -gt $((8 * 1024 * 1024)) ]; then
        echo "FAIL: tracked file over 8MB: $f ($sz bytes) — verify it is not game data." >&2
        fail=1
    fi
done < <(git ls-files)

if [ "$fail" -ne 0 ]; then
    echo "release-safety scan FAILED" >&2
    exit 1
fi
echo "release-safety scan OK (no ROM or game-derived data tracked)"
