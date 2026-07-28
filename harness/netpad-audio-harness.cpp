// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Native audio harness for the Psion netpad (SA-1110 + AC'97 codec on
// the board FPGA).
//
// The netpad's sound hardware is five halfword registers at nCS4 0x200
// (VA 0x58030200).  Three pieces of the shipped ROM drive them and they
// agree on the map:
//
//   * the ASSP accessors inside EKern.exe — control read-modify-write
//     at ROM 0x50005118, status at 0x50005140, codec register write at
//     0x50005158, codec register read at 0x50005180 / 0x50005194, and
//     the PCM sample write / read at 0x500051a0 / 0x500051b0;
//   * \System\Libs\Esdrv.pdd ("Sound.Ac97"), the sound PDD, whose
//     record path is at ROM 0x502e7ac8 and play path at 0x502e795c;
//   * \System\Libs\d_ac97.ldd ("Codec.Ac97"), whose straight-line
//     Play / Record loops are at ROM 0x5036c8b8 / 0x5036c938.
//
// This harness boots the real ROM and then performs, over the emulated
// bus, the same register sequence Esdrv.pdd's record path performs —
// power-up, codec configuration, receive-FIFO flush, then the
// poll-status / read-sample loop — with a known tone pushed in through
// the host microphone bridge.  If the samples the guest side of the bus
// reads back are the tone, the microphone works for any driver that
// follows the ROM's own contract.
// The playback direction shares the same FIFO register, so it is
// checked the same way in the other direction.
//
// Build:  bash harness/build.sh
// Usage:  ./netpad-audio-harness [<netpad.img>]   (default roms/Netpad.img)
//
// Exit codes (distinct so CI can bisect):
//   0  microphone and speaker paths both verified
//   1  usage / build / ROM error
//   2  boot failure (no device profile, ROM would not load)
//   3  codec never reported ready after the power-up sequence
//   4  receive FIFO never filled — no samples reached the guest at all
//   5  samples arrived but did not match the tone that was pushed in
//   6  the record FIFO service interrupt never reached the FPGA
//   7  playback samples written to the FIFO never reached the host

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/sa1100.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

EmuBase *g_emu = nullptr;
SA1100::Emulator *g_np = nullptr;
bool g_sawServiceIrq = false;
bool g_echoTrace = false;

// The emulator's own AC'97 trace is how the service interrupt is
// observed: the raise sets a bit in the FPGA's interrupt status
// register, and the netpad's ASSP interrupt handler acknowledges it by
// writing the bit straight back (ROM 0x500a6fac), so polling the
// register from outside races the guest and usually loses.
void captureLog(const char *line) {
    if (!std::strstr(line, "[np-ac97]")) return;
    if (g_echoTrace) std::fprintf(stderr, "  %s\n", line);
    if (std::strstr(line, "service irq")) g_sawServiceIrq = true;
}

// nCS4 physical base and the AC'97 block's registers within it.  The
// guest reaches the same locations through VA 0x58030200.
constexpr uint32_t kNcs4Base   = 0x40000000u;
constexpr uint32_t kRegData    = 0x200;
constexpr uint32_t kRegIndex   = 0x202;
constexpr uint32_t kRegValue   = 0x204;
constexpr uint32_t kRegCtrl    = 0x206;
constexpr uint32_t kRegStatus  = 0x208;
constexpr uint32_t kFpgaIrqStatus = 0x10;
constexpr uint32_t kFpgaIrqEnable = 0x12;

constexpr uint16_t kStatRxEmpty = 0x0001;
constexpr uint16_t kStatTxFull  = 0x0010;
constexpr uint16_t kStatBusy    = 0x0080;

constexpr int kSampleRate = 8000;

std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", p);
        std::exit(1);
    }
    return { std::istreambuf_iterator<char>(f), {} };
}

uint16_t rd(uint32_t reg) {
    auto v = g_np->readPhysical(kNcs4Base + reg, ARM710::V16);
    return (uint16_t)(v.value_or(0) & 0xFFFFu);
}

void wr(uint32_t reg, uint16_t value) {
    g_np->writePhysical(value, kNcs4Base + reg, ARM710::V16);
}

