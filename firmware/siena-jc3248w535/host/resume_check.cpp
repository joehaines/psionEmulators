// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Checks that saving and restoring the machine (SienaMachine::saveState,
// as the board does to the SD card) resumes exactly. A scripted session
// runs straight through; another stops at T, writes RAM + state to a file
// and exits; a fresh process loads the file and finishes the script. Fresh
// processes matter: a restore into the same process would inherit any
// state a serializer missed through statics. host/resume_check.sh runs
// all three and compares.
//
//   resume_check straight <rom> T END [state-out]
//   resume_check split    <rom> T file
//   resume_check resume   <rom> T END file [state-out]
//
// Prints FINAL (state + RAM + LCD at END) and TRAIL (cycles and LCD every
// slice from T, RAM every 10th slice).

#include "../main/app_store.h"
#include "../main/hid_keymap.h"
#include "../main/key_player.h"
#include "../main/siena_machine.h"
#include "../main/terminal_keys.h"

#include "emubase.h"
#include "v30.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
SienaMachine *g_m;
KeyPlayer g_player;
uint8_t g_levels[SienaMachine::kLcdW * SienaMachine::kLcdH];

struct Hash {
    uint64_t h = 1469598103934665603ull;
    void mix(const void *p, size_t n) {
        const uint8_t *b = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    }
};

struct Ev { int64_t ms; int kind; uint8_t mods, key; const char *text; };
// kind 0: HID report (mods, key); 1: terminal text
std::vector<Ev> script() {
    std::vector<Ev> e;
    auto tap = [&](int64_t t, uint8_t mods, uint8_t key) {
        e.push_back({ t, 0, mods, key, nullptr });
        e.push_back({ t + 60, 0, mods, 0, nullptr });
        e.push_back({ t + 120, 0, 0, 0, nullptr });
    };
    tap(20000, 0, 0x3C);                       // F3 Word
    const uint8_t word[] = { 0x0B, 0x08, 0x0F, 0x0F, 0x12, 0x2C, 0x1A, 0x12, 0x15, 0x0F, 0x07 };
    int64_t t = 23000;
    for (uint8_t k : word) { tap(t, k == 0x0B ? 0x02 : 0, k); t += 200; }
    e.push_back({ 26000, 1, 0, 0, "\r\nusb: (ok?) @#\xa3 [x]" });
    tap(30000, 0x04, 0x16);                    // Psion+S
    tap(33000, 0, 0x3D);                       // F4 Agenda
    tap(37000, 0x04, 0);                       // Alt alone: diamond
    tap(39000, 0x01, 0);                       // Ctrl alone: Menu
    tap(41000, 0, 0x29);                       // Esc
    return e;
}

std::vector<uint8_t> readFile(const char *p) {
    std::vector<uint8_t> v;
    FILE *f = std::fopen(p, "rb");
    if (!f) { std::perror(p); std::exit(1); }
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) v.insert(v.end(), buf, buf + n);
    std::fclose(f);
    return v;
}
}

int main(int argc, char **argv) {
    const std::string mode = argv[1];
    std::vector<uint8_t> rom = readFile(argv[2]);
    rom.resize(0x100000);
    const int64_t T = std::atoll(argv[3]);
    g_m = new SienaMachine();
    g_m->loadRom(rom.data(), rom.size());
    // With STORE set (an app store image), its pack sits in the slot and
    // the script installs STORE_APP from it at 42 s: the Siena writes the
    // pack's directory, and those writes must survive the split too.
    std::vector<uint8_t> store;
    std::string installProgram;
    if (const char *path = std::getenv("STORE")) {
        store = readFile(path);
        uint32_t packId = 0;
        for (const app_store::Entry &e : app_store::parseIndex(
                 reinterpret_cast<const char *>(store.data()) + 0x800000, 0x10000, packId))
            if (const char *want = std::getenv("STORE_APP"); want && e.app.name == want)
                installProgram = std::string(e.program);
        if (!g_m->attachPack(store.data(), 0x800000) || installProgram.empty()) {
            std::fprintf(stderr, "no such store or app\n");
            return 1;
        }
    }
    HidKeymap hid(g_player);
    TerminalKeys term(g_player);
    const size_t ramSize = g_m->emu().getRamSize();

    int64_t end = 0;
    if (mode == "straight") end = std::atoll(argv[4]);
    if (mode == "split") end = T;
    if (mode == "resume") {
        end = std::atoll(argv[4]);
        std::vector<uint8_t> f = readFile(argv[5]);
        std::memcpy(g_m->emu().getRamBuffer(), f.data(), ramSize);
        if (!g_m->loadState(f.data() + ramSize, f.size() - ramSize)) {
            std::fprintf(stderr, "loadState failed\n");
            return 1;
        }
        if (g_m->nowMs() < T) { std::fprintf(stderr, "resumed at %lld, not %lld\n", (long long)g_m->nowMs(), (long long)T); return 1; }
    }

    Hash trail;
    const std::vector<Ev> ev = script();
    size_t next = 0;
    while (next < ev.size() && ev[next].ms < g_m->nowMs()) ++next;   // resume: past events done
    int n = 0;
    while (g_m->nowMs() < end) {
        const int64_t now = g_m->nowMs();
        for (; next < ev.size() && ev[next].ms <= now; ++next) {
            const Ev &e = ev[next];
            if (e.kind == 0) { const uint8_t r[8] = { e.mods, 0, e.key }; hid.onReport(r, 8); }
            else for (const char *c = e.text; *c; ++c) term.onByte(uint8_t(*c), now);
        }
        if (!installProgram.empty() && now >= 42000 && now < 42010)
            app_store::queueInstall(g_player, installProgram);
        term.poll(now);
        g_player.update(g_m->emu(), now);
        g_m->runUntilMs(now + 10);
        if (now >= T) {
            int64_t c = g_m->emu().currentCycles();
            trail.mix(&c, sizeof c);
            if (++n % 10 == 0) trail.mix(g_m->emu().getRamBuffer(), ramSize);
            if (n % 5 == 0) { g_m->readLcd(g_levels); trail.mix(g_levels, sizeof g_levels); }
        }
    }

    if (mode == "split") {
        if (!g_player.idle()) { std::fprintf(stderr, "player busy at T\n"); return 1; }
        std::vector<uint8_t> out(g_m->emu().getRamBuffer(), g_m->emu().getRamBuffer() + ramSize);
        std::vector<uint8_t> st;
        g_m->saveState(st);
        out.insert(out.end(), st.begin(), st.end());
        FILE *f = std::fopen(argv[4], "wb");
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
        std::printf("split at %lld ms: state %zu bytes, halted %d\n", (long long)g_m->nowMs(), st.size(),
                    int(g_m->emu().getV30Cpu()->halted));
        return 0;
    }
    Hash fin;
    std::vector<uint8_t> st;
    g_m->saveState(st);
    fin.mix(st.data(), st.size());
    fin.mix(g_m->emu().getRamBuffer(), ramSize);
    g_m->readLcd(g_levels);
    fin.mix(g_levels, sizeof g_levels);
    std::printf("FINAL %016llx TRAIL %016llx at %lld ms\n", (unsigned long long)fin.h,
                (unsigned long long)trail.h, (long long)g_m->nowMs());
    if (argc > 6 || (mode == "straight" && argc > 5)) {
        FILE *f = std::fopen(argv[argc - 1], "wb");
        std::fwrite(st.data(), 1, st.size(), f);
        std::fclose(f);
    }
    return 0;
}
