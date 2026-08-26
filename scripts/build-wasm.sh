#!/usr/bin/env bash
# Compiles the Psion emulator C++ core to WebAssembly via Emscripten.
# Outputs psion.js + psion.wasm into frontend/public/.
#
# The C++ translation units and the link step use em++, not emcc: from
# Emscripten 4 onward the C driver no longer pulls in libc++ when the inputs
# are object files, and the link fails with operator new / __cxa_throw /
# std::to_string undefined. The two .c decoder files stay on emcc.
#
# Prerequisites:
#   - Emscripten SDK activated: source <emsdk>/emsdk_env.sh
#   - Run from the repo root or from the scripts/ directory

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$REPO_ROOT/core"
WASM_SRC="$REPO_ROOT/wasm"
OBJ_DIR="$WASM_SRC/obj"
OUT_DIR="$REPO_ROOT/frontend/public"

# Flags shared by the compile (-c) and link steps.
#
# WASM-specific perf flags:
# -mbulk-memory: lets the compiler emit memory.copy / memory.fill
#   for memcpy / memset (RAM array access in readPhysical, TLB
#   refills, framebuffer reads).  Supported on all current browsers.
# -msimd128: enables 128-bit SIMD instructions where the compiler
#   can vectorise (e.g. LCD framebuffer reads, audio sample drain).
# NOT -flto. It is worth +13% on the native build (see harness/build.sh) and
# the obvious guess is that it should be worth more here, because the hot path
# is built out of cross-translation-unit virtual calls that emcc would
# otherwise leave as call_indirect. Measured on a Series 7 boot: 10.06 s
# without, 10.08 s with — no difference, on a workload whose wall time is ~73%
# interpreter. wasm-ld/Binaryen already do the whole-program work at link time.
# Left off so the build stays fast; do not re-add it without a measurement.
COMPILE_FLAGS=(
    "-O3"
    "-mbulk-memory"
    "-msimd128"
    "-std=c++17"
)

# Link-only flags. emcc ignores these during the -c compile step and
# emits a "linker setting ignored during compilation" warning for each
# one on every translation unit — so keep them out of COMPILE_FLAGS and
# only pass them at link time. (--bind / -lembind, the -s SETTINGS, and
# SINGLE_FILE are all consumed by wasm-ld, not clang.)
LINK_FLAGS=(
    ${PSION_WASM_EXTRA_LINK:-}
    "${COMPILE_FLAGS[@]}"
    "--bind"
    "-s" "MODULARIZE=1"
    "-s" "EXPORT_NAME=createPsionModule"
    "-s" "ALLOW_MEMORY_GROWTH=1"
    # The code generator (docs/jit-engine-scope.md) puts each compiled region's
    # exported function into the module's own table and calls it by index —
    # which in Emscripten is what a C function pointer is. Growing the table is
    # how a new region gets a slot.
    "-s" "ALLOW_TABLE_GROWTH=1"
    "-s" "INITIAL_MEMORY=134217728"
    "-s" "EXPORTED_FUNCTIONS=[\"_malloc\",\"_free\"]"
    "-s" "EXPORTED_RUNTIME_METHODS=[\"HEAPU8\"]"
    # web,worker so the same module can be instantiated either on the main
    # thread (the existing path) or inside a dedicated Web Worker (the
    # off-main-thread emulation path — see docs/web-worker-emulation-scope.md).
    "-s" "ENVIRONMENT=web,worker,node"
    "-s" "EXPORT_ES6=0"
    "-s" "SINGLE_FILE=0"
)

mkdir -p "$OBJ_DIR" "$OUT_DIR"

echo "=== Building WASM: core C++ objects ==="
# Mirror harness/build.sh so the WASM and native builds link the same set of
# translation units. device_registry.cpp references every factory (Series 3 /
# Series 3c / V30 / ASIC1/2, in addition to the Windermere/CLPS-family cores),
# so all of those must be present at link time — leaving any of them out
# reproduces the wasm-ld "undefined symbol: Series3::Emulator::Emulator()"
# failure seen in CI.
SOURCES=(arm710 wasm_emit arm_jit arm_jit_runtime rtc_seed emubase etna eiger eiger_classifier vcfcard netpad_mmc windermere windermere_cpu revo clps7111 clps7111_serial_bridge clps7110 osaris series5 clps7600 \
         sa1100 sa1100_cpu \
         audio_codec \
         sibo_audio \
         v30 v30_ops_mov v30_ops_arith v30_ops_ctrl v30_ops_misc \
         psion_asic1 psion_asic2 psion_asic3 psion_ssd psion_condor psion_honda series3 series3c series3c_serial_bridge \
         hd6303 hd44780 psion_datapak organiser2 \
         device_registry)
# psion_asic9 is landing on a parallel branch; compile it in if present.
if [ -f "$CORE/psion_asic9.cpp" ]; then
    SOURCES+=(psion_asic9)
fi
for src in "${SOURCES[@]}"; do
    echo "  Compiling $src.cpp"
    em++ "${COMPILE_FLAGS[@]}" -c -o "$OBJ_DIR/$src.o" "$CORE/$src.cpp"
done

echo "=== Building WASM: C decoder objects ==="
emcc -O3 -mbulk-memory -msimd128 -c -o "$OBJ_DIR/decoder.o"     "$CORE/decoder.c"
emcc -O3 -mbulk-memory -msimd128 -c -o "$OBJ_DIR/decoder-arm.o" "$CORE/decoder-arm.c"

echo "=== Linking ==="
em++ "${LINK_FLAGS[@]}" "$OBJ_DIR"/*.o "$WASM_SRC/main.cpp" -o "$OUT_DIR/psion.js"

echo ""
echo "=== WASM build complete ==="
echo "  $OUT_DIR/psion.js"
echo "  $OUT_DIR/psion.wasm"
