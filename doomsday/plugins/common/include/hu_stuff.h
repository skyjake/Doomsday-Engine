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

#ifndef __COMMON_HU_STUFF_H__
#define __COMMON_HU_STUFF_H__

#include "doomsday.h"

#ifdef __JDOOM__
# include "d_event.h"
# include "r_defs.h"
#elif __JHERETIC__
# include "jHeretic/h_event.h"
# include "jHeretic/r_defs.h"
#elif __JHEXEN__
//#include "jHexen/mn_def.h"

#elif __JSTRIFE__
# include "jStrife/mn_def.h"
#endif

enum {
    ALIGN_LEFT = 0,
    ALIGN_CENTER,
    ALIGN_RIGHT
};

//
// Globally visible constants.
//
#define HU_FONTSTART    '!'    // the first font characters
#define HU_FONTEND  '_'    // the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE (HU_FONTEND - HU_FONTSTART + 1)

//
// HEADS UP TEXT
//

// A combination of patch data and its lump number.
typedef struct dpatch_s {
    int             width, height;
    int             leftoffset, topoffset;
    int             lump;
} dpatch_t;

// The fonts.
extern dpatch_t hu_font[HU_FONTSIZE];
extern dpatch_t hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];

void    HU_Init(void);
void    HU_Ticker(void);

void    R_CachePatch(dpatch_t * dp, char *name);

// Implements patch replacement.
void    WI_DrawPatch(int x, int y, float r, float g, float b, float a,
                     int lump, char *altstring, boolean builtin, int halign);

void    WI_DrawParamText(int x, int y, char *string, dpatch_t * defFont,
                         float defRed, float defGreen, float defBlue,
                         float defAlpha, boolean defCase, boolean defTypeIn,
                         int halign);

int     M_DrawText(int x, int y, boolean direct, char *string);

int     M_StringWidth(char *string, dpatch_t * font);
int     M_StringHeight(char *string, dpatch_t * font);

void Draw_BeginZoom(float s, float originX, float originY);
void Draw_EndZoom(void);

#define HU_BROADCAST    5

#define HU_TITLEX    0
#define HU_TITLEY    (167 - SHORT(hu_font[0].height))

extern boolean  message_noecho;

#ifdef __JDOOM__
// Plutonia and TNT map names.
extern char    *mapnamesp[32], *mapnamest[32];

#endif

void        HU_Start(void);
void        HU_UnloadData(void);
boolean     HU_Responder(event_t *ev);
void        HU_Drawer(void);
char        HU_dequeueChatChar(void);
void        HU_Erase(void);

#endif
