#!/usr/bin/env bash
# Build ROMDUMP.EXE — an EPOC Release 5 Unicode (ARM) executable — with
# clang and ld.lld, then pack it into an E32Image with tools/e32.
#
# There is no EPOC SDK here and petran does not run on Linux, so the
# image is assembled by hand: the objects are linked twice, at two
# different bases, and tools/e32/e32link.mts turns the pair into a
# relocatable E32Image (see its header for the format).
#
# Needs: clang (with the ARM target), ld.lld, llvm-objcopy, llvm-nm,
# node 22+.  Writes build/ROMDUMP.EXE and copies it next to the source
# as the committed artifact.
#
# Usage: bash tools/romdump/build.sh [--no-install]

set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/build"
BASE1=0x400000      # where EPOC links RAM-loaded executables
BASE2=0x900000      # second link, for finding the relocations
mkdir -p "$OUT"

CFLAGS="--target=armv4t-none-eabi -mfloat-abi=soft -march=armv4t -mno-thumb
        -ffreestanding -fno-builtin -fno-stack-protector -fomit-frame-pointer
        -fno-unwind-tables -fno-asynchronous-unwind-tables -Os -Wall -Wextra"

clang $CFLAGS -c "$HERE/romdump.c" -o "$OUT/romdump.o"
clang $CFLAGS -c "$HERE/start.S"   -o "$OUT/start.o"

for pass in 1 2; do
    base=$([ "$pass" = 1 ] && echo $BASE1 || echo $BASE2)
    sed "s/BASE;/$base;/" "$HERE/romdump.ld" > "$OUT/pass$pass.ld"
    ld.lld -T "$OUT/pass$pass.ld" --no-dynamic-linker --build-id=none \
           -o "$OUT/pass$pass.elf" "$OUT/start.o" "$OUT/romdump.o"
    llvm-objcopy -O binary "$OUT/pass$pass.elf" "$OUT/pass$pass.bin"
done
llvm-nm --numeric-sort "$OUT/pass1.elf" > "$OUT/pass1.nm"

node --experimental-strip-types "$REPO/tools/e32/e32link.mts" \
    --bin "$OUT/pass1.bin" --bin2 "$OUT/pass2.bin" --syms "$OUT/pass1.nm" \
    --base $BASE1 --base2 $BASE2 \
    --import 'EFSRV[100039e4].DLL:15,18,121,136,151,194' \
    --import 'EUSER[100039e5].DLL:511,731,746,837' \
    --uid2 0 --uid3 0 --stack 0x4000 \
    --out "$OUT/ROMDUMP.EXE"

if [ "${1:-}" != "--no-install" ]; then
    cp "$OUT/ROMDUMP.EXE" "$HERE/ROMDUMP.EXE"
    echo "installed $HERE/ROMDUMP.EXE"
fi
