// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Syncing with Google over Wi-Fi, in a task of its own: the time (SNTP),
// then the calendar. A sync fetches the calendar's iCal file, picks the
// events from yesterday to a month on, and hands the ones the current
// profile's Agenda hasn't had yet to `deliver` (the emulator task types
// them in). What has been handed over is remembered per profile on the
// SD card (CALSYNC.TXT beside the profile's slots), so each goes in once.
//
// It runs when asked (syncNow), once Wi-Fi first connects, and every 30
// minutes after.

#pragma once

#include "calendar_sync.h"

#include <functional>
#include <string>
#include <vector>

namespace sync_service {

struct Hooks {
    // New events for the Siena's Agenda.
    std::function<void(std::vector<calendar_sync::Event>)> deliver;
    // A line for the settings panel.
    std::function<void(const std::string &)> status;
};

void start(const Hooks &hooks);
// The calendar's iCal address (set from the settings).
void setCalendarUrl(const std::string &url);
void syncNow();
// The real time is known (SNTP has answered).
bool haveTime();

} // namespace sync_service
