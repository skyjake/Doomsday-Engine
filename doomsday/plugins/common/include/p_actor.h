/**
 * @file p_actor.h
 * Common code relating to mobj management. @ingroup libcommon
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_P_ACTOR_H
#define LIBCOMMON_P_ACTOR_H

/**
 * Removes the given mobj from the world.
 *
 * @param mo            The mobj to be removed.
 * @param noRespawn     Disable the automatical respawn which occurs
 *                      with mobjs of certain type(s) (also dependant on
 *                      the current gamemode).
 *                      Generally this should be @c false.
 */
void P_MobjRemove(mobj_t* mo, boolean noRespawn);

void P_RemoveAllPlayerMobjs(void);

/**
 * Unlinks a mobj from the world so that it can be moved.
 */
void P_MobjUnsetOrigin(mobj_t* mo);

/**
 * To be called after a move, to link the mobj back into the world.
 */
void P_MobjSetOrigin(mobj_t* mo);

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVO(mobj_t* mo, coord_t stepx, coord_t stepy);

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVOZ(mobj_t* mo, coord_t stepz);

/**
 * The thing's timer has run out, which means the thing has completed its
 * step. Or there has been a teleport.
 */
void P_MobjClearSRVO(mobj_t* mo);

/**
 * Turn visual angle towards real angle. An engine cvar controls whether
 * the visangle or the real angle is used in rendering.
 * Real-life analogy: angular momentum (you can't suddenly just take a
 * 90 degree turn in zero time).
 */
void P_MobjAngleSRVOTicker(mobj_t* mo);

boolean P_MobjIsCamera(const mobj_t* mo);

/**
 * The first three bits of the selector special byte contain a relative
 * health level.
 */
void P_UpdateHealthBits(mobj_t* mo);

/**
 * Given a mobjtype, lookup the statenum associated to the named state.
 *
 * @param mobjType      Type of mobj.
 * @param name          State name identifier.
 *
 * @return              Statenum of the associated state ELSE @c, S_NULL.
 */
statenum_t P_GetState(mobjtype_t mobjType, statename_t name);

void P_ProcessDeferredSpawns(void);
void P_PurgeDeferredSpawns(void);

/**
 * Deferred mobj spawning until at least @a minTics have passed.
 * Spawn behavior is otherwise exactly the same as an immediate spawn.
 */
void P_DeferSpawnMobj3f(int minTics, mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle,
    int spawnFlags, void (*callback) (mobj_t* mo, void* context), void* context);
void P_DeferSpawnMobj3fv(int minTics, mobjtype_t type, coord_t const pos[3], angle_t angle,
    int spawnFlags, void (*callback) (mobj_t* mo, void* context), void* context);

#endif /// LIBCOMMON_P_ACTOR_H
