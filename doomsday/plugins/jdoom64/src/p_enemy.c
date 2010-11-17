/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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
 * p_enemy.c: Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_map.h"
#include "p_actor.h"
#include "p_door.h"
#include "p_floor.h"

// MACROS ------------------------------------------------------------------

#define FATSPREAD               (ANG90/8)
#define FAT_DELTAANGLE          (85*ANGLE_1) // jd64
#define FAT_ARM_EXTENSION_SHORT (32) // jd64
#define FAT_ARM_EXTENSION_LONG  (16) // jd64
#define FAT_ARM_HEIGHT          (64) // jd64
#define SKULLSPEED              (20)

#define TRACEANGLE              (0xc000000)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void C_DECL A_ReFire(player_t *player, pspdef_t *psp);
void C_DECL A_Fall(mobj_t *actor);
void C_DECL A_Fire(mobj_t *actor);
void C_DECL A_SpawnFly(mobj_t *mo);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mobj_t *corpseHit;

static float dropoffDelta[2], floorZ;

// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309f)
static const float dirSpeed[8][2] =
{
    {1, 0},
    {MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {0, 1},
    {-MOVESPEED_DIAGONAL, MOVESPEED_DIAGONAL},
    {-1, 0},
    {-MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL},
    {0, -1},
    {MOVESPEED_DIAGONAL, -MOVESPEED_DIAGONAL}
};
#undef MOVESPEED_DIAGONAL

// CODE --------------------------------------------------------------------

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

static boolean checkMeleeRange(mobj_t *actor)
{
    mobj_t             *pl;
    float               dist, range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!cfg.netNoMaxZMonsterMeleeAttack)
    {   // Account for Z height difference.
        if(pl->pos[VZ] > actor->pos[VZ] + actor->height ||
           pl->pos[VZ] + pl->height < actor->pos[VZ])
            return false;
    }

    range = MELEERANGE - 20 + pl->info->radius;
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

static boolean checkMissileRange(mobj_t *actor)
{
    float               dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {   // The target just hit the enemy.
        // So fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false; // Do not attack yet.

    // OPTIMIZE: get this from a global checksight.
    dist =
        P_ApproxDistance(actor->pos[VX] - actor->target->pos[VX],
                         actor->pos[VY] - actor->target->pos[VY]) - 64;

    if(P_GetState(actor->type, SN_MELEE) == S_NULL)
        dist -= 128; // No melee attack, so fire more.

    if(actor->type == MT_CYBORG || actor->type == MT_SKULL)
    {
        dist /= 2;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if(P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction. $dropoff_fix
 *
 * @return              @c false, if the move is blocked.
 */
static boolean moveMobj(mobj_t *actor, boolean dropoff)
{
    float               pos[3], step[3];
    linedef_t          *ld;
    boolean             good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if((unsigned) actor->moveDir >= DI_NODIR)
        Con_Error("Weird actor->moveDir!");

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][MX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][MY];
    pos[VX] = actor->pos[VX] + step[VX];
    pos[VY] = actor->pos[VY] + step[VY];

    // $dropoff_fix
    if(!P_TryMove(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Open any specials.
        if((actor->flags & MF_FLOAT) && floatOk)
        {
            // Must adjust height.
            if(actor->pos[VZ] < tmFloorZ)
                actor->pos[VZ] += FLOATSPEED;
            else
                actor->pos[VZ] -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(spechit))
            return false;

        actor->moveDir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            /**
             * If the special is not a door that can be opened, return false.
             *
             * $unstuck: This is what caused monsters to get stuck in
             * doortracks, because it thought that the monster freed itself
             * by opening a door, even if it was moving towards the
             * doortrack, and not the door itself.
             *
             * If a line blocking the monster is activated, return true 90%
             * of the time. If a line blocking the monster is not activated,
             * but some other line is, return false 90% of the time.
             * A bit of randomness is needed to ensure it's free from
             * lockups, but for most cases, it returns the correct result.
             *
             * Do NOT simply return false 1/4th of the time (causes monsters
             * to back out when they shouldn't, and creates secondary
             * stickiness).
             */

            if(P_ActivateLine(ld, actor, 0, SPAC_USE))
                good |= ld == blockLine ? 1 : 2;
        }

        if(!good || cfg.monstersStuckInDoors)
            return good;
        else
            return (P_Random() >= 230) || (good & 1);
    }
    else
    {
        P_MobjSetSRVO(actor, step[VX], step[VY]);
        actor->flags &= ~MF_INFLOAT;
    }

    // $dropoff_fix: fall more slowly, under gravity, if fellDown==true
    if(!(actor->flags & MF_FLOAT) && !fellDown)
    {
        if(actor->pos[VZ] > actor->floorZ)
            P_HitFloor(actor);

        actor->pos[VZ] = actor->floorZ;
    }

    return true;
}

/**
 * Attempts to move actor on in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE
 * If move is either clear or blocked only by a door, returns TRUE and sets...
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
static boolean tryMoveMobj(mobj_t *actor)
{
    // $dropoff_fix
    if(!moveMobj(actor, false))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void doNewChaseDir(mobj_t *actor, float deltaX, float deltaY)
{
    dirtype_t           xdir, ydir, tdir;
    dirtype_t           olddir = actor->moveDir;
    dirtype_t           turnaround = olddir;

    if(turnaround != DI_NODIR) // Find reverse direction.
        turnaround ^= 4;

    xdir = (deltaX > 10 ? DI_EAST : deltaX < -10 ? DI_WEST : DI_NODIR);
    ydir = (deltaY < -10 ? DI_SOUTH : deltaY > 10 ? DI_NORTH : DI_NODIR);

    // Try direct route.
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->moveDir =
                      deltaY < 0 ? deltaX >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltaX >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && tryMoveMobj(actor))
        return;

    // Try other directions.
    if(P_Random() > 200 || fabs(deltaY) > fabs(deltaX))
    {
        dirtype_t temp = xdir;

        xdir = ydir;
        ydir = temp;
    }

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->moveDir = xdir, tryMoveMobj(actor)))
        return; // Either moved forward or attacked.

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->moveDir = ydir, tryMoveMobj(actor)))
        return;

    // There is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->moveDir = olddir, tryMoveMobj(actor)))
        return;

    // Randomly determine direction of search.
    if(P_Random() & 1)
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
    }
    else
    {
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !tryMoveMobj(actor))
        actor->moveDir = DI_NODIR;
}

/**
 * Monsters try to move away from tall dropoffs.
 *
 * In Doom, they were never allowed to hang over dropoffs, and would remain
 * stuck if involuntarily forced over one. This logic, combined with
 * p_map.c::P_TryMove(), allows monsters to free themselves without making
 * them tend to hang over dropoffs.
 */
static boolean PIT_AvoidDropoff(linedef_t* line, void* data)
{
    sector_t*           backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    float*              bbox = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       tmBBox[BOXRIGHT]  > bbox[BOXLEFT] &&
       tmBBox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       tmBBox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       tmBBox[BOXBOTTOM] < bbox[BOXTOP]    &&
       P_BoxOnLineSide(tmBBox, line) == -1)
    {
        sector_t*           frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
        float               front = P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        float               back = P_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        float               d1[2];
        angle_t             angle;

        P_GetFloatpv(line, DMU_DXY, d1);

        // The monster must contact one of the two floors, and the other
        // must be a tall drop off (more than 24).
        if(back == floorZ && front < floorZ - 24)
        {
            angle = R_PointToAngle2(0, 0, d1[0], d1[1]); // Front side drop off.
        }
        else
        {
            if(front == floorZ && back < floorZ - 24)
                angle = R_PointToAngle2(d1[0], d1[1], 0, 0); // Back side drop off.
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        dropoffDelta[VX] -= FIX2FLT(finesine[angle >> ANGLETOFINESHIFT]) * 32;
        dropoffDelta[VY] += FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT]) * 32;
    }

    return true;
}

/**
 * Driver for above
 */
static boolean avoidDropoff(mobj_t *actor)
{
    floorZ = actor->pos[VZ]; // Remember floor height.

    dropoffDelta[VX] = dropoffDelta[VY] = 0;

    VALIDCOUNT++;

    // Check lines.
    P_MobjLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed.
    return !(dropoffDelta[VX] == 0 || dropoffDelta[VY] == 0);
}

static void newChaseDir(mobj_t *actor)
{
    mobj_t             *target = actor->target;
    float               deltaX = target->pos[VX] - actor->pos[VX];
    float               deltaY = target->pos[VY] - actor->pos[VY];

    if(actor->floorZ - actor->dropOffZ > 24 &&
       actor->pos[VZ] <= actor->floorZ &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && avoidDropoff(actor))
    {
        // Move away from dropoff.
        doNewChaseDir(actor, dropoffDelta[VX], dropoffDelta[VY]);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->moveCount = 1;
        return;
    }

    doNewChaseDir(actor, deltaX, deltaY);
}

/**
 * If allaround is false, only look 180 degrees in front.
 *
 * @return              @c true, if a player is targeted.
 */
static boolean lookForPlayers(mobj_t *actor, boolean allAround)
{
    int                 c, stop, playerCount;
    player_t           *player;
    angle_t             an;
    float               dist;

    playerCount = 0;
    for(c = 0; c < MAXPLAYERS; ++c)
    {
        if(players[c].plr->inGame)
            playerCount++;
    }

    // Are there any players?
    if(!playerCount)
        return false;

    c = 0;
    stop = (actor->lastLook - 1) & 3;

    for(;; actor->lastLook = (actor->lastLook + 1) & 3)
    {
        if(!players[actor->lastLook].plr->inGame)
            continue;

        if(c++ == 2 || actor->lastLook == stop)
        {   // Done looking.
            return false;
        }

        player = &players[actor->lastLook];

        if(player->health <= 0)
            continue; // Player is already dead.

        if(!P_CheckSight(actor, player->plr->mo))
            continue; // Player is out of sight.

        if(!allAround)
        {
            an = R_PointToAngle2(actor->pos[VX],
                                 actor->pos[VY],
                                 player->plr->mo->pos[VX],
                                 player->plr->mo->pos[VY]);
            an -= actor->angle;

            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(player->plr->mo->pos[VX] - actor->pos[VX],
                                     player->plr->mo->pos[VY] - actor->pos[VY]);
                // If real close, react anyway.
                if(dist > MELEERANGE)
                    continue; // Behind back.
            }
        }

        actor->target = player->plr->mo;
        return true;
    }
}

static boolean massacreMobj(thinker_t* th, void* context)
{
    int*                count = (int*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(!mo->player && sentient(mo) && (mo->flags & MF_SHOOTABLE))
    {
        P_DamageMobj(mo, NULL, NULL, 10000, false);
        (*count)++;
    }

    return true; // Continue iteration.
}

int P_Massacre(void)
{
    int                 count = 0;

    // Only massacre when actually in a level.
    if(G_GetGameState() == GS_MAP)
    {
        DD_IterateThinkers(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

typedef struct {
    mobjtype_t          type;
    size_t              count;
} countmobjoftypeparams_t;

static boolean countMobjOfType(thinker_t* th, void* context)
{
    countmobjoftypeparams_t *params = (countmobjoftypeparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(params->type == mo->type && mo->health > 0)
        params->count++;

    return true; // Continue iteration.
}

/**
 * DJS - Next up we have an obscene amount of repetion; 15(!) copies of
 * DOOM's A_KeenDie() with only very minor changes.
 *
 * \todo Replace this lot with XG (maybe need to add a new flag for
 * targeting "mobjs like me").
 */

/**
 * kaiser - Used for special stuff. works only per monster!!!
 */
void C_DECL A_RectSpecial(mobj_t* actor)
{
    countmobjoftypeparams_t params;
    int                 sound;
    mobj_t*             mo;
    float               pos[3];

    switch(actor->info->deathSound)
    {
    case 0:
        return;

    case SFX_PODTH1:
    case SFX_PODTH2:
    case SFX_PODTH3:
        sound = SFX_PODTH1 + P_Random() % 3;
        break;

    case SFX_BGDTH1:
    case SFX_BGDTH2:
        sound = SFX_BGDTH1 + P_Random() % 2;
        break;

    default:
        sound = actor->info->deathSound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_CYBORG || actor->type == MT_BITCH)
    {
        // Full volume.
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
        actor->reactionTime += 30;  // jd64
    }
    else
        S_StartSound(sound, actor);

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - 128) << 11);
    pos[VY] += FIX2FLT((P_Random() - 128) << 11);
    pos[VZ] += actor->height / 2;

    if((mo = P_SpawnMobj3fv(MT_KABOOM, pos, P_Random() << 24, 0)))
    {
        S_StartSound(SFX_BAREXP, mo);
        mo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        mo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        mo->target = actor;
    }

    actor->reactionTime--;
    if(actor->reactionTime <= 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH) + 2);
    }

    // Check if there are no more Bitches left in the map.
    params.type = actor->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {   // No Bitches left alive.
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4459; // jd64 was 666.
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST); // jd64 was open.
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - Used for special stuff. works only per monster!!!
 */
void C_DECL A_PossSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4444;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SposSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4445;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_TrooSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = (mo->type == MT_TROOP? 4446 : 4447);
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SargSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4448;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_HeadSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4450;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SkulSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4452;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Bos2Special(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4453;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BossSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4454;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_PainSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4455;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_FattSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4456;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BabySpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4457;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_CybrSpecial(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4458;
        EV_DoDoor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    sector_t           *sec = NULL;
    mobj_t             *targ;

    sec = P_GetPtrp(actor->subsector, DMU_SECTOR);

    if(!sec)
        return;

    actor->threshold = 0; // Any shot will wake us up.
    targ = P_ToXSector(sec)->soundTarget;

    if(targ && (targ->flags & MF_SHOOTABLE))
    {
        actor->target = targ;

        if(actor->flags & MF_AMBUSH)
        {
            if(P_CheckSight(actor, actor->target))
                goto seeyou;
        }
        else
            goto seeyou;
    }

    if(!lookForPlayers(actor, false))
        return;

    // Go into chase state.
  seeyou:
    if(actor->info->seeSound)
    {
        int                 sound;

        switch(actor->info->seeSound)
        {
        case SFX_POSIT1:
        case SFX_POSIT2:
        case SFX_POSIT3:
            sound = SFX_POSIT1 + P_Random() % 3;
            break;

        case SFX_BGSIT1:
        case SFX_BGSIT2:
            sound = SFX_BGSIT1 + P_Random() % 2;
            break;

        default:
            sound = actor->info->seeSound;
            break;
        }

        if(actor->flags2 & MF2_BOSS)
        {   // Full volume.
            S_StartSound(sound | DDSF_NO_ATTENUATION, actor);
        }
        else
        {
            S_StartSound(sound, actor);
        }
    }

    P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
}

/**
 * Used by the demo cyborg to select the camera as a target on spawn.
 */
void C_DECL A_TargetCamera(mobj_t* actor)
{
    int                 i;
    player_t*           player;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player = &players[i];

        if(!player->plr->inGame || !player->plr->mo)
            continue;

        actor->target = player->plr->mo;
        return;
    }

    // Should never get here.
    Con_Error("A_TargetCamera: Could not find suitable target!");
}

/**
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t* actor)
{
    int                 delta;
    statenum_t          state;

    // jd64 >
    if(actor->flags & MF_FLOAT)
    {
        int                 r = P_Random();

        if(r < 64)
            actor->mom[MZ] += 1;
        else if(r < 128)
            actor->mom[MZ] -= 1;
    }
    // < d64tc

    if(actor->reactionTime)
        actor->reactionTime--;

    // Modify target threshold.
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // Turn towards movement direction if not there yet.
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // Look for a new target.
        if(lookForPlayers(actor, true))
        {   // Got a new target.
        }
        else
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        }

        return;
    }

    // Do not attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(!fastParm)
            newChaseDir(actor);

        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       checkMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, state);
        return;
    }

    // Check for missile attack.
    if((state = P_GetState(actor->type, SN_MISSILE)) != S_NULL)
    {
        if(!(!fastParm && actor->moveCount))
        {
            if(checkMissileRange(actor))
            {
                P_MobjChangeState(actor, state);
                actor->flags |= MF_JUSTATTACKED;
                return;
            }
        }
    }

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(lookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->moveCount < 0 || !moveMobj(actor, false))
    {
        newChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

void C_DECL A_RectChase(mobj_t* actor)
{
    A_Chase(actor);
}

void C_DECL A_FaceTarget(mobj_t *actor)
{
    if(!actor->target)
        return;

    actor->turnTime = true; // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;
    actor->angle =
        R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                        actor->target->pos[VX], actor->target->pos[VY]);

    if(actor->target->flags & MF_SHADOW)
        actor->angle += (P_Random() - P_Random()) << 21;
}

void C_DECL A_BspiFaceTarget(mobj_t* actor)
{
    A_FaceTarget(actor);
}

void C_DECL A_PosAttack(mobj_t *actor)
{
    int                 damage;
    angle_t             angle;
    float               slope;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(SFX_PISTOL, actor);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_SPosAttack(mobj_t *actor)
{
    int                 i, damage;
    angle_t             angle, bangle;
    float               slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; ++i)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;

        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

void C_DECL A_EMarineAttack2(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

void C_DECL A_SpidRefire(mobj_t *actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 10)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

/**
 * d64tc: BspiAttack. Throw projectile.
 */
void BabyFire(mobj_t *actor, int type, boolean right)
{
#define BSPISPREAD                  (ANG90/8) //its cheap but it works
#define BABY_DELTAANGLE             (85*ANGLE_1)
#define BABY_ARM_EXTENSION_SHORT    (18)
#define BABY_ARM_HEIGHT             (24)

    mobj_t             *mo;
    angle_t             ang;
    float               pos[3];

    ang = actor->angle;
    if(right)
        ang += BABY_DELTAANGLE;
    else
        ang -= BABY_DELTAANGLE;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += BABY_ARM_EXTENSION_SHORT * FIX2FLT(finecosine[ang]);
    pos[VY] += BABY_ARM_EXTENSION_SHORT * FIX2FLT(finesine[ang]);
    pos[VZ] -= actor->floorClip + BABY_ARM_HEIGHT;

    mo = P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                              actor, actor->target);

    if(right)
        mo->angle += BSPISPREAD/6;
    else
        mo->angle -= BSPISPREAD/6;

    ang = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[ang]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[ang]);

#undef BSPISPREAD
#undef BABY_DELTAANGLE
#undef BABY_ARM_EXTENSION_SHORT
#undef BABY_ARM_HEIGHT
}

/**
 * Shoot two plasmaballs while aligned to cannon (should of been like this
 * in Doom 2! - kaiser).
 */
void C_DECL A_BspiAttack(mobj_t *actor)
{
    BabyFire(actor, MT_ARACHPLAZ, false);
    BabyFire(actor, MT_ARACHPLAZ, true);
}

/**
 * Formerly A_BspiAttack? - DJS
 */
void C_DECL A_TroopAttack(mobj_t *actor)
{
    mobjtype_t          missileType;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // Launch a missile.
    if(actor->type == MT_TROOP)
        missileType = MT_TROOPSHOT;
    else
        missileType = MT_NTROSHOT;

    P_SpawnMissile(missileType, actor, actor->target);
}

/**
 * Formerly A_TroopAttack? - DJS
 */
void C_DECL A_TroopClaw(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        S_StartSound(SFX_CLAW, actor);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, damage, false);
        return;
    }
}

