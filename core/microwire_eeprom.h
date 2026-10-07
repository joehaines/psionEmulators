// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <array>
#include <cstdint>

// A 93C46-class Microwire serial EEPROM: 64 words of 16 bits, driven by
// four wires — chip select (CS), clock (SK), data in (DI) and data out
// (DO). The WorkaboutMX keeps its factory configuration in one, bit-banged
// over ASIC9 port B (see Series3c::Emulator).
//
// Protocol, as the part's datasheet gives it and as the v7.20f ROM drives
// it (its routines are at ROM 0x1b727d / 0x1b7343): with CS high, DI is
// sampled on each rising SK edge. A transfer is a start bit (1), a 2-bit
// opcode and a 6-bit address, MSB first:
//
//   10 aaaaaa   READ   — DO drives a dummy 0, then D15..D0, one bit per
//                        further rising SK edge
//   01 aaaaaa   WRITE  — 16 data bits follow on DI
//   11 aaaaaa   ERASE  — the word becomes 0xFFFF
//   00 11xxxx   EWEN   — enable writes and erases
//   00 00xxxx   EWDS   — disable them (the power-up state)
//   00 10xxxx   ERAL   — erase every word
//   00 01xxxx   WRAL   — 16 data bits follow, written to every word
//
// Programming is instantaneous here: DO reads 1 (ready) as soon as CS is
// re-asserted after a write, which is all the ROM's busy poll waits for.
class MicrowireEeprom {
public:
    static constexpr int kWords = 64;

    void load(const std::array<uint16_t, kWords> &words) { m_words = words; }
    uint16_t word(int index) const { return m_words[index & (kWords - 1)]; }

    // Present the four host-driven lines. Call on every change; edges are
    // detected against the previous call.
    void setPins(bool cs, bool sk, bool di) {
        if (!cs) {
            // Deselect: abandon any transfer. DO floats; reads high.
            m_state = State::Idle;
            m_do = true;
        } else if (!m_cs) {
            // Select: a finished program cycle reports ready on DO.
            m_state = State::Start;
            m_do = true;
        } else if (sk && !m_sk) {
            risingEdge(di);
        }
        m_cs = cs;
        m_sk = sk;
    }

    bool dataOut() const { return m_do; }

private:
    enum class State { Idle, Start, Command, ReadOut, WriteIn, Done };

    void risingEdge(bool di) {
        switch (m_state) {
        case State::Idle:
        case State::Done:
            break;
        case State::Start:
            // Leading zeros before the start bit are ignored.
            if (di) { m_state = State::Command; m_shift = 0; m_bits = 0; }
            break;
        case State::Command:
            m_shift = uint16_t((m_shift << 1) | (di ? 1 : 0));
            if (++m_bits == 8) command(uint8_t(m_shift));
            break;
        case State::ReadOut:
            // Each edge after the address shifts the next data bit out.
            m_do = (m_shift & 0x8000) != 0;
            m_shift = uint16_t(m_shift << 1);
            if (++m_bits == 16) {
                // Sequential read: carry on into the next word.
                m_addr = (m_addr + 1) & (kWords - 1);
                m_shift = m_words[m_addr];
                m_bits = 0;
            }
            break;
        case State::WriteIn:
            m_shift = uint16_t((m_shift << 1) | (di ? 1 : 0));
            if (++m_bits == 16) {
                if (m_writeEnabled) {
                    if (m_writeAll) m_words.fill(m_shift);
                    else            m_words[m_addr] = m_shift;
                }
                m_state = State::Done;
            }
            break;
        }
    }

    void command(uint8_t cmd) {
        const int op = cmd >> 6;
        m_addr = cmd & (kWords - 1);
        m_bits = 0;
        m_shift = 0;
        switch (op) {
        case 2:                                     // READ
            m_shift = m_words[m_addr];
            m_do = false;                           // the dummy zero
            m_state = State::ReadOut;
            break;
        case 1:                                     // WRITE
            m_writeAll = false;
            m_state = State::WriteIn;
            break;
        case 3:                                     // ERASE
            if (m_writeEnabled) m_words[m_addr] = 0xFFFF;
            m_state = State::Done;
            break;
        default:                                    // 00: by address bits 5:4
            switch ((cmd >> 4) & 3) {
            case 3: m_writeEnabled = true;  m_state = State::Done; break;  // EWEN
            case 0: m_writeEnabled = false; m_state = State::Done; break;  // EWDS
            case 2:                                                         // ERAL
                if (m_writeEnabled) m_words.fill(0xFFFF);
                m_state = State::Done;
                break;
            case 1: m_writeAll = true; m_state = State::WriteIn; break;    // WRAL
            }
            break;
        }
    }

    std::array<uint16_t, kWords> m_words{};
    State    m_state = State::Idle;
    bool     m_cs = false, m_sk = false, m_do = true;
    bool     m_writeEnabled = false, m_writeAll = false;
    uint16_t m_shift = 0;
    int      m_bits = 0;
    int      m_addr = 0;
};
