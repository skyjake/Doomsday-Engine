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
 * gl_font.c: Font Renderer
 *
 * The font must be small enough to fit in one texture 
 * (not a problem with *real* video cards! ;-)).
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#ifndef WIN32
#define SYSTEM_FONT			1
#define SYSTEM_FIXED_FONT 	2
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#ifdef WIN32
extern HWND hWndMain;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int		initOk = 0;
static int		numFonts;
static jfrfont_t *fonts;			// The list of fonts.
static int		current;			// Index of the current font.
static char		fontpath[256] = ""; 

// CODE --------------------------------------------------------------------

//===========================================================================
// FR_Init
//	Returns zero if there are no errors.
//===========================================================================
int FR_Init()
{
	if(initOk) return -1; // No reinitializations...

	numFonts = 0;
	fonts = 0;			// No fonts!
	current = -1;
	initOk = 1;

	// Check the font path.
	if(ArgCheckWith("-fontdir", 1))
	{
		M_TranslatePath(ArgNext(), fontpath);
		Dir_ValidDir(fontpath);
		M_CheckPath(fontpath);
	}
	else
	{
		strcpy(fontpath, ddBasePath);
		strcat(fontpath, "Data\\Fonts\\");
	}
	return 0;
}

//===========================================================================
// FR_DestroyFontIdx
//	Destroys the font with the index.
//===========================================================================
static void FR_DestroyFontIdx(int idx)
{
	jfrfont_t *font = fonts + idx;
	
	gl.DeleteTextures(1, &font->texture);
	memmove(fonts+idx, fonts+idx+1, sizeof(jfrfont_t)*(numFonts-idx-1));
	numFonts--;
	fonts = realloc(fonts, sizeof(jfrfont_t)*numFonts);
	if(current == idx) current = -1;
}

//===========================================================================
// FR_Shutdown
//===========================================================================
void FR_Shutdown()
{
	// Destroy all fonts.
	while(numFonts) FR_DestroyFontIdx(0);
	fonts = 0;
	current = -1;
	initOk = 0;
}

//===========================================================================
// OutByte
//===========================================================================
static void OutByte(FILE *f, byte b)
{
	fwrite(&b, sizeof(b), 1, f);
}

//===========================================================================
// OutShort
//===========================================================================
static void OutShort(FILE *f, short s)
{
	fwrite(&s, sizeof(s), 1, f);
}

//===========================================================================
// InByte
//===========================================================================
static byte InByte(DFILE *f)
{
	byte b;
	F_Read(&b, sizeof(b), f);
	return b;
}

//===========================================================================
// InShort
//===========================================================================
static unsigned short InShort(DFILE *f)
{
	unsigned short s;

	F_Read(&s, sizeof(s), f);
	return s;
}

//===========================================================================
// FR_GetFontIdx
//===========================================================================
int FR_GetFontIdx(int id)
{
	int i;
	for(i=0; i<numFonts; i++)
		if(fonts[i].id == id) return i;
	return -1;
}

//===========================================================================
// FR_DestroyFont
//===========================================================================
void FR_DestroyFont(int id)
{
	int idx = FR_GetFontIdx(id);

	FR_DestroyFontIdx(idx);
	if(current == idx) current = -1;
}

//===========================================================================
// FR_GetFont
//===========================================================================
jfrfont_t *FR_GetFont(int id)
{
	int idx = FR_GetFontIdx(id);
	if(idx == -1) return 0;
	return fonts + idx;
}

//===========================================================================
// FR_GetMaxId
//===========================================================================
static int FR_GetMaxId()
{
	int	i, grid=0;
	for(i=0; i<numFonts; i++)
		if(fonts[i].id > grid) grid = fonts[i].id;
	return grid;
}

#ifdef WIN32
//===========================================================================
// findPow2
//===========================================================================
static int findPow2(int num)
{
	int cumul = 1;
	
	for(; num > cumul; cumul *= 2);
	return cumul;
}
#endif

