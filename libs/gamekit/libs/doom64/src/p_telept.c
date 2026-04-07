/** @file p_telept.c
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "mobj.h"
#include "p_actor.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_start.h"

mobj_t *P_SpawnTeleFog(coord_t x, coord_t y, angle_t angle)
{
    return P_SpawnMobjXYZ(MT_TFOG, x, y, TELEFOGHEIGHT, angle, MSF_Z_FLOOR);
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

static mobj_t* getTeleportDestination(short tag)
{
    iterlist_t* list;

    list = P_GetSectorIterListForTag(tag, false);
    if(list)
    {
        Sector* sec = NULL;
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
    mobj_t* dest;

    // Clients cannot teleport on their own.
    if(IS_CLIENT) return 0;

    if(mo->flags2 & MF2_NOTELEPORT) return 0;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1) return 0;

    if((dest = getTeleportDestination(P_ToXLine(line)->tag)) != NULL)
    {
        // A suitable destination has been found.
        coord_t oldPos[3], aboveFloor;
        angle_t oldAngle;
        mobj_t* fog;
        uint an;

        memcpy(oldPos, mo->origin, sizeof(mo->origin));
        oldAngle = mo->angle;
        aboveFloor = mo->origin[VZ] - mo->floorZ;

        if(!P_TeleportMove(mo, dest->origin[VX], dest->origin[VY], false))
            return 0;

        mo->origin[VZ] = mo->floorZ;

        if(spawnFog)
        {
            // Spawn teleport fog at source and destination.
            if((fog = P_SpawnMobj(MT_TFOG, oldPos, oldAngle + ANG180, 0)))
                S_StartSound(SFX_TELEPT, fog);

            an = dest->angle >> ANGLETOFINESHIFT;
            if((fog = P_SpawnMobjXYZ(MT_TFOG, dest->origin[VX] + 20 * FIX2FLT(finecosine[an]),
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
        }

        return 1;
    }

    return 0;
}

/**
 * d64tc
 * If the given doomed number is a type which fade spawns, this will return
 * the corresponding mobjtype. Else -1.
 *
 * DJS - Added in order to cleanup EV_FadeSpawn() somewhat.
 * \todo This is still far from ideal. An MF*_* flag would be better.
 */
static mobjtype_t isFadeSpawner(int doomEdNum)
{
    typedef struct fadespawner_s {
        int doomEdNum;
        mobjtype_t type;
    } fadespawner_t;

    static const fadespawner_t  spawners[] =
    {
        {7575, MT_SHOTGUN},
        {7576, MT_CHAINGUN},
        {7577, MT_SUPERSHOTGUN},
        {7578, MT_MISC27},
        {7579, MT_MISC28},
        {7580, MT_MISC25},
        {7581, MT_MISC11},
        {7582, MT_MISC10},
        {7583, MT_MISC0},
        {7584, MT_MISC1},
        {7585, MT_LASERGUN},
        {7586, MT_LPOWERUP1},
        {7587, MT_LPOWERUP2},
        {7588, MT_LPOWERUP3},
        {7589, MT_MEGA},
        {7590, MT_MISC12},
        {7591, MT_INS},
        {7592, MT_INV},
        {7593, MT_MISC13},
        {7594, MT_MISC2},
        {7595, MT_MISC3},
        {7596, MT_MISC15},
        {7597, MT_MISC16},
        {7598, MT_MISC14},
        {7599, MT_MISC22},
        {7600, MT_MISC23},
        {7601, MT_CLIP},
        {7602, MT_MISC17},
        {7603, MT_MISC18},
        {7604, MT_MISC19},
        {7605, MT_MISC20},
        {7606, MT_MISC21},
        {7607, MT_MISC24},
        {7608, MT_POSSESSED},
        {7609, MT_SHOTGUY},
        {7610, MT_TROOP},
        {7611, MT_NTROOP},
        {7612, MT_SERGEANT},
        {7613, MT_SHADOWS},
        {7615, MT_HEAD},
        {7617, MT_SKULL},
        {7618, MT_PAIN},
        {7619, MT_FATSO},
        {7620, MT_BABY},
        {7621, MT_CYBORG},
        {7622, MT_BITCH},
        {7623, MT_KNIGHT},
        {7624, MT_BRUISER},
        {7625, MT_MISC5},
        {7626, MT_MISC8},
        {7627, MT_MISC4},
        {7628, MT_MISC9},
        {7629, MT_MISC6},
        {7630, MT_MISC7},
        {-1, -1} // Terminator.
    };
    int                 i;

    for(i = 0; spawners[i].doomEdNum; ++i)
        if(spawners[i].doomEdNum == doomEdNum)
            return spawners[i].type;

    return -1;
}

