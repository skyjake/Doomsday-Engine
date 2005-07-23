// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:  Heads-up displays, font handling, text drawing routines
//
//-----------------------------------------------------------------------------

#ifdef WIN32
#  pragma warning(disable:4018)
#endif

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "../Common/hu_stuff.h"
#include "../Common/hu_msg.h"

#ifdef __JDOOM__
# include "../jDoom/Mn_def.h"
# include "../jDoom/m_menu.h"
# include "../jDoom/doomstat.h"
# include "../jDoom/p_local.h"
# include "../jDoom/d_config.h"
# include "../jDoom/dstrings.h"
#elif __JHERETIC__
# include "../jHeretic/Doomdef.h"
# include "../jHeretic/Mn_def.h"
# include "../jHeretic/h_config.h"
#elif __JHEXEN__
# include "../jHexen/h2def.h"
# include "../jHexen/mn_def.h"
# include "../jHexen/x_config.h"
#elif __JSTRIFE__
# include "../jStrife/h2def.h"
# include "../jStrife/mn_def.h"
# include "../jStrife/d_config.h"
#endif

// MACROS ------------------------------------------------------------------

#ifdef __JDOOM__

//
// Locally used constants, shortcuts.
//
#define HU_TITLE	(mapnames[(gameepisode-1)*9+gamemap-1])
#define HU_TITLE2	(mapnames2[gamemap-1])
#define HU_TITLEP	(mapnamesp[gamemap-1])
#define HU_TITLET	(mapnamest[gamemap-1])
#define HU_TITLEHEIGHT	1

#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean automapactive;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dpatch_t hu_font[HU_FONTSIZE];
dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

int     typein_time = 0;

#ifdef __JDOOM__

boolean hu_showallfrags = false;

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

char   *mapnames[9 * 5];		// DOOM shareware/registered/retail (Ultimate) names.
int     mapnames_idx[] = {
	TXT_HUSTR_E1M1,
	TXT_HUSTR_E1M2,
	TXT_HUSTR_E1M3,
	TXT_HUSTR_E1M4,
	TXT_HUSTR_E1M5,
	TXT_HUSTR_E1M6,
	TXT_HUSTR_E1M7,
	TXT_HUSTR_E1M8,
	TXT_HUSTR_E1M9,

	TXT_HUSTR_E2M1,
	TXT_HUSTR_E2M2,
	TXT_HUSTR_E2M3,
	TXT_HUSTR_E2M4,
	TXT_HUSTR_E2M5,
	TXT_HUSTR_E2M6,
	TXT_HUSTR_E2M7,
	TXT_HUSTR_E2M8,
	TXT_HUSTR_E2M9,

	TXT_HUSTR_E3M1,
	TXT_HUSTR_E3M2,
	TXT_HUSTR_E3M3,
	TXT_HUSTR_E3M4,
	TXT_HUSTR_E3M5,
	TXT_HUSTR_E3M6,
	TXT_HUSTR_E3M7,
	TXT_HUSTR_E3M8,
	TXT_HUSTR_E3M9,

	TXT_HUSTR_E4M1,
	TXT_HUSTR_E4M2,
	TXT_HUSTR_E4M3,
	TXT_HUSTR_E4M4,
	TXT_HUSTR_E4M5,
	TXT_HUSTR_E4M6,
	TXT_HUSTR_E4M7,
	TXT_HUSTR_E4M8,
	TXT_HUSTR_E4M9,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1
};

