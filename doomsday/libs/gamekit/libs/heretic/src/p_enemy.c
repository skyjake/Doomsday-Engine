/** @file p_enemy.c Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 by id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <math.h>

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jheretic.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_floor.h"

#define MONS_LOOK_RANGE     (20*64)
#define MONS_LOOK_LIMIT     64

#define MNTR_CHARGE_SPEED   (13)

#define MAX_GEN_PODS        16

#define BODYQUESIZE         32

// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309f)

static const coord_t dirSpeed[8][2] =
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

mobj_t *bodyque[BODYQUESIZE];
int bodyqueslot;

void P_ClearBodyQueue(void)
{
    memset(bodyque, 0, sizeof(bodyque));
    bodyqueslot = 0;
}

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, Mobj_Sector(emitter), 0);
}

dd_bool P_CheckMeleeRange(mobj_t* actor)
{
    mobj_t* pl;
    coord_t dist, range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = M_ApproxDistance(pl->origin[VX] - actor->origin[VX],
                            pl->origin[VY] - actor->origin[VY]);

    if(!cfg.common.netNoMaxZMonsterMeleeAttack)
    {
        // Account for Z height difference.
        if(pl->origin[VZ] > actor->origin[VZ] + actor->height ||
           pl->origin[VZ] + pl->height < actor->origin[VZ])
            return false;
    }

    range = MELEERANGE - 20 + pl->info->radius;
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

dd_bool P_CheckMissileRange(mobj_t* actor)
{
    coord_t dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {   // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactionTime)
        return false; // Don't attack yet

    dist = M_ApproxDistance(actor->origin[VX] - actor->target->origin[VX],
                            actor->origin[VY] - actor->target->origin[VY]) - 64;

    if(P_GetState(actor->type, SN_MELEE) == S_NULL)
        dist -= 128; // No melee attack, so fire more frequently.

    // Imp's fly attack from far away
    if(actor->type == MT_IMP)
        dist /= 2;

    if(dist > 200)
        dist = 200;

    if(P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction.
 *
 * @return                  @c false, if the move is blocked.
 */
