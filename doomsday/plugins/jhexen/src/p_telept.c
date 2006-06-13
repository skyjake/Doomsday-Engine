
//**************************************************************************
//**
//** p_telept.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "h2def.h"
#include "p_local.h"
#include "soundst.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_SpawnTeleFog
//
//==========================================================================

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
