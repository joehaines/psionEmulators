#!/usr/bin/env bash
# Standalone runner for the FAT16 directory-support test in
# frontend/src/lib/__tests__/fat16.test.mts.
#
# Mirrors scripts/test-fefs.sh — uses Node 22's --experimental-strip-types
# flag so we don't need a vitest / tsx install.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST="$REPO_ROOT/frontend/src/lib/__tests__/fat16.test.mts"

if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi

node --experimental-strip-types "$TEST"
