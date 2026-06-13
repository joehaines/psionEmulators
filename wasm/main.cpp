// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten/emscripten.h>
#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/windermere.h"
#include "../core/series3c.h"
#include "../core/sa1100.h"
#include "../core/clps7111.h"
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

static EmuBase *g_emu = nullptr;
static std::vector<uint8_t> g_romBuffer;
static std::vector<uint8_t> g_cardBuffer;
// Staging buffer for SSD image uploads (parallel to g_cardBuffer for CF).
// SSDs are uploaded one at a time from the UI; the buffer is reused
// across slots. Re-allocated on each prepareSSDImageUpload call.
static std::vector<uint8_t> g_ssdBuffer;
// Tracks the "Show Logs" panel state so a newly-loaded emulator inherits
// the right setting. When false, ARM710::log short-circuits before
// vsnprintf — so a release user pays zero for the thousands of log() call
// sites in the core. Flipped on by the JS side when the log panel opens.
static bool g_loggingEnabled = false;

// Resizes g_romBuffer to `size` bytes and returns its WASM heap pointer.
// JS caller writes ROM bytes via HEAPU8.set(bytes, ptr), then calls loadBufferedROM().
uintptr_t prepareROMUpload(unsigned size) {
    g_romBuffer.resize(size);
    return reinterpret_cast<uintptr_t>(g_romBuffer.data());
}

// Detects device from g_romBuffer content and boots the emulator.
// If `deviceId` is non-empty the caller's explicit device selection
// takes precedence — several devices share a variant-ID (5mx / 5mx Pro
// / MC218 all report 0x07060001, so findProfileByVariant alone would
// always resolve to the first registered match). Returns the picked
// device's displayName on success, or "" if unknown/unsupported.
std::string loadBufferedROM(unsigned len, std::string deviceId) {
    const DeviceProfile *profile = nullptr;
    if (!deviceId.empty()) profile = findProfileById(deviceId.c_str());
    if (!profile) {
        uint32_t variant = detectROMVariant(g_romBuffer.data(), len);
        profile = findProfileByVariant(variant);
    }
    // Fallback: some ROMs (e.g. Series 5) have no detectable variant ID.
    // Match by expected ROM size among supported devices with romVariantId == 0.
    if (!profile) {
        for (const DeviceProfile *p = allProfiles(); p->id != nullptr; ++p) {
            if (p->romVariantId == 0 && p->createEmulator && p->expectedRomSize == len) {
                profile = p;
                break;
            }
        }
    }

    if (!profile || !profile->createEmulator) return "";

    delete g_emu;
    g_emu = profile->createEmulator();
    g_emu->setLogger([](const char *s) {
        EM_ASM({ console.log(UTF8ToString($0)); }, s);
    });
    g_emu->setLoggingEnabled(g_loggingEnabled);
    g_emu->loadROM(g_romBuffer.data(), len);

    return std::string(profile->displayName);
}

// Advances the emulator by ~1/64 second of cycles.
// Returns false if no emulator is loaded.
//
// Wall-clock budget — keeps the browser responsive under a CPU-bound guest.
// stepFrame() is called once per requestAnimationFrame on the *main thread*,
// so the whole tab is frozen for however long the synchronous executeUntil()
// takes.  A normal idle guest spends most of each frame in WFI, and
// executeUntil() fast-forwards through that almost for free — so the call
// returns in well under a millisecond and the budget below never trips.  But
// an intensive app that pins the CPU (a game such as Pac-Man, which sits in a
// tight render/logic loop and never enters WFI) forces the emulator to
// actually execute every one of the clock/64 cycles in the frame — ~3.45M on
// the 221 MHz SA-1100 (Series 7 / netBook).  If the host can't emulate that
// many cycles inside a frame, the one executeUntil() call blocks the main
// thread for tens of milliseconds at a time: input, redraw and the rest of
// the page go barely-responsive and the emulator appears to "not work".
//
// So we cap the wall time spent inside a single stepFrame.  We advance the
// guest in sub-frame chunks and stop once the budget is spent, returning to
// the RAF loop so it can paint, pump audio and — crucially — let the browser
// service input before the next tick.  When the host can't keep up, sim time
// simply falls behind real time (the game runs in slow motion) instead of
// wedging the tab — a far better failure mode.  Because the next frame
// recomputes its target from currentCycles(), we never accrue a backlog of
// "owed" cycles (no catch-up spiral); the emulator just runs at whatever rate
// the host can sustain within the per-frame budget.
//
// Tunables (env vars, honoured without a rebuild via setEnvVar):
//   PSION_FRAME_BUDGET_MS  — per-frame wall budget in ms (default 8).
//   PSION_FRAME_BUDGET_OFF — set to restore the old unbounded behaviour.
bool stepFrame() {
    if (!g_emu) return false;
    const int64_t target = g_emu->currentCycles() + (g_emu->getClockSpeed() / 64);

    static int    budgetOff = -1;
    static double budgetMs  = 8.0;
    if (budgetOff < 0) {
        budgetOff = std::getenv("PSION_FRAME_BUDGET_OFF") ? 1 : 0;
        if (const char *e = std::getenv("PSION_FRAME_BUDGET_MS")) {
            double v = std::atof(e);
            if (v > 0.0) budgetMs = v;
        }
    }

    // Unbounded path: when the budget is disabled, or while the CF "loading
    // from card" gap is active — that path has its own explicit fast-forward
    // burst in the frontend RAF loop (isCFPollGapActive), and the screen is a
    // near-static progress bar, so we deliberately let it run a full frame per
    // step so a card read still compresses to a few hundred ms.
    if (budgetOff || g_emu->cfGapActive()) {
        g_emu->executeUntil(target);
        return true;
    }

    // Step in sub-frame chunks, checking the wall clock between chunks so a
    // CPU-bound run yields the main thread close to on schedule (worst-case
    // overshoot is one chunk's wall time).  Chunk = 1/16 of a frame of sim
    // cycles; an idle guest still fast-forwards through each chunk almost for
    // free, so this adds only a handful of cheap loop iterations per frame.
    int64_t chunk = (g_emu->getClockSpeed() / 64) / 16;
    if (chunk < 1) chunk = 1;  // guarantee forward progress on any clock
    const double startMs = emscripten_get_now();
    while (static_cast<int64_t>(g_emu->currentCycles()) < target) {
        int64_t next = static_cast<int64_t>(g_emu->currentCycles()) + chunk;
        if (next > target) next = target;
        g_emu->executeUntil(next);
        if (emscripten_get_now() - startMs >= budgetMs) break;
    }
    return true;
}

