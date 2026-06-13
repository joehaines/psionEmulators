#!/usr/bin/env bash
# Two-pass Profile-Guided Optimization build of the native harness.
#
# PGO delivers ~15 % wall-time improvement on the netBook full-OS
# benchmark (35 s vs 41 s for 25 sim sec).
#
# KNOWN ISSUE (2026-05-24): Windermere devices (revo, 5mx, 5mxpro,
# mc218) segfault (exit 139) with the PGO-optimised binary even when
# their boot is included in the training profile.  Root cause is
# likely UB in windermere.cpp that PGO's aggressive inlining /
# devirtualisation exposes.  Use for SA-1100 / SIBO devices only
# until the Windermere crash is investigated.
#
# Usage:
#   bash harness/build-pgo.sh                    # full pipeline
#   bash harness/build-pgo.sh --profile-only     # just generate .gcda
#   bash harness/build-pgo.sh --rebuild-only     # reuse existing .gcda

set -e
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORE="$REPO_ROOT/core"
HARNESS="$REPO_ROOT/harness"
OBJ_DIR="$HARNESS/obj"
OUT="$HARNESS/run"
PROF_DIR="/tmp/psion-pgo"

CXX="${CXX:-g++}"
COMMON=(-std=c++17 -Wno-deprecated-declarations -Wno-multichar)

SOURCES=(arm710 emubase etna eiger eiger_classifier vcfcard windermere revo clps7111 clps7110 osaris series5 clps7600 \
         sa1100 audio_codec sibo_audio \
         v30 v30_ops_mov v30_ops_arith v30_ops_ctrl v30_ops_misc \
         psion_asic1 psion_asic2 psion_asic3 psion_ssd psion_condor psion_honda series3 series3c series3c_serial_bridge \
         hd6303 hd44780 psion_datapak organiser2 device_registry)
[ -f "$CORE/psion_asic9.cpp" ] && SOURCES+=(psion_asic9)

do_instrument() {
    echo "=== Pass 1: instrument build ==="
    rm -rf "$OBJ_DIR" "$PROF_DIR" && mkdir -p "$OBJ_DIR" "$PROF_DIR"
    for src in "${SOURCES[@]}"; do
        "$CXX" -O3 -fprofile-generate="$PROF_DIR" "${COMMON[@]}" -c -o "$OBJ_DIR/$src.o" "$CORE/$src.cpp"
    done
    "$CXX" -O3 -fprofile-generate="$PROF_DIR" -c -o "$OBJ_DIR/decoder.o" "$CORE/decoder.c"
    "$CXX" -O3 -fprofile-generate="$PROF_DIR" -c -o "$OBJ_DIR/decoder-arm.o" "$CORE/decoder-arm.c"
    "$CXX" -O3 -fprofile-generate="$PROF_DIR" "${COMMON[@]}" "$HARNESS/run.cpp" "$OBJ_DIR"/*.o -o "$OUT" -lgcov

    echo "=== Pass 1: generating profile (boot all devices) ==="
    bash "$REPO_ROOT/scripts/test-boot.sh" --all 2>&1 | grep -E "^(PASS|FAIL)"
    echo "=== $(ls "$PROF_DIR"/*.gcda 2>/dev/null | wc -l) gcda files generated ==="
}

do_rebuild() {
    echo "=== Pass 2: PGO-optimised build ==="
    rm -rf "$OBJ_DIR" && mkdir -p "$OBJ_DIR"
    for src in "${SOURCES[@]}"; do
        "$CXX" -O3 -fprofile-use="$PROF_DIR" -fprofile-correction "${COMMON[@]}" -c -o "$OBJ_DIR/$src.o" "$CORE/$src.cpp" 2>/dev/null
    done
    "$CXX" -O3 -fprofile-use="$PROF_DIR" -fprofile-correction -c -o "$OBJ_DIR/decoder.o" "$CORE/decoder.c" 2>/dev/null
    "$CXX" -O3 -fprofile-use="$PROF_DIR" -fprofile-correction -c -o "$OBJ_DIR/decoder-arm.o" "$CORE/decoder-arm.c" 2>/dev/null
    "$CXX" -O3 -fprofile-use="$PROF_DIR" -fprofile-correction "${COMMON[@]}" "$HARNESS/run.cpp" "$OBJ_DIR"/*.o -o "$OUT"
    echo "=== PGO build complete: $OUT ==="
}

case "${1:-}" in
    --profile-only) do_instrument ;;
    --rebuild-only) do_rebuild ;;
    *)              do_instrument; do_rebuild ;;
esac
