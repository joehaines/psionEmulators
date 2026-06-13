#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Faithful CompactFlash boot test for the netBook YModem bootloader.
#
# GOAL (the "real CF reads" target): the bootloader should load the OS from
# the CompactFlash card *through its own code path* —
#
#     boot-app 0x500c6064  ->  OS-loader 0x500c6db0  ->  RFile::Open "D:\OS.IMG"
#       ->  FAT16 FSY  ->  LocDrv  ->  ATA media driver  ->  emulated CF taskfile
#
# The single authoritative success signal is therefore CF_STATS' ata_cmds:
# if the bootloader is genuinely reading the card, ata_cmds > 0.  Reaching
# the boot-app / OS-loader is necessary-but-insufficient progress and is
# reported separately so we can see how far the chain gets.
#
# This is deliberately NOT the YModem-over-serial route and NOT the
# synthetic netBookLoadOsFromCard() image-copy shortcut — both bypass the
# real CF read path.  We pass PSION_NB_NATIVE_CF_NO_HANDOFF to keep the
# synthetic handoff out of the way.
#
# Usage:
#   scripts/test-netbook-cf-bootloader.sh [boot-seconds] [post-attach-seconds]
#
# Exit status: 0 if ata_cmds > 0 (faithful CF read achieved), 1 otherwise.
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/../.."

BOOT="${1:-6}"
ATTACH="${2:-6}"
ROM="roms/netBook_BL_v011_eng.bin"
RUN="harness/run"
LOGDIR="/tmp/nb-cf-bl"
LOG="$LOGDIR/run.log"
ERR="$LOGDIR/run.err"
mkdir -p "$LOGDIR"

# The faithful path mounts the CF as a FAT16 volume and opens D:\OS.IMG as a
# FILE — so it needs a real FAT16 disk image, NOT the raw EPOCARM ROM
# (roms/netbook_os.img starts with "EPOCARM ROM" and has no FAT boot sector;
# only the synthetic netBookLoadOsFromCard handoff handles the raw image).
# Build the FAT16 image the way the frontend's "Insert CF card" dialog does.
CARD="$LOGDIR/cf_fat16.img"
if command -v node >/dev/null 2>&1; then
    node --experimental-strip-types scripts/build-cf-fat16-image.mts \
        roms/netbook_os.img "$CARD" 24 >/dev/null 2>&1 \
        || { echo "FAT16 image build failed" >&2; CARD="roms/netbook_os.img"; }
else
    echo "node not found — falling back to raw image (faithful mount will fail)" >&2
    CARD="roms/netbook_os.img"
fi

if [[ ! -x "$RUN" ]]; then
    echo "harness/run missing — building..." >&2
    bash harness/build.sh >/dev/null 2>&1 || { echo "BUILD FAILED" >&2; exit 2; }
fi

# PSION_NB_NATIVE_CF wires up the emulated CF card; NATIVE_CF_NO_HANDOFF keeps
# the synthetic netBookLoadOsFromCard image-copy shortcut out of the way so the
# bootloader must read the card through its own driver path.
#
# SKIP_WFI / FLUSH_FIX (which NOP the kernel's WFI idle-park and the cache-flush
# stall) are no longer used: they suppress the natural WFI idle that the
# PC-Card power-up state machine relies on (its per-step DFC re-queues on the
# 64 Hz tick that fires while the kernel idles), so with them the power-up never
# completes.  With the faithful card-detect/card-ready/socket-1 modelling the
# driver now reaches the boot-app, powers up the socket, enumerates the card
# and issues ATA reads naturally, so these diagnostic levers are dropped as the
# header anticipated.
export PSION_NB_NATIVE_CF=1
export PSION_NB_NATIVE_CF_NO_HANDOFF=1
# This test measures the FULL faithful READ (ata_cmds).  Disable the
# read-complete -> boot handoff, which now fires early (~6000 sectors) to dodge
# the post-park white screen (see netBookLoadOsFromCard); with it off the
# bootloader reads the whole OS.IMG so we can score the real read.
export PSION_NB_NO_FAITHFUL_BOOT=1

echo "=== netBook bootloader faithful-CF test ==="
echo "rom=$ROM card=$CARD boot=${BOOT}s post-attach=${ATTACH}s"

timeout 300 "$RUN" "$ROM" \
    --boot-seconds "$BOOT" \
    --card-path "$CARD" \
    --post-attach-seconds "$ATTACH" \
    --log-file "$LOG" \
    >"$LOGDIR/run.stdout" 2>"$ERR"
rc=$?

stats=$(grep -h "^CF_STATS:" "$ERR" | tail -1)
ata=$(echo "$stats" | sed -n 's/.*ata_cmds=\([0-9]*\).*/\1/p')
drains=$(echo "$stats" | sed -n 's/.*sector_drains=\([0-9]*\).*/\1/p')
ata="${ata:-0}"; drains="${drains:-0}"

reached_bootapp=$(grep -c "BOOT-APP:" "$ERR" 2>/dev/null || echo 0)
reached_loader=$(grep -c "OS-LOADER:" "$ERR" 2>/dev/null || echo 0)

echo "--- progress markers ---"
echo "boot-app reached (0x500c60xx) : $([ "$reached_bootapp" -gt 0 ] && echo yes || echo NO)"
echo "OS-loader reached (0x500c6db0): $([ "$reached_loader" -gt 0 ] && echo yes || echo NO)"
echo "ata_cmds                      : $ata"
echo "sector_drains                 : $drains"
echo "harness rc                    : $rc"
echo "logs                          : $ERR"

if [[ "$ata" -gt 0 ]]; then
    echo "RESULT: PASS — bootloader issued real ATA reads from CF (faithful CF works)"
    exit 0
else
    echo "RESULT: FAIL — no ATA reads (drive D: not mounted / OS-loader bailed)"
    exit 1
fi