dd_bool P_Move(mobj_t *actor, dd_bool dropoff)
{
    coord_t pos[2], step[2];
    Line *ld;
    dd_bool good;

    if(actor->moveDir == DI_NODIR)
        return false;

    assert(VALID_MOVEDIR(actor->moveDir));

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][VX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][VY];
    pos[VX] = actor->origin[VX] + step[VX];
    pos[VY] = actor->origin[VY] + step[VY];

    // killough $dropoff_fix.
    if(!P_TryMoveXY(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Float up and down to the contacted floor height.
        if((actor->flags & MF_FLOAT) && tmFloatOk)
        {
            coord_t oldZ = actor->origin[VZ];

            if(actor->origin[VZ] < tmFloorZ)
                actor->origin[VZ] += FLOATSPEED;
            else
                actor->origin[VZ] -= FLOATSPEED;

            // What if we just floated into another mobj?
            if(P_CheckPosition(actor, actor->origin))
            {
                // Looks ok: floated to an unoccupied spot.
                actor->flags |= MF_INFLOAT;
            }
            else
            {
                // Let's not do this; undo the float.
                actor->origin[VZ] = oldZ;
            }
            return true;
        }

        // Open any specials.
        if(IterList_Empty(spechit)) return false;

        actor->moveDir = DI_NODIR;
        good = false;
        while((ld = IterList_Pop(spechit)) != NULL)
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
                good |= ld == tmBlockingLine ? 1 : 2;
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

    // $dropoff_fix: fall more slowly, under gravity, if tmFellDown==true
    if(!(actor->flags & MF_FLOAT) && !tmFellDown)
    {
        if(actor->origin[VZ] > actor->floorZ)
            P_HitFloor(actor);

        actor->origin[VZ] = actor->floorZ;
    }

    return true;
}

/**
 * Attempts to move actor on in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE
 * If move is either clear or blocked only by a door, returns TRUE and sets...
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
static dd_bool tryMoveMobj(mobj_t *actor)
{
    // $dropoff_fix
    if(!P_Move(actor, false))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void doNewChaseDir(mobj_t *actor, coord_t deltaX, coord_t deltaY)
{
    dirtype_t xdir, ydir;
    dirtype_t olddir = actor->moveDir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR) // Find reverse direction.
        turnaround ^= 4;

    xdir = (deltaX > 10  ? DI_EAST  : deltaX < -10 ? DI_WEST  : DI_NODIR);
    ydir = (deltaY < -10 ? DI_SOUTH : deltaY > 10  ? DI_NORTH : DI_NODIR);

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
        int tdir;
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        {
            if((dirtype_t)tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
        }
    }
    else
    {
        int tdir;
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
        {
            if((dirtype_t)tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
        }
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !tryMoveMobj(actor))
        actor->moveDir = DI_NODIR;
}

typedef struct {
    mobj_t *averterMobj;  ///< Mobj attempting to avert the drop off.
    AABoxd averterAABox;  ///< Current axis-aligned bounding box of the averter.
    vec2d_t direction;    ///< Direction in which to move to avoid the drop off.
} pit_avoiddropoff_params_t;

static int PIT_AvoidDropoff(Line *line, void *context)
{
    pit_avoiddropoff_params_t *parm = (pit_avoiddropoff_params_t *)context;
    Sector *backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    AABoxd *aaBox      = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       // Line must be contacted
       parm->averterAABox.minX < aaBox->maxX &&
       parm->averterAABox.maxX > aaBox->minX &&
       parm->averterAABox.minY < aaBox->maxY &&
       parm->averterAABox.maxY > aaBox->minY &&
       !Line_BoxOnSide(line, &parm->averterAABox))
    {
        Sector *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
        coord_t front = P_GetDoublep(frontsector, DMU_FLOOR_HEIGHT);
        coord_t back  = P_GetDoublep(backsector, DMU_FLOOR_HEIGHT);
        vec2d_t lineDir;
        angle_t angle;
        uint an;

        P_GetDoublepv(line, DMU_DXY, lineDir);

        // The monster must contact one of the two floors, and the other must be
        // a tall drop off (more than 24).
        if(FEQUAL(back, parm->averterMobj->floorZ) &&
           front < parm->averterMobj->floorZ - 24)
        {
            angle = M_PointToAngle(lineDir); // Front drop off.
        }
        else
        {
            if(FEQUAL(front, parm->averterMobj->floorZ) &&
               back < parm->averterMobj->floorZ - 24)
            {
                angle = M_PointXYToAngle(-lineDir[0], -lineDir[1]); // Back drop off.
            }
            return false;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted lines are cumulative (e.g., hanging over a corner).
        an = angle >> ANGLETOFINESHIFT;
        parm->direction[VX] -= FIX2FLT(finesine  [an]) * 32;
        parm->direction[VY] += FIX2FLT(finecosine[an]) * 32;
    }

    return false;
}

/**
 * Monsters try to move away from tall drop offs. (From PrBoom.)
 *
 * In Doom, they were never allowed to hang over drop offs, and would remain
 * stuck if involuntarily forced over one. This logic, combined with
 * p_map.c::P_TryMoveXY(), allows monsters to free themselves without making
 * them tend to hang over drop offs.
 *
 * @param chaseDir  Direction in which the mobj is currently "chasing". If a
 *                  drop off is found, this direction will be updated with a
 *                  direction that will take the mobj back onto terra firma.
 *
 * @return  @c true iff the direction was changed to avoid a drop off.
 */
static dd_bool shouldAvoidDropoff(mobj_t *mobj, pvec2d_t chaseDir)
{
    pit_avoiddropoff_params_t parm;

    DE_ASSERT(mobj != 0);

    // Disabled? (inverted var name!)
    if(cfg.avoidDropoffs) return false;

    if(mobj->floorZ - mobj->dropOffZ <= 24) return false;
    if(mobj->origin[VZ] > mobj->floorZ) return false;
    if(mobj->flags & (MF_DROPOFF | MF_FLOAT)) return false;

    parm.averterMobj       = mobj;
    parm.averterAABox.minX = mobj->origin[VX] - mobj->radius;
    parm.averterAABox.minY = mobj->origin[VY] - mobj->radius;
    parm.averterAABox.maxX = mobj->origin[VX] + mobj->radius;
    parm.averterAABox.maxY = mobj->origin[VY] + mobj->radius;
    V2d_Set(parm.direction, 0, 0);

    VALIDCOUNT++;
    Mobj_TouchedLinesIterator(mobj, PIT_AvoidDropoff, &parm);

    if(IS_ZERO(parm.direction[VX]) && IS_ZERO(parm.direction[VY]))
        return false;

    // The mobj should attempt to move away from the drop off.
    V2d_Copy(chaseDir, parm.direction);
    return true;
}

static void newChaseDir(mobj_t *mobj)
{
    vec2d_t chaseDir;
    dd_bool avoiding;

    DE_ASSERT(mobj != 0);

    // Nothing to chase?
    if(!mobj->target) return;

    // Chase toward the target, unless there is a drop off to avoid.
    V2d_Subtract(chaseDir, mobj->target->origin, mobj->origin);
    avoiding = shouldAvoidDropoff(mobj, chaseDir);

    // Apply the direction change (if any).
    doNewChaseDir(mobj, chaseDir[VX], chaseDir[VY]);

    if(avoiding)
    {
        // Take small steps away from the drop off.
        mobj->moveCount = 1;
    }
}

typedef struct {
    size_t              count;
    size_t              maxTries;
    mobj_t*             notThis;
    mobj_t*             foundMobj;
    coord_t             origin[2];
    coord_t             maxDistance;
    int                 minHealth;
    int                 compFlags;
    dd_bool             checkLOS;
    byte                randomSkip;
} findmobjparams_t;

static int findMobj(thinker_t* th, void* context)
{
    findmobjparams_t*   params = (findmobjparams_t*) context;
    mobj_t*             mo = (mobj_t *) th;

    // Flags requirement?
    if(params->compFlags > 0 && !(mo->flags & params->compFlags))
        return false; // Continue iteration.

    // Minimum health requirement?
    if(params->minHealth > 0 && mo->health < params->minHealth)
        return false; // Continue iteration.

    // Exclude this mobj?
    if(params->notThis && mo == params->notThis)
        return false; // Continue iteration.

    // Out of range?
    if(params->maxDistance > 0 &&
       M_ApproxDistance(params->origin[VX] - mo->origin[VX],
                        params->origin[VY] - mo->origin[VY]) >
       params->maxDistance)
        return false; // Continue iteration.

    // Randomly skip this?
    if(params->randomSkip && P_Random() < params->randomSkip)
        return false; // Continue iteration.

    if(params->maxTries > 0 && params->count++ > params->maxTries)
        return true; // Stop iteration.

    // Out of sight?
    if(params->checkLOS && params->notThis &&
       !P_CheckSight(params->notThis, mo))
        return false; // Continue iteration.

    // Found one!
    params->foundMobj = mo;
    return true; // Stop iteration.
}

dd_bool P_LookForMonsters(mobj_t* mo)
{
    findmobjparams_t    params;

    if(!P_CheckSight(players[0].plr->mo, mo))
        return false; // Player can't see the monster.

    params.count = 0;
    params.notThis = mo;
    params.origin[VX] = mo->origin[VX];
    params.origin[VY] = mo->origin[VY];
    params.foundMobj = NULL;
    params.maxDistance = MONS_LOOK_RANGE;
    params.maxTries = MONS_LOOK_LIMIT;
    params.minHealth = 1;
    params.compFlags = MF_COUNTKILL;
    params.checkLOS = true;
    params.randomSkip = 16;
    Thinker_Iterate(P_MobjThinker, findMobj, &params);

    if(params.foundMobj)
    {
        mo->target = params.foundMobj;
        return true;
    }

    return false;
}

/**
 * If allaround is false, only look 180 degrees in front
 * returns true if a player is targeted
 */
dd_bool P_LookForPlayers(mobj_t* actor, dd_bool allAround)
{
    // If in single player and player is dead, look for monsters.
    if(!IS_NETGAME && players[0].health <= 0)
        return P_LookForMonsters(actor);

    return Mobj_LookForPlayers(actor, allAround);
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    mobj_t *targ;
    Sector *sec;

    // Any shot will wake up
    actor->threshold = 0;
    sec = Mobj_Sector(actor);
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

    if(!P_LookForPlayers(actor, false))
        return;

    // go into chase state
  seeyou:
    if(actor->info->seeSound)
    {
        int sound = actor->info->seeSound;

        if(actor->flags2 & MF2_BOSS)
        {
            // Full volume
            S_StartSound(sound, NULL);
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
void C_DECL A_Chase(mobj_t *actor)
{
    int delta;
    statenum_t state;

    if(actor->reactionTime)
    {
        actor->reactionTime--;
    }

    // Modify target threshold.
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gfw_Rule(skill) == SM_NIGHTMARE ||
       gfw_Rule(fast))
    {
        // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
            actor->tics = 3;
    }

    // Turn towards movement direction if not there yet.
    if(actor->moveDir < DI_NODIR)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->moveDir << 29);

        if(delta > 0)
        {
            actor->angle -= ANG90 / 2;
        }
        else if(delta < 0)
        {
            actor->angle += ANG90 / 2;
        }
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE) ||
       P_MobjIsCamera(actor->target))
    {
        // Look for a new target.
        if(!P_LookForPlayers(actor, true))
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        }
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE)
        {
            newChaseDir(actor);
        }
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, state);
        return;
    }

    // Check for missile attack.
    if((state = P_GetState(actor->type, SN_MISSILE)) != S_NULL)
    {
        if(!(gfw_Rule(skill) != SM_NIGHTMARE && actor->moveCount))
        {
            if(P_CheckMissileRange(actor))
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
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->moveCount < 0 || !P_Move(actor, false))
    {
        newChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        if(actor->type == MT_WIZARD && P_Random() < 128)
        {
            S_StartSound(actor->info->seeSound, actor);
        }
        else if(actor->type == MT_SORCERER2)
        {
            S_StartSound(actor->info->activeSound, NULL);
        }
        else
        {
            S_StartSound(actor->info->activeSound, actor);
        }
    }
}

void C_DECL A_FaceTarget(mobj_t* actor)
{
    if(!actor->target) return;

    actor->turnTime = true; // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;

    actor->angle = M_PointToAngle2(actor->origin, actor->target->origin);

    // Is target a ghost?
    if(actor->target->flags & MF_SHADOW)
    {
        actor->angle += (P_Random() - P_Random()) << 21;
    }
}

