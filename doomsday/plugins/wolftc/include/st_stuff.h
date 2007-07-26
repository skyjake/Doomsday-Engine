/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * Status bar code.
 * Does the face/direction indicator animatin.
 * Does palette indicators as well (red pain/berserk, bright pickup)
 */

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

// Size of statusbar.
// Now sensitive for scaling.
#define ST_HEIGHT   32*SCREEN_MUL
#define ST_WIDTH    SCREENWIDTH
#define ST_Y        (SCREENHEIGHT - ST_HEIGHT)

//
// STATUS BAR
//

// Called by main loop.
void    ST_Ticker(void);

// Called by main loop.
void    ST_Drawer(int fullscreenmode, boolean refresh);

// Called when the console player is spawned on each level.
void    ST_Start(void);

// Called by startup code.
void    ST_Register(void);
void    ST_Init(void);

void    ST_updateGraphics(void);

// Called when it might be neccessary for the hud to unhide.
void    ST_HUDUnHide(hueevent_t event);

// States for status bar code.
typedef enum {
    AutomapState,
    FirstPersonState
} st_stateenum_t;

// States for the chat code.
typedef enum {
    StartChatState,
    WaitDestState,
    GetChatState
} st_chatstateenum_t;

int             R_GetFilterColor(int filter);

#endif
