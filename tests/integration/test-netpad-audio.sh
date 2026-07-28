#!/usr/bin/env bash
# netpad audio (AC'97 codec on the board FPGA) validation.
#
# Boots roms/Netpad.img and then drives the codec over the emulated bus
# with the exact register sequence the ROM's own sound PDD performs —
# \System\Libs\Esdrv.pdd, published as "Sound.Ac97", whose record path
# is at ROM 0x502e7ac8 and play path at 0x502e795c — with a tone pushed
# in through the host microphone bridge.
#
# It has to be driven from the harness rather than from the machine's UI
# because Netpad.img ships no recorder: the only audio app in the image
# is Control panel -> "AC97 Record", which just stores the three gain
# settings in publish-and-subscribe keys, and the key clicks the Sound
# control panel governs go to a separate board register rather than
# through the codec.  See docs/netpad-rom-and-mmc.md.
#
# Checks, in order:
#   - the codec powers up and reports its four sections ready;
#   - a tone pushed into the host microphone comes back out of the PCM
#     data register in order and unaltered;
#   - it arrives at the codec's programmed sample rate rather than as
#     fast as the guest can read;
#   - samples written to the same register reach the host speaker;
#   - the receive FIFO raises its service interrupt on the FPGA.
#
#   bash tests/integration/test-netpad-audio.sh

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/netpad-audio-harness"
ROM="${ROM:-$REPO_ROOT/roms/Netpad.img}"

if [ ! -f "$ROM" ]; then
    echo "SKIP: ROM $ROM not present" >&2
    exit 0
fi
if [ ! -x "$HARNESS" ]; then
    echo "netpad-audio-harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2 || exit 1
fi

"$HARNESS" "$ROM"
rc=$?
if [ $rc -ne 0 ]; then
    echo "FAIL: netpad audio harness exited $rc (see its output above for" \
         "which stage failed)" >&2
    exit 1
fi
echo "PASS: netpad microphone and speaker" >&2
