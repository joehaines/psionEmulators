#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on an emulated Series 5mx: the EXE goes into the ROM image over
# the "Welcome to Series 5mx" file (into the space of two adjacent data files,
# which the 5mx's full 16 MB image needs) and is opened with Enter; the WAD is
# on a card in the CompactFlash slot.
#
#   run5mx.sh OUT.pgm TOTAL_SECONDS [harness args]
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$1"; SECS="$2"; shift 2
ROM="$REPO/roms/Series5mx/5mx_v1.05(260)_eng/5mx_v1.05(260)_eng.bin"
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$WORK/card.img" "$REPO/tools/Doom1.WAD" >/dev/null
python3 "$REPO/tools/doom/tools/romx.py" "$ROM" replace 'Z:\System\Samples\Welcome to Series 5mx' "$EXE" "$WORK/mx.bin" \
    --rename 'LEMMINGSABCDEFGHI.EXE' --donor 'Z:\System\Data\Iens9522.dat,Z:\System\Data\Rektc400.dat' >/dev/null
"${DOOM_HARNESS:-$REPO/harness/run}" "$WORK/mx.bin" --device 5mx --log-file "$OUT.full" --boot-seconds 30 \
    --card-path "$WORK/card.img" --post-attach-seconds $((SECS - 30)) \
    --press-key 50 3 --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
