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

#include "de/Date"

#include <chrono>
#include <ctime>
#include <c_plus/time.h>

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
    }
};

Date::Date() : d(new Impl)
{}

Date::Date(const Time &time) : d(new Impl(time))
{}

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

int Date::hours() const
{
    return d->date.hour;
}

int Date::minutes() const
{
    return d->date.minute;
}

int Date::seconds() const
{
    return d->date.second;
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

Date Date::fromText(String const &text)
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

std::ostream &operator << (std::ostream &os, const Date &d)
{
    os << d.asText().c_str();
    return os;
}

} // namespace de