// The ROM's codec accessors: stage the value at 0x204 and commit it with
// an even index (ROM 0x50005158), or write index|1 and collect the
// answer from 0x204 (0x50005180 / 0x50005194).  Both wait out the busy
// bit afterwards, which is what the loop here mirrors.
void codecWrite(uint8_t index, uint16_t value) {
    wr(kRegValue, value);
    wr(kRegIndex, (uint16_t)(index & 0x7E));
    for (int i = 0; i < 64 && (rd(kRegStatus) & kStatBusy); i++) {}
}

uint16_t codecRead(uint8_t index) {
    wr(kRegIndex, (uint16_t)((index & 0x7E) | 1));
    for (int i = 0; i < 64 && (rd(kRegStatus) & kStatBusy); i++) {}
    return rd(kRegValue);
}

void runFor(double seconds) {
    const int64_t clock = g_emu->getClockSpeed();
    const int64_t frameCycles = clock / 64;
    const int frames = (int)(seconds * 64);
    for (int i = 0; i < frames; i++)
        g_emu->executeUntil(g_emu->currentCycles() + frameCycles);
}

void runMicros(int us) {
    const int64_t clock = g_emu->getClockSpeed();
    g_emu->executeUntil(g_emu->currentCycles() + (int64_t)clock * us / 1000000);
}

// Esdrv.pdd's power-up sequence (ROM 0x502e7bd0): enable the AC-link,
// drop the codec's reset, raise it again, then spin until the codec's
// powerdown register reports all four sections ready.
bool powerUpCodec() {
    wr(kRegCtrl, 0x0000);
    wr(kRegCtrl, 0x0002);
    wr(kRegCtrl, 0x0000);
    wr(kRegCtrl, 0x0003);
    for (int i = 0; i <= 9; i++)
        if ((codecRead(0x26) & 0x0F) == 0x0F) return true;
    return false;
}

// Esdrv.pdd's record configuration (ROM 0x502e7ac8): mute every
// playback path, turn on variable-rate audio, set the ADC rate, select
// the microphone as the record source and program the two gains.
void configureRecord() {
    codecWrite(0x02, 0x8000);                       // master volume, muted
    codecWrite(0x0A, 0x8000);                       // PC beep, muted
    codecWrite(0x18, 0x8000);                       // PCM out, muted
    codecWrite(0x2A, (uint16_t)(codecRead(0x2A) | 1));   // VRA on
    codecWrite(0x32, (uint16_t)kSampleRate);        // PCM L/R ADC rate
    codecWrite(0x1A, 0x0000);                       // record select = mic
    codecWrite(0x20, (uint16_t)(codecRead(0x20) & ~0x0200));  // mic 1
    codecWrite(0x0E, 0x0008);                       // mic volume
    codecWrite(0x1C, 0x0000);                       // record gain
    // Flush the receive FIFO, as the PDD does at ROM 0x502e7ba0.
    wr(kRegCtrl, (uint16_t)(rd(kRegCtrl) | 0x0004));
    wr(kRegCtrl, (uint16_t)(rd(kRegCtrl) & ~0x0004));
}

// Esdrv.pdd's play configuration (ROM 0x502e795c).
void configurePlay() {
    codecWrite(0x0A, 0x8000);                       // PC beep, muted
    codecWrite(0x18, 0x0600);                       // PCM out volume
    codecWrite(0x2A, (uint16_t)(codecRead(0x2A) | 1));   // VRA on
    codecWrite(0x2C, (uint16_t)kSampleRate);        // PCM front DAC rate
    codecWrite(0x02, 0x0000);                       // master volume, full
}

// One sample of the 500 Hz test tone, as the host microphone would
// deliver it.
int16_t toneSample(int n) {
    return (int16_t)(std::lround(20000.0 * std::sin(2.0 * M_PI * 500.0 * n / kSampleRate)));
}

}  // namespace

