/** @file
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/*
 * m_profiler.h: Utility Macros for Profiling
 */

#ifndef __DOOMSDAY_MISC_PROFILER_H__
#define __DOOMSDAY_MISC_PROFILER_H__

#include "dd_types.h"
#include <de/legacy/timer.h>

/*
 * This header defines some handy macros for profiling.
 * Define DD_PROFILE to active.
 */

typedef struct profiler_s {
    uint            totalTime;
    uint            startTime;
    uint            startCount;
} profiler_t;

#define BEGIN_PROF_TIMERS() enum {

#ifdef DD_PROFILE                  // Profiling is enabled.
# define END_PROF_TIMERS()  ,NUM_PROFS }; static profiler_t profiler_[NUM_PROFS];
# define BEGIN_PROF(x)      (profiler_[x].startCount++, profiler_[x].startTime = Timer_RealMilliseconds())
# define END_PROF(x)        (profiler_[x].totalTime += Timer_RealMilliseconds() - profiler_[x].startTime)
# define PRINT_PROF(x)      App_Log(DE2_LOG_DEV, "[%f ms] " #x ": %i ms (%i starts)", \
                                profiler_[x].startCount? profiler_[x].totalTime / \
                                (float) profiler_[x].startCount : 0, \
                                profiler_[x].totalTime, profiler_[x].startCount)
#else                           // Profiling is disabled.
# define END_PROF_TIMERS()  ,NUM_PROFS };
# define BEGIN_PROF(x)
# define END_PROF(x)
# define PRINT_PROF(x)
#endif                          // DD_PROFILE

#endif
