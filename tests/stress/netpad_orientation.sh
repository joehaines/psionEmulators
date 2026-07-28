#!/usr/bin/env bash
# "Switch orientation" test for the netpad.
#
# The netpad's Tools menu carries "Switch orientation", and choosing it
# turns the machine from a landscape slab into a portrait one.  None of
# that happens in hardware: LCCR1/LCCR2/DBAR1 are untouched across the
# switch, so the LCD controller keeps scanning the same 640x240 panel out
# of the same framebuffer and EPOC's screen driver simply starts drawing
# the whole desktop rotated inside it.  A host that wants to show the
# machine the way its user is holding it therefore has to (a) be told,
# and (b) turn the panel image a quarter-turn ANTICLOCKWISE.
#
# Emulator::getScreenOrientation reports (a) by reading the state out of
# the screen driver's draw device in guest RAM (see netpadScreenOrientation
# in core/sa1100.cpp); this test drives the menu for real and checks it,
# and checks (b) by measuring the screenshots.
#
# Checks:
#   1. A freshly-booted netpad reports orientation 0 (landscape).
#   2. Tools -> "Switch orientation" flips it to 1, and the desktop is
#      redrawn rotated: the System sidebar leaves the right-hand edge of
#      the panel and the title bar leaves the left-hand one.
#   3. Rotating the screenshot a quarter-turn anticlockwise puts the
#      furniture back where an upright screen has it — i.e. that is the
#      direction the frontend has to turn the device (and NOT clockwise,
#      which is also checked).
#   4. Selecting it again returns the machine to orientation 0.  The tap
#      that does it is placed in panel coordinates through the same
#      rotation, so this doubles as an end-to-end check that the
#      digitiser keeps reporting panel coordinates while rotated.
#
# Exit codes: 0 = PASS, 1 = a check failed, 3 = the netpad never booted.
#
# Usage: tests/stress/netpad_orientation.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/Netpad.img
TMP=$(mktemp -d /tmp/nporient.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then bash "$REPO/harness/build.sh" >&2; fi

# Menu (EStdKeyMenu = 148) opens the menu bar on File; the Tools title is
# at panel (391, 17) and "Switch orientation" at (445, 166) under it.
# Reopening the menu bar afterwards comes up on Tools with the same entry
# highlighted, so the second switch is one tap — at the panel coordinate
# the entry occupies once the desktop is drawn rotated, (331, 122).
"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 36 --screenshot "$TMP/landscape.pgm" "$@" \
    > "$TMP/landscape.log" 2>&1

"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 36 --press-key 18 148 8 --tap-seq 20 391 17 \
    --tap-seq 22 445 166 --screenshot "$TMP/portrait.pgm" "$@" \
    > "$TMP/portrait.log" 2>&1

"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 36 --press-key 18 148 8 --tap-seq 20 391 17 \
    --tap-seq 22 445 166 --press-key 28 148 8 --tap-seq 30 331 122 \
    --screenshot "$TMP/back.pgm" "$@" \
    > "$TMP/back.log" 2>&1

python3 - "$TMP" <<'PY'
import re, sys

tmp = sys.argv[1]
rc = 0


def load(path):
    with open(path, 'rb') as fh:
        assert fh.readline().strip() == b'P5'
        line = fh.readline()
        while line.startswith(b'#'):
            line = fh.readline()
        w, h = map(int, line.split())
        fh.readline()
        return w, h, fh.read()


def orientation(logpath):
    """The '=== Screen orientation: N ===' line the harness prints."""
    with open(logpath) as fh:
        m = re.findall(r'Screen orientation: (\d+)', fh.read())
    return int(m[-1]) if m else None


def ink(img, x0, y0, x1, y1, thr=90):
    """Dark-pixel count in a region of a (w, h, bytes) image."""
    w, h, d = img
    n = 0
    for y in range(max(0, y0), min(h, y1)):
        base = y * w
        for x in range(max(0, x0), min(w, x1)):
            if d[base + x] < thr:
                n += 1
    return n


def rotate(img, direction):
    """Quarter-turn of a (w, h, bytes) image; 'ccw' or 'cw'."""
    w, h, d = img
    nw, nh = h, w
    out = bytearray(nw * nh)
    for y in range(h):
        row = y * w
        for x in range(w):
            if direction == 'ccw':
                out[(nh - 1 - x) * nw + y] = d[row + x]
            else:
                out[x * nw + (nw - 1 - y)] = d[row + x]
    return nw, nh, bytes(out)


try:
    landscape = load(f'{tmp}/landscape.pgm')
    portrait  = load(f'{tmp}/portrait.pgm')
    back      = load(f'{tmp}/back.pgm')
except Exception as e:                                   # noqa: BLE001
    print(f'FAIL[3]: could not read screenshots ({e}) — netpad may not have booted')
    sys.exit(3)

# ── 1. A fresh boot is landscape ─────────────────────────────────────
o_landscape = orientation(f'{tmp}/landscape.log')
o_portrait  = orientation(f'{tmp}/portrait.log')
o_back      = orientation(f'{tmp}/back.log')
print(f'reported orientation: boot={o_landscape} after-switch={o_portrait} '
      f'after-switch-back={o_back}')
if o_landscape != 0:
    print('FAIL[1]: a freshly-booted netpad should report orientation 0')
    rc = 1

# ── 2. The switch is seen, and the desktop really is redrawn rotated ──
# The window's title bar — a black band of reversed text, the densest
# ink on the desktop — runs down the panel's left-hand edge while the
# machine is landscape.  Rotating the drawing moves it to the right-hand
# edge, which is what a quarter-turn anticlockwise then puts along the
# top.  Comparing the two edge columns is therefore a direct read of
# which way up the guest is drawing.
def title_edges(img):
    return ink(img, 0, 0, 20, 240), ink(img, 620, 0, 640, 240)

left_landscape, right_landscape = title_edges(landscape)
left_portrait,  right_portrait  = title_edges(portrait)
print(f'title-bar ink: landscape left={left_landscape} right={right_landscape}')
print(f'title-bar ink: portrait  left={left_portrait} right={right_portrait}')
if o_portrait != 1:
    print('FAIL[1]: "Switch orientation" did not report orientation 1 — either '
          'the menu tap missed or the draw device was not found in RAM '
          '(netpadScreenOrientation in core/sa1100.cpp)')
    rc = 1
if not (left_landscape > right_landscape * 2 and right_portrait > left_portrait * 2):
    print('FAIL[1]: the desktop was not redrawn rotated — the title bar is '
          'still down the panel edge it occupies in landscape')
    rc = 1

# ── 3. Anticlockwise is the way up ───────────────────────────────────
# Turned the right way, the portrait screenshot has its furniture where an
# upright screen keeps it: the title bar along the top and the sidebar
# along the bottom.  Turned the wrong way they land on the opposite
# edges, so comparing the two rotations settles the direction.
ccw = rotate(portrait, 'ccw')
cw  = rotate(portrait, 'cw')
ccw_top,    ccw_bottom    = ink(ccw, 0, 0, 240, 20), ink(ccw, 0, 620, 240, 640)
cw_top,     cw_bottom     = ink(cw,  0, 0, 240, 20), ink(cw,  0, 620, 240, 640)
print(f'anticlockwise: top={ccw_top} bottom={ccw_bottom}')
print(f'clockwise:     top={cw_top} bottom={cw_bottom}')
# The title bar is a black band of reversed text — far more ink than the
# toolbar strip below it, and the discriminator between the two turns.
if not (ccw_top > ccw_bottom and cw_top < cw_bottom):
    print('FAIL[1]: the rotated desktop does not read upright when the panel '
          'is turned a quarter-turn anticlockwise')
    rc = 1

# ── 4. Choosing it again turns the machine back ──────────────────────
left_back, right_back = title_edges(back)
print(f'title-bar ink: back      left={left_back} right={right_back}')
if o_back != 0:
    print('FAIL[1]: selecting "Switch orientation" a second time did not '
          'return the machine to landscape — if orientation 1 was reported '
          'above, the rotated tap coordinate missed the menu entry, which '
          'means pen coordinates are no longer panel-relative while rotated')
    rc = 1
if left_back < right_back * 2:
    print('FAIL[1]: the desktop is still drawn rotated after switching back')
    rc = 1

print('PASS netpad orientation' if rc == 0 else f'FAIL (rc={rc})')
sys.exit(rc)
PY
rc=$?

exit $rc
