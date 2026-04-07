/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

// -------------------------
// Status flags for refresh.
//

extern dd_bool  paused;            // Game Pause?

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
DE_EXTERN_C int viewangleoffset;

// Timer, for scores.
DE_EXTERN_C int mapTime; // Tics in game play for par.

// Quit after playing a demo from cmdline.
DE_EXTERN_C dd_bool singledemo;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

//#define GAMETIC             (*((timespan_t*) DD_GetVariable(DD_GAMETIC)))

//-----------------------------------------
// Internal parameters, used for engine.
//

extern int      bodyqueslot;

#endif
