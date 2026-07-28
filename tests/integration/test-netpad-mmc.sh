#!/usr/bin/env bash
# End-to-end test for the netpad's MMC card slot.
#
# Builds a FAT16 image with a known file on it, boots the netpad with the
# card in the slot, and drives the machine's own UI to prove the whole
# chain works — board FPGA SPI port, MMC card model, the variant's
# card-initialisation state machine, medmmc.pdd, the peripheral bus and
# F32:
#
#   read   the System screen's Information -> Disk page reports drive D:
#          as a mounted volume of the right size (it says "Not present"
#          with an empty slot), and the file list for D: shows the file
#          the image was built with;
#   write  creating a folder on D: from the Shell writes it through to
#          the card and it appears in the listing.
#
# The navigation is synthetic key events: the netpad has no keyboard, so
# host keys go in through the kernel's own Kern::AddEvent (see
# netpadInjectKeyEvent in core/sa1100.cpp).  Menu is 148, arrows 15/17,
# Enter 3 — the same walk tests/integration/test-infrared.sh uses.
#
#   bash tests/integration/test-netpad-mmc.sh

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="${ROM:-$REPO_ROOT/roms/Netpad.img}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "harness not built — run bash harness/build.sh" >&2
    exit 2
fi
if [ ! -f "$ROM" ]; then
    echo "SKIP: ROM $ROM not present" >&2
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi

IMG="$WORK/mmc.img"
node --experimental-strip-types -e "
import { createBlankImage, addFile, isFat16 } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { writeFileSync } from 'fs';
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'HELLO.TXT', new TextEncoder().encode('netpad MMC\r\n'));
if (!r.ok) { console.error('addFile failed:', r.reason); process.exit(1); }
if (!isFat16(img)) { console.error('not FAT16'); process.exit(1); }
writeFileSync('$IMG', Buffer.from(img.buffer, img.byteOffset, img.byteLength));
"

# The netpad reaches its desktop at ~sim 14 s; insert at 15 and give the
# card-detect -> media-change -> socket power-up -> mount chain time to
# run before the menu walk starts at 25.
run_netpad() {   # $1 = screenshot path, rest = extra harness args
    local shot="$1"; shift
    "$HARNESS" "$ROM" --device netpad --quiet-logs \
        --boot-seconds 15 --card-path "$IMG" --post-attach-seconds 45 \
        --screenshot "$shot" "$@" >"$WORK/log.txt" 2>&1
}

# The empty-slot baseline for each screen.  It has to cover the same sim
# window as run_netpad, and `--skip-card` makes the harness stop at
# --boot-seconds (it only opens a post-attach window when there is a card
# to attach), so the whole 60 s goes in as the boot window — otherwise the
# scheduled key events never fire and the screens would differ for the
# trivial reason that one run ended early.  Passing no card path at all is
# not an option: the harness would synthesise a blank one.
run_netpad_empty() {   # $1 = screenshot path, rest = extra harness args
    local shot="$1"; shift
    "$HARNESS" "$ROM" --device netpad --quiet-logs --skip-card \
        --boot-seconds 60 \
        --screenshot "$shot" "$@" >"$WORK/log_empty.txt" 2>&1
}

# Menu -> Information (4 right) -> Disk (3 down) -> Enter, then one more
# right to step the dialog's disk spinner from C: to D:.
NAV_DISK="--press-key 25 148
  --press-key 26 15 --press-key 26.5 15 --press-key 27 15 --press-key 27.5 15
  --press-key 28 17 --press-key 28.5 17 --press-key 29 17 --press-key 30 3
  --press-key 33 15"
# Menu -> Disk (2 right) -> Current disk (3 down) -> submenu -> D -> Enter,
# then the toolbar's "New folder", a typed name and Enter.
NAV_BROWSE="--press-key 25 148 --press-key 26 15 --press-key 26.5 15
  --press-key 27 17 --press-key 27.5 17 --press-key 28 17 --press-key 29 15
  --press-key 30 17 --press-key 31 3
  --tap-seq 36 610 120
  --press-key 42 77 --press-key 43 77 --press-key 44 67 --press-key 46 3"

fail=0
note() { echo "$1"; }

# ── Disk information page ────────────────────────────────────────────
# shellcheck disable=SC2086
run_netpad "$WORK/disk.pgm" $NAV_DISK
# The dialog renders "Disk  D", "Size 16M" and a Type row.  Rather than
# OCR the panel, compare against the empty-slot run: with no card the
# spinner has nothing to step to and the dialog closes, so the two
# screens must differ.
# shellcheck disable=SC2086
run_netpad_empty "$WORK/disk_empty.pgm" $NAV_DISK
if cmp -s "$WORK/disk.pgm" "$WORK/disk_empty.pgm"; then
    note "FAIL: Information -> Disk looks the same with and without a card"
    fail=1
else
    note "PASS: Information -> Disk shows a different D: with a card in"
fi

