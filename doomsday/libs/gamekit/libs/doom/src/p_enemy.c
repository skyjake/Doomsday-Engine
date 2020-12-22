/** @file p_enemy.c Enemy thinking, AI (Doom-specific).
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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

#include "jdoom.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_saveg.h"
#include "p_tick.h"

#define FATSPREAD               (ANG90/8)
#define SKULLSPEED              (20)
#define TRACEANGLE              (0xc000000)

// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309)

static coord_t const dirSpeed[8][2] =
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

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, Mobj_Sector(emitter), 0);
}

static dd_bool checkMeleeRange(mobj_t *actor)
{
    mobj_t *pl;
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

static dd_bool checkMissileRange(mobj_t* actor)
{
    coord_t dist;

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

    dist = M_ApproxDistance(actor->origin[VX] - actor->target->origin[VX],
                            actor->origin[VY] - actor->target->origin[VY]) - 64;

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

    if((coord_t) P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction if not blocked.
 *
 * @return              @c false, if the move is blocked.
 */
static dd_bool moveMobj(mobj_t *actor, dd_bool dropoff)
{
    coord_t pos[3], step[3];
    Line *ld;
    dd_bool good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if(!VALID_MOVEDIR(actor->moveDir))
        Con_Error("Weird actor->moveDir!");

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][MX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][MY];
    pos[VX] = actor->origin[VX] + step[VX];
    pos[VY] = actor->origin[VY] + step[VY];

    // $dropoff_fix
    if(!P_TryMoveXY(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Float up and down to the contacted floor height.
        if((actor->flags & MF_FLOAT) && tmFloatOk)
        {
            if(actor->origin[VZ] < tmFloorZ)
                actor->origin[VZ] += FLOATSPEED;
            else
                actor->origin[VZ] -= FLOATSPEED;

            // What if we just floated into another mobj??
            actor->flags |= MF_INFLOAT;
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
    if(!moveMobj(actor, false))
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
            if((dirtype_t)tdir != turnaround &&
               (actor->moveDir = tdir, tryMoveMobj(actor)))
                return;
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

static int massacreMobj(thinker_t *th, void *context)
{
    int *count = (int*) context;
    mobj_t *mo = (mobj_t *) th;

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
    int count = 0;

    // Only massacre when actually in a map.
    if(G_GameState() == GS_MAP)
    {
        Thinker_Iterate(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

typedef struct {
    mobj_t *excludeMobj; // Ignore this mobj.
    mobjtype_t type; // Type requirement or MT_NONE.
    int minHealth; // Minimum health requirement or DDMAXINT.
    int count;
} countmobjworker_params_t;

static int countMobjWorker(thinker_t *th, void *parms)
{
    mobj_t *mo = (mobj_t *) th;
    countmobjworker_params_t *p = (countmobjworker_params_t *) parms;

    // Exclussion?
    if(p->excludeMobj == mo) return false;

    // Type requirement?
    if(p->type != mo->type) return false;

    // Minimum health requirement?
    if(mo->health < p->minHealth) return false;

    // This mobj passes.

    // Early out?
    if(p->count < 0) return true;

    // Add this to our running total.
    ++p->count;

    return false;
}

/**
 * Determines the number of mobj thinkers in the map which meet the criteria
 * specified with @a parm.
 */
static int countMobjs(countmobjworker_params_t *parm)
{
    DE_ASSERT(parm != 0);
    parm->count = 0;
    Thinker_Iterate(P_MobjThinker, countMobjWorker, parm);
    return parm->count;
}

/**
 * Determines whether there are no more mobj thinkers in the map which meet the
 * criteria specified with @a parm.
 */
static dd_bool noMobjRemains(countmobjworker_params_t *parm)
{
    DE_ASSERT(parm != 0);
    parm->count = -1; // Stop when first is found.
    return !Thinker_Iterate(P_MobjThinker, countMobjWorker, parm);
}

void C_DECL A_KeenDie(mobj_t *mo)
{
    countmobjworker_params_t parm;

    A_Fall(mo);

    // Check if there are any more mobjs of the same type left alive.
    parm.excludeMobj = mo;
    parm.type        = mo->type;
    parm.minHealth   = 1;

    if(noMobjRemains(&parm))
    {
        Line *dummyLine = P_AllocDummyLine();
        P_ToXLine(dummyLine)->tag = 666;
        EV_DoDoor(dummyLine, DT_OPEN);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    Sector *sec = Mobj_Sector(actor);
    mobj_t *targ;

    if(!sec) return;

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

    if(!Mobj_LookForPlayers(actor, false)) return;

    // Go into chase state.
  seeyou:
    if(actor->info->seeSound)
    {
        int sound;

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
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
        {
            actor->threshold--;
        }
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
        if(!Mobj_LookForPlayers(actor, true))
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        }
        return;
    }

    // Do not attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE && !gfw_Rule(fast))
        {
            newChaseDir(actor);
        }
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       checkMeleeRange(actor))
    {
        if(actor->info->attackSound)
        {
            S_StartSound(actor->info->attackSound, actor);
        }

        P_MobjChangeState(actor, state);
        return;
    }

    // Check for missile attack.
    if((state = P_GetState(actor->type, SN_MISSILE)) != S_NULL)
    {
        if(!(gfw_Rule(skill) != SM_NIGHTMARE &&
             !gfw_Rule(fast) && actor->moveCount))
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
        if(Mobj_LookForPlayers(actor, true))
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
    actor->angle = M_PointToAngle2(actor->origin, actor->target->origin);

    if(actor->target->flags & MF_SHADOW)
        actor->angle += (P_Random() - P_Random()) << 21;
}

void C_DECL A_PosAttack(mobj_t *actor)
{
    int damage;
    angle_t angle;
    float slope;

    if(!actor->target) return;

    A_FaceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(SFX_PISTOL, actor);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage, MT_PUFF);
}

void C_DECL A_SPosAttack(mobj_t *actor)
{
    int i, damage;
    angle_t angle, bangle;
    float slope;

    if(!actor) return;
    if(!actor->target) return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; ++i)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;

        P_LineAttack(actor, angle, MISSILERANGE, slope, damage, MT_PUFF);
    }
}

void C_DECL A_CPosAttack(mobj_t *actor)
{
    int angle, bangle, damage;
    float slope;

    if(!actor) return;
    if(!actor->target) return;

    S_StartSound(SFX_SHOTGN, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage, MT_PUFF);
}

void C_DECL A_CPosRefire(mobj_t *actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 40) return;

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
    mobj_t *mo;

    if(!actor->target) return;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(MT_TRACER, actor, actor->target);
    if(mo)
    {
        P_MobjUnlink(mo);
        mo->origin[VX] += mo->mom[MX];
        mo->origin[VY] += mo->mom[MY];
        P_MobjLink(mo);
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t *actor)
{
    uint an;
    angle_t angle;
    coord_t dist;
    float slope;
    mobj_t *dest, *th;

    if(mapTime & 3) return;

    // Clients do not spawn puffs.
    if(!IS_CLIENT)
    {
        // Spawn a puff of smoke behind the rocket.
        if((th = P_SpawnMobjXYZ(MT_ROCKETPUFF, actor->origin[VX], actor->origin[VY],
                                actor->origin[VZ] + FIX2FLT((P_Random() - P_Random()) << 10),
                                actor->angle + ANG180, 0)))
        {
            th->mom[MZ] = FIX2FLT(FRACUNIT);

            th->tics -= P_Random() & 3;
            if(th->tics < 1) th->tics = 1; // Always at least one tic.
        }
    }

    if((th = P_SpawnMobjXYZ(MT_SMOKE, actor->origin[VX] - actor->mom[MX],
                            actor->origin[VY] - actor->mom[MY], actor->origin[VZ],
                            actor->angle + ANG180, 0)))
    {
        th->mom[MZ] = FIX2FLT(FRACUNIT);

        th->tics -= P_Random() & 3;
        if(th->tics < 1) th->tics = 1;
    }

    // Adjust direction.
    dest = actor->tracer;

    if(!dest) return;
    if(dest->health <= 0) return;

    // Change angle.
    angle = M_PointToAngle2(actor->origin, dest->origin);
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
    dist = M_ApproxDistance(dest->origin[VX] - actor->origin[VX],
                            dest->origin[VY] - actor->origin[VY]);
    dist /= actor->info->speed;
    if(dist < 1) dist = 1;

    slope = (dest->origin[VZ] + 40 - actor->origin[VZ]) / dist;

    if(slope < actor->mom[MZ])
    {
        actor->mom[MZ] -= FIX2FLT(FRACUNIT / 8);
    }
    else
    {
        actor->mom[MZ] += FIX2FLT(FRACUNIT / 8);
    }
}

void C_DECL A_SkelWhoosh(mobj_t* actor)
{
    if(!actor->target) return;

    A_FaceTarget(actor);
    S_StartSound(SFX_SKESWG, actor);
}

void C_DECL A_SkelFist(mobj_t* actor)
{
    int damage;

    if(!actor->target) return;

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
typedef struct {
    mobj_t *resurrector;
    vec2d_t resurrectorOrigin; ///< Use this predicted origin (factors momentum).
    mobj_t *foundCorpse;
} pit_vilecheckparams_t;

int PIT_VileCheck(mobj_t *corpse, void *context)
{
    pit_vilecheckparams_t *parm = (pit_vilecheckparams_t*)context;
    dd_bool raisePointOpen;
    coord_t maxDist;

    // Not actually a monster corpse?
    if(!(corpse->flags & MF_CORPSE)) return false;

    // Not lying still yet?
    if(corpse->tics != -1) return false;

    // Does this mobj type have a raise state?
    if(P_GetState(corpse->type, SN_RAISE) == S_NULL)
    {
        return false;
    }

    /*
     * Don't raise if its too far from the resurrector.
     *
     * Compat option: The original game would always use the radius of the
     * MT_VILE mobj type regardless of the actual type of the resurrector.
     * This means that HacX v1.2 must have been developed and tested with
     * a port that changes this behavior by default.
     */
    maxDist = corpse->info->radius +
        (cfg.vileChaseUseVileRadius? MOBJINFO[ MT_VILE ].radius :
                                     parm->resurrector->info->radius);

    if(fabs(corpse->origin[VX] - parm->resurrectorOrigin[VX]) > maxDist ||
       fabs(corpse->origin[VY] - parm->resurrectorOrigin[VY]) > maxDist)
    {
        return false;
    }

    corpse->mom[MX] = corpse->mom[MY] = 0;

    if(!cfg.raiseGhosts) // Compat option.
    {
        coord_t const oldHeight = corpse->height;
        coord_t const oldRadius = corpse->radius;

        corpse->height = corpse->info->height;
        corpse->radius = corpse->info->radius;
        corpse->flags |= MF_SOLID;

        raisePointOpen = P_CheckPositionXY(corpse, corpse->origin[VX], corpse->origin[VY]);

        corpse->height = oldHeight;
        corpse->radius = oldRadius;
        corpse->flags &= ~MF_SOLID;
    }
    else
    {
        // Use the original more buggy approach, which may result in
        // non-solid "ghost" monsters.
        corpse->height = (coord_t)FIX2FLT(FLT2FIX(corpse->height) << 2);
        raisePointOpen = P_CheckPositionXY(corpse, corpse->origin[VX], corpse->origin[VY]);
        corpse->height = (coord_t)FIX2FLT(FLT2FIX(corpse->height) >> 2);
    }

    if(raisePointOpen)
    {
        parm->foundCorpse = corpse;
    }

    // Stop iteration as soon as a suitable corpse is found.
    return parm->foundCorpse != 0;
}

void C_DECL A_VileChase(mobj_t *actor)
{
    if(actor->moveDir != DI_NODIR)
    {
        // Search for a monster corpse that can be resurrected.
        pit_vilecheckparams_t parm;
        AABoxd aaBB;

        parm.resurrector = actor;
        parm.foundCorpse = 0;
        V2d_Copy (parm.resurrectorOrigin, dirSpeed[actor->moveDir]);
        V2d_Scale(parm.resurrectorOrigin, actor->info->speed);
        V2d_Sum  (parm.resurrectorOrigin, parm.resurrectorOrigin, actor->origin);

        aaBB.minX = parm.resurrectorOrigin[VX] - MAXRADIUS * 2;
        aaBB.minY = parm.resurrectorOrigin[VY] - MAXRADIUS * 2;
        aaBB.maxX = parm.resurrectorOrigin[VX] + MAXRADIUS * 2;
        aaBB.maxY = parm.resurrectorOrigin[VY] + MAXRADIUS * 2;

        VALIDCOUNT++;
        if(Mobj_BoxIterator(&aaBB, PIT_VileCheck, &parm))
        {
            mobj_t *corpse    = parm.foundCorpse;
            mobj_t *oldTarget = actor->target;

            // Rotate the corpse to face it's new master.
            actor->target = corpse;
            A_FaceTarget(actor);
            actor->target = oldTarget;

            // Posture a little while we work our magic.
            P_MobjChangeState(actor, S_VILE_HEAL1);
            S_StartSound(SFX_SLOP, corpse);

            // Re-animate the corpse (mostly initial state):
            P_MobjChangeState(corpse, P_GetState(corpse->type, SN_RAISE));
            if(!cfg.raiseGhosts)
            {
                corpse->height = corpse->info->height;
                corpse->radius = corpse->info->radius;
            }
            else
            {
                // The original more buggy approach.
                corpse->height = (coord_t)FIX2FLT(FLT2FIX(corpse->height) << 2);
            }
            corpse->flags  = corpse->info->flags;
            corpse->health = corpse->info->spawnHealth;
            corpse->target = NULL;
            corpse->corpseTics = 0;

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

void C_DECL A_StartFire(mobj_t* actor)
{
    S_StartSound(SFX_FLAMST, actor);
    A_Fire(actor);
}

void C_DECL A_FireCrackle(mobj_t* actor)
{
    S_StartSound(SFX_FLAME, actor);
    A_Fire(actor);
}

/**
 * Keep fire in front of player unless out of sight.
 */
void C_DECL A_Fire(mobj_t *actor)
{
    mobj_t *dest;
    vec3d_t offset;
    uint an;

    dest = actor->tracer;
    if(!dest) return;

    // Don't move it if the vile lost sight.
    if(!P_CheckSight(actor->target, dest)) return;

    an = dest->angle >> ANGLETOFINESHIFT;

    P_MobjUnlink(actor);
    V3d_Set(offset, 24 * FIX2FLT(finecosine[an]), 24 * FIX2FLT(finesine[an]), 0);
    V3d_Sum(actor->origin, dest->origin, offset);
    P_MobjLink(actor);
}

/**
 * Spawn the archviles' hellfire
 */
void C_DECL A_VileTarget(mobj_t* actor)
{
    mobj_t* fog;

    if(!actor->target) return;

    A_FaceTarget(actor);

    if((fog = P_SpawnMobj(MT_FIRE, actor->target->origin,
                          actor->target->angle + ANG180, 0)))
    {
        actor->tracer = fog;
        fog->target = actor;
        fog->tracer = actor->target;
        A_Fire(fog);
    }
}

void C_DECL A_VileAttack(mobj_t *actor)
{
    mobj_t *fire;
    uint an;

    if(!actor->target) return;

    A_FaceTarget(actor);

    if(!P_CheckSight(actor, actor->target))
    {
        return;
    }

    S_StartSound(SFX_BAREXP, actor);
    P_DamageMobj(actor->target, actor, actor, 20, false);
    actor->target->mom[MZ] =
        FIX2FLT(1000 * FRACUNIT / actor->target->info->mass);

    fire = actor->tracer;
    if(!fire) return;

    // Move the fire between the Vile and the player.
    an = actor->angle >> ANGLETOFINESHIFT;
    P_MobjUnlink(fire);
    fire->origin[VX] = actor->target->origin[VX] - 24 * FIX2FLT(finecosine[an]);
    fire->origin[VY] = actor->target->origin[VY] - 24 * FIX2FLT(finesine[an]);
    P_MobjLink(fire);
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
void C_DECL A_SkullAttack(mobj_t* actor)
{
    mobj_t* dest;
    uint an;
    coord_t dist;

    if(!actor->target) return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attackSound, actor);
    A_FaceTarget(actor);

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = SKULLSPEED * FIX2FLT(finecosine[an]);
    actor->mom[MY] = SKULLSPEED * FIX2FLT(finesine[an]);

    dist = M_ApproxDistance(dest->origin[VX] - actor->origin[VX],
                            dest->origin[VY] - actor->origin[VY]);
    dist /= SKULLSPEED;

    if(dist < 1)
        dist = 1;
    actor->mom[MZ] = (dest->origin[VZ] + (dest->height / 2) - actor->origin[VZ]) / dist;
}

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    coord_t pos[3];
    mobj_t *newmobj;
    uint an;
    coord_t prestep;
    Sector *sec;

    if(cfg.maxSkulls)
    {
        // Limit the number of MT_SKULL's we should spawn.
        countmobjworker_params_t parm;

        parm.excludeMobj = 0;
        parm.type        = MT_SKULL;
        parm.minHealth   = 1;

        if(countMobjs(&parm) > 20)
            return; // Too many, don't spit another.
    }

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 +
        3 * ((actor->info->radius + MOBJINFO[MT_SKULL].radius) / 2);

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    // Compat option to prevent spawning lost souls inside walls.
    if(!cfg.allowSkullsInWalls)
    {
        /*
         * Check whether the Lost Soul is being fired through a 1-sided
         * wall or an impassible line, or a "monsters can't cross" line.
         * If it is, then we don't allow the spawn.
         */

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        if(!(newmobj = P_SpawnMobj(MT_SKULL, pos, angle, 0)))
            return;

        sec = Mobj_Sector(newmobj);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->origin[VZ] > (P_GetDoublep(sec, DMU_CEILING_HEIGHT) - newmobj->height)) ||
           (newmobj->origin[VZ] < P_GetDoublep(sec, DMU_FLOOR_HEIGHT)))
        {
            // Kill it immediately.
            P_DamageMobj(newmobj, actor, actor, 10000, false);
            return;
        }
    }
    else
    {   // Use the original DOOM method.
        if(!(newmobj = P_SpawnMobj(MT_SKULL, pos, angle, 0)))
            return;
    }

    // Check for movements, $dropoff_fix.
    if(!P_TryMoveXY(newmobj, newmobj->origin[VX], newmobj->origin[VY], false, false))
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

typedef enum {
    ST_SPAWN_FLOOR,
    ST_SPAWN_DOOR,
    ST_LEAVEMAP
} SpecialType;

/// @todo Should be defined in MapInfo.
typedef struct {
    int gameModeBits;
    char const *mapPath;
    dd_bool compatAnyBoss; ///< @c true= type requirement optional by compat option.
    mobjtype_t bossType;
    //dd_bool massacreOnDeath;
    SpecialType special;
    int tag;
    int type;
} BossTrigger;

/**
 * Trigger special effects on certain maps if all "bosses" are dead.
 *
 * @note DOOM ver 1.9 behavoir change:
 * @see http://doomwiki.org/wiki/Tag_666#Difference_in_behaviour_between_Doom1_and_Ultimate_Doom
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    static BossTrigger const bossTriggers[] =
    {
        { GM_ANY_DOOM2, "MAP07", false, MT_FATSO,   ST_SPAWN_FLOOR, 666, (int)FT_LOWERTOLOWEST  },
        { GM_ANY_DOOM2, "MAP07", false, MT_BABY,    ST_SPAWN_FLOOR, 667, (int)FT_RAISETOTEXTURE },
        { GM_ANY_DOOM,  "E1M8",  true,  MT_BRUISER, ST_SPAWN_FLOOR, 666, (int)FT_LOWERTOLOWEST  },
        { GM_ANY_DOOM,  "E2M8",  true,  MT_CYBORG,  ST_LEAVEMAP,    0,   0                      },
        { GM_ANY_DOOM,  "E3M8",  true,  MT_SPIDER,  ST_LEAVEMAP,    0,   0                      },
        { GM_ANY_DOOM,  "E4M6",  false, MT_CYBORG,  ST_SPAWN_DOOR,  666, (int)DT_BLAZEOPEN      },
        { GM_ANY_DOOM,  "E4M8",  false, MT_SPIDER,  ST_SPAWN_FLOOR, 666, (int)FT_LOWERTOLOWEST  }
    };
    static int const numBossTriggers = sizeof(bossTriggers) / sizeof(bossTriggers[0]);

    AutoStr *currentMapPath = G_CurrentMapUriPath();

    // Make sure there is a player alive.
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame && players[i].health > 0)
            break;
    }
    if(i == MAXPLAYERS) return;

    for(i = 0; i < numBossTriggers; ++i)
    {
        BossTrigger const *trigger = &bossTriggers[i];

        if(!(trigger->gameModeBits & gameModeBits)) continue;

        // Mobj type requirement change in DOOM ver 1.9
        if(!(cfg.anyBossDeath && trigger->compatAnyBoss))
        {
            // Not a boss on this map?
            if(mo->type != trigger->bossType) continue;
        }

        if(Str_CompareIgnoreCase(currentMapPath, trigger->mapPath)) continue;

        // Scan the remaining thinkers to determine if this is indeed the last boss.
        {
            countmobjworker_params_t parm;
            parm.excludeMobj = mo;
            parm.type        = mo->type;
            parm.minHealth   = 1;

            // Anything left alive?
            if(!noMobjRemains(&parm)) continue;
        }

        // Kill all remaining enemies?
        /*if(trigger->massacreOnDeath)
        {
            P_Massacre();
        }*/

        // Trigger the special.
        switch(trigger->special)
        {
        case ST_SPAWN_FLOOR: {
            Line *dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = trigger->tag;
            EV_DoFloor(dummyLine, (floortype_e)trigger->type);
            P_FreeDummyLine(dummyLine);
            break; }

        case ST_SPAWN_DOOR: {
            Line *dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = trigger->tag;
            EV_DoDoor(dummyLine, (doortype_e)trigger->type);
            P_FreeDummyLine(dummyLine);
            break; }

        case ST_LEAVEMAP:
            G_SetGameActionMapCompletedAndSetNextMap();
            break;

        default: DE_ASSERT_FAIL("A_BossDeath: Unknown trigger special type");
        }
    }
}

void C_DECL A_Hoof(mobj_t *mo)
{
    /**
     * @todo Kludge: Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    AutoStr *currentMapPath = G_CurrentMapUriPath();
    S_StartSound(SFX_HOOF |
                 (!(gameModeBits & GM_ANY_DOOM2) &&
                  (!Str_CompareIgnoreCase(currentMapPath, "E1M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E2M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E3M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E4M8"))? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    /**
     * @todo Kludge: Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    AutoStr *currentMapPath = G_CurrentMapUriPath();
    S_StartSound(SFX_METAL |
                 (!(gameModeBits & GM_ANY_DOOM2) &&
                  (!Str_CompareIgnoreCase(currentMapPath, "E1M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E2M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E3M8") ||
                   !Str_CompareIgnoreCase(currentMapPath, "E4M8"))? DDSF_NO_ATTENUATION : 0), mo);
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
    coord_t pos[3];

    pos[VX] = mo->origin[VX] - 196; // -280
    pos[VY] = mo->origin[VY] - 320; //  280

    for(; pos[VX] < mo->origin[VX] + 320 /*280*/; pos[VX] += 8)
    {
        mobj_t *th;

        pos[VZ] = FIX2FLT(128) + (P_Random() * 2);

        if((th = P_SpawnMobj(MT_ROCKET, pos, P_Random() << 24, 0)))
        {
            th->mom[MZ] = FIX2FLT(P_Random() * 512);

            P_MobjChangeState(th, S_BRAINEXPLODE1);

            th->tics -= P_Random() & 7;
            if(th->tics < 1)
                th->tics = 1;
        }
    }

    S_StartSound(SFX_BOSDTH, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    coord_t pos[3];
    mobj_t *th;

    pos[VX] = mo->origin[VX] + FIX2FLT((P_Random() - P_Random()) * 2048);
    pos[VY] = mo->origin[VY];
    pos[VZ] = FIX2FLT(128) + (P_Random() * 2);

    if((th = P_SpawnMobj(MT_ROCKET, pos, P_Random() << 24, 0)))
    {
        th->mom[MZ] = FIX2FLT(P_Random() * 512);

        P_MobjChangeState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }
}

void C_DECL A_BrainDie(mobj_t *mo)
{
    DE_UNUSED(mo);
    G_SetGameActionMapCompletedAndSetNextMap();
}

void C_DECL A_BrainSpit(mobj_t *mo)
{
    mobj_t *targ = BossBrain_NextTarget(theBossBrain);
    mobj_t *newmobj;

    if(!targ) return;

    // Shoot a cube at this target.
    newmobj = P_SpawnMissile(MT_SPAWNSHOT, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->origin[VY] - mo->origin[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
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
    if((fog = P_SpawnMobj(MT_SPAWNFIRE, targ->origin, targ->angle + ANG180, 0)))
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

    if((newmobj = P_SpawnMobj(type, targ->origin, P_Random() << 24, 0)))
    {
        if(Mobj_LookForPlayers(newmobj, true))
            P_MobjChangeState(newmobj, P_GetState(newmobj->type, SN_SEE));

        // Telefrag anything in this spot.
        P_TeleportMove(newmobj, newmobj->origin[VX], newmobj->origin[VY], false);
    }

    // Remove self (i.e., cube).
    P_MobjRemove(mo, true);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    int sound = SFX_PLDETH; // Default death sound.

    if((gameModeBits & GM_ANY_DOOM2) && (mo->health < -50))
    {
        // If the player dies less with less than -50% without gibbing.
        sound = SFX_PDIEHI;
    }

    S_StartSound(sound, mo);
}
