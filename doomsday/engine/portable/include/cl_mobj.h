/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * cl_mobj.h: Client Map Objects
 */

#ifndef __DOOMSDAY_CLIENT_MOBJ_H__
#define __DOOMSDAY_CLIENT_MOBJ_H__

// Flags for clmobjs.
#define CLMF_HIDDEN         0x01 // Not officially created yet
#define CLMF_UNPREDICTABLE  0x02 // Temporarily hidden (until next delta)
#define CLMF_SOUND          0x04 // Sound is queued for playing on unhide.
#define CLMF_NULLED         0x08 // Once nulled, it can't be updated.
#define CLMF_STICK_FLOOR    0x10 // Mobj will stick to the floor.
#define CLMF_STICK_CEILING  0x20 // Mobj will stick to the ceiling.

// Clmobj knowledge flags. This keeps track of the information that has been
// received.
#define CLMF_KNOWN_X        0x10000
#define CLMF_KNOWN_Y        0x20000
#define CLMF_KNOWN_Z        0x40000
#define CLMF_KNOWN_STATE    0x80000
#define CLMF_KNOWN          0xf0000 // combination of all the KNOWN-flags

typedef struct clmobj_s {
    struct clmobj_s *next, *prev;
    int             flags;
    uint            time; // Time of last update.
    int             sound; // Queued sound ID.
    float           volume; // Volume for queued sound.
    mobj_t          mo;
} clmobj_t;

void            Cl_InitClientMobjs(void);
void            Cl_Reset(void);
void            Cl_DestroyClientMobjs(void);
clmobj_t       *Cl_CreateMobj(thid_t id);
void            Cl_DestroyMobj(clmobj_t *cmo);
boolean         Cl_MobjIterator(boolean (*callback) (clmobj_t *, void *),
                                void *parm);
void            Cl_PredictMovement(void);
void            Cl_UnsetMobjPosition(clmobj_t *cmo);
void            Cl_SetMobjPosition(clmobj_t *cmo);
int             Cl_ReadMobjDelta(void); // obsolete
void            Cl_ReadMobjDelta2(boolean skip);
void            Cl_ReadNullMobjDelta2(boolean skip);
clmobj_t       *Cl_FindMobj(thid_t id);
void            Cl_CheckMobj(clmobj_t *cmo, boolean justCreated);
void            Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags);

#endif
