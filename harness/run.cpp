// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Native host-side harness for driving the emulator core without emscripten.
// Boots a ROM, runs for a bit, attaches a CF image, keeps ticking while
// streaming every log line to stderr. Optional diagnostics help isolate hangs:
//   --pc-sample-hz N      sample PC every N frames, print top hotspots at end
//   --screenshot-every T  write a numbered PGM every T seconds of sim time
//   --detach-after T      detach the card T seconds after attach
//   --cycles N            repeat attach/detach N times to stress card state
//   --language N          pick the ROM's language variant (Geofox: 0 UK, 1 USA)
//
// Logs from the core get a [cycles=...] prefix so they can be correlated with
// sector-drain / ATA-command traces from vcfcard.cpp (which print direct to
// stderr without going through the core logger).
//
// Build:
//   bash harness/build.sh

#include "../core/emubase.h"
#include "../core/device_registry.h"
#include "../core/windermere.h"
#include "../core/series3c.h"
#include "../core/sa1100.h"
#include "../core/clps7111.h"
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

// Parse a space/comma-separated hex byte list (e.g. "16 10 02 21 10 03 34
// 43") into a byte vector. Returns empty on parse error.
static std::vector<uint8_t> parseHexBytes(const std::string &s) {
    std::vector<uint8_t> out;
    std::string tok;
    auto flush = [&]() {
        if (tok.empty()) return;
        char *end = nullptr;
        long v = strtol(tok.c_str(), &end, 16);
        if (end == tok.c_str() || v < 0 || v > 0xFF) { out.clear(); tok.clear(); return; }
        out.push_back((uint8_t)v);
        tok.clear();
    };
    for (char c : s) {
        if (c == ' ' || c == ',' || c == '\t' || c == '\n') { flush(); }
        else tok.push_back(c);
    }
    flush();
    return out;
}

// Encode a PLP frame (SYN | DLE STX | DLE-stuffed payload | DLE ETX | CRC-hi CRC-lo)
// for the --serial-tx-framed flag. Kept in sync with frontend/src/lib/plp/framing.ts.
static std::vector<uint8_t> encodePlpFrame(const std::vector<uint8_t> &payload) {
    uint16_t crc = 0;
    for (uint8_t b : payload) {
        crc ^= (uint16_t)b << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    std::vector<uint8_t> out;
    out.push_back(0x16);             // SYN preamble
    out.push_back(0x10); out.push_back(0x02);  // DLE STX
    for (uint8_t b : payload) {
        if (b == 0x10) { out.push_back(0x10); out.push_back(0x10); }
        else out.push_back(b);
    }
    out.push_back(0x10); out.push_back(0x03);  // DLE ETX
    out.push_back((uint8_t)(crc >> 8));        // CRC big-endian: hi then lo
    out.push_back((uint8_t)(crc & 0xFF));
    return out;
}

// Streaming PLP frame decoder. Same state machine as
// frontend/src/lib/plp/framing.ts FrameDecoder, ported to C++ so the
// harness's auto-responder can react to inbound frames in real time
// (the timestamp inside the device's 0x31 frame varies each run, so
// static --serial-tx scripts can't reproduce them).
struct HarnessFrameDecoder {
    enum { HUNT, PROLOGUE_DLE, IN_FRAME, ESCAPE_OR_END, GOT_ETX, GOT_CRC_HI };
    int state = HUNT;
    std::vector<uint8_t> payload;
    uint8_t crcHi = 0;
    uint32_t badCrc = 0;
    static constexpr uint8_t DLE = 0x10, STX = 0x02, ETX = 0x03;
    static uint16_t crc16(const std::vector<uint8_t> &d) {
        uint16_t crc = 0;
        for (uint8_t b : d) {
            crc ^= (uint16_t)b << 8;
            for (int i = 0; i < 8; i++)
                crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
        return crc;
    }
    // Returns ALL complete frames found in this byte run, in order.
    std::vector<std::vector<uint8_t>> feed(const uint8_t *bytes, size_t n) {
        std::vector<std::vector<uint8_t>> out;
        for (size_t i = 0; i < n; i++) {
            uint8_t b = bytes[i];
            switch (state) {
                case HUNT:
                    if (b == DLE) state = PROLOGUE_DLE;
                    break;
                case PROLOGUE_DLE:
                    if      (b == STX) { payload.clear(); state = IN_FRAME; }
                    else if (b == DLE) state = PROLOGUE_DLE;
                    else               state = HUNT;
                    break;
                case IN_FRAME:
                    if (b == DLE) state = ESCAPE_OR_END;
                    else          payload.push_back(b);
                    break;
                case ESCAPE_OR_END:
                    if      (b == DLE) { payload.push_back(DLE); state = IN_FRAME; }
                    else if (b == ETX) state = GOT_ETX;
                    else               state = HUNT;
                    break;
                case GOT_ETX:
                    crcHi = b;
                    state = GOT_CRC_HI;
                    break;
                case GOT_CRC_HI: {
                    uint16_t got = (uint16_t)((crcHi << 8) | b);
                    uint16_t want = crc16(payload);
                    if (got == want) out.push_back(std::move(payload));
                    else             badCrc++;
                    payload.clear();
                    state = HUNT;
                    break;
                }
            }
        }
        return out;
    }
};

static FILE *g_logFile = nullptr;
static EmuBase *g_emu = nullptr;
// PSION_REALTIME=1 throttles the sim loop to wall-clock (~64 frames/sec) so the
// device runs at real-time vs the host serial bridge — repros browser/WASM
// pacing (IrDA large-file beam stall/reboot) that flat-out sim hides.
static bool g_realtimeThrottle = false;
static uint64_t g_trapCount = 0;

// Any log line with one of these substrings counts as a "trap-like" event for
// boot validation. Only catches messages that go through the emu logger;
// scripts/test-boot.sh independently greps the combined stdout+stderr for the
// core's direct-printf messages (e.g. "unhandled 32bit uart write").
static bool lineLooksLikeTrap(const char *s) {
    static const char *kNeedles[] = {
        "prefetch error",
        "data abort",
        "undef",
    };
    for (const char *n : kNeedles) {
        if (std::strstr(s, n)) return true;
    }
    return false;
}

static void emitLog(const char *s) {
    if (lineLooksLikeTrap(s)) g_trapCount++;
    if (g_emu) {
        std::fprintf(stderr, "[%10llu] %s\n",
                     (unsigned long long)g_emu->currentCycles(), s);
    } else {
        std::fprintf(stderr, "%s\n", s);
    }
    if (g_logFile) {
        if (g_emu)
            std::fprintf(g_logFile, "[%10llu] %s\n",
                         (unsigned long long)g_emu->currentCycles(), s);
        else
            std::fprintf(g_logFile, "%s\n", s);
        std::fflush(g_logFile);
    }
}

// Build a fresh CF image partitioned and formatted to match what the Psion's
// own CF formatter produces. EPOC's pccd_ata PartitionInfo rejects any card
// whose sector 0 lacks a 0x55AA MBR signature (KErrCorrupt), which triggers
// a driver-close / reopen loop and makes every "attach" look like a storm of
// IDENTIFY commands. A zero-filled buffer hits that loop; a blob matching the
// Psion layout reads clean (IDENTIFY → READ 0 → READ 32 BPB → READ FAT) and
// mounts in a few milliseconds.
//
// Mirrors frontend/src/lib/fat16.ts::createBlankImage — keep the two in sync
// if the layout ever changes. Ignores the `label` parameter (Psion always
// writes "NO NAME   ") for the same reason.
static std::vector<uint8_t> makePsionBlankImage(size_t sizeBytes) {
    const size_t BPS = 512;
    const uint32_t PART_FIRST_SEC = 32;
    std::vector<uint8_t> img(sizeBytes, 0xFF);
    const uint32_t totalSectorsOnImage = static_cast<uint32_t>(sizeBytes / BPS);
    const uint32_t partTotalSectors = totalSectorsOnImage - PART_FIRST_SEC;
    const uint16_t reservedSectorCount = 1;
    const uint8_t numFats = 2;
    const uint16_t rootEntryCount = 512;
    const uint16_t rootDirSectors = (rootEntryCount * 32 + BPS - 1) / BPS;

    uint8_t sectorsPerCluster = 0;
    uint32_t sectorsPerFat = 0;
    // Pick the smallest cluster size that produces (a) a FAT16 cluster count
    // in-range [4085, 65524] AND (b) sectorsPerFat ≤ 255. EPOC's FAT16
    // mounter fails to allocate its FAT cache when BPB_FATSz16 > ~256
    // sectors (observed as the mounter reading the BPB in a tight retry
    // loop and EPOC reporting "corrupt"). Empirically 255-sector FATs
    // mount cleanly; 508 does not. For a 32 MB card that bumps the
    // cluster size from 1 sector (spf=508) to 2 sectors (spf=255); for
    // 64 MB, 8 sectors (spf=128); for 128 MB, 16 sectors (spf=128).
    for (uint8_t spc : {1, 2, 4, 8, 16, 32, 64}) {
        uint32_t tmp1 = partTotalSectors - reservedSectorCount - rootDirSectors;
        uint32_t tmp2 = (256u * spc + numFats) / 2;
        uint32_t spf = std::max<uint32_t>(1, (tmp1 + tmp2 - 1) / tmp2);
        uint32_t dataSectors = partTotalSectors - reservedSectorCount - numFats * spf - rootDirSectors;
        uint32_t clusters = dataSectors / spc;
        if (clusters >= 4085 && clusters <= 65524 && spf <= 255) {
            sectorsPerCluster = spc;
            sectorsPerFat = spf;
            break;
        }
    }
    if (sectorsPerCluster == 0) {
        std::fprintf(stderr, "Image size %zu cannot be formatted as FAT16\n", sizeBytes);
        std::exit(4);
    }

    auto wU16 = [&](size_t off, uint16_t v) {
        img[off]     = v & 0xFF;
        img[off + 1] = (v >> 8) & 0xFF;
    };
    auto wU32 = [&](size_t off, uint32_t v) {
        img[off]     =  v        & 0xFF;
        img[off + 1] = (v >>  8) & 0xFF;
        img[off + 2] = (v >> 16) & 0xFF;
        img[off + 3] = (v >> 24) & 0xFF;
    };

    // MBR: zero up to the partition table + signature area, then write one
    // FAT16 partition entry and the 0x55AA signature.
    std::fill(img.begin(), img.begin() + 0x200, 0x00);
    const size_t peOff = 0x1BE;
    const uint8_t partType = partTotalSectors <= 0xFFFF ? 0x04 /*FAT16 small*/ : 0x06 /*FAT16*/;
    img[peOff + 0] = 0x80;                 // bootable
    img[peOff + 1] = 0x01;                 // start head
    img[peOff + 2] = 0x01;                 // start sector
    img[peOff + 3] = 0x00;                 // start cylinder
    img[peOff + 4] = partType;
    img[peOff + 5] = 0x01; img[peOff + 6] = 0xA0; img[peOff + 7] = 0x62;  // fake end-CHS
    wU32(peOff + 8,  PART_FIRST_SEC);
    wU32(peOff + 12, partTotalSectors);
    img[0x1FE] = 0x55; img[0x1FF] = 0xAA;

    // BPB at LBA 32 (partition start).
    const size_t bpb = PART_FIRST_SEC * BPS;
    std::fill(img.begin() + bpb, img.begin() + bpb + 0x3E, 0x00);
    img[bpb + 0] = 0xE9; img[bpb + 1] = 0x00; img[bpb + 2] = 0x90;   // jump/NOP
    const char *oem = "EPOC";
    for (int i = 0; oem[i]; i++) img[bpb + 3 + i] = oem[i];
    wU16(bpb + 11, BPS);
    img[bpb + 13] = sectorsPerCluster;
    wU16(bpb + 14, reservedSectorCount);
    img[bpb + 16] = numFats;
    wU16(bpb + 17, rootEntryCount);
    wU16(bpb + 19, partTotalSectors <= 0xFFFF ? partTotalSectors : 0);
    img[bpb + 21] = 0xF8;
    wU16(bpb + 22, sectorsPerFat);
    wU16(bpb + 24, 32);
    wU16(bpb + 26, 2);
    wU32(bpb + 28, PART_FIRST_SEC);
    wU32(bpb + 32, partTotalSectors > 0xFFFF ? partTotalSectors : 0);
    img[bpb + 36] = 0x80; img[bpb + 37] = 0; img[bpb + 38] = 0x29;
    wU32(bpb + 39, 0x12345678);
    const char *labelField = "NO NAME    ";
    for (int i = 0; i < 11; i++) img[bpb + 43 + i] = labelField[i];
    const char *fsType = "FAT16   ";
    for (int i = 0; i < 8; i++) img[bpb + 54 + i] = fsType[i];
    img[bpb + 510] = 0x55; img[bpb + 511] = 0xAA;

    // FATs: two copies, each with the reserved entries 0-3 populated.
    const size_t fatStartSector = PART_FIRST_SEC + reservedSectorCount;
    for (uint8_t f = 0; f < numFats; f++) {
        size_t fatOff = (fatStartSector + f * sectorsPerFat) * BPS;
        size_t fatBytes = sectorsPerFat * BPS;
        std::fill(img.begin() + fatOff, img.begin() + fatOff + fatBytes, 0x00);
        img[fatOff + 0] = 0xF8; img[fatOff + 1] = 0xFF;
        img[fatOff + 2] = 0xFF; img[fatOff + 3] = 0xFF;
        img[fatOff + 4] = 0xFF; img[fatOff + 5] = 0xFF;
        img[fatOff + 6] = 0xFF; img[fatOff + 7] = 0xFF;
    }

    // Zero the root directory so listings terminate cleanly.
    const size_t rootOff = (fatStartSector + numFats * sectorsPerFat) * BPS;
    std::fill(img.begin() + rootOff, img.begin() + rootOff + rootDirSectors * BPS, 0x00);

    return img;
}

static std::vector<uint8_t> readFile(const char *path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "Failed to open %s\n", path);
        std::exit(2);
    }
    std::streamsize sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(sz);
    f.read(reinterpret_cast<char *>(bytes.data()), sz);
    return bytes;
}