void C_DECL A_Pain(mobj_t* actor)
{
    if(actor->info->painSound)
        S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_DripBlood(mobj_t* actor)
{
    mobj_t* mo;

    if((mo = P_SpawnMobjXYZ(MT_BLOOD, actor->origin[VX] + FIX2FLT((P_Random() - P_Random()) << 11),
                                      actor->origin[VY] + FIX2FLT((P_Random() - P_Random()) << 11),
                                      actor->origin[VZ], P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);

        mo->flags2 |= MF2_LOGRAV;
    }
}

void C_DECL A_KnightAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3), false);
        S_StartSound(SFX_KGTAT2, actor);
        return;
    }

    // Throw axe.
    S_StartSound(actor->info->attackSound, actor);
    if(actor->type == MT_KNIGHTGHOST || P_Random() < 40)
    {
        // Red axe.
        P_SpawnMissile(MT_REDAXE, actor, actor->target, true);
        return;
    }

    // Green axe.
    P_SpawnMissile(MT_KNIGHTAXE, actor, actor->target, true);
}

void C_DECL A_ImpExplode(mobj_t* actor)
{
    mobj_t* mo;

    if((mo = P_SpawnMobj(MT_IMPCHUNK1, actor->origin, P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 9;
    }

    if((mo = P_SpawnMobj(MT_IMPCHUNK2, actor->origin, P_Random() << 24, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 10);
        mo->mom[MZ] = 9;
    }

    if(actor->special1 == 666)
        P_MobjChangeState(actor, S_IMP_XCRASH1); // Extreme death crash.
}

void C_DECL A_BeastPuff(mobj_t* actor)
{
    if(P_Random() > 64)
    {
        P_SpawnMobjXYZ(MT_PUFFY,
                      actor->origin[VX] + FIX2FLT((P_Random() - P_Random()) << 10),
                      actor->origin[VY] + FIX2FLT((P_Random() - P_Random()) << 10),
                      actor->origin[VZ] + FIX2FLT((P_Random() - P_Random()) << 10),
                      P_Random() << 24, 0);
    }
}

void C_DECL A_ImpMeAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7), false);
    }
}

void C_DECL A_ImpMsAttack(mobj_t* actor)
{
    mobj_t* dest;
    uint an;
    int dist;

    if(!actor->target || P_Random() > 64)
    {
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
        return;
    }

    dest = actor->target;

    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);

    A_FaceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = 12 * FIX2FLT(finecosine[an]);
    actor->mom[MY] = 12 * FIX2FLT(finesine[an]);

    dist = M_ApproxDistance(dest->origin[VX] - actor->origin[VX],
                            dest->origin[VY] - actor->origin[VY]);
    dist /= 12;
    if(dist < 1)
        dist = 1;

    actor->mom[MZ] = (dest->origin[VZ] + (dest->height /2) - actor->origin[VZ]) / dist;
}

/**
 * Fireball attack of the imp leader.
 */
void C_DECL A_ImpMsAttack2(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7), false);
        return;
    }

    P_SpawnMissile(MT_IMPBALL, actor, actor->target, true);
}

void C_DECL A_ImpDeath(mobj_t* actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags2 |= MF2_FLOORCLIP;

    if(actor->origin[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_IMP_CRASH1);
}

void C_DECL A_ImpXDeath1(mobj_t* actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags |= MF_NOGRAVITY;
    actor->flags2 |= MF2_FLOORCLIP;

    actor->special1 = 666; // Flag the crash routine.
}

void C_DECL A_ImpXDeath2(mobj_t* actor)
{
    actor->flags &= ~MF_NOGRAVITY;

    if(actor->origin[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_IMP_CRASH1);
}

/**
 * @return          @c true, if the chicken morphs.
 */
dd_bool P_UpdateChicken(mobj_t* actor, int tics)
{
    mobj_t* fog;
    coord_t pos[3];
    mobjtype_t moType;
    mobj_t* mo;
    mobj_t oldChicken;

    actor->special1 -= tics;

    if(actor->special1 > 0)
        return false;

    moType = actor->special2;

    memcpy(pos, actor->origin, sizeof(pos));

    //// @todo Do this properly!
    memcpy(&oldChicken, actor, sizeof(oldChicken));

    if(!(mo = P_SpawnMobj(moType, pos, oldChicken.angle, 0)))
        return false;

    P_MobjChangeState(actor, S_FREETARGMOBJ);

    if(P_TestMobjLocation(mo) == false)
    {
        // Didn't fit.
        P_MobjRemove(mo, true);

        if((mo = P_SpawnMobj(MT_CHICKEN, pos, oldChicken.angle, 0)))
        {
            mo->flags = oldChicken.flags;
            mo->health = oldChicken.health;
            mo->target = oldChicken.target;

            mo->special1 = 5 * TICSPERSEC;  // Next try in 5 seconds.
            mo->special2 = moType;
        }

        return false;
    }

    mo->target = oldChicken.target;

    if((fog = P_SpawnMobjXYZ(MT_TFOG, pos[VX], pos[VY],
                            pos[VZ] + TELEFOGHEIGHT, mo->angle + ANG180, 0)))
        S_StartSound(SFX_TELEPT, fog);

    return true;
}

void C_DECL A_ChicAttack(mobj_t* actor)
{
    if(P_UpdateChicken(actor, 18))
        return;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
        P_DamageMobj(actor->target, actor, actor, 1 + (P_Random() & 1), false);
}

void C_DECL A_ChicLook(mobj_t* actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    A_Look(actor);
}

void C_DECL A_ChicChase(mobj_t* actor)
{
    if(P_UpdateChicken(actor, 3))
        return;

    A_Chase(actor);
}

void C_DECL A_ChicPain(mobj_t* actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    S_StartSound(actor->info->painSound, actor);
}

void C_DECL A_Feathers(mobj_t* actor)
{
    int         i, count;
    mobj_t     *mo;

    // In Pain?
    if(actor->health > 0)
        count = P_Random() < 32 ? 2 : 1;
    else // Death.
        count = 5 + (P_Random() & 3);

    for(i = 0; i < count; ++i)
    {
        if((mo = P_SpawnMobjXYZ(MT_FEATHER,
                               actor->origin[VX], actor->origin[VY],
                               actor->origin[VZ] + 20, P_Random() << 24, 0)))
        {
            mo->target = actor;

            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 9);

            P_MobjChangeState(mo, S_FEATHER1 + (P_Random() & 7));
        }
    }
}

void C_DECL A_MummyAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2), false);
        S_StartSound(SFX_MUMAT2, actor);
        return;
    }

    S_StartSound(SFX_MUMAT1, actor);
}

/**
 * Mummy leader missile attack.
 */
void C_DECL A_MummyAttack2(mobj_t* actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2), false);
        return;
    }

    mo = P_SpawnMissile(MT_MUMMYFX1, actor, actor->target, true);

    if(mo != NULL)
        mo->tracer = actor->target;
}

void C_DECL A_MummyFX1Seek(mobj_t* actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 20);
}

void C_DECL A_MummySoul(mobj_t* mummy)
{
    mobj_t* mo;

    if((mo = P_SpawnMobjXYZ(MT_MUMMYSOUL, mummy->origin[VX],
                                          mummy->origin[VY],
                                          mummy->origin[VZ] + 10,
                                          mummy->angle, 0)))
    {
        mo->mom[MZ] = 1;
    }
}

