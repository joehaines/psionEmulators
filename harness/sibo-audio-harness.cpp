// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// SIBO audio harness: validates the ASIC9 A-law codec path (Series 3a /
// 3c / 3mx / Siena / PB2) at three tiers.
//
//   A. A-law unit tests — SiboAudio::alawExpand/alawCompress round-trip
//      all 256 codec bytes and pin the silence byte. No ROM needed.
//   B. Chip-level codec test — standalone PsionAsic9 + SiboAudio wired
//      like core/series3c.cpp. Drives A9WControl/A9WControlExtra/
//      A9BSoundData directly and asserts FIFO movement, A9IntSnd and
//      the expanded waveform on both the playback and capture sides.
//      No ROM needed, fully deterministic.
//   C. ROM-level characterisation (optional) — boot a SIBO2 ROM with a
//      scripted key sequence, feed a mic tone, and report (or assert,
//      with --require-pcm-*) the Series3c::Emulator codec counters.
//      This is what tells us whether a given kernel actually reaches
//      A9MSoundEnable under our emulation.
//
// Build:  bash harness/build.sh
// Usage:  ./harness/sibo-audio-harness                       # tiers A+B
//         ./harness/sibo-audio-harness <rom> [--device id]
//             [--boot-seconds N] [--press-key AT_S CODE HOLD_FRAMES]...
//             (same --press-key argument order as harness/run.cpp)
//             [--mic-tone-at SEC] [--run-seconds N]
//             [--require-pcm-out N] [--require-pcm-in N]   # tier C
//
// Exit codes:
//   0 pass   1 usage   2 A-law fail   3 chip playback fail
//   4 chip capture fail   5 ROM boot/profile fail
//   6 --require-pcm-out unmet   7 --require-pcm-in unmet

#include "../core/device_registry.h"
#include "../core/emubase.h"
#include "../core/psion_asic9.h"
#include "../core/series3c.h"
#include "../core/sibo_audio.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// ── Tier A: A-law unit tests ───────────────────────────────────────────

static bool tierAlaw() {
    std::fprintf(stderr, "=== Tier A: A-law unit tests ===\n");
    // Round-trip: every codec byte expands to a value that compresses
    // back to the same byte (the expand output grid is exactly the set
    // of representable A-law levels, so this must be lossless).
    for (int b = 0; b < 256; b++) {
        int16_t lin = SiboAudio::alawExpand(uint8_t(b));
        uint8_t back = SiboAudio::alawCompress(lin);
        if (back != uint8_t(b)) {
            std::fprintf(stderr, "FAIL: byte %02x -> %d -> %02x (round-trip broke)\n",
                         b, lin, back);
            return false;
        }
    }
    // Silence: compressing 0 must give the A-law positive-zero byte the
    // codec idles on (0x55 in MAME's convention — seg 0, mantissa 0,
    // positive sign), and it must expand to the smallest magnitude.
    uint8_t silence = SiboAudio::alawCompress(0);
    if (silence != 0x55) {
        std::fprintf(stderr, "FAIL: alawCompress(0) = %02x, want 55\n", silence);
        return false;
    }
    if (SiboAudio::alawExpand(silence) != 8) {
        std::fprintf(stderr, "FAIL: alawExpand(0x55) = %d, want 8 (min magnitude x8)\n",
                     SiboAudio::alawExpand(silence));
        return false;
    }
    // Sign symmetry and full-scale: 0x2A is 0x7F^0x55 (max positive),
    // 0xAA its negative twin; both must hit ±4032×8 = ±32256.
    if (SiboAudio::alawExpand(0x2A) != 32256 || SiboAudio::alawExpand(0xAA) != -32256) {
        std::fprintf(stderr, "FAIL: full-scale expand %d / %d, want 32256 / -32256\n",
                     SiboAudio::alawExpand(0x2A), SiboAudio::alawExpand(0xAA));
        return false;
    }
    // Monotonicity over positive codes: bigger A-law magnitude codes
    // must expand to strictly bigger linear values.
    int16_t prev = 0;
    for (int code = 0; code < 128; code++) {
        // Positive codes pre-XOR are 0x00..0x7F with bit7 clear; on the
        // wire they appear as code^0x55.
        int16_t v = SiboAudio::alawExpand(uint8_t(code) ^ 0x55);
        if (code > 0 && v <= prev) {
            std::fprintf(stderr, "FAIL: expand not monotonic at code %d (%d <= %d)\n",
                         code, v, prev);
            return false;
        }
        prev = v;
    }
    std::fprintf(stderr, "PASS: 256-byte round-trip, silence byte 0x55, "
                         "full scale +/-32256, monotonic\n");
    return true;
}

