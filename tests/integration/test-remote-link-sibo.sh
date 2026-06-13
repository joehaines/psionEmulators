#!/usr/bin/env bash
# SIBO Remote Link validation: boot a SIBO2 device, enable the link via
# the System screen's Communications dialog (Psion+L → "Link cable" /
# "Serial cable" → Enter), attach the host bridge to the Condor
# (uartIndex 0), and drive the SIBO PLP handshake with raw frames:
#
#   host  Req_Pdu   16 10 02 21 10 03 34 43
#   dev   Req_Pdu   (frame type=20)            <- asserted
#   host  Ack       16 10 02 00 10 03 00 00
#   dev   Data_Pdu  31 00 00 06 03 <id>        <- NCP Info v3, asserted
#
# All byte vectors were pinned against the live ROMs — see
# docs/sibo-remote-link.md. The full RFSV16 file-transfer path is
# exercised by the node-side repro (_plp_repro.mts series3c|siena) and
# the protocol unit tests (npm run test:plp).
#
# Usage:
#   scripts/test-remote-link-sibo.sh            # all devices
#   scripts/test-remote-link-sibo.sh series3c   # one device
#
# series3mx links through the ASIC9MX-integrated 16550 (word-spaced at
# I/O 0x50-0x5E, interrupt = ASIC9MX status bit 9 / vector 0x77), not
# the Condor window — see docs/sibo-remote-link.md.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROMS="$REPO_ROOT/roms"
LOG_DIR="$REPO_ROOT/tests/logs"
mkdir -p "$LOG_DIR"

if [ ! -x "$HARNESS" ]; then
    echo "Harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2 || exit 1
fi

REQ="16 10 02 21 10 03 34 43"
ACK="16 10 02 00 10 03 00 00"

run_one() {
    local id="$1"
    local rom="$2"
    shift 2
    local rom_path="$ROMS/$rom"
    if [ ! -f "$rom_path" ]; then
        echo "SKIP $id: ROM $rom not present" >&2
        return 0
    fi
    local log="$LOG_DIR/$id-remote-link.log"
    "$HARNESS" "$rom_path" --device "$id" --skip-card --quiet-logs \
        --min-variance 0 --min-unique-pcs 0 \
        "$@" >"$log" 2>&1
    if ! grep -q 'serial-frame type=20' "$log"; then
        echo "FAIL $id — device never sent its Req_Pdu (frame type=20); tail:" >&2
        tail -6 "$log" >&2
        return 1
    fi
    if ! grep -q 'serial-frame type=31.*00 00 06 03' "$log"; then
        echo "FAIL $id — no NCP Information frame (00 00 06 03) after Ack; tail:" >&2
        tail -6 "$log" >&2
        return 1
    fi
    echo "PASS $id — SIBO Req_Pdu + NCP Info v3 observed" >&2
    return 0
}

fail=0
want="${1:-all}"

if [ "$want" = "all" ] || [ "$want" = "series3c" ]; then
    run_one series3c series3c_v5.20f_eng.bin \
        --boot-seconds 50 \
        --press-key 8 4 16 --press-key 18 4 16 \
        --press-key 30 20 64 --press-key 30.3 76 16 \
        --press-key 33 15 32 --press-key 35 3 16 \
        --serial-attach 0 36.5 \
        --serial-tx 0 40 "$REQ" \
        --serial-tx 0 43 "$ACK" \
        --serial-poll-until 49 0 || fail=1
fi

if [ "$want" = "all" ] || [ "$want" = "series3mx" ]; then
    run_one series3mx series3mx_v6.16f_eng.bin \
        --boot-seconds 50 \
        --press-key 8 4 16 --press-key 18 4 16 \
        --press-key 30 20 64 --press-key 30.3 76 16 \
        --press-key 33 15 32 --press-key 35 3 16 \
        --serial-attach 0 26 \
        --serial-tx 0 40 "$REQ" \
        --serial-tx 0 43 "$ACK" \
        --serial-poll-until 49 0 || fail=1
fi

if [ "$want" = "all" ] || [ "$want" = "workaboutmx" ]; then
    # Workabout boot menu: Menu -> System screen -> Enter, then the
    # standard Psion+L Remote link dialog (Right toggles Off->On).
    run_one workaboutmx workaboutMX_v7.20f_eng.bin \
        --boot-seconds 54 \
        --press-key 26 148 32 --press-key 28 17 32 --press-key 30 3 16 \
        --serial-attach 0 31 \
        --press-key 33 20 64 --press-key 33.3 76 16 \
        --press-key 36 15 32 --press-key 38 3 16 \
        --serial-tx 0 44 "$REQ" \
        --serial-tx 0 47 "$ACK" \
        --serial-poll-until 53 0 || fail=1
fi

if [ "$want" = "all" ] || [ "$want" = "siena" ]; then
    run_one siena siena_v4.20f_eng.bin \
        --boot-seconds 52 \
        --press-key 8 4 16 --press-key 18 4 16 --press-key 28 4 16 \
        --press-key 32 20 64 --press-key 32.3 76 16 \
        --press-key 35 15 32 --press-key 37 3 16 \
        --serial-attach 0 38.5 \
        --serial-tx 0 42 "$REQ" \
        --serial-tx 0 45 "$ACK" \
        --serial-poll-until 51 0 || fail=1
fi

exit $fail
