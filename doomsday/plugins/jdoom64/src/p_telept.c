/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#include <stdio.h>
#include <string.h>

#include "doom64tc.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_map.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t *P_SpawnTeleFog(float x, float y)
{
    subsector_t        *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj3f(MT_TFOG, x, y,
                         P_GetFloatp(ss, DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT);
}

int EV_Teleport(linedef_t *line, int side, mobj_t *thing, boolean spawnFog)
{
    mobj_t             *m;
    mobj_t             *fog;
    unsigned            an;
    thinker_t          *th;
    sector_t           *sec = NULL;
    float               oldPos[3];
    float               aboveFloor;
    iterlist_t         *list;

    if(thing->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return 0;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue; // Not a mobj.

            m = (mobj_t *) th;

            if(m->type != MT_TELEPORTMAN)
                continue; // Not a teleportman.

            if(P_GetPtrp(m->subsector, DMU_SECTOR) != sec)
                continue; // Wrong sector.

            memcpy(oldPos, thing->pos, sizeof(thing->pos));
            aboveFloor = thing->pos[VZ] - thing->floorZ;

            if(!P_TeleportMove(thing, m->pos[VX], m->pos[VY], false))
                return 0;

            if(spawnFog)
            {
                // Spawn teleport fog at source and destination.
                fog = P_SpawnMobj3fv(MT_TFOG, oldPos);
                S_StartSound(sfx_telept, fog);
                an = m->angle >> ANGLETOFINESHIFT;
                fog =
                    P_SpawnMobj3f(MT_TFOG,
                                  m->pos[VX] + 20 * FIX2FLT(finecosine[an]),
                                  m->pos[VY] + 20 * FIX2FLT(finesine[an]),
                                  thing->pos[VZ]);

                // Emit sound, where?
                S_StartSound(sfx_telept, fog);
            }

            thing->angle = m->angle;
            if(thing->flags2 & MF2_FLOORCLIP)
            {
                if(thing->pos[VZ] ==
                    P_GetFloatp(thing->subsector,
                                DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) &&
                   P_MobjGetFloorType(thing) >= FLOOR_LIQUID)
                {
                    thing->floorClip = 10;
                }
                else
                {
                    thing->floorClip = 0;
                }
            }
            thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;

            // Don't move for a bit.
            if(thing->player)
            {
                thing->reactionTime = 18;
                if(thing->player->powers[PT_FLIGHT] && aboveFloor)
                {
                    thing->pos[VZ] = thing->floorZ + aboveFloor;
                    if(thing->pos[VZ] + thing->height > thing->ceilingZ)
                    {
                        thing->pos[VZ] = thing->ceilingZ - thing->height;
                    }
                    thing->dPlayer->viewZ =
                        thing->pos[VZ] + thing->dPlayer->viewHeight;
                }
                else
                {
                    //thing->dPlayer->clLookDir = 0; /* $unifiedangles */
                    thing->dPlayer->lookDir = 0;
                }

                //thing->dPlayer->clAngle = thing->angle; /* $unifiedangles */
                thing->dPlayer->flags |=
                    DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
            }
            return 1;
        }
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
static mobjtype_t isFadeSpawner(int doomednum)
{
    typedef struct fadespawner_s {
        int doomednum;
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
        {7614, MT_DNIGHTMARE},
        {7615, MT_HEAD},
        {7616, MT_NIGHTMARECACO},
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
        {7631, MT_CHAINGUNGUY},
        {7632, MT_NIGHTCRAWLER},
        {7633, MT_ACID},
        {-1, -1} // Terminator.
    };
    int                 i;

    for(i = 0; spawners[i].doomednum; ++i)
        if(spawners[i].doomednum == doomednum)
            return spawners[i].type;

    return -1;
}

/**
 * d64tc
 * kaiser - This sets a thing spawn depending on thing type placed in
 *       tagged sector.
 * \todo DJS - This is not a good design. There must be a better way
 *       to do this using a new thing flag (MF_NOTSPAWNONSTART?).
 */
int EV_FadeSpawn(linedef_t *line, mobj_t *thing)
{
    mobj_t             *mobj, *mo;
    angle_t             an;
    thinker_t          *th;
    sector_t           *sector, *tagsec = NULL;
    float               pos[3];
    mobjtype_t          spawntype;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((tagsec = P_IterListIterator(list)) != NULL)
    {
        for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue; // Not a mobj.

            mobj = (mobj_t *) th;

            sector = P_GetPtrp(mobj->subsector, DMU_SECTOR);

            if(sector != tagsec)
                continue; // Wrong sector.

            // Only fade spawn mobjs of a certain type.
            spawntype = isFadeSpawner(mobj->info->doomedNum);
            if(spawntype != -1)
            {
                an = mobj->angle >> ANGLETOFINESHIFT;

                memcpy(pos, mobj->pos, sizeof(pos));
                pos[VX] += 20 * FIX2FLT(finecosine[an]);
                pos[VY] += 20 * FIX2FLT(finesine[an]);
                pos[VZ] = thing->pos[VZ];

                mo = P_SpawnMobj3fv(spawntype, pos);
                if(mo)
                {
                    mo->translucency = 255;
                    mo->intFlags |= MIF_FADE;
                    mo->angle = mobj->angle;

                    // Emit sound, where?
                    S_StartSound(sfx_itmbk, mo);

                    if(mobjInfo[spawntype].flags & MF_COUNTKILL)
                        totalKills++;
                }
            }
        }
    }
    return 0;
}

/**
 * d64tc
 * kaiser - removes things in tagged sector!
 * DJS - actually, no it doesn't at least not directly.
 *
 * \fixme: It appears the MF_TELEPORT flag has been hijacked.
 */
int EV_FadeAway(linedef_t *line, mobj_t *thing)
{
    mobj_t             *mobj;
    thinker_t          *th;
    sector_t           *sec = NULL;
    iterlist_t         *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue; // Not a mobj.

            mobj = (mobj_t *) th;

            if(sec != P_GetPtrp(mobj->subsector, DMU_SECTOR))
                continue; // Wrong sector

            if(!mobj->player)
                mobj->flags = MF_TELEPORT; // Why do it like this??
        }
    }

    return 0;
}
