#!/usr/bin/env bash
# Mouse-pad end-to-end test for the Geofox One.
#
# The Geofox has no touchscreen. It points with a capacitive pad in the
# keyboard deck that reports RELATIVE motion, read by the ROM's Exyin.dll
# as a PS/2-shaped packet — status byte carrying the button and the two
# sign bits, then X, then Y — over two SSI frames, started only when the
# pad raises EINT2 (INTSR1 bit 6). core/geofox.h models that, and turns
# the absolute position a host mouse or touchscreen reports back into the
# deltas that walk the guest's own pointer there.
#
# That conversion is the part worth a test: it only works if the emulator
# keeps a shadow of the pointer the driver holds and never hands over a
# delta the driver would accelerate, so a drift of a few pixels per packet
# would compound into clicks landing somewhere else entirely. Both phases
# below assert on where the guest's pointer actually ended up, not just
# that something moved.
#
#   Phase 1  double-tap the Word icon — the pointer has to travel from the
#            driver's start position (320, 160) to (110, 81) and press
#            there twice, and Word has to open.
#   Phase 2  drag across the desktop from the Word icon to (400, 250) —
#            the button has to stay down the whole way, which rubber-band
#            selects Word and Sheet.
#
# Exit codes:
#   0 = PASS
#   1 = FAIL — the double-tap did not open Word
#   2 = FAIL — no mouse-pad packets, or the pointer never arrived at the
#              tap position (the SSI/EINT2 path or the tracking is broken)
#   3 = FAIL — the machine never reached its desktop
#   4 = FAIL — the drag did not select both icons
#
# Usage: tests/integration/test-geofox-mouse.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM="$REPO/roms/Geofox_v1.01(146)_eng.bin"
TMP=$(mktemp -d /tmp/gfmouse.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "FAIL: $HARNESS not built — run bash harness/build.sh first"
    exit 3
fi

# The desktop is up well before 40 s of sim time; tap at 42 s so the shell
# is idle, and keep running afterwards so Word has time to paint. No
# --skip-card: the harness dispatches its scheduled events from the
# post-attach phase, which that flag skips along with the card.
run_phase() {
    local out=$1; shift
    PSION_GF_MOUSE_TRACE=1 "$HARNESS" "$ROM" \
        --device geofox \
        --boot-seconds 40 \
        --post-attach-seconds 25 \
        --screenshot "$out.pgm" \
        "$@" \
        > "$out.log" 2>&1
}

echo "=== phase 1: double-tap the Word icon ==="
run_phase "$TMP/tap" --tap-seq 42 110 81 --tap-seq 43.2 110 81

echo "=== phase 2: drag from the Word icon across the desktop ==="
run_phase "$TMP/drag" --drag-seq 42 110 81 400 250

python3 - "$TMP" <<'PY'
import os, re, sys

tmp = sys.argv[1]


def load(path):
    with open(path, 'rb') as fh:
        assert fh.readline().strip() == b'P5'
        line = fh.readline()
        while line.startswith(b'#'):
            line = fh.readline()
        w, h = map(int, line.split())
        fh.readline()
        return w, h, fh.read()


def ink(path, x0, y0, x1, y1, thr=90):
    """Fraction of dark pixels in a region."""
    w, h, d = load(path)
    tot = dark = 0
    for y in range(y0, y1):
        base = y * w
        for x in range(x0, x1):
            tot += 1
            if d[base + x] < thr:
                dark += 1
    return dark / tot if tot else 0.0


for name in ('tap', 'drag'):
    if not os.path.exists(f'{tmp}/{name}.pgm'):
        print(f'FAIL[3]: {name} phase produced no screenshot — the machine '
              f'may not have booted')
        sys.exit(3)

# ── the packets themselves ────────────────────────────────────────────
# "mouse pad: step (dx,dy) -> pointer (x,y) status ss  queue n"
PACKET = re.compile(r'mouse pad: step \((-?\d+),(-?\d+)\) -> pointer '
                    r'\((\d+),(\d+)\) status ([0-9a-f]{2})')
packets = [(int(a), int(b), int(x), int(y), int(s, 16))
           for a, b, x, y, s in PACKET.findall(open(f'{tmp}/tap.log').read())]
if not packets:
    print('FAIL[2]: the pad delivered no packets at all — EINT2 is not '
          'reaching the driver, or it never read SSI control byte 0x80')
    sys.exit(2)

# Every press has to happen at the tap position: that is the whole point of
# only changing the button once the pointer has arrived.
presses = [(x, y) for _, _, x, y, s in packets if s & 0x01]
if not presses:
    print('FAIL[2]: no packet ever carried the button bit — the press was '
          'lost between the host and the pad')
    sys.exit(2)
astray = [p for p in presses if p != (110, 81)]
if astray:
    print(f'FAIL[2]: the button was reported at {astray[0]} instead of '
          f'(110,81) — the shadow pointer has drifted from the driver\'s, '
          f'so clicks land somewhere other than where the user pointed')
    sys.exit(2)

# No delta may be large twice running on one axis: that is exactly the
# condition the driver accelerates on (ROM 0x5007DD20), and an accelerated
# delta is one the emulator cannot predict, which is what would make the
# shadow drift in the first place.
FLOOR = 8
big_x = big_y = False
for dx, dy, _, _, _ in packets:
    if abs(dx) >= FLOOR and big_x:
        print(f'FAIL[2]: two accelerable X deltas in a row ({dx}) — the '
              f'driver would scale this one and the shadow would drift')
        sys.exit(2)
    if abs(dy) >= FLOOR and big_y:
        print(f'FAIL[2]: two accelerable Y deltas in a row ({dy}) — the '
              f'driver would scale this one and the shadow would drift')
        sys.exit(2)
    big_x, big_y = abs(dx) >= FLOOR, abs(dy) >= FLOOR

print(f'pad packets={len(packets)} presses={len(presses)} all at (110,81)')

# ── phase 1: Word opened ──────────────────────────────────────────────
# Word's toolbar (style, font, size, B/I/U, alignment) fills the top band
# right across the panel. The Shell's desktop leaves that band empty.
toolbar = ink(f'{tmp}/tap.pgm', 150, 2, 520, 16)
print(f'toolbar band ink={toolbar:.4f}')
if toolbar < 0.05:
    print('FAIL[1]: the double-tap did not open Word — the pointer moved '
          'but the clicks did not land on the icon')
    sys.exit(1)

# ── phase 2: the drag selected both icons ─────────────────────────────
# A selected icon label is drawn inverted, so its ink roughly triples.
word = ink(f'{tmp}/drag.pgm', 80, 70, 140, 93)
sheet = ink(f'{tmp}/drag.pgm', 80, 122, 140, 145)
print(f'drag selection ink: Word={word:.4f} Sheet={sheet:.4f}')
if word < 0.4 or sheet < 0.4:
    print('FAIL[4]: the drag did not rubber-band select Word and Sheet — '
          'the button bit is not being held across the packets that move '
          'the pointer')
    sys.exit(4)

print('PASS geofox mouse pad: pointer tracked, tap opened Word, drag selected')
PY
