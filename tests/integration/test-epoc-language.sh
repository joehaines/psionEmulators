#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# ROM language on the EPOC R5 machines: Series 5, 5mx, 5mx Pro, MC218, Revo
# and Conan.
#
# Each of these ROMs carries more than one locale DLL (ELocl.dll, ELocl1.dll
# …). The Series 5 and the 5mx family pick one at boot from the factory
# settings PROM (bits 18..20 of its first word), which the emulator programs;
# on the Revo board the emulator edits the ROM's directory so the DLL the
# machine loads by default is the chosen one (see the locale notes in
# core/windermere.cpp). Either way the kernel keeps the locale it installs —
# country code, UTC offset, date and time formats — in RAM.
# docs/rom-audit.md has the chain.
#
# The locales are English with different formats (UK, Scandinavian, USA, …),
# so the desktop looks the same in all of them and a screenshot proves
# nothing. Instead the test walks the ROM's own file system, takes the
# TLocale record out of every ELocl<n>.dll in it, boots the machine with
# --language N, and checks that the record of the DLL that entry names is
# live in RAM and that no other ELocl's is.
#
#   bash tests/integration/test-epoc-language.sh
#
# Needs harness/run (bash harness/build.sh) and python3.

set -u
REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
HARNESS=$REPO/harness/run
TMP=$(mktemp -d /tmp/epoclang.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

if [ ! -x "$HARNESS" ]; then
    echo "FAIL: $HARNESS not built — run bash harness/build.sh first"
    exit 1
fi

# device | ROM | boot seconds | --language | what that entry boots: an ELocl
# DLL whose TLocale record must be the only one live in RAM, or
# "text:WANT!NOT" — a UTF-16 string that must be in RAM and one that must not
# (the Conan's locales are told apart by their month names).
CASES=(
    "series5|series5_v1.01(144)_eng.bin|40|1|ELocl1.dll"
    "5mx|5mx_v1.05(260)_eng.bin|40|0|ELocl.dll"
    "5mx|5mx_v1.05(260)_eng.bin|40|1|ELocl1.dll"
    "mc218|MC218_v1.05(259)_eng.bin|40|1|ELocl1.dll"
    "mc218|MC218_v1.05(259)_eng.bin|40|2|ELocl2.dll"
    "mc218|MC218_v1.05(259)_eng.bin|40|4|ELocl4.dll"
    "5mxpro|5mxPRO_v1.05(319)_patch_eng.bin|40|2|ELocl2.dll"
    "revo|Revo_v1.06(390)_eng.bin|40|0|ELocl.dll"
    "revo|Revo_v1.06(390)_eng.bin|40|1|ELocl1.dll"
    "revo|Revo_v1.06(390)_eng.bin|40|2|Elocl2.dll"
    "conan|conan_v0.10(17)_eng.IMG|90|1|text:Janvier!January"
    "conan|conan_v0.10(17)_eng.IMG|90|2|text:Januar!January"
)

run_case() {
    local dev=$1 rom=$2 secs=$3 lang=$4 tag=$5
    PSION_RTC_SEED=1000000000 "$HARNESS" "$REPO/roms/$rom" \
        --device "$dev" --skip-card \
        --boot-seconds "$secs" --post-attach-seconds 2 \
        --language "$lang" \
        --save-ram-snapshot "$TMP/$tag.bin" \
        > "$TMP/$tag.log" 2>&1
}

i=0
for c in "${CASES[@]}"; do
    IFS='|' read -r dev rom secs lang want <<< "$c"
    echo "=== booting $dev with language $lang (expects $want) ==="
    run_case "$dev" "$rom" "$secs" "$lang" "case$i" &
    i=$((i + 1))
    # Three at a time: each boot is single-threaded.
    if (( i % 3 == 0 )); then wait; fi
done
wait

python3 - "$REPO" "$TMP" "${CASES[@]}" <<'PY'
import os, re, struct, sys

repo, tmp, cases = sys.argv[1], sys.argv[2], sys.argv[3:]


def rom_files(rom):
    """Every file in an EPOC R1/R5 ROM image: {name: bytes}. Handles the
    8-bit-name builds with a root-directory list (5mx family, Revo) and
    the Series 5's direct root."""
    base, root = struct.unpack_from('<I', rom, 0x8c)[0], struct.unpack_from('<I', rom, 0x94)[0]
    off = lambda a: a - base
    n = struct.unpack_from('<I', rom, off(root))[0]
    if 1 <= n <= 8:
        root = struct.unpack_from('<I', rom, off(root) + 8)[0]
    files = {}

    def walk(addr, depth):
        p = off(addr)
        size = struct.unpack_from('<I', rom, p)[0]
        q, end = p + 4, p + 4 + size
        while q < end:
            esz, ea, att, nl = struct.unpack_from('<IIBB', rom, q)
            name = rom[q + 10:q + 10 + nl].decode('latin1')
            if att & 0x10:
                if depth < 8:
                    walk(ea, depth + 1)
            else:
                files[name] = rom[off(ea):off(ea) + esz]
            q = (q + 10 + nl + 3) & ~3

    walk(root, 0)
    return files


COUNTRIES = {1, 31, 33, 34, 36, 39, 44, 46, 49, 351}


def locale_record(dll):
    """The TLocale block an ELocl DLL installs, from its country code on:
    the first [language, country, UTC offset, date format, time format]
    run that looks like one (see core/windermere.cpp's locale notes)."""
    for i in range(0, len(dll) - 40, 4):
        lang, cc, utc, df, tf = struct.unpack_from('<5i', dll, i)
        if 1 <= lang <= 40 and cc in COUNTRIES and -14 <= utc <= 14 \
                and 0 <= df <= 2 and 0 <= tf <= 1:
            return dll[i + 4:i + 40]
    return None


failed = False
for idx, case in enumerate(cases):
    dev, rom_name, _, lang, want = case.split('|')
    tag = f'case{idx}'
    snap = os.path.join(tmp, f'{tag}.bin')
    log = open(os.path.join(tmp, f'{tag}.log'), errors='replace').read()
    label = f'{dev} language {lang}'
    if '--language' in log and 'ignored' in log:
        print(f'FAIL {label}: the harness says the device has no such language')
        failed = True
        continue
    if not os.path.exists(snap) or os.path.getsize(snap) == 0:
        print(f'FAIL {label}: no RAM snapshot — the machine did not run')
        failed = True
        continue
    ram = open(snap, 'rb').read()
    if want.startswith('text:'):
        need, avoid = want[5:].split('!')
        has = need.encode('utf-16le') in ram
        hasnt = avoid.encode('utf-16le') not in ram
        if has and hasnt:
            print(f'ok   {label}: "{need}" in RAM and "{avoid}" not')
        else:
            print(f'FAIL {label}: expected "{need}" in RAM (found={has}) and '
                  f'no "{avoid}" (absent={hasnt})')
            failed = True
        continue
    files = rom_files(open(os.path.join(repo, 'roms', rom_name), 'rb').read())
    locales = {n: locale_record(b) for n, b in files.items()
               if re.fullmatch(r'(?i)elocl\d?\.dll', n)}
    live = sorted(n for n, r in locales.items() if r and r in ram)
    if live == [want]:
        print(f'ok   {label}: {want} is the live locale')
    else:
        print(f'FAIL {label}: expected only {want} live in RAM, found {live or "none"}')
        failed = True

if failed:
    sys.exit(1)
print('PASS epoc language: every entry boots its own locale DLL')
PY