/**
 * Mother Demon: Floorfire attack.
 */
void C_DECL A_MotherFloorFire(mobj_t *actor)
{
/*
#define FIRESPREAD      (ANG90 / 8 * 4)

    mobj_t             *mo;
    int                 an;
*/
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    S_StartSound(SFX_MTHATK, actor);
/*
    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);

    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);
    mo->angle -= FIRESPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);

    mo = P_SpawnMissile(MT_FIREEND, actor, actor->target);
    mo->angle += FIRESPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);

#undef FIRESPREAD
*/
}

static void motherFire(mobj_t *actor, int type, angle_t angle,
                       float distance, float height)
{
    angle_t             ang;
    float               pos[3];

    ang = actor->angle + angle;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += distance * FIX2FLT(finecosine[ang]);
    pos[VY] += distance * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + height;

    P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                         actor, actor->target);
}

/**
 * d64tc: MotherDemon's Missle Attack code
 */
void C_DECL A_MotherMissle(mobj_t *actor)
{
#define MOTHER_DELTAANGLE           (85*ANGLE_1)
#define MOTHER_ARM_EXTENSION_SHORT  (40)
#define MOTHER_ARM_EXTENSION_LONG   (55)
#define MOTHER_ARM1_HEIGHT          (128)
#define MOTHER_ARM2_HEIGHT          (128)
#define MOTHER_ARM3_HEIGHT          (64)
#define MOTHER_ARM4_HEIGHT          (64)

    // Fire 4 missiles at once.
    motherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_SHORT, MOTHER_ARM1_HEIGHT);
    motherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_SHORT, MOTHER_ARM2_HEIGHT);
    motherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_LONG, MOTHER_ARM3_HEIGHT);
    motherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE,
               MOTHER_ARM_EXTENSION_LONG, MOTHER_ARM4_HEIGHT);

