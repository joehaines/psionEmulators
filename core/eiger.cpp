// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "eiger.h"
#include "arm710.h"
#include "eiger_classifier.h"
#include <cstdio>
#include <cstring>

// Psion "Eiger" companion ASIC for the Series 7 / netBook (SA-1100).
// See eiger.h for the rationale and architectural comparison with the
// 5mx's ETNA ASIC (core/etna.cpp).
//
// The Eiger register map is not publicly documented.  The offsets and
// semantics implemented here come from observing what the EKA1 kernel
// BSP and platform-variant init code read/write during boot.  The
// per-byte access classifier (core/eiger_classifier.cpp, env var
// PSION_S7_ASIC_CLASSIFY=1) produces an empirical map of every
// offset's access pattern; the comments below summarise findings
// from S7 v1.05(254) + netBook v1.05(450) boots.
//
// Register map as empirically observed (* = polled heavily; § = both
// roms; † = S7 only; ‡ = netBook only).  Reader / writer columns show
// the kernel PCs that touch each offset (callers vary by ROM).
//
//   off  size  class       R/W  observed values            inferred role
//   0x00 byte  R/W       § R+W  ←:{00,01,10,11,31,0d,1d,3d} control reg A:
//                              →:{00,01,10,11,31,0d,1d,3d}  BSP builds state
//                                                            bit-by-bit at boot
//   0x01 byte  R/W       § R+W  {00,01}                     control reg B
//   0x04 byte  WR-once   † W    {00}                        zero on init
//   0x05 byte  WR-once   † W    {00}                        zero on init
//   0x06 byte  POLLED *  § P    ←:{18}; →:{00,18}            status A (S7 polls
//                                                            1300+x reading 18)
//   0x07 byte  POLLED *  § P    ←:{00,01,02,03} (synth)      counter / status B
//                              →:{00}
//   0x08 byte  R/W       § R+W  ←:{02,03,37}                 keyboard column drive
//                              →:{00,02,08,0f,37,ff}
//   0x09 byte  R/W       § R+W  ←:{03}; →:{00,03,ff}          keyboard row read
//   0x0a byte  POLLED *  † P    ←:{02}; →:{00,02}            status C (S7 polls
//                                                            ~184x reading 02)
//   0x0b byte  POLLED *  † P    ←:{00,01,02,03}; →:{00}      counter / status D
//   0x0e byte  WR-once   † W    {08}                         single init write
//   0x0f byte  WR-once   † W    {00}                         single init write
//   0x12 byte  R+W       § R+W  ←:{cc,c0,c8}; →:{cc,c0,c4,c8,40,44,48}
//                                                            IRQ status (16-slot
//                                                            map at ROM 0x500873a8)
//   0x13 byte  R/W       § R+W  {00}                          IRQ status hi byte
//   0x16 byte  WR-once   § W    {13,00}                       init seq
//   0x17 byte  WR-once   § W    {00}                          init seq
//   0x18 byte  WR-once   § W    {02,08,00}                    init seq
//   0x19 byte  WR-once   § W    {00}                          init seq
//   0x26 byte  WR cmd    § W    {ef,00}                       UCB1200 ADC cmd
//                                                            lo byte
//   0x27 byte  WR cmd    § W    {12,00}                       UCB1200 ADC cmd
//                                                            hi byte
//   0x2a byte  WR        § W    {ff}                          touch X coord
//                                                            ADC-clear / reset
//   0x2b byte  WR        § W    {ff}                          touch X high byte
//   0x2c byte  WR        § W    {00,03,82}                    touch Y / channel
//                                                            select
//   0x2d byte  WR        § W    {00,03,23,60,63,e0}           touch Y high /
//                                                            channel flags
//   0x2e byte  R/W         R+W  ←:{01,03}; →:{01,03}          UCB channel state
//   0x2f byte  R/W         R+W  ←:{01,03}; →:{01,03}          UCB channel state
//   0x30 byte  R/W         R+W  {03}                          UCB channel state
//   0x31 byte  R/W         R+W  ←:{03,ff}; →:{03,ff,80}       UCB channel state
//   0x32 byte  R/W         R+W  ←:{00,02,60,62}; →:{60,62}    UCB channel state
//   0x33 byte  R/W         R+W  ←:{00,02,60,62,e2}; →:{00,60,62,02,e2}
//                                                            UCB channel state
//   0x3e byte  READ-ONLY † R    ←:{00,01,02,03} (synth)        live counter (?)
//   0x3f byte  READ-ONLY † R    ←:{00,01,02,03} (synth)        live counter (?)
//   0x40 byte  POLLED *  § P    ←:{00,02}; →:{08,04,01}        EEPROM busy / cmd
//                                                            status (already
//                                                            modelled via
//                                                            eepromBusyPolls_)
//   0x41 byte  POLLED *  § P    ←:{00,02}; →:{00}             EEPROM busy hi
//   0x44 byte  WR-once   † W    {00}                          single init write
//   0x45 byte  WR-once   † W    {00}                          single init write
//   0x48 byte  R/W       § R+W  EEPROM address-write / data-read window
//                                                            (already modelled)
//   0x49 byte  R/W       § R+W  EEPROM address-write / data-read window
//   0x4c byte  W cmd     § W    {01,04,06,07}                 EEPROM command
//                                                            port (already
//                                                            modelled)
//   0x4d byte  W cmd     § W    {00}
//
// To regenerate this map, run with:
//   PSION_S7_ASIC_CLASSIFY=1 \
//   PSION_S7_ASIC_CLASSIFY_FILE=/tmp/eiger.txt \
//     harness/run <rom> --device <id> --boot-seconds N
// and compare /tmp/eiger.txt with the table above.

