/**\file p_actor.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Common actor (mobj) routines.
 */

#ifndef LIBCOMMON_P_ACTOR_H
#define LIBCOMMON_P_ACTOR_H

void P_MobjRemove(mobj_t* mo, boolean noRespawn);

void P_MobjUnsetOrigin(mobj_t* mo);
void P_MobjSetOrigin(mobj_t* mo);

void P_MobjSetSRVO(mobj_t* mo, coord_t stepx, coord_t stepy);

void P_MobjSetSRVOZ(mobj_t* mo, coord_t stepz);

void P_MobjClearSRVO(mobj_t* mo);

void P_MobjAngleSRVOTicker(mobj_t* mo);

boolean P_MobjIsCamera(const mobj_t* mo);

void P_UpdateHealthBits(mobj_t* mo);
statenum_t P_GetState(mobjtype_t mobjType, statename_t name);

void P_ProcessDeferredSpawns(void);
void P_PurgeDeferredSpawns(void);

/**
 * Deferred mobj spawning until at least @minTics have passed.
 * Spawn behavior is otherwise exactly the same as an immediate spawn, via   * P_SpawnMobj*
 */
void P_DeferSpawnMobj3f(int minTics, mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle,
    int spawnFlags, void (*callback) (mobj_t* mo, void* context), void* context);
void P_DeferSpawnMobj3fv(int minTics, mobjtype_t type, coord_t const pos[3], angle_t angle,
    int spawnFlags, void (*callback) (mobj_t* mo, void* context), void* context);

#endif /// LIBCOMMON_P_ACTOR_H
