/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * st_stuff.h:
 */

#ifndef __ST_STUFF_H__
#define __ST_STUFF_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "x_player.h"
#include "x_config.h"

// Size of statusbar.
#define ST_HEIGHT           (42*SCREEN_MUL)
#define ST_WIDTH            (SCREENWIDTH)
#define ST_Y                (SCREENHEIGHT - ST_HEIGHT)

// Called by startup code.
void        ST_Register(void);
void        ST_Init(void);

// Called by main loop.
void        ST_Ticker(void);

void        ST_Start(int player);
void        ST_Stop(int player);

void        ST_updateGraphics(void);

// Called when it might be neccessary for the hud to unhide.
void        ST_HUDUnHide(int player, hueevent_t ev);

// Called in P_inter & P_enemy
void        ST_doPaletteStuff(int player, boolean forceChange);

void        ST_Init(void);
boolean     ST_Responder(event_t* ev);
void        ST_Ticker(void);
void        ST_Drawer(int player, int fullscreenMode, boolean refresh);
void        ST_PaletteFlash(boolean forceChange);

void        ST_FlashCurrentItem(int player);

#endif
