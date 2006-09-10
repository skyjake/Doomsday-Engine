/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2006 Daniel Swanson <danij@dengine.net>
 * Copyright © 1999 Activision
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

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

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

void P_ArtiTeleportOther(player_t *player)
{
    mobj_t *mo;

    mo = P_SpawnPlayerMissile(player->plr->mo, MT_TELOTHER_FX1);

    if(mo)
        mo->target = player->plr->mo;
}

void P_TeleportToPlayerStarts(mobj_t *victim)
{
    int     i, selections = 0;
    fixed_t destX, destY;
    angle_t destAngle;
    thing_t *start;

    for(i = 0; i < MAXPLAYERS; i++)
    {
        if(!players[i].plr->ingame)
            continue;

        selections++;
    }

    i = P_Random() % selections;
    start = P_GetPlayerStart(0, i);

    destX = start->x << FRACBITS;
    destY = start->y << FRACBITS;
    destAngle = ANG45 * (playerstarts[i].angle / 45);

    P_Teleport(victim, destX, destY, destAngle, true);
    //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
}

void P_TeleportToDeathmatchStarts(mobj_t *victim)
{
    int     i, selections;
    fixed_t destX, destY;
    angle_t destAngle;

    selections = deathmatch_p - deathmatchstarts;
    if(selections)
    {
        i = P_Random() % selections;
        destX = deathmatchstarts[i].x << FRACBITS;
        destY = deathmatchstarts[i].y << FRACBITS;
        destAngle = ANG45 * (deathmatchstarts[i].angle / 45);

        P_Teleport(victim, destX, destY, destAngle, true);
        //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
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
        // If death action, run it upon teleport
        if(victim->flags & MF_COUNTKILL && victim->special)
        {
            P_RemoveMobjFromTIDList(victim);
            P_ExecuteLineSpecial(victim->special, victim->args, NULL, 0,
                                 victim);
            victim->special = 0;
        }

        // Send all monsters to deathmatch spots
        P_TeleportToDeathmatchStarts(victim);
    }
}

mobj_t *P_SpawnTeleFog(int x, int y)
{
    subsector_t *ss = R_PointInSubsector(x, y);

    return P_SpawnMobj(x, y, P_GetFixedp(ss, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_HEIGHT) +
                       TELEFOGHEIGHT, MT_TFOG);
}

//==========================================================================
//
// P_Teleport
//
//==========================================================================

boolean P_Teleport(mobj_t *thing, fixed_t x, fixed_t y, angle_t angle,
                   boolean useFog)
{
    fixed_t oldpos[3];
    fixed_t aboveFloor;
    fixed_t fogDelta;
    player_t *player;
    unsigned an;
    mobj_t *fog;

    memcpy(oldpos, thing->pos, sizeof(oldpos));

    aboveFloor = thing->pos[VZ] - thing->floorz;
    if(!P_TeleportMove(thing, x, y, false))
        return false;

    if(thing->player)
    {
        player = thing->player;
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
        if(player->powers[pw_flight] && aboveFloor)
        {
            thing->pos[VZ] = thing->floorz + aboveFloor;
            if(thing->pos[VZ] + thing->height > thing->ceilingz)
            {
                thing->pos[VZ] = thing->ceilingz - thing->height;
            }
            player->plr->viewz = thing->pos[VZ] + player->plr->viewheight;
        }
        else
        {
            thing->pos[VZ] = thing->floorz;
            player->plr->viewz = thing->pos[VZ] + player->plr->viewheight;
            if(useFog)
            {
                player->plr->lookdir = 0;
            }
        }
    }
    else if(thing->flags & MF_MISSILE)
    {
        thing->pos[VZ] = thing->floorz + aboveFloor;
        if(thing->pos[VZ] + thing->height > thing->ceilingz)
        {
            thing->pos[VZ] = thing->ceilingz - thing->height;
        }
    }
    else
    {
        thing->pos[VZ] = thing->floorz;
    }
    // Spawn teleport fog at source and destination
    if(useFog)
    {
        fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
        fog = P_SpawnMobj(oldpos[VX], oldpos[VY], oldpos[VZ] + fogDelta, MT_TFOG);
        S_StartSound(SFX_TELEPORT, fog);
        an = angle >> ANGLETOFINESHIFT;
        fog =
            P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                        thing->pos[VZ] + fogDelta, MT_TFOG);
        S_StartSound(SFX_TELEPORT, fog);
        if(thing->player && !thing->player->powers[pw_speed])
        {                       // Freeze player for about .5 sec
            thing->reactiontime = 18;
        }
        thing->angle = angle;
    }
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
    if(thing->flags & MF_MISSILE)
    {
        angle >>= ANGLETOFINESHIFT;
        thing->momx = FixedMul(thing->info->speed, finecosine[angle]);
        thing->momy = FixedMul(thing->info->speed, finesine[angle]);
    }
    else if(useFog)             // no fog doesn't alter the player's momentums
    {
        thing->momx = thing->momy = thing->momz = 0;
    }
    P_ClearThingSRVO(thing);

    // Update the floor pic
    thing->floorpic = tmfloorpic;
    return true;
}

//==========================================================================
//
// EV_Teleport
//
//==========================================================================

boolean EV_Teleport(int tid, mobj_t *thing, boolean fog)
{
    int     i;
    int     count;
    mobj_t *mo = 0;
    int     searcher;

    if(!thing)
    {                           // Teleport function called with an invalid mobj
        return false;
    }
    if(thing->flags2 & MF2_NOTELEPORT)
    {
        return false;
    }
    count = 0;
    searcher = -1;
    while(P_FindMobjFromTID(tid, &searcher) != NULL)
    {
        count++;
    }
    if(count == 0)
    {
        return false;
    }
    count = 1 + (P_Random() % count);
    searcher = -1;
    for(i = 0; i < count; i++)
    {
        mo = P_FindMobjFromTID(tid, &searcher);
    }
    if(!mo)
        Con_Error("Can't find teleport mapspot\n");
    return P_Teleport(thing, mo->pos[VX], mo->pos[VY], mo->angle, fog);
}

#if __JHERETIC__ || __JHEXEN__
void P_ArtiTele(player_t *player)
{
    int     i;
    int     selections;
    fixed_t destX;
    fixed_t destY;
    angle_t destAngle;

    if(deathmatch)
    {
        selections = deathmatch_p - deathmatchstarts;
        i = P_Random() % selections;
        destX = deathmatchstarts[i].x << FRACBITS;
        destY = deathmatchstarts[i].y << FRACBITS;
        destAngle = ANG45 * (deathmatchstarts[i].angle / 45);
    }
    else
    {
        // FIXME?: DJS - this doesn't seem right...
        destX = playerstarts[0].x << FRACBITS;
        destY = playerstarts[0].y << FRACBITS;
        destAngle = ANG45 * (playerstarts[0].angle / 45);
    }

# if __JHEXEN__
    P_Teleport(player->plr->mo, destX, destY, destAngle, true);
    if(player->morphTics)
    {   // Teleporting away will undo any morph effects (pig)
        P_UndoPlayerMorph(player);
    }
    //S_StartSound(NULL, sfx_wpnup); // Full volume laugh
# else
    P_Teleport(player->plr->mo, destX, destY, destAngle);
    /*S_StartSound(sfx_wpnup, NULL); // Full volume laugh
       NetSv_Sound(NULL, sfx_wpnup, player-players); */
    S_StartSound(sfx_wpnup, NULL);
# endif
}
#endif