#undef MOTHER_DELTAANGLE
#undef MOTHER_ARM_EXTENSION_SHORT
#undef MOTHER_ARM_EXTENSION_LONG
#undef MOTHER_ARM1_HEIGHT
#undef MOTHER_ARM2_HEIGHT
#undef MOTHER_ARM3_HEIGHT
#undef MOTHER_ARM4_HEIGHT
}

/**
 * d64tc - Unused?
 */
void C_DECL A_SetFloorFire(mobj_t *actor)
{
/*
    mobj_t             *mo;
    float               pos[3];

    actor->pos[VZ] = actor->floorZ;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VZ]  = ONFLOORZ;

    if((mo = P_SpawnMobj3fv(MT_SPAWNFIRE, pos)))
        mo->target = actor->target;
*/
}

/**
 * d64tc
 */
void C_DECL A_MotherBallExplode(mobj_t* spread)
{
    int                 i;

    for(i = 0; i < 8; ++i)
    {
        angle_t             angle = i * ANG45;
        mobj_t*             shard;

        if((shard = P_SpawnMobj3fv(MT_HEADSHOT, spread->pos, angle, 0)))
        {
            unsigned int        an = angle >> ANGLETOFINESHIFT;

            shard->target = spread->target;
            shard->mom[MX] = shard->info->speed * FIX2FLT(finecosine[an]);
            shard->mom[MY] = shard->info->speed * FIX2FLT(finesine[an]);
        }
    }
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack.
 */
void C_DECL A_RectTracerPuff(mobj_t *smoke)
{
    if(!smoke)
        return;

    P_SpawnMobj3fv(MT_MOTHERPUFF, smoke->pos, P_Random() << 24, 0);
}

void C_DECL A_SargAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage, false);
    }
}

