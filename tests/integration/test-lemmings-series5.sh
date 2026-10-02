#!/usr/bin/env bash
# End-to-end test of LEMMINGS.EXE (tools/lemmings) on an emulated Psion
# Series 5: its own ROM, its own loader, its own window server.
#
# The program gets onto the machine the way tools/romdump-er1's does — put
# into the ROM over the Welcome file, renamed to end .EXE, opened from the
# System screen (see test-er1-romdump.sh for why). Then:
#
#   40 s  Enter — the desktop opens the selected file, which is the game
#   48 s  Enter — on the title screen, play the highlighted level (1)
#   80 s  screenshot: the level with lemmings walking out of the trapdoor
#
# tests/integration/test-lemmings-series5.mts reads the screenshot.
#
#   bash tests/integration/test-lemmings-series5.sh

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="$REPO_ROOT/roms/Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin"
EXE="$REPO_ROOT/tools/lemmings/LEMMINGS.EXE"
RESULTS="$REPO_ROOT/tests/results"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "harness not built — run bash harness/build.sh" >&2
    exit 2
fi
for f in "$ROM" "$EXE"; do
    if [ ! -f "$f" ]; then
        echo "SKIP: $f not present" >&2
        exit 0
    fi
done
mkdir -p "$RESULTS"

node --experimental-strip-types "$REPO_ROOT/tools/e32/romfs1.mts" \
    replace "$ROM" 'Z:\System\Samples\Welcome to Series 5' "$EXE" \
    "$WORK/series5-lemmings.bin" --rename 'LEMMINGSABCDEFG.EXE' \
    --donor 'Z:\System\Data\Help'

"$HARNESS" "$WORK/series5-lemmings.bin" --device series5 --quiet-logs --skip-card \
    --boot-seconds 80 --press-key 40 3 --press-key 48 3 \
    --screenshot "$RESULTS/lemmings-series5.pgm" \
    > "$RESULTS/lemmings-series5.log" 2>&1 || {
        echo "harness run failed — see $RESULTS/lemmings-series5.log" >&2
        exit 1
    }

node --experimental-strip-types "$REPO_ROOT/tests/integration/test-lemmings-series5.mts" \
    "$RESULTS/lemmings-series5.pgm"
