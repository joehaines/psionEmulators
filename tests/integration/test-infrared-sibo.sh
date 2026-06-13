#!/usr/bin/env bash
# SIBO infrared ("Psion IRLink") beam validation against the live ROMs.
#
# Drives the REAL production clients through the harness socket bridge
# (frontend/src/lib/__tests__/_plp_repro.mts):
#
#   series3c-irrecv  host → device : arm the 3c's Infrared receive
#                    screen (Psion+Tab, ↓) and beam a file in with
#                    IrSiboSendClient. Expects SEND-BEAM SUCCESS (the
#                    device saves the file and lists it on the System
#                    screen — verified via the run's final screenshot).
#   series3c-beam    device → host : close all apps (Shift+Psion+A, Y)
#                    and Infrared-send the Data file (Psion+Tab, ↑);
#                    IrReceiveClient (SIBO branch) must hand the file
#                    back. Expects BEAM SUCCESS.
#   siena-irrecv     host → Siena via its dedicated IR Receive key.
#                    Expects SEND-BEAM SUCCESS (file saved on-device,
#                    same verification as the 3c).
#   siena-beam       Siena → host via its dedicated IR Send key.
#   series3mx-irrecv / series3mx-beam — same two directions on the 3mx,
#                    whose infrared rides the ASIC9MX's second
#                    integrated UART (host bridge uartIndex 1).
#
# The in-process round-trip (sender ↔ receiver, no ROM) runs in
# npm run test:irda (irda.sibo.e2e.test.mts).
#
# Usage:
#   scripts/test-infrared-sibo.sh          # all four flows

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if [ ! -x harness/run ]; then
    bash harness/build.sh >&2 || exit 1
fi

fail=0
run_target() {
    local target="$1" expect="$2"
    local log="tests/logs/ir-$target.log"
    mkdir -p tests/logs
    rm -f "/tmp/psion-plp-$target.sock"
    if timeout 480 node --experimental-strip-types \
        frontend/src/lib/__tests__/_plp_repro.mts "$target" >"$log" 2>&1 \
        && grep -q "$expect" "$log"; then
        echo "PASS $target" >&2
    else
        echo "FAIL $target (wanted '$expect'); tail:" >&2
        tail -5 "$log" >&2
        fail=1
    fi
}

run_target series3c-irrecv 'SEND-BEAM SUCCESS'
run_target series3c-beam   'BEAM SUCCESS'
run_target siena-beam      'BEAM SUCCESS'
run_target siena-irrecv    'SEND-BEAM SUCCESS'
run_target series3mx-irrecv 'SEND-BEAM SUCCESS'
run_target series3mx-beam  'BEAM SUCCESS'

exit $fail
