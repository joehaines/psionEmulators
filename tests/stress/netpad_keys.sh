#!/usr/bin/env bash
# Key-delivery and OS-timer-rate test for the netpad.
#
# The netpad is a pen machine: no keyboard matrix, no BSP scanner, just
# two assignable case buttons and EPOC's on-screen Psiboard.  Two things
# used to be broken as a result, and this test guards both:
#
#   1. Nothing delivered host keys at all — setKeyboardKey drove the
#      Series 7 / netBook Eiger matrix and returned early otherwise — so
#      the frontend's Menu button (the only route to EPOC's menu bar on a
#      machine with no Menu key, and hence to Remote link, battery info
#      and the rest of the Tools menu) did nothing.  Keys now go in as
#      synthetic TRawEvents through the netpad kernel's own
#      Kern::AddEvent (see netpadInjectKeyEvent in core/sa1100.cpp).
#
#   2. The OS timer ran at 10x the spec-correct 3.6864 MHz (a Series 7 /
#      netBook workaround the netpad inherited), so every duration the
#      guest measured was ten times too long.  EPOC's key auto-repeat
#      (Control panel -> Keyboard: 800 ms initial, 50 ms subsequent) then
#      fired *during* a normal press: one ~125 ms press typed nine
#      characters.  The same skew expired the board codec's SSP settling
#      waits before the transfers had clocked, leaving stale words in the
#      RX FIFO — which misaligned the next batch (the backup-battery
#      channel read 0, i.e. "Backup battery status: Defective") and, when
#      it landed during an app launch, wedged the UI half-painted.
#
# Checks:
#   1. Menu opens the System screen's menu bar.
#   2. One 125 ms key press types exactly one character.
#   3. The SSP never reports a receive overrun — the board codec's FIFO
#      stays aligned, so the battery/temperature channels read true.
#
# Exit codes: 0 = PASS, 1 = a check failed, 3 = the netpad never booted.
#
# Usage: tests/stress/netpad_keys.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/Netpad.img
TMP=$(mktemp -d /tmp/npkeys.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then bash "$REPO/harness/build.sh" >&2; fi

# Reference frame: the bare desktop, for the menu-bar ink comparison.
"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 20 --screenshot "$TMP/desktop.pgm" "$@" \
    > "$TMP/desktop.log" 2>&1

# Menu (EStdKeyMenu = 148) on the System screen.
"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 24 --press-key 18 148 8 --screenshot "$TMP/menu.pgm" "$@" \
    > "$TMP/menu.log" 2>&1

# One 'x' (EPOC scan code 88) into the "Create new file" name field,
# held 8 frames = 125 ms — a normal browser click.  The SSP trace comes
# from this run because it is the one with stylus traffic on the codec
# (no --quiet-logs here: that switches the logger off entirely).
PSION_NETPAD_SSP_TRACE=1 "$HARNESS" "$ROM" --device netpad --skip-card \
    --boot-seconds 26 \
    --tap-seq 12 608 143 --press-key 20 88 8 \
    --screenshot "$TMP/typed.pgm" --log-file "$TMP/ssp.log" "$@" \
    > "$TMP/typed.log" 2>&1

# Right-arrow on the open menu bar, held 8 frames = 125 ms.  Three runs:
# a plain press, the same press with the host OS repeating it into the
# emulator as a stream of key-downs (--repeat-key, what a browser
# delivers for a held key), and that repeat again with the netpad's
# key-edge filter switched off so the test also proves it is what fixes
# the run-away.
"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 28 --press-key 18 148 8 --press-key 21 15 8 \
    --screenshot "$TMP/arrow_once.pgm" "$@" > "$TMP/arrow_once.log" 2>&1
"$HARNESS" "$ROM" --device netpad --skip-card --quiet-logs \
    --boot-seconds 28 --press-key 18 148 8 --repeat-key 21 15 8 \
    --screenshot "$TMP/arrow_repeat.pgm" "$@" > "$TMP/arrow_repeat.log" 2>&1
PSION_NETPAD_NO_KEY_DEDUPE=1 "$HARNESS" "$ROM" --device netpad --skip-card \
    --quiet-logs --boot-seconds 28 --press-key 18 148 8 --repeat-key 21 15 8 \
    --screenshot "$TMP/arrow_nodedupe.pgm" "$@" > "$TMP/arrow_nodedupe.log" 2>&1

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


def ink(path, x0, y0, x1, y1, thr=90):
    """Dark-pixel count in a region."""
    w, h, d = load(path)
    n = 0
    for y in range(y0, y1):
        base = y * w
        for x in range(x0, x1):
            if d[base + x] < thr:
                n += 1
    return n


# ── 1. Menu opens the menu bar ───────────────────────────────────────
# The top strip left of the System bar is nearly blank on the desktop
# (just the folder icon's top edge) and carries "File Edit Disk View
# Information Tools" once the menu bar is up.
try:
    bare = ink(f'{tmp}/desktop.pgm', 0, 0, 440, 15)
    menu = ink(f'{tmp}/menu.pgm', 0, 0, 440, 15)
except Exception as e:                                   # noqa: BLE001
    print(f'FAIL[3]: could not read screenshots ({e}) — netpad may not have booted')
    sys.exit(3)
print(f'menu-bar ink: desktop={bare} after-Menu={menu}')
keys_delivered = menu >= bare * 1.5
if not keys_delivered:
    print('FAIL[1]: the Menu key did not open the menu bar — host keys are '
          'not reaching the netpad kernel (netpadInjectKeyEvent)')
    rc = 1

# ── 2. One press types one character ─────────────────────────────────
# The name field starts with "Data" selected, so a single keystroke
# replaces it.  One glyph is ~37 dark pixels here; the auto-repeat bug
# produced nine ("xxxxxxxxx", ~338).  Only meaningful once check 1 has
# established that keys arrive at all — otherwise we would just be
# measuring the untouched "Data" the field came up with.
typed = ink(f'{tmp}/typed.pgm', 292, 88, 455, 102)
print(f'name-field ink after one 125 ms press: {typed}')
if not keys_delivered:
    print('SKIP: character count is meaningless while no keys are delivered')
elif typed == 0:
    print('FAIL[1]: nothing was typed — the key never reached the editor')
    rc = 1
elif typed > 90:
    print('FAIL[1]: one key press produced several characters — EPOC key '
          'auto-repeat is firing mid-press, i.e. the OS timer is running '
          'fast again (osTimerScale)')
    rc = 1

# ── 3. The board codec's SSP FIFO stays aligned ──────────────────────
# SSSR bit 6 is ROR (receive overrun).  It only ever set because a
# client abandoned a transfer early — the tell-tale of guest waits
# expiring before the SSP had clocked its frames — and the stale words
# left behind shifted the next batch's results by a word.
ror = 0
total = 0
with open(f'{tmp}/ssp.log', encoding='utf-8', errors='replace') as fh:
    for line in fh:
        m = re.search(r'SSSR -> ([0-9a-f]+)', line)
        if not m:
            continue
        total += 1
        if int(m.group(1), 16) & (1 << 6):
            ror += 1
print(f'SSP status reads: {total} (receive-overrun: {ror})')
if total == 0:
    print('FAIL[1]: no SSP traffic — the board codec was never driven')
    rc = 1
elif ror:
    print('FAIL[1]: the SSP reported a receive overrun — stale words are '
          'left in the codec FIFO, which misaligns the next conversion '
          '(a zero backup-battery reading: "Backup battery: Defective")')
    rc = 1

# ── 4. A held arrow key steps the menu bar once ──────────────────────
# The host OS repeats a held key into the browser as a stream of
# keydown events.  A matrix machine absorbs those (the bit is already
# set); the netpad turns every down into its own TRawEvent, so they
# stacked on top of EPOC's own auto-repeat and one 125 ms press of Right
# ran from File all the way past Information.  setKeyboardKey now
# forwards only genuine key edges.
try:
    once = load(f'{tmp}/arrow_once.pgm')[2]
    rept = load(f'{tmp}/arrow_repeat.pgm')[2]
    nodd = load(f'{tmp}/arrow_nodedupe.pgm')[2]
except Exception as e:                                   # noqa: BLE001
    print(f'FAIL[3]: could not read the arrow-key screenshots ({e})')
    sys.exit(3)
if keys_delivered:
    if rept != once:
        print('FAIL[1]: a held Right arrow moved further than a single '
              'press — host key auto-repeat is being forwarded as extra '
              'key-down events (setKeyboardKey key-edge filter)')
        rc = 1
    elif nodd == once:
        print('FAIL[1]: the run-away did not reproduce with the key-edge '
              'filter disabled — this check is no longer testing anything')
        rc = 1
    else:
        print('held Right arrow: one menu step (run-away reproduces with '
              'PSION_NETPAD_NO_KEY_DEDUPE=1)')
else:
    print('SKIP: arrow-key check is meaningless while no keys are delivered')

if rc == 0:
    print('PASS netpad keys: Menu opens the menu bar, one press types one '
          'character, a held arrow steps once, board-codec FIFO stays aligned')
sys.exit(rc)
PY
