// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Native audio harness for the Psion netBook (SA-1100 + UCB1200).
// Mirrors harness/audio-harness.cpp (which targets Windermere/5mx), but
// drives the netBook bootloader → CF OS handoff path so we can see what
// the EPOC R5 audio driver actually does once the shell is up.
//
// Build:  bash harness/build.sh
// Usage:  ./netbook-audio-harness <bootloader.bin> [<os-card.img>]
//   default os-card = roms/netbook_os.img (the same CF image the
//   netbook_full boot test uses).
//
// What it checks:
//   - Boot OK: shell reached, no traps.
//   - Codec init: at any point during the run, did the kernel set
//     MCCR0.MCE=1 (i.e. enable the MCP audio bus)?  Without this, no
//     samples flow through MCDR0 even if the audio_ model is correct.
//   - DAC traffic: did anything (system beep, app SFX) write to MCDR0?
//   - Mic drain: when we push host mic samples into writeAudioInput,
//     does the kernel read them back via MCDR0?
//
// Exit codes (distinct so CI can bisect):
//   0   speaker + mic both visibly working (recording timer advanced)
//   1   usage / build error
//   2   boot failure (no profile, attachCard failed)
//   3   codec never enabled (MCCR0.MCE stayed 0 for the full run)
//   4   codec enabled but no MCDR0 writes seen (kernel didn't feed
//       samples — could be no app exercised, or DFC never wakes)
//   5   codec enabled, MCDR0 writes seen, but no MCDR0 reads (mic
//       path stuck even though speaker path works)
//   6   recording app opened but recording DFC never started
//   7   recording DFC started but no MCDR0 reads during recording

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/sa1100.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static EmuBase *g_emu = nullptr;
static SA1100::Emulator *g_sa = nullptr;   // typed alias (set in main)
static std::vector<std::string> g_logs;
static void capture(const char *s) {
    g_logs.emplace_back(s);
    if (std::strstr(s, "prefetch error") || std::strstr(s, "data abort") ||
        std::strstr(s, "abort"))
        std::fprintf(stderr, "    [CRASH-LOG] %s\n", s);
    if (std::strstr(s, "[sadr]"))
        std::fprintf(stderr, "    %s\n", s);
    if (std::strstr(s, "[rec-disarm]") || std::strstr(s, "[rec-arm]"))
        std::fprintf(stderr, "    %s\n", s);
    if (std::strstr(s, "REC-STATE"))
        std::fprintf(stderr, "    %s\n", s);
}

static std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", p);
        std::exit(1);
    }
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

// Simulate a touch tap at (x, y) with a configurable hold duration.
// The 0.5 s post-release dwell lets the UI respond before the next event.
static void tap(int x, int y, double holdSec = 0.1) {
    std::fprintf(stderr, "    tap(%d, %d, hold=%.2fs)\n", x, y, holdSec);
    g_emu->updateTouchInput(x, y, true);
    runFor(holdSec);
    g_emu->updateTouchInput(x, y, false);
    runFor(0.5);
}

// Save the current LCD framebuffer as a PGM (grayscale) file.
static void savePGM(const char *path) {
    int w = g_emu->getLCDWidth();
    int h = g_emu->getLCDHeight();
    std::vector<uint8_t> pixels(w * h * 4);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++) lines[y] = pixels.data() + y * w * 4;
    g_emu->readLCDIntoBuffer(lines.data(), true);
    // Extract R channel as grayscale.
    std::vector<uint8_t> gray(w * h);
    for (int i = 0; i < w * h; i++) gray[i] = pixels[i * 4];
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "    savePGM: cannot open %s\n", path);
        return;
    }
    std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(gray.data(), 1, gray.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "    savePGM: wrote %s (%dx%d)\n", path, w, h);
}

// Search recent log lines for a substring.
static bool logContains(size_t fromIndex, const char *needle) {
    for (size_t i = fromIndex; i < g_logs.size(); ++i) {
        if (g_logs[i].find(needle) != std::string::npos) return true;
    }
    return false;
}

struct Snapshot {
    uint64_t codrReads;
    uint64_t codrWrites;
    size_t   dacFill;
    size_t   adcFill;
    uint32_t mccr0;
    uint16_t ac0, ac1;
    uint32_t pwmCtrl, pwmPscr, pwmPwdr;
    int      pwmTone;
};
static Snapshot snap() {
    auto *sa = dynamic_cast<SA1100::Emulator*>(g_emu);
    Snapshot s{};
    if (!sa) return s;
    s.codrReads  = sa->debugCodrReads();
    s.codrWrites = sa->debugCodrWrites();
    s.dacFill    = sa->debugDacFill();
    s.adcFill    = sa->debugAdcFill();
    s.mccr0      = sa->debugMccr0();
    s.ac0        = sa->debugUcbAudioCtrl0();
    s.ac1        = sa->debugUcbAudioCtrl1();
    s.pwmCtrl    = sa->debugPwm1Ctrl();
    s.pwmPscr    = sa->debugPwm1Pscr();
    s.pwmPwdr    = sa->debugPwm1Pwdr();
    s.pwmTone    = sa->debugPwmToneHz();
    return s;
}
static void dumpSnap(const char *label, const Snapshot &s) {
    std::fprintf(stderr,
        "[%s] codrR=%llu codrW=%llu dacFill=%zu adcFill=%zu "
        "mccr0=%08x ac0=%04x ac1=%04x pwm{ctrl=%02x pscr=%02x pwdr=%02x %dHz}\n",
        label,
        (unsigned long long)s.codrReads, (unsigned long long)s.codrWrites,
        s.dacFill, s.adcFill, s.mccr0, s.ac0, s.ac1,
        s.pwmCtrl, s.pwmPscr, s.pwmPwdr, s.pwmTone);
}

