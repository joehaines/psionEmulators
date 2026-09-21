#!/usr/bin/env bash
# Builds the Psion emulator WASM module using Emscripten.
# Outputs psion.js + psion.wasm into ../frontend/public/
# Requires: emcc (Emscripten), run after activating emsdk environment.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE="$SCRIPT_DIR/../core"
OUT_DIR="$SCRIPT_DIR/../frontend/public"

FLAGS="-O3 -flto -mbulk-memory -msimd128 -std=c++17 --bind \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createPsionModule \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=134217728 \
  -s EXPORTED_FUNCTIONS=[\"_malloc\",\"_free\"] \
  -s EXPORTED_RUNTIME_METHODS=[\"HEAPU8\"] \
  -s ENVIRONMENT=web,worker \
  -s SINGLE_FILE=0"

mkdir -p obj "$OUT_DIR"

echo "--- Compiling core C++ objects ---"
# Keep in sync with harness/build.sh: device_registry.cpp pulls in all
# device factories, so every translation unit that backs a registered
# device must be compiled here or the WASM link will have undefined symbols.
SOURCES=(
    arm710 emubase etna eiger eiger_classifier vcfcard windermere windermere_cpu revo clps7111 clps7111_serial_bridge clps7110 osaris series5 clps7600
    sa1100 sa1100_cpu
    audio_codec
    sibo_audio
    v30 v30_ops_mov v30_ops_arith v30_ops_ctrl v30_ops_misc
    psion_asic1 psion_asic2 psion_asic3 psion_ssd psion_condor psion_honda series3 series3c series3c_serial_bridge
    hd6303 hd44780 psion_datapak organiser1 organiser2
    device_registry
)
# psion_asic9 is landing on a parallel branch; compile it in if present.
if [ -f "$CORE/psion_asic9.cpp" ]; then
    SOURCES+=(psion_asic9)
fi
for src in "${SOURCES[@]}"; do
    echo "  $src.cpp"
    emcc -c $FLAGS -o obj/$src.o "$CORE/$src.cpp"
done

echo "--- Compiling C decoder objects ---"
emcc -c -O3 -flto -o obj/decoder.o "$CORE/decoder.c"
emcc -c -O3 -flto -o obj/decoder-arm.o "$CORE/decoder-arm.c"

echo "--- Linking ---"
emcc $FLAGS obj/*.o "$SCRIPT_DIR/main.cpp" -o "$OUT_DIR/psion.js"

echo "--- Done ---"
echo "Output: $OUT_DIR/psion.js + psion.wasm"
