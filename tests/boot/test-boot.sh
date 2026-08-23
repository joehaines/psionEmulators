#!/usr/bin/env bash
# Device boot-validation driver.
#
# Usage:
#   scripts/test-boot.sh <device-id> [--update-golden]
#   scripts/test-boot.sh --all [--update-golden]
#
# For each device id it:
#   1. Looks up the ROM filename from the device registry (via tests/devices.txt)
#   2. Invokes the native harness with --assert-boot --skip-card, forcing the
#      correct profile via --device <id>.
#   3. Writes a JSON summary to tests/results/<id>.json and a PGM screenshot to
#      tests/golden/<id>.pgm (or tests/actual/<id>.pgm if the golden exists and
#      we're not updating it).
#   4. Exits non-zero on failure. Harness stdout/stderr go to tests/logs/<id>.log.
#
# The device registry only owns the C++ runtime side; tests/devices.txt lists
# which devices to validate along with their ROM files and per-device boot
# window. Keep it in sync with core/device_registry.cpp (Supported entries).

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROMS="$REPO_ROOT/roms"
GOLDEN_DIR="$REPO_ROOT/tests/golden"
ACTUAL_DIR="$REPO_ROOT/tests/actual"
RESULTS_DIR="$REPO_ROOT/tests/results"
LOG_DIR="$REPO_ROOT/tests/logs"
DEVICES_FILE="$REPO_ROOT/tests/devices.txt"

update_golden=0
target=""
for arg in "$@"; do
    case "$arg" in
        --update-golden) update_golden=1 ;;
        --all) target="--all" ;;
        --ssd) target="--ssd" ;;
        -*) echo "unknown flag: $arg" >&2; exit 2 ;;
        *) target="$arg" ;;
    esac
done
if [ -z "$target" ]; then
    echo "usage: $0 <device-id>|--all [--update-golden]" >&2
    exit 2
fi

if [ ! -x "$HARNESS" ]; then
    echo "Harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2
fi

mkdir -p "$GOLDEN_DIR" "$ACTUAL_DIR" "$RESULTS_DIR" "$LOG_DIR"

test_device() {
    local id="$1"
    local rom="$2"
    local boot_s="$3"
    local min_var="$4"
    # Optional 5th field: extra harness args (whitespace-separated). Used
    # by SSD test rows to add `--device <real-device-id> --ssd-a <path>`.
    # The id-vs-device split lets us run the same ROM with and without
    # an SSD attached (e.g. siena vs siena-ssd both use --device siena).
    local extra="${5:-}"
    # Optional 6th field: device id override for --device (so an SSD
    # row can have id=siena-ssd while still passing --device siena).
    local device_id="${6:-$id}"

    local rom_path="$ROMS/$rom"
    if [ ! -f "$rom_path" ]; then
        echo "SKIP $id: ROM $rom not present" >&2
        return 77
    fi

    local log="$LOG_DIR/$id.log"
    local json="$RESULTS_DIR/$id.json"
    local pgm_target
    if [ "$update_golden" -eq 1 ] || [ ! -f "$GOLDEN_DIR/$id.pgm" ]; then
        pgm_target="$GOLDEN_DIR/$id.pgm"
    else
        pgm_target="$ACTUAL_DIR/$id.pgm"
    fi

    # Synthesised boot cards. The 5mx Pro bootloader boots by loading
    # SYS$ROM.BIN off a FAT16 CF card (the frontend builds the card on the
    # fly — see buildOsCardImage in useEmulator.ts). The card is 16 MB so
    # it isn't checked in; build it here on first use from the patch ROM,
    # mirroring what the frontend ships.
    if [[ "$extra" == *tests/cards/5mxpro-osboot.img* ]] \
       && [ ! -f "$REPO_ROOT/tests/cards/5mxpro-osboot.img" ]; then
        local os_rom="$ROMS/5mxPRO_v1.05(319)_patch_eng.bin"
        if [ ! -f "$os_rom" ] || ! command -v node >/dev/null 2>&1; then
            echo "SKIP $id: cannot synthesise boot card (need node + $os_rom)" >&2
            return 77
        fi
        mkdir -p "$REPO_ROOT/tests/cards"
        node --experimental-strip-types - "$os_rom" \
            "$REPO_ROOT/tests/cards/5mxpro-osboot.img" <<EOF
import { createBlankImage, addFile } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'node:fs';
const [rom, out] = process.argv.slice(2);
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'SYS\$ROM.BIN', new Uint8Array(readFileSync(rom)), 0);
if (!r.ok) { console.error('addFile failed', r); process.exit(1); }
writeFileSync(out, img);
EOF
    fi

    # The netBook's ESHELL boot card: the same FAT16 container the frontend
    # synthesises for the 'eshell' osCardSpec variant, carrying
    # roms/ESHELL/OS.IMG as D:\OS.IMG. Exercises the faithful CF read (the
    # browser's default path) against an OS image far smaller than the stock
    # one, which is what the size-scaled faithful-boot gate exists for.
    if [[ "$extra" == *tests/cards/eshell-netbook.img* ]] \
       && [ ! -f "$REPO_ROOT/tests/cards/eshell-netbook.img" ]; then
        local esh_rom="$ROMS/ESHELL/OS.IMG"
        if [ ! -f "$esh_rom" ] || ! command -v node >/dev/null 2>&1; then
            echo "SKIP $id: cannot synthesise ESHELL card (need node + $esh_rom)" >&2
            return 77
        fi
        mkdir -p "$REPO_ROOT/tests/cards"
        node --experimental-strip-types - "$esh_rom" \
            "$REPO_ROOT/tests/cards/eshell-netbook.img" <<EOF
