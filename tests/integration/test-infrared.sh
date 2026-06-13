#!/usr/bin/env bash
# Drives the native harness to an EPOC "Infrared receive" dialog and validates
# the JS IrDA stack's discovery / connect / file-beam against the real device.
#
# Devices (DEVICE=...):
#   5mx     Windermere CL-PS7110 successor (PROVEN)         — UART1 SIR
#   osaris  CL-PS7111 (EPOC R5, 320x200 mono touch)         — UART1 SIR, verified
#   series5 CL-PS7110 (EPOC R1, 640x240 touch)              — UART1 SIR, verified
#   series7 SA-1100 (EPOC R5, 640x480, ICP=SER2)            — UART2 ICP, verified
#   netbook SA-1100 (EPOC R5 v450, boots OS from CF)        — UART2 ICP, verified
#
# On the SA-1100 (Series 7 + netBook) the IrDA transceiver is the on-chip
# Infrared Communications Port wired to SER2/UART2 (UART3 is the cable port).
# The netBook boots its EPOC R5 OS image from a CompactFlash card, so it uses
# the boot/post-attach harness phasing (with PSION_REALTIME so its in-sim
# IrLAP timers track the wall-clock host bridge) rather than --serial-poll-until.
#
# Background (the original 5mx bug)
# ---------------------------------
# The "No infrared device responding" failure was an IrLAP wire-format bug:
# we sent the XID discovery command with control byte 0xAF, but the standard
# HDLC/IrLAP XID *command* control is 0x2F. The real 5mx received our frame
# (valid FCS + address) but, not recognising the control as XID, replied
# with DM (0x0F) instead of an XID response — so discovery found nobody.
# Fixing U_XID to 0x2F (and accepting the secondary's 0xAF XID *response*)
# makes the device answer discovery.
#
# Reaching receive mode arms the UART1 SIR encoder (SYSCON1 SIREN/IRTXM on the
# CL-PS711x; portcon irdatx on Windermere) and the device answers IrLAP
# discovery on UART1.
#
# Modes:
#   scripts/test-infrared.sh capture   # inject our discovery, dump the
#                                       # device's XID response (default)
#   scripts/test-infrared.sh live      # run the real IrSendClient over a
#                                       # socket bridge (discover+connect,
#                                       # +sendFile when IR_SEND_FILE=1)
#   scripts/test-infrared.sh trace      # full beam + LM-PDU trace + final
#                                       # inbox screenshot
#
# Env overrides:
#   DEVICE=5mx|osaris|series5   device to drive (default 5mx)
#   ROM=...                     override ROM path
#   UART=N                      serial UART index (default 1)
#   ATTACH_SEC=N                sim-time to attach host bridge
#   POLL_UNTIL=N                sim-time to run to
#   OSARIS_NAV="..."            override the osaris nav token array
#   SERIES5_NAV="..."           override the series5 nav token array
#   FIVEMX_NAV="..."            override the 5mx nav token array
#   IR_SEND_FILE=1              exercise the full file beam (live/trace)
#   IR_HOLD_MS=N                keep the link up N ms after sendFile (bridge)

set -eu
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
MODE="${1:-capture}"
DEVICE="${DEVICE:-5mx}"

# Per-device defaults filled in by the case block below. BOOT_MODE selects how
# the harness is driven: "poll" runs continuously to POLL_UNTIL sim-seconds
# (ROM-booting devices); "card" boots, attaches a CF OS image, then runs a
# post-attach window (the netBook). DEF_UART is the SoC UART the IrDA
# transceiver is wired to (1 = SIR on Windermere/CL-PS711x, 2 = ICP on SA-1100).
DEF_UART=1
BOOT_MODE="poll"
DEF_CARD=""
DEF_BOOT_SEC=3

if [ ! -x "$HARNESS" ]; then bash "$REPO_ROOT/harness/build.sh" >&2; fi

