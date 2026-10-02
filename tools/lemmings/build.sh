#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Build LEMMINGS.EXE for the Psion Series 5 (EPOC Release 1; the imports
# are also the 5mx's). Same method as tools/romdump-er1/build.sh: clang for
# armv4 (an ARM710a is ARMv3), linked twice at two bases so e32link can find
# the relocations, then checked for anything an ARM710a cannot execute.
#
# Usage: bash tools/lemmings/build.sh [--no-install] [-DEVLOG=1]
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/build"
mkdir -p "$OUT"
INSTALL=1; EXTRA=""
for a in "$@"; do case "$a" in --no-install) INSTALL=0 ;; *) EXTRA="$EXTRA $a" ;; esac; done

python3 "$HERE/epoc/gen_thunks.py" "$HERE/epoc/lemmings.spec" "$OUT/thunks.inc" "$OUT/iat.inc"
IMPORTS=()
while IFS= read -r l; do
    [ -n "$l" ] || continue
    dll="$(python3 "$HERE/epoc/padname.py" "${l%%:*}")"
    ords="$(echo "${l#*:}" | sed 's/=[A-Za-z0-9_]*//g')"
    IMPORTS+=(--import "$dll:$ords")
done < "$HERE/epoc/lemmings.spec"

CFLAGS="--target=armv4-none-eabi -mfloat-abi=soft -march=armv4 -mno-thumb
        -ffreestanding -fno-builtin -fno-stack-protector -fomit-frame-pointer
        -fno-unwind-tables -fno-asynchronous-unwind-tables -Os -Wall -Wextra
        -Wno-unused-function -I$OUT $EXTRA"
OBJS=()
for src in src/game.c src/gfx.c src/sprites.c src/levels.c src/font.c src/rt.c src/palette.c src/present.c epoc/platform_epoc.c epoc/main.c; do
    o="$OUT/$(basename "$src" .c).o"
    clang $CFLAGS -c "$HERE/$src" -o "$o"
    OBJS+=("$o")
done
clang $CFLAGS -c "$HERE/epoc/start.S" -o "$OUT/start.o"
for pass in 1 2; do
    base=$([ "$pass" = 1 ] && echo 0x400000 || echo 0x900000)
    sed "s/BASE;/$base;/" "$HERE/epoc/lemmings.ld" > "$OUT/pass$pass.ld"
    ld.lld -T "$OUT/pass$pass.ld" --no-dynamic-linker --build-id=none \
        -o "$OUT/pass$pass.elf" "$OUT/start.o" "${OBJS[@]}"
    llvm-objcopy -O binary "$OUT/pass$pass.elf" "$OUT/pass$pass.bin"
done
llvm-nm --numeric-sort "$OUT/pass1.elf" > "$OUT/pass1.nm"
node --experimental-strip-types "$REPO/tools/e32/e32link.mts" \
    --bin "$OUT/pass1.bin" --bin2 "$OUT/pass2.bin" --syms "$OUT/pass1.nm" \
    --base 0x400000 --base2 0x900000 "${IMPORTS[@]}" \
    --uid2 0 --uid3 0 --stack 0x6000 --tools-version 0x560001 \
    --out "$OUT/LEMMINGS.EXE"
# The read-only data (level shapes, sprites) is numbers, not code; the
# checker is told where it starts.
RODATA=$(( 0x$(llvm-nm "$OUT/pass1.elf" | awk '$3=="__rodata_start"{print $1}') - 0x400000 ))
node --experimental-strip-types "$REPO/tools/e32/armv3check.mts" --data-from "$RODATA" --e32 "$OUT/LEMMINGS.EXE"
if [ "$INSTALL" = 1 ]; then
    cp "$OUT/LEMMINGS.EXE" "$HERE/LEMMINGS.EXE"
    echo "installed $HERE/LEMMINGS.EXE"
    # the app library carries a copy, with the icon file beside it
    if [ -d "$REPO/applib/epocgames/Lemmings" ]; then
        cp "$OUT/LEMMINGS.EXE" "$REPO/applib/epocgames/Lemmings/LEMMINGS.EXE"
        echo "copied to applib/epocgames/Lemmings"
    fi
fi
