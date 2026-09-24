// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include <stdint.h>

class ARM710;

class Etna {
    uint8_t prom[0x80] = {};
    // Bytes covered by the XOR check, the last of them being the checksum:
    // 0x80 on the Windermere machines, 0x20 on the Series 5.
    int promChecksumLength = 0x80;
    uint16_t promReadAddress = 0, promReadValue = 0;
    bool promReadActive = false;
    int promAddressBitsReceived = 0;
    // Data bits clocked out of the current word (0..16); see the
    // sequential-read note in setPromBit1High.
    int promDataBitsSent = 0;
    void loadPromWord();

    uint8_t pendingInterrupts = 0, interruptMask = 0;
    uint8_t wake1 = 0, wake2 = 0;
    bool cardPresent = false;

    // Latched PC-Card interrupt status. Bit 0 = card-insert/remove event.
    // Set by setCardPresent() on transitions, cleared by EPOC writing
    // with the same bit set (write-1-to-clear).
    uint8_t pcCdIntStatus = 0;
    uint8_t pcCdIntMask = 0;
    uint8_t sktCtrl = 0;
    // Previous cfIreq-wire level, for edge detection. pcCdIntStatus bit 0
    // is latched on the rising edge of the CF's IREQ# input; it stays set
    // until software writes 1 to clear (via IntClear or write-1-to-clear
    // on PcCdIntStatus). Without edge-latching, the handler's write to
    // IntClear would be undone on the very next emulator cycle because
    // the CF's IREQ# is still held low until the kernel reads Status.
    bool cfIreqWirePrev = false;

    // SktVarB0 / SktVarB1 are writable socket-state registers used by the
    // PCCARD-ARM driver. On boot the driver probes SktVarB0 by writing 0x0F
    // and reading it back to verify ETNA is responsive; without this,
    // PCCARD-ARM::Bind() returns KErrGeneral and the PCMCIA stack never
    // comes up. Implement them as plain readable/writable bytes.
    uint8_t sktVarB0 = 0;
    uint8_t sktVarB1 = 0;

    // Rate limit for diagnostic logging of socket-status reads.
    int socketReadLogs = 0;
    int pcCdIntStatusLogs = 0;

    // PCCARD-ARM driver polls SktVarA0 bit 7 (RDY) after writing SktCtrl's
    // VPP/reset-release bits (0x41 then 0x61). Real CF hardware pulses RDY
    // low during the self-test that follows power-up, then releases it once
    // the controller is ready. We model that by reporting RDY=0 for the
    // first N SktVarA0 polls after card insertion, then RDY=1 thereafter.
    // Without this edge the driver's power-up state machine never advances
    // to the CIS-parsing phase.
    int rdyPollsUntilReady = 0;

    // Re-folds the PROM's trailing XOR checksum byte. Every write into
    // the image has to end with this or the guest reads it as corrupt.
    void recalcChecksum();

	ARM710 *owner;

public:
	Etna(ARM710 *owner);

    uint32_t readReg8(uint32_t reg);
    uint32_t readReg32(uint32_t reg);
    void writeReg8(uint32_t reg, uint8_t value);
    void writeReg32(uint32_t reg, uint32_t value);

    // PROM serial protocol (93Cxx-style, bit-banged over Windermere's
    // port B by ECust). Per word the guest clocks in a command frame —
    // start bit 1, the 2-bit READ opcode (10), then a 6-bit word index —
    // and clocks out 16 data bits, MSB first.
    //
    // Like the real chip, we ignore clocks until the start bit arrives
    // rather than counting a fixed-width address, because the ROMs
    // disagree on the leading padding: the 5mx v1.05(260) clocks the
    // frame in 9 pulses (25 per word) while the MC218 v1.05(259) sends a
    // leading zero first, 10 pulses (26 per word). Traced by logging
    // port-B edges against the ETNA register writes that carry each bit.
    //
    // Getting this wrong is silent and total: a decoder fixed at 10 bits
    // eats the 5mx's first DATA clock, so every word it hands back is
    // shifted, the kernel's XOR-to-0x42 check over the image fails, and
    // EPOC discards the whole PROM. That is what made Machine
    // information report "Unique id 0000-0000-0000-0000" and ignore the
    // PROM's device-name field. One fixed at 9 breaks the MC218 the same
    // way. Start-bit detection serves both, and leaves the Revo — whose
    // ROM drives none of these lines — reading nothing at all, as it
    // should.
    static constexpr int kPromFrameBits = 9;   // start + opcode + index
    static constexpr uint16_t kPromWordIndexMask = 0x3F;   // 64 words
    void setPromBit0High(); // port B, bit 0
    void setPromBit0Low(); // port B, bit 0
    void setPromBit1High(); // port B, bit 1
    void setDeviceName(const char *name);

