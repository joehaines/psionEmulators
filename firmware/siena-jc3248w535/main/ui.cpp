// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "ui.h"

#include "emubase.h"
#include "key_player.h"

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstring>

namespace {

constexpr int64_t kNoticeMs = 2000;
constexpr int kListTop = 2, kListRows = Overlay::kRows - 4;   // rows 2..15

void put(Overlay &o, int row, const std::string &text) {
    std::snprintf(o.lines[row], sizeof o.lines[row], "%s", text.c_str());
}

std::string onOff(bool on) { return on ? "on" : "off"; }

} // namespace

bool Ui::onKey(int key, uint8_t mods, int64_t nowMs) {
    if (m_installing >= 0 || m_busy) return true;   // hands off while it types
    if (m_panel == Panel::Help) {
        open(Panel::None);   // any key closes the help
        return true;
    }
    if (mods == kModShift) {
        switch (key) {
        case EStdKeyF1:
            m_settings.theme = (m_settings.theme + 1) % Screen::kThemeCount;
            notify(std::string("Colour: ") + Screen::themeName(m_settings.theme), nowMs);
            if (onSettingsChanged) onSettingsChanged(m_settings);
            return true;
        case EStdKeyF2:
            m_settings.smooth = !m_settings.smooth;
            notify("Smoothing " + onOff(m_settings.smooth), nowMs);
            if (onSettingsChanged) onSettingsChanged(m_settings);
            return true;
        case EStdKeyF3:
            if (m_panel == Panel::Store) open(Panel::None);
            else if (m_apps.empty()) notify("No app store in this firmware", nowMs);
            else open(Panel::Store);
            return true;
        case EStdKeyF4:
            m_settings.turbo = !m_settings.turbo;
            notify("Turbo " + onOff(m_settings.turbo), nowMs);
            if (onSettingsChanged) onSettingsChanged(m_settings);
            return true;
        case EStdKeyF5: {
            if (m_profiles.empty()) { notify("Profiles need the SD card", nowMs); return true; }
            if (m_profiles.size() < 2) { notify("One profile: Shift+F6 adds one", nowMs); return true; }
            size_t i = 0;
            while (i < m_profiles.size() && m_profiles[i] != m_profile) ++i;
            if (onSwitchProfile) onSwitchProfile(m_profiles[(i + 1) % m_profiles.size()]);
            return true;
        }
        case EStdKeyF6:
            if (m_panel == Panel::Profiles) open(Panel::None);
            else if (m_profiles.empty()) notify("Profiles need the SD card", nowMs);
            else {
                m_profileSel = 0;
                while (m_profileSel < int(m_profiles.size()) - 1 &&
                       m_profiles[m_profileSel] != m_profile) ++m_profileSel;
                m_deleteArmed = -1;
                open(Panel::Profiles);
            }
            return true;
        case EStdKeyF7:
            if (m_panel == Panel::Settings) {
                if (!m_editing) open(Panel::None);
            } else {
                m_field = 0;
                m_editing = false;
                open(Panel::Settings);
            }
            return true;
        case EStdKeyF8:
            notify("Syncing with Google...", nowMs);
            if (onSyncNow) onSyncNow();
            return true;
        case EStdKeyHelp:
            open(Panel::Help);
            return true;
        }
    }
    if (m_panel == Panel::Settings) {
        settingsKey(key, nowMs);
        return true;
    }
    if (m_panel == Panel::Profiles) {
        profilesKey(key, nowMs);
        return true;
    }
    if (m_panel == Panel::Store) {
        storeKey(key, mods, nowMs);
        return true;
    }
    return false;
}

void Ui::storeKey(int key, uint8_t mods, int64_t nowMs) {
    const int n = int(m_apps.size());
    int sel = m_selected;
    if (key == EStdKeyEscape) {
        open(Panel::None);
        return;
    }
    if (key == EStdKeyEnter) {
        // The Siena's Install leaves an installed app out of its list, so
        // typing its name would pick a different one: say so instead,
        // unless Shift (it was removed on the Siena since).
        if (m_apps[sel].installed && !(mods & kModShift)) {
            notify("Installed. Shift+Enter: install again", nowMs);
            return;
        }
        m_installing = sel;
        open(Panel::None);
        notify("Installing " + std::string(m_apps[sel].name) + "...", nowMs);
        m_noticeUntil = INT64_MAX;   // until it's done
        if (onInstall) onInstall(sel);
        return;
    }
    const bool page = mods & kModPsion;   // PgUp / PgDn arrive as Psion+Up / Down
    if (key == EStdKeyUpArrow)   sel -= page ? kListRows : 1;
    if (key == EStdKeyDownArrow) sel += page ? kListRows : 1;
    if (key == EStdKeyLeftArrow && page)  sel = 0;        // Home
    if (key == EStdKeyRightArrow && page) sel = n - 1;    // End
    if (key >= 'A' && key <= 'Z') {
        // The next app whose name starts with the letter.
        for (int i = 1; i <= n; ++i) {
            const int j = (m_selected + i) % n;
            if (!m_apps[j].name.empty() && (m_apps[j].name[0] & ~0x20) == key) { sel = j; break; }
        }
    }
    if (sel < 0) sel = 0;
    if (sel > n - 1) sel = n - 1;
    m_selected = sel;
    if (m_selected < m_top) m_top = m_selected;
    if (m_selected >= m_top + kListRows) m_top = m_selected - kListRows + 1;
    redraw();
}

