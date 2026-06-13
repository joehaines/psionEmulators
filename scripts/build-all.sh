#!/usr/bin/env bash
# Full build: WASM + React frontend.
# Run this from the repo root after activating the Emscripten SDK.
#
# Usage:
#   source <emsdk>/emsdk_env.sh
#   bash scripts/build-all.sh

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=============================="
echo "  Psion Emulator — Full Build"
echo "=============================="
echo ""

echo "--- Step 1: WASM ---"
bash "$REPO_ROOT/scripts/build-wasm.sh"

echo ""
echo "--- Step 2: Frontend ---"
bash "$REPO_ROOT/scripts/build-frontend.sh"

echo ""
echo "=============================="
echo "  Build complete!"
echo "  Artefacts in $REPO_ROOT/dist/"
echo "=============================="
