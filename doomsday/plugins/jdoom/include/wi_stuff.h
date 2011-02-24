/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * wi_stuff.h: Intermission screens
 */

#ifndef __WI_STUFF__
#define __WI_STUFF__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_player.h"

// States for the intermission

typedef enum {
    ILS_NONE = -1,
    ILS_SHOW_STATS,
    ILS_SHOW_NEXTMAP
} interludestate_t;

// Setup for an intermission screen.
void            WI_Init(wbstartstruct_t *wbstartstruct);

void            WI_SetState(interludestate_t st);
void            WI_End(void);

// Called by main loop, animate the intermission.
void            WI_Ticker(void);

// Draw the intermission screen.
void            WI_Drawer(void);

#endif
