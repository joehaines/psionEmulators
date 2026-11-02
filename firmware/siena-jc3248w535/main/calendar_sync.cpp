// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "calendar_sync.h"

#include "emubase.h"
#include "key_player.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>

namespace calendar_sync {

namespace {

// Seconds since 1970 for a UTC date and time (timegm, which newlib lacks).
time_t utc(int y, int mon, int d, int h, int mi, int s) {
    y -= mon <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long days = long(era) * 146097 + doe - 719468;
    return time_t(days) * 86400 + h * 3600 + mi * 60 + s;
}

// Local wall-clock time to UTC, per TZ.
time_t local(int y, int mon, int d, int h, int mi, int s) {
    std::tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    t.tm_isdst = -1;
    return std::mktime(&t);
}

// An iCalendar DATE or DATE-TIME: "20261026", "20261026T093000" (local, or
// in a TZID, taken as local) or "20261026T093000Z" (UTC).
bool parseTime(const std::string &v, time_t &out, bool &dateOnly) {
    int y, mo, d, h = 0, mi = 0, s = 0;
    if (v.size() < 8 || std::sscanf(v.c_str(), "%4d%2d%2d", &y, &mo, &d) != 3) return false;
    dateOnly = v.size() < 15 || v[8] != 'T';
    if (!dateOnly && std::sscanf(v.c_str() + 9, "%2d%2d%2d", &h, &mi, &s) != 3) return false;
    out = (!dateOnly && v.back() == 'Z') ? utc(y, mo, d, h, mi, s) : local(y, mo, d, h, mi, s);
    return true;
}

// An iCalendar DURATION ("PT1H30M", "P1D").
long parseDuration(const std::string &v) {
    long total = 0, n = 0;
    const int sign = !v.empty() && v[0] == '-' ? -1 : 1;
    for (char c : v) {
        if (c >= '0' && c <= '9') { n = n * 10 + (c - '0'); continue; }
        switch (c) {
        case 'W': total += n * 604800; break;
        case 'D': total += n * 86400; break;
        case 'H': total += n * 3600; break;
        case 'M': total += n * 60; break;
        case 'S': total += n; break;
        }
        n = 0;
    }
    return sign * total;
}

std::string unescape(const std::string &v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == '\\' && i + 1 < v.size()) {
            const char c = v[++i];
            out += (c == 'n' || c == 'N') ? ' ' : c;
        } else {
            out += v[i];
        }
    }
    return out;
}

struct Raw {
    std::string uid, summary, rrule, status;
    time_t start = 0, end = 0;
    bool allDay = false, hasEnd = false;
    long duration = -1;
    std::vector<time_t> exdates;
    time_t recurrenceId = 0;
};

std::string ruleValue(const std::string &rule, const char *key) {
    const std::string k = std::string(key) + "=";
    size_t at = 0;
    while ((at = rule.find(k, at)) != std::string::npos) {
        if (at == 0 || rule[at - 1] == ';') {
            const size_t end = rule.find(';', at);
            return rule.substr(at + k.size(), end == std::string::npos ? std::string::npos
                                                                       : end - at - k.size());
        }
        at += k.size();
    }
    return "";
}

