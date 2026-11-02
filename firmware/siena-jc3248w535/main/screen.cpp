// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "screen.h"

#include "font5x8.h"

namespace {

constexpr uint16_t rgb565be(uint8_t r, uint8_t g, uint8_t b) {
    const uint16_t v = uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return uint16_t((v >> 8) | (v << 8));
}

struct Theme { const char *name; uint8_t rgb[3][3]; };   // paper, ink, grey
constexpr Theme kThemes[Screen::kThemeCount] = {
    // The web emulator's Siena palette (Series3c::Emulator::readLCDIntoBuffer).
    { "Siena",   { { 0xA0, 0xB4, 0xAA }, { 0x28, 0x28, 0x20 }, { 0x68, 0x70, 0x60 } } },
    { "Paper",   { { 0xF4, 0xF2, 0xEA }, { 0x10, 0x10, 0x18 }, { 0x88, 0x88, 0x88 } } },
    { "Backlit", { { 0x18, 0x34, 0x88 }, { 0xE8, 0xF0, 0xFF }, { 0x70, 0x88, 0xC8 } } },
    { "Amber",   { { 0x1A, 0x10, 0x04 }, { 0xFF, 0xB0, 0x20 }, { 0x9A, 0x68, 0x10 } } },
    { "Green",   { { 0x9B, 0xBC, 0x0F }, { 0x0F, 0x38, 0x0F }, { 0x4C, 0x72, 0x18 } } },
    { "Night",   { { 0x10, 0x14, 0x18 }, { 0xC8, 0xD0, 0xD8 }, { 0x60, 0x68, 0x70 } } },
};
constexpr uint16_t kBoxColour = rgb565be(0xF0, 0xF0, 0xE8);
constexpr uint16_t kBoxInk    = rgb565be(0x10, 0x30, 0x80);

// 3x5 digits for the passkey, one row per nibble (bit 2 = left column).
constexpr uint8_t kDigits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};
constexpr int kDigitScale = 10;                       // 30x50 per digit
constexpr int kDigitPitch = 4 * kDigitScale;          // 10px gap
constexpr int kTextW = 6 * kDigitPitch - kDigitScale; // 230
constexpr int kBoxW = kTextW + 60, kBoxH = 5 * kDigitScale + 60;
constexpr int kBoxX = (480 - kBoxW) / 2, kBoxY = (320 - kBoxH) / 2;

} // namespace

bool Screen::passkeyPixel(int x, int y, bool &ink) const {
    if (m_passkey < 0) return false;
    if (x < kBoxX || x >= kBoxX + kBoxW || y < kBoxY || y >= kBoxY + kBoxH)
        return false;
    ink = false;
    // A 4px frame, then the digits centred inside.
    if (x < kBoxX + 4 || x >= kBoxX + kBoxW - 4 || y < kBoxY + 4 || y >= kBoxY + kBoxH - 4) {
        ink = true;
        return true;
    }
    const int tx = x - (kBoxX + 30), ty = y - (kBoxY + 30);
    if (tx < 0 || tx >= kTextW || ty < 0 || ty >= 5 * kDigitScale) return true;
    const int slot = tx / kDigitPitch, cx = (tx % kDigitPitch) / kDigitScale;
    if (cx >= 3) return true;
    int div = 100000;
    for (int i = 0; i < slot; ++i) div /= 10;
    const int digit = (m_passkey / div) % 10;
    ink = (kDigits[digit][ty / kDigitScale] >> (2 - cx)) & 1;
    return true;
}

const char *Screen::themeName(int theme) {
    return kThemes[(theme % kThemeCount + kThemeCount) % kThemeCount].name;
}

void Screen::setTheme(int theme) {
    const Theme &t = kThemes[(theme % kThemeCount + kThemeCount) % kThemeCount];
    for (int i = 0; i < 3; ++i) m_colour[i] = rgb565be(t.rgb[i][0], t.rgb[i][1], t.rgb[i][2]);
}

// The Siena level for picture pixel (x, y). Doubled, each LCD pixel is a
// 2x2 block; smoothed, Scale2x (EPX) gives each quarter of the block the
// colour of the two neighbours it touches when they match each other and
// not the other two, which turns stair-steps into diagonals.
uint8_t Screen::sienaLevel(const uint8_t *levels, int x, int y) const {
    const int sx = x >> 1, sy = y >> 1;
    const uint8_t *p = levels + sy * kSienaW + sx;
    if (!m_smooth) return *p;
    const uint8_t up    = sy > 0           ? p[-kSienaW] : *p;
    const uint8_t down  = sy < kSienaH - 1 ? p[kSienaW]  : *p;
    const uint8_t left  = sx > 0           ? p[-1]       : *p;
    const uint8_t right = sx < kSienaW - 1 ? p[1]        : *p;
    // The neighbours this quarter touches: vertical then horizontal.
    const uint8_t v = (y & 1) ? down : up, h = (x & 1) ? right : left;
    const uint8_t vOther = (y & 1) ? up : down, hOther = (x & 1) ? left : right;
    return (v == h && v != vOther && h != hOther) ? v : *p;
}