// ── Tier B: chip-level ASIC9 codec ─────────────────────────────────────

// Mirrors the series3c.cpp wiring: pcm_out pushes raw A-law bytes into
// the SiboAudio DAC ring, pcm_in pops A-law bytes from the ADC ring.
struct ChipBench {
    PsionAsic9 asic9;
    SiboAudio  audio;
    uint64_t   pcmOut = 0, pcmIn = 0;
    std::vector<uint8_t> outBytes;  // every byte the codec played

    static constexpr int64_t kClock = 7'680'000;
    static constexpr int64_t kCyclesPerSample = kClock / 8000;  // 960

    ChipBench() {
        asic9.setBusClock(kClock);
        audio.configure(int(kClock));
        audio.setHostEnabled(true, true);
        asic9.setPcmOut([this](uint8_t s) {
            pcmOut++;
            outBytes.push_back(s);
            audio.pushPcmSample(s);
        });
        asic9.setPcmIn([this]() -> uint8_t {
            auto r = audio.popPcmSample();
            if (r.valid) pcmIn++;
            return r.sample;
        });
        asic9.reset();
    }

    void enableSound(bool playbackDir) {
        // A9WControlExtra bit 7 = A9MSoundDir (1 = playback).
        asic9.ioWrite(0x2c, playbackDir ? 0x0080 : 0x0000, 0xFFFF);
        // A9WControl bit 11 = A9MSoundEnable.
        asic9.ioWrite(0x02, 0x0800, 0xFFFF);
    }
};

static bool tierChipPlayback() {
    std::fprintf(stderr, "\n=== Tier B1: chip-level playback ===\n");
    ChipBench b;
    b.enableSound(true);

    // Push a recognisable 16-byte A-law ramp into the sound FIFO the way
    // the kernel's ISR does (byte writes to A9BSoundData).
    std::vector<uint8_t> ramp;
    for (int i = 0; i < 16; i++)
        ramp.push_back(SiboAudio::alawCompress(int16_t((i - 8) * 2048)));
    for (uint8_t v : ramp) b.asic9.ioWrite(0x1a, v, 0x00FF);

    // 16 entries = hardware FIFO full: A9MFifoFull (status bit 0x0800).
    if (!(b.asic9.status() & 0x0800)) {
        std::fprintf(stderr, "FAIL: FIFO full bit not set after 16 writes\n");
        return false;
    }

    // Run the Snd timer long enough to drain all 16 samples at 8 kHz.
    b.asic9.tick(17 * ChipBench::kCyclesPerSample);

    if (b.pcmOut != 16) {
        std::fprintf(stderr, "FAIL: codec played %llu bytes, want 16\n",
                     (unsigned long long)b.pcmOut);
        return false;
    }
    if (b.outBytes != ramp) {
        std::fprintf(stderr, "FAIL: played bytes differ from the queued ramp\n");
        return false;
    }
    if (b.asic9.status() & 0x0800) {
        std::fprintf(stderr, "FAIL: FIFO full bit still set after drain\n");
        return false;
    }
    if (!(b.asic9.intStatus() & 0x01)) {  // A9IntSnd
        std::fprintf(stderr, "FAIL: A9IntSnd not pending after Snd fires\n");
        return false;
    }
    // Sound EOI clears the interrupt.
    b.asic9.ioWrite(0x1c, 0x00, 0x00FF);
    if (b.asic9.intStatus() & 0x01) {
        std::fprintf(stderr, "FAIL: A9BSoundEoi did not clear A9IntSnd\n");
        return false;
    }
    // The host-facing DAC ring must contain the A-law-expanded ramp.
    std::vector<int16_t> got(64);
    size_t n = b.audio.readAudioOutput(got.data(), got.size());
    if (n != 16) {
        std::fprintf(stderr, "FAIL: DAC ring yielded %zu samples, want 16\n", n);
        return false;
    }
    for (int i = 0; i < 16; i++) {
        if (got[i] != SiboAudio::alawExpand(ramp[i])) {
            std::fprintf(stderr, "FAIL: sample %d = %d, want %d\n",
                         i, got[i], SiboAudio::alawExpand(ramp[i]));
            return false;
        }
    }
    std::fprintf(stderr, "PASS: 16-byte ramp played, FIFO/IRQ/EOI sequence correct, "
                         "DAC ring matches A-law expansion\n");
    return true;
}

