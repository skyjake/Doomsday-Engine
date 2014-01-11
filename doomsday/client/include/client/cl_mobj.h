/** @file cl_mobj.h  Clientside map objects.
 * @ingroup client
 *
 * @author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_WORLD_MOBJ_H
#define DENG_CLIENT_WORLD_MOBJ_H

#include "world/p_object.h"

/**
 * @defgroup clMobjFlags Client Mobj Flags
 * @ingroup flags
 */
///@{
#define CLMF_HIDDEN         0x01 ///< Not officially created yet
#define CLMF_UNPREDICTABLE  0x02 ///< Temporarily hidden (until next delta)
#define CLMF_SOUND          0x04 ///< Sound is queued for playing on unhide.
#define CLMF_NULLED         0x08 ///< Once nulled, it can't be updated.
#define CLMF_STICK_FLOOR    0x10 ///< Mobj will stick to the floor.
#define CLMF_STICK_CEILING  0x20 ///< Mobj will stick to the ceiling.
#define CLMF_LOCAL_ACTIONS  0x40 ///< Allow local action execution.
///@}

// Clmobj knowledge flags. This keeps track of the information that has been
// received.
#define CLMF_KNOWN_X        0x10000
#define CLMF_KNOWN_Y        0x20000
#define CLMF_KNOWN_Z        0x40000
#define CLMF_KNOWN_STATE    0x80000
#define CLMF_KNOWN          0xf0000 ///< combination of all the KNOWN-flags

// Magic number for client mobj information.
#define CLM_MAGIC1          0xdecafed1
#define CLM_MAGIC2          0xcafedeb8

/// Asserts that a given mobj is a client mobj.
#define CL_ASSERT_CLMOBJ(mo)    assert(Cl_IsClientMobj(mo));

/**
 * Information about a client mobj. This structure is attached to
 * gameside mobjs. The last 4 bytes must be CLM_MAGIC.
 */
struct ClMobjInfo
{
    uint startMagic; // The client mobj magic number (CLM_MAGIC1).
    ClMobjInfo *next, *prev;
    int flags;
    uint time; // Time of last update.
    int sound; // Queued sound ID.
    float volume; // Volume for queued sound.
    uint endMagic; // The client mobj magic number (CLM_MAGIC2).

    ClMobjInfo();
};

/**
 * Make the real player mobj identical with the client mobj.
 * The client mobj is always unlinked. Only the *real* mobj is visible.
 * (The real mobj was created by the Game.)
 */
void Cl_UpdateRealPlayerMobj(mobj_t *localMobj, mobj_t *remoteClientMobj, int flags,
                             dd_bool onFloor);

ClMobjInfo *ClMobj_GetInfo(mobj_t *mo);

mobj_t *ClMobj_MobjForInfo(ClMobjInfo *info);

/**
 * Call for Hidden client mobjs to make then visible.
 * If a sound is waiting, it's now played.
 *
 * @return  @c true, if the mobj was revealed.
 */
dd_bool ClMobj_Reveal(mobj_t *cmo);

/**
 * Unlinks the mobj from sectorlinks and if the object is solid,
 * the blockmap.
 */
void ClMobj_Unlink(mobj_t *cmo); // needed?

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
dd_bool Cl_IsClientMobj(mobj_t *mo); // public

#endif // DENG_CLIENT_WORLD_MOBJ_H
