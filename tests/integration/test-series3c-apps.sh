#!/usr/bin/env bash
# Series 3c per-app launch validation.
#
# Walks the 8 built-in apps reachable via F1..F8 from the System screen,
# pressing each F-key in turn and asserting that the resulting frame
# buffer carries a substantial non-paper pixel count. Confirms that
# every shipped app reaches its steady-state UI (rather than landing on
# a blank screen or a "Media is corrupt" dialog).
#
# Usage:
#   scripts/test-series3c-apps.sh           # all 8 apps
#   scripts/test-series3c-apps.sh F4        # one app (case-insensitive)
#
# A row in tests/series3c-apps.txt drives each test case. See that file
# for the methodology and how the per-app non-paper thresholds were
# chosen.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="$REPO_ROOT/roms/series3c_v5.20f_eng.bin"
CONF="$REPO_ROOT/tests/series3c-apps.txt"
LOG_DIR="$REPO_ROOT/tests/logs"
RESULTS_DIR="$REPO_ROOT/tests/results"

mkdir -p "$LOG_DIR" "$RESULTS_DIR"

if [ ! -x "$HARNESS" ]; then
    echo "Harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2
fi

if [ ! -f "$ROM" ]; then
    echo "SKIP: $ROM not present" >&2
    exit 77
fi

target="${1:-}"
fails=0
ran=0

while IFS='|' read -r fkey epoc label minnp; do
    fkey="$(echo "$fkey" | tr -d '[:space:]')"
    case "$fkey" in
        ''|\#*) continue;;
    esac
    epoc="$(echo "$epoc" | tr -d '[:space:]')"
    label="$(echo "$label" | sed 's/^ *//;s/ *$//')"
    minnp="$(echo "$minnp" | tr -d '[:space:]')"

    if [ -n "$target" ]; then
        # Compare case-insensitively against the F-key label
        if [ "$(echo "$target" | tr '[:upper:]' '[:lower:]')" != \
             "$(echo "$fkey"   | tr '[:upper:]' '[:lower:]')" ]; then
            continue
        fi
    fi

    ran=$((ran + 1))
    pgm="$RESULTS_DIR/series3c-app-${fkey}.pgm"
    log="$LOG_DIR/series3c-app-${fkey}.log"

    echo "=== ${fkey} (EpocKey ${epoc}, ${label}) — min-non-paper=${minnp} ===" >&2
    "$HARNESS" "$ROM" \
        --device series3c \
        --boot-seconds 50 \
        --skip-card \
        --quiet-logs \
        --press-key 8 4 16 \
        --press-key 18 "$epoc" 32 \
        --screenshot "$pgm" \
        > "$log" 2>&1 || {
            rc=$?
            echo "FAIL ${fkey}: harness exit=${rc}; tail of log:" >&2
            tail -8 "$log" >&2
            fails=$((fails + 1))
            continue
        }

    np=$(python3 -c "
import sys
data = open(sys.argv[1],'rb').read()
parts = data.split(b'\n', 3)
pixels = parts[3]
print(sum(1 for px in pixels if px != 160))
" "$pgm")

    if [ "$np" -ge "$minnp" ]; then
        echo "PASS ${fkey} ${label} (non-paper=${np} >= ${minnp})" >&2
    else
        echo "FAIL ${fkey} ${label} (non-paper=${np} < ${minnp}); see ${pgm}" >&2
        fails=$((fails + 1))
    fi
done < "$CONF"

if [ "$ran" -eq 0 ]; then
    echo "No matching app for target '${target}'." >&2
    exit 2
fi

if [ "$fails" -gt 0 ]; then
    echo "=== ${fails}/${ran} app(s) failed launch validation ===" >&2
    exit 1
fi
echo "=== ${ran}/${ran} app(s) launched successfully ===" >&2