char   *mapnames2[32];			// DOOM 2 map names.
int     mapnames2_idx[] = {
	TXT_HUSTR_1,
	TXT_HUSTR_2,
	TXT_HUSTR_3,
	TXT_HUSTR_4,
	TXT_HUSTR_5,
	TXT_HUSTR_6,
	TXT_HUSTR_7,
	TXT_HUSTR_8,
	TXT_HUSTR_9,
	TXT_HUSTR_10,
	TXT_HUSTR_11,

	TXT_HUSTR_12,
	TXT_HUSTR_13,
	TXT_HUSTR_14,
	TXT_HUSTR_15,
	TXT_HUSTR_16,
	TXT_HUSTR_17,
	TXT_HUSTR_18,
	TXT_HUSTR_19,
	TXT_HUSTR_20,

	TXT_HUSTR_21,
	TXT_HUSTR_22,
	TXT_HUSTR_23,
	TXT_HUSTR_24,
	TXT_HUSTR_25,
	TXT_HUSTR_26,
	TXT_HUSTR_27,
	TXT_HUSTR_28,
	TXT_HUSTR_29,
	TXT_HUSTR_30,
	TXT_HUSTR_31,
	TXT_HUSTR_32
};

char   *mapnamesp[32];			// Plutonia WAD map names.
int     mapnamesp_idx[] = {
	TXT_PHUSTR_1,
	TXT_PHUSTR_2,
	TXT_PHUSTR_3,
	TXT_PHUSTR_4,
	TXT_PHUSTR_5,
	TXT_PHUSTR_6,
	TXT_PHUSTR_7,
	TXT_PHUSTR_8,
	TXT_PHUSTR_9,
	TXT_PHUSTR_10,
	TXT_PHUSTR_11,

	TXT_PHUSTR_12,
	TXT_PHUSTR_13,
	TXT_PHUSTR_14,
	TXT_PHUSTR_15,
	TXT_PHUSTR_16,
	TXT_PHUSTR_17,
	TXT_PHUSTR_18,
	TXT_PHUSTR_19,
	TXT_PHUSTR_20,

	TXT_PHUSTR_21,
	TXT_PHUSTR_22,
	TXT_PHUSTR_23,
	TXT_PHUSTR_24,
	TXT_PHUSTR_25,
	TXT_PHUSTR_26,
	TXT_PHUSTR_27,
	TXT_PHUSTR_28,
	TXT_PHUSTR_29,
	TXT_PHUSTR_30,
	TXT_PHUSTR_31,
	TXT_PHUSTR_32
};

char   *mapnamest[32];			// TNT WAD map names.
int     mapnamest_idx[] = {
	TXT_THUSTR_1,
	TXT_THUSTR_2,
	TXT_THUSTR_3,
	TXT_THUSTR_4,
	TXT_THUSTR_5,
	TXT_THUSTR_6,
	TXT_THUSTR_7,
	TXT_THUSTR_8,
	TXT_THUSTR_9,
	TXT_THUSTR_10,
	TXT_THUSTR_11,

	TXT_THUSTR_12,
	TXT_THUSTR_13,
	TXT_THUSTR_14,
	TXT_THUSTR_15,
	TXT_THUSTR_16,
	TXT_THUSTR_17,
	TXT_THUSTR_18,
	TXT_THUSTR_19,
	TXT_THUSTR_20,

	TXT_THUSTR_21,
	TXT_THUSTR_22,
	TXT_THUSTR_23,
	TXT_THUSTR_24,
	TXT_THUSTR_25,
	TXT_THUSTR_26,
	TXT_THUSTR_27,
	TXT_THUSTR_28,
	TXT_THUSTR_29,
	TXT_THUSTR_30,
	TXT_THUSTR_31,
	TXT_THUSTR_32
};

#elif __JSTRIFE__

char   *mapnames[32];			// STRIFE map names.
int     mapnames_idx[] = {
	TXT_HUSTR_1,
	TXT_HUSTR_2,
	TXT_HUSTR_3,
	TXT_HUSTR_4,
	TXT_HUSTR_5,
	TXT_HUSTR_6,
	TXT_HUSTR_7,
	TXT_HUSTR_8,
	TXT_HUSTR_9,
	TXT_HUSTR_10,
	TXT_HUSTR_11,

	TXT_HUSTR_12,
	TXT_HUSTR_13,
	TXT_HUSTR_14,
	TXT_HUSTR_15,
	TXT_HUSTR_16,
	TXT_HUSTR_17,
	TXT_HUSTR_18,
	TXT_HUSTR_19,
	TXT_HUSTR_20,

	TXT_HUSTR_21,
	TXT_HUSTR_22,
	TXT_HUSTR_23,
	TXT_HUSTR_24,
	TXT_HUSTR_25,
	TXT_HUSTR_26,
	TXT_HUSTR_27,
	TXT_HUSTR_28,
	TXT_HUSTR_29,
	TXT_HUSTR_30,
	TXT_HUSTR_31,
	TXT_HUSTR_32
};
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean headsupactive = false;

