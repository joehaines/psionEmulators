// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Windermere (5mx, MC218, 5mxPro, Revo) audio probe — drives a touch tap
// and checks whether the EPOC R5 kernel emits any audible activity through
// the Windermere BZCONT buzzer register (codec at 0xA00 isn't involved on
// the click path).
//
// Build via harness/build.sh; run as:
//   harness/windermere-audio-test <device-id> <rom>
//
// Devices: 5mx | mc218 | 5mxpro | revo
//
// Exit 0 = tap produced non-zero audio samples on the DAC ring; 1 = silence.
//
// Companion to harness/series5-audio-test.cpp — same shape, different chip.

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/windermere.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static EmuBase *g_emu = nullptr;
static bool g_verbose = false;
static std::vector<std::string> g_logs;

static void capture(const char *s) {
    g_logs.emplace_back(s);
    if (g_verbose) std::fprintf(stderr, "[core] %s\n", s);
}

static bool logContains(size_t since, const char *needle) {
    for (size_t i = since; i < g_logs.size(); ++i)
        if (g_logs[i].find(needle) != std::string::npos) return true;
    return false;
}

static std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", p); std::exit(1); }
    return { std::istreambuf_iterator<char>(f), {} };
}

static void runFor(double seconds) {
    const int64_t clock = g_emu->getClockSpeed();
    const int64_t frameCycles = clock / 64;
    int frames = (int)(seconds * 64);
    for (int i = 0; i < frames; ++i) {
        g_emu->executeUntil(g_emu->currentCycles() + frameCycles);
    }
}

