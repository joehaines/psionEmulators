#!/usr/bin/env bash
# Native host build of the emulator core + harness. No emscripten required.

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$REPO_ROOT/core"
HARNESS="$REPO_ROOT/harness"
OBJ_DIR="$HARNESS/obj"
OUT="$HARNESS/run"

mkdir -p "$OBJ_DIR"

CXX="${CXX:-g++}"
CC="${CC:-gcc}"

# -DPSION_PROFILE_CYCLES compiles in the SA-1100 executed-cycle counter so the
# harness can report raw interpreter throughput (SA1100_THROUGHPUT line). The
# WASM build deliberately omits it, so the shipping emulator pays nothing.
# -flto: link-time optimisation lets the hot interpreter paths (cpu.tick →
# readVirtual/writeVirtual, etc.) inline across translation units. Measured
# +13% Series 7 / +3% netBook native throughput; same code, so the boot suite
# validates correctness.
CXXFLAGS=(-O3 -flto -std=c++17 -Wno-deprecated-declarations -Wno-multichar -DPSION_PROFILE_CYCLES)
CFLAGS=(-O3 -flto)

echo "=== Compiling core C++ sources ==="
# Objects every target that links arm710.o also needs: ARM710's destructor and
# initFastPathGate reach the code generator's dispatcher seam.
ARM_JIT_OBJS=("$OBJ_DIR/wasm_emit.o" "$OBJ_DIR/arm_jit.o" "$OBJ_DIR/arm_jit_runtime.o")
SOURCES=(arm710 wasm_emit arm_jit arm_jit_runtime rtc_seed emubase etna eiger eiger_classifier vcfcard netpad_mmc windermere windermere_cpu revo clps7111 clps7111_serial_bridge clps7110 osaris series5 clps7600 \
         sa1100 sa1100_cpu \
         audio_codec \
         sibo_audio \
         v30 v30_ops_mov v30_ops_arith v30_ops_ctrl v30_ops_misc \
         psion_asic1 psion_asic2 psion_asic3 psion_ssd psion_condor psion_honda series3 series3c series3c_serial_bridge \
         hd6303 hd44780 psion_datapak organiser1 organiser2 \
         device_registry)
# psion_asic9 is landing on a parallel branch; compile it in if present.
if [ -f "$CORE/psion_asic9.cpp" ]; then
    SOURCES+=(psion_asic9)
fi
for src in "${SOURCES[@]}"; do
    echo "  $src.cpp"
    "$CXX" "${CXXFLAGS[@]}" -c -o "$OBJ_DIR/$src.o" "$CORE/$src.cpp"
done

# L9: core/decoder.c and core/decoder-arm.c are mGBA-derived ARM
# disassembly tables. Nothing in the emulator core or harness references
# `ARMDecodeARM`, `ARMDisassemble`, or `_armTable`; they were compiled in
# as dead weight. The .c files remain in the tree (low-risk: just not
# part of the build) so anyone who needs them for a future debugger
# integration can re-enable trivially.

echo "=== Linking harness ==="
"$CXX" "${CXXFLAGS[@]}" "$HARNESS/run.cpp" "$OBJ_DIR"/*.o -o "$OUT"

echo "=== Linking audio-harness ==="
"$CXX" "${CXXFLAGS[@]}" "$HARNESS/audio-harness.cpp" "$OBJ_DIR"/*.o \
    -o "$HARNESS/audio-harness"

for tgt in series5-audio-test clps7111-record-test windermere-audio-test netbook-audio-harness series7-audio-harness netpad-audio-harness sibo-audio-harness pacman-profile; do
    if [ -f "$HARNESS/$tgt.cpp" ]; then
        echo "=== Linking $tgt ==="
        "$CXX" "${CXXFLAGS[@]}" "$HARNESS/$tgt.cpp" "$OBJ_DIR"/*.o \
            -o "$HARNESS/$tgt"
    fi
done

# SSD frame-protocol smoke test. Tiny binary; always built so CI can
# exercise it via `tests/ssd_smoke` after the harness link succeeds.
TESTS_DIR="$REPO_ROOT/tests"
# ARM -> WASM code generator differential test (Stage 3). The C++ half writes a
# case file: for each case, the instruction word, the register file it started
# from, the state the REAL interpreter produced from it, and the generated WASM
# module. tests/unit/arm_jit_test.mjs replays the modules under node and
# compares. Run both halves with:
#   tests/unit/arm_jit_test /tmp/arm-jit-cases.bin &&
#   node tests/unit/arm_jit_test.mjs /tmp/arm-jit-cases.bin
if [ -f "$TESTS_DIR/unit/arm_jit_test.cpp" ]; then
    echo "=== Linking arm_jit_test ==="
    "$CXX" "${CXXFLAGS[@]}" "$TESTS_DIR/unit/arm_jit_test.cpp" \
        "${ARM_JIT_OBJS[@]}" "$OBJ_DIR/arm710.o" \
        -o "$TESTS_DIR/unit/arm_jit_test"
fi

if [ -f "$TESTS_DIR/unit/ssd_smoke.cpp" ]; then
    echo "=== Linking ssd_smoke ==="
    "$CXX" "${CXXFLAGS[@]}" "$TESTS_DIR/unit/ssd_smoke.cpp" \
        "$OBJ_DIR/psion_ssd.o" -o "$TESTS_DIR/unit/ssd_smoke"
fi

# VCFCard write-path test — guards the "Disk corrupt on copy/format" fix
# (no spurious post-WRITE-command IREQ; correct per-sector completion IRQ;
# written bytes land at the right LBA) for both faithfulMode states.
# netpad MMC-over-SPI card test — drives the card model the way the ROM's
# variant card-init state machine and medmmc.pdd do (CMD0/1/9/17/18/24/58,
# data tokens, CRC16) and checks the answers they depend on.
if [ -f "$TESTS_DIR/unit/mmc_card_test.cpp" ]; then
    echo "=== Linking mmc_card_test ==="
    "$CXX" "${CXXFLAGS[@]}" "$TESTS_DIR/unit/mmc_card_test.cpp" \
        "$OBJ_DIR/netpad_mmc.o" -o "$TESTS_DIR/unit/mmc_card_test"
fi

# EPOC machine-ID test — drives the ETNA identity PROM through the same
# bit-banged word reader the guest uses, so a reprogrammed machine ID is
# proven visible to EPOC (and the image checksum proven re-folded).
if [ -f "$TESTS_DIR/unit/machine_id_test.cpp" ]; then
    echo "=== Linking machine_id_test ==="
    "$CXX" "${CXXFLAGS[@]}" "$TESTS_DIR/unit/machine_id_test.cpp" \
        "$OBJ_DIR/etna.o" "$OBJ_DIR/arm710.o" "${ARM_JIT_OBJS[@]}" "$OBJ_DIR/emubase.o" \
        -o "$TESTS_DIR/unit/machine_id_test"
fi

if [ -f "$TESTS_DIR/unit/cf_write_test.cpp" ]; then
    echo "=== Linking cf_write_test ==="
    "$CXX" "${CXXFLAGS[@]}" "$TESTS_DIR/unit/cf_write_test.cpp" \
        "$OBJ_DIR/vcfcard.o" "$OBJ_DIR/arm710.o" "${ARM_JIT_OBJS[@]}" "$OBJ_DIR/emubase.o" \
        -o "$TESTS_DIR/unit/cf_write_test"
fi

echo ""
echo "Built:"
echo "  $OUT"
echo "  $HARNESS/audio-harness"