// Code -------------------------------------------------------------------

/*
 *  R_CachePatch
 */
void R_CachePatch(dpatch_t * dp, char *name)
{
	patch_t *patch;

	if(IS_DEDICATED)
		return;

	dp->lump = W_CheckNumForName(name);
	if(dp->lump == -1)
		return;
	patch = (patch_t *) W_CacheLumpNum(dp->lump, PU_CACHE);
	dp->width = patch->width;
	dp->height = patch->height;
	dp->leftoffset = patch->leftoffset;
	dp->topoffset = patch->topoffset;

	// Precache the texture while we're at it.
	GL_SetPatch(dp->lump);
}

/*
 *  HU_Init
 *		Loads the font patches and inits various strings
 *		jHEXEN Note: Don't bother with the yellow font, we'll colour the white version
 */
void HU_Init(void)
{
	int     i;
	int     j;
	char    buffer[9];
#ifndef __JDOOM__    
	dpatch_t tmp;
#endif

#ifdef __JDOOM__

	// Setup strings.
#define INIT_STRINGS(x, x_idx) \
	for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
		x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

	INIT_STRINGS(mapnames, mapnames_idx);
	INIT_STRINGS(mapnames2, mapnames2_idx);
	INIT_STRINGS(mapnamesp, mapnamesp_idx);
	INIT_STRINGS(mapnamest, mapnamest_idx);

#elif __JSTRIFE__
	// Setup strings.
#define INIT_STRINGS(x, x_idx) \
	for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
		x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

	INIT_STRINGS(mapnames, mapnames_idx);
#endif

#ifdef __JDOOM__
	// load the heads-up fonts
	j = HU_FONTSTART;
	for(i = 0; i < HU_FONTSIZE; i++, j++)
	{
		// The original small red font.
		sprintf(buffer, "STCFN%.3d", j);
		R_CachePatch(&hu_font[i], buffer);

		// Small white font.
		sprintf(buffer, "FONTA%.3d", j);
		R_CachePatch(&hu_font_a[i], buffer);

		// Large (12) white font.
		sprintf(buffer, "FONTB%.3d", j);
		R_CachePatch(&hu_font_b[i], buffer);
		if(hu_font_b[i].lump == -1)
		{
			// This character is missing! (the first character
			// is supposedly always found)
			memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
		}
	}
#elif __JSTRIFE__
	// load the heads-up fonts

	// Tell Doomsday to load the following patches in monochrome mode (2 = weighted average)
	DD_SetInteger(DD_MONOCHROME_PATCHES, 2);

	j = HU_FONTSTART;
	for(i = 0; i < HU_FONTSIZE; i++, j++)
	{
		// The original small red font.
		sprintf(buffer, "STCFN%.3d", j);
		R_CachePatch(&hu_font[i], buffer);

		// Small white font.
		sprintf(buffer, "STCFN%.3d", j);
		R_CachePatch(&hu_font_a[i], buffer);

		// Large (12) white font.
		sprintf(buffer, "STBFN.3d", j);
		R_CachePatch(&hu_font_b[i], buffer);
		if(hu_font_b[i].lump == -1)
		{
			// This character is missing! (the first character
			// is supposedly always found)
			memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
		}
	}

	// deactivate monochrome mode
	DD_SetInteger(DD_MONOCHROME_PATCHES, 0);
#else
	// Tell Doomsday to load the following patches in monochrome mode (2 = weighted average)
	DD_SetInteger(DD_MONOCHROME_PATCHES, 2);

	// Heretic/Hexen don't use ASCII numbered font patches
	// plus they don't even have a full set eg '!' = 1 '_'= 58 !!!
	j = 1;
	for(i = 0; i < HU_FONTSIZE; i++, j++)
	{
		// Small font.
		sprintf(buffer, "FONTA%.2d", j);
		R_CachePatch(&hu_font_a[i], buffer);

		// Large (12) font.
		sprintf(buffer, "FONTB%.2d", j);
		R_CachePatch(&hu_font_b[i], buffer);
		if(hu_font_b[i].lump == -1)
		{
			// This character is missing! (the first character
			// is supposedly always found)
			memcpy(&hu_font_b[0 + i], &hu_font_b[4], sizeof(dpatch_t));
		}
	}

	// deactivate monochrome mode
	DD_SetInteger(DD_MONOCHROME_PATCHES, 0);

	// Heretic and Hexen don't use ASCII numbering for all font patches.
	// As such we need to switch some pathches.

	tmp = hu_font_a[59];
	memcpy(&hu_font_a[59], &hu_font_a[63], sizeof(dpatch_t));
	memcpy(&hu_font_a[63], &tmp, sizeof(dpatch_t));

	tmp = hu_font_b[59];
	memcpy(&hu_font_b[59], &hu_font_b[63], sizeof(dpatch_t));
	memcpy(&hu_font_b[63], &tmp, sizeof(dpatch_t));

#endif

	HUMsg_Init();
}

