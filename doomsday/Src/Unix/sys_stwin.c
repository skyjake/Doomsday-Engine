
//**************************************************************************
//**
//** SYS_STWIN.C
//**
//** Startup message and progress bar window.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"
#include "sys_stwin.h"

// MACROS ------------------------------------------------------------------

// Color values for graphical display.
#define CREF_BACKGROUND		0
#define CREF_PROGRESS		0xc08080
#define CREF_TEXT			0xffc0c0

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean swActive;
static int barPos, barMax;

// CODE --------------------------------------------------------------------

//===========================================================================
// SW_IsActive
//===========================================================================
int SW_IsActive(void)
{
	return swActive;
}

//===========================================================================
// SW_Printf
//===========================================================================
void SW_Printf(const char *format, ...)
{
	/*
	va_list args;

	if(!SW_IsActive()) return;

	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	*/
}

//===========================================================================
// SW_Init
//===========================================================================
void SW_Init(void)
{
	if(swActive) return; // Already initialized.
	Con_Message("SW_Init: Startup message window opened.\n");
	swActive = true;
}

//===========================================================================
// SW_Shutdown
//===========================================================================
void SW_Shutdown(void)
{
	if(!swActive) return;	// Not initialized.
	swActive = false;
}

//===========================================================================
// SW_DrawBar
//===========================================================================
void SW_DrawBar(void)
{
}

//===========================================================================
// SW_SetBarPos
//===========================================================================
void SW_SetBarPos(int pos)
{
	barPos = pos;
	SW_DrawBar();
}

//===========================================================================
// SW_SetBarMax
//===========================================================================
void SW_SetBarMax(int max)
{
	if(!SW_IsActive()) return;
	barMax = max;
}

