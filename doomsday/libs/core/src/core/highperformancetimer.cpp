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

#include "de/HighPerformanceTimer"
#include "de/Guard"
#include "de/Lockable"
#include "de/ElapsedTimer"

#include <chrono>

namespace de {

namespace chr = std::chrono;

DE_PIMPL_NOREF(HighPerformanceTimer)
{
    chr::system_clock::time_point origin = chr::system_clock::now();
    ElapsedTimer timer;

//    QDateTime origin;
//    QElapsedTimer startedAt;

    Impl()
    {
        timer.start();
//        origin = QDateTime::currentDateTime();
//        startedAt.start();
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
    return TimeSpan(d->milliSeconds() / 1000.0);
}

Time HighPerformanceTimer::startedAt() const
{
    return d->origin;
}

} // namespace de