const char *Eiger::nameRegister(uint32_t offset) {
    offset &= kRegMask;
    switch (offset) {
    case 0x00: return "CTRL_A";
    case 0x01: return "CTRL_B";
    case 0x04: return "INIT_04";
    case 0x05: return "INIT_05";
    case 0x06: return "STATUS_A";
    case 0x07: return "STATUS_B_CTR";
    case 0x08: return "KBD_COL_DRIVE";
    case 0x09: return "KBD_ROW_READ";
    case 0x0a: return "STATUS_C";
    case 0x0b: return "STATUS_D_CTR";
    case 0x0e: return "INIT_0E";
    case 0x0f: return "INIT_0F";
    case 0x12: return "IRQ_STATUS_LO";
    case 0x13: return "IRQ_STATUS_HI";
    case 0x16: return "INIT_16";
    case 0x17: return "INIT_17";
    case 0x18: return "INIT_18";
    case 0x19: return "INIT_19";
    case 0x26: return "UCB_ADC_CMD_LO";
    case 0x27: return "UCB_ADC_CMD_HI";
    case 0x2a: return "TOUCH_X_LO";
    case 0x2b: return "TOUCH_X_HI";
    case 0x2c: return "TOUCH_Y_LO";
    case 0x2d: return "TOUCH_Y_HI";
    case 0x2e: return "UCB_CH_2E";
    case 0x2f: return "UCB_CH_2F";
    case 0x30: return "UCB_CH_30";
    case 0x31: return "UCB_CH_31";
    case 0x32: return "UCB_CH_32";
    case 0x33: return "UCB_CH_33";
    case 0x3e: return "LIVE_CTR_3E";
    case 0x3f: return "LIVE_CTR_3F";
    case 0x40: return "EEPROM_BUSY_LO";
    case 0x41: return "EEPROM_BUSY_HI";
    case 0x44: return "INIT_44";
    case 0x45: return "INIT_45";
    case 0x48: return "EEPROM_DATA_LO";
    case 0x49: return "EEPROM_DATA_HI";
    case 0x4c: return "EEPROM_CMD";
    case 0x4d: return "EEPROM_CMD_HI";
    default:   return nullptr;
    }
}

int Eiger::expectedInitValue(uint32_t offset) {
    offset &= kRegMask;
    switch (offset) {
    case 0x04: return 0x00;
    case 0x05: return 0x00;
    case 0x0e: return 0x08;
    case 0x0f: return 0x00;
    case 0x16: return 0x13;  // first write (followed by 0x00)
    case 0x17: return 0x00;
    case 0x18: return 0x02;  // first write (followed by 0x08, 0x00)
    case 0x19: return 0x00;
    case 0x44: return 0x00;
    case 0x45: return 0x00;
    default:   return -1;
    }
}

