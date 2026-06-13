// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME hd44780_device)
//                  + adaptation by the Psion emulator project, 2026.
//
// Hitachi HD44780 character-LCD controller — scaffold.
//
// Used by the Psion Organiser II's 16x4 character display (LZ / LZ64
// models). This is a minimal stub: it accepts command + data writes,
// keeps a DDRAM buffer, and renders an all-paper framebuffer. The full
// character-set rendering (CGROM font + CGRAM user glyphs + display
// shift / cursor blink / 4-bit interface mode) lands once the HD6303
// CPU port is far enough along to actually exercise the controller.
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

    HD44780() = default;

    void reset();

    // CPU-side register interface. The Organiser II decodes RS via an
    // address bit inside the $0100-$03FF I/O region; the driver picks
    // the right call.
    void    writeCommand(uint8_t cmd);
    void    writeData(uint8_t data);
    uint8_t readStatus() const;
    uint8_t readData();

    // Render the current DDRAM into an 80x32 dot buffer (0 = paper,
    // 1 = ink). Stub: paints all-paper. Once the CGROM font is wired
    // in, this will render real character glyphs.
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
    uint8_t m_ac      = 0;     // Address counter
    bool    m_acIsCgram = false;
    bool    m_displayOn = false;
    bool    m_cursorOn  = false;
    bool    m_blinkOn   = false;
    bool    m_increment = true;
    uint8_t m_busy      = 0;   // Busy-flag bit (BF) for status reads
};