# Per-device ROM, navigation token array (passed straight to the harness as
# --press-key AT_SEC EPOC_KEY ... tokens), bridge-attach time and run window.
#
# EPOC keys used: Menu=148, Right=15, Down=17, Enter=3.
case "$DEVICE" in
  5mx)
    DEF_ROM="$REPO_ROOT/roms/5mx_v1.05(260)_eng.bin"
    # Menu -> Right x5 (Tools) -> Down x3 (Infrared) -> Right -> Down -> Enter
    DEF_NAV="--press-key 35 148 \
      --press-key 36 15 --press-key 36.5 15 --press-key 37 15 --press-key 37.5 15 --press-key 38 15 \
      --press-key 39 17 --press-key 39.5 17 --press-key 40 17 \
      --press-key 41 15 \
      --press-key 42 17 \
      --press-key 43 3"
    DEF_ATTACH=44; DEF_POLL=250; DEF_DISCOVER=45000
    DEF_CONNECT=3000; DEF_SEND_DISC=1000; DEF_SEND_REQ=8000; DEF_SEND_RETRY=15000
    NAV_OVERRIDE="${FIVEMX_NAV:-}"
    ;;
  osaris)
    DEF_ROM="$REPO_ROOT/roms/Osaris_v1.02(209)_eng.bin"
    # Desktop renders ~45s. Menu bar: File Edit Disk View Information Tools.
    # Menu -> Right x5 (Tools) -> Down x4 (Infrared, 5th item) -> Right (submenu,
    # Send highlighted) -> Down (Receive) -> Enter -> "Infrared receive" dialog.
    DEF_NAV="--press-key 45 148 \
      --press-key 46 15 --press-key 46.5 15 --press-key 47 15 --press-key 47.5 15 --press-key 48 15 \
      --press-key 48.5 17 --press-key 49 17 --press-key 49.5 17 --press-key 50 17 \
      --press-key 50.5 15 \
      --press-key 51 17 \
      --press-key 51.5 3"
    # Osaris runs ~1.25x slower than realtime; dialog at ~52s sim ≈ 65s wall.
    DEF_ATTACH=53; DEF_POLL=250; DEF_DISCOVER=180000
    DEF_CONNECT=6000; DEF_SEND_DISC=2000; DEF_SEND_REQ=12000; DEF_SEND_RETRY=30000
    NAV_OVERRIDE="${OSARIS_NAV:-}"
    ;;
  series5)
    DEF_ROM="$REPO_ROOT/roms/series5_v1.01(144)_eng.bin"
    # Desktop renders ~55s sim. Same EPOC R1 menu bar as the 5mx:
    # File Edit Disk View Information Tools. Menu -> Right x5 (Tools) ->
    # Down x3 (Infrared, 4th item) -> Right (submenu, Send highlighted) ->
    # Down (Receive) -> Enter -> "Infrared receive" dialog.
    #
    # STATUS: FULLY WORKING end-to-end — `live`/`trace` report discovery,
    # connect AND sendFile SUCCEEDED; the beamed file lands in the Series 5
    # Documents view (device replies "ACK Y" then disconnects to save it).
    # EPOC R1 uses the EikonIr *v1.0* IAS class (R5 devices use v2.0); the
    # stack already falls back to v1.0.
    #
    # Root cause of the former "drops every I-frame after UA" failure, found by
    # disassembling the R1 IrLAP received-frame dispatch (ROM 0x500961b8):
    #   * R1 DOES enter NRM on UA (link state == 3). FCS, addressing of the
    #     U-frames, discovery — all fine.
    #   * For each inbound frame R1 validates the connection address with
    #       (frameAddr & 0xFE) == storedConnAddr      (ROM 0x50096200)
    #     where storedConnAddr is the SNRM "new connection address" byte kept
    #     VERBATIM. We used to send that byte as 0x03 = (conn<<1)|C/R, so R1
    #     stored an ODD value; the even-masked compare ((x & 0xFE) is always
    #     even) could never match it, so every numbered I/S frame was silently
    #     dropped while the link sat in NRM until its DM timeout.
    #   * Fix: send the SNRM connection-address byte in EVEN form 0x02 =
    #     (conn<<1)|0 (lib/irda/irlap.ts). R1 then stores 0x02; our command
    #     frames at 0x03 mask to 0x02 and validate. 5mx/Osaris normalise the
    #     address internally and are unaffected (regression-tested green).
    DEF_NAV="--press-key 75 148 \
      --press-key 76 15 --press-key 76.5 15 --press-key 77 15 --press-key 77.5 15 --press-key 78 15 \
      --press-key 78.5 17 --press-key 79 17 --press-key 79.5 17 \
      --press-key 80 15 \
      --press-key 80.5 17 \
      --press-key 81 3"
    # Series 5 runs ~2.5x slower than realtime; dialog at ~81s sim ≈ 200s wall.
    DEF_ATTACH=82; DEF_POLL=400; DEF_DISCOVER=360000
    # Series 5 is ~2.5x slow: SNRM turnaround + upper-layer requests need
    # generous wall-clock windows.
    DEF_CONNECT=15000; DEF_SEND_DISC=8000; DEF_SEND_REQ=30000; DEF_SEND_RETRY=60000
    NAV_OVERRIDE="${SERIES5_NAV:-}"
    ;;
  series7)
    DEF_ROM="$REPO_ROOT/roms/series7_v1.05(254)_b756_eng.bin"
    DEF_UART=2   # IrDA = SA-1100 ICP on SER2/UART2
    # EPOC R5 desktop renders by ~sim 35s. Menu bar: File Edit Disk View
    # Information Tools. Menu -> Right x5 (Tools) -> Down x4 (Infrared, after
    # Preferences/Control panel/Re-install/Link-to-desktop) -> Right (submenu,
    # Send highlighted) -> Down (Receive) -> Enter -> "Infrared receive".
    DEF_NAV="--press-key 35 148 \
      --press-key 36 15 --press-key 36.5 15 --press-key 37 15 --press-key 37.5 15 --press-key 38 15 \
      --press-key 38.5 17 --press-key 39 17 --press-key 39.5 17 --press-key 40 17 \
      --press-key 40.5 15 \
      --press-key 41 17 \
      --press-key 41.5 3"
    # Series 7 boots from its 16 MB ROM directly (no CF). Dialog up ~sim 42.
    DEF_ATTACH=44; DEF_POLL=250; DEF_DISCOVER=80000
    DEF_CONNECT=8000; DEF_SEND_DISC=4000; DEF_SEND_REQ=20000; DEF_SEND_RETRY=40000
    NAV_OVERRIDE="${SERIES7_NAV:-}"
    ;;
  netbook)
    DEF_ROM="$REPO_ROOT/roms/netBook_BL_v011_eng.bin"
    DEF_UART=2          # IrDA = SA-1100 ICP on SER2/UART2
    BOOT_MODE="card"    # boots EPOC R5 OS image from CF, then runs post-attach
    DEF_CARD="${NB_CARD:-$REPO_ROOT/roms/netbook_os.img}"
    DEF_BOOT_SEC=3
    # Same EPOC R5 shell as the Series 7, but the OS handoff happens after the
    # CF attach (~sim 3) so the desktop renders later (~sim 45). Walk the same
    # Tools -> Infrared -> Receive path, scheduled well after the desktop is up.
    DEF_NAV="--press-key 50 148 \
      --press-key 51 15 --press-key 51.5 15 --press-key 52 15 --press-key 52.5 15 --press-key 53 15 \
      --press-key 53.5 17 --press-key 54 17 --press-key 54.5 17 --press-key 55 17 \
      --press-key 55.5 15 \
      --press-key 56 17 \
      --press-key 56.5 3"
    # Attach the bridge after the dialog is up (~sim 57). POLL_UNTIL is the
    # absolute sim-end time (the card boot path turns it into a post-attach
    # window); with PSION_REALTIME the run tracks wall time so the host
    # bridge's discovery/connect/beam complete in-sim.
    DEF_ATTACH=58; DEF_POLL=143; DEF_DISCOVER=90000
    DEF_CONNECT=8000; DEF_SEND_DISC=8000; DEF_SEND_REQ=25000; DEF_SEND_RETRY=50000
    NAV_OVERRIDE="${NETBOOK_NAV:-}"
    ;;
  *)
    echo "unknown DEVICE=$DEVICE (want 5mx|osaris|series5|series7|netbook)" >&2; exit 2 ;;
