// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstdint>
#include <cstddef>

class ARM710;

// Psion "Eiger" companion ASIC for the Series 7 / netBook (SA-1100).
// Equivalent to ETNA on the 5mx but proprietary to Psion with no
// public register map.  This class mirrors ETNA's structure: a small
// EEPROM (read via the kernel's parallel cmd/data port at 0x40-0x4F),
// per-register state for the well-known offsets (IRQ status at
// 0x12-0x13, touch coords at 0x2a-0x2d), plus a 4 KB scratch backing
// store for the long tail of registers we don't yet model.
//
// We start with the same approach ETNA uses: provide sensible
// non-zero defaults for chip-ID / version registers so the kernel's
// platform-identification probes find a recognisable Eiger.
// Subsequent iterations refine specific registers as we observe
// which ones gate which kernel paths.
//
// Wired into sa1100.cpp via readReg / writeReg dispatched from the
// existing readAsic / writeAsic entry points; we keep the same scratch
// behaviour for unknown offsets so the existing model's accumulated
// fixes (touch coords, kbd matrix, EEPROM port, IRQ status auto-clear)
// don't regress.
class Eiger {
public:
    static constexpr uint32_t kRegSpaceSize = 0x1000;   // 4 KB byte-addressable
    static constexpr uint32_t kRegMask      = kRegSpaceSize - 1;
    static constexpr size_t   kEepromSize   = 0x80;     // 128 bytes

    // (T9) Eiger can either own its own scratch (legacy / unit-test
    // mode) or share a caller-provided backing store so the SA-1100
    // emulator's asicRegs[] and this class's view of registers stay
    // coherent without explicit sync calls.  When backing is non-null,
    // all reads/writes are routed to *backing instead of regs_.
    explicit Eiger(ARM710 *owner, uint8_t *backing = nullptr);

    // NOTE: the historic register-access API (readReg8 / writeReg8 /
    // initDefaults + EEPROM cmd protocol) has been removed — sa1100.cpp's
    // readAsic / writeAsic implement that logic inline.  This class
    // now exists as a diagnostic-only helper: static name / classify /
    // expected-init lookups, the per-bit irqCauseCount array, and
    // dumpRegisterMap().

    // Symbolic register name for trace output.  Returns nullptr for
    // offsets that haven't been identified yet, so callers can fall
    // back to the raw offset.  Mirrors ETNA's nameReg() helper
    // (core/etna.cpp:25-44).
    static const char *nameRegister(uint32_t offset);

    // Symbolic name for a slot in the Eiger 16-slot IRQ map at
    // register 0x12 (low byte) / 0x13 (high byte).  Slot numbers and
    // names come from the ROM dispatch table at 0x500873a8 — the BSP
    // builds a function pointer table indexed by the bit position of
    // a set bit in {0x13, 0x12}.  Returns nullptr for slots that
    // haven't been identified yet.
    static const char *nameIrqBit(int bit);

    // Documented BSP-expected first-write value for WR-ONCE init
    // registers, per the classifier observations in eiger.cpp:21-82.
    // Returns -1 for offsets with no documented expectation (most
    // offsets).  Used by the rolling dump and by an optional
    // PSION_S7_TRACE_INIT_WARN check that flags writes diverging from
    // the documented BSP init pattern — a leading indicator that
    // something earlier corrupted the register state (e.g. our
    // synthetic-tick leak that the existing isS7Ctrl opt-out fixed).
    static int expectedInitValue(uint32_t offset);

    // Static documented classification per offset.  Returns one of:
    //   "POLLED"   — kernel busy-waits reading this offset
    //   "WR-ONCE"  — written exactly once during boot
    //   "R/W"      — read-modify-write storage
    //   "IRQ-W1C"  — write-1-to-clear IRQ status latch
    //   "EEPROM"   — part of the EEPROM cmd/data window
    //   "CTRL"     — control register (BSP builds bit-by-bit)
    //   "STATUS"   — hardware status register
    //   "TOUCH"    — touch ADC coordinate channel
    //   "UCB"      — UCB1200 codec channel state
    //   "CTR"      — free-running counter
    //   "KBD"      — keyboard matrix row/column
    //   nullptr    — unclassified
    static const char *classifyOffset(uint32_t offset);

    // Per-bit IRQ delivery counters for the 16 Eiger IRQ slots.
    // Incremented in sa1100.cpp writeAsic each time a kernel W1C ack
    // clears a pending bit.  Mirrors windermere.h:132's irqCauseCount[].
    uint32_t irqCauseCount[16] = {};

    // Emit a per-offset register-map line for every offset that has
    // been touched.  Format mirrors the comment table in eiger.cpp.
    // Called from the SA-1100 rolling dump and from
    // EigerClassify::dumpSummary.
    void     dumpRegisterMap(class ARM710 &owner) const;

private:
    uint8_t       *store_()       { return backing_ ? backing_ : regs_; }
    const uint8_t *store_() const { return backing_ ? backing_ : regs_; }

    ARM710  *owner_;
    uint8_t *backing_ = nullptr;  // non-null = use external scratch (T9)
    uint8_t  regs_[kRegSpaceSize] = {};

public:
    // Empirical poll-clear count — the number of reads the BSP issues
    // before observing the "operation done" bit clear.  Used by the
    // inline poll-clear path in sa1100.cpp readAsic/writeAsic for
    // offsets 0x06 / 0x0a.  Picked by matching the BSP's longest
    // observed run of repeated reads against the same offset and
    // stopping just before the second-shortest path.  Not derived
    // from datasheet timings (we don't have one).
    static constexpr int kEiger06OpDoneAfterPolls = 6;
    static constexpr int kEiger0aOpDoneAfterPolls = 6;
};