// Overlay text: 12x16 cells (the font doubled, a blank column between
// letters). The panel fills the screen bar a margin; a notice sits in a
// box at the foot.
namespace {
constexpr int kCellW = 12, kCellH = 16;
constexpr int kPanelX = (480 - Overlay::kCols * kCellW) / 2;
constexpr int kPanelY = (320 - Overlay::kRows * kCellH) / 2;

// Ink at (cx, cy) inside a character cell of `ch`?
bool glyphInk(char ch, int cx, int cy) {
    const int c = uint8_t(ch);
    if (c < 0x20 || c > 0x7F) return false;
    const int col = cx >> 1, row = cy >> 1;
    return col < 5 && ((kFont5x8[c - 0x20][row] >> (4 - col)) & 1);
}

int textLength(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    return n;
}
} // namespace

bool Screen::overlayPixel(int x, int y, uint16_t &colour) const {
    const uint16_t paper = m_colour[0], ink = m_colour[1];
    if (m_overlay.panel) {
        // Everything outside the text area is a 4px frame on the margin.
        const int tx = x - kPanelX, ty = y - kPanelY;
        if (tx < 0 || ty < 0 || tx >= Overlay::kCols * kCellW || ty >= Overlay::kRows * kCellH) {
            const bool frame = tx >= -8 && tx < Overlay::kCols * kCellW + 8 &&
                               ty >= -8 && ty < Overlay::kRows * kCellH + 8 &&
                               !(tx >= -4 && tx < Overlay::kCols * kCellW + 4 &&
                                 ty >= -4 && ty < Overlay::kRows * kCellH + 4);
            colour = frame ? ink : paper;
            return true;
        }
        const int row = ty / kCellH, col = tx / kCellW;
        const bool inverted = row == m_overlay.highlight;
        const char *line = m_overlay.lines[row];
        const bool on = col < textLength(line) && glyphInk(line[col], tx % kCellW, ty % kCellH);
        colour = (on != inverted) ? ink : paper;
        return true;
    }
    if (m_overlay.notice[0]) {
        const int n = textLength(m_overlay.notice);
        const int w = n * kCellW + 24, h = kCellH + 16;
        const int bx = (480 - w) / 2, by = 320 - h - 12;
        if (x < bx || x >= bx + w || y < by || y >= by + h) return false;
        const int tx = x - bx - 12, ty = y - by - 8;
        const bool frame = x < bx + 3 || x >= bx + w - 3 || y < by + 3 || y >= by + h - 3;
        const bool on = frame || (tx >= 0 && ty >= 0 && tx < n * kCellW && ty < kCellH &&
                                  glyphInk(m_overlay.notice[tx / kCellW], tx % kCellW, ty));
        colour = on ? ink : paper;
        return true;
    }
    return false;
}

uint16_t Screen::pixel(const uint8_t *levels, int x, int y) const {
    bool ink;
    if (passkeyPixel(x, y, ink)) return ink ? kBoxInk : kBoxColour;
    uint16_t colour;
    if (overlayPixel(x, y, colour)) return colour;
    const uint8_t level = sienaLevel(levels, x, y);
    return m_colour[level < 3 ? level : 1];
}

// Does anything the board draws cross picture column x?
bool Screen::columnCovered(int x) const {
    if (m_overlay.panel) return true;
    if (m_passkey >= 0 && x >= kBoxX && x < kBoxX + kBoxW) return true;
    if (m_overlay.notice[0]) {
        const int w = textLength(m_overlay.notice) * kCellW + 24;
        if (x >= (480 - w) / 2 && x < (480 + w) / 2) return true;
    }
    return false;
}

void Screen::renderRows(const uint8_t *levels, int firstRow, int count,
                        uint16_t *out) const {
    // Panel (col, row) → landscape (x, y): the panel's rows run along the
    // picture's width, so each is one picture column.
    for (int r = firstRow; r < firstRow + count; ++r) {
        const int x = m_flipped ? (kPanelH - 1 - r) : r;
        if (columnCovered(x)) {
            for (int c = 0; c < kPanelW; ++c)
                *out++ = pixel(levels, x, m_flipped ? c : (kPanelW - 1 - c));
            continue;
        }
        if (m_smooth) {
            for (int c = 0; c < kPanelW; ++c) {
                const uint8_t level = sienaLevel(levels, x, m_flipped ? c : (kPanelW - 1 - c));
                *out++ = m_colour[level < 3 ? level : 1];
            }
            continue;
        }
        // Plain doubling, the common case: each LCD pixel is two in a row
        // here (and the next panel row repeats this one).
        const uint8_t *column = levels + (x >> 1);
        for (int c = 0; c < kPanelW; c += 2) {
            const int sy = (m_flipped ? c : (kPanelW - 1 - c)) >> 1;
            const uint8_t level = column[sy * kSienaW];
            const uint16_t colour = m_colour[level < 3 ? level : 1];
            *out++ = colour;
            *out++ = colour;
        }
    }
}