void C_DECL A_Sor1Pain(mobj_t* actor)
{
    actor->special1 = 20; // Number of steps to walk fast.
    A_Pain(actor);
}

void C_DECL A_Sor1Chase(mobj_t* actor)
{
    if(actor->special1)
    {
        actor->special1--;
        actor->tics -= 3;
    }

    A_Chase(actor);
}

/**
 * Sorcerer demon attack.
 */
void C_DECL A_Srcr1Attack(mobj_t* actor)
{
    mobj_t     *mo;
    angle_t     angle;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(8), false);
        return;
    }

    if(actor->health > (actor->info->spawnHealth / 3) * 2)
    {
        // Spit one fireball.
        P_SpawnMissile(MT_SRCRFX1, actor, actor->target, true);
    }
    else
    {
        // Spit three fireballs.
        mo = P_SpawnMissile(MT_SRCRFX1, actor, actor->target, true);
        if(mo)
        {
            angle = mo->angle;
            P_SpawnMissileAngle(MT_SRCRFX1, actor, angle - ANGLE_1 * 3, mo->mom[MZ]);
            P_SpawnMissileAngle(MT_SRCRFX1, actor, angle + ANGLE_1 * 3, mo->mom[MZ]);
        }

        if(actor->health < actor->info->spawnHealth / 3)
        {
            // Maybe attack again?
            if(actor->special1)
            {
                // Just attacked, so don't attack again/
                actor->special1 = 0;
            }
            else
            {
                // Set state to attack again/
                actor->special1 = 1;
                P_MobjChangeState(actor, S_SRCR1_ATK4);
            }
        }
    }
}

void C_DECL A_SorcererRise(mobj_t* actor)
{
    mobj_t*             mo;

    actor->flags &= ~MF_SOLID;
    if((mo = P_SpawnMobj(MT_SORCERER2, actor->origin, actor->angle, 0)))
    {
        P_MobjChangeState(mo, S_SOR2_RISE1);
        mo->target = actor->target;
    }
}

void P_DSparilTeleport(mobj_t* actor)
{
    // No spots?
    if(bossSpotCount > 0)
    {
        int i, tries;
        const mapspot_t* dest;

        i = P_Random();
        tries = bossSpotCount;

        do
        {
            dest = &mapSpots[bossSpots[++i % bossSpotCount]];
            if(M_ApproxDistance(actor->origin[VX] - dest->origin[VX],
                                actor->origin[VY] - dest->origin[VY]) >= 128)
            {
                // A suitable teleport destination is available.
                coord_t prevpos[3];
                angle_t oldAngle;

                memcpy(prevpos, actor->origin, sizeof(prevpos));
                oldAngle = actor->angle;

                if(P_TeleportMove(actor, dest->origin[VX], dest->origin[VY], false))
                {
                    mobj_t* mo;

                    if((mo = P_SpawnMobj(MT_SOR2TELEFADE, prevpos, oldAngle + ANG180, 0)))
                        S_StartSound(SFX_TELEPT, mo);

                    P_MobjChangeState(actor, S_SOR2_TELE1);
                    actor->origin[VZ] = actor->floorZ;
                    actor->angle = dest->angle;
                    actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;
                    S_StartSound(SFX_TELEPT, actor);
                }

                return;
            }
        } while(tries-- > 0); // Don't stay here forever.
    }
}

void C_DECL A_Srcr2Decide(mobj_t* actor)
{
    static int chance[] = {
        192, 120, 120, 120, 64, 64, 32, 16, 0
    };

    // No spots?
    if(!bossSpotCount) return;

    if(P_Random() < chance[actor->health / (actor->info->spawnHealth / 8)])
    {
        P_DSparilTeleport(actor);
    }
}

void C_DECL A_Srcr2Attack(mobj_t* actor)
{
    int chance;

    if(!actor->target) return;

    S_StartSound(actor->info->attackSound, NULL);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(20), false);
        return;
    }

    chance = actor->health < actor->info->spawnHealth / 2 ? 96 : 48;
    if(P_Random() < chance)
    {
        // Wizard spawners.
        P_SpawnMissileAngle(MT_SOR2FX2, actor, actor->angle - ANG45, 1.0f/2);
        P_SpawnMissileAngle(MT_SOR2FX2, actor, actor->angle + ANG45, 1.0f/2);
    }
    else
    {
        // Blue bolt.
        P_SpawnMissile(MT_SOR2FX1, actor, actor->target, true);
    }
}

void C_DECL A_BlueSpark(mobj_t* actor)
{
    int i;

    for(i = 0; i < 2; ++i)
    {
        mobj_t* mo;

        if((mo = P_SpawnMobj(MT_SOR2FXSPARK, actor->origin, P_Random() << 24, 0)))
        {
            mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            mo->mom[MZ] = 1 + FIX2FLT(P_Random() << 8);
        }
    }
}

void C_DECL A_GenWizard(mobj_t* actor)
{
    mobj_t* mo, *fog;

    if(!(mo = P_SpawnMobjXYZ(MT_WIZARD, actor->origin[VX], actor->origin[VY],
                                        actor->origin[VZ] - (MOBJINFO[MT_WIZARD].height / 2),
                                        actor->angle, 0)))
        return;

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit.
        P_MobjRemove(mo, true);
        return;
    }

    actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;

    P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));

    actor->flags &= ~MF_MISSILE;

    if((fog = P_SpawnMobj(MT_TFOG, actor->origin, actor->angle + ANG180, 0)))
        S_StartSound(SFX_TELEPT, fog);
}

void C_DECL A_Sor2DthInit(mobj_t* actor)
{
    // Set the animation loop counter.
    actor->special1 = 7;

    // Kill monsters early.
    P_Massacre();
}

void C_DECL A_Sor2DthLoop(mobj_t* actor)
{
    if(--actor->special1)
    {   // Need to loop.
        P_MobjChangeState(actor, S_SOR2_DIE4);
    }
}

/**
 * D'Sparil Sound Routines.
 */
void C_DECL A_SorZap(mobj_t* actor)
{
    S_StartSound(SFX_SORZAP, NULL);
}

void C_DECL A_SorRise(mobj_t* actor)
{
    S_StartSound(SFX_SORRISE, NULL);
}

void C_DECL A_SorDSph(mobj_t* actor)
{
    S_StartSound(SFX_SORDSPH, NULL);
}

void C_DECL A_SorDExp(mobj_t* actor)
{
    S_StartSound(SFX_SORDEXP, NULL);
}

void C_DECL A_SorDBon(mobj_t* actor)
{
    S_StartSound(SFX_SORDBON, NULL);
}

void C_DECL A_SorSightSnd(mobj_t* actor)
{
    S_StartSound(SFX_SORSIT, NULL);
}

/**
 * Minotaur Melee attack.
 */
void C_DECL A_MinotaurAtk1(mobj_t* actor)
{
    player_t*           player;

    if(!actor->target)
        return;

    S_StartSound(SFX_STFPOW, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);

        if((player = actor->target->player) != NULL)
        {
            // Squish the player.
            player->viewHeightDelta = -16;
        }
    }
}

/**
 * Minotaur : Choose a missile attack.
 */
