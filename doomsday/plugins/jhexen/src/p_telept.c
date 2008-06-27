/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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

void P_ArtiTeleportOther(player_t *plr)
{
    mobj_t         *mo;

    mo = P_SpawnPlayerMissile(MT_TELOTHER_FX1, plr->plr->mo);
    if(mo)
        mo->target = plr->plr->mo;
}

void P_TeleportToPlayerStarts(mobj_t *victim)
{
    int             i, selections = 0;
    float           destPos[3];
    angle_t         destAngle;
    spawnspot_t    *start;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!players[i].plr->inGame)
            continue;

        selections++;
    }

    i = P_Random() % selections;
    start = P_GetPlayerStart(0, i);

    destPos[VX] = start->pos[VX];
    destPos[VY] = start->pos[VY];
    destAngle = playerStarts[i].angle;

    P_Teleport(victim, destPos[VX], destPos[VY], destAngle, true);
}

void P_TeleportToDeathmatchStarts(mobj_t *victim)
{
    int             i, selections;
    float           destPos[3];
    angle_t         destAngle;

    selections = deathmatchP - deathmatchStarts;
    if(selections)
    {
        i = P_Random() % selections;
        destPos[VX] = deathmatchStarts[i].pos[VX];
        destPos[VY] = deathmatchStarts[i].pos[VY];
        destAngle = deathmatchStarts[i].angle;

        P_Teleport(victim, destPos[VX], destPos[VY], destAngle, true);
    }
    else
    {
        P_TeleportToPlayerStarts(victim);
    }
}

void P_TeleportOther(mobj_t *victim)
{
    if(victim->player)
    {
        if(deathmatch)
            P_TeleportToDeathmatchStarts(victim);
        else
            P_TeleportToPlayerStarts(victim);
    }
    else
    {
        // If death action, run it upon teleport.
        if(victim->flags & MF_COUNTKILL && victim->special)
        {
            P_MobjRemoveFromTIDList(victim);
            P_ExecuteLineSpecial(victim->special, victim->args, NULL, 0,
                                 victim);
            victim->special = 0;
        }

        // Send all monsters to deathmatch spots.
        P_TeleportToDeathmatchStarts(victim);
    }
}

mobj_t *P_SpawnTeleFog(float x, float y)
{
    float       fheight;

    fheight = P_GetFloatp(R_PointInSubsector(x, y), DMU_FLOOR_HEIGHT);

    return P_SpawnMobj3f(MT_TFOG, x, y, fheight + TELEFOGHEIGHT);
}

boolean P_Teleport(mobj_t *mo, float x, float y, angle_t angle,
                   boolean useFog)
{
    float       oldpos[3], aboveFloor, fogDelta;
    player_t   *player;
    unsigned    an;
    mobj_t     *fog;

    memcpy(oldpos, mo->pos, sizeof(oldpos));

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
        fogDelta = FIX2FLT(mo->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT);
        fog = P_SpawnMobj3f(MT_TFOG, oldpos[VX], oldpos[VY], oldpos[VZ] + fogDelta);

        S_StartSound(SFX_TELEPORT, fog);

        an = angle >> ANGLETOFINESHIFT;
        fog = P_SpawnMobj3f(MT_TFOG,
                            x + 20 * FIX2FLT(finecosine[an]),
                            y + 20 * FIX2FLT(finesine[an]),
                            mo->pos[VZ] + fogDelta);
        S_StartSound(SFX_TELEPORT, fog);

        if(mo->player && !mo->player->powers[PT_SPEED])
        {   // Freeze player for about .5 sec
            mo->reactionTime = 18;
        }
        mo->angle = angle;
    }

    if(mo->flags2 & MF2_FLOORCLIP)
    {
        if(mo->pos[VZ] ==
           P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT) &&
           P_MobjGetFloorTerrainType(mo) >= FLOOR_LIQUID)
        {
            mo->floorClip = 10;
        }
        else
        {
            mo->floorClip = 0;
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

    // Update the floor material.
    mo->floorMaterial = tmFloorMaterial;
    return true;
}

boolean EV_Teleport(int tid, mobj_t* thing, boolean fog)
{
    int             i, count, searcher;
    mobj_t*         mo = 0;

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
    int             i, selections;
    float           destPos[3];
    angle_t         destAngle;

    if(deathmatch)
    {
        selections = deathmatchP - deathmatchStarts;
        i = P_Random() % selections;

        destPos[VX] = deathmatchStarts[i].pos[VX];
        destPos[VY] = deathmatchStarts[i].pos[VY];
        destAngle = deathmatchStarts[i].angle;
    }
    else
    {
        //// \fixme DJS - this doesn't seem right... why always player 0?
        destPos[VX] = playerStarts[0].pos[VX];
        destPos[VY] = playerStarts[0].pos[VY];
        destAngle = playerStarts[0].angle;
    }

# if __JHEXEN__
    P_Teleport(player->plr->mo, destPos[VX], destPos[VY], destAngle, true);
    if(player->morphTics)
    {   // Teleporting away will undo any morph effects (pig)
        P_UndoPlayerMorph(player);
    }

# else
    P_Teleport(player->plr->mo, destPos[VX], destPos[VY], destAngle);
    S_StartSound(SFX_WPNUP, NULL);
# endif
}
#endif
