#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run the headless validation runner and print the report location.
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ ! -x build/tools/headless-runner/prismatic_headless ]]; then
  ./scripts/build.sh
fi
./build/tools/headless-runner/prismatic_headless
echo "Open: local_data/validation_output/report.html"
