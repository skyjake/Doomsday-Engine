/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_telept.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_terraintype.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t* P_SpawnTeleFog(float x, float y, angle_t angle)
{
    return P_SpawnMobj3f(MT_TFOG, x, y, 0, angle, MSF_Z_FLOOR);
}

typedef struct {
    sector_t*           sec;
    mobjtype_t          type;
    mobj_t*             foundMobj;
} findmobjparams_t;

static boolean findMobj(thinker_t* th, void* context)
{
    findmobjparams_t*   params = (findmobjparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    // Must be of the correct type?
    if(params->type >= 0 && params->type != mo->type)
        return true; // Continue iteration.

    // Must be in the specified sector?
    if(params->sec &&
       params->sec != P_GetPtrp(mo->subsector, DMU_SECTOR))
        return true; // Continue iteration.

    // Found it!
    params->foundMobj = mo;
    return false; // Stop iteration.
}

static mobj_t* getTeleportDestination(short tag)
{
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        sector_t*           sec = NULL;
        findmobjparams_t    params;

        params.type = MT_TELEPORTMAN;
        params.foundMobj = NULL;

        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);
        while((sec = IterList_MoveIterator(list)) != NULL)
        {
            params.sec = sec;

            if(!DD_IterateThinkers(P_MobjThinker, findMobj, &params))
            {   // Found one.
                return params.foundMobj;
            }
        }
    }

    return NULL;
}

int EV_Teleport(linedef_t* line, int side, mobj_t* mo, boolean spawnFog)
{
    mobj_t*             dest;

    // Clients cannot teleport on their own.
    if(IS_CLIENT)
        return 0;

    if(mo->flags2 & MF2_NOTELEPORT)
        return 0;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return 0;

    if((dest = getTeleportDestination(P_ToXLine(line)->tag)) != NULL)
    {   // A suitable destination has been found.
        mobj_t*             fog;
        uint                an;
        float               oldPos[3];
        float               aboveFloor;
        angle_t             oldAngle;

        memcpy(oldPos, mo->pos, sizeof(mo->pos));
        oldAngle = mo->angle;
        aboveFloor = mo->pos[VZ] - mo->floorZ;

        if(!P_TeleportMove(mo, dest->pos[VX], dest->pos[VY], false))
            return 0;

        // In Final Doom things teleported to their destination but the
        // height wasn't set to the floor.
        if(gameMission != GM_TNT && gameMission != GM_PLUT)
            mo->pos[VZ] = mo->floorZ;

        if(spawnFog)
        {
            // Spawn teleport fog at source and destination.
            if((fog = P_SpawnMobj3fv(MT_TFOG, oldPos, oldAngle + ANG180, 0)))
                S_StartSound(SFX_TELEPT, fog);

            an = dest->angle >> ANGLETOFINESHIFT;
            if((fog = P_SpawnMobj3f(MT_TFOG,
                                    dest->pos[VX] + 20 * FIX2FLT(finecosine[an]),
                                    dest->pos[VY] + 20 * FIX2FLT(finesine[an]),
                                    mo->pos[VZ], dest->angle + ANG180, 0)))
            {
                // Emit sound, where?
                S_StartSound(SFX_TELEPT, fog);
            }
        }

        mo->angle = dest->angle;
        if(mo->flags2 & MF2_FLOORCLIP)
        {
            mo->floorClip = 0;

            if(mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
            {
                const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

                if(tt->flags & TTF_FLOORCLIP)
                {
                    mo->floorClip = 10;
                }
            }
        }

        mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

        // Don't move for a bit.
        if(mo->player)
        {
            mo->reactionTime = 18;
            if(mo->player->powers[PT_FLIGHT] && aboveFloor > 0)
            {
                mo->pos[VZ] = mo->floorZ + aboveFloor;
                if(mo->pos[VZ] + mo->height > mo->ceilingZ)
                {
                    mo->pos[VZ] = mo->ceilingZ - mo->height;
                }
            }
            else
            {
                //mo->dPlayer->clLookDir = 0; /* $unifiedangles */
                mo->dPlayer->lookDir = 0;
            }
            mo->player->viewHeight = (float) cfg.plrViewHeight;
            mo->player->viewHeightDelta = 0;
            mo->player->viewZ = mo->pos[VZ] + mo->player->viewHeight;

            //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
            mo->dPlayer->flags |=
                DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;

#ifdef _DEBUG
            Con_Message("EV_Teleport: Player %p set FIX flags.\n", mo->dPlayer);
#endif
        }

        return 1;
    }

    return 0;
}
