#!/usr/bin/env bash
# Brute-force the Series 7 EKA1 touch/event DProcess binding.
#
# Investigation rounds 15-17 (docs/series7-input-investigation.md)
# localised the input freeze to: the kernel's RequestComplete at
# 0x5000c06c is called with r0 = 0x80312dcc (a non-WSrv DProcess)
# instead of 0x8030c8d4 (WSrv's DProcess) when touch IRQs fire.  The
# correct slot in EKA1 kernel memory holding the wrong pointer isn't
# documented (EKA1 was never open-sourced).  Brute-force:
#
#   Pass 1: scan-only — find all RAM slots containing 0x80312dcc.
#   Pass 2: iterate slot 0..N-1.  For each, override the slot with
#           0x8030c8d4 and run tests/s7_func.sh.  Record exit code
#           per slot.
#   Pass 3: report which slot(s), if any, produced PASS.
#
# Each iteration runs tests/s7_func.sh in full (~3 min wall) so total
# wall time is ~3 min × N.  Use BIND_SLOT_RANGE=lo:hi to limit.
#
# Usage:
#   tests/s7_bind_bruteforce.sh                 # full sweep
#   BIND_SLOT_RANGE=0:9 tests/s7_bind_bruteforce.sh
#   BIND_TARGET=0xdeadbeef tests/s7_bind_bruteforce.sh
#
# Env overrides (passed through to harness):
#   BIND_TARGET       hex word to search for     (default 0x80312dcc)
#   BIND_REPL         hex word to substitute     (default 0x8030c8d4)
#   BIND_SCAN_AT_MS   sim ms when scan runs      (default 5500)
#   BIND_PERSIST      1 to keep re-writing slot  (default 1)
#   BIND_SLOT_RANGE   "lo:hi" inclusive limit
#   BIND_PARALLEL     N concurrent runs          (default 1)
#
# Exit codes:
#   0  one or more slots produced PASS
#   1  scan succeeded but no slot unblocked input
#   2  scan found zero matches (target value not in RAM at scan time)
#   3  harness/run missing or scan-pass failed

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM=$REPO/roms/'S7_v1.05(254)_b754_eng.bin'
S7_FUNC=$REPO/tests/stress/s7_func.sh

[ -x "$HARNESS" ] || { echo "ERROR: harness not built — run harness/build.sh first" >&2; exit 3; }
[ -x "$S7_FUNC" ] || { echo "ERROR: tests/s7_func.sh missing" >&2; exit 3; }

BIND_TARGET=${BIND_TARGET:-0x80312dcc}
BIND_REPL=${BIND_REPL:-0x8030c8d4}
BIND_SCAN_AT_MS=${BIND_SCAN_AT_MS:-5500}
BIND_PERSIST=${BIND_PERSIST:-1}
BIND_PARALLEL=${BIND_PARALLEL:-1}

OUT_DIR=$(mktemp -d /tmp/s7_bind_brute.XXXXXX)
echo "=== Brute-force binding: target=$BIND_TARGET repl=$BIND_REPL ==="
echo "    work dir: $OUT_DIR"
echo ""

# ---------- Pass 1: scan only ----------
echo "=== Pass 1: scan SDRAM for $BIND_TARGET at sim $BIND_SCAN_AT_MS ms ==="
SCAN_LOG=$OUT_DIR/scan.log
PSION_S7_BIND_TARGET=$BIND_TARGET \
PSION_S7_BIND_REPL=$BIND_REPL \
PSION_S7_BIND_SCAN_AT_MS=$BIND_SCAN_AT_MS \
PSION_S7_BIND_SLOT=-1 \
PSION_S7_BIND_MAX=4096 \
    "$HARNESS" "$ROM" --device series7 \
        --boot-seconds $(( (BIND_SCAN_AT_MS + 500) / 1000 + 1 )) \
        --skip-card > "$SCAN_LOG" 2>&1 || true