void Ui::profilesKey(int key, int64_t nowMs) {
    const int n = int(m_profiles.size());
    if (key == EStdKeyEscape) {
        open(Panel::None);
        return;
    }
    if (key == EStdKeyUpArrow && m_profileSel > 0) --m_profileSel;
    if (key == EStdKeyDownArrow && m_profileSel < n - 1) ++m_profileSel;
    const int sel = m_profiles[m_profileSel];
    if (key == EStdKeyEnter) {
        open(Panel::None);
        if (sel != m_profile && onSwitchProfile) onSwitchProfile(sel);
        return;
    }
    if (key == 'N') {
        open(Panel::None);
        const int made = onNewProfile ? onNewProfile() : -1;
        if (made < 0) notify("No room for another profile", nowMs);
        return;
    }
    if (key == EStdKeyBackspace) {
        if (sel == 1 || sel == m_profile) {
            notify(sel == 1 ? "Profile 1 stays" : "Switch away from it first", nowMs);
        } else if (m_deleteArmed != sel) {
            m_deleteArmed = sel;
            notify("Del again deletes Profile " + std::to_string(sel), nowMs);
        } else {
            m_deleteArmed = -1;
            if (onDeleteProfile && onDeleteProfile(sel)) {
                m_profiles.erase(m_profiles.begin() + m_profileSel);
                if (m_profileSel >= int(m_profiles.size())) m_profileSel = int(m_profiles.size()) - 1;
                notify("Profile " + std::to_string(sel) + " deleted", nowMs);
            }
        }
    } else {
        m_deleteArmed = -1;
    }
    redraw();
}

// The settings panel's fields, in order.
namespace {
constexpr int kFields = 5;
const char *const kFieldNames[kFields] = {
    "Wi-Fi network", "Wi-Fi password", "Gmail address", "Gmail app pw", "Calendar iCal",
};
constexpr bool kFieldSecret[kFields] = { false, true, false, true, false };
}

std::string *Ui::field(int i) {
    switch (i) {
    case 0: return &m_account.wifiName;
    case 1: return &m_account.wifiPassword;
    case 2: return &m_account.gmailAddress;
    case 3: return &m_account.gmailAppPassword;
    default: return &m_account.calendarUrl;
    }
}

bool Ui::onChar(char ch, int64_t) {
    if (!m_editing) return false;
    if (m_edit.size() < 300) m_edit.push_back(ch);
    redraw();
    return true;
}

void Ui::settingsKey(int key, int64_t nowMs) {
    if (m_editing) {
        if (key == EStdKeyEnter) {
            *field(m_field) = m_edit;
            m_editing = false;
            if (onAccountChanged) onAccountChanged(m_account);
            notify(std::string(kFieldNames[m_field]) + " saved", nowMs);
        } else if (key == EStdKeyEscape) {
            m_editing = false;
        } else if (key == EStdKeyBackspace && !m_edit.empty()) {
            m_edit.pop_back();
        }
        redraw();
        return;
    }
    if (key == EStdKeyEscape) {
        open(Panel::None);
        return;
    }
    if (key == EStdKeyUpArrow && m_field > 0) --m_field;
    if (key == EStdKeyDownArrow && m_field < kFields - 1) ++m_field;
    if (key == EStdKeyEnter) {
        m_edit = *field(m_field);
        m_editing = true;
    }
    redraw();
}

void Ui::setStatus(const std::string &status) {
    m_status = status;
    if (m_panel == Panel::Settings) redraw();
}

void Ui::setInstalled(const std::string &names) {
    for (App &a : m_apps)
        a.installed = ("\n" + names + "\n").find("\n" + std::string(a.name) + "\n") !=
                      std::string::npos;
    redraw();
}

void Ui::setProfiles(std::vector<int> profiles, int current) {
    m_profiles = std::move(profiles);
    m_profile = current;
    redraw();
}

void Ui::open(Panel p) {
    const bool was = capturing();
    m_panel = p;
    redraw();
    if (capturing() != was && onCaptureChanged) onCaptureChanged(capturing());
}

void Ui::beginBusy(const std::string &notice, int64_t nowMs) {
    const bool was = capturing();
    m_busy = true;
    notify(notice, nowMs);
    m_noticeUntil = INT64_MAX;
    if (!was && onCaptureChanged) onCaptureChanged(true);
}

void Ui::endBusy(const std::string &notice, int64_t nowMs) {
    m_busy = false;
    notify(notice, nowMs);
    if (onCaptureChanged) onCaptureChanged(capturing());
}

void Ui::installDone(int64_t nowMs) {
    if (m_installing < 0) return;
    App &app = m_apps[m_installing];
    m_installing = -1;
    app.installed = true;
    if (onAppsChanged) onAppsChanged();
    if (onCaptureChanged) onCaptureChanged(capturing());
    notify(std::string(app.name) + " installed", nowMs);
}

