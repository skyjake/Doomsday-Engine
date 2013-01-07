/**
 * @file timer.h
 * Timing subsystem. @ingroup system
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_SYSTEM_TIMER_H
#define LIBDENG_SYSTEM_TIMER_H

#include "types.h"

#define TICRATE             35 // Number of tics / second.
#define TICSPERSEC          35
#define SECONDSPERTIC       (1.0f/TICSPERSEC)

#ifdef __cplusplus
extern "C" {
#endif

void Timer_Init(void);

void Timer_Shutdown(void);

/**
 * Current time measured in game ticks.
 *
 * @return Game ticks.
 */
DENG_PUBLIC int Timer_Ticks(void);

/**
 * Current time measured in game ticks.
 *
 * @return Floating-point game ticks.
 */
DENG_PUBLIC double Timer_Ticksf(void);

/**
 * Current time in seconds. Affected by the ticksPerSecond modifier.
 *
 * @return Seconds.
 */
DENG_PUBLIC double Timer_Seconds(void);

/**
 * Elapsed time since initialization. Not affected by the ticksPerSecond modifier.
 *
 * @return Milliseconds.
 */
DENG_PUBLIC uint Timer_RealMilliseconds(void);

/**
 * Elapsed time since initialization. Not affected by the ticksPerSecond modifier.
 *
 * @return Seconds.
 */
DENG_PUBLIC double Timer_RealSeconds(void);

/**
 * Set the number of game ticks per second.
 *
 * @param num  Number of ticks per second (default: 35).
 */
DENG_PUBLIC void Timer_SetTicksPerSecond(float num);

/**
 * Returns the current number of ticks per second (default: 35).
 */
DENG_PUBLIC float Timer_TicksPerSecond(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_SYSTEM_TIMER_H
