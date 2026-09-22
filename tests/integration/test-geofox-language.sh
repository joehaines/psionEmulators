#!/usr/bin/env bash
# Language-variant test for the Geofox One.
#
# The Geofox is the one machine in the tree whose ROM carries two
# languages — English (UK) and English (USA) — and picks between them at
# boot from a number the emulator supplies: the iLanguageIndex and
# iKeyboardIndex fields in the settings PROM it reads over the SSI bus.
# See the language section in core/geofox.h for the whole chain, from
# VArmPG.dll's accessors through TMachineInfoV1 to the window server
# loading ELOCL<n> and EKDATA<n>, and on to Bafl.dll turning the locale's
# language into a ".ruk" or ".rus" resource extension.
#
# The chain is invisible in a screenshot: both variants are English, so
# the desktop comes up pixel-for-pixel identical either way — the icon
# labels, the epoc hand and the analogue clock are the same in both. What
# does differ is which files the machine has open, so this test boots each
# variant and reads the answer out of RAM:
#
#   index 0  Z:\System\Apps\SHELL\Shell.ruk, and no ELocl1 / Ekdata1
#   index 1  Z:\System\Apps\SHELL\Shell.rus, plus Z:\System\Libs\ELocl1.dll
#            and Z:\System\Libs\Ekdata1.dll
#
# Both halves matter. Finding .rus proves the language reached the
# resource loader (the thing the user sees); finding ELocl1.dll proves it
# got there the way the hardware does it, through the window server's
# locale load, rather than by some other route.
#
# Exit codes:
#   0 = PASS
#   1 = FAIL — a boot produced no RAM snapshot (the machine did not run)
#   2 = FAIL — the UK boot is not on the UK resources
#   3 = FAIL — the USA boot is not on the USA resources
#
# Usage: tests/integration/test-geofox-language.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM="$REPO/roms/Geofox_v1.01(146)_eng.bin"
TMP=$(mktemp -d /tmp/gflang.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "FAIL: $HARNESS not built — run bash harness/build.sh first"
    exit 1
fi

# The window server loads the locale and keyboard DLLs well before the
# desktop paints, so 45 s of simulated boot is ample. PSION_RTC_SEED is
# pinned so the two runs differ in nothing but the language.
run_variant() {
    local index=$1; shift
    PSION_RTC_SEED=1000000000 "$HARNESS" "$ROM" \
        --device geofox \
        --language "$index" \
        --boot-seconds 45 \
        --post-attach-seconds 5 \
        --save-ram-snapshot "$TMP/ram$index.bin" \
        "$@" \
        > "$TMP/run$index.log" 2>&1
}

echo "=== booting English (UK), language index 0 ==="
run_variant 0 "$@"
echo "=== booting English (USA), language index 1 ==="
run_variant 1 "$@"

python3 - "$TMP" <<'PY'
import os, sys

tmp = sys.argv[1]


def ram(index):
    path = f'{tmp}/ram{index}.bin'
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        print(f'FAIL[1]: language {index} produced no RAM snapshot — the '
              f'machine did not run. See {tmp}/run{index}.log')
        sys.exit(1)
    with open(path, 'rb') as fh:
        return fh.read()


uk, usa = ram(0), ram(1)

SHELL_UK = rb'Z:\System\Apps\SHELL\Shell.ruk'
SHELL_US = rb'Z:\System\Apps\SHELL\Shell.rus'
LOCALE_US = rb'Z:\System\Libs\ELocl1.dll'
KEYB_US = rb'Z:\System\Libs\Ekdata1.dll'

# ── index 0: the UK resources, and nothing suffixed ───────────────────
if SHELL_UK not in uk:
    print('FAIL[2]: the default boot has no Shell.ruk open — the language '
          'index 0 is not reaching the resource loader at all')
    sys.exit(2)
if SHELL_US in uk or LOCALE_US in uk or KEYB_US in uk:
    print('FAIL[2]: the default boot picked up USA resources — index 0 has '
          'to leave the machine on the unsuffixed ELocl.dll / Ekdata.dll')
    sys.exit(2)
print('index 0: Shell.ruk open, no ELocl1 / Ekdata1')

# ── index 1: the USA resources, by way of the window server ───────────
if SHELL_US not in usa:
    print('FAIL[3]: the USA boot has no Shell.rus open — the language index '
          'reached the PROM but not the resource extension. Check the '
          'iLanguageIndex field (PROM bytes 4..5) and its checksum')
    sys.exit(3)
if LOCALE_US not in usa:
    print('FAIL[3]: the USA boot never loaded Z:\\System\\Libs\\ELocl1.dll — '
          'the window server did not act on iLanguageIndex')
    sys.exit(3)
if KEYB_US not in usa:
    print('FAIL[3]: the USA boot never loaded Z:\\System\\Libs\\Ekdata1.dll — '
          'iKeyboardIndex (PROM bytes 6..7) is not getting through')
    sys.exit(3)
if SHELL_UK in usa:
    print('FAIL[3]: the USA boot still has Shell.ruk open alongside '
          'Shell.rus — the machine is running a mix of the two')
    sys.exit(3)
print('index 1: Shell.rus open, ELocl1.dll and Ekdata1.dll loaded')

print('PASS geofox language: both variants boot on their own resources')
PY