void C_DECL A_ShadowsAction1(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

void C_DECL A_ShadowsAction2(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

void C_DECL A_HeadAttack(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage, false);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_HEADSHOT, actor, actor->target);
}

/**
 * Cyber Demon: Missile Attack.
 *
 * Heavily modified for d64tc.
 */
void C_DECL A_CyberAttack(mobj_t *actor)
{
#define CYBER_DELTAANGLE            (85*ANGLE_1)
#define CYBER_ARM_EXTENSION_SHORT   (35)
#define CYBER_ARM1_HEIGHT           (68)

    angle_t             ang;
    float               pos[3];

    // This aligns the rocket to the d64tc cyberdemon's rocket launcher.
    ang = (actor->angle + CYBER_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finecosine[ang]);
    pos[VY] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + CYBER_ARM1_HEIGHT;

    P_SpawnMotherMissile(MT_CYBERROCKET, pos[VX], pos[VY], pos[VZ],
                         actor, actor->target);

#undef CYBER_DELTAANGLE
#undef CYBER_ARM_EXTENSION_SHORT
#undef CYBER_ARM1_HEIGHT
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int                 damage;
    mobjtype_t          missileType;

    if(!actor->target)
        return;

    if(checkMeleeRange(actor))
    {
        S_StartSound(SFX_CLAW, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage, false);
        return;
    }

    // Launch a missile.
    if(actor->type == MT_BRUISER)
        missileType = MT_BRUISERSHOTRED;
    else
        missileType = MT_BRUISERSHOT;

    P_SpawnMissile(missileType, actor, actor->target);
}