// Advances the emulator by exactly one frame of *simulated* time, UNBOUNDED —
// it blocks until the full clock/64 cycles have been emulated, however long
// that takes in wall time. This is the pre-budget stepFrame() behaviour, kept
// for the cold-boot preroll: that path runs a fixed number of frames to fast-
// forward a fixed amount of SIM time past a device's blank boot window (so the
// first rendered frame has content). It must advance the full sim time per
// call — the wall-clock budget in stepFrame() would cut a frame short on a
// busy bootloader (e.g. the netBook's `B .` idle loop, which never enters WFI)
// and leave the preroll short of the splash. The user is already waiting on
// boot here, so blocking is fine and expected.
bool stepFrameFull() {
    if (!g_emu) return false;
    g_emu->executeUntil(g_emu->currentCycles() + (g_emu->getClockSpeed() / 64));
    return true;
}

// Run a burst of cycles for serial transfer acceleration.
// Called from the JS poll loop after writing bytes, so the kernel
// processes the request within the same poll tick instead of waiting
// for the next requestAnimationFrame.
void serialPumpCycles() {
    if (!g_emu) return;
    // 500K cycles ≈ 2.3ms sim time — enough for ISR + DFC +
    // Reschedule + RLS4 RunL + RFSV response + UART TX.
    g_emu->executeUntil(g_emu->currentCycles() + 500000);
}

// Reads the LCD framebuffer into a pre-allocated WASM heap buffer (RGBA, 4 bytes/pixel).
// bufferPtr is a pointer into HEAPU8 obtained via Module._malloc(lcdWidth * lcdHeight * 4).
void readLCD(uintptr_t bufferPtr) {
    if (!g_emu) return;
    int w = g_emu->getLCDWidth();
    int h = g_emu->getLCDHeight();
    uint8_t *buf = reinterpret_cast<uint8_t *>(bufferPtr);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++)
        lines[y] = buf + (y * w * 4);
    g_emu->readLCDIntoBuffer(lines.data(), true);
}

struct DeviceInfo {
    int digitiserWidth;
    int digitiserHeight;
    int lcdOffsetX;
    int lcdOffsetY;
    int lcdWidth;
    int lcdHeight;
    std::string deviceName;
    // Audio capability of the currently-loaded emulator subclass.
    // The frontend conditions Speaker/Mic controls and the WebAudio engine
    // on hasAudio; audioSampleRate tells it what rate to resample to.
    // hasMic gates the Mic toggle separately so speaker-only hardware
    // (buzzer-era SIBO devices, Siena, Workabout) never shows it.
    bool hasAudio;
    bool hasMic;
    int audioSampleRate;
};

DeviceInfo getDeviceInfo() {
    if (!g_emu) return {};
    return {
        g_emu->getDigitiserWidth(),
        g_emu->getDigitiserHeight(),
        g_emu->getLCDOffsetX(),
        g_emu->getLCDOffsetY(),
        g_emu->getLCDWidth(),
        g_emu->getLCDHeight(),
        std::string(g_emu->getDeviceName()),
        g_emu->hasAudio(),
        g_emu->hasMicrophone(),
        g_emu->getAudioSampleRate(),
    };
}

