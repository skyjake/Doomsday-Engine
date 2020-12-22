/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * p_spec.h: Implements special effects:
 *
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing, or shooting
 * special lines, or by timed thinkers.
 */

#ifndef __P_SPEC_H__
#define __P_SPEC_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_player.h"
#include "r_data.h"

#define MO_TELEPORTMAN          14

#ifdef __cplusplus
extern "C" {
#endif

// at game start
void            P_InitLava(void);

// at map load
void P_SpawnSectorSpecialThinkers(void);
void P_SpawnLineSpecialThinkers(void);
void P_SpawnAllSpecialThinkers(void);

void P_InitAmbientSound(void);

void P_DefineAmbientSfx(int sequence, const int *seq, size_t count);

/**
 * Called by spawnMapThing during @ref P_SetupMap()
 */
void P_AddAmbientSfx(int sequence);

/**
 * Called every tic by (P_tick):P_Ticker.
 */
void P_AmbientSound(void);

dd_bool         P_ActivateLine(Line* ld, mobj_t* mo, int side,
                               int activationType);

void            P_PlayerInSpecialSector(player_t* player);

void            P_PlayerInWindSector(player_t* player);

int             EV_DoDonut(Line* line);

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

typedef enum {
    build8, // Slowly build by 8.
    build16 // Slowly build by 16.
} stair_e;

result_e        T_MovePlane(Sector* sector, float speed, coord_t dest,
                            int crush, int floorOrCeiling, int direction);

int             EV_BuildStairs(Line* line, stair_e type);

dd_bool         P_UseSpecialLine2(mobj_t* mo, Line* line, int side);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
