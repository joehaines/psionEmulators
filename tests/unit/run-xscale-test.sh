#!/usr/bin/env bash
# Differential test for the XScale core (core/xscale.cpp): build
# tests/unit/xscale/guest.c for the host and for ARMv5TE (ARM state, then
# Thumb), run the ARM builds on the core, and require identical output.
# Needs a clang that targets ARM (most do) and ld.lld; skips otherwise.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
SRC="$HERE/xscale"
OUT="${TMPDIR:-/tmp}/xscale_test.$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
CLANG="${CLANG:-clang}"
LLD="$(command -v ld.lld || command -v ld.lld-18 || true)"
OBJCOPY="$(command -v llvm-objcopy || command -v llvm-objcopy-18 || true)"
if ! "$CLANG" --target=armv5te-none-eabi -c -x c /dev/null -o "$OUT/probe.o" 2>/dev/null || [ -z "$LLD" ] || [ -z "$OBJCOPY" ]; then
    echo "SKIP: no ARM-capable clang / ld.lld / llvm-objcopy"
    exit 0
fi

"${CXX:-g++}" -O2 -std=c++17 -o "$OUT/xscale_test" "$SRC/xscale_test.cpp" "$REPO/core/xscale.cpp"
"${CC:-cc}" -O2 -std=c99 -o "$OUT/host" "$SRC/guest.c"
"$OUT/host" > "$OUT/expected.txt"

status=0
for mode in arm thumb; do
    flags="--target=armv5te-none-eabi -mfloat-abi=soft -ffreestanding -fno-builtin -nostdlib -O2 -DXS_GUEST"
    [ "$mode" = thumb ] && flags="$flags -mthumb"
    "$CLANG" $flags -c "$SRC/guest.c" -o "$OUT/guest.o"
    "$CLANG" --target=armv5te-none-eabi -mfloat-abi=soft -ffreestanding -nostdlib -O2 -c "$SRC/rt.c" -o "$OUT/rt.o"
    "$CLANG" --target=armv5te-none-eabi -c "$SRC/start.S" -o "$OUT/start.o"
    "$LLD" -T "$SRC/link.ld" "$OUT/start.o" "$OUT/guest.o" "$OUT/rt.o" -o "$OUT/guest.elf"
    "$OBJCOPY" -O binary "$OUT/guest.elf" "$OUT/guest.bin"
    # Each build twice: through the decoded-instruction cache, and through
    # the plain decoder (the path traced runs and Thumb take).
    for engine in predecode plain; do
        if "$OUT/xscale_test" "$OUT/guest.bin" $engine > "$OUT/actual.txt" && diff -u "$OUT/expected.txt" "$OUT/actual.txt"; then
            echo "PASS: $mode ($engine)"
        else
            echo "FAIL: $mode ($engine)"
            status=1
        fi
    done
done
exit $status
