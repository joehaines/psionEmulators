// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Host check of the firmware's portable half: boots the Siena exactly as the
// board does, drives it through the same keyboard paths (Bluetooth HID
// reports, USB terminal bytes) and writes what the panel would show.
//
//   bash host/build.sh && host/host_sim <siena.rom> <out-dir>
//
// Writes <out-dir>/boot.ppm, word.ppm, menu.ppm, agenda.ppm, passkey.ppm
// and restored.ppm (480x320, the picture as it appears on the glass) and
// prints emulation speed.

#include "../main/hid_keymap.h"
#include "../main/key_player.h"
#include "../main/screen.h"
#include "../main/siena_machine.h"
#include "../main/terminal_keys.h"
#include "../main/ui.h"

#include "emubase.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

SienaMachine *g_machine;
KeyPlayer     g_player;
uint8_t       g_levels[SienaMachine::kLcdW * SienaMachine::kLcdH];

// Run in 10 ms slices, as the firmware does, playing queued keys.
void run(int64_t ms) {
    const int64_t end = g_machine->nowMs() + ms;
    while (g_machine->nowMs() < end) {
        g_player.update(g_machine->emu(), g_machine->nowMs());
        g_machine->runUntilMs(g_machine->nowMs() + 10);
    }
}

// Render through Screen (panel rows, rotation and all), then turn the
// panel image back into the landscape picture a viewer sees.
void snapshot(const Screen &screen, const std::string &path) {
    g_machine->readLcd(g_levels);
    std::vector<uint16_t> panel(Screen::kPanelW * Screen::kPanelH);
    for (int r = 0; r < Screen::kPanelH; r += 48)
        screen.renderRows(g_levels, r, 48, &panel[size_t(r) * Screen::kPanelW]);
    FILE *f = std::fopen(path.c_str(), "wb");
    std::fprintf(f, "P6 480 320 255\n");
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 480; ++x) {
            // Inverse of Screen::renderRows (unflipped): row = x, col = 319 - y.
            uint16_t be = panel[size_t(x) * Screen::kPanelW + (319 - y)];
            uint16_t v = uint16_t((be >> 8) | (be << 8));
            uint8_t rgb[3] = { uint8_t((v >> 8) & 0xF8), uint8_t((v >> 3) & 0xFC),
                               uint8_t((v << 3) & 0xF8) };
            std::fwrite(rgb, 1, 3, f);
        }
    }
    std::fclose(f);
    std::printf("wrote %s\n", path.c_str());
}

