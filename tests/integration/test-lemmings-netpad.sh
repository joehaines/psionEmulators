#!/usr/bin/env bash
# LEMMINGS.EXE on an emulated netpad — the colour panel.
#
# The netpad's ROM has no spare file to swap the game into (the Series 5
# test does that), so the game goes onto an MMC card, which is inserted as
# D: and opened from the System screen: the same pen route the app-install
# test uses (test-netpad-app-install.mts).
#
#   15 s  machine up, card goes in
#   45 s  tap the disk button, bottom-left
#   50 s  tap the D: row
#   55 s  Enter — open LEMMINGS.EXE, which is the only file
#   85 s  Enter — on the title screen, play level 1
#  105 s  screenshot (in colour, a PPM): level 1, lemmings walking
#
# tests/integration/test-lemmings-netpad.mts reads the screenshot.
#
#   bash tests/integration/test-lemmings-netpad.sh

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="$REPO_ROOT/roms/netPad/netPad_v1.75(247)_eng/Netpad.img"
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

node --experimental-strip-types "$REPO_ROOT/tools/lemmings/mkcard.mts" "$WORK/card.img" "$EXE"

"$HARNESS" "$ROM" --device netpad --quiet-logs --boot-seconds 15 \
    --card-path "$WORK/card.img" --post-attach-seconds 90 \
    --tap-seq 45 10 228 --tap-seq 50 60 214 \
    --press-key 55 3 --press-key 85 3 \
    --screenshot "$RESULTS/lemmings-netpad.ppm" \
    > "$RESULTS/lemmings-netpad.log" 2>&1 || {
        echo "harness run failed — see $RESULTS/lemmings-netpad.log" >&2
        exit 1
    }

node --experimental-strip-types "$REPO_ROOT/tests/integration/test-lemmings-netpad.mts" \
    "$RESULTS/lemmings-netpad.ppm"