// The occurrences of `r` that start in [from, to).
void expand(const Raw &r, time_t from, time_t to, std::vector<time_t> &out) {
    if (r.rrule.empty()) {
        if (r.start >= from && r.start < to) out.push_back(r.start);
        return;
    }
    const std::string freq = ruleValue(r.rrule, "FREQ");
    const int interval = std::max(1, std::atoi(ruleValue(r.rrule, "INTERVAL").c_str()));
    const std::string countV = ruleValue(r.rrule, "COUNT");
    const int count = countV.empty() ? -1 : std::atoi(countV.c_str());
    time_t until = to;
    if (!ruleValue(r.rrule, "UNTIL").empty()) {
        bool d;
        if (parseTime(ruleValue(r.rrule, "UNTIL"), until, d) && d) until += 86399;
    }
    // BYDAY for weekly rules: the weekdays (0 = Sunday).
    std::vector<int> byDay;
    static const char *const kDays[7] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
    const std::string bd = ruleValue(r.rrule, "BYDAY");
    for (int i = 0; i < 7; ++i)
        if (bd.find(kDays[i]) != std::string::npos) byDay.push_back(i);

    std::tm base;
    localtime_r(&r.start, &base);
    int made = 0;
    auto take = [&](time_t t) {
        if (t < r.start) return true;
        if (t > until || (count >= 0 && made >= count)) return false;
        ++made;
        if (t >= from && t < to &&
            std::find(r.exdates.begin(), r.exdates.end(), t) == r.exdates.end())
            out.push_back(t);
        return true;
    };
    for (int step = 0; step < 5000; ++step) {
        std::tm t = base;
        t.tm_isdst = -1;
        if (freq == "DAILY") {
            t.tm_mday += step * interval;
        } else if (freq == "WEEKLY") {
            if (byDay.empty()) {
                t.tm_mday += step * 7 * interval;
            } else {
                // Each listed weekday of the week `step` weeks on.
                const int weekStart = base.tm_mday - base.tm_wday + step * 7 * interval;
                bool more = true;
                for (int wd : byDay) {
                    std::tm w = base;
                    w.tm_isdst = -1;
                    w.tm_mday = weekStart + wd;
                    const time_t at = std::mktime(&w);
                    if (!take(at)) { more = false; break; }
                }
                if (!more) break;
                std::tm w = base;
                w.tm_isdst = -1;
                w.tm_mday = weekStart;
                if (std::mktime(&w) > to) break;
                continue;
            }
        } else if (freq == "MONTHLY") {
            t.tm_mon += step * interval;
        } else if (freq == "YEARLY") {
            t.tm_year += step * interval;
        } else {
            take(r.start);
            break;
        }
        const time_t at = std::mktime(&t);
        // A month too short for the day (the 31st) is skipped, not moved.
        if ((freq == "MONTHLY" || freq == "YEARLY") && t.tm_mday != base.tm_mday) continue;
        if (!take(at) || at >= to) break;
    }
}

} // namespace

std::string Event::key() const {
    return uid + "@" + std::to_string(long(start));
}

std::vector<Event> parseIcs(const std::string &ics, time_t from, time_t to) {
    // Unfold: a line starting with a space or tab continues the last.
    std::vector<std::string> lines;
    size_t at = 0;
    while (at < ics.size()) {
        size_t end = ics.find('\n', at);
        if (end == std::string::npos) end = ics.size();
        std::string line = ics.substr(at, end - at);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t') && !lines.empty())
            lines.back() += line.substr(1);
        else
            lines.push_back(line);
        at = end + 1;
    }

    std::vector<Raw> raws;
    Raw cur;
    bool in = false;
    for (const std::string &line : lines) {
        if (line == "BEGIN:VEVENT") { cur = Raw(); in = true; continue; }
        if (line == "END:VEVENT") { if (in) raws.push_back(cur); in = false; continue; }
        if (!in) continue;
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string head = line.substr(0, colon), value = line.substr(colon + 1);
        const std::string name = head.substr(0, head.find(';'));
        bool dateOnly = false;
        time_t t;
        if (name == "UID") cur.uid = value;
        else if (name == "SUMMARY") cur.summary = unescape(value);
        else if (name == "STATUS") cur.status = value;
        else if (name == "RRULE") cur.rrule = value;
        else if (name == "DURATION") cur.duration = parseDuration(value);
        else if (name == "DTSTART" && parseTime(value, t, dateOnly)) { cur.start = t; cur.allDay = dateOnly; }
        else if (name == "DTEND" && parseTime(value, t, dateOnly)) { cur.end = t; cur.hasEnd = true; }
        else if (name == "RECURRENCE-ID" && parseTime(value, t, dateOnly)) cur.recurrenceId = t;
        else if (name == "EXDATE") {
            size_t p = 0;
            while (p <= value.size()) {
                const size_t comma = value.find(',', p);
                const std::string one = value.substr(p, comma == std::string::npos ? std::string::npos : comma - p);
                if (parseTime(one, t, dateOnly)) cur.exdates.push_back(t);
                if (comma == std::string::npos) break;
                p = comma + 1;
            }
        }
    }

    // A changed occurrence (RECURRENCE-ID) replaces the one the rule makes.
    std::map<std::string, std::vector<time_t>> moved;
    for (const Raw &r : raws)
        if (r.recurrenceId) moved[r.uid].push_back(r.recurrenceId);

    std::vector<Event> events;
    for (Raw r : raws) {
        if (r.status == "CANCELLED" || r.uid.empty()) continue;
        if (!r.recurrenceId) {
            auto m = moved.find(r.uid);
            if (m != moved.end()) r.exdates.insert(r.exdates.end(), m->second.begin(), m->second.end());
        }
        const long length = r.hasEnd ? long(r.end - r.start)
                          : r.duration >= 0 ? r.duration : (r.allDay ? 86400 : 0);
        std::vector<time_t> starts;
        expand(r, from, to, starts);
        for (time_t s : starts) {
            Event e;
            e.uid = r.uid;
            e.summary = r.summary.empty() ? "(no title)" : r.summary;
            e.start = s;
            e.end = s + length;
            e.allDay = r.allDay;
            events.push_back(e);
        }
    }
    std::sort(events.begin(), events.end(),
              [](const Event &a, const Event &b) { return a.start < b.start; });
    return events;
}