static bool tierChipCapture() {
    std::fprintf(stderr, "\n=== Tier B2: chip-level capture ===\n");
    ChipBench b;

    // Feed one second of 440 Hz mic tone into the host-side ADC ring.
    std::vector<int16_t> tone(8000);
    for (size_t i = 0; i < tone.size(); i++)
        tone[i] = int16_t(std::sin(2.0 * M_PI * 440.0 * double(i) / 8000.0) * 16000.0);
    b.audio.enqueueMicSamples(tone.data(), tone.size());

    b.enableSound(false);  // capture direction

    // 16 codec fires fill the hardware FIFO exactly.
    b.asic9.tick(16 * ChipBench::kCyclesPerSample);
    if (b.pcmIn != 16) {
        std::fprintf(stderr, "FAIL: codec captured %llu bytes, want 16\n",
                     (unsigned long long)b.pcmIn);
        return false;
    }
    // Capture-direction status semantics (pinned from the 3c kernel's
    // CSINT drain loop): bit 0x0800 means "FIFO empty / stop reading",
    // so it must be CLEAR while captured data is waiting...
    if (b.asic9.status() & 0x0800) {
        std::fprintf(stderr, "FAIL: capture stop-bit set while FIFO has data\n");
        return false;
    }
    // Drain the FIFO the way the kernel ISR does and check each byte is
    // the A-law compression of the fed tone, in order.
    for (int i = 0; i < 16; i++) {
        uint8_t v = uint8_t(b.asic9.ioRead(0x1a, 0x00FF));
        uint8_t want = SiboAudio::alawCompress(tone[size_t(i)]);
        if (v != want) {
            std::fprintf(stderr, "FAIL: captured byte %d = %02x, want %02x\n",
                         i, v, want);
            return false;
        }
    }
    // ...and SET once the drain empties it (this is what terminates the
    // kernel's read loop).
    if (!(b.asic9.status() & 0x0800)) {
        std::fprintf(stderr, "FAIL: capture stop-bit not set after drain\n");
        return false;
    }
    // Backlog replay: let 24 fires elapse before any drain — 16 fill the
    // FIFO, 8 are "missed" (FIFO full). The drain must still deliver all
    // 24 samples in order (no loss from CSINT latency) and only then
    // raise the stop bit.
    b.asic9.tick(24 * ChipBench::kCyclesPerSample);
    for (int i = 0; i < 24; i++) {
        uint8_t v = uint8_t(b.asic9.ioRead(0x1a, 0x00FF));
        uint8_t want = SiboAudio::alawCompress(tone[size_t(16 + i)]);
        if (v != want) {
            std::fprintf(stderr, "FAIL: backlog byte %d = %02x, want %02x\n",
                         i, v, want);
            return false;
        }
    }
    if (!(b.asic9.status() & 0x0800)) {
        std::fprintf(stderr, "FAIL: stop-bit not set after backlog drain\n");
        return false;
    }
    // With the mic ring exhausted the codec must capture the A-law
    // silence byte, not a DC ramp.
    ChipBench quiet;
    quiet.enableSound(false);
    quiet.asic9.tick(2 * ChipBench::kCyclesPerSample);
    uint8_t idleByte = uint8_t(quiet.asic9.ioRead(0x1a, 0x00FF));
    if (idleByte != SiboAudio::alawCompress(0)) {
        std::fprintf(stderr, "FAIL: idle capture byte %02x, want %02x\n",
                     idleByte, SiboAudio::alawCompress(0));
        return false;
    }
    std::fprintf(stderr, "PASS: 440 Hz tone captured as A-law in order, "
                         "idle mic captures silence byte\n");
    return true;
}

