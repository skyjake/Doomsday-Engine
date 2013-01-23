/** @file clock.cpp Time source.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Clock"

namespace de {

Clock *Clock::_appClock = 0;

Clock::Clock()
{}

Clock::~Clock()
{}

void Clock::setTime(Time const &currentTime)
{
    bool changed = (_time != currentTime);

    _time = currentTime;

    if(changed)
    {
        emit timeChanged();
    }
}

void Clock::advanceTime(TimeDelta const &span)
{
    setTime(_time + span);
}

TimeDelta Clock::elapsed() const
{
    return _time - _startedAt;
}

Time const &Clock::time() const
{
    return _time;
}

void Clock::setAppClock(Clock *c)
{
    _appClock = c;
}

Clock &Clock::appClock()
{
    DENG2_ASSERT(_appClock != 0);
    return *_appClock;
}

} // namespace de
