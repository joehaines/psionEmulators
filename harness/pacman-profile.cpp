// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Pac-Man launch + profiling harness for the Psion Series 7.
//
//   1. Boots the Series 7 ROM to the desktop.
//   2. Attaches a CF image (tests/fixtures/psion-cf.img — PacMan installed under
//      System\Apps\PacMan) and waits for the OS to mount it.
//   3. Drives the launch sequence the user supplied (digitiser coords):
//        Extras (677,458) -> PacMan icon (600,359) -> Enter -> Start (619,84).
//   4. Runs the game loop, with callgrind instrumentation toggled ON only for
//      that phase — so boot + mount run at near-native speed and the profile
//      captures the steady-state game, not the one-off boot.
//
// Build:  bash harness/build.sh   (adds ./harness/pacman-profile)
// Verify: ./harness/pacman-profile <series7-rom.bin> <cf.img>      (PGMs only)
// Profile: valgrind --tool=callgrind --instr-atstart=no \
//            --callgrind-out-file=/tmp/pacman.callgrind \
//            ./harness/pacman-profile <rom> <cf.img>
//
// Env: BOOT_SECONDS (60) MOUNT_SECONDS (35) GAME_SECONDS (6) PGM_DIR (/tmp)

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/sa1100.h"
#include <valgrind/callgrind.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static EmuBase *g_emu = nullptr;
static SA1100::Emulator *g_sa = nullptr;
static std::string g_pgmDir = "/tmp";

static std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", p); std::exit(1); }
    return { std::istreambuf_iterator<char>(f), {} };
}

static void runFor(double seconds) {
    const int64_t frameCycles = g_emu->getClockSpeed() / 64;
    int frames = (int)(seconds * 64);
    for (int i = 0; i < frames; ++i)
        g_emu->executeUntil(g_emu->currentCycles() + frameCycles);
}

static void tap(int x, int y, double holdSec = 0.12, double after = 0.6) {
    std::fprintf(stderr, "    tap(%d, %d)\n", x, y);
    g_emu->updateTouchInput(x, y, true);
    runFor(holdSec);
    g_emu->updateTouchInput(x, y, false);
    runFor(after);
}

static void pressKey(int code, double hold = 0.08, double after = 0.7) {
    g_emu->setKeyboardKey((EpocKey)code, true);  runFor(hold);
    g_emu->setKeyboardKey((EpocKey)code, false); runFor(after);
}

static void savePGM(const char *name) {
    int w = g_emu->getLCDWidth(), h = g_emu->getLCDHeight();
    std::vector<uint8_t> pixels(w * h * 4);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++) lines[y] = pixels.data() + y * w * 4;
    g_emu->readLCDIntoBuffer(lines.data(), true);
    std::vector<uint8_t> gray(w * h);
    for (int i = 0; i < w * h; i++) gray[i] = pixels[i * 4];
    std::string path = g_pgmDir + "/" + name;
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(gray.data(), 1, gray.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "    savePGM: %s (%dx%d)\n", path.c_str(), w, h);
}

static int envInt(const char *k, int dflt) {
    const char *v = std::getenv(k);
    return v ? std::atoi(v) : dflt;
}

int main(int argc, char **argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s <series7-rom.bin> <cf.img>\n", argv[0]); return 1; }
    if (const char *d = std::getenv("PGM_DIR")) g_pgmDir = d;
    auto rom  = readFile(argv[1]);
    auto card = readFile(argv[2]);

    const DeviceProfile *profile = findProfileById("series7");
    if (!profile || !profile->createEmulator) { std::fprintf(stderr, "no series7 profile\n"); return 2; }
    g_emu = profile->createEmulator();
    g_sa  = dynamic_cast<SA1100::Emulator *>(g_emu);
    g_emu->setLoggingEnabled(false);             // keep the profile clean
    g_emu->loadROM(rom.data(), rom.size());

    const int bootSecs  = envInt("BOOT_SECONDS", 60);
    const int mountSecs = envInt("MOUNT_SECONDS", 35);
    const int gameSecs  = envInt("GAME_SECONDS", 6);

    std::fprintf(stderr, "=== Boot Series 7 to desktop (%d s) ===\n", bootSecs);
    runFor(bootSecs);
    savePGM("pac_0_desktop.pgm");

    std::fprintf(stderr, "=== Attach CF (%zu bytes) + wait %d s for mount ===\n", card.size(), mountSecs);
    bool ok = g_emu->attachCard(card.data(), card.size());
    std::fprintf(stderr, "    attachCard -> %s\n", ok ? "true" : "false");
    runFor(mountSecs);
    savePGM("pac_1_mounted.pgm");

    std::fprintf(stderr, "=== Launch sequence ===\n");
    tap(677, 458); runFor(1.5);  savePGM("pac_2_extras.pgm");   // Extras
    tap(600, 359); runFor(1.5);  savePGM("pac_3_icon.pgm");     // PacMan icon
    pressKey(EStdKeyEnter); runFor(1.0); savePGM("pac_4_reg.pgm");  // close registration
    tap(619, 84);  runFor(3.0);  savePGM("pac_5_started.pgm");  // Start
    std::fprintf(stderr, "    (PacMan should now be running)\n");

    // ── Profiled phase: steady-state game loop only ──────────────────────
    std::fprintf(stderr, "=== Profiling %d s of game loop ===\n", gameSecs);
    const int64_t c0 = g_emu->currentCycles();
    // --instr-atstart=no keeps boot/mount uninstrumented; turning instrumentation
    // ON here begins counting (collection is on by default — do NOT toggle it,
    // that switches counting OFF and yields an empty profile).
    CALLGRIND_START_INSTRUMENTATION;
    runFor(gameSecs);
    CALLGRIND_STOP_INSTRUMENTATION;
    CALLGRIND_DUMP_STATS;
    const int64_t simCycles = g_emu->currentCycles() - c0;
    savePGM("pac_6_final.pgm");
    std::fprintf(stderr, "=== Done. Profiled %lld sim-cycles over %d sim-seconds ===\n",
                 (long long)simCycles, gameSecs);
    return 0;
}
