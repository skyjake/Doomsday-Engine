//===========================================================================
// SYS_TIMER.H
//===========================================================================
#ifndef __DOOMSDAY_SYSTEM_TIMER_H__
#define __DOOMSDAY_SYSTEM_TIMER_H__

#include "dd_types.h"

void	Sys_InitTimer(void);
void	Sys_ShutdownTimer(void);
int		Sys_GetTime(void);
double	Sys_GetTimef(void);
double	Sys_GetSeconds(void);
uint	Sys_GetRealTime(void);
void	Sys_TicksPerSecond(float num);

#endif