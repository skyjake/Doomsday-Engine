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
 * con_start.c: Console Startup Screen
 *
 * Draws the GL startup screen & messages.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int bufferLines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		startupScreen = false;
int			startupLogo;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char *titletext;
static int fontHgt = 8;	// Height of the font.
static DGLuint bgflat;
char *bitmap = NULL;

// CODE --------------------------------------------------------------------

//===========================================================================
// Con_StartupInit
//===========================================================================
void Con_StartupInit(void)
{
	static boolean firstTime = true;

	if(novideo) return;

	GL_InitVarFont();
	fontHgt = FR_TextHeight("Doomsday!");

	startupScreen = true;
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	if(firstTime)
	{
		titletext = "Doomsday "DOOMSDAY_VERSION_TEXT" Startup";
		firstTime = false;
		bgflat = 0;
	}
	else
	{
		titletext = "Doomsday "DOOMSDAY_VERSION_TEXT;
	}

	// Load graphics.
	startupLogo = GL_LoadGraphics("Background", LGM_GRAYSCALE);
}

void Con_SetBgFlat(int lump)
{
	bgflat = GL_BindTexFlat(R_GetFlat(lump));
}

void Con_StartupDone(void)
{
	if(isDedicated) return;
	startupScreen = false;
	gl.DeleteTextures(1, &startupLogo);
	startupLogo = 0;
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
	GL_ShutdownVarFont();
}

//===========================================================================
// Con_DrawStartupBackground
//	Background with the "The Doomsday Engine" text superimposed.
//===========================================================================
void Con_DrawStartupBackground(void)
{
	float mul = (startupLogo? 1.5f : 1.0f);
	ui_color_t *dark = UI_COL(UIC_BG_DARK), *light = UI_COL(UIC_BG_LIGHT);

	// Background gradient picture.
	gl.Bind(startupLogo);
	gl.Disable(DGL_BLENDING);
	gl.Begin(DGL_QUADS);
	// Top color.
	gl.Color3f(dark->red*mul, dark->green*mul, dark->blue*mul);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(0, 0);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(screenWidth, 0); 
	// Bottom color.
	gl.Color3f(light->red*mul, light->green*mul, light->blue*mul); 
	gl.TexCoord2f(1, 1);
	gl.Vertex2f(screenWidth, screenHeight);
	gl.TexCoord2f(0, 1);
	gl.Vertex2f(0, screenHeight);
	gl.End();
	gl.Enable(DGL_BLENDING);

	// Draw logo.
/*	gl.Enable(DGL_TEXTURING);
	gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE);
	if(startupLogo)
	{
		// Calculate logo placement.
		w = screenWidth/1.25f;
		h = LOGO_HEIGHT * w/LOGO_WIDTH;
		x = (screenWidth - w)/2;
		y = (screenHeight - h)/2;

		// Draw it in two passes: additive and subtractive. 
		gl.Bind(startupLogo);
		GL_DrawRect(x, y, w, h, .05f, .05f, .05f, 1);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		GL_DrawRect(x - w/170, y - w/170, w, h, .1f, .1f, .1f, 1);
		gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE);
		gl.Color3f(.05f, .05f, .05f);
		FR_TextOut(DOOMSDAY_VERSIONTEXT, 
			x + w - FR_TextWidth(DOOMSDAY_VERSIONTEXT),	
			y + h);
	}
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);	*/
}

//===========================================================================
// Con_DrawStartupScreen
//	Does not show anything on screen.
//===========================================================================
void Con_DrawStartupScreen(int show)
{
	int i, vislines, y, x, st;
	int topy;
	
	// Print the messages in the console.
	if(!startupScreen || ui_active) return;

	Con_DrawStartupBackground();

	// Draw the title.
	FR_SetFont(glFontVariable);
	UI_DrawTitleEx(titletext, topy = FR_TextHeight("W") + UI_BORDER*2);
	FR_SetFont(glFontFixed);

	topy += UI_BORDER;
	vislines = (screenHeight - topy + fontHgt/2)/fontHgt;
	y = topy;

	st = bufferLines - vislines;
	// Show the last line, too, if there's something.
	if(Con_GetBufferLine(bufferLines-1)->len) st++;
	if(st < 0) st = 0;
	for(i = 0; i < vislines && st+i < bufferLines; i++)
	{
		cbline_t *line = Con_GetBufferLine(st+i);
		if(!line) break;
		if(line->flags & CBLF_RULER)
		{
			Con_DrawRuler(y, fontHgt, 1);
		}
		else
		{
			x = line->flags & CBLF_CENTER? 
				(screenWidth - FR_TextWidth(line->text))/2 : 3;
			gl.Color3f(0, 0, 0);
			FR_TextOut(line->text, x+1, y+1);
			gl.Color3f(1, 1, 1);
			FR_TextOut(line->text, x, y);			
		}
		y += fontHgt;
	}
	if(show) 
	{
		// Update the progress bar, if one is active.
		Con_Progress(0, PBARF_NOBACKGROUND | PBARF_NOBLIT);
		gl.Show();
	}
}


