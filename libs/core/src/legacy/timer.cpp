/**
 * @file timer.cpp
 * Timing subsystem.
 *
 * @note Under WIN32, uses Win32 multimedia timing routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007-2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <mmsystem.h>
#endif

#include "de/legacy/timer.h"
#include "de/legacy/concurrency.h"

#include "de/time.h"

static float ticksPerSecond = TICSPERSEC;
static double timeOffset = 0;
//static mutex_t timerMutex;         // To prevent Data races in the timer
//static QTime startedAt;
//static uint timerOffset = 0;

static de::Time startedAt;

//static const uint TIMER_WARP_INTERVAL = 12*60*60*1000;

void Timer_Shutdown(void)
{
#ifdef WIN32
    timeEndPeriod(1);
#endif
    //mutex_t m = timerMutex;
    //timerMutex = 0;
//    Sys_DestroyMutex(m);
}

void Timer_Init(void)
{
    //assert(timerMutex == 0);
    //timerMutex = Sys_CreateMutex("TIMER_MUTEX");
#ifdef WIN32
    timeBeginPeriod(1);
#endif
    //startedAt.start();
    de::Time::updateCurrentHighPerformanceTime();
    startedAt = de::Time::currentHighPerformanceTime();
    //timerOffset = 0;
}

unsigned int Timer_RealMilliseconds(void)
{
#if 0
    static dd_bool first = true;
    static unsigned int start;
#ifdef WIN32
    DWORD return_time, now;
#else
    uint return_time, now;
#endif

    Sys_Lock(timerMutex);

#ifdef WIN32
    now = timeGetTime();
#else
    now = uint(startedAt.elapsed());
    if (now > TIMER_WARP_INTERVAL)
    {
        now += timerOffset;

        // QTime will wrap around every 24 hours; we'll wrap it manually before that.
        timerOffset += TIMER_WARP_INTERVAL;
        startedAt = startedAt.addMSecs(TIMER_WARP_INTERVAL);
    }
    else
    {
        now += timerOffset;
    }
#endif

    if (first)
    {
        first = false;
        start = now;

        Sys_Unlock(timerMutex);
        return 0;
    }

    // Wrapped around? (Every 50 days...)
    if (now < start)
    {
        return_time = 0xffffffff - start + now + 1;

        Sys_Unlock(timerMutex);
        return return_time;
    }

    return_time = now - start;

    Sys_Unlock(timerMutex);
    return return_time;
#endif

    de::Time::updateCurrentHighPerformanceTime();
    const de::TimeSpan delta = de::Time::currentHighPerformanceTime() - startedAt;
    return (unsigned int) delta.asMilliSeconds();
}

double Timer_Seconds(void)
{
    return (double) ((Timer_RealMilliseconds() / 1000.0) * (ticksPerSecond / 35)) + timeOffset;
}

double Timer_RealSeconds(void)
{
    return (double) (Timer_RealMilliseconds() / 1000.0);
}

double Timer_Ticksf(void)
{
    return Timer_Seconds() * 35;
}

int Timer_Ticks(void)
{
    return (int) Timer_Ticksf();
}

void Timer_SetTicksPerSecond(float newTics)
{
    double nowTime = Timer_RealMilliseconds() / 1000.0;

    if (newTics <= 0)
        newTics = TICSPERSEC;

    // Update the time offset so that after the change time will
    // continue from the same value.
    timeOffset += nowTime * (ticksPerSecond - newTics) / 35;

    ticksPerSecond = newTics;
}

float Timer_TicksPerSecond(void)
{
    return ticksPerSecond;
}