/*
 *  HU_Stop
 */
void HU_Stop(void)
{
	headsupactive = false;
}

/*
 *  HU_Start
 */
void HU_Start(void)
{
#ifdef __JDOOM__
	char   *s;
#endif

	if(headsupactive)
		HU_Stop();
#ifdef __JDOOM__
	if(Get(DD_MAP_NAME))
		s = (char *) Get(DD_MAP_NAME);
	else
	{
		switch (gamemode)
		{
		case shareware:
		case registered:
		case retail:
			s = HU_TITLE;
			break;
		case commercial:
		default:
			s = HU_TITLE2;
			break;
		}
	}
	// Plutonia and TNT are a special case.
	if(gamemission == pack_plut)
		s = HU_TITLEP;
	if(gamemission == pack_tnt)
		s = HU_TITLET;
#endif
	HUMsg_Start();

	headsupactive = true;

}

/*
 *  HU_Drawer
 */
void HU_Drawer(void)
{
#ifdef __JDOOM__
	int     i, k, x, y;
	char    buf[80];
#endif

	HUMsg_Drawer();

#ifdef __JDOOM__
	if(hu_showallfrags)
	{
		for(y = 8, i = 0; i < MAXPLAYERS; i++, y += 10)
		{
			sprintf(buf, "%i%s", i, i == consoleplayer ? "=" : ":");
			M_WriteText(0, y, buf);
			x = 20;
			for(k = 0; k < MAXPLAYERS; k++, x += 18)
			{
				sprintf(buf, "%i", players[i].frags[k]);
				M_WriteText(x, y, buf);
			}
		}
	}
#endif
}

/*
 *  HU_Ticker
 */
void HU_Ticker(void)
{
	HUMsg_Ticker();
}

/*
 *  MN_FilterChar
 */
int MN_FilterChar(int ch)
{
	ch = toupper(ch);
	if(ch == '_')
		ch = '[';
	else if(ch == '\\')
		ch = '/';
	else if(ch < 32 || ch > 'Z')
		ch = 32;				// We don't have this char.
	return ch;
}

/*
 *  MN_TextFilter
 */
void MN_TextFilter(char *text)
{
	int     k;

	for(k = 0; text[k]; k++)
	{
		text[k] = MN_FilterChar(text[k]);
		/*      char ch = toupper(text[k]);
		   if(ch == '_') ch = '[';  // Mysterious... (from save slots).
		   else if(ch == '\\') ch = '/';
		   // Check that the character is printable.
		   else if(ch < 32 || ch > 'Z') ch = 32; // Character out of range.
		   text[k] = ch;            */
	}
}

/*
 *  WI_ParseFloat
 *		Expected: <whitespace> = <whitespace> <float>
 */
