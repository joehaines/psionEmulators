// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Series 5 audio probe — drives an icon tap and checks whether the EPOC R1
// kernel emits any audible activity through the CL-PS7110 buzzer pin
// (SYSCON1 bits 9/10 = BZTOG/BZMOD).
//
// Build via harness/build.sh; run as:
//   harness/series5-audio-test roms/series5_v1.01\(144\)_eng.bin
//
// Exit 0 = touch produced non-zero audio samples on dacQueue; 1 = silence.

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

static EmuBase *g_emu = nullptr;
static bool g_verbose = false;

static void capture(const char *s) {
    if (g_verbose) std::fprintf(stderr, "[core] %s\n", s);
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

// Count non-zero int16 samples currently sitting in the device's DAC queue.
// Drains in 1024-sample chunks so a sustained tone shows up.
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
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <rom>\n", argv[0]);
        return 2;
    }
    g_verbose = std::getenv("VERBOSE") != nullptr;

    auto rom = readFile(argv[1]);
    uint32_t variant = detectROMVariant(rom.data(), rom.size());
    const DeviceProfile *profile = findProfileByVariant(variant);
    if (!profile) {
        for (const DeviceProfile *p = allProfiles(); p->id; ++p) {
            if (p->romVariantId == 0 && p->createEmulator &&
                p->expectedRomSize == rom.size()) { profile = p; break; }
        }
    }
    // Allow forcing the Series 5 profile since this test only makes sense
    // for the CL-PS7110/7111 buzzer-pin driver.
    if (!profile || std::getenv("PSION_DEVICE_OVERRIDE_SERIES5")) {
        profile = findProfileById("series5");
    }
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "no profile for ROM\n");
        return 2;
    }

    g_emu = profile->createEmulator();
    g_emu->setLogger(capture);
    g_emu->setLoggingEnabled(true);
    g_emu->loadROM(rom.data(), rom.size());

    if (!g_emu->hasAudio()) {
        std::fprintf(stderr, "device reports hasAudio()=false; bailing\n");
        return 2;
    }
    // Mark the host speaker as enabled so readAudioOutput surfaces real
    // sample values rather than zeros.
    g_emu->setHostAudioEnabled(true, false);

    std::fprintf(stderr, "=== Booting %s for 15 s ===\n", profile->displayName);
    runFor(15.0);

    // Drain anything left over from the boot phase first; the kernel
    // briefly enables BZMOD=1 with TC1 loaded during early init, which
    // pumps a few hundred ms of tone into the queue before going idle.
    // We want a clean baseline before measuring the tap-induced click.
    std::fprintf(stderr, "=== Drain boot residue (3 s) ===\n");
    drainAudio(3.0);

    std::fprintf(stderr, "=== Pre-tap drain (1 s) ===\n");
    Counts pre = drainAudio(1.0);
    std::fprintf(stderr, "  pre: total=%llu nonZero=%llu peak=%d\n",
                 (unsigned long long)pre.total,
                 (unsigned long long)pre.nonZero, (int)pre.peak);

    // Pre-tap silence gate. Catches the Osaris/CL-PS7111 "constant
    // beeping when speaker enabled" regression: the kernel sets
    // BZTOG=1 BZMOD=0 once at boot and never clears it, and if the
    // manual-mode buzzer pump treats steady BZTOG=1 as audible we
    // emit a continuous 3 kHz tone for the rest of the session.
    // Real piezo hardware in BZMOD=0 mode only sounds on transitions,
    // not while the pin is held high. Allow a small slack for legit
    // boot residue (e.g. a tail click from late driver init).
    const uint64_t kPreTapSilenceMax = 200;
    if (pre.nonZero > kPreTapSilenceMax) {
        std::fprintf(stderr, "FAIL: %llu non-zero samples in 1 s of idle — "
                              "manual-mode buzzer pump is emitting continuous "
                              "tone (peak %d). Was BZTOG=1 BZMOD=0 left high "
                              "by the kernel?\n",
                     (unsigned long long)pre.nonZero, (int)pre.peak);
        return 1;
    }

    std::fprintf(stderr, "=== Simulating icon tap at (320,160) ===\n");
    g_emu->updateTouchInput(320, 160, true);
    runFor(0.05);
    g_emu->updateTouchInput(0, 0, false);
    runFor(0.05);

    std::fprintf(stderr, "=== Post-tap drain (2 s) ===\n");
    Counts post = drainAudio(2.0);
    std::fprintf(stderr, "  post: total=%llu nonZero=%llu peak=%d\n",
                 (unsigned long long)post.total,
                 (unsigned long long)post.nonZero, (int)post.peak);

    if (post.nonZero > pre.nonZero + 8) {
        std::fprintf(stderr, "PASS: %llu non-zero samples emitted after tap "
                              "(peak %d) — buzzer is reaching the host speaker\n",
                     (unsigned long long)(post.nonZero - pre.nonZero),
                     (int)post.peak);
        return 0;
    }
    std::fprintf(stderr, "FAIL: tap produced no audible buzzer activity\n");
    return 1;
}