void C_DECL A_SkelMissile(mobj_t* actor)
{
    mobj_t*             mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if((mo = P_SpawnMissile(MT_TRACER, actor, actor->target)))
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t* actor)
{
    angle_t             exact;
    float               dist, slope;
    mobj_t*             dest, *th;

    if((int) GAMETIC & 3)
        return;

    // Spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROCKETPUFF, actor->pos[VX],
                      actor->pos[VY],
                      actor->pos[VZ], actor->angle + ANG180);

    if((th = P_SpawnMobj3f(MT_SMOKE, actor->pos[VX] - actor->mom[MX],
                           actor->pos[VY] - actor->mom[MY], actor->pos[VZ],
                           actor->angle + ANG180, 0)))
    {
        th->mom[MZ] = 1;
        th->tics -= P_Random() & 3;
        if(th->tics < 1)
            th->tics = 1;
    }

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    exact = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            dest->pos[VX], dest->pos[VY]);

    if(exact != actor->angle)
    {
        if(exact - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(exact - actor->angle < 0x80000000)
                actor->angle = exact;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(exact - actor->angle > 0x80000000)
                actor->angle = exact;
        }
    }

    exact = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FIX2FLT(FixedMul(actor->info->speed, finecosine[exact]));
    actor->mom[MY] = FIX2FLT(FixedMul(actor->info->speed, finesine[exact]));

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);

    dist /= FIX2FLT(actor->info->speed);

    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

