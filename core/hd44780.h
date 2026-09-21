// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME hd44780_device)
//                  + adaptation by the Psion emulator project, 2026.
//
// Hitachi HD44780 character-LCD controller.
//
// Drives both Psion Organisers' panels: the II's 4x20 (LZ / LZ64) and
// the I's single row of 16 characters. Command + data writes, a DDRAM
// buffer, and rendering through the A00 CGROM font plus CGRAM user
// glyphs. Not modelled: display shift, cursor blink, and the 4-bit
// interface mode — no Psion ROM here uses them.
//
// Porting source: MAME src/devices/video/hd44780.{cpp,h} (BSD-3-Clause).

#pragma once

#include <array>
#include <cstdint>

class HD44780 {
public:
    // Display geometry — Psion Organiser II LZ panel: 4 rows × 20 cols
    // (NOT the standard 16×4 HD44780 layout). The LZ uses a custom
    // pixel-update callback in MAME (psion2_state::lz_pixel_update)
    // that remaps each visible char cell to a non-contiguous DDRAM
    // address via psion_display_layout[]; this layout is a Psion-
    // specific scrambling that we replicate verbatim in renderFramebuffer.
    static constexpr int COLS    = 20;
    static constexpr int ROWS    = 4;
    static constexpr int CELL_W  = 6;   // 5 active dots + 1 inter-cell gap
    static constexpr int CELL_H  = 9;   // 8 dots + 1 inter-row gap
    static constexpr int PIXEL_W = COLS * CELL_W;            // 120
    static constexpr int PIXEL_H = ROWS * CELL_H;            // 36

    // Which machine's panel wiring the controller drives. Both Psions
    // put the visible cells somewhere other than where a stock HD44780
    // module would, and each does it differently, so the mapping from
    // "controller cell" to "screen position" is per-machine:
    //
    //   PsionLZ      Organiser II LZ / LZ64: 4 rows × 20 cols through
    //                the scrambled kPsionLayout table (see above).
    //   Organiser1   Organiser I: one physical row of 16 characters,
    //                driven as the controller's 2-line mode × 8
    //                positions — line 0 lights the left half of the
    //                row, line 1 the right (MAME
    //                psion1_state::psion1_pixel_update, which paints
    //                cell (line, pos) at x = (line * 8 + pos) * 6).
    enum class Layout { PsionLZ, Organiser1 };

    static constexpr int ORG1_COLS    = 16;
    static constexpr int ORG1_CELL_H  = 8;   // single row: no inter-row gap
    static constexpr int ORG1_PIXEL_W = ORG1_COLS * CELL_W;  // 96
    static constexpr int ORG1_PIXEL_H = ORG1_CELL_H;         // 8

    HD44780() = default;

    // Set once by the driver, before the first render. The PIXEL_W/H
    // (LZ) and ORG1_PIXEL_W/H constants above are the buffer sizes
    // renderFramebuffer expects for each.
    void setLayout(Layout l) { m_layout = l; }

    void reset();

    // CPU-side register interface. The Organiser II decodes RS via an
    // address bit inside the $0100-$03FF I/O region; the driver picks
    // the right call.
    void    writeCommand(uint8_t cmd);
    void    writeData(uint8_t data);
    uint8_t readStatus() const;
    uint8_t readData();

    // Render the current DDRAM into a dot buffer (0 = paper, 1 = ink)
    // through the configured layout — PIXEL_W x PIXEL_H for the LZ,
    // ORG1_PIXEL_W x ORG1_PIXEL_H for the Organiser I.
    void renderFramebuffer(uint8_t *out, int outW, int outH) const;

    // Debug helper: returns a pointer to the 80-byte DDRAM buffer.
    // Used by the harness to dump on-screen text without rendering.
    const uint8_t *debugDdram() const { return m_ddram.data(); }
    bool debugDisplayOn() const { return m_displayOn; }

private:
    // 128 bytes covers the full 7-bit DDRAM addressable range that the
    // HD44780 / HD66780 (Psion's variant) supports. The Psion LZ
    // scrambles its 80 visible cells across DDRAM addresses up to $4F,
    // and the OS writes scratch / off-screen data into $50-$67, so the
    // canonical 80-byte array overflows. The full $00-$7F range fits
    // every documented controller mode.
    std::array<uint8_t, 128> m_ddram{};
    std::array<uint8_t, 64> m_cgram{};
    Layout  m_layout = Layout::PsionLZ;
    uint8_t m_ac      = 0;     // Address counter
    bool    m_acIsCgram = false;
    bool    m_displayOn = false;
    bool    m_cursorOn  = false;
    bool    m_blinkOn   = false;
    bool    m_increment = true;
    uint8_t m_busy      = 0;   // Busy-flag bit (BF) for status reads
};
