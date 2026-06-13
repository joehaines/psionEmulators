// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_honda)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Honda expansion slot. The Siena exposes a single Honda
// connector that carries BOTH a Condor serial UART (RS-232 modem /
// comm cable / external drive) AND an ASIC9 SIBO parallel data
// channel (channel 4) for SSD packs. A given carrier device plugged
// into the Honda slot uses one half or the other:
//
//   - SSD pack carrier: uses the parallel SIBO-frame path. Modem
//     status lines stay in their idle "no carrier" state. Asserts
//     medchng on attach / detach so EPOC16 re-scans the slot.
//   - Modem / comm-cable carrier (not implemented here): uses the
//     serial UART path through Condor; ASIC9 channel 4 stays idle.
//
// This class is a thin slot abstraction matching MAME's wiring in
// reference/mame-psion/psion/siena.cpp:299-312:
//
//   Condor TXD/RTS/DTR     -> Honda::write{Txd,Rts,Dtr}
//   Honda::write{Rxd,...}  -> Condor write_{rxd,cts,dsr,dcd}
//   ASIC9.data_r<4> /  _w  -> Honda::read{Sib,...}, Honda::writeSib
//   Honda  medchng         -> ASIC9.medchng_w
//
// The class owns no state of its own — it's pure routing.

#pragma once

#include <cstdint>
#include <functional>

class PsionSSD;

class PsionHonda {
public:
    PsionHonda() = default;

    // ------------------------------------------------------------------
    // Serial half (Condor side)
    // ------------------------------------------------------------------

    // Inputs from Condor: TXD byte, RTS/DTR edge. Forwarded to the
    // currently-attached carrier (none for an SSD carrier, since
    // SSD doesn't use the serial path).
    void writeTxd(uint8_t b);
    void writeRts(bool s);
    void writeDtr(bool s);

    // Modem-status outputs to Condor. The slot pushes carrier-state
    // edges into these so Condor's MSR reads reflect the carrier.
    void setRxdInCb(std::function<void(uint8_t)> cb) { m_rxdIn = std::move(cb); }
    void setCtsInCb(std::function<void(bool)> cb)    { m_ctsIn = std::move(cb); }
    void setDsrInCb(std::function<void(bool)> cb)    { m_dsrIn = std::move(cb); }
    void setDcdInCb(std::function<void(bool)> cb)    { m_dcdIn = std::move(cb); }

    // ------------------------------------------------------------------
    // Parallel half (ASIC9 SIBO channel 4 side)
    // ------------------------------------------------------------------

    // Called by ASIC9's channel-4 reader/writer. With no carrier
    // attached these are no-ops; with an SSD carrier they route to
    // PsionSSD::readFrame / writeFrame.
    uint8_t readSibFrame();
    void    writeSibFrame(uint16_t frame);

    // Medchng output to ASIC9 — fires when a carrier is attached or
    // detached, so the kernel re-scans the slot.
    void setMedChngOutCb(std::function<void(bool)> cb) { m_medChngOut = std::move(cb); }

    // ------------------------------------------------------------------
    // Carriers
    // ------------------------------------------------------------------

    // Attach an SSD pack as the carrier. The slot routes parallel
    // SIBO frames to the SSD and pulses medchng. The Condor serial
    // half stays "open" (no RXD, modem lines stay deasserted).
    void attachSsdCarrier(PsionSSD *ssd);

    // Detach whatever's there. Pulses medchng on detach so the
    // kernel notices the slot is now empty.
    void detachCarrier();

private:
    PsionSSD *m_ssd = nullptr;

    std::function<void(uint8_t)> m_rxdIn;
    std::function<void(bool)>    m_ctsIn;
    std::function<void(bool)>    m_dsrIn;
    std::function<void(bool)>    m_dcdIn;
    std::function<void(bool)>    m_medChngOut;
};
