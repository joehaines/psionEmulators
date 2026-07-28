// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#include "etna.h"
#include "arm710.h"
#include <stdio.h>
#include <string.h>

enum EtnaReg {
    regUnk0 = 0,
    regUnk1 = 1,
    regUartIntStatus = 2,
    regUartIntMask = 3,
    regUartBaudRateLo8 = 4,
    regUartBaudRateHi4 = 5,
    regPcCdIntStatus = 6,
    regPcCdIntMask = 7,
    regIntClear = 8,
    regSktVarA0 = 9,
    regSktVarA1 = 0xA,
    regSktCtrl = 0xB,
    regWake1 = 0xC,
    regSktVarB0 = 0xD,
    regSktVarB1 = 0xE,
    regWake2 = 0xF
};

static const char *nameReg(uint32_t reg) {
    switch (reg) {
    case regUnk0: return "unk0";
    case regUnk1: return "unk1";
    case regUartIntStatus: return "UartIntStatus";
    case regUartIntMask: return "UartIntMask";
    case regUartBaudRateLo8: return "UartBaudRateLo8";
    case regUartBaudRateHi4: return "UartBaudRateHi4";
    case regPcCdIntStatus: return "PcCdIntStatus";
    case regPcCdIntMask: return "PcCdIntMask";
    case regIntClear: return "IntClear";
    case regSktVarA0: return "SktVarA0";
    case regSktVarA1: return "SktVarA1";
    case regSktCtrl: return "SktCtrl";
    case regWake1: return "wake1";
    case regSktVarB0: return "SktVarB0";
    case regSktVarB1: return "SktVarB1";
    case regWake2: return "wake2";
    }
    return nullptr;
}


Etna::Etna(ARM710 *owner) {
    this->owner = owner;

    for (int i = 0; i < 0x80; i++)
        prom[i] = 0;

	// defaults expected by the touchscreen code
	prom[0xA] = 20;
	prom[0xB] = 20;
	prom[0xC] = 20;
	prom[0xD] = 30;

    // some basic stuff to begin with
    // set up the Psion's unique ID (also folds the checksum)
    setMachineId(kDefaultMachineId);

    // give ourselves a neat custom device name
    setDeviceName("PockEmul");
}

void Etna::setDeviceName(const char *name) {
    const char *key = "PSIONPSIONPSION";
    prom[0x28] = strlen(name);
    if (prom[0x28] > 15)
        prom[0x28] = 15;
    for (int i = 0; i < prom[0x28]; i++)
        prom[0x29 + i] = name[i] ^ key[i];
    // Recalculate the checksum after changing the name bytes.
    recalcChecksum();
}

uint32_t Etna::getMachineId() const {
    return  (uint32_t)prom[kPromMachineId]
         | ((uint32_t)prom[kPromMachineId + 1] << 8)
         | ((uint32_t)prom[kPromMachineId + 2] << 16)
         | ((uint32_t)prom[kPromMachineId + 3] << 24);
}

void Etna::setMachineId(uint32_t id) {
    prom[kPromMachineId]     = (uint8_t)(id & 0xFF);
    prom[kPromMachineId + 1] = (uint8_t)((id >> 8) & 0xFF);
    prom[kPromMachineId + 2] = (uint8_t)((id >> 16) & 0xFF);
    prom[kPromMachineId + 3] = (uint8_t)((id >> 24) & 0xFF);
    // The ID bytes are covered by the PROM checksum, so a write that
    // didn't re-fold it would leave the image looking corrupt to the
    // kernel's validity check.
    recalcChecksum();
}

void Etna::recalcChecksum() {
    uint8_t chk = 0;
    for (int i = 0; i < 0x7F; i++)
        chk ^= prom[i];
    prom[0x7F] = chk ^ 66;
}


