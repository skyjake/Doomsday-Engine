/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Menu widget stuff, episode selection and such.
 */

#ifndef __M_MENU__
#define __M_MENU__

#ifndef __WOLFTC__
#  error "Using WolfTC headers without __WOLFTC__"
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
void            M_StartMenu(void);
void            M_ClearMenus(void);

void            M_StartMessage(char *string, void *routine, boolean input);
void            M_StopMessage(void);

float           MN_MenuAlpha(void);
boolean         MN_CurrentMenuHasBackground(void);

DEFCC(CCmdMenuAction);
DEFCC(CCmdMsgResponse);

#endif
