#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run an EXE on an emulated Series 5 the way tests/integration/test-lemmings-series5.sh
# does and take a screenshot.   runexe.sh PROG.EXE OUT.pgm [seconds] [extra harness args...]
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXE="$1"; OUT="$2"; SECS="${3:-60}"; shift 3 || shift $#
ROM="$REPO/roms/Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/e32/romfs1.mts" replace "$ROM" \
    'Z:\System\Samples\Welcome to Series 5' "$EXE" "$WORK/s5.bin" \
    --rename 'LEMMINGSABCDEFG.EXE' --donor 'Z:\System\Data\Help' >/dev/null
"$REPO/harness/run" "$WORK/s5.bin" --device series5 --quiet-logs --skip-card \
    --boot-seconds "$SECS" --press-key 40 3 --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
