/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

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

mobj_t *P_SpawnTeleFog(int x, int y)
{
    subsector_t *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj(x, y, P_GetFixedp(ss, DMU_FLOOR_HEIGHT) +
                             TELEFOGHEIGHT, MT_TFOG);
}

int EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    fixed_t     oldpos[3];
    fixed_t     aboveFloor;
    mobj_t     *m;
    thinker_t  *thinker;
    sector_t   *sec = NULL, *sector;
    iterlist_t *list;

    if(thing->flags2 & MF2_NOTELEPORT)
        return 0;

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if(side == 1)
        return 0;

    list = P_GetSectorIterListForTag(P_XLine(line)->tag, false);
    if(!list)
        return 0;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
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
            if(sector != sec)
                continue;

            memcpy(oldpos, thing->pos, sizeof(thing->pos));
            aboveFloor = thing->pos[VZ] - FLT2FIX(thing->floorz);

            if(!P_TeleportMove(thing, m->pos[VX], m->pos[VY], false))
                return 0;

            // In Final Doom things teleported to their destination
            // but the height wasn't set to the floor.
            if(gamemission != GM_TNT && gamemission != GM_PLUT)
                thing->pos[VZ] = FLT2FIX(thing->floorz);
#if 0
            // spawn teleport fog at source and destination
            fog = P_SpawnMobj(oldpos[VX], oldpos[VY], oldpos[VZ], MT_TFOG);
            S_StartSound(sfx_telept, fog);
            an = m->angle >> ANGLETOFINESHIFT;
            fog =
                P_SpawnMobj(m->pos[VX] + 20 * finecosine[an],
                            m->pos[VY] + 20 * finesine[an], thing->pos[VZ], MT_TFOG);

            // emit sound, where?
            S_StartSound(sfx_telept, fog);
#endif
            thing->angle = m->angle;
            if(thing->flags2 & MF2_FLOORCLIP)
            {
                if(thing->pos[VZ] == P_GetFixedp(thing->subsector,
                                           DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) &&
                   P_GetThingFloorType(thing) >= FLOOR_LIQUID)
                {
                    thing->floorclip = 10;
                }
                else
                {
                    thing->floorclip = 0;
                }
            }
            thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;

            // don't move for a bit
            if(thing->player)
            {
                thing->reactiontime = 18;
                if(thing->player->powers[PT_FLIGHT] && aboveFloor)
                {
                    thing->pos[VZ] = FLT2FIX(thing->floorz) + aboveFloor;
                    if(FIX2FLT(thing->pos[VZ]) + thing->height > thing->ceilingz)
                    {
                        thing->pos[VZ] = FLT2FIX(thing->ceilingz - thing->height);
                    }
                    thing->dplayer->viewZ =
                        FIX2FLT(thing->pos[VZ]) + thing->dplayer->viewheight;
                }
                else
                {
                    //thing->dplayer->clLookDir = 0; /* $unifiedangles */
                    thing->dplayer->lookdir = 0;
                }

                //thing->dplayer->clAngle = thing->angle; /* $unifiedangles */
                thing->dplayer->flags |=
                    DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
            }
            return 1;
        }
    }
    return 0;
}
