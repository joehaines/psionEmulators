// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_honda)
//                  + adaptation by the Psion emulator project, 2026.

#include "psion_honda.h"
#include "psion_ssd.h"

void PsionHonda::writeTxd(uint8_t /*b*/) {
    // SSD carrier ignores the serial half; modem/comm-cable carriers
    // (future work) would forward to their own state machine here.
}
void PsionHonda::writeRts(bool /*s*/) { /* see writeTxd */ }
void PsionHonda::writeDtr(bool /*s*/) { /* see writeTxd */ }

uint8_t PsionHonda::readSibFrame() {
    return m_ssd ? m_ssd->readFrame() : 0;
}

void PsionHonda::writeSibFrame(uint16_t frame) {
    if (m_ssd) m_ssd->writeFrame(frame);
}

void PsionHonda::attachSsdCarrier(PsionSSD *ssd) {
    m_ssd = ssd;
    // The SSD itself fires its door callback on attach (via
    // PsionSSD::attach); the host wires that into ASIC9::medchng.
    // We don't double-pulse here.
}

void PsionHonda::detachCarrier() {
    m_ssd = nullptr;
    // Same: PsionSSD::detach fires the door callback already.
}
