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
// DESCRIPTION:
//   Menu widget stuff, episode selection and such.
//    
//-----------------------------------------------------------------------------


#ifndef __M_MENU__
#define __M_MENU__


#include "r_defs.h"
#include "d_event.h"

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
boolean M_Responder (event_t *ev);


// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer (void);

// Called by D_DoomMain,
// loads the config file.
void M_Init (void);
void M_LoadData(void);
void M_UnloadData(void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel (void);
void M_ClearMenus (void);

void M_StartMessage(char *string,void *routine,boolean input);

int  M_StringWidth(char *string, dpatch_t *font);
void M_WriteText2(int x, int y, char *string, dpatch_t *font, 
				  float	red, float green, float	blue);
void M_WriteText3(int x, int y, const char *string, dpatch_t *font, 
				  float	red, float green, float	blue, 
				  boolean doTypeIn, int initialCount);

#endif    
//-----------------------------------------------------------------------------
//
// $Log$
// Revision 1.5  2004/05/28 17:16:35  skyjake
// Resolved conflicts (branch-1-7 overrides)
//
// Revision 1.3.2.1  2004/05/16 10:01:30  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.3.4.1  2003/11/19 17:08:47  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.3  2003/08/24 00:26:00  skyjake
// Modified text writing routines
//
// Revision 1.2  2003/08/17 23:29:36  skyjake
// Implemented Patch Replacement
//
// Revision 1.1  2003/02/26 19:18:31  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:04:13  Jaakko
// Added all headers
//
//
//-----------------------------------------------------------------------------