# The scanner emits "[BIND] scan #0 complete: N matches" — pull N from
# scan #0 (we only run one scan in Pass 1).
N=$(grep -oE 'scan #0 complete: [0-9]+ matches' "$SCAN_LOG" | awk '{print $4}')
N=${N:-0}
if [ "$N" -eq 0 ]; then
    echo "FAIL: zero matches for $BIND_TARGET at sim ${BIND_SCAN_AT_MS}ms"
    echo "    The target value is not stored in physical RAM at scan time."
    echo "    Possibilities: target is computed dynamically; scan timing wrong;"
    echo "    value lives in ROM-mapped pages; or original investigation note"
    echo "    captured a transient r0 value not backed by stored state."
    echo "    Try: enable PSION_S7_WATCH_VAL=$BIND_TARGET to see if/when it's"
    echo "    written, then re-aim BIND_SCAN_AT_MS at that sim time."
    echo "    Log: $SCAN_LOG"
    exit 2
fi
echo "    found $N matches"

# Parse slot list.  Multi-scan format:
#   [        0] [BIND]   #0 slot 7: phys 0xc8a027a0
# We want one entry per slot index, indexed by slot number.
mapfile -t SLOT_LINES < <(grep -E '\[BIND\]   #0 slot ' "$SCAN_LOG")

# Resolve slot range
LO=0
HI=$((N - 1))
if [ -n "${BIND_SLOT_RANGE:-}" ]; then
    LO=${BIND_SLOT_RANGE%%:*}
    HI=${BIND_SLOT_RANGE##*:}
    [ "$HI" -gt $((N - 1)) ] && HI=$((N - 1))
fi
echo "    iterating slots $LO..$HI"
echo ""

# ---------- Pass 2: iterate slots ----------
RESULTS_TSV=$OUT_DIR/results.tsv
echo -e "slot\tphys\texit\tnote" > "$RESULTS_TSV"

run_one() {
    local slot=$1
    local phys
    phys=$(echo "${SLOT_LINES[$slot]:-}" | grep -oE 'phys 0x[0-9a-fA-F]+' | awk '{print $2}')
    phys=${phys:-?}
    local slot_log=$OUT_DIR/slot-$slot.log
    PSION_S7_BIND_TARGET=$BIND_TARGET \
    PSION_S7_BIND_REPL=$BIND_REPL \
    PSION_S7_BIND_SCAN_AT_MS=$BIND_SCAN_AT_MS \
    PSION_S7_BIND_SLOT=$slot \
    PSION_S7_BIND_PERSIST=$BIND_PERSIST \
    PSION_S7_BIND_MAX=4096 \
        "$S7_FUNC" > "$slot_log" 2>&1
    local rc=$?
    local note
    if [ "$rc" -eq 0 ]; then
        note=PASS
    elif grep -q 'FAIL\[1\]' "$slot_log"; then
        note=frozen
    elif grep -q 'FAIL\[2\]' "$slot_log"; then
        note=no-boot
    else
        note="rc=$rc"
    fi
    printf '%d\t%s\t%d\t%s\n' "$slot" "$phys" "$rc" "$note" >> "$RESULTS_TSV"
    printf '  slot %3d phys=%-12s exit=%d  %s\n' "$slot" "$phys" "$rc" "$note"
}

echo "=== Pass 2: iterate slots (parallel=$BIND_PARALLEL) ==="
if [ "$BIND_PARALLEL" -gt 1 ]; then
    # Simple background pool.
    pids=()
    for ((i = LO; i <= HI; i++)); do
        run_one "$i" &
        pids+=($!)
        if [ ${#pids[@]} -ge "$BIND_PARALLEL" ]; then
            wait "${pids[0]}"
            pids=("${pids[@]:1}")
        fi
    done
    wait
else
    for ((i = LO; i <= HI; i++)); do run_one "$i"; done
fi

echo ""
echo "=== Pass 3: summary ==="
PASSES=$(awk -F'\t' 'NR>1 && $3==0 {print $1"\t"$2}' "$RESULTS_TSV")
if [ -n "$PASSES" ]; then
    echo "PASS slot(s):"
    echo "$PASSES" | awk '{ printf "  slot %s phys %s\n", $1, $2 }'
    echo ""
    echo "Next step: hardcode the chosen slot's phys address (or the offset"
    echo "into RAM bank 0/1) in core/sa1100.cpp and remove the env-gate."
    echo "Full results: $RESULTS_TSV"
    exit 0
else
    echo "NO slot unblocked input.  Possibilities:"
    echo "  1. Target value isn't the only place the wrong DProcess flows in."
    echo "  2. The override isn't durable — try BIND_PERSIST=1 (already on)."
    echo "  3. There's a downstream blocker (LCD output channel) gating WSrv."
    echo "Full results: $RESULTS_TSV"
    exit 1
fi
