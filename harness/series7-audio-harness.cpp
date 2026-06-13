// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Native audio harness for the Psion Series 7 (SA-1100 + MCP/UCB1200).
//
// Unlike the netBook (SA-1111 SAC, loaded via a CF bootloader handoff), the
// Series 7 boots its ROM directly to the EPOC desktop, and its audio path is
// the SA-1100 on-die MCP (MCCR0/MCDR0/MCSR) wired to a UCB1200 codec.
//
// Build:  bash harness/build.sh
// Usage:  ./series7-audio-harness <series7-rom.bin>
//
// What it does:
//   - Boots the Series 7 ROM to the desktop.
//   - Pumps a 440 Hz mic tone and reports whether the kernel drained MCDR0.
//   - Screenshots boot + record stages for inspection.
//
// Exit codes mirror the netBook harness where sensible:
//   0 ok (mic reads seen)   2 boot fail   3 codec never enabled
//   5 codec/route present but no mic reads

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/sa1100.h"
#include <algorithm>
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
static std::vector<std::string> g_logs;
static void capture(const char *s) {
    g_logs.emplace_back(s);
    if (std::strstr(s, "abort") || std::strstr(s, "prefetch error"))
        std::fprintf(stderr, "    [CRASH-LOG] %s\n", s);
    if (std::strstr(s, "REC-TRACE") || std::strstr(s, "AUDIO MCCR0") ||
        std::strstr(s, "AUDIO MCSR") || std::strstr(s, "[ssp]") ||
        std::strstr(s, "AUDIO SSDR"))
        std::fprintf(stderr, "    %s\n", s);
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
    for (int i = 0; i < frames; ++i)
        g_emu->executeUntil(g_emu->currentCycles() + frameCycles);
}

static void tap(int x, int y, double holdSec = 0.12) {
    std::fprintf(stderr, "    tap(%d, %d)\n", x, y);
    g_emu->updateTouchInput(x, y, true);
    runFor(holdSec);
    g_emu->updateTouchInput(x, y, false);
    runFor(0.5);
}

static void savePGM(const char *path) {
    int w = g_emu->getLCDWidth(), h = g_emu->getLCDHeight();
    std::vector<uint8_t> pixels(w * h * 4);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++) lines[y] = pixels.data() + y * w * 4;
    g_emu->readLCDIntoBuffer(lines.data(), true);
    std::vector<uint8_t> gray(w * h);
    for (int i = 0; i < w * h; i++) gray[i] = pixels[i * 4];
    FILE *f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(gray.data(), 1, gray.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "    savePGM: wrote %s (%dx%d)\n", path, w, h);
}

