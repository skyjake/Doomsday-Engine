/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_timer.c: Timing Subsystem
 *
 * Uses the SDL timing routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <SDL.h>

#include "de_base.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float ticsPerSecond = TICSPERSEC;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Sys_ShutdownTimer(void)
{
}

void Sys_InitTimer(void)
{
	/*Con_Message("Sys_InitTimer.\n");*/
}

/*
 * Returns the time in milliseconds.
 */
unsigned int Sys_GetRealTime (void)
{
	static boolean first = true;
	static Uint32 start = 0;
	Uint32 now = SDL_GetTicks();

	if(first)
	{
		first = false;
		start = now;
		return 0;
	}
	
	// Wrapped around? (Every 50 days...)
	if(now < start) return 0xffffffff - start + now + 1;

	return now - start;
}

/*
 * Returns the timer value in seconds.
 */
double Sys_GetSeconds (void)
{
	return (double) Sys_GetRealTime() / 1000.0;
}

/*
 * Returns time in 35 Hz floating point tics.
 */
double Sys_GetTimef(void)
{
	return (Sys_GetRealTime() / 1000.0 * ticsPerSecond);
}

/*
 * Returns time in 35 Hz tics.
 */
int Sys_GetTime (void)
{
	return (int) Sys_GetTimef();
}

#if 0
/*
 * Set the number of game tics per second.
 */
void Sys_TicksPerSecond(float num)
{
	if(num <= 0) num = TICSPERSEC;
	ticsPerSecond = num;
}
#endif
