

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "h_stat.h"
#include "soundst.h"
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

boolean P_Teleport(mobj_t *thing, fixed_t x, fixed_t y, angle_t angle)
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
    {
        return (false);
    }
    if(thing->player)
    {
        player = thing->player;
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
            player->plr->clLookDir = 0;
            player->plr->lookdir = 0;
        }
        player->plr->clAngle = angle;
        player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
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
    fogDelta = thing->flags & MF_MISSILE ? 0 : TELEFOGHEIGHT;
    fog = P_SpawnMobj(oldpos[VX], oldpos[VY], oldpos[VZ] + fogDelta, MT_TFOG);
    S_StartSound(sfx_telept, fog);
    an = angle >> ANGLETOFINESHIFT;
    fog =
        P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                    thing->pos[VZ] + fogDelta, MT_TFOG);
    S_StartSound(sfx_telept, fog);
    if(thing->player && !thing->player->powers[pw_weaponlevel2])
    {                           // Freeze player for about .5 sec
        thing->reactiontime = 18;
    }
    thing->angle = angle;
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
    else
    {
        thing->momx = thing->momy = thing->momz = 0;
    }
    P_ClearThingSRVO(thing);
    return (true);
}

boolean EV_Teleport(line_t *line, int side, mobj_t *thing)
{
    int     i;
    int     tag;
    mobj_t *m;
    thinker_t *thinker;
    sector_t *sector;

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
                // Not a mobj
                if(thinker->function != P_MobjThinker)
                    continue;

                m = (mobj_t *) thinker;

                // Not a teleportman
                if(m->type != MT_TELEPORTMAN)
                    continue;

                sector = P_GetPtrp(m->subsector, DMU_SECTOR);
                // wrong sector
                if(P_ToIndex(sector) != i)
                    continue;

                return (P_Teleport(thing, m->pos[VX], m->pos[VY], m->angle));
            }
        }
    }
    return (false);
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