// LCD-controller diagnostics (SA-1100 only) — DBAR1/LCCR0/LCCR3, for the
// blank-screen investigation.  Returns 0 on non-SA-1100 devices.
unsigned getLcdDbar()  { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdDbar()  : 0u; }
unsigned getLcdDbar2() { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdDbar2() : 0u; }
unsigned getLcdLccr0() { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdLccr0() : 0u; }
unsigned getLcdLccr1() { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdLccr1() : 0u; }
unsigned getLcdLccr2() { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdLccr2() : 0u; }
unsigned getLcdLccr3() { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugLcdLccr3() : 0u; }
unsigned peekRam(unsigned addr) { auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu); return sa ? sa->debugPeekRam(addr) : 0u; }
unsigned getCpuPc()  { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? c->getGPR(15) : 0u; }
unsigned getIcpr() { auto *sa = dynamic_cast<SA1100::Emulator*>(g_emu); return sa ? sa->debugIcpr() : 0u; }
unsigned getIcmr() { auto *sa = dynamic_cast<SA1100::Emulator*>(g_emu); return sa ? sa->debugIcmr() : 0u; }
unsigned getCpuLr()  { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? c->getGPR(14) : 0u; }
unsigned getCpuWfi() { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return (c && c->wfiRequested) ? 1u : 0u; }
// Linear-memory byte offsets of the live register file, so a JS-side JIT can
// generate WASM blocks that read/write the real GPRs[]/CPSR in place. Returns 0
// when there is no ARM CPU (V30 devices) — the JIT path is SA-1100-only anyway.
unsigned getRegFileAddr() { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? (unsigned)c->gprFileAddr() : 0u; }
unsigned getCpsrAddr()    { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? (unsigned)c->cpsrAddr()    : 0u; }
// Architectural PC (getGPR(15) carries the prefetch convention; this is the real
// next-instruction address). The JIT dispatcher compares its returned next-PC
// against this when validating block execution against the interpreter.
unsigned getCpuRealPc() { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? c->getRealPC() : 0u; }
// Step the ARM core exactly one instruction (bypassing the SoC loop) and return
// the cycles it consumed. This is the interpreter-fallback primitive the JIT
// dispatcher calls for instructions the codegen doesn't support, and the oracle
// the JIT block is validated against. CPU-only by design — no timer/LCD/IRQ
// servicing — so it must not drive a real session on its own.
unsigned tickCpu() { auto *c = g_emu ? g_emu->getArmCpu() : nullptr; return c ? c->tick() : 0u; }
// Desktop-idle diagnostics: exception/WFI/instruction counters (see arm710.cpp).
extern uint64_t g_armExc[16];
extern uint64_t g_armWfi;
extern uint64_t g_armInsn;
double getExcCount(unsigned modeLo4) { return (double)g_armExc[modeLo4 & 0xF]; }
double getWfiCount()  { return (double)g_armWfi; }
double getInsnCount() { return (double)g_armInsn; }
extern uint64_t g_swiHist[256]; extern uint32_t g_swiInsnEx[256]; extern uint32_t g_swiPcEx[256];
double   getSwiHist(unsigned b)   { return (double)g_swiHist[b & 0xFF]; }
unsigned getSwiInsnEx(unsigned b) { return g_swiInsnEx[b & 0xFF]; }
unsigned getSwiPcEx(unsigned b)   { return g_swiPcEx[b & 0xFF]; }
void   resetCpuCounters() { for (int i=0;i<16;i++) g_armExc[i]=0; g_armWfi=0; g_armInsn=0;
    for (int i=0;i<256;i++){ g_swiHist[i]=0; g_swiInsnEx[i]=0; g_swiPcEx[i]=0; } }
// MMU-aware 32-bit read of a guest *virtual* address (for kernel-data probes
// like the WServ EventStatus cell at VA 0x80000b94).  Returns 0xDEADBEEF if
// the VA doesn't translate.
unsigned readVirt(unsigned va) {
    auto *c = g_emu ? g_emu->getArmCpu() : nullptr;
    if (!c) return 0xDEADBEEFu;
    auto v = c->readVirtualDebug(va, ARM710::V32);
    return v ? *v : 0xDEADBEEFu;
}

// Debug 32-bit virtual write — used to experiment with poking OS kernel
// state (e.g. forcing a media-driver re-open on CF swap) from JS without a
// C++ rebuild per experiment.  No-op if the VA doesn't translate.
void writeVirt(unsigned va, unsigned value) {
    auto *c = g_emu ? g_emu->getArmCpu() : nullptr;
    if (c) c->writeVirtual(value, va, ARM710::V32);
}

// Toggle the booted-netBook-OS abort/exception trace (logs FAR + faulting PC
// for every data/prefetch/undef exception).  Enable just before opening drive
// D: to localize the crash.
void setNbExcTrace(int on) {
    auto *c = g_emu ? g_emu->getArmCpu() : nullptr;
    if (c) c->setNbExcTrace(on != 0);
}

// Drains up to `maxSamples` int16 speaker samples into the WASM heap
// buffer pointed to by `bufferPtr`. Returns the number actually written.
// Typical usage: JS pre-allocates a 16-bit view, passes its byte offset.
unsigned readAudioOutput(uintptr_t bufferPtr, unsigned maxSamples) {
    if (!g_emu) return 0;
    int16_t *dst = reinterpret_cast<int16_t *>(bufferPtr);
    return static_cast<unsigned>(g_emu->readAudioOutput(dst, maxSamples));
}

// Pushes `count` int16 mic samples from the WASM heap buffer pointed to
// by `bufferPtr` into the ADC FIFO. Silently drops overflow.
void writeAudioInput(uintptr_t bufferPtr, unsigned count) {
    if (!g_emu) return;
    const int16_t *src = reinterpret_cast<const int16_t *>(bufferPtr);
    g_emu->writeAudioInput(src, count);
}

// Independent host-side gates for speaker / mic streams.
void setHostAudioEnabled(bool speaker, bool mic) {
    if (g_emu) g_emu->setHostAudioEnabled(speaker, mic);
}

