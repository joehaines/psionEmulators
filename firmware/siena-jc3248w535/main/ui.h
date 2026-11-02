// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The board's own functions, laid over the Siena and reached with Shift
// and a function key, from the Bluetooth keyboard or the USB terminal:
//
//   Shift+F1   colour theme: the next one
//   Shift+F2   edge smoothing on / off
//   Shift+F3   the app store: pick an app, Enter installs it, Esc closes
//   Shift+F4   turbo on / off: the Siena runs ahead of real time while busy
//   Shift+F5   the next profile (each its own Siena, kept on the SD card)
//   Shift+F6   the profiles: switch, add a new one, delete
//   Shift+F7   settings: Wi-Fi, the Gmail account, the Google calendar
//   Shift+F8   sync with Google now
//   Shift+F10  these shortcuts
//
// Both keyboard paths offer every key here first (as the Siena key it
// would be, with Siena modifiers); what this takes never reaches the
// Siena. While a panel is open it takes every key.
//
// Portable: no ESP-IDF dependency.

#pragma once

#include "screen.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class Ui {
public:
    struct Settings {
        int  theme = 0;
        bool smooth = false;
        bool turbo = false;
    };
    // Views of text that outlives the Ui (the store's index, in flash).
    // The board's links to the outside world, set in the settings panel.
    struct Account {
        std::string wifiName, wifiPassword;
        std::string gmailAddress, gmailAppPassword;   // an app password, not the account's
        std::string calendarUrl;                      // the calendar's secret iCal address
    };
    struct App {
        std::string_view name, description;
        bool installed = false;   // installed from the store (as far as it knows)
    };

    void setSettings(const Settings &s) { m_settings = s; }
    void setAccount(const Account &a) { m_account = a; }
    const Account &account() const { return m_account; }
    // A line of status for the settings panel (Wi-Fi, the last sync).
    void setStatus(const std::string &status);
    const Settings &settings() const { return m_settings; }
    // The apps the store offers; none hides it ("no app store").
    void setApps(std::vector<App> apps) { m_apps = std::move(apps); }
    // Mark the apps installed in the Siena now running (names, newline
    // separated), after switching profile.
    void setInstalled(const std::string &names);
    // The profiles there are, by number, and the one in use; none when
    // there's no card to keep them on.
    void setProfiles(std::vector<int> profiles, int current);

    // A key from either keyboard path, at wall-clock time nowMs. True if
    // the board took it.
    bool onKey(int key, uint8_t mods, int64_t nowMs);
    // A typed character, offered first: true if the board took it (a
    // settings field is being edited).
    bool onChar(char ch, int64_t nowMs);
    const std::vector<App> &apps() const { return m_apps; }
    // True while a panel is open, or an install is being typed, and every
    // key is taken.
    bool capturing() const { return m_panel != Panel::None || m_installing >= 0 || m_busy; }
    // The board is typing something into the Siena (calendar events, say):
    // say so, and take every key, until done.
    void beginBusy(const std::string &notice, int64_t nowMs);
    void endBusy(const std::string &notice, int64_t nowMs);
    bool busy() const { return m_busy; }
    bool installing() const { return m_installing >= 0; }
    // The install onInstall started has been typed in full.
    void installDone(int64_t nowMs);
    // Expire the notice. True when the overlay changed.
    bool poll(int64_t nowMs);
    // Show a notice for a couple of seconds.
    void notify(const std::string &text, int64_t nowMs);

    // The overlay as it stands; overlayVersion() moves on every change.
    const Overlay &overlay() const { return m_overlay; }
    unsigned overlayVersion() const { return m_version; }

    std::function<void(const Settings &)> onSettingsChanged;
    std::function<void(bool capturing)> onCaptureChanged;
    // Install the app at this index in the store's list.
    std::function<void(int app)> onInstall;
    // An app's installed mark changed.
    std::function<void()> onAppsChanged;
    // Switch to this profile; make a new one (returning its number, or -1)
    // and switch to it; delete one (not the current).
    std::function<void(int profile)> onSwitchProfile;
    std::function<int()> onNewProfile;
    std::function<bool(int profile)> onDeleteProfile;
    // A settings field changed; sync with Google now.
    std::function<void(const Account &)> onAccountChanged;
    std::function<void()> onSyncNow;

private:
    enum class Panel { None, Store, Help, Profiles, Settings };

    void open(Panel p);
    void storeKey(int key, uint8_t mods, int64_t nowMs);
    void profilesKey(int key, int64_t nowMs);
    void settingsKey(int key, int64_t nowMs);
    std::string *field(int i);
    void redraw();

    Settings m_settings;
    std::vector<App> m_apps;
    Panel    m_panel = Panel::None;
    int      m_selected = 0, m_top = 0;
    int      m_installing = -1;     // the app whose install is being typed
    bool     m_busy = false;
    std::vector<int> m_profiles;
    int      m_profile = 1;         // in use
    int      m_profileSel = 0;      // the profiles panel's cursor
    int      m_deleteArmed = -1;    // a profile Del was pressed on once
    Account  m_account;
    std::string m_status;
    int      m_field = 0;           // the settings panel's cursor
    bool     m_editing = false;
    std::string m_edit;             // the field being edited
    std::string m_notice;
    int64_t  m_noticeUntil = 0;
    Overlay  m_overlay;
    unsigned m_version = 0;
};
