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
 * con_bar.c: Console Progress Bar
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "sys_stwin.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int progress_active = false;
int progress_enabled = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char progress_title[64];
static int progress_max;
static int progress, progress_shown;

// CODE --------------------------------------------------------------------

//==========================================================================
// DD_InitProgress
//==========================================================================
void Con_InitProgress(const char *title, int full)
{
	if(isDedicated || !progress_enabled) return;
	
	// Init startup window progress bar.
	SW_SetBarMax(full);

	strcpy(progress_title, title);
	
	if(full >= 0)
	{
		progress_active = true;
		progress = progress_shown = 0;
		if(!full) full = 1;
		progress_max = full;
		Con_Progress(0, PBARF_INIT);
	}
}

//===========================================================================
// DD_HideProgress
//===========================================================================
void Con_HideProgress(void)
{
	progress_active = false;
	// Clear the startup window progress bar.
	SW_SetBarMax(0);
}

//==========================================================================
// DD_Progress
//	Draws a progress bar. Flags consists of one or more PBARF_* flags.
//==========================================================================
void Con_Progress(int count, int flags)
{
	int x, y, w, h, bor = 2, bar = 10;
	int maxWidth = 500;
	int mainBor = 5;
	int fonthgt;

	if(!progress_active 
		|| isDedicated 
		|| !progress_enabled) return;

	if(flags & PBARF_SET)
		progress = count;
	else
		progress += count;

	// Make sure it's in range.
	if(progress < 0) progress = 0;
	if(progress > progress_max) progress = progress_max;

	// Update the startup window progress bar.
	SW_SetBarPos(progress);
	
	// If GL is not available, we cannot proceed any further.
	if(!GL_IsInited()) return;

	if(flags & PBARF_DONTSHOW
		&& progress < progress_max
		&& progress_shown+5 >= progress) return; // Don't show yet.

	progress_shown = progress;

	if(!(flags & PBARF_NOBACKGROUND))
	{
		// This'll redraw the startup screen to this page (necessary
		// if page flipping is used by the display adapter).
		Con_DrawStartupScreen(false);

		// If we're in the User Interface, this'll redraw it.
		UI_Drawer();
	}

	fonthgt = FR_TextHeight("A");

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	// 1-to-1 mapping for the whole window.
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	// Calculate the size and dimensions of the progress window.
	w = screenWidth - 30;
	if(w < 50) w = 50;			// An unusual occurance...
	if(w > maxWidth) w = maxWidth;// Restrict width to the case of 640x480.
	x = (screenWidth - w) / 2;	// Center on screen.
	h = 2*bor + fonthgt + 15 + bar;
	y = screenHeight - 15 - h;
	
	/*x = 15;
	w = screenWidth - 2*x;
	h = 2*bor + fonthgt + 15 + bar;
	y = screenHeight - 15 - h;*/

	// Draw the (opaque black) shadow.
	UI_GradientEx(x, y, w, h, mainBor, UI_COL(UIC_SHADOW), 0, 1, 1);

	// Background.
	UI_GradientEx(x, y, w, h, mainBor,
		UI_COL(UIC_BG_MEDIUM), UI_COL(UIC_BG_LIGHT), 1, 1);
	UI_DrawRect(x, y, w, h, mainBor, UI_COL(UIC_BRD_HI), 1);
	x += bor;
	y += bor;
	w -= 2 * bor;
	h -= 2 * bor;

	// Title.
	x += 5;
	y += 5;
	w -= 10;
	gl.Color4f(0, 0, 0, .5f);
	FR_TextOut(progress_title, x + 3, y + 3);
	gl.Color3f(1, 1, 1);
	FR_TextOut(progress_title, x + 1, y + 1);
	y += fonthgt + 5;

	// Bar.
	UI_GradientEx(x, y, w, bar, 4, UI_COL(UIC_SHADOW), 0, .7f, .3f);
	UI_GradientEx(x + 1, y + 1, 8 + (w - 8)*progress/progress_max - 2, 
		bar - 2, 4, UI_COL(UIC_BG_LIGHT), UI_COL(UIC_BRD_LOW), 
		progress/(float)progress_max, -1);
	UI_DrawRect(x + 1, y + 1, 8 + (w - 8)*progress/progress_max - 2, 
		bar - 2, 4, UI_COL(UIC_TEXT), 1);

	// Show what was drawn.
	if(!(flags & PBARF_NOBLIT)) gl.Show();

	// Restore old projection matrix.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