void C_DECL A_SkelWhoosh(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    S_StartSound(SFX_SKESWG, actor);
}

void C_DECL A_SkelFist(mobj_t *actor)
{
    int                 damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(SFX_SKEPCH, actor);
        P_DamageMobj(actor->target, actor, actor, damage, false);
    }
}

void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(SFX_MANATK, actor);
}

/**
 * d64tc: Used for mancubus projectile.
 */
static void fatFire(mobj_t *actor, int type, angle_t spread, angle_t angle,
                    float distance, float height)
{
    mobj_t             *mo;
    angle_t             an;
    float               pos[3];

    an = (actor->angle + angle) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += distance * FIX2FLT(finecosine[an]);
    pos[VY] += distance * FIX2FLT(finesine[an]);
    pos[VZ] += -actor->floorClip + height;

    mo = P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ],
                              actor, actor->target);
    mo->angle += spread;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack1(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, FATSPREAD * 1.5, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack2(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, -(FATSPREAD * 1.5), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/**
 * d64tc
 */
void C_DECL A_FatAttack3(mobj_t *actor)
{
    fatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    fatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/**
 * LostSoul Attack: Fly at the player like a missile.
 */
void C_DECL A_SkullAttack(mobj_t *actor)
{
    mobj_t             *dest;
    uint                an;
    float               dist;

    if(!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = SKULLSPEED * FIX2FLT(finecosine[an]);
    actor->mom[MY] = SKULLSPEED * FIX2FLT(finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist /= SKULLSPEED;

    if(dist < 1)
        dist = 1;
    actor->mom[MZ] =
        (dest->pos[VZ] + (dest->height / 2) - actor->pos[VZ]) / dist;
}

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    float               pos[3];
    mobj_t*             newmobj;
    uint                an;
    float               prestep;
    sector_t*           sec;

    if(cfg.maxSkulls)
    {   // Limit the number of MT_SKULL's we should spawn.
        countmobjoftypeparams_t params;

        // Count total number currently on the map.
        params.type = MT_SKULL;
        params.count = 0;
        DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

        if(params.count > 20)
            return; // Too many, don't spit another.
    }

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 +
        3 * ((actor->info->radius + MOBJINFO[MT_SKULL].radius) / 2);

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    // Compat option to prevent spawning lost souls inside walls.
    if(!cfg.allowSkullsInWalls)
    {
        /**
         * Check whether the Lost Soul is being fired through a 1-sided
         * wall or an impassible line, or a "monsters can't cross" line.
         * If it is, then we don't allow the spawn.
         */

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        if(!(newmobj = P_SpawnMobj3fv(MT_SKULL, pos, angle, 0)))
            return;

        sec = P_GetPtrp(newmobj->subsector, DMU_SECTOR);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->pos[VZ] >
              (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newmobj->height)) ||
           (newmobj->pos[VZ] < P_GetFloatp(sec, DMU_FLOOR_HEIGHT)))
        {
            // Kill it immediately.
            P_DamageMobj(newmobj, actor, actor, 10000, false);
            return;
        }
    }
    else
    {   // Use the original DOOM method.
        if(!(newmobj = P_SpawnMobj3fv(MT_SKULL, pos, angle, 0)))
            return;
    }

    // Check for movements, $dropoff_fix.
    if(!P_TryMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false, false))
    {
        // Kill it immediately.
        P_DamageMobj(newmobj, actor, actor, 10000, false);
        return;
    }

    newmobj->target = actor->target;
    A_SkullAttack(newmobj);
}

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // jd64 - Shoots two lost souls from left and right side.
    A_PainShootSkull(actor, actor->angle + ANG270);
    A_PainShootSkull(actor, actor->angle + ANG90);
}

