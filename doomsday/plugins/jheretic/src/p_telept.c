/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#include "jheretic.h"

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
    return P_SpawnMobj3f(MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
}

boolean P_Teleport(mobj_t* mo, float x, float y, angle_t angle, boolean spawnFog)
{
    float oldpos[3], aboveFloor, fogDelta;
    angle_t oldAngle;
    mobj_t* fog;
    uint an;

    memcpy(oldpos, mo->pos, sizeof(oldpos));
    aboveFloor = mo->pos[VZ] - mo->floorZ;
    oldAngle = mo->angle;
    if(!P_TeleportMove(mo, x, y, false))
    {
        return false;
    }

    // $voodoodolls Must be the real player.
    if(mo->player && mo->player->plr->mo)
    {
        player_t* player = mo->player;
        if(player->powers[PT_FLIGHT] && aboveFloor > 0)
        {
            mo->pos[VZ] = mo->floorZ + aboveFloor;
            if(mo->pos[VZ] + mo->height > mo->ceilingZ)
            {
                mo->pos[VZ] = mo->ceilingZ - mo->height;
            }
            player->viewZ = mo->pos[VZ] + player->viewHeight;
        }
        else
        {
            //player->plr->clLookDir = 0; /* $unifiedangles */
            player->plr->lookDir = 0;
            mo->pos[VZ] = mo->floorZ;
        }
            
        player->viewHeight = (float) cfg.plrViewHeight;
        player->viewHeightDelta = 0;
        player->viewZ = mo->pos[VZ] + player->viewHeight;
        player->viewOffset[VX] = player->viewOffset[VY] = player->viewOffset[VZ] = 0;
        player->bob = 0;

        //player->plr->clAngle = angle; /* $unifiedangles */
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
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

    if(spawnFog)
    {
        // Spawn teleport fog at source and destination
        fogDelta = ((mo->flags & MF_MISSILE)? 0 : TELEFOGHEIGHT);
        if((fog = P_SpawnMobj3f(MT_TFOG, oldpos[VX], oldpos[VY], oldpos[VZ] + fogDelta, oldAngle + ANG180, 0)))
            S_StartSound(SFX_TELEPT, fog);

        an = angle >> ANGLETOFINESHIFT;

        if((fog = P_SpawnMobj3f(MT_TFOG, x + 20 * FIX2FLT(finecosine[an]), y + 20 * FIX2FLT(finesine[an]), mo->pos[VZ] + fogDelta, angle + ANG180, 0)))
            S_StartSound(SFX_TELEPT, fog);
    }

    if(mo->player && !mo->player->powers[PT_WEAPONLEVEL2])
    {   // Freeze player for about .5 sec.
        mo->reactionTime = 18;
    }

    mo->angle = angle;
    if(mo->flags2 & MF2_FLOORCLIP)
    {
        mo->floorClip = 0;

        if(mo->pos[VZ] ==
           P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
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
        an = angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[angle]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[angle]);
    }
    else
    {
        mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    }

    P_MobjClearSRVO(mo);
    return true;
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
            {   // Found one!
                return params.foundMobj;
            }
        }
    }

    return NULL;
}

boolean EV_Teleport(linedef_t* line, int side, mobj_t* mo, boolean spawnFog)
{
    mobj_t*             dest;

    // Clients cannot teleport on their own.
    if(IS_CLIENT)
        return 0;

    if(mo->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return false;

    if((dest = getTeleportDestination(P_ToXLine(line)->tag)) != NULL)
    {
        return P_Teleport(mo, dest->pos[VX], dest->pos[VY], dest->angle,
                          spawnFog);
    }

    return false;
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t* player)
{
    const playerstart_t* start;

    // Get a random deathmatch start.
    if((start = P_GetPlayerStart(0, deathmatch? -1 : 0, deathmatch)))
    {
        const mapspot_t* spot = &mapSpots[start->spot];
        P_Teleport(player->plr->mo, spot->pos[VX], spot->pos[VY], spot->angle, true);

#if __JHEXEN__
        if(player->morphTics)
        {   // Teleporting away will undo any morph effects (pig)
            P_UndoPlayerMorph(player);
        }
        //S_StartSound(NULL, SFX_WPNUP); // Full volume laugh
#else
        /*S_StartSound(SFX_WPNUP, NULL); // Full volume laugh
           NetSv_Sound(NULL, SFX_WPNUP, player-players); */
        S_StartSound(SFX_WPNUP, NULL);
#endif
    }
}
#endif