void queueAgendaEntry(KeyPlayer &p, const Event &e) {
    std::tm t;
    localtime_r(&e.start, &t);
    auto type = [&](const char *s) { for (; *s; ++s) p.typeChar(*s); };
    // Agenda, from anywhere: the System screen, its button, Enter.
    p.tap(EStdKeyEscape);
    p.waitIdle();
    p.tap(EStdKeyF1);
    p.waitIdle();
    p.tap(EStdKeyF4);
    p.waitIdle();
    p.tap(EStdKeyEnter);
    p.waitIdle();
    p.tap(EStdKeyEscape);
    p.waitIdle();
    // The day: Jump to date, typed DDMMYYYY.
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d%02d%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    p.tap('J', kModPsion);
    p.waitIdle();
    type(buf);
    p.tap(EStdKeyEnter);
    p.waitIdle();
    // The cursor to the top of the day (9 am): the new entry starts there,
    // so its time starts in the morning (see below).
    p.tap(EStdKeyUpArrow, kModPsion);
    p.waitIdle();
    // A new entry on it, its text, saved.
    p.tap('E', kModPsion);
    p.waitIdle();
    p.tap(EStdKeyEnter);
    p.waitIdle();
    std::string text;
    for (char c : e.summary)
        if (uint8_t(c) >= ' ' && uint8_t(c) < 0x7F && text.size() < 60) text += c;
    type(text.c_str());
    p.tap(EStdKeyEnter);
    p.waitIdle();
    // Its time: Change entry details.
    p.tap('D', kModPsion);
    p.waitIdle();
    if (e.allDay) {
        p.tap(EStdKeyLeftArrow);    // Timed entry: No
    } else {
        // Twelve-hour, and any letter flips am and pm: from the morning,
        // one flips it to the afternoon.
        const int h12 = t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12;
        std::snprintf(buf, sizeof buf, "%02d%02d%s", h12, t.tm_min, t.tm_hour < 12 ? "" : "p");
        p.tap(EStdKeyDownArrow);
        type(buf);
        const long minutes = std::min<long>(std::max<long>(0, long(e.end - e.start) / 60), 23 * 60 + 59);
        std::snprintf(buf, sizeof buf, "%02ld%02ld", minutes / 60, minutes % 60);
        p.tap(EStdKeyDownArrow);
        type(buf);
    }
    p.waitIdle();
    p.tap(EStdKeyEnter);
    p.waitIdle();
}

} // namespace calendar_sync
