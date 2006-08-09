/* $Id: mn_def.h 3318 2006-06-13 02:18:47Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Menu defines and types.
 */

#ifndef __MENU_DEFS_H_
#define __MENU_DEFS_H_

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "m_menu.h"

// Macros

#define LEFT_DIR        0
#define RIGHT_DIR       1
#define DIR_MASK        0x1
#define ITEM_HEIGHT     20
#define SLOTTEXTLEN     16
#define ASCII_CURSOR    '['

#define LINEHEIGHT      16
#define LINEHEIGHT_B    15
#define LINEHEIGHT_A    8

#define SKULLXOFF       -28
#define SKULLYOFF       -1
#define SKULLW          20
#define SKULLH          19
#define CURSORPREF      "M_SKULL%d"
#define NUMCURSORPATCHES 2
#define SKULLBASELMP    "M_SKL00"
#define SAVEGAME_BOX_YOFFSET 5

#define NUMSAVESLOTS    8
#define SAVESTRINGSIZE  24

#define MAX_EDIT_LEN    256

// Types

typedef struct {
    char    text[MAX_EDIT_LEN];
    char    oldtext[MAX_EDIT_LEN];  // If the current edit is canceled...
    int     firstVisible;
} EditField_t;

// Color widget.
void    DrawColorWidget();
void    SCColorWidget(menuobject_t *obj, int option, void *data);
void    M_WGCurrentColor(menuobject_t *obj, int option, void *data);

void    M_DrawSaveLoadBorder(int x, int y);
void    M_WriteMenuText(const menupage_t* menu, int index, char *text);
void    M_DrawThermo(int x, int y, int thermWidth, int thermDot);
void    M_DrawColorBox(const menupage_t *menu, int index, float r, float g, float b, float a);
int     M_StringWidth(char *string, dpatch_t * font);
int     M_StringHeight(char *string, dpatch_t * font);
void    M_StartControlPanel(void);
void    M_StartMessage(char *string, void *routine, boolean input);
void    M_StopMessage(void);
void    M_FloatMod10(float *variable, int option);


void    SCEnterMultiplayerMenu(menuobject_t *obj, int option, void *data);

void    MN_TickerEx(void); // The extended ticker.

#endif
