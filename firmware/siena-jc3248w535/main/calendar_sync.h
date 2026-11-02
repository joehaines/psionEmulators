// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Google calendar events into the Siena's Agenda. The calendar comes as
// iCalendar text (its secret iCal address); the events in a window of days
// are typed into Agenda as the user would: Jump to date, New entry, the
// text, then Change entry details for the time.
//
// Recurring events are expanded for the usual rules (DAILY, WEEKLY with
// BYDAY, MONTHLY by day of the month, YEARLY; INTERVAL, COUNT, UNTIL,
// EXDATE). Times are converted to the local time the process's TZ gives
// (set it with setenv/tzset before parsing).
//
// Portable: no ESP-IDF dependency.

#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

class KeyPlayer;

namespace calendar_sync {

struct Event {
    std::string uid;       // with the start, identifies an occurrence
    std::string summary;
    time_t start = 0;      // UTC
    time_t end = 0;
    bool allDay = false;
    // What sync remembers it by: the UID and its start.
    std::string key() const;
};

// The events (occurrences of recurring ones) starting in [from, to).
std::vector<Event> parseIcs(const std::string &ics, time_t from, time_t to);

// Queue the keys that add `e` to Agenda (UK date order, 12-hour times),
// from wherever the Siena is. The keyboard's own pauses wait for the
// Siena to finish each step.
void queueAgendaEntry(KeyPlayer &player, const Event &e);

} // namespace calendar_sync
