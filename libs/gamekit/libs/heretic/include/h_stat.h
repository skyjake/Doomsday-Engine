/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * h_stat.h: All the global variables that store the internal state.
 *
 * Theoretically speaking, the internal state of the game can be found by
 * looking at the variables collected here, and every relevant module will
 * have to include this header file. In practice, things are a bit messy.
 */

#ifndef __JHERETIC_STATE_H__
#define __JHERETIC_STATE_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdata.h"
#include "h_player.h"
#include <doomsday/gamefw/defs.h>

// wipegamestate can be set to -1 to force a wipe on the next draw
extern gamestate_t wipeGameState;

extern int bodyQueueSlot;

#endif
