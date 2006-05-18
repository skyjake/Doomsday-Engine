/* $Id$
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
 * Menu widget stuff, episode selection and such.
 */

#ifndef __M_MENU__
#define __M_MENU__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "hu_stuff.h"
#include "d_event.h"

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
boolean         M_Responder(event_t *ev);

// Called by Init
// registers all the CCmds and CVars for the menu
void        MN_Register(void);

// Called by main loop,
// only used for menu (skull cursor) animation.
// and menu fog, fading in and out...
void            MN_Ticker(void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void            M_Drawer(void);

// Called by D_DoomMain,
// loads the config file.
void            MN_Init(void);
void            M_LoadData(void);
void            M_UnloadData(void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void            M_StartControlPanel(void);
void            M_ClearMenus(void);

void            M_StartMessage(char *string, void *routine, boolean input);
void M_StopMessage(void);

void            M_WriteText2(int x, int y, char *string, dpatch_t *font,
                             float red, float green, float blue, float alpha);
void            M_WriteText3(int x, int y, const char *string, dpatch_t *font,
                             float red, float green, float blue, float alpha,
                             boolean doTypeIn, int initialCount);
DEFCC(CCmdMenuAction);
DEFCC(CCmdMsgResponse);

#endif