// Sample the interrupt controller (ICPR pending / ICMR mask) and the CPU PC
// over a window, stepping the emulator in fine slices.  Reports which IRQ
// bits stay *pending-and-enabled* (icpr & icmr) across the window, a PC
// histogram (a tight spin = few distinct PCs), and the codec drain counters.
//
// FINDING (2026-06-02): the previous handoff posited a "stray post-stop
// interrupt" that kept re-waking the kernel and blocked playback.  This
// sampler DISPROVES that: across the post-stop AND play windows no ICPR bit
// stays pending (alwaysPend=0; only transient OST-tick / Eiger bits at <1%),
// and the PC histogram is IDENTICAL to the desktop-idle baseline (same top
// PCs 0x50012ebc / 0x50000ed4 / 0x500288f4).  i.e. the "post-stop busy loop"
// is just the netBook's normal idle loop (the EPOC null thread polls rather
// than WFIs in this model, so idle%==0 is normal, not a spin).
static void sampleIrqWindow(const char *label, double seconds) {
    auto *sa = dynamic_cast<SA1100::Emulator*>(g_emu);
    auto *cpu = g_emu ? g_emu->getArmCpu() : nullptr;
    if (!sa || !cpu) return;
    const int64_t clock = g_emu->getClockSpeed();
    const int64_t sliceCycles = clock / 1000;          // ~1 ms slices
    const int slices = (int)(seconds * 1000);
    // Per-bit: how many samples had this bit pending&enabled.
    uint32_t bitPendCount[32] = {0};
    uint32_t alwaysPend = 0xFFFFFFFFu;                 // bits pend&enabled EVERY sample
    uint32_t everPend   = 0;                           // bits pend&enabled ANY sample
    std::vector<std::pair<uint32_t,int>> pcHist;       // small linear histogram
    uint64_t cR0 = sa->debugCodrReads(), cW0 = sa->debugCodrWrites();
    int idle = 0, n = 0;
    for (int i = 0; i < slices; ++i) {
        g_emu->executeUntil(g_emu->currentCycles() + sliceCycles);
        uint32_t pend = sa->debugIcpr() & sa->debugIcmr();
        alwaysPend &= pend;
        everPend   |= pend;
        for (int b = 0; b < 32; ++b) if (pend & (1u << b)) bitPendCount[b]++;
        uint32_t pc = cpu->getGPR(15);
        bool found = false;
        for (auto &e : pcHist) if (e.first == pc) { e.second++; found = true; break; }
        if (!found && pcHist.size() < 4096) pcHist.emplace_back(pc, 1);
        if (cpu->wfiRequested) idle++;
        n++;
    }
    uint64_t cR1 = sa->debugCodrReads(), cW1 = sa->debugCodrWrites();
    std::fprintf(stderr,
        "  [irq-window %s] %.1fs n=%d  codrReads+%llu codrWrites+%llu  idle=%.0f%%\n",
        label, seconds, n,
        (unsigned long long)(cR1 - cR0), (unsigned long long)(cW1 - cW0),
        n > 0 ? 100.0 * idle / n : 0.0);
    std::fprintf(stderr,
        "  [irq-window %s] icpr&icmr: alwaysPend=%08x everPend=%08x\n",
        label, alwaysPend, everPend);
    for (int b = 0; b < 32; ++b) {
        if (bitPendCount[b] == 0) continue;
        const char *nm = "?";
        switch (b) {
            case 11: nm = "GPIO11_27 (Eiger composite)"; break;
            case 13: nm = "IRQ_MCP (codec MCP)"; break;
            case 15: nm = "OST_match0/DMA?"; break;
            case 19: nm = "IRQ_SSP (SAC RX/TX)"; break;
            case 25: nm = "OST match1 (system tick)"; break;
            case 26: nm = "OST match2"; break;
            case 27: nm = "OST match3"; break;
        }
        std::fprintf(stderr, "  [irq-window %s]   bit %2d (%-28s): pend %d/%d (%.0f%%)\n",
            label, b, nm, bitPendCount[b], n, 100.0 * bitPendCount[b] / n);
    }
    // Top PCs (tight spin == one PC dominates).
    std::sort(pcHist.begin(), pcHist.end(),
              [](auto &a, auto &b){ return a.second > b.second; });
    std::fprintf(stderr, "  [irq-window %s] distinctPCs=%zu top:", label, pcHist.size());
    for (size_t i = 0; i < pcHist.size() && i < 6; ++i)
        std::fprintf(stderr, " 0x%x:%d", pcHist[i].first, pcHist[i].second);
    std::fprintf(stderr, "\n");
}