import { createBlankImage, addFile } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'node:fs';
const [rom, out] = process.argv.slice(2);
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'OS.IMG', new Uint8Array(readFileSync(rom)), 0);
if (!r.ok) { console.error('addFile failed', r); process.exit(1); }
writeFileSync(out, img);
EOF
    fi

    # The netBook's Quartz boot card: the same FAT16 container the frontend
    # synthesises for a netBook OS image, carrying roms/OS.IMG (the Quartz v6.0
    # netBook build) as D:\OS.IMG.  Exercises the faithful CF read against an
    # EPOC image whose kernel data layout differs from the stock netBook OS —
    # which is what the UCB1200-mutex guard in core/sa1100.cpp exists for.
    if [[ "$extra" == *tests/cards/quartz-netbook.img* ]] \
       && [ ! -f "$REPO_ROOT/tests/cards/quartz-netbook.img" ]; then
        local qz_rom="$ROMS/OS.IMG"
        if [ ! -f "$qz_rom" ] || ! command -v node >/dev/null 2>&1; then
            echo "SKIP $id: cannot synthesise Quartz card (need node + $qz_rom)" >&2
            return 77
        fi
        mkdir -p "$REPO_ROOT/tests/cards"
        node --experimental-strip-types - "$qz_rom" \
            "$REPO_ROOT/tests/cards/quartz-netbook.img" <<EOF
