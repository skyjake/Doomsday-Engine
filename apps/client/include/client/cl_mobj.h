/** @file cl_mobj.h  Clientside map objects.
 * @ingroup client
 *
 * @author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_WORLD_MOBJ_H
#define DE_CLIENT_WORLD_MOBJ_H

#include "world/p_object.h"
#include "world/clientmobjthinkerdata.h"

/// Asserts that a given mobj is a client mobj.
#define CL_ASSERT_CLMOBJ(mo)    DE_ASSERT(Cl_IsClientMobj(mo));

/**
 * Make the real player mobj identical with the client mobj.
 * The client mobj is always unlinked. Only the *real* mobj is visible.
 * (The real mobj was created by the Game.)
 */
void Cl_UpdateRealPlayerMobj(mobj_t *localMobj, mobj_t *remoteClientMobj, int flags,
                             dd_bool onFloor);

ClientMobjThinkerData::RemoteSync *ClMobj_GetInfo(mobj_t *mo);

/**
 * Call for Hidden client mobjs to make then visible.
 * If a sound is waiting, it's now played.
 *
 * @return  @c true, if the mobj was revealed.
 */
dd_bool ClMobj_Reveal(mobj_t *cmo);

/**
 * Links the mobj into sectorlinks and if the object is solid, the
 * blockmap. Linking to sectorlinks makes the mobj visible and linking
 * to the blockmap makes it possible to interact with it (collide).
 * If the client mobj is Hidden, it will not be linked anywhere.
 */
void ClMobj_Link(mobj_t *cmo); // needed?

/**
 * Change the state of a mobj.
 *
 * @todo  Should use the gameside function for this?
 */
void ClMobj_SetState(mobj_t *mo, int stnum); // needed?

/**
 * Reads a single mobj delta (inside PSV_FRAME2 packet) from the message buffer
 * and applies it to the client mobj in question.
 *
 * For client mobjs that belong to players, updates the real player mobj
 * accordingly.
 */
void ClMobj_ReadDelta();

/**
 * Null mobjs deltas have their own type in a PSV_FRAME2 packet.
 * Here we remove the mobj in question.
 */
void ClMobj_ReadNullDelta();

/**
 * Determines whether a mobj is a client mobj.
 *
 * @param mo  Mobj to check.
 *
 * @return  @c true, if the mobj is a client mobj; otherwise @c false.
 */
dd_bool Cl_IsClientMobj(const mobj_t *mo);

#endif // DE_CLIENT_WORLD_MOBJ_H
