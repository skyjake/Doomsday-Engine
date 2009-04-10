/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * x_state.h: All the global variables that store the internal state.
 */

#ifndef __X_STATE_H__
#define __X_STATE_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

// We need the playr data structure as well.
#include "x_player.h"

extern boolean paused; // Game Pause?

// Timer, for scores.
extern int mapStartTic; // Game tic at map start.

// Quit after playing a demo from cmdline.
extern boolean singledemo;
// Bookkeeping on players - state.
extern player_t players[MAXPLAYERS];

// if true, load all graphics at map load
extern boolean precache;
extern int bodyQueueSlot;
extern int verbose;

#endif