// Audio diagnostics exported for the browser test harness
// (scripts/test-audio-browser.mjs). Returns an opaque object the JS
// side can inspect to confirm codec activity end-to-end.
struct AudioDiag {
    unsigned codrReads;       // total CODR sample pops since boot
    unsigned codrWrites;      // total CODR sample pushes since boot
    unsigned adcFill;         // host-side ADC ring depth
    unsigned dacFill;         // host-side DAC ring depth
    unsigned codecConfig;     // CONFG mirror (3 = enabled)
    unsigned recordChannel;   // captured dictaphone channel ptr
    unsigned channelState;    // *(channel + 0x1c) via MMU peek (0..4)
    unsigned csintFired;      // total CSINT assertions since boot
    unsigned txDrainTicks;    // total virtual-TX-FIFO drain decrements
    unsigned txFifoLevel;     // current virtual TX FIFO level (0..16)
    // ── Clock-independent recording-capture diagnostics (SA-1100) ──
    unsigned recActive;       // recordingDfcWaiting_ (1 once recording armed)
    unsigned recPosValAddr;   // MMF clip position object VA (0 until captured)
    unsigned recClipSamples;  // [posVal+0x28] recorded sample count (≈bytes)
    unsigned recClipState;    // [posVal+0x1c] clip state (3 == recording)
    unsigned isNetBook;       // 1 once the netBook EPOC R5 OS image is running
    unsigned dacPeakToPeak;   // peak-to-peak of the queued DAC samples (varying audio vs constant)
    unsigned dacFreqHz;       // zero-crossing freq estimate of the DAC samples
};

AudioDiag getAudioDiag() {
    AudioDiag d{};
    if (!g_emu) return d;
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu)) {
        d.codrReads      = (unsigned)w->debugCodrReads();
        d.codrWrites     = (unsigned)w->debugCodrWrites();
        d.adcFill        = (unsigned)w->debugAdcFill();
        d.dacFill        = (unsigned)w->debugDacFill();
        d.codecConfig    = w->debugCodecConfig();
        d.recordChannel  = w->debugRecordChannelPtr();
        d.channelState   = w->debugChannelState();
        d.csintFired     = (unsigned)w->debugCsintFired();
        d.txDrainTicks   = (unsigned)w->debugTxDrainTicks();
        d.txFifoLevel    = (unsigned)w->debugTxFifoLevel();
        return d;
    }
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu)) {
        // Series 7 / netBook (SA-1100 + UCB1200).  Subset of the
        // Windermere diag covering the fields that make sense on the
        // SA-1100 audio path: MCDR0 sample counts, ring fill, MCCR0
        // (the codecConfig analogue — CRC/MCE/rate bits), and the
        // UCB1200 audio control reg shadow (read back into the
        // codecConfig field for the browser test's diag JSON, where
        // a non-zero value confirms the kernel wrote it).  Channel
        // state / recordChannel are Windermere-specific (dictaphone
        // driver internals) and stay 0 here.
        d.codrReads      = (unsigned)sa->debugCodrReads();
        d.codrWrites     = (unsigned)sa->debugCodrWrites();
        d.adcFill        = (unsigned)sa->debugAdcFill();
        d.dacFill        = (unsigned)sa->debugDacFill();
        d.codecConfig    = sa->debugMccr0();
        d.recordChannel  = sa->debugUcbAudioCtrl0();
        d.channelState   = sa->debugUcbAudioCtrl1();
        d.recActive      = sa->debugRecActive() ? 1u : 0u;
        d.recPosValAddr  = sa->debugRecPosValAddr();
        d.recClipSamples = sa->debugRecClipSamples();
        d.recClipState   = sa->debugRecClipState();
        d.isNetBook      = sa->debugIsNetBookRom() ? 1u : 0u;
        d.dacPeakToPeak  = (unsigned)sa->debugDacPeakToPeak();
        d.dacFreqHz      = (unsigned)sa->debugDacFreqHz();
        return d;
    }
    return d;
}

// Force the dictaphone channel state via the MMU. Mirrors
// Windermere::Emulator::debugForceChannelState; returns whether
// the write reached kernel RAM. Used by the in-browser test to
// jump into state=1 without the Voice Notes UI interaction.
bool debugForceChannelState(unsigned s) {
    if (!g_emu) return false;
    auto *w = dynamic_cast<Windermere::Emulator *>(g_emu);
    return w ? w->debugForceChannelState(s) : false;
}

// Play a brief 440 Hz test tone via the emulator's DAC queue. Useful
// when the user reports "speaker doesn't work" and we want to verify
// the JS→WASM→worklet pipeline independently of whether the guest is
// driving BZCONT/CODR. Call from the browser console:
//
//   window.__psionMod.debugPlayTestTone()
//
// You should hear a clear ~half-second 440 Hz beep. If you don't, the
// problem is in the host audio engine (AudioContext suspended, worklet
// node disconnected, browser muted, etc.), not in the guest emulation.
void debugPlayTestTone() {
    if (!g_emu) return;
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu)) {
        w->debugInjectTestTone();
        return;
    }
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu)) {
        sa->debugInjectTestTone();
        return;
    }
}

unsigned debugDacHistory(uintptr_t bufferPtr, unsigned capacity) {
    if (!g_emu || capacity == 0) return 0;
    auto *w = dynamic_cast<Windermere::Emulator *>(g_emu);
    if (!w) return 0;
    int8_t tmp[Windermere::Emulator::kDacHistorySize];
    size_t n = w->debugDacHistoryCopy(tmp);
    if (n > capacity) n = capacity;
    int8_t *dst = reinterpret_cast<int8_t *>(bufferPtr);
    std::memcpy(dst, tmp, n);
    return (unsigned)n;
}

void sendKey(int epocKey, bool down) {
    if (g_emu) g_emu->setKeyboardKey(static_cast<EpocKey>(epocKey), down);
}

void sendTouch(int x, int y, bool down) {
    if (g_emu) g_emu->updateTouchInput(x, y, down);
}

// Live state of the LCD electroluminescent backlight pin. The frontend
// polls this so its on-screen backlight overlay tracks whatever EPOC is
// doing (Fn+Space toggling, auto-off timeout, Control Panel slider).
// Returns false on devices without a modelled backlight pin — see
// EmuBase::getBacklight for the per-device coverage matrix.
bool getBacklight() {
    return g_emu && g_emu->getBacklight();
}

