/** @file common/mobj.h Common Map Object (Mobj) functionality.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MOBJ
#define LIBCOMMON_MOBJ

#include "g_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handles the stopping of mobj movement. Also stops player walking animation.
 *
 * @param mo  Mobj.
 */
void Mobj_XYMoveStopping(mobj_t *mo);

/**
 * Checks if @a thing is a clmobj of one of the players.
 */
boolean Mobj_IsPlayerClMobj(mobj_t *mo);

/**
 * Determines if a mobj is a player mobj. It could still be a voodoo doll, also.
 * @param mo  Map object.
 * @return @c true, iff the mobj is a player.
 */
boolean Mobj_IsPlayer(mobj_t const *mo);

/**
 * Determines if a mobj is a voodoo doll.
 * @param mo  Map object.
 * @return @c true, iff the mobj is a voodoo doll.
 */
boolean Mobj_IsVoodooDoll(mobj_t const *mo);

/**
 * @param allAround  @c false= only look 180 degrees in front.
 * @return  @c true iff a player was targeted.
 */
boolean Mobj_LookForPlayers(mobj_t *mo, boolean allAround);

/**
 * Determines if it is allowed to execute the action function of @a mo.
 * @return @c true, if allowed.
 */
boolean Mobj_ActionFunctionAllowed(mobj_t *mo);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_MOBJ