int main(int argc, char **argv) {
    const char *romPath = (argc > 1) ? argv[1] : "roms/Netpad.img";
    // The harness needs the trace on to see the service interrupt; echo
    // the lines only when the caller asked for them as well.
    g_echoTrace = std::getenv("PSION_NETPAD_AC97_TRACE") != nullptr;
    setenv("PSION_NETPAD_AC97_TRACE", "1", 0);

    const DeviceProfile *profile = findProfileById("netpad");
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "FAIL: no 'netpad' device profile\n");
        return 2;
    }
    g_emu = profile->createEmulator();
    g_np = dynamic_cast<SA1100::Emulator *>(g_emu);
    if (!g_np) {
        std::fprintf(stderr, "FAIL: netpad profile is not an SA-11x0 emulator\n");
        return 2;
    }
    g_emu->setLogger(captureLog);
    if (auto *c = g_emu->getArmCpu()) c->setLoggingEnabled(true);
    std::vector<uint8_t> rom = readFile(romPath);
    g_emu->loadROM(rom.data(), rom.size());
    if (!g_emu->hasAudio() || !g_emu->hasMicrophone()) {
        std::fprintf(stderr, "FAIL: netpad reports hasAudio=%d hasMicrophone=%d\n",
                     g_emu->hasAudio() ? 1 : 0, g_emu->hasMicrophone() ? 1 : 0);
        return 2;
    }

    // Boot to the desktop.  The variant's own codec bring-up runs during
    // this window and leaves the codec powered down, exactly the state a
    // recording has to recover from.
    std::fprintf(stderr, "booting %s ...\n", romPath);
    runFor(16.0);
    std::fprintf(stderr, "boot done at cycle %lld\n",
                 (long long)g_emu->currentCycles());

    g_emu->setHostAudioEnabled(true, true);

    // ── Record ───────────────────────────────────────────────────────
    if (!powerUpCodec()) {
        std::fprintf(stderr, "FAIL: codec never reported ready "
                             "(powerdown reg = %04x)\n", codecRead(0x26));
        return 3;
    }
    std::fprintf(stderr, "PASS: codec powered up, ready status %04x\n",
                 codecRead(0x26));
    configureRecord();
    if (codecRead(0x32) != kSampleRate) {
        std::fprintf(stderr, "FAIL: ADC rate read back %u, wrote %d\n",
                     codecRead(0x32), kSampleRate);
        return 3;
    }

    // Push the tone the way the browser's AudioWorklet does — a chunk
    // per host callback — and drain the FIFO the way the ROM's record
    // path does: wait for the RX-empty bit to clear, then read the data
    // register.  The FIFO is 16 deep, so drain every half-millisecond
    // rather than once per chunk — comfortably inside the eight samples
    // per service request the driver's own record ISR reads, and a
    // lazier reader would simply overrun the FIFO as it would on
    // hardware.
    std::vector<int16_t> pushed, got;
    const int kChunk = 160;                 // 20 ms at 8 kHz
    for (int chunk = 0; chunk < 40; chunk++) {
        int16_t buf[kChunk];
        for (int i = 0; i < kChunk; i++) {
            buf[i] = toneSample((int)pushed.size());
            pushed.push_back(buf[i]);
        }
        g_emu->writeAudioInput(buf, kChunk);
        for (int slice = 0; slice < 40; slice++) {
            runMicros(500);
            for (int guard = 0; guard < 32; guard++) {
                if (rd(kRegStatus) & kStatRxEmpty) break;
                got.push_back((int16_t)rd(kRegData));
            }
        }
    }
    size_t nonSilent = 0;
    for (int16_t v : got) if (v) nonSilent++;
    std::fprintf(stderr, "pushed %zu host samples, guest read %zu (%zu non-silent)\n",
                 pushed.size(), got.size(), nonSilent);
    if (got.empty()) {
        std::fprintf(stderr, "FAIL: receive FIFO never produced a sample\n");
        return 4;
    }

    // The ADC keeps clocking whether or not the host has samples ready,
    // so the stream starts with the silence that was in flight before
    // the first chunk landed.  Past that the tone has to come through
    // in order and unaltered — the AC-link carries whole 16-bit
    // samples, and nothing between the worklet and the data register
    // is entitled to rescale or reorder them.
    size_t matched = 0, at = 0;
    for (size_t i = 0; i + 1 < got.size(); i++) {
        if (got[i] == 0) continue;
        for (size_t j = 0; j < pushed.size(); j++) {
            if (pushed[j] != got[i]) continue;
            size_t k = 0;
            while (i + k < got.size() && j + k < pushed.size() &&
                   pushed[j + k] == got[i + k]) k++;
            if (k > matched) { matched = k; at = i; }
            break;
        }
        if (matched >= 256) break;
    }
    if (matched < 256) {
        std::fprintf(stderr, "FAIL: no contiguous run of the pushed tone in "
                             "the samples read back (longest %zu at %zu)\n",
                     matched, at);
        std::fprintf(stderr, "  first 8 read:   ");
        for (size_t i = 0; i < got.size() && i < 8; i++)
            std::fprintf(stderr, "%d ", got[i]);
        std::fprintf(stderr, "\n  first 8 pushed: ");
        for (size_t i = 0; i < pushed.size() && i < 8; i++)
            std::fprintf(stderr, "%d ", pushed[i]);
        std::fprintf(stderr, "\n");
        return 5;
    }
    std::fprintf(stderr, "PASS: host microphone reaches the guest — %zu "
                         "consecutive samples match the pushed tone\n", matched);

    // Delivery rate: the FIFO is clocked by the codec's own rate
    // register, so a second of host audio arrives as a second of
    // samples rather than as fast as the guest can read.
    const double ratio = (double)got.size() / (double)pushed.size();
    if (ratio < 0.9 || ratio > 1.1) {
        std::fprintf(stderr, "FAIL: delivered %.0f%% of the pushed samples — "
                             "the FIFO is not running at the codec rate\n",
                     ratio * 100.0);
        return 5;
    }
    std::fprintf(stderr, "PASS: delivered %.0f%% of the pushed samples "
                         "(codec rate honoured)\n", ratio * 100.0);

    // ── Play ─────────────────────────────────────────────────────────
    // The same data register in the other direction.  Drain whatever the
    // record leg left in the host output ring first, then push a ramp
    // through and check it comes out.
    {
        int16_t sink[4096];
        while (g_emu->readAudioOutput(sink, 4096) > 0) {}
    }
    if (!powerUpCodec()) {
        std::fprintf(stderr, "FAIL: codec would not power up for playback\n");
        return 3;
    }
    configurePlay();
    std::vector<int16_t> out;
    for (int chunk = 0; chunk < 20; chunk++) {
        for (int i = 0; i < 16; i++) {
            if (rd(kRegStatus) & kStatTxFull) break;
            wr(kRegData, (uint16_t)toneSample(chunk * 16 + i));
        }
        runFor(0.02);
        int16_t sink[4096];
        size_t n = g_emu->readAudioOutput(sink, 4096);
        for (size_t i = 0; i < n; i++) out.push_back(sink[i]);
    }
    size_t nonZero = 0;
    for (int16_t s : out) if (s != 0) nonZero++;
    std::fprintf(stderr, "playback: %zu samples out of the host ring, "
                         "%zu non-silent\n", out.size(), nonZero);
    if (nonZero < 16) {
        std::fprintf(stderr, "FAIL: samples written to the FIFO never "
                             "reached the host speaker\n");
        return 7;
    }
    std::fprintf(stderr, "PASS: guest playback samples reach the host speaker\n");

    // ── FIFO service interrupt ───────────────────────────────────────
    // The sound PDD waits on a variant interrupt line rather than
    // polling, so check the controller raises one.  Nothing in the ROM
    // has a handler bound to these lines during an ordinary session, so
    // arm it, let the receive FIFO fill past its eight-sample service
    // level, then acknowledge and disarm before the guest runs again.
    powerUpCodec();
    configureRecord();
    wr(kFpgaIrqEnable, (uint16_t)(rd(kFpgaIrqEnable) | (1u << 4)));
    {
        int16_t buf[80];
        for (int i = 0; i < 80; i++) buf[i] = toneSample(i);
        g_emu->writeAudioInput(buf, 80);
    }
    g_sawServiceIrq = false;
    runMicros(4000);
    wr(kFpgaIrqEnable, (uint16_t)(rd(kFpgaIrqEnable) & ~((1u << 3) | (1u << 4))));
    wr(kFpgaIrqStatus, (uint16_t)((1u << 3) | (1u << 4)));
    if (!g_sawServiceIrq) {
        std::fprintf(stderr, "FAIL: the record FIFO never raised its service "
                             "interrupt\n");
        return 6;
    }
    std::fprintf(stderr, "PASS: record FIFO raised its FPGA service interrupt\n");

    std::fprintf(stderr, "netpad audio: OK\n");
    return 0;
}
