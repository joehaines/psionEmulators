#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# A Series 7 boots with a card already in its CompactFlash slot.
#
# The frontend attaches a saved card, or the app library's Try it card, as soon as
# the ROM loads — while the OS is still starting. That used to leave the boot on
# the splash screen for good, or the card unreadable. For each attach time and
# RTC seed below (the outcome depends on the clock the machine boots with, so the
# seeds pin it) this checks that the machine reaches the System screen and that
# E:, opened from the folder dialog, lists the card's file.
#
#   bash tests/integration/test-series7-boot-card.sh
set -u
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROM="$REPO/roms/Series7/S7_v1.05(254)_b754_eng/S7_v1.05(254)_b754_eng.bin"
[ -x "$REPO/harness/run" ] || { echo "harness not built — run bash harness/build.sh" >&2; exit 2; }
[ -f "$ROM" ] || { echo "SKIP: $ROM not present" >&2; exit 0; }
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
node --experimental-strip-types "$REPO/tools/doom/mkcard.mts" "$WORK/card.img" \
    --only "README.TXT=$REPO/tools/doom/README.md" >/dev/null

ATTACH="0 0.5 0.75"
SEEDS="32548422 3254d3f3 2f000000"

check() {   # check ATTACH SEED
    local t=$1 sd=$2 out="$WORK/$1-$2"
    # At 30 s, well after the desktop is up: Tab opens the folder dialog, Up x4
    # reaches Disk, E picks the card's drive and Enter commits it.
    PSION_RTC_SEED=$sd "$REPO/harness/run" "$ROM" --device series7 --quiet-logs \
        --boot-seconds "$t" --card-path "$WORK/card.img" \
        --post-attach-seconds "$(awk -v t="$t" 'BEGIN { print 40 - t }')" \
        --press-key 30 2 --press-key 32 16 --press-key 33 16 --press-key 34 16 \
        --press-key 35 16 --press-key 36 69 --press-key 38 3 \
        --screenshot "$out.ppm" > "$out.log" 2>&1
    node -e '
        const b = require("fs").readFileSync(process.argv[1]);
        const m = /^P([56])\s+(\d+)\s+(\d+)\s+255\s/.exec(b.subarray(0, 32).toString("latin1"));
        const n = m[1] === "6" ? 3 : 1, W = +m[2], px = b.subarray(b.length - W * +m[3] * n);
        const dark = (x0, x1, y0, y1, lim) => {
            let c = 0;
            for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) if (px[(y * W + x) * n] < lim) c++;
            return c;
        };
        // The System screen toolbar down the right-hand edge (not the frame line).
        // Then README.TXT on E:: either the size and date columns of the folder
        // dialog'"'"'s first file row (which a folder row has not), or, once the
        // dialog has closed, its highlighted label at the top of the E: view.
        const desk = dark(W - 60, W - 8, 8, 300, 128) > 300;
        const listed = dark(420, 522, 88, 106, 100) > 30 || dark(40, 110, 4, 30, 60) > 400;
        console.log(desk ? (listed ? "ok" : "E: does not list the card") : "boot never reached the System screen");
    ' "$out.ppm"
}

failures=0
for sd in $SEEDS; do
    for t in $ATTACH; do
        r=$(check "$t" "$sd")
        if [ "$r" = ok ]; then echo "ok   card at ${t}s, clock $sd"
        else echo "FAIL card at ${t}s, clock $sd: $r"; failures=$((failures + 1)); fi
    done
done
[ "$failures" -eq 0 ] && echo "all checks passed" || { echo "$failures check(s) failed"; exit 1; }
