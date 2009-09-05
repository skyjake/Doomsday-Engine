/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Time"
#include "de/Date"
#include "de/Writer"
#include "de/Reader"
#include "../sdl.h"

#include <cmath>
#include <sstream>
#include <iomanip>

#ifdef UNIX
#   include <sys/time.h>
#endif

#ifdef WIN32
#   include <sys/timeb.h>
#endif

using namespace de;

duint64 Time::Delta::asMilliSeconds() const
{
    return duint64(_seconds * 1000);
}

Time::Delta Time::Delta::operator + (const ddouble& d) const
{
    return _seconds + d;
}

Time::Delta Time::Delta::operator - (const ddouble& d) const
{
    return _seconds - d;
}

void Time::Delta::sleep() const
{
    SDL_Delay(duint32(asMilliSeconds()));
}

Time::Time() 
{
#ifdef UNIX
    struct timeval tv;
    gettimeofday(&tv, 0);
    _time = tv.tv_sec;
    _micro = tv.tv_usec;
#endif

#ifdef WIN32
    struct __timeb64 tb;
    _ftime64(&tb);
    _time = tb.time;
    _micro = tb.millitm * 1000;
#endif
}

bool Time::operator < (const Time& t) const
{
    if(_time > t._time)
    {
        return false;
    }
    return _time < t._time || _micro < t._micro;
}

bool Time::operator == (const Time& t) const
{
    return _time == t._time && _micro == t._micro;
}

Time Time::operator + (const Delta& delta) const
{
    Time result = *this;
    result += delta;
    return result;
}

Time& Time::operator += (const Delta& delta)
{
    ddouble amount = std::fabs(delta);
    ddouble fullSeconds = std::floor(amount);
    ddouble fraction = amount - fullSeconds;
    if(delta > 0.0)
    {
        _time += time_t(fullSeconds);
        _micro += dint(fraction * 1.0e6);
        if(_micro > 1000000)
        {
            _micro -= 1000000;
            _time += 1;
        }
    }
    else
    {
        _time -= time_t(fullSeconds);
        _micro -= dint(fraction * 1.0e6);
        if(_micro < 0)
        {
            _micro += 1000000;
            _time -= 1;
        }
    }
    return *this;
}

Time::Delta Time::operator - (const Time& earlierTime) const
{
    ddouble seconds = std::difftime(_time, earlierTime._time);
    // The fraction.
    seconds += (_micro - earlierTime._micro) / 1.0e6;
    return seconds;
}

String Time::asText() const
{
    std::ostringstream os;
    os << _time << "." << std::setw(6) << std::setfill('0') << _micro;
    return os.str();
}

Date Time::asDate() const
{
    return Date(*this);
}

void Time::operator >> (Writer& to) const
{
    to << duint64(_time) << _micro;
}

void Time::operator << (Reader& from)
{
    duint64 t;
    from >> t;
    _time = (time_t) t;
    from >> _micro;
}

std::ostream& de::operator << (std::ostream& os, const Time& t)
{
    os << t.asText();
    return os;
}
