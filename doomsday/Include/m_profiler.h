//===========================================================================
// M_PROFILER.H
//===========================================================================
#ifndef __DOOMSDAY_MISC_PROFILER_H__
#define __DOOMSDAY_MISC_PROFILER_H__

#include "dd_types.h"
#include "sys_timer.h"

/*
 * This header defines some handy macros for profiling. 
 * Define DD_PROFILE to active.
 */

typedef struct profiler_s {
	uint totalTime;
	uint startTime;
	uint startCount;
} profiler_t;

#define BEGIN_PROF_TIMERS()	enum {

#ifdef DD_PROFILE // Profiling is enabled.
# define END_PROF_TIMERS()	,NUM_PROFS }; static profiler_t profiler_[NUM_PROFS];
# define BEGIN_PROF(x)		(profiler_[x].startCount++, profiler_[x].startTime = Sys_GetRealTime())
# define END_PROF(x)		(profiler_[x].totalTime += Sys_GetRealTime() - profiler_[x].startTime)
# define PRINT_PROF(x)		Con_Message(#x ": %i ms (%i starts)\n", profiler[x].totalTime, profiler[x].startCount) 
#else // Profiling is disabled.
# define END_PROF_TIMERS()	,NUM_PROFS };
# define BEGIN_PROF(x)		
# define END_PROF(x)		
# define PRINT_PROF(x)		
#endif // DD_PROFILE

#endif 