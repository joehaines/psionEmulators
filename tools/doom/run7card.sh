#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Run DOOM.EXE on an emulated Series 7 the way the app library's "Try it" puts it
# there: the machine already has a card in its CompactFlash slot (drive E:), and
# DOOM.EXE, DOOM.AIF and the WAD are written onto that card in place while it is
# mounted. Then E: is opened from the System screen and DOOM.EXE started from it.
#
#   run7card.sh OUT.ppm TOTAL_SECONDS [harness args]
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$1"; SECS="$2"; shift 2
ROM="$REPO/roms/Series7/S7_v1.05(254)_b754_eng/S7_v1.05(254)_b754_eng.bin"
EXE="${DOOM_EXE:-$REPO/tools/doom/DOOM.EXE}"
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
MK=(node --experimental-strip-types "$REPO/tools/doom/mkcard.mts")
"${MK[@]}" "$WORK/old.img" --only "README.TXT=$REPO/tools/doom/README.md" >/dev/null
"${MK[@]}" "$WORK/new.img" "$REPO/tools/Doom1.WAD" "DOOM.EXE=$EXE" \
    "DOOM.AIF=$REPO/tools/doom/DOOM.AIF" --base "$WORK/old.img" >/dev/null
# 40 s: the old card goes in; 45 s: the new one is written over it in place.
# 50 s on: Tab opens the folder dialog, Up x4 to Disk, Right opens its list,
# Down x2 to E, Enter, Esc closes the list, Enter for OK; then Down to DOOM.EXE
# (below DOOM.AIF) and Enter.
PSION_HARNESS_INPLACE_SWAP=1 "$REPO/harness/run" "$ROM" --device series7 --log-file "$OUT.full" \
    --boot-seconds 40 --card-path "$WORK/old.img" --post-attach-seconds 5 \
    --swap-card-path "$WORK/new.img" --swap-post-seconds $((SECS - 45)) \
    --press-key 50 2 --press-key 52 16 --press-key 53 16 --press-key 54 16 --press-key 55 16 \
    --press-key 56 15 --press-key 58 17 --press-key 59 17 --press-key 60 3 --press-key 63 4 \
    --press-key 65 3 --press-key 68 17 --press-key 70 3 --screenshot "$OUT" "$@" > "$OUT.log" 2>&1
