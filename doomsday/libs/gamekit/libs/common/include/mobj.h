/** @file mobj.h  Common playsim map object (mobj) functionality.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBCOMMON_MOBJ_H
#define LIBCOMMON_MOBJ_H

#include "common.h"

#ifdef __cplusplus

#include <de/string.h>

extern "C" {
#endif

/**
 * Returns the private internal ID of a map object. This is a unique 32-bit number
 * that the engine chooses internally for identifying the object.
 *
 * @param mob  Map object.
 * @return Private ID.
 */
uint32_t Mobj_PrivateID(const mobj_t *mob);

/**
 * Determines the current friction affecting @a mo, given the sector it is in and
 * whether it is on the floor.
 *
 * @param mob  Map-object instance.
 *
 * @return Friction factor to apply to the momentum.
 */
coord_t Mobj_Friction(const mobj_t *mob);

/**
 * Calculates the thrust multiplier for the mobj, to be applied when the mobj momentum
 * changes under thrust. The multiplier adjusts thrust behavior to account for slippery
 * or sticky surfaces.
 *
 * @param mob  Map-object instance.
 *
 * @return Thrust multiplier.
 */
coord_t Mobj_ThrustMul(const mobj_t *mob);

/**
 * Calculates the mobj thrust multiplier given a certain friction. The thrust multiplier
 * counters the effect of increased friction to retain normal thrust behavior.
 *
 * @param friction  Friction factor.
 *
 * @return Thrust factor to account for amount of friction.
 */
coord_t Mobj_ThrustMulForFriction(coord_t friction);

/**
 * Handles the stopping of mobj movement. Also stops player walking animation.
 *
 * @param mob  Map-object instance.
 */
void Mobj_XYMoveStopping(mobj_t *mob);

/**
 * Checks if @a thing is a clmobj of one of the players.
 *
 * @param mob  Map-object instance.
 */
dd_bool Mobj_IsPlayerClMobj(mobj_t *mob);

/**
 * Determines if a mobj is a player mobj. It could still be a voodoo doll, also.
 *
 * @param mob  Map-object instance.
 *
 * @return @c true iff the mobj is a player.
 */
dd_bool Mobj_IsPlayer(const mobj_t *mob);

/**
 * Determines if a map-object is a voodoo doll.
 *
 * @param mob  Map-object instance.
 *
 * @return  @c true if the map-object is a voodoo doll.
 */
dd_bool Mobj_IsVoodooDoll(const mobj_t *mob);

/**
 * Determines if the map-object is currently in mid air (i.e., not touching the floor
 * or, stood on some other object).
 *
 * @param mob  Map-object instance.
 *
 * @return  @c true if the map-object is considered to be airborne/flying.
 */
dd_bool Mobj_IsAirborne(const mobj_t *mob);

/**
 * Determines the world space angle between @em this map-object and the given @a point.
 *
 * @param mob            Map-object instance.
 * @param point          World space point being aimed at.
 * @param pointShadowed  @c true= @a point is considered "shadowed", meaning that
 *                       the final angle should include some random variance to
 *                       simulate inaccuracy (e.g., the partial-invisibility sphere
 *                       in DOOM makes the player harder to aim at).
 *
 * @return  The final angle of aim.
 */
angle_t Mobj_AimAtPoint2(mobj_t *mob, coord_t const point[3], dd_bool pointShadowed);
angle_t Mobj_AimAtPoint (mobj_t *mob, coord_t const point[3]/*, dd_bool pointShadowed = false*/);

/**
 * Behavior akin to @ref Mobj_AimAtPoint() however the parameters of the point to
 * be aimed at is taken from the currently targeted map-object. If no map-object
 * is presently targeted then @em this map-object's own angle is returned instead
 * (i.e., aim in the direction currently faced).
 *
 * @param mob  Map-object instance.
 *
 * @return  The final angle of aim.
 */
angle_t Mobj_AimAtTarget(mobj_t *mob);

/**
 * @param mob        Map-object instance.
 * @param allAround  @c false= only look 180 degrees in front.
 *
 * @return  @c true iff a player was targeted.
 */
dd_bool Mobj_LookForPlayers(mobj_t *mob, dd_bool allAround);

/**
 * @param mob       Map-object instance.
 * @param stateNum  Unique identifier of the state to change to.
 *
 * @return  @c true if the mobj is still present.
 */
dd_bool P_MobjChangeState(mobj_t *mob, statenum_t stateNum);

/**
 * Same as P_MobjChangeState but does not call action functions.
 *
 * @param mob       Map-object instance.
 * @param stateNum  Unique identifier of the state to change to.
 *
 * @return  @c true if the mobj is still present.
 */
