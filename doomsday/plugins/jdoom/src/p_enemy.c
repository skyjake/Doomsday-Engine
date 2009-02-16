/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_enemy.c: Enemy thinking, AI (jDoom-specific).
 *
 * Action Pointer Functions that are associated with STATES/frames.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jdoom.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_door.h"
#include "p_floor.h"

// MACROS ------------------------------------------------------------------

#define FATSPREAD               (ANG90/8)
#define SKULLSPEED              (20)
#define TRACEANGLE              (0xc000000)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;

mobj_t **brainTargets;
int numBrainTargets;
int numBrainTargetsAlloc;
braindata_t brain; // Global state of boss brain.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mobj_t *corpseHit;
static mobj_t *vileObj;
static float vileTry[3];

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
    {
        // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false; // Do not attack yet.

    dist =
        P_ApproxDistance(actor->pos[VX] - actor->target->pos[VX],
                         actor->pos[VY] - actor->target->pos[VY]) - 64;

    if(P_GetState(actor->type, SN_MELEE) == S_NULL)
        dist -= 128; // No melee attack, so fire more.

    if(actor->type == MT_VILE)
    {
        if(dist > 14 * 64)
            return false; // Too far away.
    }

    if(actor->type == MT_UNDEAD)
    {
        if(dist < 196)
            return false; // Close for fist attack.
        dist /= 2;
    }

    if(actor->type == MT_CYBORG || actor->type == MT_SPIDER ||
       actor->type == MT_SKULL)
    {
        dist /= 2;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if((float) P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction if not blocked.
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
 * Driver for above.
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

        if(P_MobjIsCamera(player->plr->mo))
            continue;

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

/**
 * Kills all monsters.
 */
int P_Massacre(void)
{
    int                 count = 0;

    // Only massacre when actually in a map.
    if(G_GetGameState() == GS_MAP)
    {
        P_IterateThinkers(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

static boolean findBrainTarget(thinker_t* th, void* context)
{
    mobj_t*             mo = (mobj_t *) th;

    if(mo->type == MT_BOSSTARGET)
    {
        if(numBrainTargets >= numBrainTargetsAlloc)
        {
            // Do we need to alloc more targets?
            if(numBrainTargets == numBrainTargetsAlloc)
            {
                numBrainTargetsAlloc *= 2;
                brainTargets =
                    Z_Realloc(brainTargets,
                              numBrainTargetsAlloc * sizeof(*brainTargets),
                              PU_MAP);
            }
            else
            {
                numBrainTargetsAlloc = 32;
                brainTargets =
                    Z_Malloc(numBrainTargetsAlloc * sizeof(*brainTargets),
                             PU_MAP, NULL);
            }
        }

        brainTargets[numBrainTargets++] = mo;
    }

    return true; // Continue iteration.
}

/**
 * Initialize boss brain targets at map startup, rather than at boss
 * wakeup, to prevent savegame-related crashes.
 *
 * \todo Does not belong in this file, find it a better home.
 */
void P_SpawnBrainTargets(void)
{
    // Find all the target spots.
    P_IterateThinkers(P_MobjThinker, findBrainTarget, NULL);
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
 * DOOM II special, map 32. Uses special tag 666.
 */
void C_DECL A_KeenDie(mobj_t* mo)
{
    countmobjoftypeparams_t params;

    A_Fall(mo);

    // Check if there are no more Keens left in the map.
    params.type = mo->type;
    params.count = 0;
    P_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(!params.count)
    {   // No Keens left alive.
        linedef_t*          dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 666;
        EV_DoDoor(dummyLine, DT_OPEN);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t* actor)
{
    sector_t*           sec = NULL;
    mobj_t*             targ;

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
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t* actor)
{
    int                 delta;
    statenum_t          state;

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

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE) ||
       P_MobjIsCamera(actor->target))
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
        if(gameSkill != SM_NIGHTMARE && !fastParm)
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
        if(!(gameSkill < SM_NIGHTMARE && !fastParm && actor->moveCount))
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

void C_DECL A_CPosAttack(mobj_t* actor)
{
    int                 angle, bangle, damage;
    float               slope;

    if(!actor->target)
        return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire(mobj_t* actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 40)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

void C_DECL A_SpidRefire(mobj_t* actor)
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

void C_DECL A_BspiAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // Launch a missile.
    P_SpawnMissile(MT_ARACHPLAZ, actor, actor->target);
}

void C_DECL A_TroopAttack(mobj_t *actor)
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

    P_SpawnMissile(MT_TROOPSHOT, actor, actor->target);
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
 */
void C_DECL A_CyberAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_ROCKET, actor, actor->target);
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int                 damage;

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
    P_SpawnMissile(MT_BRUISERSHOT, actor, actor->target);
}

void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t             *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(MT_TRACER, actor, actor->target);
    if(mo)
    {
        mo->pos[VX] += mo->mom[MX];
        mo->pos[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t *actor)
{
    uint                an;
    angle_t             angle;
    float               dist;
    float               slope;
    mobj_t             *dest;
    mobj_t             *th;

    if((int) GAMETIC & 3)
        return;

    // Spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROCKETPUFF, actor->pos[VX],
                      actor->pos[VY],
                      actor->pos[VZ], actor->angle + ANG180);

    th = P_SpawnMobj3f(MT_SMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ], actor->angle + ANG180);

    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Adjust direction.
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // Change angle.
    angle = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            dest->pos[VX], dest->pos[VY]);

    if(angle != actor->angle)
    {
        if(angle - actor->angle > 0x80000000)
        {
            actor->angle -= TRACEANGLE;
            if(angle - actor->angle < 0x80000000)
                actor->angle = angle;
        }
        else
        {
            actor->angle += TRACEANGLE;
            if(angle - actor->angle > 0x80000000)
                actor->angle = angle;
        }
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist /= actor->info->speed;

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

/**
 * Detect a corpse that could be raised.
 */
boolean PIT_VileCheck(mobj_t *thing, void *data)
{
    float               maxdist;
    boolean             check;

    if(!(thing->flags & MF_CORPSE))
        return true; // Not a monster.

    if(thing->tics != -1)
        return true; // Not lying still yet.

    if(P_GetState(thing->type, SN_RAISE) == S_NULL)
        return true; // Monster doesn't have a raise state.

    maxdist = thing->info->radius + MOBJINFO[MT_VILE].radius;

    if(fabs(thing->pos[VX] - vileTry[VX]) > maxdist ||
       fabs(thing->pos[VY] - vileTry[VY]) > maxdist)
        return true; // Not actually touching.

    corpseHit = thing;
    corpseHit->mom[MX] = corpseHit->mom[MY] = 0;

    // DJS - Used the PRBoom method to fix archvile raising ghosts
    // If !raiseghosts then ressurect a "normal" MF_SOLID one.
    if(cfg.raiseGhosts)
    {
        corpseHit->height *= 2*2;
        check = P_CheckPosition2f(corpseHit, corpseHit->pos[VX], corpseHit->pos[VY]);
        corpseHit->height /= 2*2;
    }
    else
    {
        float       radius, height;

        height = corpseHit->height; // Save temporarily.
        radius = corpseHit->radius; // Save temporarily.
        corpseHit->height = corpseHit->info->height;
        corpseHit->radius = corpseHit->info->radius;
        corpseHit->flags |= MF_SOLID;

        check = P_CheckPosition2f(corpseHit, corpseHit->pos[VX], corpseHit->pos[VY]);

        corpseHit->height = height; // Restore.
        corpseHit->radius = radius; // Restore.
        corpseHit->flags &= ~MF_SOLID;
    }
    // End raiseghosts.

    if(!check)
        return true; // Doesn't fit here.

    return false; // Got one, so stop checking.
}

/**
 * Check for ressurecting a body.
 */
void C_DECL A_VileChase(mobj_t *actor)
{
    mobjinfo_t         *info;
    mobj_t             *temp;
    float               box[4];

    if(actor->moveDir != DI_NODIR)
    {
        // Check for corpses to raise.
        vileTry[VX] = actor->pos[VX] +
            actor->info->speed * dirSpeed[actor->moveDir][VX];
        vileTry[VY] = actor->pos[VY] +
            actor->info->speed * dirSpeed[actor->moveDir][VY];

        box[BOXLEFT]   = vileTry[VX] - MAXRADIUS * 2;
        box[BOXRIGHT]  = vileTry[VX] + MAXRADIUS * 2;
        box[BOXBOTTOM] = vileTry[VY] - MAXRADIUS * 2;
        box[BOXTOP]    = vileTry[VY] + MAXRADIUS * 2;

        vileObj = actor;

        // Call PIT_VileCheck to check whether object is a corpse
        // that can be raised.
        VALIDCOUNT++;
        if(!P_MobjsBoxIterator(box, PIT_VileCheck, 0))
        {
            // Got one!
            temp = actor->target;
            actor->target = corpseHit;
            A_FaceTarget(actor);
            actor->target = temp;

            P_MobjChangeState(actor, S_VILE_HEAL1);
            S_StartSound(SFX_SLOP, corpseHit);
            info = corpseHit->info;

            P_MobjChangeState(corpseHit, P_GetState(corpseHit->type, SN_RAISE));

            if(cfg.raiseGhosts)
            {
                corpseHit->height *= 2*2;
            }
            else
            {
                corpseHit->height = info->height;
                corpseHit->radius = info->radius;
            }

            corpseHit->flags = info->flags;
            corpseHit->health = info->spawnHealth;
            corpseHit->target = NULL;
            corpseHit->corpseTics = 0;
            return;
        }
    }

    // Return to normal attack.
    A_Chase(actor);
}

void C_DECL A_VileStart(mobj_t *actor)
{
    S_StartSound(SFX_VILATK, actor);
}

void C_DECL A_StartFire(mobj_t *actor)
{
    S_StartSound(SFX_FLAMST, actor);
    A_Fire(actor);
}

void C_DECL A_FireCrackle(mobj_t *actor)
{
    S_StartSound(SFX_FLAME, actor);
    A_Fire(actor);
}

/**
 * Keep fire in front of player unless out of sight.
 */
void C_DECL A_Fire(mobj_t *actor)
{
    mobj_t             *dest;
    uint                an;

    dest = actor->tracer;
    if(!dest)
        return;

    // Don't move it if the vile lost sight.
    if(!P_CheckSight(actor->target, dest))
        return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_MobjUnsetPosition(actor);
    memcpy(actor->pos, dest->pos, sizeof(actor->pos));
    actor->pos[VX] += 24 * FIX2FLT(finecosine[an]);
    actor->pos[VY] += 24 * FIX2FLT(finesine[an]);
    P_MobjSetPosition(actor);
}

/**
 * Spawn the archviles' hellfire
 */
void C_DECL A_VileTarget(mobj_t *actor)
{
    mobj_t             *fog;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    fog = P_SpawnMobj3fv(MT_FIRE, actor->target->pos,
                         actor->target->angle + ANG180);

    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire(fog);
}

void C_DECL A_VileAttack(mobj_t* actor)
{
    mobj_t*             fire;
    uint                an;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if(!P_CheckSight(actor, actor->target))
        return;

    S_StartSound(SFX_BAREXP, actor);
    P_DamageMobj(actor->target, actor, actor, 20, false);
    actor->target->mom[MZ] =
        FIX2FLT(1000 * FRACUNIT / actor->target->info->mass);

    an = actor->angle >> ANGLETOFINESHIFT;
    fire = actor->tracer;

    if(!fire)
        return;

    // Move the fire between the Vile and the player.
    fire->pos[VX] = actor->target->pos[VX] - 24 * FIX2FLT(finecosine[an]);
    fire->pos[VY] = actor->target->pos[VY] - 24 * FIX2FLT(finesine[an]);
    P_RadiusAttack(fire, actor, 70, 69);
}

void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(SFX_MANATK, actor);
}

/**
 * Mancubus attack:
 */
void C_DECL A_FatAttack1(mobj_t *actor)
{
    mobj_t             *mo;
    uint                an;

    A_FaceTarget(actor);
    // Change direction  to...
    actor->angle += FATSPREAD;
    P_SpawnMissile(MT_FATSHOT, actor, actor->target);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
    }
}

void C_DECL A_FatAttack2(mobj_t *actor)
{
    mobj_t             *mo;
    uint                an;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile(MT_FATSHOT, actor, actor->target);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD * 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
    }
}

void C_DECL A_FatAttack3(mobj_t *actor)
{
    mobj_t             *mo;
    uint                an;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle -= FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
    }

    mo = P_SpawnMissile(MT_FATSHOT, actor, actor->target);
    if(mo)
    {
        mo->angle += FATSPREAD / 2;
        an = mo->angle >> ANGLETOFINESHIFT;
        mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
        mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);
    }
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
void C_DECL A_PainShootSkull(mobj_t* actor, angle_t angle)
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
        P_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

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

        newmobj = P_SpawnMobj3fv(MT_SKULL, pos, angle);
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
        newmobj = P_SpawnMobj3fv(MT_SKULL, pos, angle);
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
    A_PainShootSkull(actor, actor->angle);
}

