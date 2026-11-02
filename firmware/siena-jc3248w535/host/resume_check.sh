#!/usr/bin/env bash
# Saves the machine part-way through a scripted session, resumes it in a
# fresh process, and checks the result matches a straight run, at split
# points through boot, typing, saving and idle (see resume_check.cpp).
#
#   bash host/resume_check.sh <siena.rom> [app store image]
#
# Given the app store (build/app_store.bin), it also puts the pack in the
# slot and installs an app from it part-way, and checks across that.
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW="$(dirname "$HERE")"
CORE="$FW/../../core"
CXX="${CXX:-g++}"
ROM="$1"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$CXX" -O2 -std=c++17 -I"$CORE" -I"$FW/main" \
    "$HERE/resume_check.cpp" \
    "$FW/main/siena_machine.cpp" "$FW/main/key_player.cpp" \
    "$FW/main/hid_keymap.cpp" "$FW/main/terminal_keys.cpp" "$FW/main/app_store.cpp" \
    "$CORE/series3c.cpp" "$CORE/series3c_serial_bridge.cpp" "$CORE/psion_asic9.cpp" \
    "$CORE/psion_condor.cpp" "$CORE/psion_honda.cpp" "$CORE/psion_ssd.cpp" \
    "$CORE/sibo_audio.cpp" "$CORE/v30.cpp" "$CORE/v30_ops_mov.cpp" \
    "$CORE/v30_ops_arith.cpp" "$CORE/v30_ops_ctrl.cpp" "$CORE/v30_ops_misc.cpp" \
    -o "$TMP/resume_check"

# The RTC seeds from the host clock otherwise, and the two runs must agree.
export PSION_RTC_SEED=5a000000
END=45000
SPLITS="2000 22000 32000 40000"
if [ -n "$2" ]; then
    export STORE="$2" STORE_APP=CASTLE2
    END=90000
    SPLITS="2000 40000 80000"
fi
status=0
for T in $SPLITS; do
    a=$("$TMP/resume_check" straight "$ROM" $T $END "$TMP/a.state")
    "$TMP/resume_check" split "$ROM" $T "$TMP/split.bin" > /dev/null
    b=$("$TMP/resume_check" resume "$ROM" $T $END "$TMP/split.bin" "$TMP/b.state")
    if [ "$a" = "$b" ] && cmp -s "$TMP/a.state" "$TMP/b.state"; then
        echo "split at $T ms: resumed exactly ($a)"
    else
        echo "split at $T ms: DIVERGED"
        echo "  straight: $a"
        echo "  resumed:  $b"
        status=1
    fi
done
exit $status
