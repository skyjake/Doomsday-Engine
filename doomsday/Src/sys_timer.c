
//**************************************************************************
//**
//** SYS_TIMER.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

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

static boolean	timerInstalled = false;

// CODE --------------------------------------------------------------------

//==========================================================================
// Sys_ShutdownTimer
//==========================================================================
void Sys_ShutdownTimer(void)
{
	timerInstalled = false;
	timeEndPeriod(1);
}

//==========================================================================
// Sys_InitTimer
//==========================================================================
void Sys_InitTimer(void)
{
	Con_Message("Sys_InitTimer.\n");
	timeBeginPeriod(1);
}

//===========================================================================
// Sys_GetRealTime
//	Returns the time in milliseconds.
//===========================================================================
unsigned int Sys_GetRealTime (void)
{
	static boolean first = true;
	static DWORD start;
	DWORD now = timeGetTime();

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

//===========================================================================
// Sys_GetSeconds
//	Returns the timer value in seconds.
//===========================================================================
double Sys_GetSeconds (void)
{
	return (double) Sys_GetRealTime() / 1000.0;
}

//===========================================================================
// Sys_GetTimef
//	Returns time in 35 Hz floating point tics.
//===========================================================================
double Sys_GetTimef(void)
{
	return (Sys_GetRealTime() / 1000.0 * ticsPerSecond);
}

//==========================================================================
// Sys_GetTime
//	Returns time in 35 Hz tics.
//==========================================================================
int Sys_GetTime (void)
{
	return (int) Sys_GetTimef();
}

//==========================================================================
// Sys_TicksPerSecond
//	Set the number of game tics per second.
//==========================================================================
void Sys_TicksPerSecond(float num)
{
	ticsPerSecond = num;
}