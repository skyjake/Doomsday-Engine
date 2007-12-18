/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
    subsector_t *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj3f(MT_TFOG, x, y,
                         P_GetFloatp(ss, DMU_FLOOR_HEIGHT) + TELEFOGHEIGHT);
}

int EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    unsigned    an;
    float       oldpos[3];
    float       aboveFloor;
    mobj_t     *m;
    mobj_t     *fog;
    thinker_t  *th;
    sector_t   *sec = NULL;
    iterlist_t *list;

    if(thing->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if(side == 1)
        return 0;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        for(th = thinkercap.next; th != &thinkercap; th = th->next)
        {

            if(th->function != P_MobjThinker)
                continue; // Not a mobj.

            m = (mobj_t *) th;

            if(m->type != MT_TELEPORTMAN)
                continue; // Not a teleportman.

            if(P_GetPtrp(m->subsector, DMU_SECTOR) != sec)
                continue; // Wrong sector.

            memcpy(oldpos, thing->pos, sizeof(thing->pos));
            aboveFloor = thing->pos[VZ] - thing->floorz;

            if(!P_TeleportMove(thing, m->pos[VX], m->pos[VY], false))
                return 0;

            // In Final Doom things teleported to their destination but the
            // height wasn't set to the floor.
            if(gamemission != GM_TNT && gamemission != GM_PLUT)
                thing->pos[VZ] = thing->floorz;

            // Spawn teleport fog at source and destination.
            fog = P_SpawnMobj3fv(MT_TFOG, oldpos);
            S_StartSound(sfx_telept, fog);
            an = m->angle >> ANGLETOFINESHIFT;
            fog = P_SpawnMobj3f(MT_TFOG,
                                m->pos[VX] + 20 * FIX2FLT(finecosine[an]),
                                m->pos[VY] + 20 * FIX2FLT(finesine[an]),
                                thing->pos[VZ]);

            // Emit sound, where?
            S_StartSound(sfx_telept, fog);

            thing->angle = m->angle;
            if(thing->flags2 & MF2_FLOORCLIP)
            {
                if(thing->pos[VZ] ==
                   P_GetFloatp(thing->subsector,
                               DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) &&
                   P_MobjGetFloorType(thing) >= FLOOR_LIQUID)
                {
                    thing->floorclip = 10;
                }
                else
                {
                    thing->floorclip = 0;
                }
            }

            thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;

            // Don't move for a bit.
            if(thing->player)
            {
                thing->reactiontime = 18;
                if(thing->player->powers[PT_FLIGHT] && aboveFloor)
                {
                    thing->pos[VZ] = thing->floorz + aboveFloor;
                    if(thing->pos[VZ] + thing->height > thing->ceilingz)
                    {
                        thing->pos[VZ] = thing->ceilingz - thing->height;
                    }
                }
                else
                {
                    //thing->dplayer->clLookDir = 0; /* $unifiedangles */
                    thing->dplayer->lookdir = 0;
                }
                thing->dplayer->viewZ =
                    thing->pos[VZ] + thing->dplayer->viewheight;

                //thing->dplayer->clAngle = thing->angle; /* $unifiedangles */
                thing->dplayer->flags |=
                    DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
            }

            return 1;
        }
    }

    return 0;
}