float WI_ParseFloat(char **str)
{
	float   value;
	char   *end;

	*str = M_SkipWhite(*str);
	if(**str != '=')
		return 0;				// Now I'm confused!
	*str = M_SkipWhite(*str + 1);
	value = strtod(*str, &end);
	*str = end;
	return value;
}

/*
 *  WI_DrawParamText
 *		Draw a string of text controlled by parameter blocks.
 */
void WI_DrawParamText(int x, int y, char *string, dpatch_t *defFont,
					  float defRed, float defGreen, float defBlue, float defAlpha,
					  boolean defCase, boolean defTypeIn, int halign)
{
	char    temp[256], *end;
	dpatch_t  *font = defFont;
	float   r = defRed, g = defGreen, b = defBlue, a = defAlpha;
	float   offX = 0, offY = 0;
	float   scaleX = 1, scaleY = 1, angle = 0, extraScale;
	float   cx = x, cy = y;
	int     charCount = 0;
	boolean typeIn = defTypeIn;
	boolean caseScale = defCase;
	struct {
		float   scale, offset;
	} caseMod[2];				// 1=upper, 0=lower
	int     curCase;

	int	alignx = 0;

	caseMod[0].scale = 1;
	caseMod[0].offset = 3;
	caseMod[1].scale = 1.25f;
	caseMod[1].offset = 0;

	while(*string)
	{
		// Parse and draw the replacement string.
		if(*string == '{')		// Parameters included?
		{
			string++;
			while(*string && *string != '}')
			{
				string = M_SkipWhite(string);

				// What do we have here?
				if(!strnicmp(string, "fonta", 5))
				{
					font = hu_font_a;
					string += 5;
				}
				else if(!strnicmp(string, "fontb", 5))
				{
					font = hu_font_b;
					string += 5;
				}
				else if(!strnicmp(string, "flash", 5))
				{
					string += 5;
					typeIn = true;
				}
				else if(!strnicmp(string, "noflash", 7))
				{
					string += 7;
					typeIn = false;
				}
				else if(!strnicmp(string, "case", 4))
				{
					string += 4;
					caseScale = true;
				}
				else if(!strnicmp(string, "nocase", 6))
				{
					string += 6;
					caseScale = false;
				}
				else if(!strnicmp(string, "ups", 3))
				{
					string += 3;
					caseMod[1].scale = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "upo", 3))
				{
					string += 3;
					caseMod[1].offset = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "los", 3))
				{
					string += 3;
					caseMod[0].scale = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "loo", 3))
				{
					string += 3;
					caseMod[0].offset = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "break", 5))
				{
					string += 5;
					cx = x;
					cy += scaleY * SHORT(font[0].height);
				}
				else if(!strnicmp(string, "r", 1))
				{
					string++;
					r = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "g", 1))
				{
					string++;
					g = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "b", 1))
				{
					string++;
					b = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "x", 1))
				{
					string++;
					offX = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "y", 1))
				{
					string++;
					offY = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "scalex", 6))
				{
					string += 6;
					scaleX = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "scaley", 6))
				{
					string += 6;
					scaleY = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "scale", 5))
				{
					string += 5;
					scaleX = scaleY = WI_ParseFloat(&string);
				}
				else if(!strnicmp(string, "angle", 5))
				{
					string += 5;
					angle = WI_ParseFloat(&string);
				}
				else
				{
					// Unknown, skip it.
					if(*string != '}')
						string++;
				}
			}

			// Skip over the closing brace.
			if(*string)
				string++;
		}

		for(end = string; *end && *end != '{';)
		{
			if(caseScale)
			{
				curCase = -1;
				// Select a substring with characters of the same case
				// (or whitespace).
				for(; *end && *end != '{'; end++)
				{
					// We can skip whitespace.
					if(isspace(*end))
						continue;

					if(curCase < 0)
						curCase = (isupper(*end) != 0);
					else if(curCase != (isupper(*end) != 0))
						break;
				}
			}
			else
			{
				// Find the end of the visible part of the string.
				for(; *end && *end != '{'; end++);
			}

			strncpy(temp, string, end - string);
			temp[end - string] = 0;
			string = end;		// Continue from here.

			if(halign == ALIGN_CENTER){
				alignx = (scaleX * M_StringWidth(temp, font)) / 2;
			} else if (halign == ALIGN_RIGHT){
				alignx = scaleX * M_StringWidth(temp, font);
			} else {
				alignx = 0;
			}

			// Setup the scaling.
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();

			// Rotate.
			if(angle != 0)
			{
				// The origin is the specified (x,y) for the patch.
				// We'll undo the VGA aspect ratio (otherwise the result would
				// be skewed).
				gl.Translatef(x, y, 0);
				gl.Scalef(1, 200.0f / 240.0f, 1);
				gl.Rotatef(angle, 0, 0, 1);
				gl.Scalef(1, 240.0f / 200.0f, 1);
				gl.Translatef(-x, -y, 0);
			}

			gl.Translatef(cx + offX - alignx,
						  cy + offY +
						  (caseScale ? caseMod[curCase].offset : 0), 0);
			extraScale = (caseScale ? caseMod[curCase].scale : 1);
			gl.Scalef(scaleX, scaleY * extraScale, 1);

			// Draw it.
			M_WriteText3(0, 0, temp, font, r, g, b, a, typeIn,
						 typeIn ? charCount : 0);
			charCount += strlen(temp);

			// Advance the current position.
			cx += scaleX * M_StringWidth(temp, font);

			gl.MatrixMode(DGL_MODELVIEW);
			gl.PopMatrix();
		}
	}
}

