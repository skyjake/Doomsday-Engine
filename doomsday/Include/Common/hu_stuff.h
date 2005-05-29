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
// DESCRIPTION:  Head up display
//
//-----------------------------------------------------------------------------

#ifndef __HU_STUFF_H__
#define __HU_STUFF_H__

#ifdef __JDOOM__
#include "d_event.h"
#include "r_defs.h"

#elif __JHERETIC__
#include "jHeretic/Doomdef.h"

#elif __JHEXEN__
#include "jHexen/mn_def.h"

#elif __JSTRIFE__
#include "jStrife/mn_def.h"

#endif

enum {
	ALIGN_LEFT = 0,
	ALIGN_CENTER,
	ALIGN_RIGHT
};

//
// Globally visible constants.
//
#define HU_FONTSTART	'!'		   // the first font characters
#define HU_FONTEND	'_'			   // the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE	(HU_FONTEND - HU_FONTSTART + 1)

//
// HEADS UP TEXT
//

// The fonts.
extern dpatch_t hu_font[HU_FONTSIZE];
extern dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

void            HU_Init(void);
void            HU_Ticker(void);

void            R_CachePatch(dpatch_t * dp, char *name);

// Implements patch replacement.
void            WI_DrawPatch(int x, int y, float r, float g, float b, float a, int lump);

void            WI_DrawParamText(int x, int y, char *string, dpatch_t * defFont, float defRed,
								 float defGreen, float defBlue, float defAlpha,
								 boolean defCase, boolean defTypeIn, int halign);
int 	M_StringWidth(char *string, dpatch_t * font);
int 	M_StringHeight(char *string, dpatch_t * font);

void Draw_BeginZoom(float s, float originX, float originY);
void Draw_EndZoom(void);

#define HU_BROADCAST	5

#define HU_TITLEX	0
#define HU_TITLEY	(167 - SHORT(hu_font[0].height))

extern boolean  message_noecho;

#ifdef __JDOOM__
// Plutonia and TNT map names.
extern char    *mapnamesp[32], *mapnamest[32];

#define PLUT_AUTHOR	"Dario Casali and Milo Casali"
#define TNT_AUTHOR	"Team TNT"

#endif

void            HU_Start(void);
boolean         HU_Responder(event_t *ev);
void            HU_Drawer(void);
char            HU_dequeueChatChar(void);
void            HU_Erase(void);

#endif
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.1  2005/05/29 05:21:04  danij
// Commonised HUD widget code.
//
// Revision 1.6  2005/02/25 12:18:32  DaniJ
// Compiles for jDoom/jHeretic/jHexen
//
// Revision 1.5  2004/05/29 18:19:58  skyjake
// Refined indentation style
//
// Revision 1.4  2004/05/29 09:53:11  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.3  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.1.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.1.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.1  2003/02/26 19:18:28  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:12  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------