// One HID keyboard report: modifiers + up to two keys.
void report(HidKeymap &hid, uint8_t mods, uint8_t k1 = 0, uint8_t k2 = 0) {
    const uint8_t r[8] = { mods, 0, k1, k2, 0, 0, 0, 0 };
    hid.onReport(r, sizeof r);
    run(30);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <siena.rom> <out-dir>\n", argv[0]);
        return 2;
    }
    std::vector<uint8_t> rom(0x100000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(rom.data(), 1, rom.size(), f) != rom.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    std::fclose(f);
    const std::string out = argv[2];

    g_machine = new SienaMachine();
    g_machine->loadRom(rom.data(), rom.size());
    Screen screen;

    auto t0 = std::chrono::steady_clock::now();
    run(20'000);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("boot: 20 emulated s in %.2f host s (%.1fx real time)\n", secs, 20.0 / secs);
    snapshot(screen, out + "/boot.ppm");

    // Bluetooth path: F3 (Word), then type with Shift chords and symbols.
    HidKeymap hid(g_player);
    report(hid, 0, 0x3C);           // F3 down
    report(hid, 0);                 // up
    run(3000);
    const struct { uint8_t mods, key; } typed[] = {
        { 0x02, 0x0B }, { 0, 0x08 }, { 0, 0x0F }, { 0, 0x0F }, { 0, 0x12 },  // Hello
        { 0, 0x36 }, { 0, 0x2C },                                           // ", "
        { 0x20, 0x1A }, { 0, 0x12 }, { 0, 0x15 }, { 0, 0x0F }, { 0, 0x07 }, // World (RShift)
        { 0x02, 0x1E },                                                     // !  (Shift+1)
        { 0, 0x2C }, { 0x02, 0x25 }, { 0x02, 0x2E }, { 0, 0x2E }, { 0, 0x2D }, // " *+=-"
    };
    for (auto k : typed) {
        report(hid, k.mods, k.key);
        report(hid, k.mods);
        report(hid, 0);
    }
    // USB terminal path: Enter, then a line with a CR LF and an arrow.
    TerminalKeys term(g_player);
    for (char c : std::string("\r\nusb: (ok?) @#£ é [x] \\ ~\x1b[D"))
        term.onByte(uint8_t(c), g_machine->nowMs());
    for (int i = 0; i < 3000 && !g_player.idle(); ++i) run(10);
    run(1000);
    snapshot(screen, out + "/word.ppm");

    // Ctrl tapped on its own is Menu.
    report(hid, 0x01);
    report(hid, 0);
    run(1500);
    snapshot(screen, out + "/menu.ppm");
    report(hid, 0, 0x29);           // Esc closes it
    report(hid, 0);
    run(500);
    // Alt tapped on its own is the diamond: Agenda's Day view → Week.
    report(hid, 0, 0x3D);           // F4 Agenda
    report(hid, 0);
    run(4000);
    report(hid, 0x04);
    report(hid, 0);
    run(2000);
    snapshot(screen, out + "/agenda.ppm");

    screen.setPasskey(48213);       // shows as 048213
    snapshot(screen, out + "/passkey.ppm");
    screen.setPasskey(-1);

    // The board's own functions, through the same filter the firmware
    // puts in front of the Siena: Shift+F1 (colour), Shift+F2 (smoothing),
    // Shift+F10 (help), Shift+F3 (the store).
    Ui ui;
    ui.setApps({ { "JUMPY", "Platform game" }, { "GUITAR", "Guitar chords" },
                 { "MAPS", "World maps" } });
    hid.filter = [&](int key, uint8_t mods) { return ui.onKey(key, mods, g_machine->nowMs()); };
    ui.onCaptureChanged = [&](bool c) { hid.setCaptured(c); };
    auto show = [&](const std::string &name) {
        screen.setTheme(ui.settings().theme);
        screen.setSmooth(ui.settings().smooth);
        screen.setOverlay(ui.overlay());
        snapshot(screen, out + "/" + name);
    };
    for (int i = 0; i < 3; ++i) { report(hid, 0x02, 0x3A); report(hid, 0x02); report(hid, 0); }
    show("amber.ppm");              // three Shift+F1s: Siena → Paper → Backlit → Amber
    report(hid, 0x02, 0x3B); report(hid, 0x02); report(hid, 0);
    run(3000);
    ui.poll(g_machine->nowMs());
    show("smooth.ppm");             // smoothing on, the notice gone
    report(hid, 0x02, 0x43); report(hid, 0x02); report(hid, 0);
    show("help.ppm");
    report(hid, 0, 0x29); report(hid, 0);          // any key closes it
    report(hid, 0x02, 0x3C); report(hid, 0x02); report(hid, 0);
    report(hid, 0, 0x51); report(hid, 0);          // Down: the second app
    show("store.ppm");
    report(hid, 0, 0x29); report(hid, 0);          // Esc closes the store
    // Settings: Shift+F7, then type a Wi-Fi name into the first field.
    hid.textFilter = [&](char ch) { return ui.onChar(ch, g_machine->nowMs()); };
    report(hid, 0x02, 0x40); report(hid, 0x02); report(hid, 0);
    report(hid, 0, 0x28); report(hid, 0);          // Enter: edit
    for (uint8_t k : { 0x0B, 0x12, 0x10, 0x08 }) { report(hid, 0, k); report(hid, 0); }   // "home"
    show("settings.ppm");
    report(hid, 0, 0x28); report(hid, 0);          // Enter: save
    report(hid, 0x02, 0x40); report(hid, 0x02); report(hid, 0);   // Shift+F7 closes
    screen.setTheme(0);
    screen.setSmooth(false);
    screen.setOverlay(Overlay());

    // The SD card's job: save the document, carry the RAM over to a fresh
    // machine before it boots (a warm reset, as the board restores it),
    // and the document opens again from M:.
    report(hid, 0, 0x3C);           // F3 Word
    report(hid, 0);
    run(2000);
    report(hid, 0x04, 0x16);        // Psion+S saves
    report(hid, 0);
    run(3000);
    std::vector<uint8_t> ram(g_machine->emu().getRamBuffer(),
                             g_machine->emu().getRamBuffer() + g_machine->emu().getRamSize());
    delete g_machine;
    g_machine = new SienaMachine();
    g_machine->loadRom(rom.data(), rom.size());
    g_machine->emu().loadRamSnapshot(ram.data(), ram.size());
    g_player = KeyPlayer();
    HidKeymap hid2(g_player);
    run(20'000);
    report(hid2, 0, 0x3C);          // F3 picks Word on the System screen
    report(hid2, 0);
    run(1000);
    report(hid2, 0, 0x28);          // Enter opens it, with the saved document
    report(hid2, 0);
    run(4000);
    snapshot(screen, out + "/restored.ppm");
    return 0;
}