void C_DECL A_PainDie(mobj_t* actor)
{
    angle_t                 an;

    A_Fall(actor);

    switch(P_Random() % 3)
    {
    case 0: an = ANG90; break;
    case 1: an = ANG180; break;
    case 2: an = ANG270; break;
    }

    A_PainShootSkull(actor, actor->angle + an);
}

/**
 * d64tc: Rocket Trail Puff
 * kaiser - Current Rocket Puff code unknown because I know squat.
 *          A fixed version of the pain attack code.
 *
 * DJS - This looks to be doing something similar to the pain elemental
 *       above in that it could possibily spawn mobjs in the void. In this
 *       instance its of little consequence as they are just for fx.
 */
void A_Rocketshootpuff(mobj_t* actor, angle_t angle)
{
    uint                an;
    float               prestep;
    float               pos[3];
    mobj_t*             mo;

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 + 3 *
              (actor->info->radius + MOBJINFO[MT_ROCKETPUFF].radius) / 2;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    if((mo = P_SpawnMobj3fv(MT_ROCKETPUFF, pos, angle, 0)))
    {
        // Check for movements $dropoff_fix.
        if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY], false, false))
        {
            // Kill it immediately.
            P_DamageMobj(mo, actor, actor, 10000, false);
        }
    }
}

