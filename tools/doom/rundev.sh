#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on any emulated machine whose ROM has a sample document to swap it
# into: the EXE replaces the document (using the space of two adjacent spare data
# files if it is bigger), the WAD goes on a card, and Enter opens the EXE.
#
#   rundev.sh DEVICE_ID ROM WELCOME_FILE OUT SECS [harness args]
#   e.g. rundev.sh mc218 'roms/MC218/MC218_v1.05(259)_eng/MC218_v1.05(259)_eng.bin' \
#           'Z:\System\Samples\Welcome to EPOC' /tmp/mc218.pgm 150
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEV="$1"; ROM="$REPO/$2"; WELCOME="$3"; OUT="$4"; SECS="$5"; shift 5
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
DONORS="${DOOM_DONORS:-Z:\\System\\Data\\Iens9522.dat,Z:\\System\\Data\\Rektc400.dat}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$WORK/card.img" "$REPO/tools/Doom1.WAD" >/dev/null
python3 "$REPO/tools/doom/tools/romx.py" "$ROM" replace "$WELCOME" "$EXE" "$WORK/rom.bin" --rename AUTO --donor "$DONORS" >/dev/null
"$REPO/harness/run" "$WORK/rom.bin" --device "$DEV" --log-file "$OUT.full" --boot-seconds 30 \
    --card-path "$WORK/card.img" --post-attach-seconds $((SECS - 30)) \
    ${DOOM_KEYS:---press-key 50 3} --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
