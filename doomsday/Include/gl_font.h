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
 * gl_font.h: Font Renderer
 */

#ifndef __DGL_FONT_RENDERER_H__
#define __DGL_FONT_RENDERER_H__

#ifdef WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#		define NOSOUND
#		define NOCOMM
#		define NOHELP
#		define NOCOLOR
#		define NOCLIPBOARD
#		define NOCTLMGR
#		define NOKERNEL
#	endif
#	include <windows.h>
#endif

#define MAX_CHARS	256			// Normal 256 ANSI characters.

// Data for a character.
typedef struct
{
	int	x, y;					// The upper left corner of the character.
	int	w, h;					// The width and height.
} jfrchar_t;

// Data for a font.
typedef struct
{
	int				id;
	char			name[256];
	unsigned int	texture;	// The name of the texture for this font.
	int				texWidth, texHeight;
	jfrchar_t		chars[MAX_CHARS];	
} jfrfont_t;


int FR_Init();
void FR_Shutdown();
jfrfont_t *FR_GetFont(int id);

#ifdef WIN32
// Prepare a GDI font. Select it as the current font. Only available
// on Windows.
int FR_PrepareGDIFont(HFONT hfont);
#endif

int FR_PrepareFont(char *name);

// Change the current font.
void FR_SetFont(int id);
int FR_GetCurrent();
void FR_DestroyFont(int id);

int FR_CharWidth(int ch);
int FR_TextWidth(char *text);
int FR_TextHeight(char *text);

// (x,y) is the upper left corner. Returns the length.
int FR_TextOut(char *text, int x, int y);

#endif // __OGL_FONT_RENDERER_H__