typedef struct {
    Sector *sec;
    coord_t spawnHeight;
} fadespawnparams_t;

static int fadeSpawn(thinker_t *th, void *context)
{
    fadespawnparams_t *params = (fadespawnparams_t *) context;
    mobj_t *origin = (mobj_t *) th;
    mobjtype_t spawntype;

    if(params->sec && params->sec != Mobj_Sector(origin))
        return false; // Continue iteration.

    // Only fade spawn origins of a certain type.
    spawntype = isFadeSpawner(origin->info->doomEdNum);
    if(spawntype != -1)
    {
        coord_t pos[3];
        angle_t an;
        mobj_t *mo;

        an = origin->angle >> ANGLETOFINESHIFT;

        memcpy(pos, origin->origin, sizeof(pos));
        pos[VX] += 20 * FIX2FLT(finecosine[an]);
        pos[VY] += 20 * FIX2FLT(finesine[an]);
        pos[VZ] = params->spawnHeight;

        if((mo = P_SpawnMobj(spawntype, pos, origin->angle, 0)))
        {
            mo->translucency = 255;
            mo->spawnFadeTics = 0;
            mo->intFlags |= MIF_FADE;

            // Emit sound, where?
            S_StartSound(SFX_ITMBK, mo);

            if(MOBJINFO[spawntype].flags & MF_COUNTKILL)
                totalKills++;
        }
    }

    return false; // Continue iteration.
}

/**
 * d64tc
 * kaiser - This sets a thing spawn depending on thing type placed in
 *       tagged sector.
 * \todo DJS - This is not a good design. There must be a better way
 *       to do this using a new thing flag (MF_NOTSPAWNONSTART?).
 */
int EV_FadeSpawn(Line* li, mobj_t* mo)
{
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(P_ToXLine(li)->tag, false);
    if(list)
    {
        Sector*             sec;
        fadespawnparams_t   params;

        params.spawnHeight = mo->origin[VZ];

        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);
        while((sec = IterList_MoveIterator(list)) != NULL)
        {
            params.sec = sec;
            Thinker_Iterate(P_MobjThinker, fadeSpawn, &params);
        }
    }

    return 0;
}

// Bitwise ops (for changeMobjFlags)
typedef enum {
    BW_CLEAR,
    BW_SET,
    BW_XOR
} bitwiseop_t;

typedef struct {
    Sector *sec;
    dd_bool notPlayers;
    int flags;
    bitwiseop_t op;
} pit_changemobjflagsparams_t;

int PIT_ChangeMobjFlags(thinker_t *th, void *context)
{
    pit_changemobjflagsparams_t *parm = (pit_changemobjflagsparams_t *) context;
    mobj_t *mo = (mobj_t *) th;

    if(parm->sec && parm->sec != Mobj_Sector(mo))
        return false;

    if(parm->notPlayers && mo->player)
        return false;

    switch(parm->op)
    {
    case BW_CLEAR:
        mo->flags &= ~parm->flags;
        break;

    case BW_SET:
        mo->flags |= parm->flags;
        break;

    case BW_XOR:
        mo->flags ^= parm->flags;
        break;

    default:
        DE_ASSERT(false);
        break;
    }

    return false; // Continue iteration.
}

/**
 * d64tc
 * kaiser - removes things in tagged sector!
 * DJS - actually, no it doesn't at least not directly.
 *
 * @todo fixme: It appears the MF_TELEPORT flag has been hijacked.
 */
int EV_FadeAway(Line *line, mobj_t *thing)
{
    iterlist_t *list;

    DE_UNUSED(thing);

    if(!line) return 0;

    if((list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false)) != 0)
    {
        Sector *sec = 0;
        pit_changemobjflagsparams_t parm;

        parm.flags      = MF_TELEPORT;
        parm.op         = BW_SET;
        parm.notPlayers = true;

        IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
        IterList_RewindIterator(list);
        while((sec = IterList_MoveIterator(list)) != NULL)
        {
            parm.sec = sec;
            Thinker_Iterate(P_MobjThinker, PIT_ChangeMobjFlags, &parm);
        }
    }

    return 0;
}