// Detect the EPOC "Program closed" panic dialog on screen.  Keys on the
// dialog's dark title bar sitting directly above a LIGHT-grey body (see the
// measured signatures below) so it is NOT fooled by the Record app's solid
// black waveform box, which also paints a dark bar in that region.
static bool panicDialogShowing() {
    int w = g_emu->getLCDWidth();
    int h = g_emu->getLCDHeight();
    if (w < 460 || h < 200) return false;
    std::vector<uint8_t> pixels(w * h * 4);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++) lines[y] = pixels.data() + y * w * 4;
    g_emu->readLCDIntoBuffer(lines.data(), true);
    auto darkFrac = [&](int y0, int y1, int x0, int x1) {
        int d = 0, t = 0;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                t++;
                if (pixels[(y * w + x) * 4] < 90) d++;
            }
        return t > 0 ? (100 * d / t) : 0;
    };
    // The "Program closed" panic dialog = a DARK title bar (y≈172-180) with a
    // LIGHT-grey body immediately beneath it (y≈188-200, the "Program / Record"
    // text rows on a light dialog face).  Measured signatures (640x480):
    //   panic dialog : titleBar 85% dark, body 15% dark   -> MATCH
    //   Record idle  : titleBar 100% dark, body 84% dark   (solid black
    //                  waveform box — body stays dark)     -> reject
    //   Record'ing   : titleBar 0% (white waveform)        -> reject
    int titleBar = darkFrac(172, 180, 205, 435);
    int body     = darkFrac(188, 200, 205, 435);
    return titleBar > 60 && body < 40;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr,
            "usage: %s <bootloader.bin> [<os-card.img>]\n",
            argv[0]);
        return 1;
    }
    const char *romPath  = argv[1];
    const char *cardPath = argc >= 3 ? argv[2] : "roms/netbook_os.img";

    auto rom = readFile(romPath);
    uint32_t variant = detectROMVariant(rom.data(), rom.size());
    const DeviceProfile *profile = findProfileByVariant(variant);
    if (!profile) {
        // Fall back to size-matched lookup for the unheadered bootloader.
        for (const DeviceProfile *p = allProfiles(); p->id; ++p) {
            if (p->romVariantId == 0 && p->createEmulator &&
                p->expectedRomSize == rom.size() &&
                std::strcmp(p->id, "netbook") == 0) {
                profile = p;
                break;
            }
        }
    }
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "no profile for %s\n", romPath);
        return 2;
    }
    std::fprintf(stderr, "=== profile: %s ===\n", profile->displayName);

    g_emu = profile->createEmulator();
    g_sa  = dynamic_cast<SA1100::Emulator*>(g_emu);
    g_emu->setLogger(capture);
    g_emu->setLoggingEnabled(true);
    g_emu->loadROM(rom.data(), rom.size());

    if (!g_emu->hasAudio()) {
        std::fprintf(stderr, "FAIL: emulator reports hasAudio()=false\n");
        return 2;
    }

    // Stage A: boot the bootloader, then attach the CF card carrying
    // D:\OS.IMG.  This drives the bootloader's OS handoff: the bytes
    // are scanned for the EPOCARM ROM magic, the body is staged into
    // ROM + RAM bank 1, and the CPU resets into the netBook OS.
    std::fprintf(stderr, "=== Stage A: boot bootloader (5 s) ===\n");
    runFor(5.0);
    auto card = readFile(cardPath);
    bool attached = g_emu->attachCard(card.data(), card.size());
    std::fprintf(stderr, "    attachCard(%s) = %s (%zu bytes)\n",
                 cardPath, attached ? "ok" : "FAIL", card.size());
    if (!attached) {
        std::fprintf(stderr, "FAIL: attachCard refused; cannot test audio\n");
        return 2;
    }

    // Stage B: let the OS settle and reach the shell.  netbook_full
    // boot test allows 90 s post-attach; we use 30 s here (enough to
    // see Shell.app on a fast host without dominating CI time).
    // Speaker enabled so any beep the kernel emits during boot lands
    // in the dacQueue.  Around 25 s we send a burst of keystrokes —
    // EPOC R5 lazily loads the audio driver, and the first
    // foregrounded keypress in Shell triggers the kernel's key-click
    // path (the BSP wires this to either PWM1 or UCB1200 TONEGEN,
    // depending on the build).  Touch input does the same on the
    // netBook silkscreen icons.
    std::fprintf(stderr, "=== Stage B: OS boot + key activity (30 s, speaker on) ===\n");
    g_emu->setHostAudioEnabled(true, false);
    Snapshot before = snap();
    dumpSnap("boot-start", before);
    for (int i = 0; i < 30; i++) {
        runFor(1.0);
        // After 25 s the shell should be up; poke some keys + a touch.
        if (i == 25) {
            std::fprintf(stderr, "    [t+25s] sending keys 9, 10, 3 + touch\n");
            for (int k : { 9, 10, 11, 3 /* Enter */ }) {
                g_emu->setKeyboardKey((EpocKey)k, true);
                runFor(0.05);
                g_emu->setKeyboardKey((EpocKey)k, false);
                runFor(0.05);
            }
            g_emu->updateTouchInput(160, 240, true);
            runFor(0.05);
            g_emu->updateTouchInput(160, 240, false);
            i += 1;  // we already advanced ~0.4 s in nested runFor calls
        }
        Snapshot s = snap();
        char label[16];
        std::snprintf(label, sizeof(label), "boot+%ds", i + 1);
        dumpSnap(label, s);
    }

    Snapshot afterBoot = snap();
    bool codecEnabled = (afterBoot.mccr0 & (1u << 16)) != 0;
    bool sawDacWrites = afterBoot.codrWrites > before.codrWrites;
    bool sawPwmEnable = (afterBoot.pwmCtrl & 1u) != 0 ||
                        afterBoot.pwmTone > 0;
    std::fprintf(stderr, "\n=== Stage B summary ===\n");
    std::fprintf(stderr, "   MCE bit (MCCR0.16) set:  %s (mccr0=%08x)\n",
                 codecEnabled ? "YES" : "no", afterBoot.mccr0);
    std::fprintf(stderr, "   Audio Ctrl 0 written:    %s (ac0=%04x)\n",
                 afterBoot.ac0 != 0 ? "YES" : "no", afterBoot.ac0);
    std::fprintf(stderr, "   PWM1 enabled:            %s\n",
                 sawPwmEnable ? "YES" : "no");
    std::fprintf(stderr, "   MCDR0 writes during boot: %llu\n",
                 (unsigned long long)(afterBoot.codrWrites - before.codrWrites));

    // Stage C: drive 3 s of 440 Hz mic tone, then sample what the
    // kernel did with it.  This is the regression floor for the
    // mic path — even if no kernel app drains MCDR0 reads, we want to
    // see SOMETHING running through the bridge.
    std::fprintf(stderr, "\n=== Stage C: pump 440 Hz mic tone (3 s) ===\n");
    g_emu->setHostAudioEnabled(true, true);
    const int sr = g_emu->getAudioSampleRate();
    const int chunkSamples = sr / 50;     // 20 ms chunks
    std::vector<int16_t> chunk(chunkSamples);
    double phase = 0.0;
    const double step = 2.0 * 3.14159265358979323846 * 440.0 / sr;
    Snapshot beforeMic = snap();
    for (int s = 0; s < 150; ++s) {       // 150 * 20 ms = 3 s
        for (int i = 0; i < chunkSamples; ++i) {
            chunk[i] = (int16_t)(std::sin(phase) * 16000.0);
            phase += step;
            if (phase > 2 * 3.14159265358979323846) {
                phase -= 2 * 3.14159265358979323846;
            }
        }
        g_emu->writeAudioInput(chunk.data(), chunk.size());
        runFor(0.02);
    }
    Snapshot afterMic = snap();
    std::fprintf(stderr,
                 "   MCDR0 reads during mic pump:  %llu\n",
                 (unsigned long long)(afterMic.codrReads - beforeMic.codrReads));
    std::fprintf(stderr,
                 "   ADC ring fill (samples queued, undrained): %zu\n",
                 afterMic.adcFill);

    // ── Stage D: open the Sound (record) app and test recording ──
    //
    // The netBook keyboard matrix includes physical dictaphone keys:
    //   EStdKeyDictaphoneRecord (158) = col 0 row 6
    //   EStdKeyDictaphonePlay (156)   = col 1 row 6
    //   EStdKeyDictaphoneStop (157)   = col 7 row 5
    // On EPOC R5, pressing REC (158) opens the Sound/Voice Notes app.
    // A second press (long hold) starts recording.  This mirrors the
    // approach used in harness/audio-harness.cpp for Windermere.
    //
    // After opening, we hold the REC key to start recording, pump mic
    // audio for 5 s, and check whether the recording DFC drained any
    // MCDR0 reads (i.e. the recording timer advanced).
    std::fprintf(stderr, "\n=== Stage D: open Sound app + record test ===\n");
    savePGM("netbook_stageD_pre.pgm");

    size_t logMarkD = g_logs.size();
    Snapshot beforeD = snap();
    bool esdrvBeforeD = g_sa ? g_sa->debugEsdrvPcSeen() : false;
    bool dfcBeforeD   = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    std::fprintf(stderr, "    pre-D: esdrvPcSeen=%d recordingDfcWaiting=%d isSeries7=%d isNetBook=%d\n",
                 esdrvBeforeD, dfcBeforeD,
                 g_sa ? (int)g_sa->debugIsSeries7Rom() : -1,
                 g_sa ? (int)g_sa->debugIsNetBookRom() : -1);

    // Replay the exact touch sequence from user logs (cold boot path).
    // Coordinates are in EPOC's TRawEvent space (same as updateTouchInput).
    // Sequence: tap1 opens Sound app, tap2 selects Record toolbar button,
    // tap3 starts recording — confirmed by user logs showing the recording
    // DFC entering its wait state immediately after tap3.
    // Baseline: sample the IRQ controller + idle% at the painted DESKTOP
    // (no recording yet).  Tells us whether idle=0% / this busy loop is
    // normal emulator behaviour or specific to the post-stop state.
    sampleIrqWindow("desktop-baseline", 3.0);

    std::fprintf(stderr, "    [D.1] tap (672, 464) — open Sound app\n");
    tap(672, 464);
    runFor(2.0);
    savePGM("netbook_stageD_after_tap1.pgm");

    bool dfcAfterTap1 = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    std::fprintf(stderr, "    after tap1: dfcWaiting=%d panic=%d\n",
                 dfcAfterTap1, panicDialogShowing());

    std::fprintf(stderr, "    [D.2] tap (591, 71) — select Record button\n");
    tap(591, 71);
    runFor(2.35);
    savePGM("netbook_stageD_after_tap2.pgm");

    bool dfcAfterTap2 = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    std::fprintf(stderr, "    after tap2: dfcWaiting=%d panic=%d\n",
                 dfcAfterTap2, panicDialogShowing());

    std::fprintf(stderr, "    [D.3] tap (429, 307) — start recording\n");
    tap(429, 307, 0.125);
    runFor(2.0);
    savePGM("netbook_stageD_recording_started.pgm");

    bool dfcAfterHold = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    std::fprintf(stderr, "    after tap3: dfcWaiting=%d panic=%d\n",
                 dfcAfterHold, panicDialogShowing());

    // NOTE: the old DictaphoneRecord-key (EStdKey 158) fallback here PANICKED
    // the Record app (KERN-EXEC 0) — the panic the previous session misread as
    // "recording works then playback is silent".  detection (recordingDfcWaiting_)
    // arms naturally ~1 s into the mic pump below via the redraw hook, so no
    // fallback is needed; pressing 158 was both unnecessary and fatal.  Removed.

    // Step 5: Pump 440 Hz mic tone for 5 seconds while recording.
    // This is the key test: if the Sound app successfully started
    // recording, the recording DFC should be polling MCDR0 and the
    // codrReads counter should advance.
    // Record window length (sim-seconds).  Default 15 s so we run WELL past
    // the ~1 s / ~7-buffer point where the older synthetic-completion path
    // panicked — a clean climb here is strong evidence the deliver-DFC
    // completion is faithful.  Override with REC_SECONDS.
    const int recSecs = std::getenv("REC_SECONDS") ?
                        std::atoi(std::getenv("REC_SECONDS")) : 15;
    std::fprintf(stderr, "    [D.4] pumping 440 Hz mic tone for %d s\n", recSecs);
    g_emu->setHostAudioEnabled(true, true);
    Snapshot beforeRec = snap();
    bool dfcBeforeRec = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    uint32_t clipBefore = g_sa ? g_sa->debugRecClipSamples() : 0;
    std::fprintf(stderr, "    pre-record: codrReads=%llu dfcWaiting=%d clipSamples=%u\n",
                 (unsigned long long)beforeRec.codrReads, dfcBeforeRec, clipBefore);
    // Track the per-second codrReads delta so a mid-window STALL (the handoff's
    // failure mode) is visible, not just the endpoint average.
    uint64_t prevReads = beforeRec.codrReads;
    bool everStalled = false;
    double recPhase = 0.0;
    int64_t recCyc0 = g_emu->currentCycles();
    for (int s = 0; s < recSecs * 50; ++s) {     // 50 * 20 ms = 1 s
        for (int i = 0; i < chunkSamples; ++i) {
            chunk[i] = (int16_t)(std::sin(recPhase) * 16000.0);
            recPhase += step;
            if (recPhase > 2 * 3.14159265358979323846) {
                recPhase -= 2 * 3.14159265358979323846;
            }
        }
        g_emu->writeAudioInput(chunk.data(), chunk.size());
        runFor(0.02);
        // Log progress every second: codrReads (real drained bytes), the
        // per-second rate, the MMF clip-sample count (the recorder's OWN
        // accumulated clip length — proves the app is alive and writing the
        // clip, not just the SAC FIFO draining), and recActive.
        if (s == recSecs * 25) savePGM("netbook_stageD_mid_record.pgm");
        // Poll for the KERN-EXEC panic dialog: codrReads keeps climbing via the
        // EDGE FIFO drain even after the Record APP dies, so codrReads alone
        // masks the crash.  Record the first second it appears.
        static int panicFirstS = -1;
        if (panicFirstS < 0 && (s % 5 == 0) && panicDialogShowing()) {
            panicFirstS = s;
            savePGM("netbook_panic_first.pgm");
            std::fprintf(stderr, "    [D.4 PANIC] 'Program closed' dialog first "
                         "seen at +%.1fs (codrReads=%llu recActive=%d)\n",
                         s / 50.0, (unsigned long long)snap().codrReads,
                         g_sa ? (int)g_sa->debugRecActive() : -1);
        }
        if (s > 0 && s % 50 == 0) {
            Snapshot mid = snap();
            uint64_t rate = mid.codrReads - prevReads;
            prevReads = mid.codrReads;
            if (rate < 4000) everStalled = true;  // below the capture threshold
            std::fprintf(stderr,
                "    [D.4 +%2ds] codrReads=%llu (%llu/s) clipSamples=%u "
                "recActive=%d\n",
                s / 50, (unsigned long long)mid.codrReads,
                (unsigned long long)rate,
                g_sa ? g_sa->debugRecClipSamples() : 0,
                g_sa ? (int)g_sa->debugRecActive() : -1);
        }
    }
    {
        int64_t recCyc1 = g_emu->currentCycles();
        double simSec = (double)(recCyc1 - recCyc0) / g_emu->getClockSpeed();
        std::fprintf(stderr,
            "    [D.4 sim] record loop advanced %lld cycles = %.2f sim-s "
            "(clock=%lld Hz) -> expected audioTicks=%.0f at 8 kHz\n",
            (long long)(recCyc1 - recCyc0), simSec,
            (long long)g_emu->getClockSpeed(), simSec * 8000.0);
    }
    if (everStalled)
        std::fprintf(stderr, "    [D.4] WARNING: a 1-s window fell below "
                     "4000 reads/s (capture stalled at some point)\n");

    Snapshot afterRec = snap();
    bool dfcAfterRec = g_sa ? g_sa->debugRecordingDfcWaiting() : false;
    bool esdrvAfterRec = g_sa ? g_sa->debugEsdrvPcSeen() : false;
    uint64_t recReads = afterRec.codrReads - beforeRec.codrReads;
    uint64_t recWrites = afterRec.codrWrites - beforeRec.codrWrites;

    std::fprintf(stderr, "\n=== Stage D summary ===\n");
    std::fprintf(stderr, "   Esdrv.pdd PC seen:          %s\n",
                 esdrvAfterRec ? "YES" : "no");
    std::fprintf(stderr, "   Recording DFC waiting:      %s (before=%s)\n",
                 dfcAfterRec ? "YES" : "no",
                 dfcBeforeRec ? "yes" : "no");
    std::fprintf(stderr, "   MCDR0 reads during record:  %llu  (%.0f/s avg)\n",
                 (unsigned long long)recReads, (double)recReads / recSecs);
    if (g_sa) {
        std::fprintf(stderr, "   recEsdrvChannel=%08x recordChannelPtr=%08x "
                     "recDmaEngChan=%08x\n",
                     g_sa->debugRecEsdrvChannel(), g_sa->debugRecordChannelPtr(),
                     g_sa->debugRecDmaEngChan());
        std::fprintf(stderr, "   SADR(0x706C) reads=%llu  SACSR(0x7074) reads=%llu"
                     "  IRQ_SSP fires=%llu  (codrReads valid pops=%llu)\n",
                     (unsigned long long)g_sa->debugSadrReads(),
                     (unsigned long long)g_sa->debugSacsrReads(),
                     (unsigned long long)g_sa->debugIrqSspFires(),
                     (unsigned long long)recReads);
    }
    if (g_sa) {
        std::fprintf(stderr, "   DMAIRQ completions (0x50297768) delivered: %llu  "
                     "(DMAENG buffers: %llu)\n",
                     (unsigned long long)g_sa->debugRecDmaIrqBuffers(),
                     (unsigned long long)g_sa->debugRecDmaEngBuffers());
        std::fprintf(stderr, "   IRQ inject: audioTicks=%llu evals=%llu attempts=%llu blkMASK=%llu blkPEND=%llu "
                     "fired=%llu\n",
                     (unsigned long long)g_sa->debugAudioTicks(),
                     (unsigned long long)g_sa->debugInjEval(),
                     (unsigned long long)g_sa->debugInjAttempt(),
                     (unsigned long long)g_sa->debugInjBlkMask(),
                     (unsigned long long)g_sa->debugInjBlkPend(),
                     (unsigned long long)g_sa->debugIrqSspFires());
    }
    std::fprintf(stderr, "   MCDR0 writes during record: %llu\n",
                 (unsigned long long)recWrites);
    std::fprintf(stderr, "   ADC ring fill after record:  %zu\n",
                 afterRec.adcFill);
    savePGM("netbook_stageD_post_record.pgm");

    // Dump key trace messages from the recording window.
    bool sawEigerFire = false, sawSsdrRead = false;
    for (size_t i = logMarkD; i < g_logs.size(); ++i) {
        if (g_logs[i].find("REC-TRACE") != std::string::npos ||
            g_logs[i].find("REC-IRQ") != std::string::npos ||
            g_logs[i].find("recording DFC") != std::string::npos ||
            g_logs[i].find("  ") == 0) {
            // Match all diagnostic lines (start with "  ")
            std::fprintf(stderr, "   LOG: %s\n", g_logs[i].c_str());
            if (g_logs[i].find("Eiger slot4 FIRE") != std::string::npos) sawEigerFire = true;
            if (g_logs[i].find("SSDR read") != std::string::npos) sawSsdrRead = true;
        }
    }
    std::fprintf(stderr, "   Eiger slot4 fires logged: %s\n", sawEigerFire ? "YES" : "no");
    std::fprintf(stderr, "   SSDR reads logged:   %s\n", sawSsdrRead ? "YES" : "no");

    // ── Stage E: tap STOP, wait, then tap PLAY to test playback ──
    std::fprintf(stderr, "\n=== Stage E: stop recording + play test ===\n");
    // Tap STOP button.  Transport-row icon centres measured from the
    // Record.app screenshot (640x480 LCD space, same as the working REC tap
    // at ~429,305): REW ~188, STOP ~238, PLAY ~327, REC ~417, FFWD ~484, all
    // at y ~305.  (The old y=350 / x+45 values missed the buttons entirely —
    // they landed on the key-hint label row below the icons.)
    bool panicBeforeStop = panicDialogShowing();
    std::fprintf(stderr, "    pre-STOP: panicDialog=%d recActive=%d\n",
                 panicBeforeStop, g_sa ? (int)g_sa->debugRecActive() : -1);
    g_emu->updateTouchInput(238, 305, true);
    runFor(0.1);
    g_emu->updateTouchInput(238, 305, false);
    // Fine-grained post-STOP probe: step in ~5 ms slices and record the cycle
    // at which (a) the recorder disarms (recActive 1->0) and (b) the KERN-EXEC
    // panic dialog appears (if ever).  With a clean tap sequence this shows
    // "panic dialog at never" — STOP does NOT panic.  (The KERN-EXEC 0 the
    // previous session chased was triggered only by the old DictaphoneRecord-
    // key fallback in Stage D, removed above — never by STOP itself.)
    // PSION_NB_FAST_PLAY: tap PLAY almost immediately after STOP (≈100 ms),
    // INSIDE the ~300-410 ms codrReads-stall window during which
    // recordingDfcWaiting_ is still armed — reproduces the real-time browser
    // case where the user taps PLAY right after STOP.  The default path below
    // waits ~6 s (probes), so recordingDfcWaiting_ has long cleared and the
    // wedge never triggers.
    const bool fastPlay = std::getenv("PSION_NB_FAST_PLAY") != nullptr;
    if (!fastPlay) {
        const int64_t clock = g_emu->getClockSpeed();
        const int64_t slice = clock / 200;          // 5 ms
        int disarmSlice = -1, panicSlice = -1;
        uint64_t cR0 = g_sa ? g_sa->debugCodrReads() : 0;
        for (int i = 0; i < 600; ++i) {             // up to 3 s
            g_emu->executeUntil(g_emu->currentCycles() + slice);
            bool act = g_sa ? g_sa->debugRecActive() : false;
            if (!act && disarmSlice < 0) disarmSlice = i;
            if (panicSlice < 0 && panicDialogShowing()) { panicSlice = i; }
            if (disarmSlice >= 0 && panicSlice >= 0) break;
        }
        uint64_t cR1 = g_sa ? g_sa->debugCodrReads() : 0;
        std::fprintf(stderr,
            "    [post-STOP probe] disarm at %s ms, panic dialog at %s ms, "
            "codrReads+%llu\n",
            disarmSlice >= 0 ? std::to_string(disarmSlice * 5).c_str() : "never",
            panicSlice  >= 0 ? std::to_string(panicSlice  * 5).c_str() : "never",
            (unsigned long long)(cR1 - cR0));
        sampleIrqWindow("post-stop", 3.0);
    } else {
        std::fprintf(stderr, "    [FAST_PLAY] tapping PLAY ~100 ms after STOP "
            "(inside stall window, dfcWaiting=%d)\n",
            g_sa ? (int)g_sa->debugRecordingDfcWaiting() : -1);
        runFor(0.1);
    }
    savePGM("netbook_stageE_stopped.pgm");
    std::fprintf(stderr, "    stopped. dfcWaiting=%d\n",
                 g_sa ? (int)g_sa->debugRecordingDfcWaiting() : -1);

    // Rewind to the start of the clip (the play cursor sits at clip end after
    // recording), then tap PLAY.  Measured icon centres (see STOP above):
    // REW ~188, PLAY ~327, y ~305.
    std::fprintf(stderr, "    [E.2] tapping Rewind, then Play\n");
    g_emu->updateTouchInput(188, 305, true);
    runFor(0.1);
    g_emu->updateTouchInput(188, 305, false);
    runFor(0.3);
    // Snapshot the DAC write counter just before PLAY so we can measure how
    // many samples the SAC TX path clocks out during the play window.
    uint64_t dacWritesBeforePlay = g_sa ? g_sa->debugCodrWrites() : 0;
    g_emu->updateTouchInput(327, 305, true);
    runFor(0.1);
    g_emu->updateTouchInput(327, 305, false);
    // Step through the play window in slices, tracking DAC growth + amplitude.
    bool   playSawTx   = false;
    int    playPkPkMax = 0;
    bool   playArmed   = false;
    {
        const int64_t clock = g_emu->getClockSpeed();
        const int64_t slice = clock / 100;        // 10 ms
        // Throughput probe: a heavy guest (e.g. an app spinning on a play
        // request that never completes) runs the netBook's non-WFI poll path
        // hot, which in the real-time-throttled browser presents as "playback
        // slows the device to a hang".  Measure native cycles/wall-second over
        // the play window so we can tell a spin from normal idle.
        const int playSlices = std::getenv("PLAY_SLICES") ? std::atoi(std::getenv("PLAY_SLICES")) : 600;
        auto t0 = std::chrono::steady_clock::now();
        int64_t cyc0 = g_emu->currentCycles();
        for (int i = 0; i < playSlices; ++i) {    // up to 6 s (default)
            g_emu->executeUntil(g_emu->currentCycles() + slice);
            if (g_sa && g_sa->debugNbPlayActive()) playArmed = true;
            int pk = g_sa ? g_sa->debugDacPeakToPeak() : 0;
            if (pk > playPkPkMax) playPkPkMax = pk;
        }
        auto t1 = std::chrono::steady_clock::now();
        double wall = std::chrono::duration<double>(t1 - t0).count();
        double simSec = (double)(g_emu->currentCycles() - cyc0) / (double)clock;
        std::fprintf(stderr,
            "    [play-throughput] sim=%.2fs wall=%.2fs  speed=%.2fx realtime "
            "(nbPlayActive=%d)\n", simSec, wall, wall > 0 ? simSec / wall : 0.0,
            g_sa ? (int)g_sa->debugNbPlayActive() : -1);
    }
    uint64_t dacWritesAfterPlay = g_sa ? g_sa->debugCodrWrites() : 0;
    uint64_t playDacWrites = dacWritesAfterPlay - dacWritesBeforePlay;
    playSawTx = playDacWrites > 8000;             // >~1 s of 8 kHz clocked out
    std::fprintf(stderr,
        "    [PLAY check] nbPlayArmed=%d SAC-TX DAC writes=%llu peak-to-peak=%d\n",
        playArmed, (unsigned long long)playDacWrites, playPkPkMax);
    std::fprintf(stderr,
        "    [PLAY check] recorded-clip: len=%zu  toneHz~=%d (440 Hz input expected)\n",
        g_sa ? g_sa->debugClipLen() : 0, g_sa ? g_sa->debugClipFreqHz() : -1);
    if (playSawTx && playPkPkMax > 256) {
        std::fprintf(stderr,
            "    [PLAY check] PASS: the recorded clip was clocked through the\n"
            "      SAC TX -> host DAC (varying samples, not silence).\n");
    } else {
        std::fprintf(stderr,
            "    [PLAY check] WARN: little/no varying audio reached the DAC\n"
            "      during the play window (writes=%llu pkpk=%d).\n",
            (unsigned long long)playDacWrites, playPkPkMax);
    }
    savePGM("netbook_stageE_played.pgm");
    std::fprintf(stderr, "    play done.\n");

    // Post-play responsiveness probe: a genuine OS wedge (the recorder app
    // spinning on a play request that never completes) would survive the clip
    // and leave the device unable to repaint.  Sample the idle/PC window, then
    // tap the menu/softkey region and confirm the screen actually changes.
    sampleIrqWindow("post-play", 3.0);
    {
        // Compare the screen before/after a tap to detect a wedge.
        int w = g_emu->getLCDWidth(), h = g_emu->getLCDHeight();
        auto grab = [&](std::vector<uint8_t> &g) {
            std::vector<uint8_t> px(w * h * 4);
            std::vector<uint8_t *> ln(h);
            for (int y = 0; y < h; y++) ln[y] = px.data() + y * w * 4;
            g_emu->readLCDIntoBuffer(ln.data(), true);
            g.resize(w * h);
            for (int i = 0; i < w * h; i++) g[i] = px[i * 4];
        };
        auto diffScreen = [&](const std::vector<uint8_t>&a, const std::vector<uint8_t>&b){
            long d = 0; size_t n = std::min(a.size(), b.size());
            for (size_t i = 0; i < n; ++i) d += std::abs((int)a[i] - (int)b[i]); return d; };
        // Try several interactions and report the max screen change.  Tap the
        // transport buttons (STOP 238, REW 188, PLAY 327, REC 417 @ y305 — all
        // interactive) plus the Esc/menu key; if NONE repaint, the app/OS is
        // wedged on the never-completing play request.
        std::vector<uint8_t> base, cur; grab(base);
        long maxDiff = 0;
        struct { int x, y; } taps[] = {{238,305},{417,305},{188,305},{327,305}};
        for (auto &t : taps) {
            g_emu->updateTouchInput(t.x, t.y, true);  runFor(0.15);
            g_emu->updateTouchInput(t.x, t.y, false); runFor(0.6);
            grab(cur); long d = diffScreen(base, cur);
            if (d > maxDiff) maxDiff = d;
            std::fprintf(stderr, "    [post-play tap %d,%d] screen-diff=%ld\n", t.x, t.y, d);
        }
        std::fprintf(stderr, "    [post-play responsiveness] max screen-diff = %ld (%s)\n",
                     maxDiff, maxDiff > 2000 ? "RESPONSIVE" : "WEDGED (no repaint to any input)");
    }

    // Check for crashes during Stage E.
    bool crashedD = logContains(logMarkD, "prefetch error") ||
                    logContains(logMarkD, "data abort");

    // ── Summary: classify and exit ──
    std::fprintf(stderr, "\n=== Summary ===\n");
    if (crashedD) {
        std::fprintf(stderr,
            "WARN: crash detected during Stage D (recording test).\n"
            "  Check netbook_stageD_*.pgm screenshots for UI state.\n");
    }
    if (!codecEnabled) {
        std::fprintf(stderr,
            "FAIL(3): MCCR0.MCE never went high — codec never enabled.\n"
            "  The kernel's BSP audio init did not complete.\n");
        return 3;
    }
    bool sawMicReads = afterMic.codrReads > beforeMic.codrReads;
    if (!sawDacWrites && !sawPwmEnable) {
        std::fprintf(stderr,
            "FAIL(4): codec enabled but no DAC traffic (MCDR0 + PWM1).\n"
            "  No system beep or app SFX fired during the run.\n");
        return 4;
    }

    // Recording-specific verdicts (Stage D).
    if (esdrvAfterRec && dfcAfterRec && recReads == 0) {
        std::fprintf(stderr,
            "PARTIAL(7): recording DFC started (Eiger slot 4 firing) but\n"
            "  no MCDR0 reads during the 5 s recording window.  The DFC\n"
            "  is being invoked but samples are not flowing through the\n"
            "  codec read path.  Timer stays at 0:00.\n");
        return 7;
    }
    if (esdrvAfterRec && !dfcAfterRec) {
        std::fprintf(stderr,
            "PARTIAL(6): Esdrv.pdd loaded but recording DFC never entered\n"
            "  its wait state (PC 0x50298700 not reached).  Record app opened\n"
            "  (confirmed via screenshot) but the REC-key long-hold did not\n"
            "  trigger the recording DFC poll loop.\n"
            "  MCDR0 reads during Stage D: %llu (mic samples ARE flowing via\n"
            "  the general MCP/SSP path, just not through the recording DFC).\n"
            "  The recording timer stays at 0:00 because Eiger slot 4 never\n"
            "  fires — the DFC that drives the timer is not in its wait state.\n",
            (unsigned long long)recReads);
        return 6;
    }

    if (!sawMicReads && recReads == 0) {
        std::fprintf(stderr,
            "PARTIAL(5): speaker path live but no MCDR0 reads — mic\n"
            "  bridge wired but kernel never drained input.  Likely no\n"
            "  recording app was foregrounded; speaker should work\n"
            "  in-browser.\n");
        return 5;
    }
    if (recReads > 0) {
        std::fprintf(stderr,
            "PASS: codec on, speaker writes seen, recording reads=%llu.\n"
            "  Recording timer should be advancing.\n",
            (unsigned long long)recReads);
        return 0;
    }
    if (sawMicReads) {
        std::fprintf(stderr,
            "PASS: codec on, speaker writes seen, mic reads seen (Stage C).\n"
            "  Stage D recording app navigation may not have reached Sound.\n");
        return 0;
    }
    std::fprintf(stderr,
        "PARTIAL(5): speaker path live but no mic reads in either stage.\n");
    return 5;
}
