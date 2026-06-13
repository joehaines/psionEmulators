// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include <stdint.h>

class ARM710;

class Etna {
    uint8_t prom[0x80] = {};
    uint16_t promReadAddress = 0, promReadValue = 0;
    bool promReadActive = false;
    int promAddressBitsReceived = 0;

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

	ARM710 *owner;

public:
	Etna(ARM710 *owner);

    uint32_t readReg8(uint32_t reg);
    uint32_t readReg32(uint32_t reg);
    void writeReg8(uint32_t reg, uint8_t value);
    void writeReg32(uint32_t reg, uint32_t value);

    // PROM
    void setPromBit0High(); // port B, bit 0
    void setPromBit0Low(); // port B, bit 0
    void setPromBit1High(); // port B, bit 1
    void setDeviceName(const char *name);

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
