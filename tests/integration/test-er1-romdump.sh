#!/usr/bin/env bash
# End-to-end test of the EPOC Release 1 ROM dumper (tools/romdump-er1),
# run on an emulated Psion Series 5 — its own ROM, its own loader, its
# own file server.
#
# Getting a program onto the machine is the awkward part: this emulator
# has no working CompactFlash for the Series 5 yet and its R1 file
# server does not answer the RFSV requests the link client sends, so
# tools/e32/romfs1.mts puts the binary *into the ROM* instead, over
# Z:\System\Samples\Welcome to Series 5 — a file the System screen shows
# on the desktop — and renames the entry to a name ending .EXE. Opening
# it from the desktop then goes through RProcess::Create and the loader
# exactly as a copy on C: would: the image is read, relocated, its
# imports are bound by ordinal, and it runs.
#
# The run itself:
#   40 s  Enter — the desktop opens the selected file, which is the dumper
#  190 s  long enough for six 1 MB parts to be written, read back and logged
#
# It writes to C: (a RAM disk), so the result is checked in a snapshot of
# the machine's RAM: the report the dumper left, and the ROM's own bytes.
# See tests/integration/test-er1-romdump.mts.
#
# With --no-imports it runs ROMDUMP0.EXE instead: the same program built
# with no import table at all, which has nothing for the loader to bind
# and finds every call it makes in the machine's own ROM. That is the
# build for a machine the ordinary one will not open, and it has to
# dump the ROM just the same.
#
# With --app it runs ROMDUMP.APP: that code again, packaged as an
# application DLL, for a machine whose System screen will not open an
# EXE at all. It goes into the ROM the same way, under a name ending
# .APP, and the System screen opens it the way it opens any
# application — through AppRun, which loads it and calls its
# NewApplication, which is where the dump happens. (Putting it over
# Z:\System\Apps\Word\Word.app and tapping the Word icon does not
# work: that restarts the machine, and does so for a genuine Psion
# application such as applib's BOXES.APP too, so it is that slot in the
# ROM, not the binary, that will not take a RAM-format application.)
#
#   bash tests/integration/test-er1-romdump.sh [--no-imports | --app]

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/run"
ROM="$REPO_ROOT/roms/Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin"
EXE="$REPO_ROOT/tools/romdump-er1/ROMDUMP.EXE"
VARIANT=""
NAME="er1-romdump"
RENAME='ROMDUMPABCDEFGH.EXE'
if [ "${1:-}" = "--no-imports" ]; then
    EXE="$REPO_ROOT/tools/romdump-er1/ROMDUMP0.EXE"
    VARIANT="--no-imports"
    NAME="er1-romdump-noimports"
elif [ "${1:-}" = "--app" ]; then
    EXE="$REPO_ROOT/tools/romdump-er1/ROMDUMP.APP"
    VARIANT="--app"
    NAME="er1-romdump-app"
    RENAME='ROMDUMPABCDEFGH.APP'
fi
RESULTS="$REPO_ROOT/tests/results"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

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

# The replacement keeps the entry's name length, which is why the name
# is padded out to nineteen characters. The binary is bigger than the
# file whose place it takes on the desktop, so --donor gives it the room
# of one that is not wanted (the help text), inside the length the ROM
# header declares: a real machine's ROM window stops where the image
# does, and this test should not lean on the emulator being kinder.
node --experimental-strip-types "$REPO_ROOT/tools/e32/romfs1.mts" \
    replace "$ROM" 'Z:\System\Samples\Welcome to Series 5' "$EXE" \
    "$WORK/series5-romdump.bin" --rename "$RENAME" \
    --donor 'Z:\System\Data\Help'

"$HARNESS" "$WORK/series5-romdump.bin" --device series5 --quiet-logs --skip-card \
    --boot-seconds 190 --press-key 40 3 \
    --screenshot "$RESULTS/$NAME.pgm" \
    --save-ram-snapshot "$WORK/$NAME.ram" \
    > "$RESULTS/$NAME.log" 2>&1 || {
        echo "harness run failed — see $RESULTS/$NAME.log" >&2
        exit 1
    }

node --experimental-strip-types "$REPO_ROOT/tests/integration/test-er1-romdump.mts" \
    --ram "$WORK/$NAME.ram" --rom "$ROM" $VARIANT --exe "$EXE"
