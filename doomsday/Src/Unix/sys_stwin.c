/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * sys_stwin.c: Startup message and progress bar window
 */

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

#ifdef MACOSX
void PrintInStartupWindow(const char *message);
void CloseStartupWindow(void);
#endif

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
#ifdef MACOSX
    char buf[16384];
    va_list args;

	if(!SW_IsActive()) return;

    va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
    
    PrintInStartupWindow(buf);
#endif
}

//===========================================================================
// SW_Init
//===========================================================================
void SW_Init(void)
{
	if(swActive)
		return;					// Already initialized.
	Con_Message("SW_Init: Startup message window opened.\n");
	swActive = true;
}

//===========================================================================
// SW_Shutdown
//===========================================================================
void SW_Shutdown(void)
{
	if(!swActive)
		return;					// Not initialized.
	swActive = false;
    
#ifdef MACOSX
    CloseStartupWindow();
#endif
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
	if(!SW_IsActive())
		return;
	barMax = max;
}
