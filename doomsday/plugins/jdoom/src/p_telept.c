/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "s_sound.h"
#include "p_local.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

mobj_t *P_SpawnTeleFog(int x, int y)
{
    subsector_t *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj(x, y, P_GetFixedp(ss, DMU_FLOOR_HEIGHT) +
                             TELEFOGHEIGHT, MT_TFOG);
}

int EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    int     i;
    int     tag;
    mobj_t *m;
    mobj_t *fog;
    unsigned an;
    thinker_t *thinker;
    sector_t *sector;
    fixed_t oldpos[3];
    fixed_t aboveFloor;

    if(thing->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if(side == 1)
        return 0;

    tag = P_XLine(line)->tag;

    for(i = 0; i < numsectors; i++)
    {
        if(xsectors[i].tag == tag)
        {
            thinker = thinkercap.next;
            for(thinker = thinkercap.next; thinker != &thinkercap;
                thinker = thinker->next)
            {
                // not a mobj
                if(thinker->function != P_MobjThinker)
                    continue;

                m = (mobj_t *) thinker;

                // not a teleportman
                if(m->type != MT_TELEPORTMAN)
                    continue;

                sector = P_GetPtrp(m->subsector, DMU_SECTOR);
                // wrong sector
                if(P_ToIndex(sector) != i)
                    continue;

                memcpy(oldpos, thing->pos, sizeof(thing->pos));
                aboveFloor = thing->pos[VZ] - thing->floorz;

                if(!P_TeleportMove(thing, m->pos[VX], m->pos[VY], false))
                    return 0;

                // In Final Doom things teleported to their destination
                // but the height wasn't set to the floor.
                if(gamemission != pack_tnt && gamemission != pack_plut)
                    thing->pos[VZ] = thing->floorz;

                // spawn teleport fog at source and destination
                fog = P_SpawnMobj(oldpos[VX], oldpos[VY], oldpos[VZ], MT_TFOG);
                S_StartSound(sfx_telept, fog);
                an = m->angle >> ANGLETOFINESHIFT;
                fog =
                    P_SpawnMobj(m->pos[VX] + 20 * finecosine[an],
                                m->pos[VY] + 20 * finesine[an], thing->pos[VZ], MT_TFOG);

                // emit sound, where?
                S_StartSound(sfx_telept, fog);

                thing->angle = m->angle;
                if(thing->flags2 & MF2_FLOORCLIP)
                {
                    if(thing->pos[VZ] == P_GetFixedp(thing->subsector,
                                               DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) &&
                       P_GetThingFloorType(thing) >= FLOOR_LIQUID)
                    {
                        thing->floorclip = 10 * FRACUNIT;
                    }
                    else
                    {
                        thing->floorclip = 0;
                    }
                }
                thing->momx = thing->momy = thing->momz = 0;

                // don't move for a bit
                if(thing->player)
                {
                    thing->reactiontime = 18;
                    if(thing->player->powers[pw_flight] && aboveFloor)
                    {
                        thing->pos[VZ] = thing->floorz + aboveFloor;
                        if(thing->pos[VZ] + thing->height > thing->ceilingz)
                        {
                            thing->pos[VZ] = thing->ceilingz - thing->height;
                        }
                        thing->dplayer->viewz =
                            thing->pos[VZ] + thing->dplayer->viewheight;
                    }
                    else
                    {
                        thing->dplayer->clLookDir = 0;
                        thing->dplayer->lookdir = 0;
                    }

                    thing->dplayer->clAngle = thing->angle;
                    thing->dplayer->flags |=
                        DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
                }
                return 1;
            }
        }
    }
    return 0;
}
