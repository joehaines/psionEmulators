#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on an emulated netpad: EXE and WAD on an MMC card image, opened
# from the System screen with the pen route tests/integration/test-lemmings-netpad.sh
# uses (disk button, D: row, Enter). The screenshot is a colour PPM.
#
#   runnp.sh OUT.ppm TOTAL_SECONDS [harness args]
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$1"; SECS="$2"; shift 2
ROM="$REPO/roms/netPad/netPad_v1.75(247)_eng/Netpad.img"
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$WORK/card.img" "$REPO/tools/Doom1.WAD" "DOOM.EXE=$EXE" >/dev/null
"$REPO/harness/run" "$ROM" --device netpad --log-file "$OUT.full" --boot-seconds 15 \
    --card-path "$WORK/card.img" --post-attach-seconds $((SECS - 15)) \
    --tap-seq 45 10 228 --tap-seq 50 60 214 --press-key 55 3 \
    --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