// Pull the current framebuffer as a grayscale (R channel only) buffer. Shared
// between PGM writing and variance computation so both always see the same
// snapshot.
static std::vector<uint8_t> readGrayscale(EmuBase *emu, int &w, int &h) {
    w = emu->getLCDWidth();
    h = emu->getLCDHeight();
    std::vector<uint8_t> pixels(w * h * 4);
    std::vector<uint8_t *> lines(h);
    for (int y = 0; y < h; y++) lines[y] = pixels.data() + y * w * 4;
    emu->readLCDIntoBuffer(lines.data(), true);
    std::vector<uint8_t> gray(w * h);
    for (int i = 0; i < w * h; i++) gray[i] = pixels[i * 4];
    return gray;
}

static void writePGM(EmuBase *emu, const char *path) {
    int w, h;
    auto gray = readGrayscale(emu, w, h);
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "Could not open %s for write\n", path);
        return;
    }
    std::fprintf(f, "P5\n%d %d\n255\n", w, h);
    std::fwrite(gray.data(), 1, gray.size(), f);
    std::fclose(f);
    std::fprintf(stderr, "wrote %s (%dx%d)\n", path, w, h);
}

struct LcdStats {
    double mean;
    double variance;
    size_t uniqueValues;
    uint8_t minV;
    uint8_t maxV;
};

// Scans the flat RAM buffer for the RAMDRIVE FAT volume-label entry (attr=0x08,
// 11-char name "RAMDRIVE   ") and counts the FAT directory entries (attr=0x10)
// immediately following it in the root directory block. Searches only 32-byte-
// aligned positions so the kernel's low-RAM copy of the label (which sits at an
// unaligned offset and is NOT followed by directory entries) is naturally skipped.
struct MdriveInfo {
    bool labelFound = false;
    int subdirsFound = 0;
    std::string subdirList;
};

static MdriveInfo scanMdrive(const uint8_t *ram, size_t ramSize) {
    MdriveInfo best;
    if (!ram || ramSize < 32) return best;
    static const uint8_t kSig[12] = {
        'R','A','M','D','R','I','V','E',' ',' ',' ', 0x08
    };
    // Scan every byte: the FAT root-dir alignment varies between device types
    // (32-byte-aligned for 3c/3a, arbitrary for 3mx/Siena whose kernel places
    // the root dir at an ASIC9 address that translates to an unaligned RAM
    // offset). Scanning all positions ensures we find the label regardless.
    for (size_t i = 0; i + 32 <= ramSize; i++) {
        if (ram[i] != 'R' || std::memcmp(ram + i, kSig, 12) != 0) continue;
        best.labelFound = true;
        MdriveInfo cur;
        cur.labelFound = true;
        // Walk the 32-byte entries immediately following the label entry.
        // The label occupies one 32-byte slot; subdirectory entries follow at
        // label_pos+32, label_pos+64, ... regardless of absolute alignment.
        for (size_t j = i + 32; j + 32 <= ramSize && j < i + 512u * 32; j += 32) {
            uint8_t first = ram[j];
            if (first == 0x00) break;
            if (first == 0xE5) continue;
            if (ram[j + 11] == 0x10) {
                char name[9] = {};
                int len = 0;
                for (int k = 0; k < 8 && ram[j + k] > ' '; k++)
                    name[len++] = static_cast<char>(ram[j + k]);
                if (len > 0) {
                    cur.subdirsFound++;
                    if (!cur.subdirList.empty()) cur.subdirList += ",";
                    cur.subdirList += name;
                }
            }
        }
        if (cur.subdirsFound > best.subdirsFound) best = cur;
    }
    return best;
}

static LcdStats computeLcdStats(const std::vector<uint8_t> &gray) {
    LcdStats s{};
    if (gray.empty()) return s;
    uint64_t sum = 0;
    uint8_t mn = 255, mx = 0;
    bool seen[256] = {};
    for (uint8_t v : gray) {
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        seen[v] = true;
    }
    s.mean = (double)sum / (double)gray.size();
    double var = 0.0;
    for (uint8_t v : gray) {
        double d = (double)v - s.mean;
        var += d * d;
    }
    s.variance = var / (double)gray.size();
    size_t uniq = 0;
    for (bool b : seen) if (b) uniq++;
    s.uniqueValues = uniq;
    s.minV = mn;
    s.maxV = mx;
    return s;
}

// ── fork()-based snapshot REPL ─────────────────────────────────────────────
// Reaching the netBook bootloader's post-read park costs ~1000 s (the faithful
// OS.IMG read).  To iterate on what unblocks the post-read restart WITHOUT
// re-running that read each time, --fork-repl pauses at the park and reads
// experiments from stdin: each line is fork()ed into a child that inherits the
// EXACT emulator state (CoW, no serialization), runs the experiment, and
// exits — so every experiment starts from the same parked baseline in
// milliseconds.  A line is a ';'-separated command list:
//   pc                      print the current PC
//   regs                    print r0..r15
//   peek <hexVA> [n]        read n (default 1) 32-bit words
//   poke <hexVA> <hexVal> [sz]   write (sz = 8/16/32, default 32)
//   setr <idx> <hexVal>     set GPRn
//   call <hexPC> [hexR0]    callRomFunctionSync(PC, r0); print return + new PC
//   run <cycles>            executeUntil(+cycles); print PC/cycle
//   var <cycles>            run, then print LCD variance (did the desktop paint?)
// e.g.  poke 80306dec 1 ; run 200000000 ; var 0
static void execCmd(EmuBase *emu, const std::string &cmd) {
    char op[32] = {0};
    if (std::sscanf(cmd.c_str(), "%31s", op) != 1) return;
    if (!std::strcmp(op, "pc")) {
        std::fprintf(stderr, "    pc=%08x cyc=%llu\n", emu->getRealPC(),
                     (unsigned long long)emu->currentCycles());
    } else if (!std::strcmp(op, "regs")) {
        for (int i = 0; i < 16; i++)
            std::fprintf(stderr, "    r%-2d=%08x%s", i, emu->getGPR(i),
                         (i % 4 == 3) ? "\n" : "");
    } else if (!std::strcmp(op, "peek")) {
        unsigned va = 0; int n = 1;
        std::sscanf(cmd.c_str(), "%*s %x %d", &va, &n);
        for (int i = 0; i < n; i++) {
            auto v = emu->readVirtualDebug(va + i * 4, ARM710::V32);
            std::fprintf(stderr, "    [%08x]=%08x\n", va + i * 4,
                         v.value_or(0xFFFFFFFFu));
        }
    } else if (!std::strcmp(op, "poke")) {
        unsigned va = 0, val = 0; int sz = 32;
        std::sscanf(cmd.c_str(), "%*s %x %x %d", &va, &val, &sz);
        ARM710::ValueSize vs = sz == 8 ? ARM710::V8
                             : sz == 16 ? ARM710::V16 : ARM710::V32;
        emu->writeVirtual(val, va, vs);
        std::fprintf(stderr, "    poke [%08x]<=%08x (sz%d)\n", va, val, sz);
    } else if (!std::strcmp(op, "setr")) {
        int idx = 0; unsigned val = 0;
        std::sscanf(cmd.c_str(), "%*s %d %x", &idx, &val);
        emu->setGPR(idx, val);
        std::fprintf(stderr, "    r%d<=%08x\n", idx, val);
    } else if (!std::strcmp(op, "call")) {
        unsigned pc = 0, r0 = 0;
        std::sscanf(cmd.c_str(), "%*s %x %x", &pc, &r0);
        uint32_t ret = emu->callRomFunctionSync(pc, r0);
        std::fprintf(stderr, "    call %08x(r0=%08x) -> %08x  (pc now %08x)\n",
                     pc, r0, ret, emu->getRealPC());
    } else if (!std::strcmp(op, "run")) {
        long long c = 0; std::sscanf(cmd.c_str(), "%*s %lld", &c);
        emu->executeUntil((int64_t)emu->currentCycles() + c);
        std::fprintf(stderr, "    ran %lld -> pc=%08x cyc=%llu\n", c,
                     emu->getRealPC(), (unsigned long long)emu->currentCycles());
    } else if (!std::strcmp(op, "runto") || !std::strcmp(op, "runtorange")) {
        // Step via executeUntil (advances passedCycles so timers/IRQs fire —
        // tick() alone freezes time and diverges).  Check PC at each small
        // cycle step; stop on an exact match (runto) or a range (runtorange).
        unsigned lo = 0, hi = 0; long long maxc = 200000000;
        if (!std::strcmp(op, "runto")) {
            std::sscanf(cmd.c_str(), "%*s %x %lld", &lo, &maxc);
            hi = lo;
        } else {
            std::sscanf(cmd.c_str(), "%*s %x %x %lld", &lo, &hi, &maxc);
        }
        int64_t start = (int64_t)emu->currentCycles();
        const int64_t step = 64;
        bool hit = false;
        while ((int64_t)emu->currentCycles() - start < maxc) {
            uint32_t p = emu->getRealPC();
            if (p >= lo && p <= hi) { hit = true; break; }
            emu->executeUntil((int64_t)emu->currentCycles() + step);
        }
        std::fprintf(stderr, "    %s %08x%s%08x: %s pc=%08x cyc=%llu (+%lld)\n",
                     op, lo, (lo == hi ? "" : "-"), hi, hit ? "HIT" : "miss",
                     emu->getRealPC(), (unsigned long long)emu->currentCycles(),
                     (long long)((int64_t)emu->currentCycles() - start));
    } else if (!std::strcmp(op, "var")) {
        long long c = 0; std::sscanf(cmd.c_str(), "%*s %lld", &c);
        if (c > 0) emu->executeUntil((int64_t)emu->currentCycles() + c);
        int w, h; auto g = readGrayscale(emu, w, h);
        auto s = computeLcdStats(g);
        std::fprintf(stderr, "    var=%.1f mean=%.1f uniq=%zu pc=%08x cyc=%llu\n",
                     s.variance, s.mean, s.uniqueValues, emu->getRealPC(),
                     (unsigned long long)emu->currentCycles());
    } else {
        std::fprintf(stderr, "    ? unknown cmd: %s\n", cmd.c_str());
    }
}

