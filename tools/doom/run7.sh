#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on an emulated Series 7: the EXE goes into the ROM image over the
# "Welcome to Series 7" file (into the space of two spare data files) and is
# opened with Enter; the WAD is on a card in the CompactFlash slot.
#
#   run7.sh OUT.ppm TOTAL_SECONDS [harness args]
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$1"; SECS="$2"; shift 2
ROM="$REPO/roms/Series7/S7_v1.05(254)_b754_eng/S7_v1.05(254)_b754_eng.bin"
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$WORK/card.img" "$REPO/tools/Doom1.WAD" >/dev/null
python3 "$REPO/tools/doom/tools/romx.py" "$ROM" replace 'Z:\System\Samples\Welcome to Series 7' "$EXE" "$WORK/s7.bin" \
    --rename AUTO --donor 'Z:\System\Data\Iens9522.dat,Z:\System\Data\Rektc400.dat' >/dev/null
"$REPO/harness/run" "$WORK/s7.bin" --device series7 --log-file "$OUT.full" --boot-seconds 45 \
    --card-path "$WORK/card.img" --post-attach-seconds $((SECS - 45)) \
    --press-key 60 17 --press-key 61 17 --press-key 62 17 --press-key 63 17 --press-key 64 17 --press-key 65 17 --press-key 66 17 --press-key 68 3 --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