// True once the CPU is wedged in an unrecoverable prefetch-abort loop.
// The worker polls this after each frame to auto-halt a crashed device.
bool isCpuCrashed() {
    return g_emu && g_emu->isCpuCrashed();
}

// Resizes g_cardBuffer to `size` bytes and returns its WASM heap pointer.
// JS caller writes CF image bytes via HEAPU8.set(bytes, ptr), then calls
// attachCFImage() to hand them to the current emulator.
uintptr_t prepareCFImageUpload(unsigned size) {
    g_cardBuffer.resize(size);
    return reinterpret_cast<uintptr_t>(g_cardBuffer.data());
}

// Attaches the buffered bytes to the current emulator as a CF card.
// Returns true on success, false if no emulator is loaded.
bool attachCFImage(unsigned size) {
    if (!g_emu || size == 0) return false;
    return g_emu->attachCard(g_cardBuffer.data(), size);
}

// Updates the attached CF image IN PLACE: replaces the backing bytes but
// keeps the socket "inserted" and the OS mount alive (no eject/insert,
// no re-power).  On the Series 7 OS this also patches the F32 server's
// cached root-directory sector so a fresh E: listing reflects the new
// files — the "push card edits to the device" path that avoids the
// hot-swap re-mount wall.  Returns true on success.
bool updateCFImageInPlace(unsigned size) {
    if (!g_emu || size == 0) return false;
    return g_emu->updateCardImageInPlace(g_cardBuffer.data(), size);
}

// Simulates card removal. Safe to call when no card is attached.
void detachCFImage() {
    if (g_emu) g_emu->detachCard();
}

// Returns whether the current emulator currently has a card inserted.
bool isCFImageAttached() {
    return g_emu && g_emu->isCardInserted();
}

// Size in bytes of the currently attached image (0 if none).
unsigned getCFImageSize() {
    return g_emu ? static_cast<unsigned>(g_emu->getCardImageSize()) : 0;
}

// Copies the currently attached image's bytes into the WASM heap buffer
// pointed to by `bufferPtr`. Caller must have pre-allocated at least
// getCFImageSize() bytes via Module._malloc(). No-op if no card is attached.
void readCFImage(uintptr_t bufferPtr) {
    if (!g_emu) return;
    const uint8_t *src = g_emu->getCardImageData();
    size_t len = g_emu->getCardImageSize();
    if (!src || len == 0) return;
    std::memcpy(reinterpret_cast<uint8_t *>(bufferPtr), src, len);
}

// CF diagnostic counters: ATA commands issued by the OS driver and sectors
// drained.  Lets a browser test measure whether the OS actually reads an
// attached card (ata climbs) vs. ignoring it (flat) — drive-mount verification
// without scraping the EPOC UI.
unsigned getCfAtaCommands() {
    return g_emu ? g_emu->getCfStats().ataCommands : 0;
}
unsigned getCfSectorDrains() {
    return g_emu ? g_emu->getCfStats().sectorDrains : 0;
}

// ── Live RAM snapshot ────────────────────────────────────────────────
// Lets the frontend download the current emulator RAM image so a user-
// triggered moment of corruption can be replayed in the harness via
// --ram-snapshot. Exposed for SIBO/SIBO2 (where M:-drive lives in RAM);
// devices without a host-visible RAM return 0 here and the button no-ops.
unsigned getRamSnapshotSize() {
    return g_emu ? static_cast<unsigned>(g_emu->getRamSize()) : 0;
}

void readRamSnapshot(uintptr_t bufferPtr) {
    if (!g_emu) return;
    const uint8_t *src = g_emu->getRamBuffer();
    size_t len = g_emu->getRamSize();
    if (!src || len == 0) return;
    std::memcpy(reinterpret_cast<uint8_t *>(bufferPtr), src, len);
}

// ── Host serial bridge ──────────────────────────────────────────────
// Bridges JS to the device UART for file-transfer protocols (PsiWin /
// PLP). uartIndex is 1 or 2 matching the SoC's UART1/UART2 on
// Windermere; for SIBO2 (3a/3c/3mx/siena/workabout/workaboutmx) the
// single PsionCondor UART takes any index. No-op on devices without a
// host serial bridge.

bool serialAttachHost(int uartIndex) {
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return w->serialAttachHost(uartIndex);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return s->serialAttachHost(uartIndex);
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu))
        return sa->serialAttachHost(uartIndex);
    // Covers Osaris and Series 5 (CL-PS7110) via inheritance.
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return c->serialAttachHost(uartIndex);
    return false;
}

bool serialDetachHost(int uartIndex) {
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return w->serialDetachHost(uartIndex);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return s->serialDetachHost(uartIndex);
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu))
        return sa->serialDetachHost(uartIndex);
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return c->serialDetachHost(uartIndex);
    return false;
}

bool serialIsAttached(int uartIndex) {
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return w->serialIsAttached(uartIndex);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return s->serialIsAttached(uartIndex);
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu))
        return sa->serialIsAttached(uartIndex);
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return c->serialIsAttached(uartIndex);
    return false;
}

// JS pushes outgoing (host → device) bytes via this entry point. Returns
// the number of bytes accepted; caller can resubmit any tail on the next
// tick if the FIFO was full.
unsigned serialWriteFromHost(int uartIndex, uintptr_t bufferPtr, unsigned len) {
    if (len == 0) return 0;
    const auto *data = reinterpret_cast<const uint8_t *>(bufferPtr);
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return (unsigned)w->serialWriteFromHost(uartIndex, data, len);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return (unsigned)s->serialWriteFromHost(uartIndex, data, len);
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu))
        return (unsigned)sa->serialWriteFromHost(uartIndex, data, len);
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return (unsigned)c->serialWriteFromHost(uartIndex, data, len);
    return 0;
}