const char *Eiger::classifyOffset(uint32_t offset) {
    offset &= kRegMask;
    switch (offset) {
    case 0x00: case 0x01:                      return "CTRL";
    case 0x04: case 0x05:
    case 0x0e: case 0x0f:
    case 0x16: case 0x17: case 0x18: case 0x19:
    case 0x44: case 0x45:                      return "WR-ONCE";
    case 0x06: case 0x0a:                      return "STATUS";
    case 0x07: case 0x0b:
    case 0x3e: case 0x3f:                      return "CTR";
    case 0x08: case 0x09:                      return "KBD";
    case 0x12: case 0x13:                      return "IRQ-W1C";
    case 0x26: case 0x27:                      return "UCB";
    case 0x2a: case 0x2b: case 0x2c: case 0x2d:return "TOUCH";
    case 0x2e: case 0x2f:
    case 0x30: case 0x31: case 0x32: case 0x33:return "UCB";
    case 0x40: case 0x41:
    case 0x48: case 0x49:
    case 0x4c: case 0x4d:                      return "EEPROM";
    default:                                   return nullptr;
    }
}

const char *Eiger::nameIrqBit(int bit) {
    // Slot map for the 16-slot dispatch table at ROM 0x500873a8.
    // Names are the canonical EKA1 IrqExt* enum, extracted from the
    // Series 7 v1.05(254) ROM debug-string table — slot order matches
    // bit position in the {ASIC[0x13], ASIC[0x12]} halfword.
    // Confirmed by existing code:
    //   slot 7  = IrqExtAtoD     (sa1100.cpp:3182 pen-sample heartbeat)
    //   slot 13 = IrqExtMedChgCf (sa1100.cpp:3234 CF media-change)
    //   slot 15 = IrqExtPenDown  (sa1100.cpp:10539 pen-down)
    static const char *names[16] = {
        "IrqExtTimer1",       "IrqExtTimer2",       // 0, 1
        "IrqExtRxFifo",       "IrqExtTxFifo",       // 2, 3
        "IrqExtE2PromService","IrqExtFifoOverrun",  // 4, 5
        "IrqExtE2PromFail",   "IrqExtAtoD",         // 6, 7
        "IrqExtBit0PortB",    "IrqExtPcCardIreq",   // 8, 9
        "IrqExtCfCardIreq",   "IrqExtExpansion0",   // 10, 11
        "IrqExtExpansion1",   "IrqExtMedChgCf",     // 12, 13
        "IrqExtMedChgPc",     "IrqExtPenDown",      // 14, 15
    };
    if (bit < 0 || bit >= 16) return nullptr;
    return names[bit];
}

void Eiger::dumpRegisterMap(ARM710 &owner) const {
    // Walk the classifier's per-offset stats; print one line per
    // touched offset showing current stored value + name + documented
    // classification.  Pairs the live state with the access map so
    // the user can see at a glance which registers are well-modelled
    // (matching their static classification) vs surprising us.
    owner.log("=== Eiger register snapshot ===");
    const uint8_t *s = store_();
    for (uint32_t off = 0; off < kRegSpaceSize; off++) {
        const auto &st = EigerClassify::stats[off];
        if (st.readCount == 0 && st.writeCount == 0) continue;
        const char *n = nameRegister(off);
        const char *c = classifyOffset(off);
        owner.log("  ASIC[0x%04x %-16s %-8s] val=0x%02x R=%u W=%u pollRunMax=%u",
                  off, n ? n : "?", c ? c : "-", s[off],
                  st.readCount, st.writeCount, st.pollRunMax);
    }
    bool anyIrq = false;
    for (int b = 0; b < 16; b++) if (irqCauseCount[b]) { anyIrq = true; break; }
    if (anyIrq) {
        owner.log("--- Eiger IRQ slot ack counts ---");
        for (int b = 0; b < 16; b++) {
            if (!irqCauseCount[b]) continue;
            const char *n = nameIrqBit(b);
            owner.log("  IRQ[%2d %-14s] acks=%u",
                      b, n ? n : "?", irqCauseCount[b]);
        }
    }
    owner.log("=== end Eiger register snapshot ===");
}

// The Eiger class is now a diagnostic-only holder.  The historic
// readReg8 / writeReg8 / initDefaults / EEPROM-protocol methods were
// never wired in (sa1100.cpp readAsic / writeAsic implement the same
// logic inline).  Removed to drop dead code; the static name /
// classify / expected-init helpers and the per-bit irqCauseCount
// array are still used.
Eiger::Eiger(ARM710 *owner, uint8_t *backing)
    : owner_(owner), backing_(backing) {
    (void)owner_;
    if (!backing_) std::memset(regs_, 0, sizeof(regs_));
}