//===========================================================================
// FR_SaveFont
//===========================================================================
int FR_SaveFont(char *filename, jfrfont_t *font, unsigned int *image)
{
	FILE *file = fopen(filename, "wb");
	int i, c, bit, numPels;
	byte mask;

	if(!file) return false;

	// Write header.
	OutByte(file, 0);				// Version.
	OutShort(file, font->texWidth);
	OutShort(file, font->texHeight);
	OutShort(file, MAX_CHARS);		// Number of characters.

	// Characters.
	for(i = 0; i < MAX_CHARS; i++)
	{
		OutShort(file, font->chars[i].x);		
		OutShort(file, font->chars[i].y);
		OutByte(file, font->chars[i].w);
		OutByte(file, font->chars[i].h);
	}

	// Write a zero to indicate the data is in bitmap format (0,1).
	OutByte(file, 0);
	numPels = font->texWidth * font->texHeight;
	for(c = i = 0; i < (numPels+7)/8; i++)
	{
		for(mask = 0, bit = 7; bit >= 0; bit--, c++)
			mask |= (c < numPels? image[c] != 0 : false) << bit;
		OutByte(file, mask);
	}
	
	fclose(file);
	return true;
}

//===========================================================================
// FR_NewFont
//===========================================================================
int FR_NewFont(void)
{
	jfrfont_t *font;

	fonts = realloc(fonts, sizeof(jfrfont_t) * ++numFonts);
	font = fonts + numFonts-1;
	memset(font, 0, sizeof(jfrfont_t));
	font->id = FR_GetMaxId() + 1;
	return (current = numFonts - 1);	
}

//===========================================================================
// FR_PrepareFont
//	Prepares a font from a file. If the given file is not found,
//	prepares the corresponding GDI font.
//===========================================================================
int FR_PrepareFont(char *name)
{
#ifdef WIN32
	struct 
	{ 
		char *name;
		int gdires;
		char *winfontname;
		int pointsize;
	}	
	fontmapper[] =
	{
		{ "Fixed",		SYSTEM_FIXED_FONT },
		{ "Fixed12",	0, "Fixedsys", 12 },
		{ "System",		SYSTEM_FONT },
		{ "System12",	0, "System", 12 },
		{ "Large",		0, "MS Sans Serif", 18 },
		{ "Small7",		0, "Small Fonts", 7 },
		{ "Small8",		0, "Small Fonts", 8 },
		{ "Small10",	0, "Small Fonts", 10 },
		{ NULL, 0 }
	};
#endif		
	char buf[64];
	DFILE *file;
	int i, c, bit, mask, version, format, numPels;
	jfrfont_t *font;
	jfrchar_t *ch;
	int numChars;
	int *image;

	strcpy(buf, fontpath);
	strcat(buf, name);
	strcat(buf, ".dfn");
	if(ArgCheck("-gdifonts") || (file = F_Open(buf, "rb")) == NULL)
	{
#ifdef WIN32		
		// No luck...
		for(i = 0; fontmapper[i].name; i++)
			if(!stricmp(fontmapper[i].name, name))
			{
				if(verbose)
				{
					Con_Message("FR_PrepareFont: GDI font for \"%s\".\n", 
						fontmapper[i].name);
				}
				if(fontmapper[i].winfontname)
				{
					HDC hDC = GetDC(hWndMain);
					HFONT uifont = CreateFont(
						-MulDiv(fontmapper[i].pointsize,
								GetDeviceCaps(hDC, LOGPIXELSY), 72), 
						0, 0, 0, 0, FALSE,
						FALSE, FALSE, DEFAULT_CHARSET,
						OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY,
						VARIABLE_PITCH | FF_SWISS,
						fontmapper[i].winfontname);
					FR_PrepareGDIFont(uifont);
					DeleteObject(uifont);
					ReleaseDC(hWndMain, hDC);
				}
				else
				{
					FR_PrepareGDIFont(GetStockObject(fontmapper[i].gdires));
				}
				strcpy(fonts[current].name, name);
				return true;
			}
#endif		
		// The font was not found.
		return false;
	}

	if(verbose) Con_Message("FR_PrepareFont: %s\n", M_Pretty(buf)); 

	version = InByte(file);
	
	// Load the font from the file.
	FR_NewFont();
	font = fonts + current;
	strcpy(font->name, name);
	
	// Load in the data.
	font->texWidth = InShort(file);
	font->texHeight = InShort(file);
	numChars = InShort(file);
	for(i = 0; i < numChars; i++)
	{
		ch = &font->chars[i < MAX_CHARS? i : MAX_CHARS-1];
		ch->x = InShort(file);
		ch->y = InShort(file);
		ch->w = InByte(file);
		ch->h = InByte(file);
	}
	
	// The bitmap.
	format = InByte(file);
	if(format > 0) 
	{
		Con_Message("FR_PrepareFont: Font %s is in unknown format %i.\n",
			buf, format);
		goto unknown_format;
	}

	numPels = font->texWidth * font->texHeight;
	image = calloc(numPels, sizeof(int));
	for(c = i = 0; i < (numPels+7)/8; i++)
	{
		mask = InByte(file);
		for(bit = 7; bit >= 0; bit--, c++)
		{
			if(c >= numPels) break;
			if(mask & (1 << bit)) image[c] = ~0;
		}
	}

	// Load in the texture.
	font->texture = gl.NewTexture();
	gl.TexImage(DGL_RGBA, font->texWidth, font->texHeight, 0, image);
	gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
	gl.TexParameter(DGL_MAG_FILTER, DGL_NEAREST);
	free(image);

unknown_format:
	F_Close(file);
	return true;
}

