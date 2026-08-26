#!/usr/bin/env bash
# Runs both halves of the ARM -> WASM code generator differential test.
# Build first with `bash harness/build.sh`.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
cases="${TMPDIR:-/tmp}/arm-jit-cases.bin"
"$here/arm_jit_test" "$cases"
node "$here/arm_jit_test.mjs" "$cases"