struct Counts { uint64_t total; uint64_t nonZero; int16_t peak; };
static Counts drainAudio(double seconds) {
    Counts c{};
    const int chunk = 1024;
    int16_t buf[chunk];
    int frames = (int)(seconds * 64);
    const int64_t clock = g_emu->getClockSpeed();
    const int64_t frameCycles = clock / 64;
    for (int f = 0; f < frames; ++f) {
        g_emu->executeUntil(g_emu->currentCycles() + frameCycles);
        size_t got;
        while ((got = g_emu->readAudioOutput(buf, chunk)) > 0) {
            for (size_t i = 0; i < got; ++i) {
                c.total++;
                if (buf[i] != 0) c.nonZero++;
                int16_t a = buf[i] >= 0 ? buf[i] : (int16_t)-buf[i];
                if (a > c.peak) c.peak = a;
            }
        }
    }
    return c;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <device-id> <rom>\n", argv[0]);
        std::fprintf(stderr, "       device-id ∈ {5mx, mc218, 5mxpro, revo}\n");
        return 2;
    }
    g_verbose = std::getenv("VERBOSE") != nullptr;

    const char *deviceId = argv[1];
    auto rom = readFile(argv[2]);
    const DeviceProfile *profile = findProfileById(deviceId);
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "no profile for device-id=%s\n", deviceId);
        return 2;
    }

    g_emu = profile->createEmulator();
    g_emu->setLogger(capture);
    g_emu->setLoggingEnabled(true);
    g_emu->loadROM(rom.data(), rom.size());

    if (!g_emu->hasAudio()) {
        std::fprintf(stderr, "%s reports hasAudio()=false; bailing\n", deviceId);
        return 2;
    }
    g_emu->setHostAudioEnabled(true, false);

    std::fprintf(stderr, "=== Booting %s for 15 s ===\n", profile->displayName);
    runFor(15.0);

    // Drain the boot-time residue; Windermere kernels write to BZCONT
    // briefly during driver init.
    std::fprintf(stderr, "=== Drain boot residue (3 s) ===\n");
    drainAudio(3.0);

    std::fprintf(stderr, "=== Pre-tap drain (1 s) ===\n");
    Counts pre = drainAudio(1.0);
    std::fprintf(stderr, "  pre: total=%llu nonZero=%llu peak=%d\n",
                 (unsigned long long)pre.total,
                 (unsigned long long)pre.nonZero, (int)pre.peak);

    // Pre-tap silence gate. Catches the same Osaris-style "manual-
    // mode buzzer pump emits continuous tone while BZTOG is held
    // high" regression on the Windermere family. Real piezo hardware
    // is silent on steady BZCONT bit 0 — only transitions click.
    // Allow a small slack for boot residue.
    const uint64_t kPreTapSilenceMax = 200;
    if (pre.nonZero > kPreTapSilenceMax) {
        std::fprintf(stderr, "FAIL %s: %llu non-zero samples in 1 s of idle — "
                              "buzzer pump is emitting continuous tone (peak %d). "
                              "Was BZCONT bit 0 (BZTOG) left high by the kernel "
                              "in BZMOD=0 mode?\n",
                     deviceId,
                     (unsigned long long)pre.nonZero, (int)pre.peak);
        return 1;
    }

    // Tap an arrow / hot-key sequence the way scripts/test-revo-audio.mjs
    // does. EPOC's system click is keyed off keyboard key events when
    // system-click is enabled (the default on first-boot ROMs); a quick
    // burst of taps should surface a steady stream of BZCONT clicks.
    std::fprintf(stderr, "=== Tapping keys 9..16 ===\n");
    for (int k = 9; k <= 16; ++k) {
        g_emu->setKeyboardKey((EpocKey)k, true);
        runFor(0.05);
        g_emu->setKeyboardKey((EpocKey)k, false);
        runFor(0.05);
    }

    std::fprintf(stderr, "=== Post-tap drain (3 s) ===\n");
    Counts post = drainAudio(3.0);
    std::fprintf(stderr, "  post: total=%llu nonZero=%llu peak=%d\n",
                 (unsigned long long)post.total,
                 (unsigned long long)post.nonZero, (int)post.peak);

    // Click expectation. All four Windermere devices (5mx, MC218,
    // Revo, 5mx Pro) produce audible clicks on BZTOG transitions —
    // real piezo hardware fires its mechanical/capacitive envelope
    // on any pin transition, independent of TC1 state. The 5mx Pro
    // previously had a TC1::ENABLED gate (commit 5a7ab547) that
    // suppressed legitimate clicks as a workaround for a CF-mount
    // stall; that stall has since been fixed at its real root
    // cause in the CF accel path (3df2affa loadROM pre-arm +
    // 47f1e812 DRAM rescan safety net), so the gate is gone and
    // 5mx Pro clicks the same as its siblings.
    bool clickPass = post.nonZero > pre.nonZero + 8;
    if (clickPass) {
        std::fprintf(stderr, "PASS %s click: %llu non-zero samples "
                              "emitted after tap (peak %d)\n",
                     deviceId,
                     (unsigned long long)(post.nonZero - pre.nonZero),
                     (int)post.peak);
    } else {
        std::fprintf(stderr, "FAIL %s: tap produced no audible activity\n",
                     deviceId);
    }

    // ── Voice-Notes-open crash gate ───────────────────────────────────────
    // The 5mx Pro REC-tap regression that prompted this test fired
    // during the codec init that runs the instant the user taps REC —
    // the kernel writes CONFG=3, our CSINT-fallback path (incorrectly
    // gated on device identity rather than on the sawBootloaderPseudoDfc
    // runtime flag) hammered CSINT into a half-set-up channel struct
    // and the kernel rebooted back to splash. The crash reproduces
    // just by opening Voice Notes (the same CONFG=3 path runs in
    // warmup), so a "press key 158, wait, assert no reboot" probe is
    // enough to catch the regression in CI without the brittleness of
    // synthesising the actual REC-button touch.
    //
    // Detection signature: the kernel's self-restart resets CP15 to 0
    // before re-running its vector init. That write doesn't appear in
    // normal post-boot operation, so its presence after a logMark is
    // a reliable crash indicator. We also catch the classic CPU
    // exception paths in case some future regression takes a
    // different route to the same outcome.
    auto kernelRebootedSince = [](size_t since) {
        return logContains(since, "setting cp15_control to 00000000") ||
               logContains(since, "prefetch error") ||
               logContains(since, "data abort");
    };

    // Voice Notes open + warmup. The 5mxPro REC-tap regression that
    // prompted this test crashed during the codec init that runs
    // when CONFG=3 is first written; the same path runs on the very
    // first key 158 press as part of FUN_5009fad8's open/warmup. So
    // a "press key 158, wait, assert no reboot" probe with the host
    // mic disabled is a reliable gate for the codec init crashes
    // without the brittleness of synthesising the actual REC-button
    // touch (which on Windermere takes different kernel paths
    // depending on whether the user opened via touch vs key, and
    // whether the app is being opened for the first time vs
    // re-entered). Mic-enabled paths exercise additional kernel
    // state that the harness can't reliably reproduce — leave them
    // for the in-browser test rig.
    bool recGateOk = true;
    std::fprintf(stderr, "\n=== Voice-Notes open crash gate (mic off) ===\n");
    g_emu->setHostAudioEnabled(true, false);
    size_t logMark = g_logs.size();
    std::fprintf(stderr, "  pressing key 158\n");
    g_emu->setKeyboardKey((EpocKey)158, true);
    runFor(0.1);
    g_emu->setKeyboardKey((EpocKey)158, false);
    runFor(3.0);
    if (kernelRebootedSince(logMark)) {
        std::fprintf(stderr, "FAIL %s: kernel rebooted opening Voice Notes\n",
                     deviceId);
        recGateOk = false;
    } else {
        std::fprintf(stderr, "PASS %s vn-open: no reboot\n", deviceId);
    }

    if (!recGateOk) return 2;
    return clickPass ? 0 : 1;
}
