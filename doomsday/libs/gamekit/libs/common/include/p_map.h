/** @file p_map.h Common play/maputil functions.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_P_MAP_H
#define LIBCOMMON_P_MAP_H

#include "common.h"

DE_EXTERN_C dd_bool tmFloatOk; ///< @c true= move would be ok if within "tmFloorZ - tmCeilingZ".
DE_EXTERN_C coord_t tmFloorZ;
DE_EXTERN_C coord_t tmCeilingZ;
DE_EXTERN_C dd_bool tmFellDown;
DE_EXTERN_C Line *tmCeilingLine;
DE_EXTERN_C Line *tmFloorLine;
DE_EXTERN_C Line *tmBlockingLine;
#if __JHEXEN__
DE_EXTERN_C mobj_t *tmBlockingMobj;
#endif

DE_EXTERN_C mobj_t *lineTarget; // Who got hit (or NULL).
#if __JHEXEN__
DE_EXTERN_C mobj_t *PuffSpawned;
#endif

#ifdef __cplusplus
#include <functional>
extern "C" {
#endif

/**
 * Look from eyes of the @a beholder to any part of the @a target.
 *
 * @param beholder  Mobj doing the looking.
 * @param target    Mobj being looked at.
 *
 * @return  @c true iff an unobstructed line of sight exists from @a beholder
 * to the @a target.
 */
dd_bool P_CheckSight(const mobj_t *beholder, const mobj_t *target);

/**
 * Determines the world space angle between the points @a from and @a to.
 *
 * @param from      World space vanatage point to look from.
 * @param to        World space point to look to.
 * @param shadowed  @c true= @a to is considered "shadowed", meaning that the
 *                  final angle should include some random variance to simulate
 *                  inaccuracy (e.g., the partial-invisibility sphere in DOOM makes
 *                  the player harder to aim at).
 *
 * @return  The final angle of aim.
 */
angle_t P_AimAtPoint2(coord_t const from[3], coord_t const to[3], dd_bool shadowed);
angle_t P_AimAtPoint (coord_t const from[3], coord_t const to[3]/*, dd_bool shadowed = false*/);

/**
 * This is purely informative, nothing is modified (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP early out on solid lines?
 *
 * out:
 *  tmFloorZ
 *  tmCeilingZ
 *  tmDropoffZ - the lowest point contacted (monsters won't move to a drop off)
 */
dd_bool P_CheckPositionXYZ(mobj_t *thing, coord_t x, coord_t y, coord_t z);
dd_bool P_CheckPosition(mobj_t *thing, coord_t const pos[3]);

dd_bool P_CheckPositionXY(mobj_t *thing, coord_t x, coord_t y);

/**
 * Source is the creature that caused the explosion at @a bomb.
 *
 * @param afflictSource  @c true= the @a source is not immune to damage.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t *bomb, mobj_t *source, int damage, int distance, dd_bool afflictSource);
#else
void P_RadiusAttack(mobj_t *bomb, mobj_t *source, int damage, int distance);
#endif

/**
 * Attempts to move a mobj to a new 3D position, crossing special lines
 * and picking up things.
 *
 * @note  This function is exported from the game plugin.
 *
 * @param mobj  Mobj to move.
 * @param x     New X coordinate.
 * @param y     New Y coordinate.
 * @param z     New Z coordinate.
 *
 * @return  @c true iff the move was successful.
 */
dd_bool P_TryMoveXYZ(mobj_t *mobj, coord_t x, coord_t y, coord_t z);

#if !__JHEXEN__
dd_bool P_TryMoveXY(mobj_t *thing, coord_t x, coord_t y, dd_bool dropoff, dd_bool slide);
#else
dd_bool P_TryMoveXY(mobj_t *thing, coord_t x, coord_t y);
#endif

/**
 * Kills anything occupying the position.
 *
 * @return  @c true iff the move was successful.
 */
dd_bool P_TeleportMove(mobj_t *thing, coord_t x, coord_t y, dd_bool alwaysStomp);

void P_Telefrag(mobj_t *thing);

void P_TelefragMobjsTouchingPlayers(void);

/**
 * @todo The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess.
 *
 * @param mo  The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t *mo);

/**
 * Looks for special lines in front of the player to activate.
 *
 * @param player  The player to test.
 */
void P_UseLines(player_t *player);

/**
 * @param sector  The sector to check.
 * @param crush   Hexen: amount of crush damage to apply.
 *                Other games: apply fixed crush damage if @c > 0.
 */
dd_bool P_ChangeSector(Sector *sector, int crush);

/**
 * This is called by the engine when it needs to change sector heights without
 * consulting game logic first. Most commonly this occurs on clientside, where
 * the client needs to apply plane height changes as per the deltas.
 *
 * @param sectorIdx  Index of the sector to update.
 */
void P_HandleSectorHeightChange(int sectorIdx);

float P_AimLineAttack(mobj_t *t1, angle_t angle, coord_t distance);

/**
 * @param damage    @c 0= Perform a test trace that will leave lineTarget set.
 */
void P_LineAttack(mobj_t *t1, angle_t angle, coord_t distance, coord_t slope,
    int damage, mobjtype_t puffType);

coord_t P_GetGravity(void);

/**
 * This routine checks for Lost Souls trying to be spawned across 1-sided
 * lines, impassible lines, or "monsters can't cross" lines.
 *
 * Draw an imaginary line between the PE and the new Lost Soul spawn spot.
 * If that line crosses a 'blocking' line, then disallow the spawn. Only
 * search lines in the blocks of the blockmap where the bounding box of the
 * trajectory line resides. Then check bounding box of the trajectory vs
 * the bounding box of each blocking line to see if the trajectory and the
 * blocking line cross. Then check the PE and LS to see if they are on
 * different sides of the blocking line. If so, return true otherwise
 * false.
 */
dd_bool P_CheckSides(mobj_t *actor, coord_t x, coord_t y);

#ifdef __cplusplus
int P_IterateThinkers(thinkfunc_t func, const std::function<int(thinker_t *)> &);
#endif

#if __JHERETIC__ || __JHEXEN__
/**
 * @param mo  The mobj whoose position to test.
 * @return dd_bool  @c true iff the mobj is not blocked by anything.
 */
dd_bool P_TestMobjLocation(mobj_t *mobj);

dd_bool P_BounceWall(mobj_t *mobj);

#endif

#if __JHEXEN__

mobj_t *P_CheckOnMobj(mobj_t *mobj);

/**
 * Stomp on any mobjs contacting the specified @a mobj.
 */
void P_ThrustSpike(mobj_t *mobj);

/**
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player    The player using the puzzle item.
 * @param itemType  The type of item to try to use.
 * @return dd_bool  true if the puzzle item was used.
 */
dd_bool P_UsePuzzleItem(player_t *player, int itemType);

/**
 * Count mobjs in the current map which meet the specified criteria.
 *
 * @param type  Mobj type.
 * @param tid   Thinker id.
 */
int P_MobjCount(int type, int tid);

#endif // __JHEXEN__

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_P_MAP_H
