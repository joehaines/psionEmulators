#!/usr/bin/env bash
# LCD backlight test: every backlit machine, driven by its own key.
#
# The emulator never switches a backlight itself. Each core reports the
# output its ROM drives (EmuBase::getBacklightLevel — the per-machine pins
# are tabled in core/emubase.h) and the key its keyboard does it with
# (EmuBase::getBacklightKey). The harness's --press-backlight presses that
# key, or modifier+key chord, and logs every change of the level, so this
# test checks the three things that have to agree on a real machine:
#
#   * on/off machines: the first press lights the panel and the second
#     puts it out — the ROM's own backlight service reached through the
#     ROM's own key handling, and the level read back off the right pin;
#   * the Series 7 (and the netBook, with --slow): Fn+Space steps the
#     ASIC14 brightness register, 8 -> 12 -> 16 of 31 from the OS's
#     boot setting;
#   * machines without a lamp report none, and stay dark under the chord
#     their ROM would answer — the Revo and Series 3c ROMs both drive the
#     pin (they share the 5mx's and 3mx's code), the boards have nothing
#     on the end of it. (The Conan, which runs the same code on the
#     Revo's board, does light — a blue panel — and is checked as such.)
#
# The OS's own backlight timer is not exercised here (the 5mx's runs for a
# minute, the 3mx's two) — see the auto-off note in docs/backlight.md.
#
# Exit codes: 0 = PASS, 1 = FAIL (setup), 2 = FAIL (a device misbehaved)
#
# Usage: tests/integration/test-backlight.sh [--slow] [device-id ...]
#   --slow  also boot the netBook from its OS card (several minutes)

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=${HARNESS:-$REPO/harness/run}
TMP=$(mktemp -d /tmp/backlight.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "FAIL: $HARNESS not built — run bash harness/build.sh first"
    exit 1
fi

SLOW=0
ONLY=()
for a in "$@"; do
    case "$a" in
        --slow) SLOW=1 ;;
        *) ONLY+=("$a") ;;
    esac
done

# id | ROM | boot seconds | first press | second press | expectation | extra args
#   onoff     — dark until the first press, lit by it, dark after the second
#   dim:A,B,C — brightness A at boot, B after the first press, C after the second
#   none      — no backlight; the args carry the chord its ROM answers anyway
DEVICES=(
"5mx|Series5mx/5mx_v1.05(260)_eng/5mx_v1.05(260)_eng.bin|40|30|36|onoff|"
"5mxpro|Series5mxPRO/5mxPRO_v1.05(319)_patch_site_eng/5mxPRO_v1.05(319)_patch_eng.bin|40|30|36|onoff|"
"mc218|MC218/MC218_v1.05(259)_eng/MC218_v1.05(259)_eng.bin|40|30|36|onoff|"
"conan|Conan/Conan_v0.10(17)_eng/conan_v0.10(17)_eng.IMG|96|85|91|onoff|"
"conanv001|Conan/Conan_v0.01(22)_engbuild/s2_2201.engbuild.img|58|45|51|onoff|"
"series5|Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin|40|30|36|onoff|"
"osaris|Osaris/Osaris_v1.02(209)_eng/Osaris_v1.02(209)_eng.bin|40|30|36|onoff|"
"geofox|Geofox/Geofox_v1.01(146)_eng/Geofox_v1.01(146)_eng.bin|40|30|36|onoff|"
"series7|Series7/S7_v1.05(254)_b756_eng/S7_v1.05(254)_b756_eng.bin|34|25|30|dim:26,39,52|"
"series3mx|Series3mx/maple_v6.16f_eng/maple_v6.16f_eng.bin|20|12|16|onoff|"
"workabout|Workabout/w1_v2.40f_eng/w1_v2.40f_eng.bin|18|10|14|onoff|"
"workaboutmx|WorkaboutMX/w2mx_v7.20f_eng/w2mx_v7.20f_eng.bin|18|10|14|onoff|"
"hc120|HC120/hc120_v1.72F_eng/hc120_v1.72F.bin|18|10|14|onoff|"
"revo|Revo/Revo_v1.06(390)_eng/Revo_v1.06(390)_eng.bin|40|30|36|none|--press-key 30 24 30 --press-key 30.2 5 8"
"series3c|Series3c/oak_v5.20f_eng/oak_v5.20f_eng.bin|20|12|16|none|--press-key 12 20 30 --press-key 12.2 5 8"
)
if [ "$SLOW" = 1 ]; then
    DEVICES+=("netbook|netBook/BootLoader/netBook_BL_v011_eng/netBook_BL_v011_eng.bin|3|80|86|dim:26,39,52|--card-path roms/netBook/netBook_v1.05(450)_eng/OS.IMG --post-attach-seconds 90")