void C_DECL A_MinotaurDecide(mobj_t* actor)
{
    uint an;
    mobj_t* target;
    coord_t dist;

    target = actor->target;
    if(!target) return;

    S_StartSound(SFX_MINSIT, actor);

    dist = M_ApproxDistance(actor->origin[VX] - target->origin[VX],
                            actor->origin[VY] - target->origin[VY]);

    if(target->origin[VZ] + target->height > actor->origin[VZ] &&
       target->origin[VZ] + target->height < actor->origin[VZ] + actor->height &&
       dist < 8 * 64 && dist > 1 * 64 && P_Random() < 150)
    {
        // Charge attack.
        // Don't call the state function right away.
        P_MobjChangeStateNoAction(actor, S_MNTR_ATK4_1);
        actor->flags |= MF_SKULLFLY;

        A_FaceTarget(actor);

        an = actor->angle >> ANGLETOFINESHIFT;
        actor->mom[MX] = MNTR_CHARGE_SPEED * FIX2FLT(finecosine[an]);
        actor->mom[MY] = MNTR_CHARGE_SPEED * FIX2FLT(finesine[an]);

        // Charge duration.
        actor->special1 = 35 / 2;
    }
    else if(target->origin[VZ] == target->floorZ && dist < 9 * 64 &&
            P_Random() < 220)
    {
        // Floor fire attack.
        P_MobjChangeState(actor, S_MNTR_ATK3_1);
        actor->special2 = 0;
    }
    else
    {
        // Swing attack.
        A_FaceTarget(actor);
        // NOTE: Don't need to call P_MobjChangeState because the current
        //       state falls through to the swing attack
    }
}

void C_DECL A_MinotaurCharge(mobj_t* actor)
{
    mobj_t*             puff;

    if(actor->special1)
    {
        if((puff = P_SpawnMobj(MT_PHOENIXPUFF, actor->origin,
                                  P_Random() << 24, 0)))
            puff->mom[MZ] = 2;
        actor->special1--;
    }
    else
    {
        actor->flags &= ~MF_SKULLFLY;
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

/**
 * Minotaur : Swing attack.
 */
void C_DECL A_MinotaurAtk2(mobj_t* actor)
{
    mobj_t* mo;

    if(!actor->target) return;

    S_StartSound(SFX_MINAT2, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5), false);
        return;
    }

    mo = P_SpawnMissile(MT_MNTRFX1, actor, actor->target, true);
    if(mo)
    {
        angle_t angle = mo->angle;
        coord_t momZ = mo->mom[MZ];

        S_StartSound(SFX_MINAT2, mo);

        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle - (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle + (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle - (ANG45 / 16), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, actor, angle + (ANG45 / 16), momZ);
    }
}

/**
 * Minotaur : Floor fire attack.
 */
void C_DECL A_MinotaurAtk3(mobj_t* actor)
{

    player_t*           player;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5), false);

        if((player = actor->target->player) != NULL)
        {
            // Squish the player.
            player->viewHeightDelta = -16;
        }
    }
    else
    {
        mobj_t* mo;
        dd_bool fixFloorFire = (!cfg.fixFloorFire && actor->floorClip > 0);

        /**
         * Original Heretic bug:
         * When an attempt is made to spawn MT_MNTRFX2 (the Maulotaur's
         * ground flame) the z coordinate is set to ONFLOORZ but if the
         * Maulotaur's feet are currently clipped (i.e., it is in a sector
         * whose terrain info is set to clip) then FOOTCLIPSIZE is
         * subtracted from the z coordinate. So when P_SpawnMobj is called,
         * z != ONFLOORZ, so rather than being set to the height of the
         * floor it is left at 2146838915 (float: 32758.162).
         *
         * This in turn means that when P_TryMoveXY is called (via
         * P_CheckMissileSpawn), the test which is there to check whether a
         * missile hits an upper side section will return true
         * (ceilingheight - thingz > thingheight).
         *
         * This results in P_ExplodeMissile being called instantly.
         *
         * jHeretic fixes this bug, however we maintain original behaviour
         * using the following method:
         *
         * 1) Do not call P_CheckMissileSpawn from P_SpawnMissile.
         * 2) Use special-case logic here which behaves similarly.
         */

        if((mo = P_SpawnMissile(MT_MNTRFX2, actor, actor->target,
                                (fixFloorFire? false : true))))
        {
            if(fixFloorFire)
            {
                P_MobjUnlink(mo);
                mo->origin[VX] += mo->mom[MX] / 2;
                mo->origin[VY] += mo->mom[MY] / 2;
                mo->origin[VZ] += mo->mom[MZ] / 2;
                P_MobjLink(mo);

                P_ExplodeMissile(mo);
            }
            else
            {
                S_StartSound(SFX_MINAT1, mo);
            }
        }
    }

    if(P_Random() < 192 && actor->special2 == 0)
    {
        P_MobjChangeState(actor, S_MNTR_ATK3_4);
        actor->special2 = 1;
    }
}

void C_DECL A_MntrFloorFire(mobj_t* actor)
{
    mobj_t* mo;
    coord_t pos[3];
    angle_t angle;

    // Make sure we are on the floor.
    actor->origin[VZ] = actor->floorZ;

    pos[VX] = actor->origin[VX];
    pos[VY] = actor->origin[VY];
    pos[VZ] = 0;

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);

    angle = M_PointToAngle2(actor->origin, pos);

    if((mo = P_SpawnMobj(MT_MNTRFX3, pos, angle, MSF_Z_FLOOR)))
    {
        mo->target = actor->target;
        mo->mom[MX] = FIX2FLT(1); // Force block checking.

        P_CheckMissileSpawn(mo);
    }
}

int P_Attack(mobj_t *actor, int meleeDamage, mobjtype_t missileType)
{
    if (actor->target)
    {
        if (P_CheckMeleeRange(actor))
        {
            P_DamageMobj(actor->target, actor, actor, meleeDamage, false);
            return 1;
        }
        else
        {
            mobj_t *mis;
            if ((mis = P_SpawnMissile(missileType, actor, actor->target, true)) != NULL)
            {
                if (missileType == MT_MUMMYFX1)
                {
                    // Tracer is used to keep track of where the missile is homing.
                    mis->tracer = actor->target;
                }
                else if (missileType == MT_WHIRLWIND)
                {
                    P_InitWhirlwind(mis, actor->target);
                }
            }
            return 2;
        }
    }
    return 0;
}

void C_DECL A_BeastAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3), false);
        return;
    }

    P_SpawnMissile(MT_BEASTBALL, actor, actor->target, true);
}

void P_InitWhirlwind(mobj_t *whirlwind, mobj_t *target)
{
    whirlwind->origin[VZ] -= 32;
    whirlwind->special1 = 60;
    whirlwind->special2 = 50; // Timer for active sound.
    whirlwind->special3 = 20 * TICSPERSEC; // Duration.
    whirlwind->tracer = target;
}