uint32_t Etna::readReg8(uint32_t reg)
{
    uint32_t result = 0xFF;
    switch (reg) {
    case regPcCdIntStatus: result = pcCdIntStatus; break;
    case regPcCdIntMask: result = pcCdIntMask; break;
    case regIntClear: result = 0; break;
    // SktVarA0 encodes PCMCIA socket status bits read in parallel by four
    // separate EPOC wrappers, each AND-ing a different field:
    //   bits 0-1: CD1#/CD2# - card detect (active low; clear = card present)
    //   bits 2-3: VS1#/VS2# - voltage sense
    //   bits 4-5: BVD1/BVD2 - battery voltage (1 = battery OK; required)
    //   bit 6:    WP        - write protect (0 = writable)
    //   bit 7:    RDY       - ready (1 = card ready)
    // The caller maps VS bits -> voltage code: 0->7, 4->4, 8->3, 12->1.
    // CF cards are 3.3V keyed, so VS1#/VS2# = 0/1 (bit 3 set) → code 3.
    // Report RDY as soon as the card is present — real CF cards assert it
    // within microseconds of power, so gating on sktCtrl tends to deadlock
    // EPOC's poll loop when the driver hasn't written SktCtrl yet.
    //   0xB8 = 1011 1000: CD1#=0 CD2#=0 VS1#=0 VS2#=1 BVD1=1 BVD2=1 WP=0 RDY=1
    //   0x38 = same with RDY=0 (while we're still simulating card self-test)
    case regSktVarA0:
        if (!cardPresent) {
            result = 0x01;
        } else if (rdyPollsUntilReady > 0) {
            rdyPollsUntilReady--;
            result = 0x38;       // RDY low: card busy
        } else {
            result = 0xB8;       // RDY high: card ready
        }
        break;
    // SktVarA1 bit 0 = card-detect, bit 1 = ready. Gated on the same RDY
    // state as SktVarA0 so both polling paths see the same transition.
    case regSktVarA1:
        if (!cardPresent) result = 0;
        else if (rdyPollsUntilReady > 0) result = 0x01;  // present but not ready
        else result = 0x03;
        break;
    case regSktCtrl: result = sktCtrl; break;
    case regWake1: result = wake1; break;
    case regWake2: result = wake2; break;
    // PCCARD-ARM::Bind() writes 0x0F to SktVarB0 and reads back; if the
    // readback doesn't match, Bind fails and the whole PCMCIA stack stays
    // disabled. Back these with plain storage.
    case regSktVarB0: result = sktVarB0; break;
    case regSktVarB1: result = sktVarB1; break;
    }
    // Log a few reads of the socket-status / int-status registers so we can
    // tell whether EPOC ever polls them, and what caller is doing so.
    if (socketReadLogs < 256 &&
        (reg == regSktVarA0 || reg == regSktVarA1 ||
         reg == regPcCdIntStatus || reg == regPcCdIntMask ||
         reg == regSktVarB0 || reg == regSktVarB1 || reg == regSktCtrl)) {
        owner->log("ETNA readReg8: reg=%s -> %02x pc=%08x lr=%08x",
                   nameReg(reg), result,
                   owner->getGPR(15) - 4, owner->getGPR(14));
        socketReadLogs++;
    }
    return result;
}

uint32_t Etna::readReg32(uint32_t reg)
{
    // ETNA is byte-addressable. The 5mx Pro bootloader uses 32-bit
    // load/store to access four consecutive registers in a single
    // op (e.g. regWake1/SktVarB0/SktVarB1/Wake2 at 0xC..0xF). Synthesise
    // the 32-bit value from four byte reads so each register's actual
    // state is visible — returning 0xFFFFFFFF blanket made the bootloader
    // stuck in a wait loop, never deciding the CF was settled.
    uint32_t r0 = readReg8(reg)     & 0xFF;
    uint32_t r1 = readReg8(reg + 1) & 0xFF;
    uint32_t r2 = readReg8(reg + 2) & 0xFF;
    uint32_t r3 = readReg8(reg + 3) & 0xFF;
    return r0 | (r1 << 8) | (r2 << 16) | (r3 << 24);
}

void Etna::writeReg8(uint32_t reg, uint8_t value)
{
    if (!promReadActive)
		owner->log("ETNA writeReg8: reg=%s value=%02x @ pc=%08x,lr=%08x", nameReg(reg), value, owner->getGPR(15) - 4, owner->getGPR(14));
    uint8_t priorStatus = pcCdIntStatus;
    switch (reg) {
    // Write-1-to-clear: bits written as 1 clear the corresponding latched
    // status bit. Without this the insert-event bit would stay set forever.
    case regPcCdIntStatus: pcCdIntStatus &= ~value; break;
    case regPcCdIntMask: pcCdIntMask = value; break;
    case regSktCtrl: sktCtrl = value; break;
    // The Psion 5mx PCMCIA ISR acks the media-change FIQ by writing to
    // ETNA's IntClear register but never touches PcCdIntStatus. On real
    // hardware IntClear is the one atomic ack; on our side we mirror the
    // same bits through to PcCdIntStatus so mediaChangePending() drops as
    // soon as the ISR returns. Without this mirror MCINT re-asserts on the
    // very next instruction and the handler loops forever (observed as the
    // infinite SktCtrl=0x41 / IntClear=0x01 write pattern when a CF is
    // attached).
    case regIntClear:
        pendingInterrupts &= ~value;
        pcCdIntStatus &= ~value;
        if (value & 0x01) intClearBit0Count++;
        break;
    case regWake1: wake1 = value; break;
    case regWake2: wake2 = value; break;
    case regSktVarB0: sktVarB0 = value; break;
    case regSktVarB1: sktVarB1 = value; break;
    }
    // Surface PcCdIntStatus transitions so we can see whether the card-insert
    // latch is being cleared-and-reset (suggesting something is re-asserting
    // it) vs. simply cleared once and staying clear. pcCdIntStatusLogs keeps
    // a lid on log volume if the ISR thrashes the register.
    if (priorStatus != pcCdIntStatus && pcCdIntStatusLogs < 20) {
        owner->log("ETNA PcCdIntStatus: %02x -> %02x (via %s write %02x)",
                   priorStatus, pcCdIntStatus, nameReg(reg), value);
        pcCdIntStatusLogs++;
    }
}

