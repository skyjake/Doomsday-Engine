/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Date"

#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace de;

Date::Date()
{
    *this = Date(Time());
}

Date::Date(const Time& at) : microSeconds(at.micro_)
{
    struct tm t;

#ifdef UNIX
    localtime_r(&at.time_, &t);
#endif

#ifdef WIN32
    memcpy(&t, _localtime64(&at.time_), sizeof(t));
#endif

    seconds = t.tm_sec;
    minutes = t.tm_min;
    hours = t.tm_hour;
    month = t.tm_mon + 1;
    year = t.tm_year + 1900;
    weekDay = t.tm_wday;
    dayOfMonth = t.tm_mday;
    dayOfYear = t.tm_yday;
}

String Date::asText() const
{
    std::ostringstream os;
    os << *this;
    return os.str();
}

Time Date::asTime() const
{
    struct tm t;
    
    std::memset(&t, 0, sizeof(t));
    t.tm_sec = seconds;
    t.tm_min = minutes;
    t.tm_hour = hours;
    t.tm_mon = month - 1;
    t.tm_year = year - 1900;
    t.tm_mday = dayOfMonth;
    
#ifdef UNIX
    return mktime(&t);
#endif

#ifdef WIN32
    return _mktime64(&t);
#endif
}

std::ostream& de::operator << (std::ostream& os, const Date& d)
{
    using std::setfill;
    using std::setw;

    os << setfill('0') << 
        d.year << "-" << 
        setw(2) << d.month << "-" << 
        setw(2) << d.dayOfMonth << " " << 
        setw(2) << d.hours << ":" << 
        setw(2) << d.minutes << ":" << 
        setw(2) << d.seconds << "." << 
        setw(2) << d.microSeconds/10000;
    return os;
}