//===========================================================================
// FR_PrepareGDIFont
//	Prepare a GDI font. Select it as the current font.
//===========================================================================
#ifdef WIN32
int FR_PrepareGDIFont(HFONT hfont)
{
	RECT		rect;
	jfrfont_t	*font;
	int			i, x, y, maxh, bmpWidth=256, bmpHeight=0, imgWidth, imgHeight;
	HDC			hdc;
	HBITMAP		hbmp;
	unsigned int *image;

	// Create a new font.
	FR_NewFont();
	font = fonts + current;
		
	// Now we'll create the actual data.
	hdc = CreateCompatibleDC(NULL);
	SetMapMode(hdc, MM_TEXT);
	SelectObject(hdc, hfont);
	// Let's first find out the sizes of all the characters.
	// Then we can decide how large a texture we need.
	for(i = 0, x = 0, y = 0, maxh = 0; i < 256; i++)
	{
		jfrchar_t *fc = font->chars + i;
		SIZE size;
		byte ch[2] = { i, 0 };
		GetTextExtentPoint32(hdc, ch, 1, &size);
		fc->w = size.cx;
		fc->h = size.cy;
		maxh = max(maxh, fc->h);
		x += fc->w + 1;
		if(x >= bmpWidth)	
		{
			x = 0;
			y += maxh + 1;
			maxh = 0;
		}
	}
	bmpHeight = y + maxh;
	hbmp = CreateCompatibleBitmap(hdc, bmpWidth, bmpHeight);
	SelectObject(hdc, hbmp);
	SetBkMode(hdc, OPAQUE);
	SetBkColor(hdc, 0);
	SetTextColor(hdc, 0xffffff);
	rect.left = 0;
	rect.top = 0;
	rect.right = bmpWidth;
	rect.bottom = bmpHeight;
	FillRect(hdc, &rect, GetStockObject(BLACK_BRUSH));
	// Print all the characters.
	for(i=0, x=0, y=0, maxh=0; i<256; i++)
	{
		jfrchar_t *fc = font->chars + i;
		byte ch[2] = { i, 0 };
		if(x+fc->w+1 >= bmpWidth)
		{
			x = 0;
			y += maxh + 1;
			maxh = 0;
		}
		if(i) TextOut(hdc, x+1, y+1, ch, 1);
		fc->x = x+1;
		fc->y = y+1;
		maxh = max(maxh, fc->h);
		x += fc->w + 1;
	}
	// Now we can make a version that DGL can read.	
	imgWidth = findPow2(bmpWidth);
	imgHeight = findPow2(bmpHeight);
//	printf( "font: %d x %d\n", imgWidth, imgHeight);
	image = malloc(4*imgWidth*imgHeight);
	memset(image, 0, 4*imgWidth*imgHeight);
	for(y = 0; y < bmpHeight; y++)
		for(x = 0; x < bmpWidth; x++)			
			if(GetPixel(hdc, x, y))
			{
				image[x + y*imgWidth] = 0xffffffff;
				/*if(x+1 < imgWidth && y+1 < imgHeight)
					image[x+1 + (y+1)*imgWidth] = 0xff000000;*/
			}

	/*saveTGA24_rgba8888("ddfont.tga", bmpWidth, bmpHeight, 
		(unsigned char*)image);*/

	font->texWidth = imgWidth;
	font->texHeight = imgHeight;

	// If necessary, write the font data to a file.
	if(ArgCheck("-dumpfont"))
	{
		char buf[20];
		sprintf(buf, "font%i.dfn", font->id);
		FR_SaveFont(buf, font, image);
	}

	// Create the DGL texture.
	font->texture = gl.NewTexture();
	gl.TexImage(DGL_RGBA, imgWidth, imgHeight, 0, image);
	gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
	gl.TexParameter(DGL_MAG_FILTER, DGL_NEAREST);

	// We no longer need these.
	free(image);
	DeleteObject(hbmp);
	DeleteDC(hdc);
	return 0;
}
#endif // WIN32

