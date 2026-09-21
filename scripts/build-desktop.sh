#!/usr/bin/env bash
# Builds the native desktop apps (Windows + macOS).
#
# Usage:
#   source <emsdk>/emsdk_env.sh        # only needed if the WASM isn't built yet
#   bash scripts/build-desktop.sh [mac|win|all|dir]
#
# `dir` produces an unpacked app for the current platform without an
# installer, which is the quickest way to try a change.
#
# macOS artifacts REQUIRE a macOS host — there is no cross-compiling a .app.
# Windows NSIS is best built on Windows; from Linux it needs wine.

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-all}"

echo "=============================="
echo "  Psion Emulator — Desktop"
echo "=============================="
echo ""

# ── Step 1: WASM ─────────────────────────────────────────────────────────
# Reuses the web build's output, since the desktop renderer is the same app.
if [ ! -f "$REPO_ROOT/frontend/public/psion.js" ]; then
  echo "--- Step 1: WASM (not present — building) ---"
  bash "$REPO_ROOT/scripts/build-wasm.sh"
else
  echo "--- Step 1: WASM (reusing frontend/public/psion.js) ---"
fi

# ── Step 2: renderer ─────────────────────────────────────────────────────
# A separate vite config: base stays '/psion/' so index.html's hardcoded
# paths keep working, the PWA service worker is dropped, and the output goes
# straight into desktop/build/renderer.
echo ""
echo "--- Step 2: renderer ---"
cd "$REPO_ROOT/frontend"
[ -d node_modules ] || npm ci
npm run build:desktop

# ── Step 3: main + preload ───────────────────────────────────────────────
echo ""
echo "--- Step 3: main process + preload ---"
cd "$REPO_ROOT/desktop"
[ -d node_modules ] || npm ci
npm run build:main

# ── Step 4: strip what the desktop app never draws ───────────────────────
#
# Keep:
#   skins/         21 MB. This IS the screen-only artwork — 5mx.png, revo.png,
#                  mc218.png, organiser2.svg, 7_netbook.jpeg — and it also
#                  carries the SIBO app-button bars and the netpad silkscreen
#                  strip. Drop it and you get a bare canvas with no app
#                  buttons. (Easy to get backwards: device-skins/ is the case
#                  photos, skins/ is the screens.)
#   device-skins/  22 MB of case photos. Only reached with deviceMode on,
#                  which the shell does turn on for the Organiser II, whose
#                  36-key pad IS the machine. Shipping them is what makes that
#                  device usable at all.
#   device-logos/  1.1 MB, the device switcher grid.
#
# Drop:
#   intro/         Home page artwork; the desktop never renders Home.
#   mame-3a/       The MAME Series 3a iframe route, unreachable here.
#   api/           PHP analytics. Must not ship inside an app — and the
#                  desktop build stubs out every call site anyway.
echo ""
echo "--- Step 4: pruning renderer assets ---"
RENDERER="$REPO_ROOT/desktop/build/renderer"
for dir in api mame-3a intro; do
  if [ -d "$RENDERER/$dir" ]; then
    rm -rf "$RENDERER/$dir"
    echo "  removed $dir/"
  fi
done
# device-skins/ is kept: the shell falls back to the skinned view for the
# Organiser II, whose keypad is the machine. 22 MB is worth not shipping a
# device that cannot be used.
echo "  kept skins/ device-skins/ device-logos/"

# ── Step 5: package ──────────────────────────────────────────────────────
echo ""
echo "--- Step 5: packaging ($TARGET) ---"
case "$TARGET" in
  mac) npx electron-builder --mac dmg zip ;;
  win) npx electron-builder --win nsis zip ;;
  dir) npx electron-builder --dir ;;
  all)
    if [ "$(uname -s)" = "Darwin" ]; then
      npx electron-builder --mac dmg zip --win nsis zip
    else
      echo "  not on macOS — building Windows only."
      echo "  (a .app/.dmg cannot be cross-compiled; use a macOS host or runner)"
      npx electron-builder --win nsis zip
    fi
    ;;
  *) echo "unknown target: $TARGET (expected mac, win, all or dir)"; exit 1 ;;
esac

echo ""
echo "=============================="
echo "  Done — artefacts in desktop/dist/"
echo "=============================="