esac
UART="${UART:-$DEF_UART}"
CARD="${CARD:-$DEF_CARD}"

ROM="${ROM:-$DEF_ROM}"
ATTACH_SEC="${ATTACH_SEC:-$DEF_ATTACH}"
POLL_UNTIL="${POLL_UNTIL:-$DEF_POLL}"
NAV_STR="${NAV_OVERRIDE:-$DEF_NAV}"
if [ "$NAV_STR" = "__UNSET__" ]; then
  echo "DEVICE=$DEVICE has no NAV yet; set ${DEVICE^^}_NAV" >&2; exit 2
fi
# shellcheck disable=SC2206
NAV=( $NAV_STR )

# Emit the harness flags that run the sim up to <end> sim-seconds. ROM-booting
# devices ("poll") run continuously via --serial-poll-until; the netBook
# ("card") boots, attaches its CF OS image, then runs a post-attach window —
# the scripted --press-key / --serial-* events fire at their absolute sim
# times along the way either way.
boot_flags() {
  local end="$1"
  if [ "$BOOT_MODE" = "card" ]; then
    echo "--boot-seconds $DEF_BOOT_SEC --card-path $CARD --post-attach-seconds $((end - DEF_BOOT_SEC))"
  else
    echo "--serial-poll-until $end"
  fi
}
# Card-boot devices run the sim as fast as they can (the netBook idles ~7x
# real-time), which races ahead of the wall-clock host bridge and slams the
# device's IrLAP timers shut before the beam lands. PSION_REALTIME pins the
# sim to wall time for the live/trace bridge modes. (Capture injects static
# bytes and doesn't care about pacing, so it stays fast.)
if [ "$BOOT_MODE" = "card" ] && { [ "$MODE" = "live" ] || [ "$MODE" = "trace" ]; }; then
  export PSION_REALTIME=1
