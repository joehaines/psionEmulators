#!/usr/bin/env bash
# End-to-end test for writing to a SIBO SSD pack.
#
# A pack used to mount read-only on every SIBO machine ("Disk [A],
# Protected" in the Directory browser) because Port A on a Flash pack was
# modelled as plain memory rather than as the Intel 28F0xx-class command
# bus it is, and because the ASIC5 address counter stepped on accesses
# the SIBO control byte marks as single. See docs/sibo-ssd-packs.md.
#
# The test drives the Series 3a's own System screen and checks what the
# guest wrote back into the pack image (--ssd-dump-a), so it exercises
# the whole chain — EPOC16's SSD driver, the SIBO frame protocol, the
# flash command state machine and the address counter:
#
#   format  a blank (erased) Flash pack, formatted from Disk -> Format
#           disk, comes back carrying a FEFS volume: 0xF1A5 magic, the
#           volume name that was typed, and a ROOT directory entry. This
#           needs program AND bulk erase to work — the formatter programs
#           every byte to 0x00, erases, verifies, then writes the header.
#   mkdir   a FEFS pack with files on it accepts Disk -> Make directory,
#           and the new entry appears in the image.
#
# Navigation is synthetic key events: Menu is 148, arrows 14-17, Enter 3,
# letters their ASCII codes. The menu opens on File, so one Right reaches
# the Disk menu; positions within it are counted from the top.
#
#   bash tests/integration/test-ssd-write.sh

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="${ROM:-$REPO_ROOT/roms/series3a_v3.40f_eng.bin}"
LOG_DIR="$REPO_ROOT/tests/logs"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$LOG_DIR"

if [ ! -x "$HARNESS" ]; then
    echo "harness not built — run bash harness/build.sh" >&2
    exit 2
fi
if [ ! -f "$ROM" ]; then
    echo "SKIP: ROM $ROM not present" >&2
    exit 0
fi

fail=0

# Byte-level assertions on a dumped pack image, via od so the test needs
# nothing beyond coreutils.
#   bytes_at FILE OFFSET COUNT -> space-separated lowercase hex
bytes_at() {
    od -A n -t x1 -j "$2" -N "$3" -v "$1" | tr -s ' \n' ' ' | sed 's/^ //; s/ $//'
}
expect_bytes() {   # FILE OFFSET EXPECTED-HEX LABEL
    local n got
    n="$(echo "$3" | wc -w)"
    got="$(bytes_at "$1" "$2" "$n")"
    if [ "$got" = "$3" ]; then
        echo "  ok   $4"
    else
        echo "  FAIL $4: at 0x$(printf %x "$2") got [$got], want [$3]" >&2
        fail=1
    fi
}

# ── 1. Format a blank Flash pack on the device ────────────────────────
#
# 128K of erased flash (0xFF), the state a pack ships in. The walk is:
# Menu, Right to Disk, Down x3 to "Format disk", Enter; in the dialog
# Right x2 moves Disk from Internal to A, Down reaches Name, type TEST,
# Enter, then Y to confirm. Formatting 128K takes ~15 s of sim time.
BLANK="$WORK/blank-flash.ssd"
head -c 131072 /dev/zero | tr '\000' '\377' > "$BLANK"
FMT_OUT="$WORK/formatted.ssd"

echo "=== SSD write: format a blank Flash pack (Series 3a) ===" >&2
"$HARNESS" "$ROM" --device series3a --boot-seconds 55 --skip-card --quiet-logs \
    --ssd-a "$BLANK" --ssd-type-a flash --ssd-dump-a "$FMT_OUT" \
    --press-key 20 148 8 --press-key 21.5 15 8 \
    --press-key 22.5 17 8 --press-key 23 17 8 --press-key 23.5 17 8 \
    --press-key 24.5 3 8 \
    --press-key 26 15 8 --press-key 27.5 15 8 \
    --press-key 28.5 17 8 \
    --press-key 29 84 8 --press-key 29.5 69 8 --press-key 30 83 8 --press-key 30.5 84 8 \
    --press-key 31.5 3 8 --press-key 33 89 8 \
    >"$LOG_DIR/series3a-ssd-format.log" 2>&1

if [ ! -f "$FMT_OUT" ]; then
    echo "  FAIL format: no pack image dumped" >&2
    fail=1
else
    expect_bytes "$FMT_OUT" 0  "a5 f1"                   "FEFS magic written by the format"
    expect_bytes "$FMT_OUT" 14 "54 45 53 54"             "volume name TEST"
    # Root directory entry: the pointer at 0x0B is programmed (not left
    # erased) and the entry it points at is named ROOT.
    root=$((0x$(bytes_at "$FMT_OUT" 11 1)))
    if [ "$root" -gt 0 ] && [ "$root" -lt 255 ]; then
        echo "  ok   root directory pointer programmed (0x$(printf %x "$root"))"
        expect_bytes "$FMT_OUT" $((root + 3)) "52 4f 4f 54" "ROOT entry at the root pointer"
    else
        echo "  FAIL root directory pointer not programmed (0x$(printf %x "$root"))" >&2
        fail=1
    fi
fi

# ── 2. Create a directory on a FEFS pack ──────────────────────────────
#
# tests/fixtures/fefs-128k.ssd already carries HELLO.TXT and \WRD\. The
# walk is: Menu, Right to Disk, Down to "Make directory", Enter, type
# TEST, Down to Disk, Right x3 to A, Enter.
FIXTURE="$REPO_ROOT/tests/fixtures/fefs-128k.ssd"
MKD_OUT="$WORK/mkdir.ssd"

