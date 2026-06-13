#!/usr/bin/env bash
# Touch-reliability stress for the netBook emulator.
#
# Regression test for "~1 in 5 netBook touches don't land".  Root cause:
# updateTouchInput() mirrored the X/Y-ADC coordinate bytes into
# ASIC[0x2a-0x2d] on every pen event — but on the netBook FAITHFUL touch
# path (the default) those four bytes are the live Eiger IRQ registers
# (0x2a/0x2b = IRQ_STATUS, 0x2c = IRQ_MASK), NOT a coordinate mirror.  The
# clobber injected a fistful of phantom "interrupt pending" bits into
# IRQ_STATUS and randomly flipped the TIMER0-sampler enable; the driver
# then had to drain the phantom sources one-per-slow-kernel-tick (~31 ms
# each) before reaching the real touch sampling, adding ~70-90 ms of
# latency per tap.  Any tap shorter than that — or one whose pen-down
# landed at an unlucky phase relative to the kernel tick — was silently
# dropped.  See core/sa1100.cpp::updateTouchInput.
#
# This boots the netBook EPOC R5 OS from CF, then taps the "Word" and
# "Sheet" Document-list entries ALTERNATELY many times in quick succession
# and checks the selection highlight tracked (almost) every tap — i.e. the
# faithful path delivers each tap, not just a lucky fraction.
#
# Exit codes:
#   0 = PASS — (nearly) every tap landed
#   1 = FAIL — too many taps dropped (the 1-in-5 regression is back)
#   2 = FAIL — the OS never reached the Document browser
#
# Usage: tests/netbook_touch_stress.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/netBook_BL_v011_eng.bin
CARD=$REPO/roms/netbook_os.img
TMP=$(mktemp -d /tmp/nbstress.XXXXXX)

TAPS=${PSION_MULTI_TAP:-30}

# Word label ~ (110,210); Sheet label ~ (110,268).  Alternate between them so
# every successful tap visibly moves the highlight.  The netBook boots from
# CF, so (unlike the Series 7 stress) it can't use --skip-card: the taps run
# after the post-attach boot window via the harness card-path multi-tap path.
PSION_MULTI_TAP=$TAPS \
PSION_TAP_JITTER=0 \
PSION_TAP_GAP=${PSION_TAP_GAP:-0.4} \
PSION_TAP_HOLD_FRAMES=${PSION_TAP_HOLD_FRAMES:-6} \
PSION_TAP_ALT_X=110 PSION_TAP_ALT_Y=268 \
"$HARNESS" "$ROM" \
    --device netbook \
    --boot-seconds 3 \
    --card-path "$CARD" \
    --post-attach-seconds ${PSION_POST_ATTACH:-95} \
    --tap-at 110 210 \
    --screenshot-every 0.2 "$TMP/f" \
    --quiet-logs \
    "$@" \
    > "$TMP/log" 2>&1

TAPS=$TAPS python3 - "$TMP" <<'PY'
import glob, sys, os
tmp = sys.argv[1]
taps = int(os.environ.get('TAPS', '30'))
frames = sorted(glob.glob(f'{tmp}/f-*.pgm'))
if len(frames) < 20:
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

# Only inspect the back half of the run (the desktop is up and the taps fire).
prev = None
toggles = 0
for i, f in enumerate(frames):
    if i < len(frames) * 0.5:
        continue
    wd = dark(f, 78, 200, 150, 222)
    sh = dark(f, 78, 258, 150, 280)
    sel = 'Word' if wd > 0.4 and wd > sh else ('Sheet' if sh > 0.4 and sh > wd else None)
    if sel and sel != prev:
        toggles += 1
        prev = sel

print(f'taps={taps} highlight-toggles={toggles}')

# Each landed tap moves the highlight to the other entry, so a healthy run
# toggles ~once per tap.  The pre-fix faithful path managed roughly half
# (and 0 at short holds); require the vast majority to land.
need = int(taps * 0.9)
if toggles < need:
    print(f'FAIL[1]: only {toggles}/{taps} taps landed (need >= {need}) '
          f'— the "1 in 5 don\'t land" regression is back')
    sys.exit(1)
print(f'PASS: {toggles}/{taps} taps landed on the faithful touch path')
sys.exit(0)
PY
RC=$?
echo ""
echo "rm -rf $TMP"
exit $RC
