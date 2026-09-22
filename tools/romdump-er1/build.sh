#!/usr/bin/env bash
# Build ROMDUMP.EXE for EPOC Release 1 — the Psion Series 5 (including
# the pre-release prototype builds), the Series 5's siblings and the
# Geofox One — with clang and ld.lld, then pack it into an E32Image with
# tools/e32.
#
# There is no EPOC SDK here and petran does not run on Linux, so the
# image is assembled by hand: the objects are linked twice, at two
# different bases, and tools/e32/e32link.mts turns the pair into a
# relocatable E32Image (see its header for the format).
#
# What makes this an R1 binary rather than an ER5u one is three things,
# all of them read out of R1 machines' own ROMs and out of real R1
# binaries (docs/series5-prototype-rom-dumping.md):
#
#   * the file server client is EFSRV[100000bd].DLL, not [100039e4];
#   * its ordinals are R1's — 14, 17, 105, 119, 133, 170;
#   * iVersion is an R1 tools version. The R1 loader does not examine it
#     (its whole check is the 'EPOC' signature plus a range check on
#     every offset in the header), but there is no reason to carry a
#     number no R1 tool ever wrote.
#
# And one thing about the CPU rather than the OS: a Series 5's ARM710a
# is **ARMv3**. It has no BX (that is ARMv4T, for interworking with a
# Thumb it does not have) and no 64-bit multiply (ARMv3M), and it does
# not fault on either — the encodings alias data-processing space, so
# the machine quietly executes something else. clang has no armv3
# target, so the build asks for armv4 (which stops it ending functions
# with "bx lr"), the source does its own division, and
# tools/e32/armv3check.mts reads the finished binary back and fails the
# build if a single instruction an ARM710a cannot execute got in.
#
# Needs: clang (with the ARM target), ld.lld, llvm-objcopy, llvm-nm,
# node 22+.  Writes build/ROMDUMP.EXE and copies it next to the source
# as the committed artifact.
#
# It builds two binaries from the same source:
#
#   ROMDUMP.EXE   imports six functions from EFSRV[100000bd].DLL, checks
#                 them against the machine's own ROM, and uses what it
#                 finds there if the two disagree. This is the one to run.
#   ROMDUMP0.EXE  imports nothing at all. There is nothing for the loader
#                 to bind and so nothing it can fail to find; every call
#                 is found in the ROM before anything else happens. This
#                 is the one to try on a machine that will not open the
#                 first — a prototype whose file server library has a
#                 different UID, say — and it can say nothing at all if
#                 the ROM does not give it what it needs.
#
# Usage: bash tools/romdump-er1/build.sh [--no-install]

set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$HERE/build"
BASE1=0x400000      # where EPOC links RAM-loaded executables
BASE2=0x900000      # second link, for finding the relocations
mkdir -p "$OUT"

# armv4, not armv4t: this machine's ARM710a has no Thumb, so asking for
# armv4t only gets the compiler to end every function with a "bx lr"
# the CPU cannot execute. armv4 ends them with "mov pc, lr" instead.
CFLAGS="--target=armv4-none-eabi -mfloat-abi=soft -march=armv4 -mno-thumb
        -ffreestanding -fno-builtin -fno-stack-protector -fomit-frame-pointer
        -fno-unwind-tables -fno-asynchronous-unwind-tables -Os -Wall -Wextra"

# The build stamp the log carries, so a log sent back from a machine
# names the binary that wrote it.
STAMP="${BUILD_ID:-$(date -u +%Y-%m-%d)}"
build_one () {          # $1 = output name, $2... = extra flags
    local out="$1"; shift
    local extra_c="" import_arg=()
    if [ "$out" = "ROMDUMP0.EXE" ]; then
        extra_c="-DNO_IMPORTS"
    else
        import_arg=(--import 'EFSRV[100000bd].DLL:14,17,105,119,133,170')
    fi
    clang $CFLAGS $extra_c -DBUILD_ID="\"$STAMP\"" -c "$HERE/romdump.c" -o "$OUT/romdump.o"
    clang $CFLAGS $extra_c -c "$HERE/start.S" -o "$OUT/start.o"
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
        --base $BASE1 --base2 $BASE2 "${import_arg[@]}" \
        --uid2 0 --uid3 0 --stack 0x2000 --tools-version 0x560001 \
        --out "$OUT/$out"
    # Nothing an ARM710a cannot execute may leave this directory.
    node --experimental-strip-types "$REPO/tools/e32/armv3check.mts" --e32 "$OUT/$out"
}

build_one ROMDUMP.EXE
build_one ROMDUMP0.EXE

if [ "${1:-}" != "--no-install" ]; then
    cp "$OUT/ROMDUMP.EXE" "$HERE/ROMDUMP.EXE"
    cp "$OUT/ROMDUMP0.EXE" "$HERE/ROMDUMP0.EXE"
    echo "installed $HERE/ROMDUMP.EXE and $HERE/ROMDUMP0.EXE"
fi
