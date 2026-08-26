#!/usr/bin/env bash
# Differentially verify an execution engine against the reference interpreter.
#
# Runs the same workload twice — once with the interpreter, once with the
# engine under test — and compares (a) the architectural state-hash trace from
# core/state_trace.h and (b) the final screenshot, bit for bit. The RTC is
# pinned so the two runs are comparable at all (see PSION_RTC_SEED).
#
# An IDENTICAL trace proves the two engines agreed on every register, on CPSR
# and on the cycle count at every sample point. A DIVERGED trace prints the
# first differing line, which brackets the divergence to one interval; re-run
# with PSION_STATE_TRACE_FROM/TO around it for per-instruction records.
#
# Usage:
#   ENGINE="PSION_DECODE_PAGELOOP=1" scripts/ab-engine.sh \
#       s7 "roms/series7_v1.05(254)_b756_eng.bin" series7 8 --skip-card
#
#   ENGINE=...   the engine config under test (default: none, i.e. a self-check)
#   REF=...      the reference config (default: none). The burst engine is now
#                ON by default, so a real interpreter reference needs
#                REF="PSION_DECODE_PAGELOOP=0".
#   INTERVAL=N   state-hash sample interval (default 65536)
#   OUT=<dir>    where to put traces/screenshots (default /tmp/psion-ab)
set -u
D="${OUT:-/tmp/psion-ab}"
mkdir -p "$D"
label="$1"; rom="$2"; dev="$3"; secs="$4"; shift 4
EXTRA=( "$@" )
ENGINE="${ENGINE:-}"
INTERVAL="${INTERVAL:-65536}"

run() {
    local tag="$1"; shift
    local engine="$*"
    # shellcheck disable=SC2086
    env $engine PSION_RTC_SEED=0 \
        PSION_STATE_TRACE="$D/$label.$tag.trace" \
        PSION_STATE_TRACE_INTERVAL="$INTERVAL" \
        "$(dirname "$0")/../harness/run" "$rom" --device "$dev" --boot-seconds "$secs" \
        --quiet-logs --screenshot "$D/$label.$tag.pgm" "${EXTRA[@]}" \
        > "$D/$label.$tag.log" 2>&1
}
run ref "${REF:-}"
run new "$ENGINE"

echo "=== $label  (engine: ${ENGINE:-<none>}) ==="
if cmp -s "$D/$label.ref.trace" "$D/$label.new.trace"; then
    echo "  state-hash: IDENTICAL over $(wc -l < "$D/$label.ref.trace") sample points"
else
    echo "  state-hash: DIVERGED"
    diff "$D/$label.ref.trace" "$D/$label.new.trace" | head -4
fi
if cmp -s "$D/$label.ref.pgm" "$D/$label.new.pgm"; then
    echo "  screenshot: bit-identical"
else
    echo "  screenshot: DIFFERS"
fi
grep -h SA1100_THROUGHPUT "$D/$label.ref.log" "$D/$label.new.log" 2>/dev/null \
  | sed -e 's/^/  /' -e 's/executed_cycles=[0-9]* sim_cycles=[0-9]* //'