static void forkRepl(EmuBase *emu) {
    std::fprintf(stderr,
        "=== FORK-REPL ready: pc=%08x cyc=%llu — send experiments on stdin "
        "(';'-separated cmds per line; 'quit' to end) ===\n",
        emu->getRealPC(), (unsigned long long)emu->currentCycles());
    std::fflush(stderr);
    std::string line;
    while (std::getline(std::cin, line)) {
        // strip leading/trailing whitespace
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' ||
                                 line.back() == '\t'))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line == "quit" || line == "q") break;
        std::fflush(stdout); std::fflush(stderr);
        pid_t pid = fork();
        if (pid < 0) { std::perror("fork"); break; }
        if (pid == 0) {
            std::fprintf(stderr, "--- exp: %s\n", line.c_str());
            size_t start = 0;
            while (start <= line.size()) {
                size_t semi = line.find(';', start);
                std::string cmd = line.substr(
                    start, semi == std::string::npos ? std::string::npos
                                                     : semi - start);
                while (!cmd.empty() && cmd.front() == ' ') cmd.erase(cmd.begin());
                while (!cmd.empty() && cmd.back() == ' ') cmd.pop_back();
                if (!cmd.empty()) execCmd(emu, cmd);
                if (semi == std::string::npos) break;
                start = semi + 1;
            }
            std::fflush(stdout); std::fflush(stderr);
            _exit(0);
        }
        int st = 0; waitpid(pid, &st, 0);
        std::fprintf(stderr, "--- exp done ---\n");
        std::fflush(stderr);
    }
    std::fprintf(stderr, "=== FORK-REPL exited ===\n");
}