    // ── Unique machine ID ─────────────────────────────────────────────
    // Real Psions carry a factory-programmed 32-bit ID in the ETNA
    // identity PROM, stored little-endian at offset 0x18 (the guest
    // reads the PROM as 16-bit words through the bit-banged interface
    // above). The kernel copies it into the Superpage, and it surfaces
    // in the System screen's Machine information dialog as the LOW half
    // of "Unique id" — the high half is the model's machine UID, which
    // comes from the ROM, not from here (a 5mx with this word set to
    // 89ABCDEF displays "1000-118A-89AB-CDEF").
    //
    // EPOC software that locks itself to one machine keys off it, which
    // is why the frontend's debug UI can reprogram it live (see
    // EmuBase::setMachineId). The value the guest sees changes
    // immediately; a running EPOC cached it at boot, so the machine has
    // to be reset for the OS to report the new one.
    static constexpr uint8_t kPromMachineId = 0x18;
    static constexpr uint32_t kDefaultMachineId = 0x12345678;
    uint32_t getMachineId() const;
    void setMachineId(uint32_t id);

    // ── Language and keyboard index ───────────────────────────────────
    // The factory also programmed which locale DLL (ELocl<n>) and keyboard
    // table (Ekdata<n>) the machine boots with. Both are 3-bit fields in
    // the first 32-bit word of the PROM: language in bits 18..20 and
    // keyboard in bits 21..23, i.e. bits 2..4 and 5..7 of byte 2. The
    // variant's LanguageIndex() / KeyboardIndex() (5mx ROM 0x500875D8 /
    // 0x500875F0) return exactly those after the XOR check, EKern files
    // them in TMachineInfoV1 +0xE0 / +0xE4, and the window server loads
    // ELOCL<n> / EKDATA<n> for n in 1..7. Read once at boot, so a change
    // lands on the next reset. See docs/rom-audit.md.
    static constexpr uint8_t kPromLocaleByte = 2;
    void setLocaleIndices(int language, int keyboard);

    // The Series 5's settings PROM is the same part on the same pins, but
    // its variant (VArmP2.dll 0x5007E45C) reads only 16 words and checks
    // the XOR of those 32 bytes against 0x42, so resetImage(0x20) gives it
    // a blank image of that size. Its PROM was never wired before, so it
    // always read zeros; an all-zero image changes nothing except that it
    // now passes the check, and only the fields set afterwards
    // (setLocaleIndices) differ from what the machine has always seen.
    void resetImage(int checksumLength);

    // Card slot presence — flipped by the emulator when a virtual CF card
    // is attached or detached. Does not latch an insert event; card
    // detection on 5mx goes via SktVarA0/A1 polling.
    void setCardPresent(bool present);
    bool isCardPresent() const { return cardPresent; }

    // True while the PC-card socket supply is enabled. EPOC R1's PSU
    // power-up writes SktCtrl bit 0 to switch Vcc onto the socket before it
    // checks the socket voltage; the Series 5 senses that supply on an ADC
    // channel (see Series5::socketVccSenseAdc). Bit 0 stays set for as long
    // as the socket is powered (it is also set in the 0x41 / 0x61 I/O-enable
    // writes), so it is a faithful "Vcc is live" signal.
    bool socketPowered() const { return (sktCtrl & 0x01) != 0; }

    // Drive PcCdIntStatus bit 0 from the CF card's live IREQ# line. When
    // EPOC's ATA driver has enabled the mask (PcCdIntMask=0x01 around each
    // sector transfer), this flips MCINT so the DFC thread wakes on the
    // card's "data ready" edge instead of waiting out the 2s PCCARD-ARM
    // polling interval.
    void setCfIreq(bool asserted);

    // True when ETNA has a PC-Card interrupt cause pending AND EPOC has
    // unmasked it. Currently bit 0 carries CF IREQ#; card detection is not
    // latched here (it goes via SktVarA0/A1 polling). Gating on
    // pcCdIntMask matters because EPOC's boot leaves the mask zeroed until
    // the ATA driver actively enables it — firing MCINT before then sends
    // the kernel down a "spurious FIQ" path that defensively powers the
    // socket off.
    bool mediaChangePending() const {
        return (pcCdIntStatus & pcCdIntMask) != 0;
    }

    // Raw-latched CF IRQ level (just the pcCdIntStatus bit 0, ignoring
    // mask). Equivalent to the ETNA INT output's "raw" side before the
    // mask gate. On real 5mx hardware the Windermere INTRSR latches the
    // SoC-pin level, so bit 7 (EINT3) should track this raw latch —
    // masking via PcCdIntMask only affects the ETNA->Windermere delivery
    // edge, not the latched state once raised. Use this for driving
    // pendingInterrupts bit 7: an ISR that reads INTRSR inside
    // CardIntCallBack expects the bit to still be asserted even after
    // the driver temporarily writes PcCdIntMask=0 as part of its own
    // interrupt-handling sequence.
    bool cfIrqRawLatched() const {
        return (pcCdIntStatus & 1) != 0;
    }

    // Called from Windermere's MCEOI path. Treats the FIQ ack as an
    // implicit clear of the PC-Card status latch so the interrupt edges
    // rather than re-firing every tick when EPOC's handler leaves the
    // bits uncleared.
    void ackMediaChange() { pcCdIntStatus = 0; }

    // Counter bumped every time the driver writes ETNA's IntClear register
    // with bit 0 set (the CF-IRQ ack path). Consumed by Windermere's CF
    // re-enable strategy sweep to trigger mask-bit-7 rearm deterministically
    // on the ack.
    uint32_t intClearBit0Count = 0;
};
