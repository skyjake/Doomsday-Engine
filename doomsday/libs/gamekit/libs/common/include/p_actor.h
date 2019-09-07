/** @file p_actor.h Common code relating to mobj management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Removes the given mobj from the world.
 *
 * @param mo            The mobj to be removed.
 * @param noRespawn     Disable the automatical respawn which occurs
 *                      with mobjs of certain type(s) (also dependant on
 *                      the current gamemode).
 *                      Generally this should be @c false.
 */
void P_MobjRemove(mobj_t *mo, dd_bool noRespawn);

/**
 * To be called after a move, to link the mobj back into the world.
 *
 * @param mobj   Mobj instance.
 */
void P_MobjLink(struct mobj_s *mobj);

/**
 * Unlinks a mobj from the world so that it can be moved.
 *
 * @param mobj   Mobj instance.
 */
void P_MobjUnlink(struct mobj_s *mobj);

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVO(mobj_t *mo, coord_t stepx, coord_t stepy);

/**
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_MobjSetSRVOZ(mobj_t *mo, coord_t stepz);

/**
 * The thing's timer has run out, which means the thing has completed its
 * step. Or there has been a teleport.
 */
void P_MobjClearSRVO(mobj_t *mo);

/**
 * Turn visual angle towards real angle. An engine cvar controls whether
 * the visangle or the real angle is used in rendering.
 * Real-life analogy: angular momentum (you can't suddenly just take a
 * 90 degree turn in zero time).
 */
void P_MobjAngleSRVOTicker(mobj_t *mo);

dd_bool P_MobjIsCamera(const mobj_t *mo);

/**
 * Returns @c true iff @a mobj is currently "crunchable", i.e., it can be turned
 * into a pile of giblets if it no longer fits in the opening between floor and
 * ceiling planes.
 */
dd_bool Mobj_IsCrunchable(mobj_t *mobj);

/**
 * Returns @c true iff @a mobj is a dropped item.
 */
dd_bool Mobj_IsDroppedItem(mobj_t *mobj);

/**
 * Returns the terraintype_t of the floor plane at the mobj's origin.
 *
 * @param mobj  Mobj instance.
 */
const terraintype_t *P_MobjFloorTerrain(const mobj_t *mobj);

/**
 * The first three bits of the selector special byte contain a relative
 * health level.
 */
void P_UpdateHealthBits(mobj_t *mo);

/**
 * Given a mobjtype, lookup the statenum associated to the named state.
 *
 * @param mobjType  Type of mobj.
 * @param name      State name identifier.
 *
 * @return  Statenum of the associated state ELSE @c, S_NULL.
 */
statenum_t P_GetState(mobjtype_t mobjType, statename_t name);

void P_ProcessDeferredSpawns(void);
void P_PurgeDeferredSpawns(void);

/**
 * Deferred mobj spawning until at least @a minTics have passed.
 * Spawn behavior is otherwise exactly the same as an immediate spawn.
 */
void P_DeferSpawnMobj3f(int minTics, mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle, int spawnFlags, void (*callback) (mobj_t *mo, void *context), void *context);
void P_DeferSpawnMobj3fv(int minTics, mobjtype_t type, coord_t const pos[3], angle_t angle, int spawnFlags, void (*callback) (mobj_t *mo, void *context), void *context);

#ifdef __JHEXEN__

void P_CreateTIDList(void);

void P_MobjRemoveFromTIDList(mobj_t *mo);

void P_MobjInsertIntoTIDList(mobj_t *mo, int tid);

mobj_t *P_FindMobjFromTID(int tid, int *searchPosition);

#endif // __JHEXEN__

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_P_ACTOR_H