int main(int argc, char **argv) {
    const char *romPath = nullptr;
    double bootSeconds = 5.0;
    double postAttachSeconds = 8.0;
    int cardSizeMb = 16;
    const char *logPath = nullptr;
    const char *cardPath = nullptr;
    const char *screenshotPath = nullptr;
    const char *screenshotPrefix = nullptr;
    double screenshotEverySec = 0.0;
    int pcSampleHz = 0;
    bool wakeAfterAttach = false;
    bool quietLogs = false;
    double detachAfterSec = 0.0;
    int cycles = 1;
    int cfIrqLine = -1;
    int cfReenableMode = 0;
    bool cfReschedulePoke = false;
    bool cfFastWatchdog = false;
    bool cfAccelTimerDisable = false;
    bool cfDirectInvoke = false;
    bool cfDirectInvokeDisable = false;
    int tapX = -1, tapY = -1;
    double tapAfterSec = 0.0;
    bool zeroFill = false;
    bool assertBoot = false;
    bool skipCard = false;
    bool feedMicSilence = false;
    bool forkReplMode = false;   // --fork-repl: snapshot REPL at end of run
    int64_t replAtCycle = 0;     // --repl-at-cycle N: enter REPL when cyc >= N
    const char *summaryJsonPath = nullptr;
    double minVariance = 50.0;   // 5mx real boot is ~2000; 50 rules out stuck-blank
    int minUniquePcs = 8;        // Series 5 broken boot sits at 3; healthy 5mx has 20+
    int maxTraps = 0;            // trap-like logger lines tolerated
    // SIBO2 "Media is corrupt" dialog signature: the dialog at the cold-boot
    // window has ~27800 non-paper pixels on the 480x160 LCD, while the post-
    // dismiss System screen has ~10200. --max-nonpaper N caps the final
    // screen — over the threshold means the dialog (or another similarly
    // dense overlay) is still up, which is what the user-reported "media
    // is corrupt" symptom looks like in screenshot form.
    int maxNonPaper = -1;        // -1 disables the check
    // --max-variance: upper bound on the settled LCD variance.  Pairs with
    // --min-variance to pin a boot to ONE screen when the wrong screen is
    // also busy — e.g. the netBook Quartz build ends on its app screen
    // (variance ~450) but a regression parks it on the boot splash
    // (~11200), which a min-variance floor alone waves through.
    double maxVariance = -1.0;   // < 0 disables the check
    // Scripted-keyboard sequence: a list of (delay-after-previous-action,
    // EpocKey, hold-frames) tuples. Used by the SIBO post-boot test to
    // dismiss the cold-boot "Media is corrupt" dialog and then poke the
    // app keys to verify nothing reports KErrCorrupt afterwards.
    struct KeyEvent { double atSec; int epocKey; int holdFrames; bool repeat = false; };
    std::vector<KeyEvent> keyEvents;
    // Scripted-tap sequence: same idea as keyEvents but for screen taps,
    // so you can do "open Sketch → draw → menu/up/enter" in a single
    // harness run instead of one-shot --tap-at.
    struct TapEvent { double atSec; int x, y; };
    std::vector<TapEvent> tapEvents;
    // --drag-seq: press at (x1,y1), travel to (x2,y2) with the button
    // still down, then release. Taps cannot express this, and a pointing
    // device that reports RELATIVE motion (the Geofox mouse pad) has to
    // be driven the whole way rather than teleported, so a drag is the
    // one gesture that exercises its tracking end to end.
    struct DragEvent { double atSec; int x1, y1, x2, y2; };
    std::vector<DragEvent> dragEvents;
    // Serial-bridge scripting: attach UART N as a host endpoint at some
    // sim time, send bytes at scheduled times, and capture every byte
    // the device transmits to a file. Lets the PLP / Remote Link
    // protocol be poked from a CLI without needing the browser dialog
    // in the loop. Only meaningful on Windermere devices (Revo /
    // MC218 / Series 5 / 5mx); no-ops elsewhere.
    struct SerialAttachEvent { double atSec; int uart; };
    std::vector<SerialAttachEvent> serialAttachEvents;
    // Detach events — mirror the browser dialogs' serialDetachHost so the
    // unplug/replug modem-status IRQ sequence can be reproduced natively.
    std::vector<SerialAttachEvent> serialDetachEvents;
    struct SerialTxEvent { double atSec; int uart; std::vector<uint8_t> bytes; bool framed; };
    std::vector<SerialTxEvent> serialTxEvents;
    // Auto-responder rules: when an inbound PLP frame has its first
    // payload byte equal to `matchType`, send back `replyBytes` framed.
    // Special: `echo=true` means "send the entire received frame back
    // verbatim" (useful when the payload contains a per-session token
    // we can't pre-compute, like the timestamp in EPOC's 0x31 frame).
    struct SerialAutoRule {
        uint8_t matchType;
        std::vector<uint8_t> replyBytes;
        bool echo;
    };
    std::vector<SerialAutoRule> serialAutoRules;
    const char *serialCapturePath = nullptr;
    // --serial-bridge-socket PATH — relay raw UART bytes to/from a unix
    // domain socket so an external process can drive a live, stateful
    // protocol exchange (e.g. the JS IrDA stack's IrSendClient). Bytes the
    // device transmits are written to the socket; bytes read from the
    // socket are injected into the UART RX FIFO. Use with --serial-attach
    // and --serial-poll-until.
    const char *serialBridgeSocket = nullptr;
    double serialPollSec = 0.0;   // 0 = disabled; otherwise sample at this cadence
    double serialPollUntil = 0.0; // run sim time at which to stop
    // Which UART the periodic drain loop reads device→host bytes from.
    // Defaults to UART2 (Windermere's cable port). If the user passes
    // --serial-attach for a different UART without also setting this
    // explicitly, the sort/setup code below auto-overrides to the first
    // attached UART so the drain follows the attach — required for
    // SA-1100 netBook where the cable lives on UART3.
    int    serialPollUart = 2;
    bool   serialPollUartExplicit = false;
    const char *deviceOverride = nullptr;
    // --machine-id HEX: set the machine's Unique id before the first cycle
    // runs, the same way the frontend's debug panel does. Up to 16 hex
    // digits: the low 8 are the identity PROM / EEPROM word, and the high
    // 8 (when given) patch the model UID the ROM supplies. Ignored with a
    // warning on devices whose identity chip isn't modelled.
    bool     machineIdSet = false;
    uint64_t machineIdValue = 0;
    bool     machineIdHasPrefix = false;
    // --language N: pick the ROM's language variant before the first cycle
    // runs, the same way the frontend's Language control does. Only the
    // Geofox One has more than one (0 = English (UK), 1 = English (USA));
    // ignored with a warning everywhere else.
    int      languageIndex = -1;
    // --swap-card-path FILE: after the initial card attach + post-attach run,
    // detach the first card and attach this second one, then run
    // --swap-post-seconds more. Reproduces the browser "boot with one card,
    // then slide a different card into the bay" hot-swap flow.
    const char *swapCardPath = nullptr;
    double swapPostSeconds = 20.0;
    // SSD pack image paths (SIBO devices only). Each is loaded into the
    // matching slot via emu->attachSSD() right after ROM load, before
    // the first executeUntil() tick.
    const char *ssdPath[4] = { nullptr, nullptr, nullptr, nullptr };
    // Per-slot pack type passed to attachSSD (0 = auto-sniff 0xF1A5,
    // 1 = RAM, 2 = Type 1 Flash, 3 = hardware write-protected). Set via
    // --ssd-type-X ram|flash|protected|auto.
    int ssdType[4] = { 0, 0, 0, 0 };
    // Per-slot output paths: dump the (possibly guest-modified) pack
    // image back to a file when the run ends. Lets tests verify
    // in-device writes (e.g. file copies onto a pack) from the host.
    const char *ssdDumpPath[4] = { nullptr, nullptr, nullptr, nullptr };
    // When > 0, defer SSD attach until that many simulated seconds
    // have elapsed since boot. Used to reproduce browser-style
    // "hot-plug" attach (after the OS is up) vs cold attach.
    double ssdAttachAfterSec = 0.0;
    // Pre-execute RAM snapshot. Bytes loaded here are written into the
    // emulator's `ram` buffer right after ROM load (before any cycles
    // run), so a snapshot captured from the frontend's "Download RAM"
    // button can be replayed without the kernel re-initialising it.
    const char *ramSnapshotPath = nullptr;
    const char *saveRamSnapshotPath = nullptr;
    int minMdriveSubdirs = -1;   // -1 = no check; >= 0 required FAT subdir count

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--boot-seconds" && i + 1 < argc) bootSeconds = std::atof(argv[++i]);
        else if (a == "--post-attach-seconds" && i + 1 < argc) postAttachSeconds = std::atof(argv[++i]);
        else if (a == "--card-size-mb" && i + 1 < argc) cardSizeMb = std::atoi(argv[++i]);
        else if (a == "--log-file" && i + 1 < argc) logPath = argv[++i];
        else if (a == "--card-path" && i + 1 < argc) cardPath = argv[++i];
        else if (a == "--swap-card-path" && i + 1 < argc) swapCardPath = argv[++i];
        else if (a == "--swap-post-seconds" && i + 1 < argc) swapPostSeconds = std::atof(argv[++i]);
        else if (a == "--screenshot" && i + 1 < argc) screenshotPath = argv[++i];
        else if (a == "--screenshot-every" && i + 2 < argc) {
            screenshotEverySec = std::atof(argv[++i]);
            screenshotPrefix = argv[++i];
        }
        else if (a == "--pc-sample-hz" && i + 1 < argc) pcSampleHz = std::atoi(argv[++i]);
        else if (a == "--wake-after-attach") wakeAfterAttach = true;
        else if (a == "--detach-after" && i + 1 < argc) detachAfterSec = std::atof(argv[++i]);
        else if (a == "--cf-irq-line" && i + 1 < argc) cfIrqLine = std::atoi(argv[++i]);
        else if (a == "--cf-reenable-mode" && i + 1 < argc) cfReenableMode = std::atoi(argv[++i]);
        else if (a == "--cf-reschedule-poke") cfReschedulePoke = true;
        else if (a == "--cf-fast-watchdog") cfFastWatchdog = true;
        else if (a == "--no-cf-accel-timer") cfAccelTimerDisable = true;
        else if (a == "--cf-direct-invoke") cfDirectInvoke = true;
        else if (a == "--no-cf-direct-invoke") cfDirectInvokeDisable = true;
        else if (a == "--cycles" && i + 1 < argc) cycles = std::atoi(argv[++i]);
        else if (a == "--tap-at" && i + 2 < argc) {
            tapX = std::atoi(argv[++i]);
            tapY = std::atoi(argv[++i]);
        }
        else if (a == "--tap-after" && i + 1 < argc) tapAfterSec = std::atof(argv[++i]);
        else if (a == "--zero-fill") zeroFill = true;
        else if (a == "--quiet-logs") quietLogs = true;
        else if (a == "--assert-boot") assertBoot = true;
        else if (a == "--skip-card") skipCard = true;
        else if (a == "--fork-repl") forkReplMode = true;
        else if (a == "--repl-at-cycle" && i + 1 < argc) { replAtCycle = std::atoll(argv[++i]); forkReplMode = true; }
        else if (a == "--feed-mic-silence") feedMicSilence = true;
        else if (a == "--summary-json" && i + 1 < argc) summaryJsonPath = argv[++i];
        else if (a == "--min-variance" && i + 1 < argc) minVariance = std::atof(argv[++i]);
        else if (a == "--max-variance" && i + 1 < argc) maxVariance = std::atof(argv[++i]);
        else if (a == "--min-unique-pcs" && i + 1 < argc) minUniquePcs = std::atoi(argv[++i]);
        else if (a == "--max-traps" && i + 1 < argc) maxTraps = std::atoi(argv[++i]);
        else if (a == "--max-nonpaper" && i + 1 < argc) maxNonPaper = std::atoi(argv[++i]);
        else if (a == "--device" && i + 1 < argc) deviceOverride = argv[++i];
        else if (a == "--language" && i + 1 < argc) languageIndex = std::atoi(argv[++i]);
        else if (a == "--machine-id" && i + 1 < argc) {
            std::string hex = argv[++i];
            // Accept EPOC's grouping (1000-118A-CAFE-BABE) as typed.
            hex.erase(std::remove(hex.begin(), hex.end(), '-'), hex.end());
            machineIdValue = std::strtoull(hex.c_str(), nullptr, 16);
            machineIdHasPrefix = hex.size() > 8;
            machineIdSet = true;
        }
        else if (a == "--ssd-a" && i + 1 < argc) ssdPath[0] = argv[++i];
        else if (a == "--ssd-b" && i + 1 < argc) ssdPath[1] = argv[++i];
        else if (a == "--ssd-c" && i + 1 < argc) ssdPath[2] = argv[++i];
        else if (a == "--ssd-d" && i + 1 < argc) ssdPath[3] = argv[++i];
        // --ssd-type-X auto|ram|flash|protected: pack type presented in
        // the info byte (hardware straps on a real pack). Default auto =
        // sniff the 0xF1A5 magic, which misreads FEFS-formatted RAM packs.
        else if (a.rfind("--ssd-type-", 0) == 0 && a.size() == 12 && i + 1 < argc &&
                 a[11] >= 'a' && a[11] <= 'd') {
            std::string t = argv[++i];
            ssdType[a[11] - 'a'] =
                (t == "ram") ? 1 : (t == "flash") ? 2 : (t == "protected") ? 3 : 0;
        }
        // --ssd-dump-X PATH: write the slot's final image (including any
        // guest writes) to PATH when the run completes.
        else if (a.rfind("--ssd-dump-", 0) == 0 && a.size() == 12 && i + 1 < argc &&
                 a[11] >= 'a' && a[11] <= 'd') {
            ssdDumpPath[a[11] - 'a'] = argv[++i];
        }
        else if (a == "--ssd-attach-after" && i + 1 < argc) ssdAttachAfterSec = std::atof(argv[++i]);
        else if (a == "--ram-snapshot" && i + 1 < argc) ramSnapshotPath = argv[++i];
        else if (a == "--save-ram-snapshot" && i + 1 < argc) saveRamSnapshotPath = argv[++i];
        else if (a == "--min-mdrive-subdirs" && i + 1 < argc) minMdriveSubdirs = std::atoi(argv[++i]);
        // --press-key AT_SEC EPOC_KEY [HOLD_FRAMES]  (default hold = 8 frames ≈ 125 ms)
        else if (a == "--press-key" && i + 2 < argc) {
            KeyEvent ev;
            ev.atSec = std::atof(argv[++i]);
            ev.epocKey = std::atoi(argv[++i]);
            ev.holdFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? std::atoi(argv[++i]) : 8;
            keyEvents.push_back(ev);
        }
        // --repeat-key AT_SEC EPOC_KEY [HOLD_FRAMES] — same as --press-key
        // but re-asserts the key-down on every frame of the hold, the way a
        // host OS repeats a held key into the browser as a stream of
        // keydown events.  A device whose key delivery is edge-based (the
        // netpad, which turns each down into its own TRawEvent) must treat
        // the repeats as one press.
        else if (a == "--repeat-key" && i + 2 < argc) {
            KeyEvent ev;
            ev.atSec = std::atof(argv[++i]);
            ev.epocKey = std::atoi(argv[++i]);
            ev.holdFrames = (i + 1 < argc && argv[i + 1][0] != '-') ? std::atoi(argv[++i]) : 8;
            ev.repeat = true;
            keyEvents.push_back(ev);
        }
        // --tap-seq AT_SEC X Y  (one tap event in a multi-tap sequence)
        else if (a == "--tap-seq" && i + 3 < argc) {
            TapEvent ev;
            ev.atSec = std::atof(argv[++i]);
            ev.x = std::atoi(argv[++i]);
            ev.y = std::atoi(argv[++i]);
            tapEvents.push_back(ev);
        }
        // --drag-seq AT_SEC X1 Y1 X2 Y2
        else if (a == "--drag-seq" && i + 5 < argc) {
            DragEvent ev;
            ev.atSec = std::atof(argv[++i]);
            ev.x1 = std::atoi(argv[++i]);
            ev.y1 = std::atoi(argv[++i]);
            ev.x2 = std::atoi(argv[++i]);
            ev.y2 = std::atoi(argv[++i]);
            dragEvents.push_back(ev);
        }
        // --serial-attach UART AT_SEC  — attach the host bridge to UART
        // (1 or 2) at AT_SEC sim time. Required before any --serial-tx.
        else if (a == "--serial-attach" && i + 2 < argc) {
            SerialAttachEvent ev;
            ev.uart = std::atoi(argv[++i]);
            ev.atSec = std::atof(argv[++i]);
            serialAttachEvents.push_back(ev);
        }
        // --serial-detach UART AT_SEC — detach the host bridge (cable
        // unplug; modem-status IRQ) at AT_SEC. Pair with a later
        // --serial-attach to reproduce the browser's replug sequence.
        else if (a == "--serial-detach" && i + 2 < argc) {
            SerialAttachEvent ev;
            ev.uart = std::atoi(argv[++i]);
            ev.atSec = std::atof(argv[++i]);
            serialDetachEvents.push_back(ev);
        }
        // --serial-tx UART AT_SEC HEX  — send bytes raw (no framing) at AT_SEC.
        else if (a == "--serial-tx" && i + 3 < argc) {
            SerialTxEvent ev;
            ev.uart = std::atoi(argv[++i]);
            ev.atSec = std::atof(argv[++i]);
            ev.bytes = parseHexBytes(argv[++i]);
            ev.framed = false;
            if (ev.bytes.empty()) {
                std::fprintf(stderr, "Bad hex for --serial-tx\n");
                return 1;
            }
            serialTxEvents.push_back(ev);
        }
        // --serial-tx-framed UART AT_SEC HEX  — wrap the bytes in a PLP frame first.
        else if (a == "--serial-tx-framed" && i + 3 < argc) {
            SerialTxEvent ev;
            ev.uart = std::atoi(argv[++i]);
            ev.atSec = std::atof(argv[++i]);
            ev.bytes = parseHexBytes(argv[++i]);
            ev.framed = true;
            if (ev.bytes.empty()) {
                std::fprintf(stderr, "Bad hex for --serial-tx-framed\n");
                return 1;
            }
            serialTxEvents.push_back(ev);
        }
        // --serial-capture PATH  — append every byte received from the
        // attached UART to PATH (binary). Use with --serial-poll-until.
        else if (a == "--serial-capture" && i + 1 < argc) {
            serialCapturePath = argv[++i];
        }
        // --serial-bridge-socket PATH — connect to a unix-domain socket and
        // relay raw UART bytes both ways during the poll loop.
        else if (a == "--serial-bridge-socket" && i + 1 < argc) {
            serialBridgeSocket = argv[++i];
        }
        // --serial-auto-rule "MATCH_HEX:RESPONSE_HEX" — when an inbound
        // frame's first payload byte equals MATCH_HEX, send back a
        // framed PLP reply whose payload is RESPONSE_HEX. The special
        // RESPONSE token `echo` means "echo the inbound frame back
        // verbatim" — useful when the payload contains a per-session
        // token we can't predict (like EPOC's 0x31 timestamp).
        // Auto-rules are evaluated on every poll, in the order they
        // were declared on the command line.
        else if (a == "--serial-auto-rule" && i + 1 < argc) {
            std::string spec = argv[++i];
            auto colon = spec.find(':');
            if (colon == std::string::npos) {
                std::fprintf(stderr, "Bad --serial-auto-rule (need MATCH:REPLY): %s\n", spec.c_str());
                return 1;
            }
            std::string mhex = spec.substr(0, colon);
            std::string rhex = spec.substr(colon + 1);
            auto mbytes = parseHexBytes(mhex);
            if (mbytes.size() != 1) {
                std::fprintf(stderr, "Bad --serial-auto-rule MATCH (need 1 hex byte): %s\n", mhex.c_str());
                return 1;
            }
            SerialAutoRule rule;
            rule.matchType = mbytes[0];
            if (rhex == "echo") {
                rule.echo = true;
            } else {
                rule.echo = false;
                rule.replyBytes = parseHexBytes(rhex);
                if (rule.replyBytes.empty()) {
                    std::fprintf(stderr, "Bad --serial-auto-rule REPLY: %s\n", rhex.c_str());
                    return 1;
                }
            }
            serialAutoRules.push_back(std::move(rule));
        }
        // --serial-poll-until SEC [UART]  — runs the bridge poll loop
        // until SEC sim time, draining whatever the device sends.
        else if (a == "--serial-poll-until" && i + 1 < argc) {
            serialPollUntil = std::atof(argv[++i]);
            serialPollSec = 0.02; // 50 Hz poll matches the browser
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                serialPollUart = std::atoi(argv[++i]);
                serialPollUartExplicit = true;
            }
        }
        else if (!romPath) romPath = argv[i];
        else {
            std::fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 1;
        }
    }
    if (!romPath) {
        std::fprintf(stderr,
            "usage: %s <rom> [--boot-seconds N] [--post-attach-seconds N]\n"
            "       [--card-size-mb N] [--card-path FILE] [--log-file PATH]\n"
            "       [--screenshot PATH] [--screenshot-every T PREFIX]\n"
            "       [--pc-sample-hz N] [--wake-after-attach]\n"
            "       [--detach-after T] [--cycles N] [--zero-fill]\n"
            "       [--skip-card] [--assert-boot] [--summary-json PATH]\n"
            "       [--min-variance F] [--min-unique-pcs N] [--max-traps N]\n",
            argv[0]);
        return 1;
    }

    // --assert-boot implies PC sampling so we can evaluate unique-PC coverage.
    if (assertBoot && pcSampleHz <= 0) pcSampleHz = 64;

    g_realtimeThrottle = std::getenv("PSION_REALTIME") != nullptr;
    if (g_realtimeThrottle)
        std::fprintf(stderr, "=== PSION_REALTIME: throttling sim to wall-clock ===\n");

    if (logPath) {
        g_logFile = std::fopen(logPath, "w");
        if (!g_logFile) {
            std::fprintf(stderr, "Could not open log file %s\n", logPath);
            return 1;
        }
    }

    auto rom = readFile(romPath);
    std::fprintf(stderr, "Loaded ROM %s (%zu bytes)\n", romPath, rom.size());

    const DeviceProfile *profile = nullptr;
    if (deviceOverride) {
        profile = findProfileById(deviceOverride);
        if (!profile) {
            std::fprintf(stderr, "Unknown device id: %s\n", deviceOverride);
            return 3;
        }
    } else {
        uint32_t variant = detectROMVariant(rom.data(), rom.size());
        profile = findProfileByVariant(variant);
        if (!profile) {
            for (const DeviceProfile *p = allProfiles(); p->id != nullptr; ++p) {
                if (p->romVariantId == 0 && p->createEmulator && p->expectedRomSize == rom.size()) {
                    profile = p;
                    break;
                }
            }
        }
    }
    if (!profile || !profile->createEmulator) {
        std::fprintf(stderr, "No device profile matches ROM (device=%s)\n",
                     deviceOverride ? deviceOverride : "<auto>");
        return 3;
    }
    std::fprintf(stderr, "Using profile: %s (%s)\n", profile->id, profile->displayName);

    EmuBase *emu = profile->createEmulator();
    g_emu = emu;
    emu->setLogger([](const char *s) { emitLog(s); });
    emu->setLoggingEnabled(!quietLogs);
    emu->loadROM(rom.data(), rom.size());

    // Replay a user-supplied RAM snapshot. Loaded after loadROM (which
    // resets the chip + zero-fills RAM) so the bytes survive into
    // executeUntil. The frontend's "Download RAM" button writes its
    // file in the same byte order, so the only requirement is that the
    // snapshot's size matches the device's installed RAM.
    if (ramSnapshotPath) {
        auto snap = readFile(ramSnapshotPath);
        if (snap.empty()) {
            std::fprintf(stderr, "=== harness: --ram-snapshot %s is empty, ignoring ===\n",
                         ramSnapshotPath);
        } else if (snap.size() != emu->getRamSize()) {
            std::fprintf(stderr,
                "=== harness: --ram-snapshot %s is %zu bytes but device expects %zu — loading anyway ===\n",
                ramSnapshotPath, snap.size(), emu->getRamSize());
            emu->loadRamSnapshot(snap.data(), snap.size());
        } else {
            std::fprintf(stderr, "=== harness: --ram-snapshot %s loaded (%zu bytes) ===\n",
                         ramSnapshotPath, snap.size());
            emu->loadRamSnapshot(snap.data(), snap.size());
        }
    }

    // Attach any user-supplied SSD packs (SIBO devices only). When
    // --ssd-attach-after is 0 (default) we attach now (before any
    // cycles) so the OS sees the slot during cold boot; when > 0 we
    // defer to a hot-attach mid-loop to reproduce browser-style
    // user-clicked-Insert behaviour.
    std::vector<uint8_t> ssdImg[4];
    for (int slot = 0; slot < 4; slot++) {
        if (!ssdPath[slot]) continue;
        if (slot >= emu->getSsdSlotCount()) {
            std::fprintf(stderr,
                "=== harness: --ssd-%c ignored, device has %d SSD slot(s) ===\n",
                'a' + slot, emu->getSsdSlotCount());
            continue;
        }
        ssdImg[slot] = readFile(ssdPath[slot]);
        if (ssdAttachAfterSec <= 0.0) {
            bool ok = emu->attachSSD(slot, ssdImg[slot].data(), ssdImg[slot].size(),
                                     ssdType[slot]);
            std::fprintf(stderr,
                "=== harness: cold attachSSD(slot=%d, %s, %zu bytes, type=%d) -> %s ===\n",
                slot, ssdPath[slot], ssdImg[slot].size(), ssdType[slot],
                ok ? "true" : "false");
        } else {
            std::fprintf(stderr,
                "=== harness: deferring SSD slot %d attach to t=%.2fs ===\n",
                slot, ssdAttachAfterSec);
        }
    }

    // Language variant, before any cycles run: the guest reads the index
    // out of its settings PROM early in boot and never looks again.
    if (languageIndex >= 0) {
        if (!emu->setLanguage(languageIndex)) {
            std::fprintf(stderr,
                "=== harness: --language %d ignored, %s has no such language "
                "variant (%d available) ===\n",
                languageIndex, emu->getDeviceName(), emu->getLanguageCount());
        } else {
            std::fprintf(stderr, "=== harness: language = %d (%s) ===\n",
                         languageIndex, emu->getLanguageName(languageIndex));
        }
    }

    // Machine ID, before any cycles run so the kernel's boot-time read of
    // the identity chip already sees the new value.
    if (machineIdSet) {
        if (!emu->hasMachineId()) {
            std::fprintf(stderr,
                "=== harness: --machine-id ignored, %s has no modelled identity PROM ===\n",
                emu->getDeviceName());
        } else {
            emu->setMachineId((uint32_t)(machineIdValue & 0xFFFFFFFFu));
            if (machineIdHasPrefix) {
                const uint32_t wantPrefix = (uint32_t)(machineIdValue >> 32);
                if (!emu->setMachineIdPrefix(wantPrefix))
                    std::fprintf(stderr,
                        "=== harness: high half %08x not applied — %s has no "
                        "patchable model UID ===\n", wantPrefix, emu->getDeviceName());
            }
            // Report it the way EPOC prints it under Information → Machine.
            const uint32_t prefix = emu->getMachineIdPrefix();
            const uint32_t id = emu->getMachineId();
            if (prefix)
                std::fprintf(stderr,
                    "=== harness: Unique id = %04x-%04x-%04x-%04x (requested %llx) ===\n",
                    prefix >> 16, prefix & 0xFFFF, id >> 16, id & 0xFFFF,
                    (unsigned long long)machineIdValue);
            else
                std::fprintf(stderr,
                    "=== harness: Unique id (low half) = %04x-%04x (requested %llx) ===\n",
                    id >> 16, id & 0xFFFF, (unsigned long long)machineIdValue);
        }
    }

    if (cfIrqLine >= 0) {
        std::fprintf(stderr, "=== harness: routing CF IREQ# to pendingInterrupts bit %d ===\n", cfIrqLine);
        emu->setCfIrqLine(cfIrqLine);
    }
    if (cfReenableMode != 0) {
        static const char *modeNames[] = {
            "off", "intenc-block", "deassert-user", "deassert-nonirq",
            "intclear", "level", "delayed", "user-reenable",
            "dispatch-gate-user", "tc2-accelerate", "idle-reenable",
            "rom-sync-call",
        };
        const char *name = (cfReenableMode >= 0 && cfReenableMode < 12)
                           ? modeNames[cfReenableMode] : "?";
        std::fprintf(stderr, "=== harness: CF re-enable strategy = %d (%s) ===\n",
                     cfReenableMode, name);
        emu->setCfReenableMode(cfReenableMode);
    }
    if (cfReschedulePoke) {
        std::fprintf(stderr, "=== harness: CF reschedule-poke workaround ENABLED ===\n");
        emu->setCfReschedulePoke(true);
    }
    if (cfFastWatchdog) {
        std::fprintf(stderr, "=== harness: CF fast-watchdog (30x TC2LOAD scale) ENABLED ===\n");
        emu->setCfFastWatchdog(true);
    }
    if (cfAccelTimerDisable) {
        std::fprintf(stderr, "=== harness: CF 2s-timer accelerator DISABLED (baseline mode) ===\n");
        emu->setCfAccelTimer(false);
    }
    if (cfDirectInvoke) {
        std::fprintf(stderr, "=== harness: CF direct-invoke callback ENABLED ===\n");
        emu->setCfDirectInvoke(true);
    }
    if (cfDirectInvokeDisable) {
        std::fprintf(stderr, "=== harness: CF direct-invoke callback DISABLED (baseline mode) ===\n");
        emu->setCfDirectInvoke(false);
    }

    const int64_t clock = emu->getClockSpeed();
    int64_t frameCycles = clock / 64;  // matches wasm stepFrame
    // PSION_HARNESS_CHUNK — override the executeUntil chunk size (cycles)
    // to mimic the browser's serialPumpCycles 500K-cycle bursts, to test
    // whether fine-grained idle fast-forward changes device link timing.
    if (const char *e = std::getenv("PSION_HARNESS_CHUNK")) {
        int64_t c = std::strtoll(e, nullptr, 0);
        if (c > 0) frameCycles = c;
    }
    auto tStart = std::chrono::steady_clock::now();

    // PC sampling state.
    std::map<uint32_t, uint64_t> pcHist;
    uint64_t pcSamples = 0;

    // Screenshot timelapse state.
    int shotIndex = 0;
    double nextShotAt = screenshotEverySec;

    // Index into keyEvents of the next event to dispatch. Key-down is sent
    // when sim time crosses ev.atSec; key-up is sent ev.holdFrames frames
    // after the down. Multiple keys may be held concurrently (chord
    // support) — each key gets its own (down, hold, up) lifecycle so that
    // e.g. Shift can stay pressed while a letter goes through. Tracked in
    // a small slot table keyed by EPOC code.
    size_t nextKeyIdx = 0;
    size_t nextTapIdx = 0;
    size_t nextDragIdx = 0;
    size_t nextSerialAttachIdx = 0;
    size_t nextSerialDetachIdx = 0;
    size_t nextSerialTxIdx = 0;
    // Sort serial events by atSec so the per-frame dispatch can do a
    // simple forward walk.
    std::sort(serialAttachEvents.begin(), serialAttachEvents.end(),
              [](const SerialAttachEvent &a, const SerialAttachEvent &b) { return a.atSec < b.atSec; });
    std::sort(serialDetachEvents.begin(), serialDetachEvents.end(),
              [](const SerialAttachEvent &a, const SerialAttachEvent &b) { return a.atSec < b.atSec; });
    std::sort(serialTxEvents.begin(), serialTxEvents.end(),
              [](const SerialTxEvent &a, const SerialTxEvent &b) { return a.atSec < b.atSec; });
    // If the user attached a UART but didn't explicitly set the drain
    // port via --serial-poll-until's optional UART arg, point the drain
    // at the first attached UART.  Without this the drain stays on its
    // UART2 default and silently misses bytes the device queues on any
    // other UART — exactly the netBook (UART3 cable) case.
    if (!serialPollUartExplicit && !serialAttachEvents.empty()) {
        serialPollUart = serialAttachEvents.front().uart;
    }
    // Open the binary capture file early so partial captures survive a crash.
    FILE *serialCaptureFp = nullptr;
    if (serialCapturePath) {
        serialCaptureFp = std::fopen(serialCapturePath, "wb");
        if (!serialCaptureFp) {
            std::fprintf(stderr, "Cannot open --serial-capture path %s\n", serialCapturePath);
            return 1;
        }
    }
    // Connect to the external relay socket (if requested). The peer is
    // expected to already be listening (a node process running the real
    // protocol client). We use a non-blocking client socket and shuttle
    // bytes opportunistically inside the poll loop.
    int serialSockFd = -1;
    if (serialBridgeSocket) {
        serialSockFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (serialSockFd < 0) {
            std::fprintf(stderr, "Cannot create --serial-bridge-socket socket: %s\n", std::strerror(errno));
            return 1;
        }
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, serialBridgeSocket, sizeof(addr.sun_path) - 1);
        if (::connect(serialSockFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            std::fprintf(stderr, "Cannot connect --serial-bridge-socket %s: %s\n",
                         serialBridgeSocket, std::strerror(errno));
            return 1;
        }
        int fl = ::fcntl(serialSockFd, F_GETFL, 0);
        ::fcntl(serialSockFd, F_SETFL, fl | O_NONBLOCK);
        std::fprintf(stderr, "=== serial-bridge-socket connected: %s ===\n", serialBridgeSocket);
    }
    // Dispatch the serial bridge calls to whichever emulator class is
    // currently running. Both Windermere::Emulator (Revo / 5mx /
    // 5mxpro / mc218) and Series3c::Emulator (3a / 3c / 3mx / Siena /
    // Workabout / WorkaboutMX) expose the same method names; we hold
    // them in a std::function bundle so the rest of the loop doesn't
    // care which chipset is on the other end.
    struct SerialBridge {
        std::function<bool(int)>                                attach;
        std::function<bool(int)>                                detach;
        std::function<bool(int)>                                isAttached;
        std::function<size_t(int, const uint8_t*, size_t)>      writeFromHost;
        std::function<size_t(int, uint8_t*, size_t)>            readToHost;
        bool ok = false;
    };
    SerialBridge bridge;
    if (auto *w = dynamic_cast<Windermere::Emulator *>(emu)) {
        bridge.attach        = [w](int i)                                  { return w->serialAttachHost(i); };
        bridge.detach        = [w](int i)                                  { return w->serialDetachHost(i); };
        bridge.isAttached    = [w](int i)                                  { return w->serialIsAttached(i); };
        bridge.writeFromHost = [w](int i, const uint8_t *d, size_t n)      { return w->serialWriteFromHost(i, d, n); };
        bridge.readToHost    = [w](int i, uint8_t *d, size_t n)            { return w->serialReadToHost(i, d, n); };
        bridge.ok = true;
    } else if (auto *s = dynamic_cast<Series3c::Emulator *>(emu)) {
        bridge.attach        = [s](int i)                                  { return s->serialAttachHost(i); };
        bridge.detach        = [s](int i)                                  { return s->serialDetachHost(i); };
        bridge.isAttached    = [s](int i)                                  { return s->serialIsAttached(i); };
        bridge.writeFromHost = [s](int i, const uint8_t *d, size_t n)      { return s->serialWriteFromHost(i, d, n); };
        bridge.readToHost    = [s](int i, uint8_t *d, size_t n)            { return s->serialReadToHost(i, d, n); };
        bridge.ok = true;
    } else if (auto *sa = dynamic_cast<SA1100::Emulator *>(emu)) {
        bridge.attach        = [sa](int i)                                 { return sa->serialAttachHost(i); };
        bridge.detach        = [sa](int i)                                 { return sa->serialDetachHost(i); };
        bridge.isAttached    = [sa](int i)                                 { return sa->serialIsAttached(i); };
        bridge.writeFromHost = [sa](int i, const uint8_t *d, size_t n)     { return sa->serialWriteFromHost(i, d, n); };
        bridge.readToHost    = [sa](int i, uint8_t *d, size_t n)           { return sa->serialReadToHost(i, d, n); };
        bridge.ok = true;
    } else if (auto *c = dynamic_cast<CLPS7111::Emulator *>(emu)) {
        // Covers Osaris (CL-PS7111) and Series 5 (CL-PS7110) via inheritance.
        bridge.attach        = [c](int i)                                  { return c->serialAttachHost(i); };
        bridge.detach        = [c](int i)                                  { return c->serialDetachHost(i); };
        bridge.isAttached    = [c](int i)                                  { return c->serialIsAttached(i); };
        bridge.writeFromHost = [c](int i, const uint8_t *d, size_t n)      { return c->serialWriteFromHost(i, d, n); };
        bridge.readToHost    = [c](int i, uint8_t *d, size_t n)            { return c->serialReadToHost(i, d, n); };
        bridge.ok = true;
    }
    if ((serialAttachEvents.size() > 0 || serialTxEvents.size() > 0 || serialPollSec > 0.0)
        && !bridge.ok) {
        std::fprintf(stderr,
            "--serial-* flags require a device with a host serial bridge\n"
            "(Windermere: revo / 5mx / 5mxpro / mc218; SIBO2: 3a / 3c / 3mx / siena / workabout / workaboutmx;\n"
            " SA-1100: series7 / netbook; CL-PS711x: osaris / series5)\n");
        return 1;
    }
    // Per-frame TX-bytes buffer used for the periodic drain when polling.
    std::vector<uint8_t> serialRxScratch(4096);
    HarnessFrameDecoder serialDecoder;
    // Activate the inbound-frame decoder when any auto-rule is in play
    // OR --serial-poll-until is set (so the user sees parsed-frame
    // logging alongside the raw bytes).
    bool serialDecodeFrames = (!serialAutoRules.empty()) || (serialPollUntil > 0.0);
    struct ActiveKey { int epoc; int framesLeft; bool used; bool repeat; };
    std::vector<ActiveKey> activeKeys;
    auto pressKey = [&](int epoc, int holdFrames, bool repeat) {
        emu->setKeyboardKey(static_cast<EpocKey>(epoc), true);
        // Reuse an existing slot for the same key if one is still active —
        // re-pressing extends the hold rather than dropping the matrix
        // bit when the second hold expires before the first.
        for (auto &k : activeKeys) {
            if (k.used && k.epoc == epoc) {
                k.framesLeft = std::max(k.framesLeft, holdFrames);
                k.repeat = k.repeat || repeat;
                return;
            }
        }
        for (auto &k : activeKeys) {
            if (!k.used) {
                k.epoc = epoc; k.framesLeft = holdFrames; k.used = true;
                k.repeat = repeat;
                return;
            }
        }
        activeKeys.push_back({epoc, holdFrames, true, repeat});
    };
    auto tickActiveKeys = [&]() {
        for (auto &k : activeKeys) {
            if (!k.used) continue;
            if (--k.framesLeft <= 0) {
                emu->setKeyboardKey(static_cast<EpocKey>(k.epoc), false);
                k.used = false;
            } else if (k.repeat) {
                // Host-OS auto-repeat: another key-down with no key-up.
                emu->setKeyboardKey(static_cast<EpocKey>(k.epoc), true);
            }
        }
    };

    auto runFor = [&](double seconds, const char *label) {
        int64_t frames = static_cast<int64_t>(seconds * 64);
        std::fprintf(stderr, "=== t=%.2fs %s for %.2fs (%lld frames) ===\n",
                     (double)emu->currentCycles() / (double)clock,
                     label, seconds, (long long)frames);
        const int64_t sampleStride = pcSampleHz > 0 ? std::max((int64_t)1, (int64_t)64 / pcSampleHz) : 0;
        for (int64_t f = 0; f < frames; f++) {
            // Process scripted keyboard events. Press at ev.atSec, hold
            // for ev.holdFrames frames, then release. Multiple keys may
            // be held concurrently — useful for testing modifier+letter
            // chords (Shift+A, Ctrl+letter, etc.).
            double now = (double)emu->currentCycles() / (double)clock;
            // --repl-at-cycle: drop into the snapshot REPL the moment we reach
            // the target cycle (e.g. just before the post-read restart), so the
            // re-init can be stepped/probed interactively.  Fires once.
            if (replAtCycle > 0 && (int64_t)emu->currentCycles() >= replAtCycle) {
                replAtCycle = 0;
                forkRepl(emu);
            }
            // Decrement and release any held keys whose hold has expired.
            tickActiveKeys();
            // Dispatch all events whose at-time has been reached. This lets
            // two CLI events scheduled at the same atSec fire on the same
            // frame — required for chords because the modifier needs to be
            // held BEFORE the letter is observed by the matrix scanner.
            while (nextKeyIdx < keyEvents.size() && now >= keyEvents[nextKeyIdx].atSec) {
                const KeyEvent &ev = keyEvents[nextKeyIdx++];
                pressKey(ev.epocKey, std::max(1, ev.holdFrames), ev.repeat);
                std::fprintf(stderr, "=== t=%.2fs key %d DOWN (hold %d frames%s) ===\n",
                             now, ev.epocKey, ev.holdFrames,
                             ev.repeat ? ", host auto-repeat" : "");
            }
            while (nextTapIdx < tapEvents.size() && now >= tapEvents[nextTapIdx].atSec) {
                const TapEvent &ev = tapEvents[nextTapIdx++];
                std::fprintf(stderr, "=== t=%.2fs tap-seq (%d,%d) ===\n",
                             now, ev.x, ev.y);
                // Inline pen-down → hold N frames → pen-up. Mirrors the
                // doTapAt lambda defined later in this file. 0 is allowed:
                // down and up with no sim frames in between, mimicking a
                // browser tap whose pointerdown/pointerup both land in the
                // same inter-frame gap (e.g. while the frontend bursts
                // frames through a CF mount).
                int holdFrames = 24;
                if (const char *e = std::getenv("PSION_TAP_HOLD_FRAMES")) {
                    holdFrames = std::max(0, std::atoi(e));
                }
                emu->updateTouchInput(ev.x, ev.y, true);
                for (int i = 0; i < holdFrames; i++) {
                    emu->executeUntil(emu->currentCycles() + frameCycles);
                    // PSION_TAP_SHOT_DURING_HOLD: snapshot a PGM while the pen
                    // is held down, so transient pen-down-only UI (e.g. the
                    // System-screen drive-selector popup, which closes on
                    // pen-up) can be captured.  Harness-only diagnostic.
                    if (screenshotEverySec > 0.0 && screenshotPrefix
                        && std::getenv("PSION_TAP_SHOT_DURING_HOLD")) {
                        char hbuf[512];
                        std::snprintf(hbuf, sizeof(hbuf), "%s-hold%03d.pgm",
                                      screenshotPrefix, i);
                        writePGM(emu, hbuf);
                    }
                }
                emu->updateTouchInput(ev.x, ev.y, false);
            }
            while (nextDragIdx < dragEvents.size() && now >= dragEvents[nextDragIdx].atSec) {
                const DragEvent &ev = dragEvents[nextDragIdx++];
                std::fprintf(stderr, "=== t=%.2fs drag-seq (%d,%d) -> (%d,%d) ===\n",
                             now, ev.x1, ev.y1, ev.x2, ev.y2);
                // Press, then walk to the far end in sixteen host steps
                // with four sim frames between them — roughly the rate a
                // real pointer device reports at — then release. The
                // frames in between are what let the guest's own pointer
                // keep up on a machine whose pad is relative.
                const int kDragSteps = 16, kDragFramesPerStep = 4;
                for (int step = 0; step <= kDragSteps; step++) {   // step 0 is the press
                    int x = ev.x1 + (ev.x2 - ev.x1) * step / kDragSteps;
                    int y = ev.y1 + (ev.y2 - ev.y1) * step / kDragSteps;
                    emu->updateTouchInput(x, y, true);
                    for (int f = 0; f < kDragFramesPerStep; f++)
                        emu->executeUntil(emu->currentCycles() + frameCycles);
                }
                // Hold at the far end long enough for the guest to finish
                // travelling there before the button is let go.
                for (int f = 0; f < 32; f++)
                    emu->executeUntil(emu->currentCycles() + frameCycles);
                emu->updateTouchInput(ev.x2, ev.y2, false);
            }
            // Serial-bridge attach / TX events scheduled for this sim time.
            while (bridge.ok && nextSerialAttachIdx < serialAttachEvents.size()
                   && now >= serialAttachEvents[nextSerialAttachIdx].atSec) {
                const auto &ev = serialAttachEvents[nextSerialAttachIdx++];
                bool ok = bridge.attach(ev.uart);
                std::fprintf(stderr, "=== t=%.2fs serial-attach UART%d -> %s ===\n",
                             now, ev.uart, ok ? "ok" : "FAILED");
            }
            while (bridge.ok && nextSerialDetachIdx < serialDetachEvents.size()
                   && now >= serialDetachEvents[nextSerialDetachIdx].atSec) {
                const auto &ev = serialDetachEvents[nextSerialDetachIdx++];
                bool ok = bridge.detach(ev.uart);
                std::fprintf(stderr, "=== t=%.2fs serial-detach UART%d -> %s ===\n",
                             now, ev.uart, ok ? "ok" : "FAILED");
            }
            while (bridge.ok && nextSerialTxIdx < serialTxEvents.size()
                   && now >= serialTxEvents[nextSerialTxIdx].atSec) {
                const auto &ev = serialTxEvents[nextSerialTxIdx++];
                std::vector<uint8_t> wire = ev.framed ? encodePlpFrame(ev.bytes) : ev.bytes;
                size_t accepted = bridge.writeFromHost(ev.uart, wire.data(), wire.size());
                std::fprintf(stderr, "=== t=%.2fs serial-tx UART%d %s (%zu bytes, %zu accepted): ",
                             now, ev.uart, ev.framed ? "framed" : "raw", wire.size(), accepted);
                for (uint8_t b : wire) std::fprintf(stderr, "%02x ", b);
                std::fprintf(stderr, "===\n");
            }
            if (feedMicSilence && emu->hasAudio()) {
                static const int16_t silence[125] = {0};
                emu->writeAudioInput(silence, 125);
            }
            emu->executeUntil(emu->currentCycles() + frameCycles);
            // After stepping the frame, drain whatever the device transmitted
            // on its UART so it shows up in --serial-capture and stderr,
            // and run auto-responder rules against any complete frames.
            // Socket relay: inject any bytes the external client sent us
            // into the UART RX FIFO before draining the device's TX.
            if (serialSockFd >= 0 && bridge.ok) {
                uint8_t sockBuf[512];
                ssize_t rn = ::recv(serialSockFd, sockBuf, sizeof(sockBuf), 0);
                if (rn > 0) {
                    bridge.writeFromHost(serialPollUart, sockBuf, (size_t)rn);
                }
            }
            if (bridge.ok && (serialCaptureFp || serialPollUntil > 0.0 || !serialAutoRules.empty() || serialSockFd >= 0)) {
                size_t got = bridge.readToHost(
                    serialPollUart, serialRxScratch.data(), serialRxScratch.size());
                if (got > 0) {
                    if (serialSockFd >= 0) {
                        // Best-effort write to the relay peer; the socket is
                        // non-blocking, so a transient EAGAIN just drops a
                        // chunk (the JS framer resyncs on the next BOF).
                        ::send(serialSockFd, serialRxScratch.data(), got, MSG_NOSIGNAL);
                    }
                    if (serialCaptureFp) {
                        std::fwrite(serialRxScratch.data(), 1, got, serialCaptureFp);
                        std::fflush(serialCaptureFp);
                    }
                    std::fprintf(stderr, "=== t=%.2fs serial-rx UART%d (%zu bytes): ",
                                 now, serialPollUart, got);
                    for (size_t k = 0; k < got; k++) std::fprintf(stderr, "%02x ", serialRxScratch[k]);
                    std::fprintf(stderr, "===\n");
                    if (serialDecodeFrames) {
                        auto frames = serialDecoder.feed(serialRxScratch.data(), got);
                        for (const auto &payload : frames) {
                            if (payload.empty()) continue;
                            std::fprintf(stderr, "=== t=%.2fs serial-frame type=%02x len=%zu: ",
                                         now, payload[0], payload.size());
                            for (uint8_t b : payload) std::fprintf(stderr, "%02x ", b);
                            std::fprintf(stderr, "===\n");
                            for (const auto &rule : serialAutoRules) {
                                if (rule.matchType != payload[0]) continue;
                                const auto &replyPayload = rule.echo ? payload : rule.replyBytes;
                                auto wire = encodePlpFrame(replyPayload);
                                size_t accepted = bridge.writeFromHost(
                                    serialPollUart, wire.data(), wire.size());
                                std::fprintf(stderr,
                                    "=== t=%.2fs auto-rule [type=%02x %s] -> %zu/%zu bytes: ",
                                    now, rule.matchType, rule.echo ? "echo" : "reply",
                                    accepted, wire.size());
                                for (uint8_t b : wire) std::fprintf(stderr, "%02x ", b);
                                std::fprintf(stderr, "===\n");
                                break;  // first matching rule wins
                            }
                        }
                    }
                }
            }
            // Hot-attach: when --ssd-attach-after is set, wait until
            // that many sim-seconds have elapsed then plug each pack
            // in. Mirrors what the browser dialog does when the user
            // clicks "Insert into device" mid-session.
            if (ssdAttachAfterSec > 0.0) {
                double now = (double)emu->currentCycles() / (double)clock;
                if (now >= ssdAttachAfterSec) {
                    for (int slot = 0; slot < 4; slot++) {
                        if (ssdImg[slot].empty()) continue;
                        bool ok = emu->attachSSD(slot, ssdImg[slot].data(), ssdImg[slot].size(),
                                                 ssdType[slot]);
                        std::fprintf(stderr,
                            "=== harness: t=%.2fs hot attachSSD(slot=%d, %zu bytes, type=%d) -> %s ===\n",
                            now, slot, ssdImg[slot].size(), ssdType[slot], ok ? "true" : "false");
                        ssdImg[slot].clear();   // one-shot
                    }
                    ssdAttachAfterSec = 0.0;     // disarm
                }
            }
            if (pcSampleHz > 0 && (f % sampleStride) == 0) {
                pcHist[emu->lastPcExecuted() & ~1u]++;
                pcSamples++;
            }
            if (screenshotEverySec > 0.0 && screenshotPrefix) {
                double now = (double)emu->currentCycles() / (double)clock;
                if (now >= nextShotAt) {
                    char buf[512];
                    std::snprintf(buf, sizeof(buf), "%s-%03d.pgm", screenshotPrefix, shotIndex++);
                    writePGM(emu, buf);
                    nextShotAt += screenshotEverySec;
                }
            }
            if (g_realtimeThrottle) {
                static auto rtStart = std::chrono::steady_clock::now();
                static int64_t rtFrame = 0;
                rtFrame++;
                auto target = rtStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>((double)rtFrame / 64.0));
                std::this_thread::sleep_until(target);
            }
        }
    };

    auto doWakeTap = [&]() {
        std::fprintf(stderr, "=== t=%.2fs synthetic touch wake ===\n",
                     (double)emu->currentCycles() / (double)clock);
        emu->updateTouchInput(320, 120, true);
        for (int i = 0; i < 4; i++) emu->executeUntil(emu->currentCycles() + frameCycles);
        emu->updateTouchInput(320, 120, false);
    };

    auto doTapAt = [&](int x, int y) {
        std::fprintf(stderr, "=== t=%.2fs tap at (%d,%d) ===\n",
                     (double)emu->currentCycles() / (double)clock, x, y);
        emu->updateTouchInput(x, y, true);
        // PSION_TAP_HOLD_FRAMES=N (default 24 = 0.375 sim s) — hold duration
        // for synthetic tap. Longer values let slow pen-driver state machines
        // (e.g., Series 5 EINT3 polling debounce + ADS7843 scan) complete.
        int holdFrames = 24;
        if (const char *e = std::getenv("PSION_TAP_HOLD_FRAMES")) {
            holdFrames = std::max(1, std::atoi(e));
        }
        for (int i = 0; i < holdFrames; i++) emu->executeUntil(emu->currentCycles() + frameCycles);
        emu->updateTouchInput(x, y, false);
        for (int i = 0; i < 24; i++) emu->executeUntil(emu->currentCycles() + frameCycles);
    };

    // PSION_MULTI_TAP=N: do N taps with PSION_TAP_GAP seconds between, jittering
    // position by PSION_TAP_JITTER px each (set 0 to tap the same spot).  Used
    // to reproduce "subsequent taps stall / 1-in-N taps dropped" bugs.  Shared
    // by the skip-card and the card-attached (netBook) post-boot paths.
    auto doMultiTap = [&]() {
        int multiTap = 1;
        if (const char *e = std::getenv("PSION_MULTI_TAP")) multiTap = std::max(1, std::atoi(e));
        int jit = 20;
        if (const char *e = std::getenv("PSION_TAP_JITTER")) jit = std::atoi(e);
        double gap = 1.5;
        if (const char *e = std::getenv("PSION_TAP_GAP")) gap = std::atof(e);
        // PSION_TAP_ALT_X/Y: if set, ODD taps go to this alternate spot so each
        // tap is individually observable (highlight moves back/forth).
        int altX = -1, altY = -1;
        if (const char *e = std::getenv("PSION_TAP_ALT_X")) altX = std::atoi(e);
        if (const char *e = std::getenv("PSION_TAP_ALT_Y")) altY = std::atoi(e);
        for (int t = 0; t < multiTap; t++) {
            if (altX >= 0 && (t & 1))
                doTapAt(altX, altY);
            else
                doTapAt(tapX + t * jit, tapY + t * jit / 2);
            if (t + 1 < multiTap) runFor(gap, "between-taps");
        }
    };

    // --serial-poll-until overrides the boot/post-attach phasing: run
    // the simulator continuously up to the requested sim time and
    // dispatch every serial event along the way. CF / SSD attach is
    // skipped automatically since this is a protocol-investigation
    // mode rather than a boot-validation run.
    if (serialPollUntil > 0.0) {
        bootSeconds = serialPollUntil;
        skipCard = true;
    }

    if (feedMicSilence && emu->hasAudio()) {
        // Required so writeAudioInput() actually enqueues the silence
        // we push each tick — Windermere's writeAudioInput returns
        // early if hostMicEnabled() is false. Also flip speaker on so
        // we match the real-world "mic + speaker enabled" scenario
        // the user reports the 5mxPro REC crash from.
        emu->setHostAudioEnabled(true, true);
        std::fprintf(stderr, "=== feed-mic-silence: setHostAudioEnabled(speaker=1, mic=1) ===\n");
    }

    runFor(bootSeconds, "boot");

    if (skipCard) {
        std::fprintf(stderr, "=== harness: --skip-card, no CF attach ===\n");
        if (tapX >= 0) {
            runFor(std::max(0.5, tapAfterSec), "pre-tap");
            doMultiTap();
            runFor(std::max(1.0, postAttachSeconds), "post-tap");
        }
    }

    std::vector<uint8_t> card;
    if (!skipCard) {
    if (cardPath) {
        card = readFile(cardPath);
        std::fprintf(stderr, "=== harness: using %s (%zu bytes) as CF image ===\n",
                     cardPath, card.size());
    } else if (zeroFill) {
        card.assign(static_cast<size_t>(cardSizeMb) * 1024 * 1024, 0);
        std::fprintf(stderr, "=== harness: using %d MB zero-filled CF image "
                             "(PartitionInfo will reject, retry storm expected) ===\n",
                     cardSizeMb);
    } else {
        card = makePsionBlankImage(static_cast<size_t>(cardSizeMb) * 1024 * 1024);
        std::fprintf(stderr, "=== harness: using %d MB Psion-formatted blank CF image ===\n",
                     cardSizeMb);
    }

    for (int c = 0; c < cycles; c++) {
        std::fprintf(stderr, "=== harness: cycle %d/%d ATTACH ===\n", c + 1, cycles);
        bool ok = emu->attachCard(card.data(), card.size());
        std::fprintf(stderr, "=== harness: attachCard() returned %s ===\n", ok ? "true" : "false");
        if (wakeAfterAttach) doWakeTap();

        if (detachAfterSec > 0.0) {
            runFor(detachAfterSec, "pre-detach");
            std::fprintf(stderr, "=== harness: cycle %d/%d DETACH ===\n", c + 1, cycles);
            emu->detachCard();
            runFor(std::max(0.5, postAttachSeconds - detachAfterSec), "post-detach");
        } else if (tapX >= 0 && tapAfterSec > 0.0 && tapAfterSec < postAttachSeconds) {
            runFor(tapAfterSec, "pre-tap");
            doTapAt(tapX, tapY);
            runFor(postAttachSeconds - tapAfterSec, "post-tap");
        } else {
            runFor(postAttachSeconds, "post-attach");
            if (tapX >= 0) {
                // PSION_MULTI_TAP drives a tap-reliability stress here too —
                // the netBook boots from CF so it can't use --skip-card; this
                // is the only path that reaches its booted desktop.
                if (std::getenv("PSION_MULTI_TAP")) doMultiTap();
                else                                doTapAt(tapX, tapY);
                runFor(5.0, "post-final-tap");
            }
        }
    }

    // Hot-swap: slide a different card into the bay after the OS is up.
    if (swapCardPath) {
        std::vector<uint8_t> swapCard = readFile(swapCardPath);
        if (std::getenv("PSION_HARNESS_INPLACE_SWAP")) {
            // In-place content update: replace the card bytes without an
            // eject/insert, leaving the OS mount alive (tests whether a fresh
            // disk access re-reads the new contents).
            std::fprintf(stderr, "=== harness: IN-PLACE SWAP — update bytes %s ===\n", swapCardPath);
            bool ok = emu->updateCardImageInPlace(swapCard.data(), swapCard.size());
            std::fprintf(stderr, "=== harness: in-place update returned %s ===\n",
                         ok ? "true" : "false");
        } else {
            std::fprintf(stderr, "=== harness: SWAP — detach, attach %s ===\n", swapCardPath);
            emu->detachCard();
            runFor(2.0, "post-swap-detach");
            std::fprintf(stderr, "=== harness: swap card %s (%zu bytes) ===\n",
                         swapCardPath, swapCard.size());
            bool ok = emu->attachCard(swapCard.data(), swapCard.size());
            std::fprintf(stderr, "=== harness: swap attachCard() returned %s ===\n",
                         ok ? "true" : "false");
        }
        runFor(swapPostSeconds, "post-swap-attach");
    }
    } // end !skipCard

    std::fprintf(stderr, "=== harness: done, cardInserted=%s cycles=%llu (sim-time %.2fs) ===\n",
                 emu->isCardInserted() ? "true" : "false",
                 (unsigned long long)emu->currentCycles(),
                 (double)emu->currentCycles() / (double)clock);

    // Snapshot REPL: pause here (typically the post-read park) and fork() per
    // experiment so each starts from this exact state.  See forkRepl().
    if (forkReplMode)
        forkRepl(emu);

    // CF stats dump — parsed by scripts/cf-reenable-sweep.sh to score
    // candidate re-enable strategies. Single "CF_STATS:" line keeps the
    // grep in the sweep script trivial.
    {
        auto s = emu->getCfStats();
        double simSec = (double)emu->currentCycles() / (double)clock;
        auto tEnd = std::chrono::steady_clock::now();
        double wallSec = std::chrono::duration<double>(tEnd - tStart).count();
        std::fprintf(stderr,
                     "CF_STATS: sim_s=%.2f wall_s=%.2f ata_cmds=%u sector_drains=%u "
                     "irq_up=%u irq_down=%u eint3_dispatches=%u resched_pokes=%u "
                     "accel_timer_hits=%u\n",
                     simSec, wallSec, s.ataCommands, s.sectorDrains,
                     s.irqAssertions, s.irqDeassertions, s.eint3Dispatches,
                     s.reschedulePokes, s.accelTimerHits);
        // SA-1100 throughput: executed (ticked) cycles vs sim cycles. The
        // delta is idle fast-forwarded through WFI; executed/wall is the raw
        // interpreter speed that bounds a CPU-bound app like a game. Only
        // available when the core is built with -DPSION_PROFILE_CYCLES.
#ifdef PSION_PROFILE_CYCLES
        extern uint64_t g_sa1100ExecutedCycles;
        if (g_sa1100ExecutedCycles > 0) {
            double execMHz = g_sa1100ExecutedCycles / wallSec / 1e6;
            double rtMHz   = clock / 1e6;
            std::fprintf(stderr,
                "SA1100_THROUGHPUT: executed_cycles=%llu sim_cycles=%llu "
                "idle_frac=%.1f%% exec_MHz=%.1f realtime_MHz=%.1f "
                "busy_realtime_ratio=%.3fx\n",
                (unsigned long long)g_sa1100ExecutedCycles,
                (unsigned long long)emu->currentCycles(),
                100.0 * (1.0 - (double)g_sa1100ExecutedCycles / (double)emu->currentCycles()),
                execMHz, rtMHz, execMHz / rtMHz);
        }
#endif
    }

    if (pcSampleHz > 0 && pcSamples > 0) {
        std::vector<std::pair<uint64_t, uint32_t>> sorted;
        sorted.reserve(pcHist.size());
        for (auto &kv : pcHist) sorted.emplace_back(kv.second, kv.first);
        std::sort(sorted.begin(), sorted.end(),
                  [](auto &a, auto &b) { return a.first > b.first; });
        std::fprintf(stderr,
                     "=== PC sampling: %llu samples, %zu unique PCs, top 30: ===\n",
                     (unsigned long long)pcSamples, sorted.size());
        for (size_t i = 0; i < std::min<size_t>(30, sorted.size()); i++) {
            double pct = 100.0 * (double)sorted[i].first / (double)pcSamples;
            std::fprintf(stderr, "  pc=%08x  hits=%llu  %5.1f%%\n",
                         sorted[i].second,
                         (unsigned long long)sorted[i].first, pct);
        }
    }

    if (screenshotPath) writePGM(emu, screenshotPath);

    // Boot-validation: compute LCD stats from the final framebuffer, decide
    // pass/fail against thresholds, emit a JSON summary for scripting, and
    // (if --assert-boot) exit non-zero on failure.
    int w, h;
    auto finalGray = readGrayscale(emu, w, h);
    LcdStats ls = computeLcdStats(finalGray);
    size_t uniquePcs = pcHist.size();
    char maxVarLabel[48] = {0};
    if (maxVariance >= 0.0)
        std::snprintf(maxVarLabel, sizeof(maxVarLabel), " and <= %.2f", maxVariance);

    bool passVariance = ls.variance >= minVariance &&
                        (maxVariance < 0.0 || ls.variance <= maxVariance);
    bool passPcs = (int)uniquePcs >= minUniquePcs || pcSampleHz <= 0;
    bool passTraps = (int64_t)g_trapCount <= maxTraps;
    // Count non-paper pixels for the "no SIBO2 dialog stuck on screen" check.
    // Paper pixels render as 160 in 8-bit grayscale (see series3c.cpp's
    // readLCDIntoBuffer, which maps the paper colour to 160 for the PGM
    // path). Anything else is text/UI content; the dialog renders to a
    // dense rectangle that pushes the count well above the System screen.
    int nonPaperCount = 0;
    for (uint8_t v : finalGray) if (v != 160) nonPaperCount++;
    bool passNonPaper = maxNonPaper < 0 || nonPaperCount <= maxNonPaper;

    MdriveInfo mdriveInfo;
    {
        const uint8_t *p = emu->getRamBuffer();
        size_t n = emu->getRamSize();
        if (p && n) mdriveInfo = scanMdrive(p, n);
    }
    // Require the volume label to be present (disk was cold-formatted) AND at least
    // N subdirectory entries (0 = just the label; 5 for FAT16 devices like 3c/3mx).
    bool passMdrive = minMdriveSubdirs < 0 ||
                      (mdriveInfo.labelFound && mdriveInfo.subdirsFound >= minMdriveSubdirs);

    bool allPass = passVariance && passPcs && passTraps && passNonPaper && passMdrive;

    std::fprintf(stderr,
                 "=== LCD stats: mean=%.2f variance=%.2f unique=%zu min=%u max=%u ===\n",
                 ls.mean, ls.variance, ls.uniqueValues, ls.minV, ls.maxV);
    std::fprintf(stderr,
                 "=== Boot check: variance %s (%.2f >= %.2f%s) pcs %s (%zu >= %d) traps %s (%llu <= %d) ===\n",
                 passVariance ? "PASS" : "FAIL", ls.variance, minVariance,
                 maxVariance >= 0.0 ? maxVarLabel : "",
                 passPcs ? "PASS" : "FAIL", uniquePcs, minUniquePcs,
                 passTraps ? "PASS" : "FAIL",
                 (unsigned long long)g_trapCount, maxTraps);
    if (maxNonPaper >= 0) {
        std::fprintf(stderr,
                     "=== Boot check: nonpaper %s (%d <= %d) ===\n",
                     passNonPaper ? "PASS" : "FAIL", nonPaperCount, maxNonPaper);
    }
    // Screen orientation the OS is drawing at (quarter-turns anticlockwise
    // the panel image needs to be shown at).  Always 0 except on a netpad
    // whose Tools menu → "Switch orientation" has been used.
    const int screenOrientation = emu->getScreenOrientation();
    std::fprintf(stderr, "=== Screen orientation: %d ===\n", screenOrientation);
    std::fprintf(stderr,
                 "=== M: drive: label=%s subdirs=%d dirs=[%s]%s ===\n",
                 mdriveInfo.labelFound ? "yes" : "no",
                 mdriveInfo.subdirsFound, mdriveInfo.subdirList.c_str(),
                 minMdriveSubdirs >= 0
                     ? (passMdrive ? " PASS" : " FAIL")
                     : "");

    // Single-line JSON summary. Goes to --summary-json file if given, and
    // always to stderr with a "PSION_BOOT_JSON:" prefix so scripts can grep it
    // out of mixed output (hardware.h uses printf direct to stdout).
    auto emitJson = [&](FILE *out, const char *prefix) {
        std::fprintf(out,
                     "%s{\"device\":\"%s\",\"rom\":\"%s\",\"variance\":%.2f,"
                     "\"unique_values\":%zu,\"mean\":%.2f,"
                     "\"unique_pcs\":%zu,\"pc_samples\":%llu,"
                     "\"traps\":%llu,\"cycles\":%llu,\"sim_seconds\":%.2f,"
                     "\"mdrive_label\":%s,\"mdrive_subdirs\":%d,\"mdrive_dirs\":\"%s\","
                     "\"orientation\":%d,\"pass\":%s}\n",
                     prefix, profile->id, romPath, ls.variance, ls.uniqueValues, ls.mean,
                     uniquePcs, (unsigned long long)pcSamples,
                     (unsigned long long)g_trapCount,
                     (unsigned long long)emu->currentCycles(),
                     (double)emu->currentCycles() / (double)clock,
                     mdriveInfo.labelFound ? "true" : "false",
                     mdriveInfo.subdirsFound, mdriveInfo.subdirList.c_str(),
                     screenOrientation, allPass ? "true" : "false");
    };
    // Dump final SSD pack images (capturing any guest-side writes) so
    // tests can verify in-device file operations from the host.
    for (int slot = 0; slot < 4; slot++) {
        if (!ssdDumpPath[slot]) continue;
        const uint8_t *p = emu->getSSDImageData(slot);
        size_t n = emu->getSSDImageSize(slot);
        if (!p || n == 0) {
            std::fprintf(stderr, "=== harness: --ssd-dump-%c skipped, slot empty ===\n",
                         'a' + slot);
            continue;
        }
        FILE *df = std::fopen(ssdDumpPath[slot], "wb");
        if (df) {
            std::fwrite(p, 1, n, df);
            std::fclose(df);
            std::fprintf(stderr, "=== harness: dumped SSD slot %d (%zu bytes) to %s ===\n",
                         slot, n, ssdDumpPath[slot]);
        } else {
            std::fprintf(stderr, "warning: could not open %s for SSD dump\n",
                         ssdDumpPath[slot]);
        }
    }

    emitJson(stderr, "PSION_BOOT_JSON:");
    if (summaryJsonPath) {
        FILE *jf = std::fopen(summaryJsonPath, "w");
        if (jf) {
            emitJson(jf, "");
            std::fclose(jf);
        } else {
            std::fprintf(stderr, "warning: could not open %s for JSON summary\n", summaryJsonPath);
        }
    }

    if (saveRamSnapshotPath) {
        const uint8_t *p = emu->getRamBuffer();
        size_t n = emu->getRamSize();
        if (p && n) {
            FILE *f = std::fopen(saveRamSnapshotPath, "wb");
            if (f) {
                std::fwrite(p, 1, n, f);
                std::fclose(f);
                std::fprintf(stderr, "=== harness: saved %zu bytes of RAM to %s ===\n",
                             n, saveRamSnapshotPath);
            } else {
                std::fprintf(stderr, "warning: could not open %s for --save-ram-snapshot\n",
                             saveRamSnapshotPath);
            }
        }
    }

    if (serialSockFd >= 0) ::close(serialSockFd);
    if (g_logFile) std::fclose(g_logFile);
    delete emu;
    return (assertBoot && !allPass) ? 1 : 0;
}