/*
 *  M_StringWidth
 *		Find string width from hu_font chars
 */
int M_StringWidth(char *string, dpatch_t * font)
{
	int     i;
	int     w = 0;
	int     c;

	for(i = 0; i < strlen(string); i++)
	{
		c = toupper(string[i]) - HU_FONTSTART;
		if(c < 0 || c >= HU_FONTSIZE)
			w += 4;
		else
			w += SHORT(font[c].width);
	}
	return w;
}

/*
 *  M_StringHeight
 *		Find string height from hu_font chars
 */
int M_StringHeight(char *string, dpatch_t * font)
{
	int     i;
	int     h;
	int     height = SHORT(font[17].height);

	h = height;
	for(i = 0; i < strlen(string); i++)
		if(string[i] == '\n')
			h += height;
	return h;
}

/*
 *  M_LetterFlash
 */
void M_LetterFlash(int x, int y, int w, int h, int bright, float red,
						  float green, float blue, float alpha)
{
	float   fsize = 4 + bright;
	float   fw = fsize * w / 2.0f, fh = fsize * h / 2.0f;
	int     origColor[4];

	// Don't draw anything for very small letters.
	if(h <= 4)
		return;

	// Store original color.
	gl.GetIntegerv(DGL_RGBA, origColor);

	gl.Bind(Get(DD_DYNLIGHT_TEXTURE));

	if(bright)
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	else
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_ALPHA);

	GL_DrawRect(x + w / 2.0f - fw / 2, y + h / 2.0f - fh / 2, fw, fh, red,
				green, blue, alpha);

	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

	// Restore original color.
	gl.Color4ub(origColor[0], origColor[1], origColor[2], origColor[3]);
}

/*
 *  M_WriteText
 *		Write a string using the hu_font
 */
void M_WriteText(int x, int y, char *string)
{
	M_WriteText2(x, y, string, 0, 1, 1, 1, 1);
}

/*
 *  M_WriteText2
 */
void M_WriteText2(int x, int y, char *string, dpatch_t *font, float red,
				  float green, float blue, float alpha)
{
	M_WriteText3(x, y, string, font, red, green, blue, alpha, true, 0);
}

/*
 *  M_WriteText3
 *		Write a string using a colored, custom font.
 *		Also do a type-in effect.
 */
