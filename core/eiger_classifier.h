// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstdint>
#include <cstddef>

// PSION_S7_ASIC_CLASSIFY=1: full per-offset classifier for Eiger ASIC
// accesses.  Called from Emulator::readAsic / writeAsic for every byte
// access (8-bit) to the companion-ASIC register space.  Dump the
// summary via dumpSummary() at end-of-run.
//
// Goal: identify which of the ~256 Eiger registers are read, written,
// or polled, what value ranges appear, and which kernel PCs touch
// them.  This is the empirical input for fleshing out core/eiger.cpp
// beyond its current scratch-RAM model.

namespace EigerClassify {

constexpr uint32_t kAddrSpace = 0x1000;   // 4 KB Eiger byte-addressable

struct OffsetStats {
    uint32_t readCount    = 0;
    uint32_t writeCount   = 0;
    int64_t  firstCyc     = -1;
    int64_t  lastCyc      = -1;

    static constexpr int kMaxVals = 12;
    uint8_t  uniqueReadVals[kMaxVals]   = {};
    int      uniqueReadValN             = 0;
    uint8_t  uniqueWriteVals[kMaxVals]  = {};
    int      uniqueWriteValN            = 0;

    static constexpr int kMaxPCs = 8;
    uint32_t uniqueReadPCs[kMaxPCs]   = {};
    int      uniqueReadPCN            = 0;
    uint32_t uniqueWritePCs[kMaxPCs]  = {};
    int      uniqueWritePCN           = 0;

    // Poll detection: count of consecutive reads-without-intervening-write.
    // A "poll" is read with the same value repeatedly.  We tally the
    // single longest run.
    uint32_t pollRunCurrent = 0;     // current run length
    uint32_t pollRunMax     = 0;     // max run observed
    uint8_t  pollRunValue   = 0;     // value being polled
};

extern OffsetStats stats[kAddrSpace];
extern bool        enabled;
extern bool        registered;

// Init the classifier (reads env var once, registers atexit dump).
// Safe to call multiple times — idempotent.
void init();

// Record a single 8-bit read of the ASIC offset.
inline void recordRead(uint32_t offset, uint8_t value, uint32_t pc, int64_t cyc) {
    if (!enabled) return;
    if (offset >= kAddrSpace) return;
    OffsetStats &s = stats[offset];
    if (s.firstCyc < 0) s.firstCyc = cyc;
    s.lastCyc = cyc;
    s.readCount++;
    // Poll-run: increment if same value as previous poll, else reset.
    if (s.pollRunCurrent > 0 && s.pollRunValue == value) {
        s.pollRunCurrent++;
    } else {
        s.pollRunCurrent = 1;
        s.pollRunValue   = value;
    }
    if (s.pollRunCurrent > s.pollRunMax) s.pollRunMax = s.pollRunCurrent;
    // Unique values.
    bool seen = false;
    for (int i = 0; i < s.uniqueReadValN; i++)
        if (s.uniqueReadVals[i] == value) { seen = true; break; }
    if (!seen && s.uniqueReadValN < OffsetStats::kMaxVals)
        s.uniqueReadVals[s.uniqueReadValN++] = value;
    // Unique PCs.
    seen = false;
    for (int i = 0; i < s.uniqueReadPCN; i++)
        if (s.uniqueReadPCs[i] == pc) { seen = true; break; }
    if (!seen && s.uniqueReadPCN < OffsetStats::kMaxPCs)
        s.uniqueReadPCs[s.uniqueReadPCN++] = pc;
}

// Record a single 8-bit write.
inline void recordWrite(uint32_t offset, uint8_t value, uint32_t pc, int64_t cyc) {
    if (!enabled) return;
    if (offset >= kAddrSpace) return;
    OffsetStats &s = stats[offset];
    if (s.firstCyc < 0) s.firstCyc = cyc;
    s.lastCyc = cyc;
    s.writeCount++;
    // A write breaks any current poll run on this offset.
    s.pollRunCurrent = 0;
    // Unique values.
    bool seen = false;
    for (int i = 0; i < s.uniqueWriteValN; i++)
        if (s.uniqueWriteVals[i] == value) { seen = true; break; }
    if (!seen && s.uniqueWriteValN < OffsetStats::kMaxVals)
        s.uniqueWriteVals[s.uniqueWriteValN++] = value;
    // Unique PCs.
    seen = false;
    for (int i = 0; i < s.uniqueWritePCN; i++)
        if (s.uniqueWritePCs[i] == pc) { seen = true; break; }
    if (!seen && s.uniqueWritePCN < OffsetStats::kMaxPCs)
        s.uniqueWritePCs[s.uniqueWritePCN++] = pc;
}

// Write the summary report to stderr (and optionally to a file path
// given by PSION_S7_ASIC_CLASSIFY_FILE).  Registered as atexit handler
// by init().  Idempotent.
void dumpSummary();

// Call from the emulator's main loop periodically; rewrites the
// output file every ~5 sim seconds so a snapshot exists even if the
// harness is killed mid-boot.
void maybePeriodicDump(int64_t passedCycles);

} // namespace EigerClassify
