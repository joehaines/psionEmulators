// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "eiger_classifier.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>

namespace EigerClassify {

OffsetStats stats[kAddrSpace];
bool        enabled    = false;
bool        registered = false;

static const char *outputPath = nullptr;

static const char *classify(const OffsetStats &s) {
    if (s.readCount == 0 && s.writeCount == 0)  return "<unused>";
    if (s.readCount == 0)                        return "WRITE-ONLY";
    if (s.writeCount == 0)                       return "READ-ONLY";
    if (s.pollRunMax >= 8)                       return "POLLED";
    if (s.readCount > s.writeCount * 4)          return "READ-MOSTLY";
    if (s.writeCount > s.readCount * 4)          return "WRITE-MOSTLY";
    return "R/W";
}

static void dumpToFile(FILE *out) {
    std::fprintf(out, "=== Eiger ASIC access summary ===\n");
    std::fprintf(out, "  offsets: %u total used\n",
                 [](){ uint32_t n=0; for(uint32_t i=0;i<kAddrSpace;i++) if(stats[i].readCount||stats[i].writeCount) n++; return n; }());
    std::fprintf(out,
        "Columns: offset class            R-cnt W-cnt poll-run cyc-range                  values-read         values-written      PCs\n");
    for (uint32_t off = 0; off < kAddrSpace; off++) {
        const OffsetStats &s = stats[off];
        if (s.readCount == 0 && s.writeCount == 0) continue;

        std::fprintf(out, "  0x%04x %-15s %5u %5u %8u %10lld..%-10lld  ",
                     off,
                     classify(s),
                     s.readCount,
                     s.writeCount,
                     s.pollRunMax,
                     (long long)s.firstCyc,
                     (long long)s.lastCyc);

        std::fprintf(out, "[");
        for (int i = 0; i < s.uniqueReadValN; i++) {
            if (i) std::fprintf(out, ",");
            std::fprintf(out, "%02x", s.uniqueReadVals[i]);
        }
        std::fprintf(out, "%s]", s.uniqueReadValN == OffsetStats::kMaxVals ? "+" : "");

        std::fprintf(out, " [");
        for (int i = 0; i < s.uniqueWriteValN; i++) {
            if (i) std::fprintf(out, ",");
            std::fprintf(out, "%02x", s.uniqueWriteVals[i]);
        }
        std::fprintf(out, "%s] ", s.uniqueWriteValN == OffsetStats::kMaxVals ? "+" : "");

        // PCs: print a compact set of unique kernel addresses.
        std::fprintf(out, "rdPC=[");
        for (int i = 0; i < s.uniqueReadPCN; i++) {
            if (i) std::fprintf(out, ",");
            std::fprintf(out, "%08x", s.uniqueReadPCs[i]);
        }
        std::fprintf(out, "%s] ", s.uniqueReadPCN == OffsetStats::kMaxPCs ? "+" : "");
        std::fprintf(out, "wrPC=[");
        for (int i = 0; i < s.uniqueWritePCN; i++) {
            if (i) std::fprintf(out, ",");
            std::fprintf(out, "%08x", s.uniqueWritePCs[i]);
        }
        std::fprintf(out, "%s]\n", s.uniqueWritePCN == OffsetStats::kMaxPCs ? "+" : "");
    }
    std::fprintf(out, "=== end Eiger ASIC access summary ===\n");
    std::fflush(out);
}

void maybePeriodicDump(int64_t passedCycles) {
    if (!enabled) return;
    // SA-1100 CLOCK_SPEED is 3.6864 MHz; dump every ~5 sim sec.
    static int64_t nextDumpCyc = 5LL * 3686400LL;
    if (passedCycles < nextDumpCyc) return;
    nextDumpCyc = passedCycles + 5LL * 3686400LL;
    dumpSummary();
}

void dumpSummary() {
    if (!enabled) return;
    if (outputPath && outputPath[0]) {
        FILE *f = std::fopen(outputPath, "w");
        if (f) {
            dumpToFile(f);
            std::fclose(f);
            std::fprintf(stderr, "[eiger-classify] wrote summary to %s\n", outputPath);
            return;
        }
        std::fprintf(stderr, "[eiger-classify] could not open %s, falling back to stderr\n",
                     outputPath);
    }
    dumpToFile(stderr);
}

// Signal handler so the dump fires even on SIGINT / SIGTERM from
// `pkill` or Ctrl-C.  Cannot allocate / take locks; we just call
// dumpSummary() (which uses stdio) and then re-raise the signal with
// the default handler.
static void onSignal(int sig) {
    dumpSummary();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void init() {
    if (registered) return;
    registered = true;
    const char *e = std::getenv("PSION_S7_ASIC_CLASSIFY");
    enabled = (e && e[0] && e[0] != '0');
    if (!enabled) return;
    outputPath = std::getenv("PSION_S7_ASIC_CLASSIFY_FILE");
    std::atexit(&dumpSummary);
    std::signal(SIGINT,  onSignal);
    std::signal(SIGTERM, onSignal);
    std::fprintf(stderr, "[eiger-classify] enabled%s%s\n",
                 outputPath ? "; output=" : "",
                 outputPath ? outputPath : "");
}

} // namespace EigerClassify
