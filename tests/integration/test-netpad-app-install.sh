#!/usr/bin/env bash
# App-library install on the netpad, end to end on the real ROM.
#
# The netpad's EPOC R5 ROM ships without applications — they were SIS
# installers on Psion Teklogix's support CD, which the app library
# preserves as applib/netpad and the emulator's "Install standard apps"
# button writes onto one MMC card. This delivers with the production
# code, boots the machine, drives the install through its own UI and
# requires the app to appear in Extras.
#
# See tests/integration/test-netpad-app-install.mts for the chain and
# the pen/key script.
#
# Usage:
#   bash tests/integration/test-netpad-app-install.sh [app-id]
#   bash tests/integration/test-netpad-app-install.sh --standard-apps

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# --standard-apps (the button's own path: the whole CD set on one card)
# takes no app id; anything else names the single app to install.
if [ "${1:-}" = "--standard-apps" ]; then
    MODE=("--standard-apps")
    shift
else
    MODE=("--app" "${1:-netpad/word}")
    [ $# -gt 0 ] && shift || true
fi

if [ ! -x "$REPO_ROOT/harness/run" ]; then
    bash "$REPO_ROOT/harness/build.sh" >&2
fi
if [ ! -f "$REPO_ROOT/roms/Netpad.img" ]; then
    echo "SKIP: netpad ROM not present" >&2
    exit 0
fi

exec node --experimental-strip-types \
    "$REPO_ROOT/tests/integration/test-netpad-app-install.mts" \
    "${MODE[@]}" "$@"
