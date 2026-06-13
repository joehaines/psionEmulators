// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Series 5 / Osaris mic-recording probe — exercises the CL-PS7110/7111
// codec RX path without needing a user to navigate the device's UI.
//
// We can't easily trigger the EPOC R1 / R5 recording-app code path from
// the harness (it requires keyboard / touch UI flow), so we drive the
// hardware-level signals directly:
//   1. Boot the device until kernel RAM is settled
//   2. Programmatically write SYSCON1 with CDENRX=1 to simulate a
//      kernel-initiated recording session
//   3. Push a sine-wave mic burst into writeAudioInput
//   4. Drive the emulator forward and read CODR back through the same
//      MMIO path the kernel would, asserting we see the pushed samples
//      come back out
//
// Exit 0 = round-trip succeeded; non-zero = the codec recording path is
// broken somewhere on the device.

#include "../core/emubase.h"
#include "../core/arm710.h"
#include "../core/device_registry.h"
#include "../core/clps7111.h"
#include "../core/clps7111_defs.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

constexpr auto V8  = ARM710::V8;
constexpr auto V32 = ARM710::V32;

static EmuBase *g_emu = nullptr;
static bool g_verbose = false;

static void capture(const char *s) {
    if (g_verbose) std::fprintf(stderr, "[core] %s\n", s);
}

static std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", p); std::exit(2); }
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

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <device-id> <rom>\n", argv[0]);
        return 2;
    }
    g_verbose = std::getenv("VERBOSE") != nullptr;

    const char *deviceId = argv[1];
    auto rom = readFile(argv[2]);
    const DeviceProfile *profile = findProfileById(deviceId);
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "no profile for device %s\n", deviceId);
        return 2;
    }

    g_emu = profile->createEmulator();
    g_emu->setLogger(capture);
    g_emu->setLoggingEnabled(true);
    g_emu->loadROM(rom.data(), rom.size());

    auto *clps = dynamic_cast<CLPS7111::Emulator *>(g_emu);
    if (!clps) {
        std::fprintf(stderr, "device is not a CL-PS711x — skipping\n");
        return 2;
    }

    g_emu->setHostAudioEnabled(false, true);  // mic on, speaker off
    std::fprintf(stderr, "=== Booting %s for 15 s ===\n", profile->displayName);
    runFor(15.0);

    // Step 1: drain any pre-existing audio queue noise so the assertion
    // below isn't polluted by boot residue.
    {
        int16_t buf[1024];
        size_t got;
        while ((got = g_emu->readAudioOutput(buf, 1024)) > 0) { (void)got; }
    }

    // Step 2: programmatically simulate the kernel enabling the codec RX
    // path. SYSCON1 bit 14 = CDENRX. We use writePhysical to drive the
    // MMIO write the same way the kernel would (the chip-specific
    // dispatch in CLPS7111::Emulator::writePhysical routes 0x80000100
    // to writeReg32 SYSCON1). 0x000071B0 keeps the LCD/UART/TC bits the
    // kernel had set during boot and adds the CDENRX flag.
    std::fprintf(stderr, "=== Forcing CDENRX=1 ===\n");
    bool wrote = g_emu->writePhysical(0x000071B0u | (1u << 14),
                                      0x80000100u, V32);
    if (!wrote) {
        std::fprintf(stderr, "failed to write SYSCON1\n");
        return 3;
    }
    runFor(0.1);

    // Step 3: push a known sine pattern via writeAudioInput. 16 samples
    // is enough to fill one hardware-FIFO drain cycle, well below the
    // 8 KiB ring depth.
    const int kSamples = 16;
    int16_t inputs[kSamples];
    for (int i = 0; i < kSamples; ++i) {
        double phase = 2.0 * 3.14159265358979 * (i / 16.0);
        inputs[i] = (int16_t)(std::sin(phase) * 16000.0);
    }
    g_emu->writeAudioInput(inputs, kSamples);

    // Step 4: read CODR via writePhysical-friendly readPhysical so we
    // exercise the same dispatch path the kernel would. CODR is at
    // 0x80000440 on CL-PS7110/7111.
    auto *bridge = clps->getArmCpu();
    (void)bridge;
    std::fprintf(stderr, "=== Reading back %d samples via CODR ===\n", kSamples);
    int matched = 0, total = 0, nonZero = 0;
    runFor(0.02);  // let the codec settle / CSINT fire path run
    for (int i = 0; i < kSamples; ++i) {
        auto v = clps->readPhysical(0x80000440u, V8);
        if (!v.has_value()) {
            std::fprintf(stderr, "  CODR read %d: <no value>\n", i);
            break;
        }
        uint8_t got = (uint8_t)(v.value() & 0xFF);
        int8_t  expectedByte = (int8_t)(inputs[i] >> 8);
        std::fprintf(stderr, "  CODR[%2d]: got=%4d expected=%4d\n",
                     i, (int)(int8_t)got, (int)expectedByte);
        total++;
        if (got != 0) nonZero++;
        if ((int8_t)got == expectedByte) matched++;
    }

    if (matched >= 12) {
        std::fprintf(stderr, "PASS: %d/%d CODR reads matched expected mic samples\n",
                     matched, total);
        return 0;
    }
    if (nonZero > 0) {
        std::fprintf(stderr, "PARTIAL: %d/%d non-zero (matched %d) — samples reach CODR but values diverge\n",
                     nonZero, total, matched);
        return 1;
    }
    std::fprintf(stderr, "FAIL: CODR returned all zeros — mic samples not reaching the codec FIFO\n");
    return 1;
}