// ── Tier C: ROM-level characterisation ─────────────────────────────────

struct KeyPress { int code; double atSec; int holdFrames; };

static std::vector<uint8_t> readFile(const char *p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", p); std::exit(5); }
    return { std::istreambuf_iterator<char>(f), {} };
}

static int tierRom(const char *romPath, const char *deviceId,
                   double bootSeconds, double runSeconds, double micToneAt,
                   const std::vector<KeyPress> &keys,
                   long long requirePcmOut, long long requirePcmIn) {
    auto rom = readFile(romPath);
    const DeviceProfile *profile = nullptr;
    if (deviceId) {
        profile = findProfileById(deviceId);
    } else {
        profile = findProfileByVariant(detectROMVariant(rom.data(), rom.size()));
    }
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "FAIL: no device profile (use --device)\n");
        return 5;
    }
    EmuBase *emu = profile->createEmulator();
    emu->loadROM(rom.data(), rom.size());
    auto *s3c = dynamic_cast<Series3c::Emulator *>(emu);
    if (!s3c) {
        std::fprintf(stderr, "FAIL: %s is not a SIBO2 (Series3c-family) device\n",
                     profile->id);
        return 5;
    }
    emu->setHostAudioEnabled(true, true);

    const int64_t clock = emu->getClockSpeed();
    const int64_t frameCycles = clock / 64;
    const double totalSeconds = bootSeconds + runSeconds;
    const int totalFrames = int(totalSeconds * 64);

    std::fprintf(stderr, "=== Tier C: booting %s for %.0fs + %.0fs run ===\n",
                 profile->displayName, bootSeconds, runSeconds);

    // Mic tone state: a 440 Hz sine fed in 1-frame chunks once simTime
    // passes micToneAt (continues until the end of the run).
    const int sr = emu->getAudioSampleRate();
    const size_t chunkSamples = size_t(sr / 64);
    std::vector<int16_t> chunk(chunkSamples, 0);
    double phase = 0.0;
    const double phaseStep = 2.0 * M_PI * 440.0 / sr;

    std::vector<bool> pressed(keys.size(), false);
    std::vector<bool> released(keys.size(), false);
    std::vector<int> releaseFrame(keys.size(), 0);

    // Drain the DAC ring as the host audio engine would, tracking
    // whether any non-buzzer-shaped PCM ever shows up.
    std::vector<int16_t> sink(4096);

    // CS:IP histogram over the final two seconds (sampled at frame
    // boundaries) — cheap "where is the kernel sitting" diagnostic.
    std::map<uint32_t, int> pcHist;
    const int histFromFrame = totalFrames - 128;

    for (int frame = 0; frame < totalFrames; frame++) {
        double now = double(frame) / 64.0;
        for (size_t k = 0; k < keys.size(); k++) {
            if (!pressed[k] && now >= keys[k].atSec) {
                emu->setKeyboardKey(EpocKey(keys[k].code), true);
                pressed[k] = true;
                releaseFrame[k] = frame + keys[k].holdFrames;
                std::fprintf(stderr, "  [%.1fs] press key %d (hold %d frames)\n",
                             now, keys[k].code, keys[k].holdFrames);
            }
            if (pressed[k] && !released[k] && frame >= releaseFrame[k]) {
                emu->setKeyboardKey(EpocKey(keys[k].code), false);
                released[k] = true;
            }
        }
        if (now >= micToneAt) {
            for (size_t i = 0; i < chunkSamples; i++) {
                chunk[i] = int16_t(std::sin(phase) * 16000.0);
                phase += phaseStep;
                if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
            }
            emu->writeAudioInput(chunk.data(), chunk.size());
        }
        emu->executeUntil(emu->currentCycles() + frameCycles);
        emu->readAudioOutput(sink.data(), sink.size());
        if (frame >= histFromFrame) {
            const V30 *cpu = emu->getV30Cpu();
            pcHist[(uint32_t(cpu->sregs[1]) << 16) | cpu->ip]++;
        }
        // Sound-driver kernel variables (DS=0x4b0): duration reload
        // [0x2234], countdown [0x2238], units left [0x1c2a], state
        // [0x2241], waiter flag [0x1c24]. Offsets pinned from the
        // v5.20f CSINT handler disassembly (a000:8d86..8f00).
        if ((frame & 31) == 31 && std::getenv("PSION_SND_TRACE")) {
            auto w16 = [&](uint32_t lin) { return s3c->readMemWord(lin); };
            static uint16_t last2241 = 0xFFFF;
            uint16_t st = s3c->readMemByte(0x4B00 + 0x2241);
            if (st != last2241 || (now >= 55.0 && now <= 59.0)) {
                std::fprintf(stderr,
                    "  [%5.2fs] drv state=%u wait=%u wake[43c]=%u "
                    "standby[15ec]=%04x ctl=%04x\n",
                    now, st, s3c->readMemByte(0x4B00 + 0x1c24),
                    s3c->readMemByte(0x4B00 + 0x043c),
                    w16(0x4B00 + 0x15ec), s3c->debugAsic9().control());
                last2241 = st;
            }
        }
        // Once-per-second codec service trace so stalls show up with a
        // timestamp (reads/writes/EOIs are cumulative counters).
        if ((frame & 63) == 63) {
            const PsionAsic9 &a9 = s3c->debugAsic9();
            static uint64_t lastReads = 0, lastEois = 0, lastIn = 0;
            uint64_t r = a9.sndDataReads(), e = a9.sndEoiWrites(),
                     in = s3c->debugPcmInCount();
            if (r != lastReads || e != lastEois || in != lastIn) {
                std::fprintf(stderr,
                    "  [%2.0fs] sndReads=%llu (+%llu) eois=%llu (+%llu) pcmIn=%llu "
                    "fifo=%d istat=%02x imask=%02x iflag=%d\n",
                    now, (unsigned long long)r, (unsigned long long)(r - lastReads),
                    (unsigned long long)e, (unsigned long long)(e - lastEois),
                    (unsigned long long)in, a9.sndFifoFill(), a9.intStatus(),
                    a9.intMask(), emu->getV30Cpu()->iflag);
                lastReads = r; lastEois = e; lastIn = in;
            }
        }
    }

    uint64_t pcmOut = s3c->debugPcmOutCount();
    uint64_t pcmIn  = s3c->debugPcmInCount();
    std::fprintf(stderr, "\n=== Tier C result (%s) ===\n", profile->id);
    {
        const PsionAsic9 &a9 = s3c->debugAsic9();
        const V30 *cpu = emu->getV30Cpu();
        std::fprintf(stderr,
            "   asic9: ctl=%04x extra=%04x status=%04x imask=%02x istat=%02x sndFifo=%d\n",
            a9.control(), a9.controlExtra(), a9.status(), a9.intMask(),
            a9.intStatus(), a9.sndFifoFill());
        std::fprintf(stderr,
            "   cpu:   halted=%d iflag=%d cs:ip=%04x:%04x\n",
            cpu->halted ? 1 : 0, cpu->iflag, cpu->sregs[1], cpu->ip);
        std::fprintf(stderr, "   cs:ip histogram (last 2s, top 8):\n");
        std::vector<std::pair<int, uint32_t>> top;
        for (auto &kv : pcHist) top.push_back({ kv.second, kv.first });
        std::sort(top.rbegin(), top.rend());
        for (size_t i = 0; i < top.size() && i < 8; i++)
            std::fprintf(stderr, "     %04x:%04x x%d\n",
                         top[i].second >> 16, top[i].second & 0xFFFF, top[i].first);
    }
    std::fprintf(stderr, "   codec playback bytes (pcmOut): %llu\n",
                 (unsigned long long)pcmOut);
    std::fprintf(stderr, "   codec captured bytes (pcmIn):  %llu\n",
                 (unsigned long long)pcmIn);
    std::fprintf(stderr, "   ADC ring undrained:            %zu\n",
                 s3c->debugAdcFill());

    // Scan RAM for a recorded WVE file (EPOC16 A-law sound files start
    // with the "ALawSoundFile**" signature) and report its header +
    // whether the sample data looks like the fed 440 Hz tone (non-silence
    // A-law bytes) rather than constant silence.
    {
        const uint8_t *ram = emu->getRamBuffer();
        size_t ramSz = emu->getRamSize();
        static const char magic[] = "ALawSoundFile";
        for (size_t i = 0; i + 64 < ramSz; i++) {
            if (std::memcmp(ram + i, magic, sizeof(magic) - 1) == 0) {
                std::fprintf(stderr, "   WVE header at RAM 0x%zx:", i);
                for (int k = 0; k < 48; k++) std::fprintf(stderr, " %02x", ram[i + k]);
                std::fprintf(stderr, "\n");
                size_t data = i + 0x20;
                size_t nonSilence = 0;
                for (size_t k = 0; k < 4096 && data + k < ramSz; k++) {
                    uint8_t b = ram[data + k];
                    if (b != 0x55 && b != 0xD5 && b != 0x00) nonSilence++;
                }
                std::fprintf(stderr,
                    "   WVE data: %zu/4096 non-silence bytes after header\n",
                    nonSilence);
                break;
            }
        }
    }

    if (requirePcmOut >= 0 && pcmOut < uint64_t(requirePcmOut)) {
        std::fprintf(stderr, "FAIL: pcmOut %llu < required %lld\n",
                     (unsigned long long)pcmOut, requirePcmOut);
        return 6;
    }
    if (requirePcmIn >= 0 && pcmIn < uint64_t(requirePcmIn)) {
        std::fprintf(stderr, "FAIL: pcmIn %llu < required %lld\n",
                     (unsigned long long)pcmIn, requirePcmIn);
        return 7;
    }
    std::fprintf(stderr, "PASS: tier C requirements met\n");
    return 0;
}