dd_bool P_MobjChangeStateNoAction(mobj_t *mob, statenum_t stateNum);

/**
 * Check whether the mobj is currently obstructed and explode immediately if so.
 *
 * @return  Pointer to @em this mobj iff it survived, otherwise @c nullptr.
 */
mobj_t *Mobj_ExplodeIfObstructed(mobj_t *mob);

/**
 * Launch the given map-object @a missile (if any) at the specified @a angle.
 *
 * @param missile    Map-object to be launched.
 * @param angle      World space angle at which to launch.
 * @param targetPos  World space point being targeted (for determining speed).
 * @param sourcePos  World space point to use as the source (for determining
 *                   speed). Can be @c nullptr in which case the origin coords
 *                   of @a missile are used instead.
 * @param extraMomZ  Additional momentum to apply to the missile.
 *
 * @return  Same as @a missile, for caller convenience.
 */
mobj_t *P_LaunchMissile(mobj_t *missile, angle_t angle, coord_t const targetPos[3],
    coord_t const sourcePos[3], coord_t extraMomZ);

/**
 * Launch the given map-object @a missile (if any) at the specified @a angle,
 * enqueuing a new launch sound and recording @em this map-object as the source.
 *
 * @param mob        Map-object hurler of @a missile.
 * @param missile    Map-object to be launched.
 * @param angle      World space angle at which to launch.
 * @param targetPos  World space point being targeted (for determining speed).
 * @param sourcePos  World space point to use as the source (for determining
 *                   speed). Can be @c nullptr in which case the origin coords
 *                   of @a missile are used instead.
 * @param extraMomZ  Additional momentum to apply to the missile.
 *
 * @return  Same as @a missile, for caller convenience.
 *
 * @see P_LaunchMissile()
 */
mobj_t *Mobj_LaunchMissileAtAngle2(mobj_t *mob, mobj_t *missile, angle_t angle, coord_t const targetPos[3], coord_t const sourcePos[3], coord_t extraMomZ);
mobj_t *Mobj_LaunchMissileAtAngle (mobj_t *mob, mobj_t *missile, angle_t angle, coord_t const targetPos[3], coord_t const sourcePos[3]/*, coord_t extraMomZ = 0*/);

/**
 * Same as @ref Mobj_LaunchMissileAtAngle() except the angle is that which the
 * @a missile is presently facing.
 *
 * @param mob        Map-object hurler of @a missile.
 * @param missile    Map-object to be launched.
 * @param targetPos  World space point being targeted (for determining speed).
 * @param sourcePos  World space point to use as the source (for determining
 *                   speed). Can be @c nullptr in which case the origin coords
 *                   of @a missile are used instead.
 * @param extraMomZ  Additional momentum to apply to the missile.
 *
 * @return  Same as @a missile, for caller convenience.
 */
mobj_t *Mobj_LaunchMissile2(mobj_t *mob, mobj_t *missile, coord_t const targetPos[3], coord_t const sourcePos[3], coord_t extraMomZ);
mobj_t *Mobj_LaunchMissile (mobj_t *mob, mobj_t *missile, coord_t const targetPos[3], coord_t const sourcePos[3]/*, coord_t extraMomZ = 0*/);

void Mobj_InflictDamage(mobj_t *mob, const mobj_t *inflictor, int damage);

enum mobjtouchresult_e {
    MTR_UNDEFINED,
    MTR_KEEP,
    MTR_MAKE_DORMANT,
    MTR_HIDE,
    MTR_DESTROY
};

void Mobj_RunScriptOnDeath(mobj_t *mob, mobj_t *killer);

/**
 * Called when @a toucher comes into contact with special thing @a special that has a
 * custom scripted touch action.
 *
 * @param mob      Thing doing the touching.
 * @param special  Special thing being touched.
 * @param result   What to do with the special thing afterwards.
 *
 * @return @c true if a scripted action is defined for the object. In this case, @a result
 * will contain a valid value afterwards.
 */
dd_bool Mobj_RunScriptOnTouch(mobj_t *mob, mobj_t *special, enum mobjtouchresult_e *result);

#ifdef __cplusplus
}  // extern "C"

/**
 * Describe the object in plain text Info syntax.
 * @param mob  Map object.
 * @return Description text.
 */
de::String Mobj_StateAsInfo(const mobj_t *mob);

void Mobj_RestoreObjectState(mobj_t *mob, const de::Info::BlockElement &state);

#endif

#endif  // LIBCOMMON_MOBJ_H
