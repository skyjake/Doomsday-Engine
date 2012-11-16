/**
 * @file cl_mobj.h
 * Clientside map objects. @ingroup client
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_CLIENT_MOBJ_H__
#define __DOOMSDAY_CLIENT_MOBJ_H__

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
#define CLMF_KNOWN          0xf0000 // combination of all the KNOWN-flags

// Magic number for client mobj information.
#define CLM_MAGIC1          0xdecafed1
#define CLM_MAGIC2          0xcafedeb8

/// Asserts that a given mobj is a client mobj.
#define CL_ASSERT_CLMOBJ(mo)    assert(Cl_IsClientMobj(mo));

/**
 * Information about a client mobj. This structure is attached to
 * gameside mobjs. The last 4 bytes must be CLM_MAGIC.
 */
typedef struct clmoinfo_s {
    uint            startMagic;     // The client mobj magic number (CLM_MAGIC1).
    struct clmoinfo_s *next, *prev;
    int             flags;
    uint            time;           // Time of last update.
    int             sound;          // Queued sound ID.
    float           volume;         // Volume for queued sound.
    uint            endMagic;       // The client mobj magic number (CLM_MAGIC2).
} clmoinfo_t;

void            Cl_UpdateRealPlayerMobj(mobj_t *localMobj, mobj_t *remoteClientMobj, int flags, boolean onFloor);

mobj_t         *ClMobj_Create(thid_t id);
void            ClMobj_Destroy(mobj_t *mo);
clmoinfo_t     *ClMobj_GetInfo(mobj_t* mo);
struct mobj_s  *ClMobj_Find(thid_t id);
mobj_t         *ClMobj_MobjForInfo(clmoinfo_t* info);
void            ClMobj_Unlink(mobj_t *cmo); // needed?
void            ClMobj_Link(mobj_t *cmo); // needed?
void            ClMobj_SetState(mobj_t *mo, int stnum); // needed?
void            ClMobj_CheckPlanes(mobj_t *mo, boolean justCreated);

/**
 * Reads a single mobj delta (inside PSV_FRAME2 packet) from the message buffer
 * and applies it to the client mobj in question.
 *
 * For client mobjs that belong to players, updates the real player mobj
 * accordingly.
 *
 * @param skip  If @c true, all read data is ignored.
 */
void            ClMobj_ReadDelta2(boolean skip);

void            ClMobj_ReadNullDelta2(boolean skip);

boolean         Cl_IsClientMobj(mobj_t* mo); // public
boolean         ClMobj_IsValid(mobj_t* mo); // public

#endif