int main(int argc, char **argv) {
    const char *romPath = nullptr;
    const char *deviceId = nullptr;
    double bootSeconds = 20.0, runSeconds = 20.0, micToneAt = 1e9;
    long long requirePcmOut = -1, requirePcmIn = -1;
    std::vector<KeyPress> keys;

    for (int i = 1; i < argc; i++) {
        auto need = [&](int n) {
            if (i + n >= argc) {
                std::fprintf(stderr, "missing argument after %s\n", argv[i]);
                std::exit(1);
            }
        };
        if (!std::strcmp(argv[i], "--device"))            { need(1); deviceId = argv[++i]; }
        else if (!std::strcmp(argv[i], "--boot-seconds")) { need(1); bootSeconds = std::atof(argv[++i]); }
        else if (!std::strcmp(argv[i], "--run-seconds"))  { need(1); runSeconds = std::atof(argv[++i]); }
        else if (!std::strcmp(argv[i], "--mic-tone-at"))  { need(1); micToneAt = std::atof(argv[++i]); }
        else if (!std::strcmp(argv[i], "--require-pcm-out")) { need(1); requirePcmOut = std::atoll(argv[++i]); }
        else if (!std::strcmp(argv[i], "--require-pcm-in"))  { need(1); requirePcmIn  = std::atoll(argv[++i]); }
        else if (!std::strcmp(argv[i], "--press-key")) {
            need(3);
            KeyPress k{};
            k.atSec = std::atof(argv[++i]);
            k.code = std::atoi(argv[++i]);
            k.holdFrames = std::atoi(argv[++i]);
            keys.push_back(k);
        }
        else if (argv[i][0] != '-' && !romPath) { romPath = argv[i]; }
        else {
            std::fprintf(stderr, "unknown argument %s\n", argv[i]);
            return 1;
        }
    }

    if (!tierAlaw())         return 2;
    if (!tierChipPlayback()) return 3;
    if (!tierChipCapture())  return 4;

    if (romPath)
        return tierRom(romPath, deviceId, bootSeconds, runSeconds, micToneAt,
                       keys, requirePcmOut, requirePcmIn);

    std::fprintf(stderr, "\nAll chip-level tiers passed (no ROM given; tier C skipped)\n");
    return 0;
}
