#!/usr/bin/env bash
# App-library 'epocdir' install, end to end on the real 5mx ROM.
#
# Delivers an installed-app-folder bundle over the Remote Link with the
# production code (see test-epocdir-install.mts for the chain), reads
# every file back off the device, then drives the machine's own UI —
# tap Extras, tap the app's icon — and requires the app to actually be
# on screen at the end.
#
# The tap coordinates are 5mx digitiser pixels (695 x 280, with the
# 640 x 240 LCD at offset 45,5):
#   645,262  the Extras silkscreen icon, right-hand end of the strip
#            printed below the screen
#   102,178  the first slot in the Extras bar, where a newly installed
#            app lands (LCD 57,173)
#
# Usage:
#   bash tests/integration/test-epocdir-install.sh [app-id]

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_ID="${1:-epocgames/wallinstalled}"
OUT_DIR="$REPO_ROOT/tests/results"
mkdir -p "$OUT_DIR"

if [ ! -x "$REPO_ROOT/harness/run" ]; then
    bash "$REPO_ROOT/harness/build.sh" >&2
fi
if [ ! -f "$REPO_ROOT/roms/5mx_v1.05(260)_eng.bin" ]; then
    echo "SKIP: 5mx ROM not present" >&2
    exit 0
fi

exec node --experimental-strip-types \
    "$REPO_ROOT/tests/integration/test-epocdir-install.mts" \
    --app "$APP_ID" \
    --seconds 95 \
    --tap 60 645 262 \
    --tap 67 102 178 \
    --screenshot "$OUT_DIR/epocdir-install.pgm" \
    --log "$OUT_DIR/epocdir-install.log" \
    --assert-launched