void Etna::writeReg32(uint32_t reg, uint32_t value)
{
    // Mirror readReg32: dispatch a 32-bit write across four byte writes so
    // each ETNA register sees the right byte. Without this, 32-bit writes
    // to ETNA from the 5mx Pro bootloader are silently dropped.
    writeReg8(reg,      value        & 0xFF);
    writeReg8(reg + 1, (value >>  8) & 0xFF);
    writeReg8(reg + 2, (value >> 16) & 0xFF);
    writeReg8(reg + 3, (value >> 24) & 0xFF);
}

void Etna::setCardPresent(bool present)
{
    if (cardPresent == present) return;
    cardPresent = present;
    socketReadLogs = 0;
    pcCdIntStatusLogs = 0;
    // Report RDY=1 from the first SktVarA0 poll. Real CF cards pulse RDY
    // low during power-up self-test, so we used to hold RDY=0 for the
    // first N polls to mimic that edge. Empirically PCCARD-ARM accepts an
    // instant RDY=1 just fine (the state machine doesn't actually require
    // observing a 0→1 transition), and every poll we made it wait cost
    // ~31 ms of sim time (the driver's DFC tick while RDY=0). Previously
    // this was 16 polls = 500 ms per mount; dropping to 0 saves that
    // whole window.
    rdyPollsUntilReady = 0;
    // Do NOT latch a card-insert event into pcCdIntStatus bit 0. On Psion
    // 5mx, card detection happens by polling SktVarA0/A1 from the PCCARD-ARM
    // driver, NOT via FIQ. ETNA's PcCdIntStatus bit 0 is the CF IREQ# line
    // (see setCfIreq below): EPOC's ATA driver sets PcCdIntMask=0x01 around
    // each sector transfer so the CF card's "data ready" IREQ# edge wakes
    // the DFC thread. If we latched a card-insert event here, the first
    // time EPOC enabled the mask for ATA it would see bit 0 set and treat
    // the FIQ as a removal event — powering the socket down mid-mount.
    owner->log("ETNA setCardPresent(%d): pcCdIntStatus=%02x pcCdIntMask=%02x",
               (int)present, pcCdIntStatus, pcCdIntMask);
}

void Etna::setCfIreq(bool asserted)
{
    // Route the card's live IREQ# line into PcCdIntStatus bit 0. CF semantics:
    // IREQ# is a level-triggered output from the card. Real ETNA latches
    // the *falling edge* (i.e. the moment the card starts requesting
    // attention) into PcCdIntStatus bit 0, and software clears the latch
    // via IntClear / write-1-to-clear. We mirror that edge-latching here;
    // a pure level-mirror would let the interrupt fire continuously until
    // software reads CF Status, making IntClear useless because the
    // *next* emulator cycle would re-assert the bit.
    bool prevWire = cfIreqWirePrev;
    cfIreqWirePrev = asserted;
    if (!asserted || prevWire) return;    // only rising edges latch

    uint8_t prior = pcCdIntStatus;
    pcCdIntStatus |= 0x01;
    if (prior != pcCdIntStatus && pcCdIntStatusLogs < 20) {
        owner->log("ETNA CF IREQ# edge-latched: pcCdIntStatus=%02x pcCdIntMask=%02x",
                   pcCdIntStatus, pcCdIntMask);
        pcCdIntStatusLogs++;
    }
}

void Etna::setPromBit0High()
{
    // begin reading a word
    promReadAddress = 0;
    promReadValue = 0;
    promAddressBitsReceived = 0;
    promReadActive = true;
}

void Etna::setPromBit0Low()
{
    promReadActive = false;
}

void Etna::setPromBit1High()
{
    if (promAddressBitsReceived < kPromFrameBits) {
        // Still receiving the command frame. Clocks before the start bit
        // are idle padding — how many there are differs per ROM, so we
        // wait for the 1 rather than counting (see kPromFrameBits).
        const int bit = (wake1 & 4) >> 2;
        if (promAddressBitsReceived == 0 && bit == 0)
            return;
        promReadAddress <<= 1;
        promReadAddress |= bit;
        if (++promAddressBitsReceived == kPromFrameBits) {
            // we can fetch the value now
            int addressInBytes = (promReadAddress & kPromWordIndexMask) * 2;
            addressInBytes %= sizeof(prom);
            promReadValue = prom[addressInBytes] | (prom[addressInBytes + 1] << 8);
        }
    } else {
        wake1 &= ~8;
        if (promReadValue & 0x8000)
            wake1 |= 8;
        promReadValue <<= 1;
    }
}
