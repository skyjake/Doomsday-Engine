/** @file mobj.h  Common playsim map object (mobj) functionality.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MOBJ
#define LIBCOMMON_MOBJ

#include "g_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handles the stopping of mobj movement. Also stops player walking animation.
 *
 * @param mobj  Mobj instance.
 */
void Mobj_XYMoveStopping(mobj_t *mobj);

/**
 * Checks if @a thing is a clmobj of one of the players.
 *
 * @param mobj  Mobj instance.
 */
dd_bool Mobj_IsPlayerClMobj(mobj_t *mobj);

/**
 * Determines if a mobj is a player mobj. It could still be a voodoo doll, also.
 *
 * @param mobj  Mobj instance.
 *
 * @return @c true, iff the mobj is a player.
 */
dd_bool Mobj_IsPlayer(mobj_t const *mobj);

/**
 * Determines if a mobj is a voodoo doll.
 *
 * @param mobj  Mobj instance.
 *
 * @return @c true, iff the mobj is a voodoo doll.
 */
dd_bool Mobj_IsVoodooDoll(mobj_t const *mobj);

/**
 * @param mobj       Mobj instance.
 * @param allAround  @c false= only look 180 degrees in front.
 *
 * @return  @c true iff a player was targeted.
 */
dd_bool Mobj_LookForPlayers(mobj_t *mobj, dd_bool allAround);

/**
 * @param mobj      Mobj instance.
 * @param stateNum  Unique identifier of the state to change to.
 *
 * @return  @c true, if the mobj is still present.
 */
dd_bool P_MobjChangeState(mobj_t *mobj, statenum_t stateNum);

/**
 * Same as P_MobjChangeState but does not call action functions.
 *
 * @param mobj      Mobj instance.
 * @param stateNum  Unique identifier of the state to change to.
 *
 * @return  @c true, if the mobj is still present.
 */
dd_bool P_MobjChangeStateNoAction(mobj_t *mobj, statenum_t stateNum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_MOBJ