void M_WriteText3(int x, int y, const char *string, dpatch_t *font, float red,
				  float green, float blue, float alpha, boolean doTypeIn, int initialCount)
{
	int     pass;
	int     w, h;
	const char *ch;
	int     c;
	int     cx;
	int     cy;
	int     count, maxCount, yoff;
	float   flash;

	float 	fr = (1 + 2 * red) / 3;
	float	fb = (1 + 2 * blue) / 3;
	float	fg = (1 + 2 * green) / 3;
	float	fa = cfg.menuGlitter * alpha;

	for(pass = 0; pass < 2; ++pass)
	{
		count = initialCount;
		maxCount = typein_time * 2;

		// Disable type-in?
		if(!doTypeIn || cfg.menuEffects > 0)
			maxCount = 0xffff;

		if(red >= 0)
			gl.Color4f(red, green, blue, alpha);

		ch = string;
		cx = x;
		cy = y;

		while(1)
		{
			c = *ch++;
			count++;
			yoff = 0;
			flash = 0;
			if(count == maxCount)
			{
				flash = 1;
				if(red >= 0)
					gl.Color4f(1, 1, 1, 1);
			}
			else if(count + 1 == maxCount)
			{
				flash = 0.5f;
				if(red >= 0)
					gl.Color4f((1 + red) / 2, (1 + green) / 2, (1 + blue) / 2,
							   alpha);
			}
			else if(count + 2 == maxCount)
			{
				flash = 0.25f;
				if(red >= 0)
					gl.Color4f(red, green, blue, alpha);
			}
			else if(count + 3 == maxCount)
			{
				flash = 0.12f;
				if(red >= 0)
					gl.Color4f(red, green, blue, alpha);
			}
			else if(count > maxCount)
			{
				break;
			}
			if(!c)
				break;
			if(c == '\n')
			{
				cx = x;
				cy += 12;
				continue;
			}

			c = toupper(c) - HU_FONTSTART;
			if(c < 0 || c >= HU_FONTSIZE)
			{
				cx += 4;
				continue;
			}

			w = SHORT(font[c].width);
			h = SHORT(font[c].height);

			if(!font[c].lump){
				// crap. a character we don't have a patch for...?!
				continue;
			}

			if(pass)
			{
				// The character itself.
				GL_DrawPatch_CS(cx, cy + yoff, font[c].lump);

				// Do something flashy!
				if(flash > 0)
				{
					M_LetterFlash(cx, cy + yoff, w, h, true, fr, fg, fb, flash * fa);
				}
			}
			else if(cfg.menuShadow > 0)
			{
				// Shadow.
				M_LetterFlash(cx, cy + yoff, w, h, false, 1, 1, 1,
							  (red <
							   0 ? gl.GetInteger(DGL_A) /
							   255.0f : alpha) * cfg.menuShadow);
			}

			cx += w;
		}
	}
}

/*
 *  WI_DrawPatch
 *		This routine tests for a string-replacement for the patch. If one is
 *		found, it's used instead of the original graphic. 
 *
 *		If the patch is not in an IWAD, it won't be replaced!
 */
void WI_DrawPatch(int x, int y, float r, float g, float b, float a, int lump)
{
	char    def[80], *string;
	const char *name = W_LumpName(lump);

	// "{fontb; r=0.5; g=1; b=0; x=2; y=-2}This is good!"

	strcpy(def, "Patch Replacement|");
	strcat(def, name);

	if(!cfg.usePatchReplacement || !W_IsFromIWAD(lump) ||
	   !Def_Get(DD_DEF_VALUE, def, &string))
	{
		// Replacement string not found

		gl.Color4f(r, g, b, a);
		GL_DrawPatch_CS(x, y, lump);

		return;
	}

	WI_DrawParamText(x, y, string, hu_font_b, 1, 0, 0, a, false, false, ALIGN_LEFT);
}

/*
 *  Draw_BeginZoom
 */
void Draw_BeginZoom(float s, float originX, float originY)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();

    gl.Translatef(originX, originY, 0);
    gl.Scalef(s, s, 1);
    gl.Translatef(-originX, -originY, 0);
}

/*
 *  Draw_EndZoom
 */
void Draw_EndZoom(void)
{
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}
