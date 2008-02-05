/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/**
 * p_telept.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

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

boolean P_Teleport(mobj_t *thing, float x, float y, angle_t angle)
{
    float       oldpos[3];
    float       aboveFloor;
    float       fogDelta;
    player_t   *player;
    uint        an;
    mobj_t     *fog;

    memcpy(oldpos, thing->pos, sizeof(oldpos));
    aboveFloor = thing->pos[VZ] - thing->floorZ;
    if(!P_TeleportMove(thing, x, y, false))
    {
        return false;
    }

    if(thing->player)
    {
        player = thing->player;
        if(player->powers[PT_FLIGHT] && aboveFloor)
        {
            thing->pos[VZ] = thing->floorZ + aboveFloor;
            if(thing->pos[VZ] + thing->height > thing->ceilingZ)
            {
                thing->pos[VZ] = thing->ceilingZ - thing->height;
            }
            player->plr->viewZ = thing->pos[VZ] + player->plr->viewHeight;
        }
        else
        {
            thing->pos[VZ] = thing->floorZ;
            player->plr->viewZ = thing->pos[VZ] + player->plr->viewHeight;
            //player->plr->clLookDir = 0; /* $unifiedangles */
            player->plr->lookDir = 0;
        }
        //player->plr->clAngle = angle; /* $unifiedangles */
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    }
    else if(thing->flags & MF_MISSILE)
    {
        thing->pos[VZ] = thing->floorZ + aboveFloor;
        if(thing->pos[VZ] + thing->height > thing->ceilingZ)
        {
            thing->pos[VZ] = thing->ceilingZ - thing->height;
        }
    }
    else
    {
        thing->pos[VZ] = thing->floorZ;
    }

    // Spawn teleport fog at source and destination
    fogDelta = thing->flags & MF_MISSILE? 0 : TELEFOGHEIGHT;
    fog = P_SpawnMobj3f(MT_TFOG, oldpos[VX], oldpos[VY], oldpos[VZ] + fogDelta);
    S_StartSound(sfx_telept, fog);
    an = angle >> ANGLETOFINESHIFT;
    fog =
        P_SpawnMobj3f(MT_TFOG,
                      x + 20 * FIX2FLT(finecosine[an]),
                      y + 20 * FIX2FLT(finesine[an]),
                      thing->pos[VZ] + fogDelta);
    S_StartSound(sfx_telept, fog);
    if(thing->player && !thing->player->powers[PT_WEAPONLEVEL2])
    {   // Freeze player for about .5 sec.
        thing->reactionTime = 18;
    }

    thing->angle = angle;
    if(thing->flags2 & MF2_FLOORCLIP)
    {
        if(thing->pos[VZ] == P_GetFloatp(thing->subsector,
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

    if(thing->flags & MF_MISSILE)
    {
        an = angle >> ANGLETOFINESHIFT;
        thing->mom[MX] = thing->info->speed * FIX2FLT(finecosine[angle]);
        thing->mom[MY] = thing->info->speed * FIX2FLT(finesine[angle]);
    }
    else
    {
        thing->mom[MX] = thing->mom[MY] = thing->mom[MZ] = 0;
    }

    P_MobjClearSRVO(thing);
    return true;
}

boolean EV_Teleport(linedef_t *line, int side, mobj_t *thing)
{
    mobj_t     *m;
    thinker_t  *th;
    sector_t   *sec = NULL;
    iterlist_t *list;

    if(thing->flags2 & MF2_NOTELEPORT)
        return false;

    // Don't teleport if hit back of line, so you can get out of teleporter.
    if(side == 1)
        return false;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return false;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        th = thinkerCap.next;
        for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
        {
            if(th->function != P_MobjThinker)
                continue; // Not a mobj.

            m = (mobj_t *) th;

            if(m->type != MT_TELEPORTMAN)
                continue; // Not a teleportman.

            if(P_GetPtrp(m->subsector, DMU_SECTOR) != sec)
                continue; // Wrong sector.

            return P_Teleport(thing, m->pos[VX], m->pos[VY], m->angle);
        }
    }

    return false;
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t *player)
{
    int         i;
    int         selections;
    float       destPos[2];
    angle_t     destAngle;

    //// \todo Spawn spot selection does not belong in this file.
    if(deathmatch)
    {
        selections = deathmatch_p - deathmatchstarts;
        i = P_Random() % selections;
        destPos[VX] = deathmatchstarts[i].pos[VX];
        destPos[VY] = deathmatchstarts[i].pos[VY];
        destAngle = deathmatchstarts[i].angle;
    }
    else
    {
        destPos[VX] = playerstarts[0].pos[VX];
        destPos[VY] = playerstarts[0].pos[VY];
        destAngle = playerstarts[0].angle;
    }

# if __JHEXEN__
    P_Teleport(player->plr->mo, destX, destY, destAngle, true);
    if(player->morphTics)
    {   // Teleporting away will undo any morph effects (pig)
        P_UndoPlayerMorph(player);
    }
    //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
# else
    P_Teleport(player->plr->mo, destPos[VX], destPos[VY], destAngle);
    /*S_StartSound(sfx_wpnup, NULL); // Full volume laugh
       NetSv_Sound(NULL, sfx_wpnup, player-players); */
    S_StartSound(sfx_wpnup, NULL);
# endif
}
#endif
