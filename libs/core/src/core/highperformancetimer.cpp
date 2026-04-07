/** @file highperformancetimer.cpp  Timer for performance-critical use.
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

#include "de/highperformancetimer.h"
#include "de/guard.h"
#include "de/lockable.h"
#include "de/elapsedtimer.h"

#include <chrono>

namespace de {

DE_PIMPL_NOREF(HighPerformanceTimer)
{
    Time::TimePoint origin = std::chrono::system_clock::now();
    ElapsedTimer timer;

    Impl()
    {
        timer.start();
    }

    duint64 milliSeconds()
    {
        return duint64(timer.elapsedSeconds() * 1000L);
    }
};

HighPerformanceTimer::HighPerformanceTimer() : d(new Impl)
{}

TimeSpan HighPerformanceTimer::elapsed() const
{
    return TimeSpan::fromMilliSeconds(d->milliSeconds());
}

Time HighPerformanceTimer::startedAt() const
{
    return d->origin;
}

} // namespace de