void C_DECL A_HeadAttack(mobj_t* actor)
{
    static int atkResolve1[] = { 50, 150 };
    static int atkResolve2[] = { 150, 200 };
    mobj_t* fire, *baseFire, *mo, *target;
    int randAttack;
    coord_t dist;
    int i;

    // Ice ball     (close 20% : far 60%)
    // Fire column  (close 40% : far 20%)
    // Whirlwind    (close 40% : far 20%)
    // Distance threshold = 8 cells

    target = actor->target;
    if(target == NULL)
        return;

    A_FaceTarget(actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(target, actor, actor, HITDICE(6), false);
        return;
    }

    dist = M_ApproxDistance(actor->origin[VX] - target->origin[VX],
                            actor->origin[VY] - target->origin[VY]) > 8 * 64;

    randAttack = P_Random();
    if(randAttack < atkResolve1[(FLT2FIX(dist) != 0)? 1 : 0])
    {
        // Ice ball
        P_SpawnMissile(MT_HEADFX1, actor, target, true);
        S_StartSound(SFX_HEDAT2, actor);
    }
    else if(randAttack < atkResolve2[(FLT2FIX(dist) != 0)? 1 : 0])
    {
        // Fire column
        baseFire = P_SpawnMissile(MT_HEADFX3, actor, target, true);
        if(baseFire != NULL)
        {
            P_MobjChangeState(baseFire, S_HEADFX3_4);  // Don't grow
            for(i = 0; i < 5; ++i)
            {
                if((fire = P_SpawnMobj(MT_HEADFX3, baseFire->origin,
                                                   baseFire->angle, 0)))
                {
                    if(i == 0)
                        S_StartSound(SFX_HEDAT1, actor);

                    fire->target = baseFire->target;
                    fire->mom[MX] = baseFire->mom[MX];
                    fire->mom[MY] = baseFire->mom[MY];
                    fire->mom[MZ] = baseFire->mom[MZ];
                    fire->damage = 0;
                    fire->special3 = (i + 1) * 2;

                    P_CheckMissileSpawn(fire);
                }
            }
        }
    }
    else
    {
        // Whirlwind.
        if((mo = P_SpawnMissile(MT_WHIRLWIND, actor, target, true)))
        {
            P_InitWhirlwind(mo, target);
            S_StartSound(SFX_HEDAT3, actor);
        }
    }
}

void C_DECL A_WhirlwindSeek(mobj_t* actor)
{
    actor->special3 -= 3;
    if(actor->special3 < 0)
    {
        actor->mom[MX] = actor->mom[MY] = actor->mom[MZ] = 0;
        P_MobjChangeState(actor, P_GetState(actor->type, SN_DEATH));
        actor->flags &= ~MF_MISSILE;
        return;
    }

    if((actor->special2 -= 3) < 0)
    {
        actor->special2 = 58 + (P_Random() & 31);
        S_StartSound(SFX_HEDAT3, actor);
    }

    if(actor->tracer && actor->tracer->flags & MF_SHADOW)
        return;

    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

void C_DECL A_HeadIceImpact(mobj_t* ice)
{
    uint i;

    for(i = 0; i < 8; ++i)
    {
        mobj_t* shard;
        angle_t angle = i * ANG45;

        if((shard = P_SpawnMobj(MT_HEADFX2, ice->origin, angle, 0)))
        {
            unsigned int an = angle >> ANGLETOFINESHIFT;

            shard->target = ice->target;
            shard->mom[MX] = shard->info->speed * FIX2FLT(finecosine[an]);
            shard->mom[MY] = shard->info->speed * FIX2FLT(finesine[an]);
            shard->mom[MZ] = -.6f;

            P_CheckMissileSpawn(shard);
        }
    }
}

void C_DECL A_HeadFireGrow(mobj_t* fire)
{
    fire->special3--;
    fire->origin[VZ] += 9;

    if(fire->special3 == 0)
    {
        fire->damage = fire->info->damage;
        P_MobjChangeState(fire, S_HEADFX3_4);
    }
}

void C_DECL A_SnakeAttack(mobj_t* actor)
{
    if(!actor->target)
    {
        P_MobjChangeState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(MT_SNAKEPRO_A, actor, actor->target, true);
}

void C_DECL A_SnakeAttack2(mobj_t* actor)
{
    if(!actor->target)
    {
        P_MobjChangeState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(MT_SNAKEPRO_B, actor, actor->target, true);
}

void C_DECL A_ClinkAttack(mobj_t* actor)
{
    int     damage;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 7) + 3);
        P_DamageMobj(actor->target, actor, actor, damage, false);
    }
}

void C_DECL A_GhostOff(mobj_t* actor)
{
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk1(mobj_t* actor)
{
    A_FaceTarget(actor);
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk2(mobj_t* actor)
{
    A_FaceTarget(actor);
    actor->flags |= MF_SHADOW;
}

void C_DECL A_WizAtk3(mobj_t* actor)
{
    mobj_t* mo;
    angle_t angle;
    coord_t momZ;

    actor->flags &= ~MF_SHADOW;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);
        return;
    }

    mo = P_SpawnMissile(MT_WIZFX1, actor, actor->target, true);
    if(mo)
    {
        momZ = mo->mom[MZ];
        angle = mo->angle;

        P_SpawnMissileAngle(MT_WIZFX1, actor, angle - (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_WIZFX1, actor, angle + (ANG45 / 8), momZ);
    }
}

void C_DECL A_Scream(mobj_t* actor)
{
    switch(actor->type)
    {
    case MT_CHICPLAYER:
    case MT_SORCERER1:
    case MT_MINOTAUR:
        // Make boss death sounds full volume
        S_StartSound(actor->info->deathSound, NULL);
        break;

    case MT_PLAYER:
        // Handle the different player death screams
        if(actor->special1 < 10)
        {
            // Wimpy death sound.
            S_StartSound(SFX_PLRWDTH, actor);
        }
        else if(actor->health > -50)
        {
            // Normal death sound.
            S_StartSound(actor->info->deathSound, actor);
        }
        else if(actor->health > -100)
        {
            // Crazy death sound.
            S_StartSound(SFX_PLRCDTH, actor);
        }
        else
        {
            // Extreme death sound.
            S_StartSound(SFX_GIBDTH, actor);
        }
        break;

    default:
        S_StartSound(actor->info->deathSound, actor);
        break;
    }
}

mobj_t* P_DropItem(mobjtype_t type, mobj_t* source, int special, int chance)
{
    mobj_t* mo;

    if(P_Random() > chance)
        return NULL;

    if((mo = P_SpawnMobjXYZ(type, source->origin[VX], source->origin[VY],
                                  source->origin[VZ] + source->height / 2, source->angle, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 8);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 8);
        if(!(mo->info->flags2 & MF2_FLOATBOB))
            mo->mom[MZ] = 5 + FIX2FLT(P_Random() << 10);

        mo->flags |= MF_DROPPED;
        mo->health = special;
    }

    return mo;
}

void C_DECL A_NoBlocking(mobj_t* actor)
{
    actor->flags &= ~MF_SOLID;

    // Check for monsters dropping things.
    switch(actor->type)
    {
    case MT_MUMMY:
    case MT_MUMMYLEADER:
    case MT_MUMMYGHOST:
    case MT_MUMMYLEADERGHOST:
        P_DropItem(MT_AMGWNDWIMPY, actor, 3, 84);
        break;

    case MT_KNIGHT:
    case MT_KNIGHTGHOST:
        P_DropItem(MT_AMCBOWWIMPY, actor, 5, 84);
        break;

    case MT_WIZARD:
        P_DropItem(MT_AMBLSRWIMPY, actor, 10, 84);
        P_DropItem(MT_ARTITOMEOFPOWER, actor, 0, 4);
        break;

    case MT_HEAD:
        P_DropItem(MT_AMBLSRWIMPY, actor, 10, 84);
        P_DropItem(MT_ARTIEGG, actor, 0, 51);
        break;

    case MT_BEAST:
        P_DropItem(MT_AMCBOWWIMPY, actor, 10, 84);
        break;

    case MT_CLINK:
        P_DropItem(MT_AMSKRDWIMPY, actor, 20, 84);
        break;

    case MT_SNAKE:
        P_DropItem(MT_AMPHRDWIMPY, actor, 5, 84);
        break;

    case MT_MINOTAUR:
        P_DropItem(MT_ARTISUPERHEAL, actor, 0, 51);
        P_DropItem(MT_AMPHRDWIMPY, actor, 10, 84);
        break;

    default:
        break;
    }
}

void C_DECL A_Explode(mobj_t *actor)
{
    int damage = 128;

    switch(actor->type)
    {
    case MT_FIREBOMB: // Time Bomb
        actor->origin[VZ] += 32;
        actor->flags &= ~MF_SHADOW;
        actor->flags |= /*MF_BRIGHTSHADOW |*/ MF_VIEWALIGN;
        break;

    case MT_MNTRFX2: // Minotaur floor fire
        damage = 24;
        break;

    case MT_SOR2FX1: // D'Sparil missile
        damage = 80 + (P_Random() & 31);
        break;

    default: break;
    }

    P_RadiusAttack(actor, actor->target, damage, damage - 1);
    P_HitFloor(actor);
}

void C_DECL A_PodPain(mobj_t* actor)
{
    int                 i, count, chance;

    chance = P_Random();
    if(chance < 128)
        return;

    if(chance > 240)
        count = 2;
    else
        count = 1;

    for(i = 0; i < count; ++i)
    {
        mobj_t*             goo;

        if((goo = P_SpawnMobjXYZ(MT_PODGOO, actor->origin[VX], actor->origin[VY],
                                actor->origin[VZ] + 48, actor->angle, 0)))
        {
            goo->target = actor;
            goo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            goo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            goo->mom[MZ] = 1.0f / 2 + FIX2FLT((P_Random() << 9));
        }
    }
}

void C_DECL A_RemovePod(mobj_t* actor)
{
    mobj_t*             mo;

    if((mo = actor->generator))
    {
        if(mo->special1 > 0)
            mo->special1--;
    }
}

void C_DECL A_MakePod(mobj_t* actor)
{
    mobj_t* mo;

    // Too many generated pods?
    if(actor->special1 == MAX_GEN_PODS)
        return;

    if(!(mo = P_SpawnMobjXYZ(MT_POD, actor->origin[VX], actor->origin[VY], 0,
                            actor->angle, MSF_Z_FLOOR)))
        return;

    if(P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]) == false)
    {
        // Didn't fit.
        P_MobjRemove(mo, true);
        return;
    }

    P_MobjChangeState(mo, S_POD_GROW1);
    P_ThrustMobj(mo, P_Random() << 24, 4.5f);

    S_StartSound(SFX_NEWPOD, mo);

    // Increment generated pod count.
    actor->special1++;

    // Link the generator to the pod.
    mo->generator = actor;
    return;
}

