// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Draws the Siena's 240x160 LCD on the JC3248W535's 320x480 portrait panel,
// landscape and doubled (480x320), so the Siena screen fills the glass:
// in one of several colour themes, and either as square pixels or with
// Scale2x edge smoothing, which rounds the steps in diagonals and curves.
// Over it go the board's own overlays: a one-line notice, a full text
// panel (the app store), and while a Bluetooth keyboard is pairing, the
// passkey to type on it.
//
// The AXS15231B ignores the row address over QSPI, so a frame is always
// sent as consecutive full-width panel rows from the top; renderRows()
// produces those rows directly, rotation included.
//
// Portable: no ESP-IDF dependency.

#pragma once

#include <cstdint>

// What the board draws over the Siena's picture, in 12x16 character
// cells (the 5x8 font, doubled).
struct Overlay {
    static constexpr int kCols = 38, kRows = 18;
    char notice[kCols + 1] = "";    // a notice at the foot; "" for none
    bool panel = false;             // the text panel, over everything else
    char lines[kRows][kCols + 1] = {};
    int  highlight = -1;            // the panel line drawn inverted
};

class Screen {
public:
    static constexpr int kSienaW = 240, kSienaH = 160;
    static constexpr int kPanelW = 320, kPanelH = 480;

    static constexpr int kThemeCount = 6;
    static const char *themeName(int theme);

    Screen() { setTheme(0); }

    // Which way up the panel sits in the case: false puts the panel's top
    // edge (the end away from its flex cable) on the left of the picture.
    void setFlipped(bool flipped) { m_flipped = flipped; }
    void setTheme(int theme);
    void setSmooth(bool smooth) { m_smooth = smooth; }
    void setOverlay(const Overlay &overlay) { m_overlay = overlay; }
    // A six-digit passkey to show, or -1 for none.
    void setPasskey(int passkey) { m_passkey = passkey; }

    // Fill `count` panel rows from `firstRow` with big-endian RGB565, from
    // `levels`: kSienaW x kSienaH bytes, 0 paper, 1 ink, 2 grey (the
    // ASIC9 LCD levels).
    void renderRows(const uint8_t *levels, int firstRow, int count,
                    uint16_t *out) const;

private:
    // The landscape picture's colour at (x, y), 480x320.
    uint16_t pixel(const uint8_t *levels, int x, int y) const;
    uint8_t  sienaLevel(const uint8_t *levels, int x, int y) const;
    bool overlayPixel(int x, int y, uint16_t &colour) const;
    bool columnCovered(int x) const;
    bool passkeyPixel(int x, int y, bool &ink) const;

    bool    m_flipped = false;
    bool    m_smooth = false;
    int     m_passkey = -1;
    uint16_t m_colour[3];           // the theme's paper, ink, grey
    Overlay m_overlay;
};
