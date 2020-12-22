/**\file p_telept.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "jdoom.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_terraintype.h"

mobj_t *P_SpawnTeleFog(coord_t x, coord_t y, angle_t angle)
{
    return P_SpawnMobjXYZ(MT_TFOG, x, y, 0, angle, MSF_Z_FLOOR);
}

typedef struct {
    Sector *sec;
    mobjtype_t type;
    mobj_t *foundMobj;
} findmobjparams_t;

static int findMobj(thinker_t *th, void *context)
{
    findmobjparams_t *params = (findmobjparams_t *) context;
    mobj_t *mo = (mobj_t *) th;

    // Must be of the correct type?
    if(params->type >= 0 && params->type != mo->type)
        return false; // Continue iteration.

    // Must be in the specified sector?
    if(params->sec && params->sec != Mobj_Sector(mo))
        return false; // Continue iteration.

    // Found it!
    params->foundMobj = mo;
    return true; // Stop iteration.
}

static mobj_t *getTeleportDestination(short tag)
{
    iterlist_t *list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        Sector *sec = NULL;
        findmobjparams_t params;

        params.type = MT_TELEPORTMAN;
        params.foundMobj = NULL;

        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);
        while((sec = IterList_MoveIterator(list)) != NULL)
        {
            params.sec = sec;

            if(Thinker_Iterate(P_MobjThinker, findMobj, &params))
            {   // Found one.
                return params.foundMobj;
            }
        }
    }

    return NULL;
}

int EV_Teleport(Line* line, int side, mobj_t* mo, dd_bool spawnFog)
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
    {
        // A suitable destination has been found.
        mobj_t* fog;
        coord_t oldPos[3];
        coord_t aboveFloor;
        angle_t oldAngle;
        uint an;

        memcpy(oldPos, mo->origin, sizeof(mo->origin));
        oldAngle = mo->angle;
        aboveFloor = mo->origin[VZ] - mo->floorZ;

        if(!P_TeleportMove(mo, dest->origin[VX], dest->origin[VY], false))
            return 0;

        // In Final Doom things teleported to their destination but the
        // height wasn't set to the floor.
        if(gameMode != doom2_tnt && gameMode != doom2_plut)
            mo->origin[VZ] = mo->floorZ;

        if(spawnFog)
        {
            // Spawn teleport fog at source and destination.
            if((fog = P_SpawnMobj(MT_TFOG, oldPos, oldAngle + ANG180, 0)))
                S_StartSound(SFX_TELEPT, fog);

            an = dest->angle >> ANGLETOFINESHIFT;
            if((fog = P_SpawnMobjXYZ(MT_TFOG,
                                     dest->origin[VX] + 20 * FIX2FLT(finecosine[an]),
                                     dest->origin[VY] + 20 * FIX2FLT(finesine[an]),
                                     mo->origin[VZ], dest->angle + ANG180, 0)))
            {
                // Emit sound, where?
                S_StartSound(SFX_TELEPT, fog);
            }
        }

        mo->angle = dest->angle;
        if(mo->flags2 & MF2_FLOORCLIP)
        {
            mo->floorClip = 0;

            if(FEQUAL(mo->origin[VZ], P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
            {
                terraintype_t const *tt = P_MobjFloorTerrain(mo);
                if(tt->flags & TTF_FLOORCLIP)
                {
                    mo->floorClip = 10;
                }
            }
        }

        mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

        // $voodoodolls Must be the real player.
        if(mo->player && mo->player->plr->mo == mo)
        {
            mo->reactionTime = 18; // Don't move for a bit.
            if(mo->player->powers[PT_FLIGHT] && aboveFloor > 0)
            {
                mo->origin[VZ] = mo->floorZ + aboveFloor;
                if(mo->origin[VZ] + mo->height > mo->ceilingZ)
                {
                    mo->origin[VZ] = mo->ceilingZ - mo->height;
                }
            }
            else
            {
                //mo->dPlayer->clLookDir = 0; /* $unifiedangles */
                mo->dPlayer->lookDir = 0;
            }
            mo->player->viewHeight = (coord_t) cfg.common.plrViewHeight;
            mo->player->viewHeightDelta = 0;
            mo->player->viewZ = mo->origin[VZ] + mo->player->viewHeight;
            mo->player->viewOffset[VX] = mo->player->viewOffset[VY] = mo->player->viewOffset[VZ] = 0;
            mo->player->bob = 0;

            //mo->dPlayer->clAngle = mo->angle; /* $unifiedangles */
            mo->dPlayer->flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;

            App_Log(DE2_DEV_NET_VERBOSE, "EV_Teleport: Player %p set FIX flags", mo->dPlayer);
        }

        return 1;
    }

    return 0;
}
