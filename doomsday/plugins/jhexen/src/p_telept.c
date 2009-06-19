/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"

#include "dmu_lib.h"
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

    if(!mo)
        return;

    // Get a random player start.
    if((start = P_GetPlayerStart(0, -1, false)))
    {
        P_Teleport(mo, start->pos[VX], start->pos[VY],
                   start->angle, true);
    }
}

void P_TeleportToDeathmatchStarts(mobj_t* mo)
{
    const playerstart_t* start;

    if(!mo)
        return;

    // First, try a random deathmatch start.
    if((start = P_GetPlayerStart(0, -1, true)))
    {
        P_Teleport(mo, start->pos[VX], start->pos[VY],
                   start->angle, true);
    }
    else
    {
        P_TeleportToPlayerStarts(mo);
    }
}

mobj_t* P_SpawnTeleFog(float x, float y, angle_t angle)
{
    return P_SpawnMobj3f(MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
}

boolean P_Teleport(mobj_t* mo, float x, float y, angle_t angle,
                   boolean useFog)
{
    float               oldpos[3], aboveFloor, fogDelta;
    player_t*           player;
    unsigned int        an;
    angle_t             oldAngle;
    mobj_t*             fog;

    memcpy(oldpos, mo->pos, sizeof(oldpos));
    oldAngle = mo->angle;

    aboveFloor = mo->pos[VZ] - mo->floorZ;
    if(!P_TeleportMove(mo, x, y, false))
        return false;

    if(mo->player)
    {
        player = mo->player;
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
        if(player->powers[PT_FLIGHT] && aboveFloor)
        {
            mo->pos[VZ] = mo->floorZ + aboveFloor;
            if(mo->pos[VZ] + mo->height > mo->ceilingZ)
            {
                mo->pos[VZ] = mo->ceilingZ - mo->height;
            }
            player->plr->viewZ = mo->pos[VZ] + player->plr->viewHeight;
        }
        else
        {
            mo->pos[VZ] = mo->floorZ;
            player->plr->viewZ = mo->pos[VZ] + player->plr->viewHeight;
            if(useFog)
            {
                player->plr->lookDir = 0;
            }
        }
    }
    else if(mo->flags & MF_MISSILE)
    {
        mo->pos[VZ] = mo->floorZ + aboveFloor;
        if(mo->pos[VZ] + mo->height > mo->ceilingZ)
        {
            mo->pos[VZ] = mo->ceilingZ - mo->height;
        }
    }
    else
    {
        mo->pos[VZ] = mo->floorZ;
    }

    // Spawn teleport fog at source and destination
    if(useFog)
    {
        fogDelta = (mo->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT);
        fog = P_SpawnMobj3f(MT_TFOG, oldpos[VX], oldpos[VY],
                            oldpos[VZ] + fogDelta, oldAngle + ANG180, 0);

        S_StartSound(SFX_TELEPORT, fog);

        an = angle >> ANGLETOFINESHIFT;
        fog = P_SpawnMobj3f(MT_TFOG,
                            x + 20 * FIX2FLT(finecosine[an]),
                            y + 20 * FIX2FLT(finesine[an]),
                            mo->pos[VZ] + fogDelta, angle + ANG180, 0);
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

        if(mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
        {
            const terraintype_t* tt = P_MobjGetFloorTerrainType(mo);

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

boolean EV_Teleport(int tid, mobj_t* thing, boolean fog)
{
    int                 i, count, searcher;
    mobj_t*             mo = 0;

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

    if(!mo)
        Con_Error("Can't find teleport mapspot\n");

    return P_Teleport(thing, mo->pos[VX], mo->pos[VY], mo->angle, fog);
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t* player)
{
    const playerstart_t* start;

    if((start = P_GetPlayerStart(0, deathmatch? -1 : 0, deathmatch)))
    {
        P_Teleport(player->plr->mo, start->pos[VX], start->pos[VY],
                   start->angle, true);

#if __JHEXEN__
        if(player->morphTics)
        {   // Teleporting away will undo any morph effects (pig)
            P_UndoPlayerMorph(player);
        }
#else
        S_StartSound(SFX_WPNUP, NULL);
#endif
    }
}
#endif
