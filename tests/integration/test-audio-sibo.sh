#!/usr/bin/env bash
# SIBO codec (speaker / microphone) validation.
#
# Tier A+B (no ROM): A-law expand/compress unit tests + chip-level
# ASIC9 codec playback/capture against a standalone PsionAsic9 +
# SiboAudio bench. Deterministic, runs everywhere.
#
# Tier C (ROM): boots the Series 3c v5.20f ROM, navigates the System
# screen to the Sound app (11 right-arrows + Enter), accepts the
# "Record new file" dialog and starts a recording with Tab while a
# 440 Hz host mic tone is fed in. Passes when the kernel drains a full
# multi-second recording's worth of codec capture bytes (the require
# threshold below) — the full wire: host mic ring -> SiboAudio ADC ->
# ASIC9 codec FIFO -> kernel CSINT drain loop -> Sound app WVE buffer.
#
# The recording used to abort ~1 s in via a watchdog NMI; that was a
# CPU bug (interrupts taken between a segment-override prefix and its
# instruction corrupted the kernel's syscall dispatcher) fixed in
# core/v30.cpp. The session now runs to its natural length and the
# Sound app saves a playable SOUND.WVE — see docs/audio-architecture.md.
#
# Usage:
#   scripts/test-audio-sibo.sh            # tiers A+B+C
#   scripts/test-audio-sibo.sh --chip     # tiers A+B only (no ROM needed)

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HARNESS="$REPO_ROOT/harness/sibo-audio-harness"
ROM="$REPO_ROOT/roms/series3c_v5.20f_eng.bin"

if [ ! -x "$HARNESS" ]; then
    echo "sibo-audio-harness not built. Running harness/build.sh..." >&2
    bash "$REPO_ROOT/harness/build.sh" >&2 || exit 1
fi

echo "=== SIBO audio: chip-level tiers (A-law + ASIC9 codec) ===" >&2
if ! "$HARNESS"; then
    echo "FAIL: chip-level codec tiers" >&2
    exit 1
fi

if [ "${1:-}" = "--chip" ]; then
    echo "PASS (chip-level only)" >&2
    exit 0
fi

if [ ! -f "$ROM" ]; then
    echo "SKIP tier C: ROM $ROM not present" >&2
    exit 0
fi

echo "=== SIBO audio: Series 3c Sound-app record flow ===" >&2
# Esc x2 dismisses cold-boot dialogs; 11 right-arrows reach the Sound
# icon (System-screen order: Data Word Agenda Time World Calc Sheet
# Jotter Spell Patience Files Sound Program); Enter opens the app
# (straight into "Record new file"); Enter accepts the dialog; Tab
# starts the recording. The mic tone starts at 50 s so the ADC ring is
# primed before the codec drains it.
if ! "$HARNESS" "$ROM" --device series3c \
    --boot-seconds 0 --run-seconds 66 --mic-tone-at 50 \
    --press-key 8 4 16 --press-key 18 4 16 \
    --press-key 28 15 32 --press-key 30 15 32 --press-key 32 15 32 \
    --press-key 34 15 32 --press-key 36 15 32 --press-key 38 15 32 \
    --press-key 40 15 32 --press-key 42 15 32 --press-key 44 15 32 \
    --press-key 46 15 32 --press-key 48 15 32 \
    --press-key 51 3 32 --press-key 54 3 32 --press-key 56 2 32 \
    --require-pcm-in 4000; then
    echo "FAIL: series3c record flow" >&2
    exit 1
fi

echo "=== SIBO audio: Series 3a Record-app flow ===" >&2
# The Series 3a v3.40f System screen reaches the Record app via 9
# right-arrows (Data Word Agenda Time World Calc Sheet Spell Patience
# Record). Enter launches it into the "Record to new file" dialog;
# Enter accepts the dialog (-> "Start recording / Space" prompt);
# Space (EpocKey 5, NOT Tab=2 — the 3a start button is Space, unlike
# the 3c's Tab) begins the 2-second capture. Mic tone primed from 52 s,
# Space at 54 s. This used to be unreachable (the System screen
# appeared to stop at Time and the record session aborted via watchdog
# NMI); both are now fixed (see docs/audio-architecture.md). Same M7702
# codec silicon as the 3c.
S3A_ROM="$REPO_ROOT/roms/series3a_v3.40f_eng.bin"
if [ -f "$S3A_ROM" ]; then
    if ! "$HARNESS" "$S3A_ROM" --device series3a \
        --boot-seconds 0 --run-seconds 62 --mic-tone-at 52 \
        --press-key 8 4 16 --press-key 18 4 16 \
        --press-key 22 15 32 --press-key 24 15 32 --press-key 26 15 32 \
        --press-key 28 15 32 --press-key 30 15 32 --press-key 32 15 32 \
        --press-key 34 15 32 --press-key 36 15 32 --press-key 38 15 32 \
        --press-key 42 3 32 --press-key 46 3 32 --press-key 54 5 32 \
        --require-pcm-in 2000; then
        echo "FAIL: series3a record flow" >&2
        exit 1
    fi
else
    echo "SKIP series3a record flow: ROM not present" >&2
fi

echo "PASS: SIBO audio (chip tiers + 3c & 3a record flows)" >&2