void C_DECL A_PainDie(mobj_t *actor)
{
    A_Fall(actor);
    A_PainShootSkull(actor, actor->angle + ANG90);
    A_PainShootSkull(actor, actor->angle + ANG180);
    A_PainShootSkull(actor, actor->angle + ANG270);
}

void C_DECL A_Scream(mobj_t *actor)
{
    int                 sound;

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
    if(actor->type == MT_SPIDER || actor->type == MT_CYBORG)
    {   // Full volume.
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
    }
    else
    {   // Normal volume.
        S_StartSound(sound, actor);
    }
}

void C_DECL A_XScream(mobj_t* actor)
{
    S_StartSound(SFX_SLOP, actor);
}

void C_DECL A_Pain(mobj_t* actor)
{
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Fall(mobj_t* actor)
{
    // Actor is on ground, it can be walked over.
    actor->flags &= ~MF_SOLID;
}

void C_DECL A_Explode(mobj_t* mo)
{
    P_RadiusAttack(mo, mo->target, 128, 127);
}

/**
 * Possibly trigger special effects if on first boss map.
 */
void C_DECL A_BossDeath(mobj_t* mo)
{
    int                 i;
    linedef_t*          dummyLine;
    countmobjoftypeparams_t params;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if(gameMode == commercial)
    {
        if(gameMap != 7)
            return;
        if((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
    }
    else
    {
        switch(gameEpisode)
        {
        case 1:
            if(gameMap != 8)
                return;

            /**
             * Ultimate DOOM behavioral change:
             * This test was added so that the (variable) effects of the
             * 666 special would only take effect when the last Baron
             * died and not ANY monster.
             * Many classic PWADS such as "Doomsday of UAC" (UAC_DEAD.wad)
             * relied on the old behaviour and cannot be completed.
             */

            // Added compatibility option.
            if(!cfg.anyBossDeath)
                if(mo->type != MT_BRUISER)
                    return;
            break;

        case 2:
            if(gameMap != 8)
                return;

            if(mo->type != MT_CYBORG)
                return;
            break;

        case 3:
            if(gameMap != 8)
                return;

            if(mo->type != MT_SPIDER)
                return;

            break;

        case 4:
            switch(gameMap)
            {
            case 6:
                if(mo->type != MT_CYBORG)
                    return;
                break;

            case 8:
                if(mo->type != MT_SPIDER)
                    return;
                break;

            default:
                return;
                break;
            }
            break;

        default:
            if(gameMap != 8)
                return;
            break;
        }

    }

    // Make sure there is a player alive for victory...
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame && players[i].health > 0)
            break;
    }

    if(i == MAXPLAYERS)
        return; // No one left alive, so do not end game.

    // Scan the remaining thinkers to see if all bosses are dead.
    params.type = mo->type;
    params.count = 0;
    P_IterateThinkers(P_MobjThinker, countMobjOfType, &params);

    if(params.count)
    {   // Other boss not dead.
        return;
    }

    // Victory!
    if(gameMode == commercial)
    {
        if(gameMap == 7)
        {
            if(mo->type == MT_FATSO)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
                P_FreeDummyLine(dummyLine);
                return;
            }

            if(mo->type == MT_BABY)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 667;
                EV_DoFloor(dummyLine, FT_RAISETOTEXTURE);
                P_FreeDummyLine(dummyLine);

                // Only activate once (rare, "DOOM2::MAP07-Dead Simple" bug).
                bossKilled = true;
                return;
            }
        }
    }
    else
    {
        switch(gameEpisode)
        {
        case 1:
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
            P_FreeDummyLine(dummyLine);
            bossKilled = true;
            return;
            break;

        case 4:
            switch(gameMap)
            {
            case 6:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoDoor(dummyLine, DT_BLAZEOPEN);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;

            case 8:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;
            }
        }
    }

    G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
}

