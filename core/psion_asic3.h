// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_asic3, BSD-3-Clause)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion ASIC3 / ASIC5 PSU (PS34) — minimal stub
//
// The MC and HC laptops use a custom Maxim MAX616 power-supply ASIC
// connected to ASIC2 SIBO channel 0. The MC400 boot ROM polls this
// channel for the PowerFail bit and won't enable the LCD until it sees
// a valid acknowledgement.
//
// We only model the SIBO frame protocol (CONTROL_FRAME / DATA_FRAME /
// SerialSelect / SerialRead) and just enough of A3Status / A3Adc for
// the kernel to reach the LCD-enable code path. The actual power-rail
// outputs and DAC channels aren't observable from the CPU side, so
// writes to A3Control{1,2,3} are stored but otherwise ignored.
//
// MC400 / MC600 / HC use the older PS34 (ASIC3 variant); MC200 / MC Word
// use the newer ASIC5 variant. The two share the SIBO frame protocol
// but have different register layouts. We default to the ASIC5 layout
// because it's what MAME wires for the MC400 (`PSION_PSU_ASIC5(... m_asic3)`
// in `reference/mame-psion/psion/mc400.cpp`).

#pragma once

#include <cstdint>

class PsionAsic3 {
public:
    enum class Variant {
        Asic3,  // MC600 / HC family, PS34W_CONTROL / PS34R_STATUS layout
        Asic5,  // MC400 / MC200 / MC Word family, A3Control1/2/3 layout
    };

    explicit PsionAsic3(Variant v = Variant::Asic5) : m_variant(v) {}

    void reset();

    // SIBO frame protocol — wired up via PsionAsic2::setChannelReadCb /
    // setChannelWriteCb on channel 0. data is a 10-bit SIBO frame:
    //   0x000-0x0FF  data byte
    //   0x100-0x1FF  data byte alt encoding
    //   0x200        NULL_FRAME — channel reset
    //   0x300        CONTROL_FRAME (low byte stored as m_sibo_control)
    void     writeFrame(uint16_t frame);
    uint8_t  readFrame();

    // ADC input for the MC400 trackpad. The PSU multiplexes four analog
    // channels onto its ADC: digitiser X (channel 0 with AnalogueMux=0),
    // digitiser Y (digitiser X-axis with the digitiser-direction line
    // toggled — handled outside the chip), Vh, and the two batteries.
    // The host calls setDigitiserAxis(0, x) / setDigitiserAxis(1, y) to
    // feed the ADC; readFrame's case 0x00 (A3Adc) returns the value of
    // m_digitiser[m_dr] where m_dr is the X/Y switch driven by ASIC2
    // A2Control2 bit 1.
    void setDigitiserAxis(int axis, uint16_t value);
    void setDigitiserDirection(int dr) { m_dr = dr ? 1 : 0; }

private:
    Variant  m_variant;
    uint8_t  m_sibo_control = 0;
    uint8_t  m_a3_control1  = 0;
    uint8_t  m_a3_control2  = 0;
    uint8_t  m_a3_control3  = 0;
    uint16_t m_digitiser[2] = { 0, 0 };  // X (m_dr=0) and Y (m_dr=1)
    int      m_dr           = 0;
};
