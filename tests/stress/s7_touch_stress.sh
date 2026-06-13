#!/usr/bin/env bash
# Touch-longevity stress for the Series 7 emulator.
#
# Regression test for the "touch dies after a while of tapping" wedge: under
# rapid tapping (or while drawing in Sketch — any pen-up that lands in the
# short window after the digitiser's ADC conversion trigger but before its
# scheduled AtoD-complete), the pen-up handler used to cancel the in-flight
# completion, leaving the kernel's shared ADC service busy forever.  The
# driver then never re-triggered and touch wedged for the rest of the session
# (the keyboard, a separate IRQ path, kept working).  See updateTouchInput().
#
# This taps the "Word" and "Sheet" icons ALTERNATELY many times in quick
# succession and checks that the selection highlight is STILL moving in the
# final stretch of the run — i.e. touch never wedged.
#
# Exit codes:
#   0 = PASS — touch still responding at the end of the stress
#   1 = FAIL — touch wedged partway through (highlight stopped moving)
#   2 = FAIL — OS never reached the file browser
#
# Usage: tests/s7_touch_stress.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/'S7_v1.05(254)_b754_eng.bin'
TMP=$(mktemp -d /tmp/s7stress.XXXXXX)

# Word label ~ (110,210); Sheet label ~ (110,268).  Alternate between them so
# every successful tap visibly moves the highlight.
PSION_MULTI_TAP=${PSION_MULTI_TAP:-50} \
PSION_TAP_JITTER=0 \
PSION_TAP_GAP=${PSION_TAP_GAP:-0.2} \
PSION_TAP_HOLD_FRAMES=${PSION_TAP_HOLD_FRAMES:-4} \
PSION_TAP_ALT_X=110 PSION_TAP_ALT_Y=268 \
"$HARNESS" "$ROM" \
    --device series7 \
    --boot-seconds 24 \
    --skip-card \
    --tap-at 110 210 \
    --tap-after 0.5 \
    --post-attach-seconds 4 \
    --screenshot-every 0.4 "$TMP/f" \
    --quiet-logs \
    "$@" \
    > "$TMP/log" 2>&1

python3 - "$TMP" <<'PY'
import glob, sys
tmp = sys.argv[1]
frames = sorted(glob.glob(f'{tmp}/f-*.pgm'))
if len(frames) < 10:
    print(f'FAIL[2]: only {len(frames)} screenshots — OS may not have booted')
    sys.exit(2)

def load(p):
    with open(p, 'rb') as fh:
        assert fh.readline().strip() == b'P5'
        l = fh.readline()
        while l.startswith(b'#'):
            l = fh.readline()
        w, h = map(int, l.split())
        fh.readline()
        return w, h, fh.read()

def dark(p, x0, y0, x1, y1, thr=90):
    w, h, d = load(p)
    tot = dk = 0
    for y in range(y0, y1):
        b = y * w
        for x in range(x0, x1):
            tot += 1
            if d[b + x] < thr:
                dk += 1
    return dk / tot if tot else 0.0

prev = None
toggles = 0
last_toggle = 0
for i, f in enumerate(frames):
    wd = dark(f, 78, 200, 150, 222)
    sh = dark(f, 78, 258, 150, 280)
    sel = 'Word' if wd > 0.35 and wd > sh else ('Sheet' if sh > 0.35 else None)
    if sel and prev and sel != prev:
        toggles += 1
        last_toggle = i
    if sel:
        prev = sel

n = len(frames)
print(f'taps=50 highlight-toggles={toggles} last_toggle_frame={last_toggle}/{n - 1}')

# Touch is healthy if it kept toggling well into the run.  A wedge shows up as
# the last toggle landing far before the end (taps continue but nothing moves).
if toggles < 30:
    print(f'FAIL[1]: only {toggles} toggles — touch barely responded')
    sys.exit(1)
if last_toggle < n - 12:
    print(f'FAIL[1]: highlight stopped moving at frame {last_toggle}/{n - 1} '
          f'— touch WEDGED mid-session')
    sys.exit(1)
print('PASS: touch still responding through the full tap stress')
sys.exit(0)
PY
RC=$?
echo ""
echo "rm -rf $TMP"
exit $RC
