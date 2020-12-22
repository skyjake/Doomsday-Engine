/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

#include <string.h>

#include "jhexen.h"

#include "dmu_lib.h"
#include "g_common.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_ArtiTeleportOther(player_t* plr)
{
    mobj_t*             mo;

    if(!plr || !plr->plr->mo)
        return;

    if((mo = P_SpawnPlayerMissile(MT_TELOTHER_FX1, plr->plr->mo)))
    {
        mo->target = plr->plr->mo;
    }
}

void P_TeleportToPlayerStarts(mobj_t* mo)
{
    const playerstart_t* start;

    if(!mo) return;

    // Get a random player start.
    if((start = P_GetPlayerStart(0, -1, false)))
    {
        const mapspot_t* spot = &mapSpots[start->spot];
        P_Teleport(mo, spot->origin[VX], spot->origin[VY], spot->angle, true);
    }
}

void P_TeleportToDeathmatchStarts(mobj_t* mo)
{
    const playerstart_t* start;

    if(!mo) return;

    // First, try a random deathmatch start.
    if((start = P_GetPlayerStart(0, -1, true)))
    {
        const mapspot_t* spot = &mapSpots[start->spot];
        P_Teleport(mo, spot->origin[VX], spot->origin[VY], spot->angle, true);
    }
    else
    {
        P_TeleportToPlayerStarts(mo);
    }
}

mobj_t* P_SpawnTeleFog(coord_t x, coord_t y, angle_t angle)
{
    return P_SpawnMobjXYZ(MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
}

dd_bool P_Teleport(mobj_t* mo, coord_t x, coord_t y, angle_t angle, dd_bool useFog)
{
    coord_t oldpos[3], aboveFloor, fogDelta;
    unsigned int an;
    angle_t oldAngle;
    mobj_t* fog;

    memcpy(oldpos, mo->origin, sizeof(oldpos));
    oldAngle = mo->angle;

    aboveFloor = mo->origin[VZ] - mo->floorZ;
    if(!P_TeleportMove(mo, x, y, false))
        return false;

    // $voodoodolls Must be the real player.
    if(mo->player && mo->player->plr->mo == mo)
    {
        player_t* player = mo->player;

        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;
        if(player->powers[PT_FLIGHT] && aboveFloor > 0)
        {
            mo->origin[VZ] = mo->floorZ + aboveFloor;
            if(mo->origin[VZ] + mo->height > mo->ceilingZ)
            {
                mo->origin[VZ] = mo->ceilingZ - mo->height;
            }
        }
        else
        {
            mo->origin[VZ] = mo->floorZ;
            if(useFog)
                player->plr->lookDir = 0;
        }
        player->viewHeight = (coord_t) cfg.common.plrViewHeight;
        player->viewHeightDelta = 0;
        player->viewZ = mo->origin[VZ] + player->viewHeight;
        player->viewOffset[VX] = player->viewOffset[VY] = player->viewOffset[VZ] = 0;
        player->bob = 0;
    }
    else if(mo->flags & MF_MISSILE)
    {
        mo->origin[VZ] = mo->floorZ + aboveFloor;
        if(mo->origin[VZ] + mo->height > mo->ceilingZ)
        {
            mo->origin[VZ] = mo->ceilingZ - mo->height;
        }
    }
    else
    {
        mo->origin[VZ] = mo->floorZ;
    }

    // Spawn teleport fog at source and destination
    if(useFog)
    {
        fogDelta = (mo->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT);
        if((fog = P_SpawnMobjXYZ(MT_TFOG, oldpos[VX], oldpos[VY],
                                oldpos[VZ] + fogDelta, oldAngle + ANG180, 0)))
            S_StartSound(SFX_TELEPORT, fog);

        an = angle >> ANGLETOFINESHIFT;
        if((fog = P_SpawnMobjXYZ(MT_TFOG,
                                x + 20 * FIX2FLT(finecosine[an]),
                                y + 20 * FIX2FLT(finesine[an]),
                                mo->origin[VZ] + fogDelta, angle + ANG180, 0)))
            S_StartSound(SFX_TELEPORT, fog);

        if(mo->player && !mo->player->powers[PT_SPEED])
        {   // Freeze player for about .5 sec
            mo->reactionTime = 18;
        }
        mo->angle = angle;
    }

    if(mo->flags2 & MF2_FLOORCLIP)
    {
        mo->floorClip = 0;

        if(FEQUAL(mo->origin[VZ], P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
        {
            const terraintype_t* tt = P_MobjFloorTerrain(mo);
            if(tt->flags & TTF_FLOORCLIP)
            {
                mo->floorClip = 10;
            }
        }
    }

    if(mo->flags & MF_MISSILE)
    {
        angle >>= ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[angle]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[angle]);
    }
    else if(useFog) // No fog doesn't alter the player's momentums.
    {
        mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    }

    P_MobjClearSRVO(mo);

    return true;
}

dd_bool EV_Teleport(int tid, mobj_t* thing, dd_bool fog)
{
    int                 i, count, searcher;
    mobj_t*             mo = 0;

    // Clients cannot teleport on their own.
    if(IS_CLIENT)
        return 0;

    if(!thing)
        return false;

    if(thing->flags2 & MF2_NOTELEPORT)
        return false;

    count = 0;
    searcher = -1;
    while(P_FindMobjFromTID(tid, &searcher) != NULL)
        count++;

    if(count == 0)
        return false;

    count = 1 + (P_Random() % count);
    searcher = -1;
    for(i = 0; i < count; ++i)
    {
        mo = P_FindMobjFromTID(tid, &searcher);
    }

    if (!mo)
    {
        App_Log(DE2_MAP_WARNING, "Can't find teleport mapspot");
        return false;
    }

    return P_Teleport(thing, mo->origin[VX], mo->origin[VY], mo->angle, fog);
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t *player)
{
    playerstart_t const *start;

    if((start = P_GetPlayerStart(0, gfw_Rule(deathmatch)? -1 : 0, gfw_Rule(deathmatch))))
    {
        mapspot_t const *spot = &mapSpots[start->spot];
        P_Teleport(player->plr->mo, spot->origin[VX], spot->origin[VY], spot->angle, true);

#if __JHEXEN__
        if(player->morphTics)
        {
            // Teleporting away will undo any morph effects (pig)
            P_UndoPlayerMorph(player);
        }
#else
        S_StartSound(SFX_WPNUP, NULL);
#endif
    }
}
#endif
