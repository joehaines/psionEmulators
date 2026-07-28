#!/usr/bin/env bash
# Touch end-to-end test for the netpad.
#
# The netpad drives its stylus through the SoC's own SSP: the digitiser
# driver (Exyin.dll) queues four command/dummy word pairs for an
# ADS7846-class board codec, waits for SSSR.BSY to drop, reads the eight
# receive words back and rebuilds a 12-bit sample from the last two.  Two
# emulator bugs used to break that chain end to end:
#
#   1. A Series 7 / netBook page-table hack redirected every mapping of
#      PA 0x80070000 (the SSP) into the Eiger register window, where SSSR
#      reads back 0 — BSY never dropped, so the driver's completion DFC
#      re-queued itself forever and no sample was ever taken.
#   2. Pen-down was injected on GPIO 10.  The netpad's pen-detect line is
#      GPIO 14 ("IrqGpioEdge14", the interrupt Exyin binds), so the ISR
#      that did run belonged to no driver and the stylus was dead.
#
# This boots to the netpad desktop and taps the System bar's "Ctrl. panel"
# button.  That only opens if the whole chain works AND the coordinates
# land within the button, so it covers the codec model, the pen interrupt
# and the ADC calibration in one go.
#
# Exit codes:
#   0 = PASS — the Control panel opened and X/Y conversions were clocked
#   1 = FAIL — the tap didn't land (screen unchanged)
#   2 = FAIL — the digitiser never sampled X/Y over the SSP
#   3 = FAIL — the netpad never reached its desktop
#
# Usage: tests/stress/netpad_touch.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/Netpad.img
TMP=$(mktemp -d /tmp/nptouch.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

# The desktop is up by ~7 s of sim time; tap at 30 s so the shell is idle,
# and keep running afterwards so the Control panel has time to paint.
PSION_NETPAD_SSP_TRACE=1 \
"$HARNESS" "$ROM" \
    --device netpad \
    --boot-seconds 40 \
    --skip-card \
    --tap-at 605 33 \
    --tap-after 30 \
    --screenshot-every 2 "$TMP/f" \
    --log-file "$TMP/emu.log" \
    "$@" \
    > "$TMP/log" 2>&1

# The codec's X (control 0xd3/0xd0) and Y (0x93/0x90) channels are only
# clocked while the driver believes the pen is down.
xy=$(grep -c -E 'SSDR <- (d3|d0|93|90)00' "$TMP/emu.log" || true)

python3 - "$TMP" "$xy" <<'PY'
import glob, sys

tmp, xy = sys.argv[1], int(sys.argv[2])
frames = sorted(glob.glob(f'{tmp}/f-*.pgm'))
if len(frames) < 10:
    print(f'FAIL[3]: only {len(frames)} screenshots — netpad may not have booted')
    sys.exit(3)


def load(p):
    with open(p, 'rb') as fh:
        assert fh.readline().strip() == b'P5'
        line = fh.readline()
        while line.startswith(b'#'):
            line = fh.readline()
        w, h = map(int, line.split())
        fh.readline()
        return w, h, fh.read()


def ink(p, x0, y0, x1, y1, thr=90):
    """Fraction of dark pixels in a region — the Control panel's icon grid
    fills the desktop area, the bare desktop leaves it almost white."""
    w, h, d = load(p)
    tot = dark = 0
    for y in range(y0, y1):
        base = y * w
        for x in range(x0, x1):
            tot += 1
            if d[base + x] < thr:
                dark += 1
    return dark / tot if tot else 0.0


# Desktop area, excluding the left "Documents" bar and the System bar.
before = ink(frames[len(frames) // 2 - 2], 30, 10, 560, 200)
after = ink(frames[-1], 30, 10, 560, 200)
print(f'desktop ink before={before:.4f} after={after:.4f} xy-conversions={xy}')

if xy == 0:
    print('FAIL[2]: no X/Y conversions on the SSP — the pen interrupt or the '
          'board codec is not reaching the digitiser driver')
    sys.exit(2)

# The Control panel's 12 icons and their labels are far more ink than the
# empty desktop's single folder icon plus the netpad logo.
if after < before * 3 or after < 0.02:
    print('FAIL[1]: the tap did not open the Control panel — check the '
          'digitiser ADC calibration (kNpTouchXAtLeft etc.)')
    sys.exit(1)

print('PASS netpad touch: Ctrl. panel opened from a stylus tap')
PY