import { createBlankImage, addFile } from '$REPO_ROOT/frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'node:fs';
const [rom, out] = process.argv.slice(2);
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'OS.IMG', new Uint8Array(readFileSync(rom)), 0);
if (!r.ok) { console.error('addFile failed', r); process.exit(1); }
writeFileSync(out, img);
EOF
    fi

    echo "=== Testing $id (ROM: $rom, boot: ${boot_s}s${extra:+, extra=$extra}) ===" >&2
    local exit_code=0
    # If the extra args include --card-path, the test wants a CF card
    # attached (e.g. netbook_full booting the OS image), so we omit
    # --skip-card.  Otherwise force --skip-card to keep the default
    # boot-validation path deterministic.
    local card_flag="--skip-card"
    if [[ "$extra" == *--card-path* ]]; then
        card_flag=""
    fi
    # shellcheck disable=SC2086
    "$HARNESS" "$rom_path" \
        --device "$device_id" \
        --boot-seconds "$boot_s" \
        $card_flag \
        --quiet-logs \
        --assert-boot \
        --min-variance "$min_var" \
        --summary-json "$json" \
        --screenshot "$pgm_target" \
        $extra \
        >"$log" 2>&1 || exit_code=$?

    # Count fatal direct-printf events. "unhandled uart write" is benign
    # (ROM touching a register we don't model, boot continues) — real
    # failures are data/prefetch aborts, undefined instructions, or the
    # stack-underflow guard. --assert-boot inside the harness already
    # catches blank LCD / stuck PC via variance + PC spread, so we only
    # layer on the aborts here.
    local fatal_traps
    fatal_traps=$(grep -cE 'data abort|prefetch abort|undefined instruction|stack underflow' "$log" || true)

    if [ "$exit_code" -eq 0 ] && [ "$fatal_traps" -eq 0 ]; then
        echo "PASS $id ($(cat "$json"))" >&2
        return 0
    else
        echo "FAIL $id (exit=$exit_code, fatal_traps=$fatal_traps, see $log)" >&2
        [ -f "$json" ] && cat "$json" >&2 || true
        return 1
    fi
}

run_all() {
    local fails=0
    local devfile="$1"
    while IFS='|' read -r id rom boot_s min_var extra device_id; do
        case "$id" in ''|\#*) continue ;; esac
        id="${id//[[:space:]]/}"
        rom="${rom//[[:space:]]/}"
        boot_s="${boot_s//[[:space:]]/}"
        min_var="${min_var//[[:space:]]/}"
        # Trim leading/trailing whitespace from extra args, but keep
        # internal whitespace (it's the `--ssd-a path` separator).
        extra="${extra#"${extra%%[![:space:]]*}"}"
        extra="${extra%"${extra##*[![:space:]]}"}"
        device_id="${device_id//[[:space:]]/}"
        if ! test_device "$id" "$rom" "$boot_s" "$min_var" "$extra" "$device_id"; then
            rc=$?
            if [ "$rc" -ne 77 ]; then
                fails=$((fails + 1))
            fi
        fi
    done < "$devfile"
    if [ "$fails" -gt 0 ]; then
        echo "=== $fails device(s) failed boot validation ===" >&2
        return 1
    fi
    echo "=== all devices passed ===" >&2
}

DEVICES_SSD_FILE="$REPO_ROOT/tests/devices-ssd.txt"

if [ "$target" = "--all" ]; then
    run_all "$DEVICES_FILE"
elif [ "$target" = "--ssd" ]; then
    if [ ! -f "$DEVICES_SSD_FILE" ]; then
        echo "$DEVICES_SSD_FILE not present" >&2
        exit 2
    fi
    run_all "$DEVICES_SSD_FILE"
else
    line=$(grep -E "^[[:space:]]*$target[[:space:]]*\|" "$DEVICES_FILE" || true)
    [ -z "$line" ] && [ -f "$DEVICES_SSD_FILE" ] && \
        line=$(grep -E "^[[:space:]]*$target[[:space:]]*\|" "$DEVICES_SSD_FILE" || true)
    if [ -z "$line" ]; then
        echo "device $target not in $DEVICES_FILE or $DEVICES_SSD_FILE" >&2
        exit 2
    fi
    IFS='|' read -r id rom boot_s min_var extra device_id <<<"$line"
    extra="${extra#"${extra%%[![:space:]]*}"}"
    extra="${extra%"${extra##*[![:space:]]}"}"
    test_device "${id//[[:space:]]/}" "${rom//[[:space:]]/}" \
        "${boot_s//[[:space:]]/}" "${min_var//[[:space:]]/}" \
        "$extra" "${device_id//[[:space:]]/}"
fi
