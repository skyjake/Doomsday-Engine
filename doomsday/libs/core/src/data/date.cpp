/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/date.h"

#include <chrono>
#include <ctime>
#include <the_Foundation/time.h>

namespace de {

using namespace std::chrono;

DE_PIMPL_NOREF(Date)
{
    Time time;
    iDate date;

    Impl() { zap(date); }

    Impl(const Time &time)
        : time(time)
    {
        initSinceEpoch_Date(&date, time.toTime_t());
        date.nsecs = (time.millisecondsSinceEpoch() % 1000) * 1000000L;
    }
};

Date::Date() : d(new Impl)
{}

Date::Date(const Time &time) : d(new Impl(time))
{}

bool Date::isValid() const
{
    return d->time.isValid();
}

Date Date::fromJulianDayNumber(int julianDay) // static
{
    // https://en.wikipedia.org/wiki/Julian_day#Converting_Gregorian_calendar_date_to_Julian_Day_Number

    const int y = 4716;
    const int j = 1401;
    const int m = 2;
    const int n = 12;
    const int r = 4;
    const int p = 1461;
    const int v = 3;
    const int u = 5;
    const int s = 153;
    const int w = 2;
    const int B = 274277;
    const int C = -38;

    int f = julianDay + j + (((4 * julianDay + B) / 146097) * 3) / 4 + C;
    int e = r * f + v;
    int g = mod(e, p) / r;
    int h = u * g + w;
    int D = mod(h, s) / u + 1;
    int M = mod(h / s + m, n) + 1;
    int Y = (e / p) - y + (n + m - M) / n;

    return Time(Y, M, D, 0, 0, 0);
}

int Date::year() const
{
    return d->date.year;
}

int Date::month() const
{
    return d->date.month;
}

int Date::dayOfMonth() const
{
    return d->date.day;
}

int Date::dayOfYear() const
{
    return d->date.dayOfYear;
}

int Date::dayOfWeek() const
{
    return d->date.dayOfWeek; // 0 == Sunday
}

int Date::julianDayNumber() const
{
    // For reference, see
    // https://en.wikipedia.org/wiki/Julian_day#Converting_Gregorian_calendar_date_to_Julian_Day_Number

    const int Y = year();
    const int M = month();
    const int D = dayOfMonth();

    return (1461 * (Y + 4800 + (M - 14)/12))/4 +
            (367 * (M - 2 - 12 * ((M - 14)/12)))/12 -
            (3 * ((Y + 4900 + (M - 14)/12)/100))/4 + D - 32075;
}

int Date::hours() const
{
    return d->date.hour;
}

int Date::minutes() const
{
    return d->date.minute;
}

ddouble Date::seconds() const
{
    return d->date.second + double(d->date.nsecs) / 1.0e9;
}

int Date::daysTo(const Date &other) const
{
    return duration_cast<std::chrono::minutes>(other.d->time.toTimePoint() - d->time.toTimePoint())
        .count() / (60 * 24);
}

Time Date::asTime() const
{
    return d->time;
}

bool Date::isSameDay(const Date &other) const
{
    return year() == other.year() && dayOfYear() == other.dayOfYear();
}

Date Date::currentDate()
{
    return Time().asDate();
}

Date Date::fromText(const String &text)
{
    return Time::fromText(text, Time::HumanDate);
}

String Date::format(const char *format) const
{
    const time_t tm = d->time.toTime_t();
    char buf[256];
    strftime(buf, sizeof(buf) - 1, format, localtime(&tm));
    return buf;
}

std::ostream &operator<<(std::ostream &os, const Date &d)
{
    os << d.asText().c_str();
    return os;
}

} // namespace de
