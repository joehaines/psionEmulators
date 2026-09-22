#!/usr/bin/env bash
# Compile tools/romdump-er1/romdump.c for this machine, against the
# stand-in file server and stand-in ROM in er1_romdump_test.c, and run
# what it does. See that file's header for the cases.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="${TMPDIR:-/tmp}/er1_romdump_test.$$"
CC="${CC:-gcc}"

# -Wno-int-to-pointer-cast: the program casts 32-bit ROM addresses to
# pointers, which is what it is for. The stand-in ROM is mapped where it
# looks for it, well inside the low 4 GB.
"$CC" -O1 -std=c99 -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
      -o "$OUT" "$HERE/er1_romdump_test.c" "$REPO/tools/romdump-er1/romdump.c"
"$OUT"
status=$?
rm -f "$OUT"
exit $status
