#!/usr/bin/env bash
# Builds the React/TypeScript frontend with Vite.
# Output goes to dist/ at the repo root.
#
# Prerequisites:
#   - Node.js 20+
#   - psion.js and psion.wasm already present in frontend/public/
#     (run scripts/build-wasm.sh first)

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FRONTEND="$REPO_ROOT/frontend"

if [[ ! -f "$FRONTEND/public/psion.js" || ! -f "$FRONTEND/public/psion.wasm" ]]; then
    echo "ERROR: frontend/public/psion.js or psion.wasm not found."
    echo "Run scripts/build-wasm.sh first."
    exit 1
fi

cd "$FRONTEND"

echo "=== Installing dependencies ==="
npm ci

echo "=== Building frontend ==="
npm run build

echo ""
echo "=== Frontend build complete ==="
echo "  Output in $REPO_ROOT/dist/"
