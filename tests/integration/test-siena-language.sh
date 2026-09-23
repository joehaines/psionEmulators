#!/usr/bin/env bash
# Locale test for the Psion Siena.
#
# The Siena v4.20f image carries four locales — English (UK), English
# (USA), Swedish and Spanish — and picks one at boot from three strap pins
# on ASIC9 port C, which the emulator supplies. See the locale section in
# core/series3c.cpp: the ROM's index of locale blocks is the eight segment
# words at 0xFFFE0, and at A000:27E9 the kernel senses the straps and
# copies the chosen block into RAM as the live country record.
#
# So this test boots each locale and checks the machine's own copy of it:
# the first 16 bytes of locale block N (country code, date and time
# format, separators, currency symbol) have to be in RAM after booting
# with --language N, and no other block's have to be. Those 16 bytes differ
# between all four blocks — 44 / £, 1 / $, 46 / SEK, 34 / Pts — so a
# locale that did not land shows up as the wrong one, not as a pass.
#
# A screenshot would show it too, but only in some apps (the Time app's
# clock is 12-hour am/pm for UK and USA, 24-hour for Swedish and
# Spanish); the System screen the machine boots to is identical in all
# four.
#
# Exit codes:
#   0 = PASS
#   1 = FAIL — a boot produced no RAM snapshot, or the ROM's table is
#              not the four-entry one this test expects
#   2 = FAIL — a boot is not running on the locale it was given
#
# Usage: tests/integration/test-siena-language.sh [extra args passed to harness/run]

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
ROM="$REPO/roms/siena_v4.20f_eng.bin"
TMP=$(mktemp -d /tmp/sienalang.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "FAIL: $HARNESS not built — run bash harness/build.sh first"
    exit 1
fi

# The locale is copied into RAM early in the kernel's start-up, long
# before the System screen paints; 20 s of simulated boot is ample.
# PSION_RTC_SEED is pinned so the runs differ in nothing but the locale.
run_variant() {
    local index=$1; shift
    PSION_RTC_SEED=1000000000 "$HARNESS" "$ROM" \
        --device siena \
        --language "$index" \
        --boot-seconds 20 \
        --skip-card \
        --min-mdrive-subdirs 0 \
        --save-ram-snapshot "$TMP/ram$index.bin" \
        "$@" \
        > "$TMP/run$index.log" 2>&1
}

NAMES=("English (UK)" "English (USA)" "Swedish" "Spanish")
for i in 0 1 2 3; do
    echo "=== booting ${NAMES[$i]}, language index $i ==="
    run_variant "$i" "$@" &
done
wait

python3 - "$TMP" "$ROM" <<'PY'
import os, struct, sys

tmp, rom_path = sys.argv[1], sys.argv[2]
rom = open(rom_path, 'rb').read()

# The locale index: segment words at linear 0xFFFE0, which on the 1 MiB
# Siena image is file offset 0xFFFE0; 0xFFFF ends it.
segs = []
for i in range(8):
    seg = struct.unpack_from('<H', rom, 0xFFFE0 + 2 * i)[0]
    if seg == 0xFFFF:
        break
    segs.append(seg)
if len(segs) != 4:
    print(f'FAIL[1]: expected four locale entries at 0xFFFE0, found {len(segs)}')
    sys.exit(1)

heads = [rom[s * 16:s * 16 + 16] for s in segs]
if len(set(heads)) != 4:
    print('FAIL[1]: two locale blocks share their first 16 bytes — this test '
          'could not tell them apart')
    sys.exit(1)

names = ['English (UK)', 'English (USA)', 'Swedish', 'Spanish']
expected_country = [44, 1, 46, 34]
for i, head in enumerate(heads):
    country = struct.unpack_from('<H', head, 0)[0]
    if country != expected_country[i]:
        print(f'FAIL[1]: locale {i} is country {country}, expected '
              f'{expected_country[i]} ({names[i]})')
        sys.exit(1)

for i in range(4):
    path = f'{tmp}/ram{i}.bin'
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        print(f'FAIL[1]: language {i} produced no RAM snapshot — the machine '
              f'did not run. See {tmp}/run{i}.log')
        sys.exit(1)
    ram = open(path, 'rb').read()
    found = [j for j, head in enumerate(heads) if head in ram]
    if found != [i]:
        got = ', '.join(names[j] for j in found) or 'none of them'
        print(f'FAIL[2]: booted as {names[i]} but RAM holds the locale record '
              f'of {got} — the port C strap is not reaching the kernel')
        sys.exit(2)
    print(f'index {i}: running on {names[i]} (country {expected_country[i]})')

print('PASS siena language: all four locales boot on their own record')
PY
