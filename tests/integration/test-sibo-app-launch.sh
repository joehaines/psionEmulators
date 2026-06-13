#!/usr/bin/env bash
# SIBO app-launch validation: boot the device, dismiss the cold-boot
# "Media is corrupt" dialog with Esc, run for long enough that any
# follow-up dialog (e.g. an app launch tripping KErrCorrupt) would be
# on screen, and assert the final framebuffer is the System screen
# rather than a centered text-dense dialog.
#
# Usage:
#   scripts/test-sibo-app-launch.sh series3a   # one device
#   scripts/test-sibo-app-launch.sh --all      # 3a, 3c, 3mx, siena
#
# A device passes when the final framebuffer's non-paper pixel count
# stays under the per-device threshold. The "Media is corrupt" dialog
# pushes that count to ~27 800 on the 480x160 panel (Siena: ~12 000 on
# 240x160), the System screen sits at ~10 200 (Siena ~7 000), so a
# midpoint threshold cleanly separates the two states.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROMS="$REPO_ROOT/roms"
LOG_DIR="$REPO_ROOT/tests/logs"
RESULTS_DIR="$REPO_ROOT/tests/results"

mkdir -p "$LOG_DIR" "$RESULTS_DIR"

if [ ! -x "$HARNESS" ]; then
    echo "Harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2
fi

# id | rom | boot-seconds | max-nonpaper | esc-press-1 | esc-press-2 | esc-press-3
# Kernel cycles between dialog and LCD-off during cold boot, so we send
# multiple Esc presses spaced out to catch the dialog whenever it's up.
DEVICES=(
    "series3a:series3a_v3.40f_eng.bin:35:20000"
    "pocketbk2:pb2_v1.30f_acn.bin:35:20000"
    "series3c:series3c_v5.20f_eng.bin:50:15000"
    "series3mx:series3mx_v6.16f_eng.bin:35:15000"
    "siena:siena_v4.20f_eng.bin:60:9000"
)

run_one() {
    local id="$1"
    local rom="$2"
    local boot_s="$3"
    local max_np="$4"
    local rom_path="$ROMS/$rom"
    if [ ! -f "$rom_path" ]; then
        echo "SKIP $id: ROM $rom not present" >&2
        return 77
    fi
    local log="$LOG_DIR/$id-applaunch.log"
    local pgm="$RESULTS_DIR/$id-applaunch.pgm"
    echo "=== $id app-launch (ROM: $rom, boot: ${boot_s}s, max-nonpaper: ${max_np}) ===" >&2
    local rc=0
    "$HARNESS" "$rom_path" \
        --device "$id" \
        --boot-seconds "$boot_s" \
        --skip-card \
        --quiet-logs \
        --press-key 8  4 16 \
        --press-key 18 4 16 \
        --press-key 28 4 16 \
        --max-nonpaper "$max_np" \
        --min-variance 0 \
        --min-unique-pcs 0 \
        --assert-boot \
        --screenshot "$pgm" \
        >"$log" 2>&1 || rc=$?
    if [ "$rc" -eq 0 ]; then
        echo "PASS $id app-launch" >&2
        return 0
    fi
    echo "FAIL $id app-launch (exit=$rc); tail:" >&2
    tail -8 "$log" >&2
    return 1
}

target="${1:-}"
if [ -z "$target" ]; then
    echo "usage: $0 <device-id>|--all" >&2
    exit 2
fi

fails=0
for entry in "${DEVICES[@]}"; do
    IFS=':' read -r id rom boot_s max_np <<<"$entry"
    if [ "$target" != "--all" ] && [ "$target" != "$id" ]; then
        continue
    fi
    if ! run_one "$id" "$rom" "$boot_s" "$max_np"; then
        rc=$?
        if [ "$rc" -ne 77 ]; then
            fails=$((fails + 1))
        fi
    fi
done

if [ "$fails" -gt 0 ]; then
    echo "=== $fails device(s) failed app-launch validation ===" >&2
    exit 1
fi
echo "=== app-launch validation passed ===" >&2
