#!/usr/bin/env bash
# End-to-end test for netBook keyboard input reaching an OS image that the
# emulator has no kernel-level knowledge of.
#
# The netBook and Series 7 have long taken their key events through a
# synthetic path: the emulator calls the kernel's Kern::AddEvent directly, at
# addresses that only exist in the v1.05(450) / v254 builds.  ESHELL is the
# same machine running a different EPOC image, so that path can't touch it —
# it has to work the way the hardware does:
#
#   host key -> kbdMatrix_ -> Eiger[0x30] column drive / Eiger[0x04] row
#   readback -> the ROM's own ekeyb.dll tick poll -> EPOC key event
#
# The test types "dir" + Enter at the ESHELL prompt and requires
#
#   screen   the console changed versus an untouched boot (ESHELL echoes the
#            command and prints its directory listing);
#   hardware the ROM actually scanned the matrix — the trace shows non-zero
#            rows read back from Eiger[0x04] under a real column drive, so
#            the keys travelled through the registers rather than a shortcut;
#   scope    the synthetic path declined this ROM instead of writing into a
#            kernel whose layout it doesn't know.
#
#   bash tests/integration/test-netbook-eshell-keyboard.sh

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="${ROM:-$REPO_ROOT/roms/netBook_BL_v011_eng.bin}"
OSIMG="${OSIMG:-$REPO_ROOT/roms/ESHELL/OS.IMG}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "harness not built — run bash harness/build.sh" >&2
    exit 2
fi
for f in "$ROM" "$OSIMG"; do
    if [ ! -f "$f" ]; then
        echo "SKIP: $f not present" >&2
        exit 0
    fi
done
if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi

# The same FAT16 boot card the frontend synthesises for the 'eshell'
# osCardSpec variant: ESHELL's OS.IMG as D:\OS.IMG.
IMG="$WORK/eshell.img"
node --experimental-strip-types -e "
import { createBlankImage, addFile } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'fs';
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'OS.IMG', new Uint8Array(readFileSync('$OSIMG')), 0);
if (!r.ok) { console.error('addFile failed:', r.reason); process.exit(1); }
writeFileSync('$IMG', Buffer.from(img.buffer, img.byteOffset, img.byteLength));
"

# ESHELL is at its C:\> prompt by ~sim 20 s (3 s of bootloader splash, then
# the faithful CF read and the OS's own cold boot).  Type from 22 s on, one
# key at a time, and let the console settle before the screenshot.
#   68='D' 73='I' 82='R' 3=Enter
TYPE_DIR="--press-key 22 68 12 --press-key 24 73 12
          --press-key 26 82 12 --press-key 28 3 12"

run_eshell() {   # $1 = screenshot path, rest = extra harness args
    local shot="$1"; shift
    PSION_S7_KBD_TRACE=1 "$HARNESS" "$ROM" --device netbook \
        --boot-seconds 3 --card-path "$IMG" --post-attach-seconds 34 \
        --screenshot "$shot" "$@" >"$WORK/$(basename "$shot").log" 2>&1
}

fail=0
note() { echo "$1"; }

# ── The console reacts to typing ─────────────────────────────────────
run_eshell "$WORK/typed.pgm" $TYPE_DIR
run_eshell "$WORK/quiet.pgm"

if cmp -s "$WORK/typed.pgm" "$WORK/quiet.pgm"; then
    note "FAIL: the ESHELL console is identical with and without typing"
    fail=1
else
    note "PASS: typing 'dir' changed the ESHELL console"
fi

# ── The keys went through the keyboard matrix ────────────────────────
# A row readback of 0 proves nothing (that is the idle state), so require a
# non-zero row byte under a column drive of 8+n — i.e. the ROM walking the
# columns and finding the key we are holding.
if grep -qE 'kbd-trace\] asic\[0x04\] rows -> 0x[0-9a-f]*[1-9a-f][0-9a-f]*  drive=[89a-f]' \
        "$WORK/typed.pgm.log"; then
    note "PASS: the ROM scanned the matrix and read the pressed key back"
else
    note "FAIL: no non-zero row readback under a column drive — the ROM's"
    note "      ekeyb never saw the key at Eiger[0x04]"
    fail=1
fi

# ── …and NOT through the synthetic kernel-event path ─────────────────
if grep -q "kernel event ring not recognised" "$WORK/typed.pgm.log"; then
    note "PASS: the synthetic Kern::AddEvent path declined this ROM"
else
    note "FAIL: the synthetic path did not decline an unrecognised kernel —"
    note "      it may be calling into ESHELL's memory at netBook addresses"
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "=== netBook ESHELL keyboard test FAILED ==="
    exit 1
fi
echo "=== netBook ESHELL keyboard test PASSED ==="
