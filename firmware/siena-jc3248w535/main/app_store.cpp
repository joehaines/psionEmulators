// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "app_store.h"

#include "emubase.h"
#include "key_player.h"

#include <cstdlib>
#include <cstring>

namespace app_store {

std::vector<Entry> parseIndex(const char *text, size_t size, uint32_t &packId) {
    std::vector<Entry> out;
    const std::string_view all(text, strnlen(text, size));
    if (all.substr(0, 12) != "SIENASTORE1 ") return out;
    packId = uint32_t(std::strtoul(std::string(all.substr(12, 8)).c_str(), nullptr, 16));
    size_t at = all.find('\n') + 1;
    while (at > 0 && at < all.size()) {
        size_t end = all.find('\n', at);
        if (end == std::string_view::npos) end = all.size();
        const std::string_view line = all.substr(at, end - at);
        at = end + 1;
        const size_t t1 = line.find('\t'), t2 = line.find('\t', t1 + 1);
        if (t1 == std::string_view::npos || t2 == std::string_view::npos) continue;
        out.push_back({ { line.substr(0, t1), line.substr(t2 + 1) }, line.substr(t1 + 1, t2 - t1 - 1) });
    }
    return out;
}

void queueInstall(KeyPlayer &p, std::string_view program) {
    // Esc closes a menu or dialog in the current app; System (F1) then
    // switches to the System screen from anywhere.
    p.tap(EStdKeyEscape);
    p.waitIdle();
    p.tap(EStdKeyF1);
    p.waitIdle();
    // Install: Disk A (typing the letter picks it from the list), then
    // wait while the dialog reads \APP\ on the pack.
    p.tap('I', kModPsion);
    p.waitIdle();
    p.tap(EStdKeyDownArrow);
    p.waitIdle();
    p.tap('A');
    p.wait(500);
    p.waitIdle();
    // The file field finds the program as its name is typed.
    p.tap(EStdKeyUpArrow);
    p.waitIdle();
    const size_t slash = program.rfind('\\');
    for (char c : program.substr(slash == std::string_view::npos ? 0 : slash + 1))
        p.typeChar(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    p.waitIdle();
    p.tap(EStdKeyEnter);
    // Installed, the dialog closes. Already installed, it stays open and
    // this closes it; on the System screen Esc does nothing.
    p.wait(500);
    p.waitIdle();
    p.tap(EStdKeyEscape);
}

} // namespace app_store
