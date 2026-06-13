// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_asic3, BSD-3-Clause)
//                  + adaptation by the Psion emulator project, 2026.
//
// See psion_asic3.h for context. This implementation mirrors
// reference/mame-psion/machine/psion_asic3.cpp; the read paths return
// enough PowerFail / battery state for the boot ROM to advance into
// LCD init.

#include "psion_asic3.h"

namespace {

// SIBO frame opcodes (bits 8-9 of the 10-bit frame). Match
// PsionAsic2::NULL_FRAME / CONTROL_FRAME / DATA_FRAME.
constexpr uint16_t SIBO_NULL_FRAME    = 0x000;
constexpr uint16_t SIBO_CONTROL_FRAME = 0x100;
constexpr uint16_t SIBO_DATA_FRAME    = 0x200;
constexpr uint16_t SIBO_OPCODE_MASK   = 0x300;

} // namespace

void PsionAsic3::reset() {
    m_sibo_control = 0;
    m_a3_control1  = 0;
    m_a3_control2  = 0;
    m_a3_control3  = 0;
    // Don't reset the digitiser axes — they're host-side state that
    // persists across SIBO bus resets.
}

void PsionAsic3::setDigitiserAxis(int axis, uint16_t value) {
    if (axis < 0 || axis > 1) return;
    m_digitiser[axis] = value & 0x7FF;
}

void PsionAsic3::writeFrame(uint16_t frame) {
    switch (frame & SIBO_OPCODE_MASK) {
    case SIBO_NULL_FRAME:
        // Channel reset — clear sibo_control so the next read returns
        // nothing until a fresh CONTROL_FRAME arrives.
        reset();
        break;

    case SIBO_CONTROL_FRAME:
        m_sibo_control = uint8_t(frame & 0xFF);
        break;

    case SIBO_DATA_FRAME: {
        const uint8_t data = uint8_t(frame & 0xFF);
        if (m_variant == Variant::Asic5) {
            // MC400 / MC200 / MC Word path.
            switch (m_sibo_control & 0x0F) {
            case 0x01: // A3Control1: b5 Vcc3Enable (LCD), b6 Vcc4Enable (sound),
                       //              b7 Vcc5Enable (SSDs), b0-4 DtoaBits.
                m_a3_control1 = data;
                break;
            case 0x02: // A3Setup — write-only side-effects we don't model.
                break;
            case 0x03: // A3Control2: Vee1Enable, Vee2Enable, VhEnable, soft-starts,
                       //              AnalogueMultiplex (b6-b7).
                m_a3_control2 = data;
                break;
            case 0x07: // A3Control3: OffEnable, AdcReadHighEnable.
                m_a3_control3 = data;
                break;
            default:
                break;
            }
        } else {
            // MC600 / HC path (older PS34 layout).
            switch (m_sibo_control & 0x0F) {
            case 0x00: // PS34W_CONTROL — power rail enables.
                m_a3_control1 = data;
                break;
            case 0x01: // PS34W_DTOA — Dac + Adcsel + Ncc.
                m_a3_control2 = data;
                break;
            default:
                break;
            }
        }
        break;
    }
    }
}

uint8_t PsionAsic3::readFrame() {
    switch (m_sibo_control & 0xC0) {
    case 0x40: // SerialSelect
        // A3SelectId responds with the info byte 0x80 — same on both
        // ASIC3 and ASIC5 variants.
        if (m_sibo_control == 0x43) return 0x80;
        return 0;

    case 0xC0: // SerialRead
        if (m_variant == Variant::Asic5) {
            switch (m_sibo_control & 0x0F) {
            case 0x00: { // A3Adc — returns selected analogue channel
                int adcSel = (m_a3_control2 >> 6) & 0x03;
                uint16_t adc = 0;
                switch (adcSel) {
                case 0: adc = m_digitiser[m_dr & 1]; break; // touchpad X/Y
                case 1: adc = 0; break;                      // Vh
                case 2: case 3: adc = 0x7FF; break;          // batteries: full
                }
                if (m_a3_control3 & 0x04) {           // AdcReadHighEnable
                    return uint8_t((adc >> 8) & 0x0F);
                }
                return uint8_t((adc & 0xFF) ^ 0x80);
            }
            case 0x0D: // A3Status: b0 ColdStart, b1 PowerFail
                return 0x02; // PowerFail set (required to initialise MC400)
            default:
                return 0;
            }
        } else {
            // ASIC3 variant
            switch (m_sibo_control & 0x0F) {
            case 0x00: { // PS34R_ADC
                int adcSel = (m_a3_control2 >> 5) & 0x03;
                if (adcSel == 1 || adcSel == 3) return 0xFF; // VIN / VBATT full
                return 0;
            }
            case 0x01: { // PS34R_STATUS: b6 ColdStart, b7 PowerFail
                int adcSel = (m_a3_control2 >> 5) & 0x03;
                uint8_t data = 0;
                if (adcSel == 1 || adcSel == 3) data = 0x07;
                data |= 0x40; // PowerFail (required to initialise MC600)
                return data;
            }
            default:
                return 0;
            }
        }
    }
    return 0;
}
