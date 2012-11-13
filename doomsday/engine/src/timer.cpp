/**
 * @file timer.cpp
 * Timing subsystem. @ingroup system
 *
 * @note Under WIN32, uses Win32 multimedia timing routines.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#include <QTime>

#include "de_platform.h"

#ifdef WIN32
#   include <mmsystem.h>
#endif

/*
#ifdef UNIX
#   include <SDL.h>
#endif
*/

#include "de_base.h"
#include "de_console.h"
#include <de/concurrency.h>

float ticsPerSecond = TICSPERSEC;

static double timeOffset = 0;
static mutex_t timerMutex;         // To prevent Data races in the timer
static QTime startedAt;
static uint timerOffset = 0;

const static uint TIMER_WARP_INTERVAL = 12*60*60*1000;

void Sys_ShutdownTimer(void)
{
#ifdef WIN32
    timeEndPeriod(1);
#endif
    mutex_t m = timerMutex;
    timerMutex = 0;
    Sys_DestroyMutex(m);
}

void Sys_InitTimer(void)
{
    assert(timerMutex == 0);
    timerMutex = Sys_CreateMutex("TIMER_MUTEX");
#ifdef WIN32
    timeBeginPeriod(1);
#endif
    startedAt.start();
    timerOffset = 0;
}

/**
 * @return The time in milliseconds.
 */
unsigned int Sys_GetRealTime(void)
{
    static boolean first = true;
    static DWORD start;
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
    if(now > TIMER_WARP_INTERVAL)
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

    if(first)
    {
        first = false;
        start = now;

        Sys_Unlock(timerMutex);
        return 0;
    }

    // Wrapped around? (Every 50 days...)
    if(now < start)
    {
        return_time = 0xffffffff - start + now + 1;

        Sys_Unlock(timerMutex);
        return return_time;
    }

    return_time = now - start;

    Sys_Unlock(timerMutex);
    return return_time;
}

/**
 * @return The timer value in seconds. Affected by the ticsPerSecond modifier.
 */
double Sys_GetSeconds(void)
{
    return (double) ((Sys_GetRealTime() / 1000.0) * (ticsPerSecond / 35)) +
        timeOffset;
}

/**
 * @return The real timer value in seconds.
 */
double Sys_GetRealSeconds(void)
{
    return (double) (Sys_GetRealTime() / 1000.0);
}

/**
 * @return The time in 35 Hz floating point tics.
 */
double Sys_GetTimef(void)
{
    return Sys_GetSeconds() * 35;
}

/**
 * @return The time in 35 Hz tics.
 */
int Sys_GetTime(void)
{
    return (int) Sys_GetTimef();
}

/**
 * @return Set the number of game tics per second.
 */
void Sys_TicksPerSecond(float newTics)
{
    double  nowTime = Sys_GetRealTime() / 1000.0;

    if(newTics <= 0)
        newTics = TICSPERSEC;

    // Update the time offset so that after the change time will
    // continue from the same value.
    timeOffset += nowTime * (ticsPerSecond - newTics) / 35;

    ticsPerSecond = newTics;
}
