#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Build DOOM.EXE for EPOC Release 1 (the Psion Series 5). Same method as
# tools/lemmings/build.sh: clang for armv4 (an ARM710a is ARMv3), linked
# twice at two bases so e32link can find the relocations, then checked for
# anything an ARM710a cannot execute.
#
# Usage: bash tools/doom/build.sh [extra clang flags]
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
LEM="$REPO/tools/lemmings"
OUT="$HERE/build"
mkdir -p "$OUT"
EXTRA="$*"
# objects depend on the extra flags, so a change of flags starts again
if [ "$(cat "$OUT/flags.txt" 2>/dev/null)" != "$EXTRA" ]; then rm -f "$OUT"/*.o; echo "$EXTRA" > "$OUT/flags.txt"; fi

python3 "$HERE/epoc/gen_thunks.py" "$HERE/epoc/doom.spec" "$OUT/thunks.inc" "$OUT/iat.inc"
IMPORTS=()
while IFS= read -r l; do
    [ -n "$l" ] || continue
    dll="$(python3 "$HERE/epoc/padname.py" "${l%%:*}")"
    ords="$(echo "${l#*:}" | sed 's/=[A-Za-z0-9_]*//g')"
    IMPORTS+=(--import "$dll:$ords")
done < "$HERE/epoc/doom.spec"

CFLAGS="--target=armv4-none-eabi -mfloat-abi=soft -march=armv4 -mno-thumb
        -ffreestanding -fno-builtin -fno-stack-protector -fomit-frame-pointer
        -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-common -w
        -mno-unaligned-access -ffunction-sections -fdata-sections
        -nostdinc -isystem $HERE/libc -isystem $(clang --print-resource-dir)/include
        -I$HERE/doomgeneric -I$HERE/epoc -I$OUT
        -DNORMALUNIX -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -DCMAP256 $EXTRA"
OBJS=()
SRCS=$(ls "$HERE"/doomgeneric/*.c; echo "$HERE/epoc/libc.c" "$HERE/epoc/platform.c" "$LEM/src/font.c")
for src in $SRCS; do
    o="$OUT/$(basename "$src" .c).o"
    if [ ! -f "$o" ] || [ "$src" -nt "$o" ] || [ -n "$(find "$HERE/doomgeneric" "$HERE/epoc" "$HERE/libc" -name '*.h' -newer "$o" -print -quit)" ]; then
        # clang cannot target ARMv3, so compile to assembly, take out what an
        # ARM710a lacks (halfword loads and stores), and assemble that.
        # The renderer, the game simulation and the library are built for speed
        # (-O2) where the result has no long multiply - a division by a constant
        # becomes one, and an ARMv3 has none - and everything else, and anything
        # that does, for size (-Oz), which keeps a division as a call.
        case "$(basename "$src")" in
            r_*.c|p_*.c|m_fixed.c|v_video.c|z_zone.c|w_wad.c|libc.c|platform.c|tables.c|info.c) OPT=-O2 ;;
            *) OPT=-Oz ;;
        esac
        clang $CFLAGS $OPT -S "$src" -o "$o.s"
        if [ "$OPT" = -O2 ] && grep -qE '^\s+(umull|smull|umlal|smlal)' "$o.s"; then
            clang $CFLAGS -Oz -S "$src" -o "$o.s"
            echo "  $(basename "$src"): -Oz (a division by a constant)"
        fi
        python3 "$HERE/epoc/armv3fix.py" "$o.s" > "$o.v3.s"
        clang $CFLAGS -c -x assembler "$o.v3.s" -o "$o"
    fi
    OBJS+=("$o")
done
clang $CFLAGS -c "$HERE/epoc/start.S" -o "$OUT/start.o"
# First, how big is .data? (Link once with room for it and read it back.)
sed "s/BASE;/0x400000;/; s/DATASIZE/0/g; /^    ASSERT/d" "$HERE/epoc/doom.ld" > "$OUT/measure.ld"
ld.lld -T "$OUT/measure.ld" --no-dynamic-linker --build-id=none --gc-sections -o "$OUT/measure.elf" "$OUT/start.o" "${OBJS[@]}"
DATASIZE=$(( 0x$(llvm-nm "$OUT/measure.elf" | awk '$3=="__data_end"{print $1}') - 0x$(llvm-nm "$OUT/measure.elf" | awk '$3=="__data_start"{print $1}') ))
echo "writable data: $DATASIZE bytes"
for pass in 1 2; do
    base=$([ "$pass" = 1 ] && echo 0x400000 || echo 0x900000)
    sed "s/BASE;/$base;/; s/DATASIZE/$DATASIZE/g" "$HERE/epoc/doom.ld" > "$OUT/pass$pass.ld"
    ld.lld -T "$OUT/pass$pass.ld" --no-dynamic-linker --build-id=none --gc-sections \
        -o "$OUT/pass$pass.elf" "$OUT/start.o" "${OBJS[@]}"
    llvm-objcopy -O binary "$OUT/pass$pass.elf" "$OUT/pass$pass.bin"
done
llvm-nm --numeric-sort "$OUT/pass1.elf" > "$OUT/pass1.nm"
node --experimental-strip-types "$REPO/tools/e32/e32link.mts" \
    --bin "$OUT/pass1.bin" --bin2 "$OUT/pass2.bin" --syms "$OUT/pass1.nm" \
    --base 0x400000 --base2 0x900000 "${IMPORTS[@]}" \
    --uid2 0 --uid3 0 --stack 0x20000 --tools-version 0x560001 \
    --out "$OUT/DOOM.EXE"
RODATA=$(( 0x$(llvm-nm "$OUT/pass1.elf" | awk '$3=="__rodata_start"{print $1}') - 0x400000 ))
node --experimental-strip-types "$REPO/tools/e32/armv3check.mts" --data-from "$RODATA" --e32 "$OUT/DOOM.EXE"
cp "$OUT/DOOM.EXE" "$HERE/DOOM.EXE"
echo "built $HERE/DOOM.EXE"
