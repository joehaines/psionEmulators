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
# Twice: as ROMDUMP.EXE is built, and with ROMDUMP.APP's smaller log,
# report and read-back buffers (-DAPP_BUILD), so every case below is
# also run against the sizes the application build has room for.
status=0
for extra in "" "-DAPP_BUILD"; do
    echo "=== romdump.c ${extra:-(as ROMDUMP.EXE)} ==="
    "$CC" $extra -O1 -std=c99 -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
          -o "$OUT" "$HERE/er1_romdump_test.c" "$REPO/tools/romdump-er1/romdump.c"
    "$OUT" || status=1
    rm -f "$OUT"
done
exit $status
