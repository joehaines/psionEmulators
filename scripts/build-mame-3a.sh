#!/usr/bin/env bash
# Builds the Psion Series 3a target of MAME 0.253 as a WebAssembly module.
#
# Produces psion3aweb.{js,wasm,data,data.js} in frontend/public/mame-3a/.
# Source guide: docs/psion3a-web-mame-build-guide.md
#
# Prerequisites:
#   - emsdk 3.0.0 activated (source ~/emsdk/emsdk_env.sh)
#   - MAME 0.253 source tree (default: $HOME/mamedev/mame0253)
#       git clone --branch mame0253 https://github.com/mamedev/mame.git mame0253
#   - Series 3a ROM image $ROM at: roms/s3a_v3.22f_eng.bin
#
# Environment overrides:
#   MAME_DIR  : path to MAME source tree (default $HOME/mamedev/mame0253)
#   ROM       : Series 3a ROM filename inside roms/ (default s3a_v3.22f_eng.bin)
#   JOBS      : parallel make jobs (default nproc)
#   OPTIMIZE  : emscripten optimisation level 0-3 (default 0; CI uses 3)
#   SYMBOLS   : include debug symbols 0/1 (default 1; CI uses 0)

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$REPO_ROOT/frontend/public/mame-3a"
MAME_DIR="${MAME_DIR:-$HOME/mamedev/mame0253}"
ROM_NAME="${ROM:-s3a_v3.22f_eng.bin}"
ROM_PATH="$REPO_ROOT/roms/$ROM_NAME"
PB2_ROM_NAME="${PB2_ROM:-pb2_v1.30f_acn.bin}"
PB2_ROM_PATH="$REPO_ROOT/roms/$PB2_ROM_NAME"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
OPTIMIZE="${OPTIMIZE:-0}"
SYMBOLS="${SYMBOLS:-1}"

if ! command -v emcc >/dev/null 2>&1; then
    echo "ERROR: emcc not on PATH. Activate emsdk first:" >&2
    echo "       source \"\$HOME/emsdk/emsdk_env.sh\"" >&2
    exit 1
fi
if [ ! -d "$MAME_DIR/src/mame/psion" ]; then
    echo "ERROR: MAME source not found at $MAME_DIR" >&2
    echo "       git clone --branch mame0253 https://github.com/mamedev/mame.git \"$MAME_DIR\"" >&2
    exit 1
fi
if [ ! -f "$ROM_PATH" ]; then
    echo "ERROR: ROM not found at $ROM_PATH" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

# MAME's makefile builds a native `genie` binary as a bootstrap step before the
# wasm target. If emmake is the outer driver, the wasm-specific LDFLAGS leak
# into that native link and cause /usr/bin/ld to choke on -s FORCE_FILESYSTEM=1.
# Pre-building genie with the ordinary native toolchain avoids the problem: the
# subsequent emmake call finds the binary already up-to-date and skips it.
echo "=== Pre-building native genie (bootstrap tool) ==="
cd "$MAME_DIR"
make -C 3rdparty/genie -j"$JOBS"

echo "=== Building MAME psion3aweb target (this can take 30-60 min) ==="
cd "$MAME_DIR"
emmake make -j"$JOBS" \
    SUBTARGET=psion3aweb \
    SOURCES=src/mame/psion/psion3a.cpp \
    REGENIE=1 \
    WEBASSEMBLY=1 \
    NOWERROR=1 \
    SYMBOLS="$SYMBOLS" \
    OPTIMIZE="$OPTIMIZE" \
    EMCCFLAGS="-s FORCE_FILESYSTEM=1 -s NO_DISABLE_EXCEPTION_CATCHING=1" \
    LDFLAGS="-s FORCE_FILESYSTEM=1 -s NO_DISABLE_EXCEPTION_CATCHING=1"

if [ ! -f "$MAME_DIR/psion3aweb.js" ] || [ ! -f "$MAME_DIR/psion3aweb.wasm" ]; then
    echo "ERROR: psion3aweb.js / psion3aweb.wasm not produced" >&2
    exit 1
fi

echo "=== Packaging ROMs into Emscripten virtual filesystem ==="
ROM_STAGE="$(mktemp -d)"
trap 'rm -rf "$ROM_STAGE"' EXIT
mkdir -p "$ROM_STAGE/roms"
cp "$ROM_PATH" "$ROM_STAGE/roms/"
(cd "$ROM_STAGE/roms" && zip -q psion3a.zip "$ROM_NAME")

# Bundle Pocket Book II ROM if available (same MAME driver, different machine name)
PB2_PRELOAD_ARGS=""
if [ -f "$PB2_ROM_PATH" ]; then
    echo "  Bundling pocketbk2 ROM: $PB2_ROM_NAME"
    cp "$PB2_ROM_PATH" "$ROM_STAGE/roms/"
    (cd "$ROM_STAGE/roms" && zip -q pocketbk2.zip "$PB2_ROM_NAME")
    PB2_PRELOAD_ARGS="--preload $ROM_STAGE/roms/pocketbk2.zip@/roms/pocketbk2.zip --preload $ROM_STAGE/roms/pocketbk2.zip@/pocketbk2.zip --preload $ROM_STAGE/roms/pocketbk2.zip@/roms/pocketbk2/pocketbk2.zip"
else
    echo "  pocketbk2 ROM not found at $PB2_ROM_PATH — skipping"
fi

cp "$MAME_DIR/psion3aweb.js"   "$OUT_DIR/"
cp "$MAME_DIR/psion3aweb.wasm" "$OUT_DIR/"

# file_packager.py lives under the active emsdk install
FILE_PACKAGER="$(dirname "$(command -v emcc)")/../emscripten/tools/file_packager.py"
if [ ! -f "$FILE_PACKAGER" ]; then
    FILE_PACKAGER="$(python3 -c 'import os,emscripten; print(os.path.join(os.path.dirname(emscripten.__file__),"tools","file_packager.py"))' 2>/dev/null || echo "")"
fi
if [ ! -f "$FILE_PACKAGER" ]; then
    echo "ERROR: emscripten file_packager.py not found" >&2
    exit 1
fi

cd "$OUT_DIR"
rm -f psion3aweb.data psion3aweb.data.js
# shellcheck disable=SC2086
python3 "$FILE_PACKAGER" psion3aweb.data \
    --preload "$ROM_STAGE/roms/psion3a.zip@/roms/psion3a.zip" \
    --preload "$ROM_STAGE/roms/psion3a.zip@/psion3a.zip" \
    --preload "$ROM_STAGE/roms/psion3a.zip@/roms/psion3a/psion3a.zip" \
    $PB2_PRELOAD_ARGS \
    --js-output=psion3aweb.data.js

echo ""
echo "=== Built ==="
ls -la "$OUT_DIR"/psion3aweb.*