//===========================================================================
// FR_SetFont
//	Change the current font.
//===========================================================================
void FR_SetFont(int id)
{
	int idx = FR_GetFontIdx(id);	
	if(idx == -1) return;	// No such font.
	current = idx;
}

//===========================================================================
// FR_CharWidth
//===========================================================================
int FR_CharWidth(int ch)
{
	if(current == -1) return 0;
	return fonts[current].chars[ch].w;
}

//===========================================================================
// FR_TextWidth
//===========================================================================
int FR_TextWidth(char *text)
{
	int i, width = 0, len = strlen(text);
	jfrfont_t *cf;

	if(current == -1) return 0;
	
	// Just add them together.
	for(cf = fonts + current, i = 0; i < len; i++)
		width += cf->chars[(byte)text[i]].w;
	
	return width;
}

//===========================================================================
// FR_TextHeight
//===========================================================================
int FR_TextHeight(char *text)
{
	int i, height = 0, len;
	jfrfont_t *cf;

	if(current == -1 || !text) return 0;

	// Find the greatest height.
	for(len = strlen(text), cf = fonts + current, i = 0; i < len; i++)
		height = MAX_OF(height, cf->chars[(byte)text[i]].h);

	return height;
}

//===========================================================================
// FR_TextOut
//	(x,y) is the upper left corner. Returns the length.
//===========================================================================
int FR_TextOut(char *text, int x, int y)
{
	float dx, dy;
	int i, width = 0, len;
	jfrfont_t *cf;

	if(!text) return 0;
	len = strlen(text);

	// Check the font.
	if(current == -1) return 0;	// No selected font.
	cf = fonts + current;

	// Set the texture.
	gl.Bind(cf->texture);

	// Print it.
	gl.Begin(DGL_QUADS);
	for(i = 0; i < len; i++)
	{
		jfrchar_t *ch = cf->chars + (byte)text[i];
		float coff = 0; //.5f;
		float cx = (float)ch->x + coff, cy = (float)ch->y + coff;
		float texw = (float)cf->texWidth, texh = (float)cf->texHeight;

		dx = x + coff;
		dy = y + coff;

		// Upper left.
		gl.TexCoord2f(cx/texw, cy/texh);
		gl.Vertex2f(dx, dy);
		// Upper right.
		gl.TexCoord2f((cx+ch->w)/texw, cy/texh);
		gl.Vertex2f(dx+ch->w+coff, dy);
		// Lower right.
		gl.TexCoord2f((cx+ch->w)/texw, (cy+ch->h)/texh);
		gl.Vertex2f(dx+ch->w+coff, dy+ch->h+coff);
		// Lower left.
		gl.TexCoord2f(cx/texw, (cy+ch->h)/texh);
		gl.Vertex2f(dx, dy+ch->h+coff);
		// Move on.
		width += ch->w;
		x += ch->w;
	}
	gl.End();
	return width;
}

//===========================================================================
// FR_GetCurrent
//===========================================================================
int FR_GetCurrent()
{
	if(current == -1) return 0;
	return fonts[current].id;
}
