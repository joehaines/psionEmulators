#!/usr/bin/env bash
# End-to-end test of the Conan ROM dumper (tools/romdump) on the real ROM.
#
# The Conan is an EPOC Release 5 *Unicode* machine, which is why the ER5
# ROM extractors of the day do not run on it and why this one exists. The
# whole chain runs for real:
#
#   tools/romdump/ROMDUMP.EXE    hand-built ER5u ARM executable
#   tools/e32/romfs.mts          swapped into the ROM over D_EXC.exe, the
#                                engineering build's own desktop utility
#   harness/run                  boots the machine and works its UI
#
# Two runs, because they prove different things.
#
# RESUME (tests/golden/conan-romdump-resume.pgm)
#   Open the dumper, ask it for one part, quit. Open it again — a second
#   process — and it offers part 2, not part 1: it read the progress file
#   the first run left behind. Ask for that part too. This is the flow a
#   machine too small for the whole ROM needs: dump what fits, copy it
#   off, run it again for the rest.
#
# FULL (tests/golden/conan-romdump.pgm)
#   Open it, ask for all six 2 MB parts of the 12 MB ROM, let it write
#   and read back every one, then open EShell and ask the machine
#   itself: `dir` for the files, `type romdump.txt` for the report.
#   Checked from outside too — real ROM bytes from every 64 KB of the
#   first three parts have to be in a snapshot of the machine's RAM
#   (tests/integration/test-conan-romdump.mts).
#
# The RTC is pinned so the listing's timestamps — and therefore the
# screenshots — are the same on every run.
#
#   bash tests/integration/test-conan-romdump.sh [--update-golden]

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="$REPO_ROOT/roms/conan_s2_2201.engbuild.IMG"
EXE="$REPO_ROOT/tools/romdump/ROMDUMP.EXE"
GOLDEN_DIR="$REPO_ROOT/tests/golden"
RESULTS="$REPO_ROOT/tests/results"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

update_golden=0
[ "${1:-}" = "--update-golden" ] && update_golden=1

if [ ! -x "$HARNESS" ]; then
    echo "harness not built — run bash harness/build.sh" >&2
    exit 2
fi
if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi
for f in "$ROM" "$EXE"; do
    if [ ! -f "$f" ]; then
        echo "SKIP: $f not present" >&2
        exit 0
    fi
done
mkdir -p "$RESULTS"

# The dumper goes in over Z:\System\Samples\D_EXC.exe. That file is a
# RAM-format E32Image sitting in the ROM as a plain file — not an XIP ROM
# image — so the loader treats the replacement exactly as it would treat
# one installed on C:, and the engineering build puts it on the desktop.
node --experimental-strip-types "$REPO_ROOT/tools/e32/romfs.mts" \
    replace "$ROM" 'Z:\System\Samples\D_EXC.exe' "$EXE" "$WORK/conan-romdump.IMG"

# Desktop coordinates are digitiser pixels (527 x 208, with the 480 x 160
# LCD at offset 47,0): 83,112 is the D_EXC.exe icon and 217,13 EShell's.
# A tap on the selected item opens it; on anything else it only selects,
# which is why the second launch is a tap and then Enter.
check_golden() {   # $1 = name
    local shot="$RESULTS/$1.pgm" golden="$GOLDEN_DIR/$1.pgm"
    if [ "$update_golden" = 1 ]; then
        cp "$shot" "$golden"; echo "updated $golden"
    elif [ -f "$golden" ]; then
        if cmp -s "$shot" "$golden"; then echo "screen: $1 matches the golden"
        else echo "FAIL: $shot differs from $golden" >&2; exit 1; fi
    else
        echo "no golden for $1 yet — run with --update-golden" >&2
    fi
}

# ── Run 1: stop after one part, then pick up where it stopped ────────
#   50 s tap        open the dumper (its icon is the selected one)
#   56 s '1'        write one part
#   90 s 'Q', Esc   finish and close. Esc — scan code 4, where the key
#                   code the program sees is 27 — because a held key's
#                   repeats must not be able to close the closing screen
#  100 s tap+Enter  open it again — a new process
#  112 s '1'        write the part it now offers, which must be part 2
PSION_RTC_SEED=0x3AFD2A00 "$HARNESS" "$WORK/conan-romdump.IMG" --device conan --quiet-logs \
    --boot-seconds 40 --post-attach-seconds 115 \
    --press-key 42 3 \
    --tap-seq 50 83 112 --press-key 56 49 \
    --press-key 90 81 --press-key 94 4 \
    --tap-seq 100 83 112 --press-key 104 3 --press-key 112 49 \
    --screenshot "$RESULTS/conan-romdump-resume.pgm" \
    > "$RESULTS/conan-romdump-resume.log" 2>&1 || {
        echo "harness resume run failed — see $RESULTS/conan-romdump-resume.log" >&2
        exit 1
    }
check_golden conan-romdump-resume

# ── Run 2: the whole ROM, then ask the machine to show its work ──────
#   50 s tap        open the dumper
#   56 s '6'        write all six parts (12 MB written and read back);
#                   the prompt offers 1-6, because six are left
#  240 s Esc        close the screen it ends on
#  250 s tap+Enter  open EShell
#  262 s "dir"      the files, with their sizes
#  272 s "type romdump.txt"   the report the dumper wrote
PSION_RTC_SEED=0x3AFD2A00 "$HARNESS" "$WORK/conan-romdump.IMG" --device conan --quiet-logs \
    --boot-seconds 40 --post-attach-seconds 260 \
    --press-key 42 3 \
    --tap-seq 50 83 112 --press-key 56 54 \
    --press-key 240 4 \
    --tap-seq 250 217 13 --press-key 254 3 \
    --press-key 262 68 --press-key 263 73 --press-key 264 82 --press-key 266 3 \
    --press-key 272 84 --press-key 272.6 89 --press-key 273.2 80 --press-key 273.8 69 \
    --press-key 274.4 5 --press-key 275 82 --press-key 275.6 79 --press-key 276.2 77 \
    --press-key 276.8 68 --press-key 277.4 85 --press-key 278 77 --press-key 278.6 80 \
    --press-key 279.2 122 --press-key 279.8 84 --press-key 280.4 88 --press-key 281 84 \
    --press-key 281.6 3 \
    --screenshot "$RESULTS/conan-romdump.pgm" \
    --save-ram-snapshot "$WORK/conan-romdump.ram" \
    > "$RESULTS/conan-romdump.log" 2>&1 || {
        echo "harness full run failed — see $RESULTS/conan-romdump.log" >&2
        exit 1
    }
check_golden conan-romdump

node --experimental-strip-types "$REPO_ROOT/tests/integration/test-conan-romdump.mts" \
    --ram "$WORK/conan-romdump.ram" --rom "$ROM"