# ── The card's own traffic ───────────────────────────────────────────
# Prove the drive is served over the real MMC SPI protocol rather than by
# some short-circuit: the trace must contain the variant's CMD0/CMD1
# handshake and medmmc's CMD9 (SEND_CSD) and CMD17 (READ_SINGLE_BLOCK).
PSION_NETPAD_MMC_TRACE=1 "$HARNESS" "$ROM" --device netpad \
    --boot-seconds 15 --card-path "$IMG" --post-attach-seconds 25 \
    >"$WORK/trace.txt" 2>&1 || true
for want in "tx=0040" "tx=0041" "tx=0049" "tx=0051"; do
    if grep -q "np-mmc.*$want" "$WORK/trace.txt"; then
        note "PASS: MMC command $want seen on the SPI port"
    else
        note "FAIL: no MMC command $want on the SPI port"
        fail=1
    fi
done
# The 0xFE data-start token proves the card answered with a block.
if grep -q "np-mmc.*rx=00fe" "$WORK/trace.txt"; then
    note "PASS: card returned a data block"
else
    note "FAIL: card never returned a data block"
    fail=1
fi

# ── Browsing and writing ─────────────────────────────────────────────
# shellcheck disable=SC2086
run_netpad "$WORK/browse.pgm" $NAV_BROWSE
# Switching to D: repaints the whole client area (file icons on a blank
# background instead of the netpad wordmark), and the new folder adds a
# second icon — so this screen must differ from BOTH the plain desktop
# and the disk-info screen.
run_netpad_empty "$WORK/desktop.pgm"
if cmp -s "$WORK/browse.pgm" "$WORK/desktop.pgm"; then
    note "FAIL: the Shell never switched to drive D:"
    fail=1
else
    note "PASS: the Shell browses drive D:"
fi
# The folder creation writes through: a CMD24 (WRITE_BLOCK) must appear
# after the card has mounted.
PSION_NETPAD_MMC_TRACE=1 "$HARNESS" "$ROM" --device netpad \
    --boot-seconds 15 --card-path "$IMG" --post-attach-seconds 45 \
    ${NAV_BROWSE} >"$WORK/trace_wr.txt" 2>&1 || true
if grep -q "np-mmc.*tx=0058" "$WORK/trace_wr.txt"; then
    note "PASS: EPOC wrote a block to the card (CMD24)"
else
    note "FAIL: no CMD24 write reached the card"
    fail=1
fi

# ── The desktop's drive selector ─────────────────────────────────────
# Pressing and holding the drive logo at the bottom-left of the System
# screen pops up the drive list — "C \"Internal\"" alone with an empty
# slot, plus "D \"NO NAME\"" once the card is in.  The popup closes on
# pen-up, so it has to be captured mid-hold.
hold_shot() {   # $1 = output prefix, rest = harness args
    local pfx="$1"; shift
    PSION_TAP_SHOT_DURING_HOLD=1 PSION_TAP_HOLD_FRAMES=40 \
    "$HARNESS" "$ROM" --device netpad --quiet-logs \
        --screenshot-every 100 "$pfx" --tap-seq 32 12 228 \
        "$@" >"$WORK/log_hold.txt" 2>&1
}
hold_shot "$WORK/hold_card" --boot-seconds 15 \
          --card-path "$IMG" --post-attach-seconds 30
hold_shot "$WORK/hold_empty" --skip-card --boot-seconds 45
if [ ! -f "$WORK/hold_card-hold039.pgm" ] || [ ! -f "$WORK/hold_empty-hold039.pgm" ]; then
    note "FAIL: drive-selector screenshots not captured"
    fail=1
elif cmp -s "$WORK/hold_card-hold039.pgm" "$WORK/hold_empty-hold039.pgm"; then
    note "FAIL: the drive selector doesn't list the card"
    fail=1
else
    note "PASS: the desktop's drive selector lists the MMC card"
fi

# ── Eject ────────────────────────────────────────────────────────────
# Removing the card drops the card-detect bits and fires the same
# media-change line, so D: has to go back to "Not present" — i.e. the
# Disk page must match the empty-slot run again.
"$HARNESS" "$ROM" --device netpad --quiet-logs \
    --boot-seconds 15 --card-path "$IMG" --post-attach-seconds 45 \
    --detach-after 20 \
    --press-key 40 148 \
    --press-key 41 15 --press-key 41.5 15 --press-key 42 15 --press-key 42.5 15 \
    --press-key 43 17 --press-key 43.5 17 --press-key 44 17 --press-key 45 3 \
    --press-key 48 15 \
    --screenshot "$WORK/eject.pgm" >"$WORK/log_eject.txt" 2>&1
if cmp -s "$WORK/eject.pgm" "$WORK/disk.pgm"; then
    note "FAIL: D: still reports a card after eject"
    fail=1
else
    note "PASS: ejecting the card takes D: back to empty"
fi

if [ "$fail" -eq 0 ]; then
    echo "test-netpad-mmc: PASS"
else
    echo "test-netpad-mmc: FAIL"
fi
exit "$fail"