void C_DECL A_RestoreArtifact(mobj_t* mo)
{
    mo->flags |= MF_SPECIAL;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
    S_StartSound(SFX_RESPAWN, mo);
}

void C_DECL A_RestoreSpecialThing1(mobj_t *mo)
{
    if(mo->type == MT_WMACE)
    {
        // Do random mace placement.
        P_RepositionMace(mo);
    }

    mo->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(SFX_RESPAWN, mo);
}

void C_DECL A_RestoreSpecialThing2(mobj_t* thing)
{
    thing->flags |= MF_SPECIAL;
    P_MobjChangeState(thing, P_GetState(thing->type, SN_SPAWN));
}

static int massacreMobj(thinker_t* th, void* context)
{
    int*                count = (int*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(!mo->player && sentient(mo) && (mo->flags & MF_SHOOTABLE))
    {
        P_DamageMobj(mo, NULL, NULL, 10000, false);
        (*count)++;
    }

    return false; // Continue iteration.
}

/**
 * Kills all monsters.
 */
int P_Massacre(void)
{
    int                 count = 0;

    // Only massacre when actually in a level.
    if(G_GameState() == GS_MAP)
    {
        Thinker_Iterate(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

typedef struct {
    mobjtype_t type;
    size_t count;
} countmobjoftypeparams_t;

static int countMobjOfType(thinker_t *th, void *context)
{
    countmobjoftypeparams_t *params = (countmobjoftypeparams_t *) context;
    mobj_t *mo = (mobj_t *) th;

    if(params->type == mo->type && mo->health > 0)
    {
        params->count++;
    }

    return false; // Continue iteration.
}

typedef enum {
    ST_SPAWN_FLOOR,
    //ST_SPAWN_DOOR,
    ST_LEAVEMAP
} SpecialType;

/// @todo Should be defined in MapInfo.
typedef struct {
    //int gameModeBits;
    char const *mapPath;
    //dd_bool compatAnyBoss; ///< @c true= type requirement optional by compat option.
    mobjtype_t bossType;
    dd_bool massacreOnDeath;
    SpecialType special;
    int tag;
    int type;
} BossTrigger;

/**
 * Trigger special effects on certain maps if all "bosses" are dead.
 */
void C_DECL A_BossDeath(mobj_t *actor)
{
    static BossTrigger const bossTriggers[] =
    {
        { "E1M8", MT_HEAD,      false, ST_SPAWN_FLOOR, 666, FT_LOWER },
        { "E2M8", MT_MINOTAUR,  true,  ST_SPAWN_FLOOR, 666, FT_LOWER },
        { "E3M8", MT_SORCERER2, true,  ST_SPAWN_FLOOR, 666, FT_LOWER },
        { "E4M8", MT_HEAD,      true,  ST_SPAWN_FLOOR, 666, FT_LOWER },
        { "E5M8", MT_MINOTAUR,  true,  ST_SPAWN_FLOOR, 666, FT_LOWER },
    };
    static int const numBossTriggers = sizeof(bossTriggers) / sizeof(bossTriggers[0]);

    int i;
    AutoStr *currentMapPath = G_CurrentMapUriPath();
    for(i = 0; i < numBossTriggers; ++i)
    {
        BossTrigger const *trigger = &bossTriggers[i];

        // Not a boss on this map?
        if(actor->type != trigger->bossType) continue;

        if(Str_CompareIgnoreCase(currentMapPath, trigger->mapPath)) continue;

        // Scan the remaining thinkers to determine if this is indeed the last boss.
        {
            countmobjoftypeparams_t parm;
            parm.type  = actor->type;
            parm.count = 0;
            Thinker_Iterate(P_MobjThinker, countMobjOfType, &parm);

            // Anything left alive?
            if(parm.count) continue;
        }

        // Kill all remaining enemies?
        if(trigger->massacreOnDeath)
        {
            P_Massacre();
        }

        // Trigger the special.
        switch(trigger->special)
        {
        case ST_SPAWN_FLOOR: {
            Line *dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = trigger->tag;
            EV_DoFloor(dummyLine, (floortype_e)trigger->type);
            P_FreeDummyLine(dummyLine);
            break; }

        /*case ST_SPAWN_DOOR: {
            Line *dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = trigger->tag;
            EV_DoDoor(dummyLine, (doortype_e)trigger->type);
            P_FreeDummyLine(dummyLine);
            break; }*/

        case ST_LEAVEMAP:
            G_SetGameActionMapCompletedAndSetNextMap();
            break;

        default: DE_ASSERT_FAIL("A_BossDeath: Unknown trigger special type");
        }
    }
}

void C_DECL A_ESound(mobj_t *mo)
{
    int sound;

    switch(mo->type)
    {
    case MT_SOUNDWATERFALL:
        sound = SFX_WATERFL;
        break;

    case MT_SOUNDWIND:
        sound = SFX_WIND;
        break;

    default:
        return;
    }

    S_StartSound(sound, mo);
}

void C_DECL A_SpawnTeleGlitter(mobj_t* actor)
{
    mobj_t* mo;
    if(!actor) return;

    if((mo = P_SpawnMobjXYZ(MT_TELEGLITTER,
                           actor->origin[VX] + ((P_Random() & 31) - 16),
                           actor->origin[VY] + ((P_Random() & 31) - 16),
                           P_GetDoublep(Mobj_Sector(actor), DMU_FLOOR_HEIGHT),
                           P_Random() << 24, 0)))
    {
        mo->mom[MZ] = 1.0f / 4;
        mo->special3 = 1000;
    }
}

void C_DECL A_SpawnTeleGlitter2(mobj_t* actor)
{
    mobj_t* mo;

    if(!actor) return;

    if((mo = P_SpawnMobjXYZ(MT_TELEGLITTER2,
                           actor->origin[VX] + ((P_Random() & 31) - 16),
                           actor->origin[VY] + ((P_Random() & 31) - 16),
                           P_GetDoublep(Mobj_Sector(actor), DMU_FLOOR_HEIGHT),
                           P_Random() << 24, 0)))
    {
        mo->mom[MZ] = 1.0f / 4;
        mo->special3 = 1000;
    }
}

void C_DECL A_AccTeleGlitter(mobj_t* actor)
{
    if(++actor->special3 > 35)
        actor->mom[MZ] += actor->mom[MZ] / 2;
}

void C_DECL A_InitKeyGizmo(mobj_t* gizmo)
{
    mobj_t* mo;
    statenum_t state;

    switch(gizmo->type)
    {
    case MT_KEYGIZMOBLUE:
        state = S_KGZ_BLUEFLOAT1;
        break;

    case MT_KEYGIZMOGREEN:
        state = S_KGZ_GREENFLOAT1;
        break;

    case MT_KEYGIZMOYELLOW:
        state = S_KGZ_YELLOWFLOAT1;
        break;

    default:
        return;
    }

    if((mo = P_SpawnMobjXYZ(MT_KEYGIZMOFLOAT,
                            gizmo->origin[VX], gizmo->origin[VY], gizmo->origin[VZ] + 60,
                            gizmo->angle, 0)))
    {
        P_MobjChangeState(mo, state);
    }
}

void C_DECL A_VolcanoSet(mobj_t* volcano)
{
    volcano->tics = 105 + (P_Random() & 127);
}

void C_DECL A_VolcanoBlast(mobj_t* volcano)
{
    int i, count;

    count = 1 + (P_Random() % 3);
    for(i = 0; i < count; i++)
    {
        mobj_t* blast;
        unsigned int an;

        if((blast = P_SpawnMobjXYZ(MT_VOLCANOBLAST,
                                   volcano->origin[VX], volcano->origin[VY],
                                   volcano->origin[VZ] + 44, P_Random() << 24, 0)))
        {
            blast->target = volcano;

            an = blast->angle >> ANGLETOFINESHIFT;
            blast->mom[MX] = 1 * FIX2FLT(finecosine[an]);
            blast->mom[MY] = 1 * FIX2FLT(finesine[an]);
            blast->mom[MZ] = 2.5f + FIX2FLT(P_Random() << 10);

            S_StartSound(SFX_VOLSHT, blast);
            P_CheckMissileSpawn(blast);
        }
    }
}

void C_DECL A_VolcBallImpact(mobj_t* ball)
{
    uint i;

    if(ball->origin[VZ] <= ball->floorZ)
    {
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        ball->origin[VZ] += 28;
    }

    P_RadiusAttack(ball, ball->target, 25, 24);
    for(i = 0; i < 4; ++i)
    {
        mobj_t* tiny;

        if((tiny = P_SpawnMobj(MT_VOLCANOTBLAST, ball->origin, i * ANG90, 0)))
        {
            unsigned int  an = tiny->angle >> ANGLETOFINESHIFT;

            tiny->target = ball;
            tiny->mom[MX] = .7f * FIX2FLT(finecosine[an]);
            tiny->mom[MY] = .7f * FIX2FLT(finesine[an]);
            tiny->mom[MZ] = 1 + FIX2FLT(P_Random() << 9);

            P_CheckMissileSpawn(tiny);
        }
    }
}

void C_DECL A_SkullPop(mobj_t* actor)
{
    mobj_t* mo;

    if((mo = P_SpawnMobjXYZ(MT_BLOODYSKULL, actor->origin[VX], actor->origin[VY],
                           actor->origin[VZ] + 48, actor->angle, 0)))
    {
        player_t* player;

        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 6);

        // Attach player mobj to bloody skull.
        player = actor->player;
        actor->player = NULL;
        actor->dPlayer = NULL;
        actor->flags &= ~MF_SOLID;

        mo->player = player;
        mo->dPlayer = (player? player->plr : 0);
        mo->health = actor->health;

        if(player)
        {
            player->plr->mo = mo;
            player->plr->lookDir = 0;
            player->damageCount = 32;
        }
    }
}

void C_DECL A_CheckSkullFloor(mobj_t* actor)
{
    if(actor->origin[VZ] <= actor->floorZ)
        P_MobjChangeState(actor, S_BLOODYSKULLX1);
}

void C_DECL A_CheckSkullDone(mobj_t* actor)
{
    if(actor->special2 == 666)
        P_MobjChangeState(actor, S_BLOODYSKULLX2);
}

void C_DECL A_CheckBurnGone(mobj_t* actor)
{
    if(actor->special2 == 666)
        P_MobjChangeState(actor, S_PLAY_FDTH20);
}

void C_DECL A_FreeTargMobj(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    mo->origin[VZ] = mo->ceilingZ + 4;

    mo->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_SOLID);
    mo->flags |= MF_CORPSE | MF_DROPOFF | MF_NOGRAVITY;
    mo->flags2 &= ~(MF2_PASSMOBJ | MF2_LOGRAV);

    mo->player = NULL;
    mo->dPlayer = NULL;
}

void C_DECL A_AddPlayerCorpse(mobj_t* actor)
{
    // Too many player corpses?
    if(bodyqueslot >= BODYQUESIZE)
    {
        // Remove an old one.
        P_MobjRemove(bodyque[bodyqueslot % BODYQUESIZE], true);
    }

    bodyque[bodyqueslot % BODYQUESIZE] = actor;
    bodyqueslot++;
}

void C_DECL A_FlameSnd(mobj_t* actor)
{
    S_StartSound(SFX_HEDAT1, actor); // Burn sound.
}

void C_DECL A_HideThing(mobj_t* actor)
{
    //P_MobjUnlink(actor);
    actor->flags2 |= MF2_DONTDRAW;
}

void C_DECL A_UnHideThing(mobj_t* actor)
{
    //P_MobjLink(actor);
    actor->flags2 &= ~MF2_DONTDRAW;
}