// JS pulls device → host bytes the guest has queued for transmission.
// Returns bytes actually written into the JS-supplied buffer.
unsigned serialReadToHost(int uartIndex, uintptr_t bufferPtr, unsigned cap) {
    if (cap == 0) return 0;
    auto *dst = reinterpret_cast<uint8_t *>(bufferPtr);
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return (unsigned)w->serialReadToHost(uartIndex, dst, cap);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return (unsigned)s->serialReadToHost(uartIndex, dst, cap);
    if (auto *sa = dynamic_cast<SA1100::Emulator *>(g_emu))
        return (unsigned)sa->serialReadToHost(uartIndex, dst, cap);
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return (unsigned)c->serialReadToHost(uartIndex, dst, cap);
    return 0;
}

unsigned serialHostTxAvailable(int uartIndex) {
    if (auto *w = dynamic_cast<Windermere::Emulator *>(g_emu))
        return (unsigned)w->serialHostTxAvailable(uartIndex);
    if (auto *s = dynamic_cast<Series3c::Emulator *>(g_emu))
        return (unsigned)s->serialHostTxAvailable(uartIndex);
    if (auto *c = dynamic_cast<CLPS7111::Emulator *>(g_emu))
        return (unsigned)c->serialHostTxAvailable(uartIndex);
    return 0;
}

// ── SSD pack image hooks ──────────────────────────────────────────────
// Parallel to the CF helpers above but slot-indexed (Series 3 / 3a /
// 3c / 3mx have two slots; Siena has one). g_ssdBuffer is reused as a
// staging area: prepareSSDImageUpload returns a heap pointer the JS
// fills via HEAPU8.set(), then attachSSDImage(slot, size) copies it
// into the emulator.

uintptr_t prepareSSDImageUpload(unsigned size) {
    g_ssdBuffer.resize(size);
    return reinterpret_cast<uintptr_t>(g_ssdBuffer.data());
}

// `ssdType` mirrors PsionSSD::Type: 0 = auto (sniff the 0xF1A5 magic),
// 1 = RAM, 2 = writable Type 1 Flash, 3 = hardware write-protected.
// The SSD dialog passes 3 for FEFS packs (factory images only mount
// when the info byte declares write-protection) and 1 for RAM dumps.
bool attachSSDImage(unsigned slot, unsigned size, unsigned ssdType) {
    if (!g_emu || size == 0) return false;
    return g_emu->attachSSD(int(slot), g_ssdBuffer.data(), size, int(ssdType));
}

void detachSSDImage(unsigned slot) {
    if (g_emu) g_emu->detachSSD(int(slot));
}

bool isSSDImageAttached(unsigned slot) {
    return g_emu && g_emu->isSSDInserted(int(slot));
}

unsigned getSSDImageSize(unsigned slot) {
    return g_emu ? unsigned(g_emu->getSSDImageSize(int(slot))) : 0;
}

void readSSDImage(unsigned slot, uintptr_t bufferPtr) {
    if (!g_emu) return;
    const uint8_t *src = g_emu->getSSDImageData(int(slot));
    size_t len = g_emu->getSSDImageSize(int(slot));
    if (!src || len == 0) return;
    std::memcpy(reinterpret_cast<uint8_t *>(bufferPtr), src, len);
}

// ── Psion Organiser II Datapak / Rampak image hooks ──────────────────
// Distinct from the SIBO SSD pack hooks above: the Organiser II's pack
// model has a per-slot kind discriminant (read-only Datapak vs writable
// Rampak), so the frontend dialog needs to know which kind is loaded.
// Slot indexing matches the device's "Pack A / Pack B" labels (0/1).
// Shares a single staging buffer with the SSD path — the upload UI is
// always one-at-a-time so no concurrency hazard.

uintptr_t prepareDatapakUpload(unsigned size) {
    g_ssdBuffer.resize(size);
    return reinterpret_cast<uintptr_t>(g_ssdBuffer.data());
}

bool attachDatapakImage(unsigned slot, unsigned size) {
    if (!g_emu || size == 0) return false;
    return g_emu->attachDatapak(int(slot), g_ssdBuffer.data(), size);
}

void detachDatapakImage(unsigned slot) {
    if (g_emu) g_emu->detachDatapak(int(slot));
}

bool isDatapakAttached(unsigned slot) {
    return g_emu && g_emu->isDatapakInserted(int(slot));
}

unsigned getDatapakImageSize(unsigned slot) {
    return g_emu ? unsigned(g_emu->getDatapakImageSize(int(slot))) : 0;
}

void readDatapakImage(unsigned slot, uintptr_t bufferPtr) {
    if (!g_emu) return;
    const uint8_t *src = g_emu->getDatapakImageData(int(slot));
    size_t len = g_emu->getDatapakImageSize(int(slot));
    if (!src || len == 0) return;
    std::memcpy(reinterpret_cast<uint8_t *>(bufferPtr), src, len);
}

// Returns 0 (None / empty), 1 (Datapak / read-only), or 2 (Rampak /
// writable). The frontend renders a per-slot label based on this.
unsigned getDatapakKind(unsigned slot) {
    if (!g_emu) return 0;
    return unsigned(g_emu->getDatapakKind(int(slot)));
}