void Ui::notify(const std::string &text, int64_t nowMs) {
    m_notice = text.substr(0, Overlay::kCols);
    m_noticeUntil = nowMs + kNoticeMs;
    redraw();
}

bool Ui::poll(int64_t nowMs) {
    if (m_notice.empty() || nowMs < m_noticeUntil) return false;
    m_notice.clear();
    redraw();
    return true;
}

void Ui::redraw() {
    Overlay o;
    std::snprintf(o.notice, sizeof o.notice, "%s", m_notice.c_str());
    const std::string rule(Overlay::kCols, '-');
    if (m_panel == Panel::Store) {
        o.panel = true;
        const std::string title = "App store";
        const std::string count = std::to_string(m_selected + 1) + "/" + std::to_string(m_apps.size());
        put(o, 0, title + std::string(Overlay::kCols - title.size() - count.size(), ' ') + count);
        put(o, 1, rule);
        for (int r = 0; r < kListRows && m_top + r < int(m_apps.size()); ++r) {
            const App &a = m_apps[m_top + r];
            char line[Overlay::kCols + 1];
            std::snprintf(line, sizeof line, "%c%-8.*s %.*s", a.installed ? '*' : ' ',
                          int(std::min<size_t>(a.name.size(), 8)), a.name.data(),
                          int(a.description.size()), a.description.data());
            put(o, kListTop + r, line);
        }
        o.highlight = kListTop + (m_selected - m_top);
        put(o, Overlay::kRows - 2, rule);
        put(o, Overlay::kRows - 1, "Enter: install  *: installed  Esc");
    } else if (m_panel == Panel::Profiles) {
        o.panel = true;
        put(o, 0, "Profiles");
        put(o, 1, rule);
        for (int r = 0; r < int(m_profiles.size()) && r < kListRows; ++r)
            put(o, kListTop + r, " Profile " + std::to_string(m_profiles[r]) +
                                 (m_profiles[r] == m_profile ? "   (this one)" : ""));
        o.highlight = kListTop + m_profileSel;
        put(o, Overlay::kRows - 2, rule);
        put(o, Overlay::kRows - 1, "Enter: switch  N: new  Del: delete");
    } else if (m_panel == Panel::Settings) {
        o.panel = true;
        put(o, 0, "Settings");
        put(o, 1, rule);
        for (int i = 0; i < kFields; ++i) {
            const std::string &v = *const_cast<Ui *>(this)->field(i);
            std::string shown = kFieldSecret[i] ? std::string(std::min<size_t>(v.size(), 12), '*') : v;
            if (shown.empty()) shown = "-";
            char line[Overlay::kCols + 1];
            std::snprintf(line, sizeof line, " %-15s %.21s", kFieldNames[i], shown.c_str());
            put(o, kListTop + i, line);
        }
        o.highlight = kListTop + m_field;
        if (m_editing) {
            // The tail of what's typed, and a block for the cursor.
            const std::string tail = m_edit.size() > 34 ? m_edit.substr(m_edit.size() - 34) : m_edit;
            put(o, kListTop + kFields + 1, std::string(kFieldNames[m_field]) + ":");
            put(o, kListTop + kFields + 2, "> " + tail + "\x7f");
        }
        // Up to two lines of status.
        const size_t nl = m_status.find('\n');
        put(o, Overlay::kRows - 5, m_status.substr(0, nl));
        if (nl != std::string::npos) put(o, Overlay::kRows - 4, m_status.substr(nl + 1));
        put(o, Overlay::kRows - 2, rule);
        put(o, Overlay::kRows - 1, m_editing ? "Enter: save  Esc: cancel  Del"
                                             : "Enter: edit  Esc: close");
    } else if (m_panel == Panel::Help) {
        o.panel = true;
        const char *on = m_settings.smooth ? "on" : "off";
        put(o, 0, "Board shortcuts");
        put(o, 1, rule);
        put(o, 2, std::string("Shift+F1   Colour (") + Screen::themeName(m_settings.theme) + ")");
        put(o, 3, std::string("Shift+F2   Smoothing (") + on + ")");
        put(o, 4, "Shift+F3   App store");
        put(o, 5, "Shift+F4   Turbo (" + onOff(m_settings.turbo) + ")");
        put(o, 6, "Shift+F5   Next profile (" + std::to_string(m_profile) + ")");
        put(o, 7, "Shift+F6   Profiles");
        put(o, 8, "Shift+F7   Settings (Wi-Fi, Google)");
        put(o, 9, "Shift+F8   Sync with Google now");
        put(o, 10, "Shift+F10  This help");
        put(o, 12, "F1-F8 apps   F9 Menu   F10 Help");
        put(o, 13, "Ctrl alone: Menu   Alt alone: diamond");
        put(o, 14, "Alt, Win or F11 with a key: Psion");
        put(o, 15, "Hold BOOT 3 s: forget paired keyboards");
        put(o, Overlay::kRows - 2, rule);
        put(o, Overlay::kRows - 1, "Any key closes");
    }
    m_overlay = o;
    ++m_version;
}