fi

if [ "$MODE" = "capture" ]; then
  # Regenerate the current discovery wire bytes from the live stack.
  CONCAT=$(cd "$REPO_ROOT/frontend" && \
    node --experimental-strip-types src/lib/__tests__/_dump_irda.mts 2>/dev/null \
    | sed -n 's/^SIR: //p' | tr '\n' ' ')
  CAP=$(mktemp)
  "$HARNESS" "$ROM" --device "$DEVICE" "${NAV[@]}" \
    --serial-attach "$UART" "$ATTACH_SEC" \
    --serial-tx "$UART" "$((ATTACH_SEC+1))" "$CONCAT" \
    --serial-tx "$UART" "$((ATTACH_SEC+3))" "$CONCAT" \
    --serial-tx "$UART" "$((ATTACH_SEC+5))" "$CONCAT" \
    --serial-capture "$CAP" $(boot_flags "$((ATTACH_SEC+9))") >/dev/null 2>&1
  echo "Device transmitted $(wc -c < "$CAP") bytes in response to discovery:"
  od -An -tx1 "$CAP"
  rm -f "$CAP"
elif [ "$MODE" = "live" ]; then
  # The bridge connects the socket at harness launch but the device only
  # answers discovery once it reaches the receive dialog (~ATTACH_SEC sim,
  # which on the slower CL-PS711x devices is well past the 5mx's 45s wall).
  # Give the discovery loop a window comfortably past the dialog + beam.
  export IR_DISCOVER_MS="${IR_DISCOVER_MS:-$DEF_DISCOVER}"
  export IR_CONNECT_MS="${IR_CONNECT_MS:-$DEF_CONNECT}"
  export IR_SEND_DISCOVER_MS="${IR_SEND_DISCOVER_MS:-$DEF_SEND_DISC}"
  export IR_SEND_REQUEST_MS="${IR_SEND_REQUEST_MS:-$DEF_SEND_REQ}"
  export IR_SEND_RETRY_MS="${IR_SEND_RETRY_MS:-$DEF_SEND_RETRY}"
  SOCK=$(mktemp -u)
  ( cd "$REPO_ROOT/frontend" && \
    node --experimental-strip-types src/lib/__tests__/_ir_bridge.mts "$SOCK" ) &
  BRIDGE=$!
  sleep 1
  "$HARNESS" "$ROM" --device "$DEVICE" "${NAV[@]}" \
    --serial-attach "$UART" "$ATTACH_SEC" --serial-bridge-socket "$SOCK" \
    $(boot_flags "$POLL_UNTIL") >/dev/null 2>&1 &
  HPID=$!
  wait "$BRIDGE" || true
  kill "$HPID" 2>/dev/null || true
  rm -f "$SOCK"
