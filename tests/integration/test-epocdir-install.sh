#!/usr/bin/env bash
# App-library 'epocdir' install, end to end on a real ROM.
#
# Delivers an installed-app-folder bundle over the Remote Link with the
# production code (see test-epocdir-install.mts for the chain) and reads
# every file back off the device.
#
# On the 5mx it then drives the machine's own UI — tap Extras, tap the
# app's icon — and requires the app to actually be on screen at the end.
# The tap coordinates are 5mx digitiser pixels (695 x 280, with the
# 640 x 240 LCD at offset 45,5):
#   645,262  the Extras silkscreen icon, right-hand end of the strip
#            printed below the screen
#   102,178  the first slot in the Extras bar, where a newly installed
#            app lands (LCD 57,173)
#
# On the netpad the run stops at the read-back. The machine is the one
# the library delivers to by card, so the cable is the only road an
# installed-folder bundle can take to it (a card carries 8.3 names only)
# — this is what proves that road is open, and every delivered byte is
# compared against the bundle on the device. Where the app lands in
# Extras is the SIS path's business, which test-netpad-app-install.sh
# already checks on screen.
#
# Usage:
#   bash tests/integration/test-epocdir-install.sh [--device 5mx|netpad] [app-id]

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEVICE="5mx"
if [ "${1:-}" = "--device" ]; then
    DEVICE="${2:?--device needs a device id}"
    shift 2
fi
APP_ID="${1:-epocgames/wallinstalled}"
OUT_DIR="$REPO_ROOT/tests/results"
mkdir -p "$OUT_DIR"

case "$DEVICE" in
    5mx)    ROM="5mx_v1.05(260)_eng.bin" ;;
    netpad) ROM="Netpad.img" ;;
    *)      echo "unknown device $DEVICE (have 5mx, netpad)" >&2; exit 2 ;;
esac

if [ ! -x "$REPO_ROOT/harness/run" ]; then
    bash "$REPO_ROOT/harness/build.sh" >&2
fi
if [ ! -f "$REPO_ROOT/roms/$ROM" ]; then
    echo "SKIP: $DEVICE ROM not present" >&2
    exit 0
fi

if [ "$DEVICE" = "5mx" ]; then
    exec node --experimental-strip-types \
        "$REPO_ROOT/tests/integration/test-epocdir-install.mts" \
        --device "$DEVICE" \
        --app "$APP_ID" \
        --seconds 95 \
        --tap 60 645 262 \
        --tap 67 102 178 \
        --screenshot "$OUT_DIR/epocdir-install.pgm" \
        --log "$OUT_DIR/epocdir-install.log" \
        --assert-launched
fi

exec node --experimental-strip-types \
    "$REPO_ROOT/tests/integration/test-epocdir-install.mts" \
    --device "$DEVICE" \
    --app "$APP_ID" \
    --seconds 200 \
    --log "$OUT_DIR/epocdir-install-$DEVICE.log"