struct Snapshot {
    uint64_t codrReads, codrWrites;
    size_t   dacFill, adcFill;
    uint32_t mccr0;
    uint16_t ac0, ac1;
};
static Snapshot snap() {
    Snapshot s{};
    if (!g_sa) return s;
    s.codrReads  = g_sa->debugCodrReads();
    s.codrWrites = g_sa->debugCodrWrites();
    s.dacFill    = g_sa->debugDacFill();
    s.adcFill    = g_sa->debugAdcFill();
    s.mccr0      = g_sa->debugMccr0();
    s.ac0        = g_sa->debugUcbAudioCtrl0();
    s.ac1        = g_sa->debugUcbAudioCtrl1();
    return s;
}
static void dumpSnap(const char *label, const Snapshot &s) {
    std::fprintf(stderr,
        "[%s] codrR=%llu codrW=%llu dacFill=%zu adcFill=%zu mccr0=%08x ac0=%04x ac1=%04x\n",
        label, (unsigned long long)s.codrReads, (unsigned long long)s.codrWrites,
        s.dacFill, s.adcFill, s.mccr0, s.ac0, s.ac1);
}

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <series7-rom.bin>\n", argv[0]); return 1; }
    const char *romPath = argv[1];
    auto rom = readFile(romPath);
    uint32_t variant = detectROMVariant(rom.data(), rom.size());
    const DeviceProfile *profile = findProfileByVariant(variant);
    if (!profile) {
        for (const DeviceProfile *p = allProfiles(); p->id; ++p)
            if (p->createEmulator && p->expectedRomSize == rom.size() &&
                std::strcmp(p->id, "series7") == 0) { profile = p; break; }
    }
    if (!profile || !profile->createEmulator) { std::fprintf(stderr, "no profile for %s\n", romPath); return 2; }
    std::fprintf(stderr, "=== profile: %s ===\n", profile->displayName);

    g_emu = profile->createEmulator();
    g_sa  = dynamic_cast<SA1100::Emulator*>(g_emu);
    g_emu->setLogger(capture);
    g_emu->setLoggingEnabled(true);
    g_emu->loadROM(rom.data(), rom.size());
    if (!g_emu->hasAudio()) { std::fprintf(stderr, "FAIL: hasAudio()=false\n"); return 2; }

    const int bootSecs = std::getenv("BOOT_SECONDS") ? std::atoi(std::getenv("BOOT_SECONDS")) : 60;
    std::fprintf(stderr, "=== Boot Series 7 to desktop (%d s, speaker on) ===\n", bootSecs);
    g_emu->setHostAudioEnabled(true, false);
    Snapshot before = snap();
    dumpSnap("boot-start", before);
    for (int i = 0; i < bootSecs; i++) {
        runFor(1.0);
        if ((i % 10) == 9) { Snapshot s = snap(); char l[16]; std::snprintf(l, sizeof(l), "boot+%ds", i + 1); dumpSnap(l, s); }
    }
    savePGM("series7_desktop.pgm");
    Snapshot afterBoot = snap();
    bool codecEnabled = (afterBoot.mccr0 & (1u << 16)) != 0;
    std::fprintf(stderr, "   MCE (MCCR0.16): %s (mccr0=%08x)\n", codecEnabled ? "YES" : "no", afterBoot.mccr0);

    // UI exploration: tap a sequence of coords (env EXPLORE_TAPS="x:y,x:y,..."),
    // screenshotting after each, to locate the Record-app launch path.
    if (const char *taps = std::getenv("EXPLORE_TAPS")) {
        std::string s(taps); size_t pos = 0; int n = 0;
        while (pos < s.size()) {
            size_t comma = s.find(',', pos);
            std::string one = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            size_t colon = one.find(':');
            if (colon != std::string::npos) {
                int x = std::atoi(one.substr(0, colon).c_str());
                int y = std::atoi(one.substr(colon + 1).c_str());
                tap(x, y);
                runFor(1.5);
                char p[40]; std::snprintf(p, sizeof(p), "series7_explore%d.pgm", n);
                savePGM(p);
            }
            if (comma == std::string::npos) break;
            pos = comma + 1; n++;
        }
    }
    // Keyboard exploration: press a sequence of EpocKey codes
    // (env EXPLORE_KEYS="2,15,15"), screenshotting after each.
    if (const char *keys = std::getenv("EXPLORE_KEYS")) {
        std::string s(keys); size_t pos = 0; int n = 0;
        while (pos < s.size()) {
            size_t comma = s.find(',', pos);
            std::string one = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            int code = std::atoi(one.c_str());
            std::fprintf(stderr, "    key(%d)\n", code);
            g_emu->setKeyboardKey((EpocKey)code, true);
            runFor(0.08);
            g_emu->setKeyboardKey((EpocKey)code, false);
            runFor(0.8);
            char p[40]; std::snprintf(p, sizeof(p), "series7_key%d.pgm", n);
            savePGM(p);
            if (comma == std::string::npos) break;
            pos = comma + 1; n++;
        }
    }

    auto pressKey = [&](int code, double hold = 0.08, double after = 0.7) {
        g_emu->setKeyboardKey((EpocKey)code, true);  runFor(hold);
        g_emu->setKeyboardKey((EpocKey)code, false); runFor(after);
    };

    // ── Record-app end-to-end test ──────────────────────────────────────
    // Launch the Record app via New file → Program=Record → OK, then start
    // recording and pump a mic tone; check whether the kernel drains the SSP
    // RX FIFO (audio_.codrReads, incremented by popAdcSample on SSDR reads).
    if (std::getenv("RECORD_TEST")) {
        std::fprintf(stderr, "\n=== Launch Record app ===\n");
        tap(595, 215);              // New file
        runFor(1.0);
        pressKey(17);               // Down → focus Program choice
        for (int i = 0; i < 6; i++) pressKey(15);   // Right ×6 → Record
        pressKey(3); runFor(0.5);   // Enter → confirm choice
        pressKey(3); runFor(2.0);   // Enter → OK (create + launch)
        savePGM("series7_rec_launched.pgm");
        std::fprintf(stderr, "    after launch: sscr0=%04x sscr1=%04x\n",
                     g_sa->debugSscr0(), g_sa->debugSscr1());
        // Pump mic briefly right after launch, in case the open's
        // record-start left the SSP enabled (recording active at launch).
        // The Esdrv open's record-start arms recording AT LAUNCH (the codec
        // is configured SSE+RIE and stays so).  So capture happens now — pump
        // the mic and measure the SSP drain rate.  (Tapping REC runs the STOP
        // path and tears the SSP down, so we do NOT tap it here.)
        std::fprintf(stderr, "=== Recording active at launch — pump mic ===\n");
        g_emu->setHostAudioEnabled(true, true);
        Snapshot beforeRec = snap();
        const int recSecs = std::getenv("REC_SECONDS") ? std::atoi(std::getenv("REC_SECONDS")) : 6;
        double rp = 0.0, rstep = 2.0 * 3.14159265358979323846 * 440.0 / g_emu->getAudioSampleRate();
        int rchunk = g_emu->getAudioSampleRate() / 50;
        std::vector<int16_t> rc(rchunk);
        for (int s = 0; s < recSecs * 50; ++s) {
            for (int i = 0; i < rchunk; ++i) { rc[i] = (int16_t)(std::sin(rp) * 16000.0); rp += rstep; if (rp > 6.283185307) rp -= 6.283185307; }
            g_emu->writeAudioInput(rc.data(), rc.size());
            runFor(0.02);
            if (s > 0 && s % 50 == 0) {
                Snapshot m = snap();
                std::fprintf(stderr, "    [rec +%ds] sspReads=%llu adcFill=%zu sscr0=%04x sscr1=%04x icmr=%08x icpr=%08x (ssp19=%d eiger11=%d)\n",
                             s / 50, (unsigned long long)m.codrReads, m.adcFill,
                             g_sa ? g_sa->debugSscr0() : 0, g_sa ? g_sa->debugSscr1() : 0,
                             g_sa ? g_sa->debugIcmr() : 0, g_sa ? g_sa->debugIcpr() : 0,
                             g_sa ? ((g_sa->debugIcmr() >> 19) & 1) : 0,
                             g_sa ? ((g_sa->debugIcmr() >> 11) & 1) : 0);
            }
        }
        Snapshot afterRec = snap();
        savePGM("series7_rec_recording.pgm");
        uint64_t reads = afterRec.codrReads - beforeRec.codrReads;
        std::fprintf(stderr, "\n=== Record test summary ===\n   SSP mic reads during record: %llu (%.0f/s)\n",
                     (unsigned long long)reads, (double)reads / recSecs);
        std::fprintf(stderr, "   %s\n", reads > 0 ? "PASS: kernel drained SSP mic FIFO" : "FAIL: no SSP mic reads");
        std::fprintf(stderr, "   SSDR total reads=%llu silent(ring-empty)=%llu  (valid=codrReads above)\n",
                     (unsigned long long)g_sa->debugS7SsdrTotal(),
                     (unsigned long long)g_sa->debugS7SsdrSilent());

        // ── REC-button start/stop cycle (REC_CYCLE=1) ─────────────────────
        // The default RECORD_TEST records from launch and never taps the REC
        // icon — but the user's crash comes from driving the transport: tap
        // STOP, then tap REC to start a *fresh* recording.  That start path
        // tears down + re-arms the SSP/codec and re-opens the clip file, which
        // the launch-armed path skips.  Reproduce it here and keep pumping mic,
        // watching for the post-reschedule fault dump.
        if (std::getenv("REC_CYCLE")) {
            std::fprintf(stderr, "\n=== REC-button cycle: STOP then REC (fresh record) ===\n");
            tap(238, 305); runFor(1.0);   // STOP
            tap(417, 305); runFor(1.5);   // REC → start a new recording
            std::fprintf(stderr, "    after REC tap: sscr0=%04x sscr1=%04x\n",
                         g_sa->debugSscr0(), g_sa->debugSscr1());
            for (int s = 0; s < recSecs * 50; ++s) {
                for (int i = 0; i < rchunk; ++i) { rc[i] = (int16_t)(std::sin(rp) * 16000.0); rp += rstep; if (rp > 6.283185307) rp -= 6.283185307; }
                g_emu->writeAudioInput(rc.data(), rc.size());
                runFor(0.02);
                if (s > 0 && s % 50 == 0) {
                    Snapshot m = snap();
                    std::fprintf(stderr, "    [cycle-rec +%ds] sspReads=%llu adcFill=%zu sscr0=%04x sscr1=%04x icpr=%08x\n",
                                 s / 50, (unsigned long long)m.codrReads, m.adcFill,
                                 g_sa->debugSscr0(), g_sa->debugSscr1(), g_sa->debugIcpr());
                }
            }
            std::fprintf(stderr, "=== REC-button cycle done (no fault => not the start/stop path) ===\n");
        }

        // ── Playback: STOP, then PLAY the just-recorded clip ──────────────
        // Transport icons (640x480): REW ~188, STOP ~238, PLAY ~327, REC ~417
        // at y~305.  Check whether the kernel clocks the clip out the SAC TX
        // (codrWrites / dacFill grow).
        std::fprintf(stderr, "\n=== STOP, then PLAY ===\n");
        tap(238, 305);              // STOP
        runFor(1.5);
        savePGM("series7_stopped.pgm");
        std::fprintf(stderr, "    after STOP: sscr0=%04x sscr1=%04x\n",
                     g_sa->debugSscr0(), g_sa->debugSscr1());
        tap(188, 305);              // REW to clip start
        runFor(0.5);
        Snapshot beforePlay = snap();
        tap(327, 305);              // PLAY
        runFor(0.5);
        std::fprintf(stderr, "    after PLAY tap: sscr0=%04x sscr1=%04x\n",
                     g_sa->debugSscr0(), g_sa->debugSscr1());
        const int playSecs = std::getenv("PLAY_SECONDS") ? std::atoi(std::getenv("PLAY_SECONDS")) : 6;
        for (int s = 0; s < playSecs; ++s) {
            runFor(1.0);
            Snapshot m = snap();
            // The harness fed a 440 Hz tone during record; the played-back DAC
            // tone should round-trip to ~440 Hz.  Heavy sample loss/corruption
            // (e.g. mic-ring overflow drops) skews the zero-crossing estimate.
            int hz = g_sa->debugDacFreqHz();
            std::fprintf(stderr, "    [play +%ds] codrWrites=%llu dacFill=%zu sscr0=%04x sscr1=%04x toneHz~=%d\n",
                         s + 1, (unsigned long long)m.codrWrites, m.dacFill,
                         g_sa->debugSscr0(), g_sa->debugSscr1(), hz);
        }
        Snapshot afterPlay = snap();
        savePGM("series7_played.pgm");
        uint64_t txWrites = afterPlay.codrWrites - beforePlay.codrWrites;
        std::fprintf(stderr, "\n=== Playback summary ===\n   SAC TX writes during play: %llu (%.0f/s)\n",
                     (unsigned long long)txWrites, (double)txWrites / playSecs);
        std::fprintf(stderr, "   %s\n", txWrites > 0 ? "PLAY-PASS: kernel clocked clip to SAC TX" : "PLAY-FAIL: no SAC TX writes");
        return (reads > 0 && txWrites > 0) ? 0 : 7;
    }

    // Pump a 440 Hz mic tone and see if the kernel drains MCDR0.
    std::fprintf(stderr, "\n=== Pump 440 Hz mic tone (3 s) ===\n");
    g_emu->setHostAudioEnabled(true, true);
    const int sr = g_emu->getAudioSampleRate();
    const int chunkSamples = sr / 50;
    std::vector<int16_t> chunk(chunkSamples);
    double phase = 0.0, step = 2.0 * 3.14159265358979323846 * 440.0 / sr;
    Snapshot beforeMic = snap();
    for (int s = 0; s < 150; ++s) {
        for (int i = 0; i < chunkSamples; ++i) {
            chunk[i] = (int16_t)(std::sin(phase) * 16000.0);
            phase += step; if (phase > 2 * 3.14159265358979323846) phase -= 2 * 3.14159265358979323846;
        }
        g_emu->writeAudioInput(chunk.data(), chunk.size());
        runFor(0.02);
    }
    Snapshot afterMic = snap();
    std::fprintf(stderr, "   MCDR0 reads during mic pump: %llu  (adcFill=%zu)\n",
                 (unsigned long long)(afterMic.codrReads - beforeMic.codrReads), afterMic.adcFill);
    dumpSnap("after-mic", afterMic);

    bool sawMicReads = afterMic.codrReads > beforeMic.codrReads;
    if (!codecEnabled) { std::fprintf(stderr, "FAIL(3): MCE never set — codec not enabled by kernel during boot.\n"); return 3; }
    if (!sawMicReads)  { std::fprintf(stderr, "PARTIAL(5): codec enabled but no MCDR0 mic reads (no record app driven yet).\n"); return 5; }
    std::fprintf(stderr, "OK: MCDR0 mic reads seen.\n");
    return 0;
}