elif [ "$MODE" = "trace" ]; then
  # Full Eikon-IR beam with an LM-PDU-level trace of the REAL device's bytes
  # (IAS response, TinyTP confirm, "ACK Y"), a device log, and a final
  # screenshot of the Infrared-receive dialog so we can confirm the file
  # arrived. Artefacts land in ${IR_TRACE_DIR:-/tmp/ir-trace}.
  OUT_DIR="${IR_TRACE_DIR:-/tmp/ir-trace}"
  mkdir -p "$OUT_DIR"
  export IR_DISCOVER_MS="${IR_DISCOVER_MS:-$DEF_DISCOVER}"
  export IR_SEND_DISCOVER_MS="${IR_SEND_DISCOVER_MS:-$DEF_SEND_DISC}"
  export IR_SEND_REQUEST_MS="${IR_SEND_REQUEST_MS:-$DEF_SEND_REQ}"
  SOCK=$(mktemp -u)
  ( cd "$REPO_ROOT/frontend" && \
    node --experimental-strip-types src/lib/__tests__/_ir_capture.mts "$SOCK" \
    2>"$OUT_DIR/lmpdu-trace.txt" ) &
  BRIDGE=$!
  sleep 1
  # NOTE: do NOT pass --log-file here: verbose logging slows the sim enough to
  # shift boot/nav timing past the discovery window.
  "$HARNESS" "$ROM" --device "$DEVICE" "${NAV[@]}" \
    --serial-attach "$UART" "$ATTACH_SEC" --serial-bridge-socket "$SOCK" \
    --screenshot "$OUT_DIR/inbox.pgm" \
    --screenshot-every 5 "$OUT_DIR/shot" \
    $(boot_flags "$POLL_UNTIL") >/dev/null 2>&1 &
  HPID=$!
  wait "$BRIDGE" || true
  sleep 15  # let the device finish writing/renaming + render "Transfer complete"
  kill "$HPID" 2>/dev/null || true
  rm -f "$SOCK"
  echo "=== LM-PDU trace (real device bytes) ==="
  cat "$OUT_DIR/lmpdu-trace.txt"
  echo ""
  echo "Artefacts in $OUT_DIR (device.log, inbox.pgm, lmpdu-trace.txt)"
else
  echo "usage: DEVICE=5mx|osaris|series5|series7|netbook $0 [capture|live|trace]" >&2
  exit 2
fi