void C_DECL A_Scream(mobj_t *actor)
{
    int                 sound;

    if(actor->player)
    {
        sound = SFX_PLDETH; // Default death sound.

        if(actor->health < -50)
        {
            // If the player dies less with less than -50% without gibbing.
            sound = SFX_PDIEHI;
        }

        S_StartSound(sound, actor);
        return;
    }

    switch(actor->info->deathSound)
    {
    case 0:
        return;

    case SFX_PODTH1:
    case SFX_PODTH2:
    case SFX_PODTH3:
        sound = SFX_PODTH1 + P_Random() % 3;
        break;

    case SFX_BGDTH1:
    case SFX_BGDTH2:
        sound = SFX_BGDTH1 + P_Random() % 2;
        break;

    default:
        sound = actor->info->deathSound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_BITCH)
    {
        // Full volume.
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
        actor->reactionTime += 30;  // jd64
    }
    else
        S_StartSound(sound, actor);
}

/**
 * d64tc
 */
void C_DECL A_CyberDeath(mobj_t* actor)
{
    int                 i;
    mobj_t*             mo;
    float               pos[3];
    linedef_t*          dummyLine;
    countmobjoftypeparams_t params;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - 128) << 11);
    pos[VY] += FIX2FLT((P_Random() - 128) << 11);
    pos[VZ] += actor->height / 2;

    if((mo = P_SpawnMobj3fv(MT_KABOOM, pos, P_Random() << 24, 0)))
    {
        S_StartSound(SFX_BAREXP, mo);
        mo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        mo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        mo->target = actor;
    }

    actor->reactionTime--;
    if(actor->reactionTime <= 0)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH) + 2);
    }

    S_StartSound(actor->info->deathSound | DDSF_NO_ATTENUATION, NULL);

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if((gameMap != 31) && (gameMap != 32) && (gameMap != 34))
        return;

    // Make sure there is a player alive for victory.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return; // No one left alive, so do not end game.

    // Scan the remaining thinkers to see if all bosses are dead.
    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(params.count)
    {   // Other boss not dead.
        return;
    }

    if(gameMap == 31 || gameMap == 32)
    {
        dummyLine = P_AllocDummyLine();
        P_ToXLine(dummyLine)->tag = 666;
        EV_DoDoor(dummyLine, DT_BLAZERAISE);

        P_FreeDummyLine(dummyLine);
        return;
    }
    else if(gameMap == 34)
    {
        G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
    }
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack
 */
void C_DECL A_Rocketpuff(mobj_t *actor)
{
    if(!actor)
        return;

    P_SpawnMobj3fv(MT_ROCKETPUFF, actor->pos, P_Random() << 24, 0);
}

/**
 * d64tc
 */
void C_DECL A_Lasersmoke(mobj_t *mo)
{
    if(!mo)
        return;

    P_SpawnMobj3fv(MT_LASERDUST, mo->pos, P_Random() << 24, 0);
}

void C_DECL A_XScream(mobj_t *actor)
{
    S_StartSound(SFX_SLOP, actor);
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Fall(mobj_t *actor)
{
    // Actor is on ground, it can be walked over.
    actor->flags &= ~MF_SOLID;
}

void C_DECL A_Explode(mobj_t *mo)
{
    P_RadiusAttack(mo, mo->target, 128, 127);
}

void C_DECL A_BarrelExplode(mobj_t* actor)
{
    int                 i;
    linedef_t*          dummyLine;
    countmobjoftypeparams_t params;

    S_StartSound(actor->info->deathSound, actor);
    P_RadiusAttack(actor, actor->target, 128, 127);

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if(gameMap != 0)
        return;

    if(actor->type != MT_BARREL)
        return;

    // Make sure there is a player alive for victory.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return; // No one left alive, so do not end game.

    // Scan the remaining thinkers to see if all bosses are dead.
    params.type = actor->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(params.count)
    {   // Other boss not dead.
        return;
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 666;
    EV_DoDoor(dummyLine, DT_BLAZERAISE);

    P_FreeDummyLine(dummyLine);
}

/**
 * Possibly trigger special effects if on first boss level
 *
 * kaiser - Removed exit special at end to allow MT_FATSO to properly
 *          work in Map33 for d64tc.
 */
void C_DECL A_BossDeath(mobj_t* mo)
{
    int                 i;
    countmobjoftypeparams_t params;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if(gameMap != 29)
        return;

    if(mo->type != MT_BITCH)
        return;

    // Make sure there is a player alive for victory.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return; // No one left alive, so do not end game.

    // Scan the remaining thinkers to see if all bosses are dead.
    params.type = mo->type;
    params.count = 0;
    DD_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(params.count)
    {   // Other boss not dead.
        return;
    }

    G_LeaveMap(G_GetNextMap(gameEpisode, gameMap, false), 0, false);
}

void C_DECL A_Hoof(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_HOOF | (gameMap == 7 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_MEAL | (gameMap == 7 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(SFX_BSPWLK, mo);
    A_Chase(mo);
}
