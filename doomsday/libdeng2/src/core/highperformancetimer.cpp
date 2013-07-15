/** @file highperformancetimer.cpp  Timer for performance-critical use.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/HighPerformanceTimer"

#include <QTime>

namespace de {

static duint32 const WARP_INTERVAL = 12 * 60 * 60 * 1000;

DENG2_PIMPL_NOREF(HighPerformanceTimer)
{
    QTime startedAt;
    duint64 timerOffset; /// Range extension. QTime only provides a 24h range.

    Instance() : timerOffset(0)
    {
        startedAt.start();
    }

    duint64 milliSeconds()
    {
        int const elapsed = startedAt.elapsed();
        duint64 now = duint64(elapsed) + timerOffset;

        if(duint64(elapsed) > WARP_INTERVAL)
        {
            // QTime will wrap around every 24 hours; we'll wrap it manually before that.
            timerOffset += WARP_INTERVAL;
            startedAt = startedAt.addMSecs(WARP_INTERVAL);
        }

        return now;
    }
};

HighPerformanceTimer::HighPerformanceTimer() : d(new Instance)
{}

TimeDelta HighPerformanceTimer::elapsed() const
{
    return TimeDelta(d->milliSeconds() / 1000.0);
}

} // namespace de
