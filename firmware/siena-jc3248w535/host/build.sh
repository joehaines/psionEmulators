#!/usr/bin/env bash
# Builds host/host_sim: the firmware's portable half (machine, keyboard
# paths, screen renderer) plus the SIBO2 core, for checking on a PC.
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW="$(dirname "$HERE")"
CORE="$FW/../../core"
CXX="${CXX:-g++}"

"$CXX" -O2 -std=c++17 -I"$CORE" -I"$FW/main" \
    "$HERE/host_sim.cpp" \
    "$FW/main/siena_machine.cpp" "$FW/main/key_player.cpp" \
    "$FW/main/hid_keymap.cpp" "$FW/main/terminal_keys.cpp" "$FW/main/screen.cpp" \
    "$FW/main/ui.cpp" "$FW/main/app_store.cpp" \
    "$CORE/series3c.cpp" "$CORE/series3c_serial_bridge.cpp" "$CORE/psion_asic9.cpp" \
    "$CORE/psion_condor.cpp" "$CORE/psion_honda.cpp" "$CORE/psion_ssd.cpp" \
    "$CORE/sibo_audio.cpp" "$CORE/v30.cpp" "$CORE/v30_ops_mov.cpp" \
    "$CORE/v30_ops_arith.cpp" "$CORE/v30_ops_ctrl.cpp" "$CORE/v30_ops_misc.cpp" \
    -o "$HERE/host_sim"
echo "Built $HERE/host_sim"