// Enables or disables emulator log emission at the WASM→JS boundary.
// When disabled (the default), the logger callback short-circuits before
// doing any UTF-8 decode or console.log call — a significant perf win
// because the core emits hundreds of log lines per sim-second during
// boot and driver activity, and browser devtools console.log is very
// expensive. The "Show Logs" UI toggle sets this to true while the log
// panel is open.
void setLoggingEnabled(bool enabled) {
    g_loggingEnabled = enabled;
    if (g_emu) g_emu->setLoggingEnabled(enabled);
}

// Debug-only: set a process environment variable from JS.  The emulator
// reads its PSION_* feature flags via getenv() (cached on first use), so
// this must be called BEFORE a device is loaded.  Used by the recording
// test harness to select the capture mode (e.g. PSION_REC_DMAIRQ=1)
// without rebuilding the WASM.
void setEnvVar(std::string name, std::string value) {
    setenv(name.c_str(), value.c_str(), 1);
}

// Debug-only: elapsed emulated cycles and the clock rate, so a test can
// compute how much SIM time passed during a window and divide a
// clock-independent counter (e.g. codrReads) by sim-seconds rather than
// wall-seconds — defeats any stepFrame-pacing inflation.
double getSimCycles() { return g_emu ? (double)g_emu->currentCycles() : 0.0; }
double getClockHz()   { return g_emu ? (double)g_emu->getClockSpeed() : 0.0; }

// True when the emulator is stuck in the 2 s-per-sector CF polling gap.
// The frontend checks this after each stepFrame and, while true, bursts
// additional stepFrames in the same RAF tick so the user-visible wall
// time of a CF operation compresses from seconds to milliseconds. Returns
// false when no emulator, no card attached, or the card is idle.
bool isCFPollGapActive() {
    return g_emu && g_emu->cfGapActive();
}

// Returns JSON array of all known device profiles for the frontend device picker.
std::string getAllDeviceProfilesJSON() {
    std::string out = "[";
    bool first = true;
    for (const DeviceProfile *p = allProfiles(); p->id != nullptr; ++p) {
        if (!first) out += ",";
        first = false;
        out += "{";
        out += "\"id\":\"";        out += p->id;           out += "\",";
        out += "\"displayName\":\""; out += p->displayName; out += "\",";
        out += "\"romFilename\":\""; out += p->romFilename; out += "\",";
        out += "\"skinFilename\":\""; out += (p->skinFilename ? p->skinFilename : ""); out += "\",";
        out += "\"status\":\"";
        out += (p->status == DeviceStatus::Supported ? "supported" : "coming-soon");
        out += "\",";
        out += "\"hasCFSlot\":";
        out += (p->hasCFSlot ? "true" : "false");
        out += ",";
        out += "\"ssdSlotCount\":";
        out += std::to_string(p->ssdSlotCount);
        out += ",";
        out += "\"datapakSlotCount\":";
        out += std::to_string(p->datapakSlotCount);
        out += ",";
        out += "\"remoteLinkUart\":";
        out += std::to_string(p->remoteLinkUart);
        out += ",";
        out += "\"infraredUart\":";
        out += std::to_string(p->infraredUart);
        out += ",";
        out += "\"linkProtocol\":";
        out += std::to_string(int(p->linkProtocol));
        out += ",";
        out += "\"irProtocol\":";
        out += std::to_string(int(p->irProtocol));
        out += "}";
    }
    out += "]";
    return out;
}

