/** @file clock.cpp Time source.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/clock.h"
#include <atomic>

namespace de {

DE_PIMPL_NOREF(Clock)
{
    Time startedAt;
    Time time;
    std::atomic_uint tickCount { 0 };

    DE_PIMPL_AUDIENCE(TimeChange)
};

DE_AUDIENCE_METHOD(Clock, TimeChange)

Clock *Clock::_appClock = 0;

Clock::Clock() : d(new Impl)
{}

Clock::~Clock()
{}

void Clock::setTime(const Time &currentTime)
{
    bool changed = (d->time != currentTime);

    d->time = currentTime;

    if (changed)
    {
        d->tickCount++;

        DE_FOR_OBSERVERS(i, audienceForPriorityTimeChange)
        {
            i->timeChanged(*this);
        }
        DE_NOTIFY(TimeChange, i) i->timeChanged(*this);
    }
}

void Clock::advanceTime(TimeSpan span)
{
    setTime(d->time + span);
}

TimeSpan Clock::elapsed() const
{
    return d->time - d->startedAt;
}

const Time &Clock::time() const
{
    return d->time;
}

duint32 Clock::tickCount() const
{
    return d->tickCount;
}

void Clock::setAppClock(Clock *c)
{
    _appClock = c;
}

Clock &Clock::get()
{
    DE_ASSERT(_appClock != 0);
    return *_appClock;
}

const Time &Clock::appTime()
{
    return get().time();
}

} // namespace de
