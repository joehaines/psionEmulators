#!/usr/bin/env bash
# Standalone runner for the FEFS round-trip test in
# frontend/src/lib/__tests__/fefs.test.mts.
#
# Uses Node 22's --experimental-strip-types flag so we don't need a
# vitest / tsx install. CI invokes this directly.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST="$REPO_ROOT/frontend/src/lib/__tests__/fefs.test.mts"

if ! command -v node >/dev/null 2>&1; then
    echo "node not found in PATH" >&2
    exit 2
fi

node --experimental-strip-types "$TEST"