EMSCRIPTEN_BINDINGS(psion_emu) {
    emscripten::function("prepareROMUpload",         &prepareROMUpload);
    emscripten::function("loadBufferedROM",          &loadBufferedROM);
    emscripten::function("stepFrame",                &stepFrame);
    emscripten::function("stepFrameFull",            &stepFrameFull);
    emscripten::function("serialPumpCycles",         &serialPumpCycles);
    emscripten::function("readLCD",                  &readLCD);
    emscripten::function("getLcdDbar",               &getLcdDbar);
    emscripten::function("getLcdDbar2",              &getLcdDbar2);
    emscripten::function("getLcdLccr0",              &getLcdLccr0);
    emscripten::function("getLcdLccr1",              &getLcdLccr1);
    emscripten::function("getLcdLccr2",              &getLcdLccr2);
    emscripten::function("getLcdLccr3",              &getLcdLccr3);
    emscripten::function("peekRam",                  &peekRam);
    emscripten::function("getCpuPc",                 &getCpuPc);
    emscripten::function("getIcpr",                  &getIcpr);
    emscripten::function("getIcmr",                  &getIcmr);
    emscripten::function("getCpuLr",                 &getCpuLr);
    emscripten::function("getCpuWfi",                &getCpuWfi);
    emscripten::function("getRegFileAddr",           &getRegFileAddr);
    emscripten::function("getCpsrAddr",              &getCpsrAddr);
    emscripten::function("getCpuRealPc",             &getCpuRealPc);
    emscripten::function("tickCpu",                  &tickCpu);
    emscripten::function("getExcCount",              &getExcCount);
    emscripten::function("getWfiCount",              &getWfiCount);
    emscripten::function("getInsnCount",             &getInsnCount);
    emscripten::function("resetCpuCounters",         &resetCpuCounters);
    emscripten::function("getSwiHist",               &getSwiHist);
    emscripten::function("getSwiInsnEx",             &getSwiInsnEx);
    emscripten::function("getSwiPcEx",               &getSwiPcEx);
    emscripten::function("readVirt",                 &readVirt);
    emscripten::function("writeVirt",                &writeVirt);
    emscripten::function("setNbExcTrace",            &setNbExcTrace);
    emscripten::function("getDeviceInfo",            &getDeviceInfo);
    emscripten::function("sendKey",                  &sendKey);
    emscripten::function("sendTouch",                &sendTouch);
    emscripten::function("getBacklight",             &getBacklight);
    emscripten::function("isCpuCrashed",             &isCpuCrashed);
    emscripten::function("prepareCFImageUpload",     &prepareCFImageUpload);
    emscripten::function("attachCFImage",            &attachCFImage);
    emscripten::function("updateCFImageInPlace",      &updateCFImageInPlace);
    emscripten::function("detachCFImage",            &detachCFImage);
    emscripten::function("isCFImageAttached",        &isCFImageAttached);
    emscripten::function("getCFImageSize",           &getCFImageSize);
    emscripten::function("readCFImage",              &readCFImage);
    emscripten::function("getCfAtaCommands",         &getCfAtaCommands);
    emscripten::function("getCfSectorDrains",        &getCfSectorDrains);
    emscripten::function("getRamSnapshotSize",       &getRamSnapshotSize);
    emscripten::function("readRamSnapshot",          &readRamSnapshot);
    emscripten::function("serialAttachHost",         &serialAttachHost);
    emscripten::function("serialDetachHost",         &serialDetachHost);
    emscripten::function("serialIsAttached",         &serialIsAttached);
    emscripten::function("serialWriteFromHost",      &serialWriteFromHost);
    emscripten::function("serialReadToHost",         &serialReadToHost);
    emscripten::function("serialHostTxAvailable",    &serialHostTxAvailable);
    emscripten::function("prepareSSDImageUpload",    &prepareSSDImageUpload);
    emscripten::function("attachSSDImage",           &attachSSDImage);
    emscripten::function("detachSSDImage",           &detachSSDImage);
    emscripten::function("isSSDImageAttached",       &isSSDImageAttached);
    emscripten::function("getSSDImageSize",          &getSSDImageSize);
    emscripten::function("readSSDImage",             &readSSDImage);
    emscripten::function("prepareDatapakUpload",     &prepareDatapakUpload);
    emscripten::function("attachDatapakImage",       &attachDatapakImage);
    emscripten::function("detachDatapakImage",       &detachDatapakImage);
    emscripten::function("isDatapakAttached",        &isDatapakAttached);
    emscripten::function("getDatapakImageSize",      &getDatapakImageSize);
    emscripten::function("readDatapakImage",         &readDatapakImage);
    emscripten::function("getDatapakKind",           &getDatapakKind);
    emscripten::function("isCFPollGapActive",        &isCFPollGapActive);
    emscripten::function("setLoggingEnabled",        &setLoggingEnabled);
    emscripten::function("setEnvVar",                &setEnvVar);
    emscripten::function("getSimCycles",             &getSimCycles);
    emscripten::function("getClockHz",               &getClockHz);
    emscripten::function("getAllDeviceProfilesJSON", &getAllDeviceProfilesJSON);
    emscripten::function("readAudioOutput",          &readAudioOutput);
    emscripten::function("writeAudioInput",          &writeAudioInput);
    emscripten::function("setHostAudioEnabled",      &setHostAudioEnabled);
    emscripten::function("getAudioDiag",             &getAudioDiag);
    emscripten::function("debugForceChannelState",   &debugForceChannelState);
    emscripten::function("debugDacHistory",          &debugDacHistory);
    emscripten::function("debugPlayTestTone",        &debugPlayTestTone);

    emscripten::value_object<AudioDiag>("AudioDiag")
        .field("codrReads",      &AudioDiag::codrReads)
        .field("codrWrites",     &AudioDiag::codrWrites)
        .field("adcFill",        &AudioDiag::adcFill)
        .field("dacFill",        &AudioDiag::dacFill)
        .field("codecConfig",    &AudioDiag::codecConfig)
        .field("recordChannel",  &AudioDiag::recordChannel)
        .field("channelState",   &AudioDiag::channelState)
        .field("csintFired",     &AudioDiag::csintFired)
        .field("txDrainTicks",   &AudioDiag::txDrainTicks)
        .field("txFifoLevel",    &AudioDiag::txFifoLevel)
        .field("recActive",      &AudioDiag::recActive)
        .field("recPosValAddr",  &AudioDiag::recPosValAddr)
        .field("recClipSamples", &AudioDiag::recClipSamples)
        .field("recClipState",   &AudioDiag::recClipState)
        .field("isNetBook",      &AudioDiag::isNetBook)
        .field("dacPeakToPeak",  &AudioDiag::dacPeakToPeak)
        .field("dacFreqHz",      &AudioDiag::dacFreqHz);

    emscripten::value_object<DeviceInfo>("DeviceInfo")
        .field("digitiserWidth",  &DeviceInfo::digitiserWidth)
        .field("digitiserHeight", &DeviceInfo::digitiserHeight)
        .field("lcdOffsetX",      &DeviceInfo::lcdOffsetX)
        .field("lcdOffsetY",      &DeviceInfo::lcdOffsetY)
        .field("lcdWidth",        &DeviceInfo::lcdWidth)
        .field("lcdHeight",       &DeviceInfo::lcdHeight)
        .field("deviceName",      &DeviceInfo::deviceName)
        .field("hasAudio",        &DeviceInfo::hasAudio)
        .field("hasMic",          &DeviceInfo::hasMic)
        .field("audioSampleRate", &DeviceInfo::audioSampleRate);
}
