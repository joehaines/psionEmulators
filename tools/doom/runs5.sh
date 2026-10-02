#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on an emulated Series 5 with the WAD on a card, and take a
# screenshot. The EXE is put into the ROM image over the "Welcome to Series
# 5" file (tools/e32/romfs1.mts grows the image for it, which an emulator's
# ROM window allows) and opened from the System screen with Enter, the way
# the Lemmings and ROM-dump tests do it. The WAD is on a card in the slot.
#
#   runs5.sh OUT.pgm TOTAL_SECONDS [harness args, e.g. --press-key 120 16]
#
# Keys are in simulated seconds from power on; the game starts at about 52.
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$1"; SECS="$2"; shift 2
ROM="$REPO/roms/Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin"
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
CARD="${DOOM_CARD:-$WORK/card.img}"
[ -f "$CARD" ] || node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$CARD" "$REPO/tools/Doom1.WAD" >/dev/null
node --experimental-strip-types "$REPO/tools/e32/romfs1.mts" replace "$ROM" \
    'Z:\System\Samples\Welcome to Series 5' "$EXE" "$WORK/s5.bin" \
    --rename 'LEMMINGSABCDEFG.EXE' >/dev/null
"$REPO/harness/run" "$WORK/s5.bin" --device series5 --log-file "$OUT.full" \
    --boot-seconds 30 --card-path "$CARD" --post-attach-seconds $((SECS - 30)) \
    --press-key 50 3 --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
