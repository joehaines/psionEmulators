#!/usr/bin/env bash
# Functional touch test for the Series 7 emulator.
#
# Boots to the OS file browser, taps the "Word" icon, and verifies that the
# WORD ICON ACTUALLY GETS HIGHLIGHTED (its label flips to a black box with
# white text).  This is a TRUE touch test: it checks that the tapped item
# responds, not merely that "the screen changed".
#
# (The previous version counted "≥3 distinct screen frames" as PASS, which
#  was a false-positive trap — the boot splash→desktop transition plus the
#  analog clock ticking in the corner pass it without any touch working.)
#
# Exit codes:
#   0 = PASS — tapping Word highlighted the Word icon
#   1 = FAIL — Word not highlighted (touch ignored / mis-landed)
#   2 = FAIL — OS never reached the file browser
#
# Usage: tests/s7_func.sh [extra args passed to harness/run]
#   e.g. tests/s7_func.sh                       # faithful touch (default)
#        PSION_S7_LEGACY_TOUCH=1 tests/s7_func.sh   # synthetic path

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/'S7_v1.05(254)_b754_eng.bin'
TMP=$(mktemp -d /tmp/s7func.XXXXXX)

# The "Word" document icon sits in the left spine at LCD (110, 207).  Its
# label text "Word" occupies roughly x[78..150], y[197..218].
TAP_X=110
TAP_Y=207

"$HARNESS" "$ROM" \
    --device series7 \
    --boot-seconds 22 \
    --skip-card \
    --tap-at $TAP_X $TAP_Y \
    --tap-after 15 \
    --post-attach-seconds 4 \
    --screenshot-every 2 "$TMP/f" \
    --quiet-logs \
    "$@" \
    > "$TMP/log" 2>&1

python3 - "$TMP" <<'PY'
import glob, sys, os
tmp = sys.argv[1]
frames = sorted(glob.glob(f'{tmp}/f-*.pgm'))
if len(frames) < 2:
    print(f'FAIL[2]: only {len(frames)} screenshots — OS may not have reached the browser')
    sys.exit(2)

def load_pgm(path):
    with open(path, 'rb') as fh:
        assert fh.readline().strip() == b'P5'
        line = fh.readline()
        while line.startswith(b'#'):
            line = fh.readline()
        w, h = map(int, line.split())
        fh.readline()  # maxval
        data = fh.read()
    return w, h, data

def dark_frac(path, x0, y0, x1, y1, thr=90):
    w, h, data = load_pgm(path)
    tot = dark = 0
    for y in range(y0, y1):
        base = y * w
        for x in range(x0, x1):
            tot += 1
            if data[base + x] < thr:
                dark += 1
    return dark / tot if tot else 0.0

# The Word label region (highlighted => black box => mostly dark).
WX0, WY0, WX1, WY1 = 80, 197, 150, 218

# The last frame is post-tap; an early post-boot frame is the stable desktop.
last = frames[-1]
# pick a pre-tap stable-desktop frame (~60% through boot, before the tap)
pre = frames[max(0, int(len(frames) * 0.45))]

pre_d = dark_frac(pre, WX0, WY0, WX1, WY1)
post_d = dark_frac(last, WX0, WY0, WX1, WY1)

# Sanity: confirm the browser actually rendered (whole-screen variance).
w, h, data = load_pgm(last)
mean = sum(data) / len(data)
var = sum((b - mean) ** 2 for b in data) / len(data)
if var < 500:
    print(f'FAIL[2]: final screen variance {var:.0f} too low — browser not rendered')
    sys.exit(2)

print(f'Word-label dark fraction: pre-tap={pre_d:.2f}  post-tap={post_d:.2f}')

# Highlighted label is a solid black box (~0.45+ dark); an un-highlighted
# label is white with thin text (~0.15 dark).  Require the post-tap Word
# region to be clearly highlighted AND clearly darker than pre-tap.
if post_d >= 0.35 and post_d - pre_d >= 0.15:
    print('PASS: tapping Word highlighted the Word icon')
    sys.exit(0)
else:
    print('FAIL[1]: Word not highlighted after tap (touch ignored or mis-landed)')
    sys.exit(1)
PY
RC=$?
echo ""
echo "Log tail:"
tail -6 "$TMP/log" | grep -v "^$"
echo ""
echo "rm -rf $TMP"
exit $RC