void C_DECL A_Hoof(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_HOOF |
                 (gameMode != commercial &&
                  gameMap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    /**
     * \kludge Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_METAL |
                 (gameMode != commercial &&
                  gameMap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(SFX_BSPWLK, mo);
    A_Chase(mo);
}

void C_DECL A_BrainAwake(mobj_t *mo)
{
    S_StartSound(SFX_BOSSIT, NULL);
}

void C_DECL A_BrainPain(mobj_t *mo)
{
    S_StartSound(SFX_BOSPN, NULL);
}

void C_DECL A_BrainScream(mobj_t *mo)
{
    float               pos[3];
    mobj_t             *th;

    for(pos[VX] = mo->pos[VX] - 196; pos[VX] < mo->pos[VX] + 320;
        pos[VX] += 8)
    {
        pos[VY] = mo->pos[VY] - 320;
        pos[VZ] = 128 + (P_Random() * 2);

        th = P_SpawnMobj3fv(MT_ROCKET, pos, P_Random() << 24);
        th->mom[MZ] = FIX2FLT(P_Random() * 512);

        P_MobjChangeState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(SFX_BOSDTH, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    float               pos[3];
    mobj_t             *th;

    pos[VX] = mo->pos[VX] + ((P_Random() - P_Random()) * 2048);
    pos[VY] = mo->pos[VY];
    pos[VZ] = 128 + (P_Random() * 2);

    th = P_SpawnMobj3fv(MT_ROCKET, pos, P_Random() << 24);
    th->mom[MZ] = FIX2FLT(P_Random() * 512);

    P_MobjChangeState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if(th->tics < 1)
        th->tics = 1;
}

void C_DECL A_BrainDie(mobj_t *mo)
{
    G_LeaveMap(G_GetMapNumber(gameEpisode, gameMap), 0, false);
}

void C_DECL A_BrainSpit(mobj_t *mo)
{
    mobj_t             *targ;
    mobj_t             *newmobj;

    if(!numBrainTargets)
        return; // Ignore if no targets.

    brain.easy ^= 1;
    if(gameSkill <= SM_EASY && (!brain.easy))
        return;

    // Shoot a cube at current target.
    targ = brainTargets[brain.targetOn++];
    brain.targetOn %= numBrainTargets;

    // Spawn brain missile.
    newmobj = P_SpawnMissile(MT_SPAWNSHOT, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }

    S_StartSound(SFX_BOSPIT, NULL);
}

/**
 * Travelling cube sound.
 */
void C_DECL A_SpawnSound(mobj_t *mo)
{
    S_StartSound(SFX_BOSCUB, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly(mobj_t *mo)
{
    int                 r;
    mobj_t             *newmobj;
    mobj_t             *fog;
    mobj_t             *targ;
    mobjtype_t          type;

    if(--mo->reactionTime)
        return; // Still flying.

    targ = mo->target;

    // First spawn teleport fog.
    fog = P_SpawnMobj3fv(MT_SPAWNFIRE, targ->pos, targ->angle + ANG180);
    S_StartSound(SFX_TELEPT, fog);

    // Randomly select monster to spawn.
    r = P_Random();

    // Probability distribution (kind of :), decreasing likelihood.
    if(r < 50)
        type = MT_TROOP;
    else if(r < 90)
        type = MT_SERGEANT;
    else if(r < 120)
        type = MT_SHADOWS;
    else if(r < 130)
        type = MT_PAIN;
    else if(r < 160)
        type = MT_HEAD;
    else if(r < 162)
        type = MT_VILE;
    else if(r < 172)
        type = MT_UNDEAD;
    else if(r < 192)
        type = MT_BABY;
    else if(r < 222)
        type = MT_FATSO;
    else if(r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = P_SpawnMobj3fv(type, targ->pos, P_Random() << 24);

    if(lookForPlayers(newmobj, true))
        P_MobjChangeState(newmobj, P_GetState(newmobj->type, SN_SEE));

    // Telefrag anything in this spot.
    P_TeleportMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false);

    // Remove self (i.e., cube).
    P_MobjRemove(mo, true);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    int                 sound = SFX_PLDETH; // Default death sound.

    if((gameMode == commercial) && (mo->health < -50))
    {
        // If the player dies less with less than -50% without gibbing.
        sound = SFX_PDIEHI;
    }

    S_StartSound(sound, mo);
}
