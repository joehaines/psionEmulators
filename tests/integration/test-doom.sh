#!/usr/bin/env bash
# DOOM.EXE on an emulated machine: it boots, finds DOOM1.WAD on the card, and plays.
#
#   bash tests/integration/test-doom.sh [series5|5mx|netpad|series7]   (default: all four)
#
# Each runs tools/doom's runner for that machine (the EXE goes into the ROM image or onto a
# card, the WAD on a card; see the scripts) and checks the screenshot with test-doom.mts.
# The Series 5 and netpad take a few real minutes: the Series 5 is emulated at about
# real time and Doom needs a minute of that to load.
set -eu
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RESULTS="$REPO/tests/results"
mkdir -p "$RESULTS"
[ -x "$REPO/harness/run" ] || { echo "harness not built — run bash harness/build.sh" >&2; exit 2; }
[ -f "$REPO/tools/doom/DOOM.EXE" ] || bash "$REPO/tools/doom/build.sh"
[ -f "$REPO/tools/Doom1.WAD" ] || { echo "SKIP: tools/Doom1.WAD not present" >&2; exit 0; }

run_one() {
    local dev="$1" rom shot w h kind
    case "$dev" in
        series5) rom="roms/Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin"; shot="doom-series5.pgm"; w=640; h=240; kind=grey ;;
        5mx)     rom="roms/Series5mx/5mx_v1.05(260)_eng/5mx_v1.05(260)_eng.bin"; shot="doom-5mx.pgm"; w=640; h=240; kind=grey ;;
        netpad)  rom="roms/netPad/netPad_v1.75(247)_eng/Netpad.img"; shot="doom-netpad.ppm"; w=640; h=240; kind=colour ;;
        series7) rom="roms/Series7/S7_v1.05(254)_b754_eng/S7_v1.05(254)_b754_eng.bin"; shot="doom-series7.ppm"; w=640; h=480; kind=colour ;;
        *) echo "unknown device $dev" >&2; exit 2 ;;
    esac
    if [ ! -f "$REPO/$rom" ]; then echo "SKIP $dev: $rom not present" >&2; return 0; fi
    echo "== $dev"
    case "$dev" in
        series5) bash "$REPO/tools/doom/runs5.sh" "$RESULTS/$shot" 200 ;;
        5mx)     bash "$REPO/tools/doom/run5mx.sh" "$RESULTS/$shot" 170 ;;
        netpad)  bash "$REPO/tools/doom/runnp.sh" "$RESULTS/$shot" 200 ;;
        series7) bash "$REPO/tools/doom/run7.sh" "$RESULTS/$shot" 150 ;;
    esac
    node --experimental-strip-types "$REPO/tests/integration/test-doom.mts" "$RESULTS/$shot" $w $h $kind
}

if [ $# -gt 0 ]; then run_one "$1"; else for d in series5 5mx netpad series7; do run_one "$d"; done; fi