fi

run_device() {
    local row=$1
    IFS='|' read -r id rom boot p1 p2 expect extra <<<"$row"
    local presses=""
    [ "$expect" != none ] && presses="--press-backlight $p1 --press-backlight $p2"
    # shellcheck disable=SC2086
    (cd "$REPO" && PSION_RTC_SEED=1000000000 "$HARNESS" "roms/$rom" \
        --device "$id" --boot-seconds "$boot" --quiet-logs \
        $presses $extra) > "$TMP/$id.log" 2>&1
    echo "$row" > "$TMP/$id.row"
}

N=$(nproc 2>/dev/null || echo 4)
for row in "${DEVICES[@]}"; do
    id=${row%%|*}
    if [ ${#ONLY[@]} -gt 0 ] && [[ ! " ${ONLY[*]} " == *" $id "* ]]; then continue; fi
    while [ "$(jobs -rp | wc -l)" -ge "$N" ]; do sleep 0.5; done
    echo "=== $id ==="
    run_device "$row" &
done
wait

python3 - "$TMP" <<'PY'
import glob, json, os, re, sys

tmp = sys.argv[1]
fail = 0
for rowf in sorted(glob.glob(os.path.join(tmp, '*.row'))):
    dev, rom, boot, p1, p2, expect, extra = open(rowf).read().rstrip('\n').split('|')
    p1, p2 = float(p1), float(p2)
    log = open(os.path.join(tmp, dev + '.log'), errors='replace').read()
    js = re.search(r'PSION_BOOT_JSON:(\{.*\})', log)
    if not js:
        print(f'FAIL {dev}: no summary — the run did not finish'); fail = 1; continue
    summary = json.loads(js.group(1))
    # (time, level) for every change, starting from dark at power-on.
    changes = [(float(t), int(l)) for t, l in
               re.findall(r'=== t=([\d.]+)s backlight (?:ON|OFF) level (\d+) ===', log)]

    def level_at(t):
        level = 0
        for when, l in changes:
            if when <= t: level = l
        return level

    # Each press lands 0.2 s after its time (the modifier goes down first);
    # give the OS a second to answer it.
    before, after1, after2 = level_at(p1 - 0.01), level_at(p1 + 1.2), level_at(p2 + 1.2)
    got = f'{before} -> {after1} -> {after2}'
    if expect == 'none':
        ok = not summary.get('has_backlight') and not changes
        want = 'no backlight, no change'
        got = f"has_backlight={summary.get('has_backlight')}, {len(changes)} change(s)"
    elif expect == 'onoff':
        ok = summary.get('has_backlight') and (before, after1, after2) == (0, 100, 0)
        want = '0 -> 100 -> 0'
    else:
        want_levels = tuple(int(x) for x in expect.split(':')[1].split(','))
        ok = summary.get('has_backlight') and (before, after1, after2) == want_levels
        want = ' -> '.join(map(str, want_levels))
    print(f"{'PASS' if ok else 'FAIL'} {dev}: {got}" + ('' if ok else f'  (want {want})'))
    if not ok: fail = 2
sys.exit(fail)
PY
status=$?
if [ "$status" = 0 ]; then echo "=== backlight: PASS ==="; else echo "=== backlight: FAIL ==="; fi
exit "$status"