echo "=== SSD write: make a directory on a FEFS Flash pack ===" >&2
"$HARNESS" "$ROM" --device series3a --boot-seconds 46 --skip-card --quiet-logs \
    --ssd-a "$FIXTURE" --ssd-type-a flash --ssd-dump-a "$MKD_OUT" \
    --press-key 20 148 8 --press-key 21.5 15 8 --press-key 22.5 17 8 \
    --press-key 23.5 3 8 \
    --press-key 25 84 8 --press-key 25.5 69 8 --press-key 26 83 8 --press-key 26.5 84 8 \
    --press-key 27.5 17 8 \
    --press-key 28.5 15 8 --press-key 29.5 15 8 --press-key 30.5 15 8 \
    --press-key 32 3 8 \
    >"$LOG_DIR/series3a-ssd-mkdir.log" 2>&1

if [ ! -f "$MKD_OUT" ]; then
    echo "  FAIL mkdir: no pack image dumped" >&2
    fail=1
else
    # The header must survive untouched — EPOC16 opens its slot scan with
    # a flash read-array command at offset 0, and mistaking that for data
    # is what used to zap the magic.
    expect_bytes "$MKD_OUT" 0 "a5 f1" "FEFS magic intact after the scan"
    # A directory entry named TEST that the fixture doesn't have.
    if od -A n -t c -v "$MKD_OUT" | tr -s ' \n' ' ' | grep -q 'T E S T'; then
        echo "  ok   TEST directory entry written to the pack"
    else
        echo "  FAIL mkdir: no TEST entry in the pack image" >&2
        fail=1
    fi
    if cmp -s "$FIXTURE" "$MKD_OUT"; then
        echo "  FAIL mkdir: pack image unchanged — no write reached it" >&2
        fail=1
    else
        echo "  ok   pack image changed by the guest"
    fi
fi

# ── 3. Save a file on the device, then read it back off the pack ──────
#
# What the SSD dialog's per-file download does: list a pack the *guest*
# wrote and pull one file out of it. The reader (frontend/src/lib/fefs.ts,
# via ssd-extract.mts) has to follow directory and data-record chains laid
# out by EPOC16's own filing system rather than by our writer.
#
# The walk is File -> New file on the System screen with the Data app
# selected: Menu, Enter, type SSDT, Down to the Disk field, then the
# letter A to pick drive A — the choice field's arrows auto-repeat at the
# harness's default 8-frame hold (Internal -> A -> B, landing on the
# empty slot B: "Absent!"), while typing the drive letter is exact.
# Creating the file makes Data write \DAT\SSDT.DBF onto the pack.
NEW_OUT="$WORK/newfile.ssd"

echo "=== SSD write: create a file on the device and read it back ===" >&2
"$HARNESS" "$ROM" --device series3a --boot-seconds 48 --skip-card --quiet-logs \
    --ssd-a "$FIXTURE" --ssd-type-a flash --ssd-dump-a "$NEW_OUT" \
    --press-key 20 148 8 --press-key 21.5 3 8 \
    --press-key 22.5 83 8 --press-key 23 83 8 --press-key 23.5 68 8 --press-key 24 84 8 \
    --press-key 26 17 3 --press-key 27.5 65 3 --press-key 29 3 8 \
    >"$LOG_DIR/series3a-ssd-newfile.log" 2>&1

if [ ! -f "$NEW_OUT" ]; then
    echo "  FAIL new file: no pack image dumped" >&2
    fail=1
elif ! command -v node >/dev/null 2>&1; then
    echo "  SKIP new file: node not in PATH (FEFS reader checks skipped)" >&2
else
    EXTRACT="$REPO_ROOT/tests/integration/ssd-extract.mts"
    listing="$(node --experimental-strip-types "$EXTRACT" "$NEW_OUT" || true)"
    if echo "$listing" | grep -q 'DAT\\SSDT.DBF'; then
        echo "  ok   guest-written file listed by the FEFS reader"
    else
        echo "  FAIL new file: DAT\\SSDT.DBF not listed (got: $(echo "$listing" | tr '\n' ' '))" >&2
        fail=1
    fi
    # The fixture's own files must still list alongside it — the guest
    # chained its new entries into directories we wrote.
    if echo "$listing" | grep -q 'HELLO.TXT'; then
        echo "  ok   pre-existing files still listed after the guest's writes"
    else
        echo "  FAIL new file: HELLO.TXT lost from the listing" >&2
        fail=1
    fi
    # Extract it: an EPOC16 Data file, so it opens with the OPL database
    # signature the Data app stamps on a new database.
    GOT="$WORK/ssdt.dbf"
    if node --experimental-strip-types "$EXTRACT" "$NEW_OUT" 'DAT\SSDT.DBF' "$GOT" \
            >>"$LOG_DIR/series3a-ssd-newfile.log" 2>&1; then
        if head -c 15 "$GOT" | grep -q 'OPLDatabaseFile'; then
            echo "  ok   file extracted from the pack ($(wc -c <"$GOT" | tr -d ' ') bytes, OPLDatabaseFile)"
        else
            echo "  FAIL new file: extracted bytes are not an EPOC16 database" >&2
            fail=1
        fi
    else
        echo "  FAIL new file: extraction failed" >&2
        fail=1
    fi
fi

if [ "$fail" -eq 0 ]; then
    echo "PASS ssd-write" >&2
    exit 0
fi
echo "FAIL ssd-write (logs in $LOG_DIR/series3a-ssd-*.log)" >&2
exit 1
