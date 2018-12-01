/** @file p_enemy.c Enemy thinking, AI.
 *
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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

#include "jdoom64.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_actor.h"
#include "p_tick.h"

#define FATSPREAD               (ANG90/8)
#define FAT_DELTAANGLE          (85*ANGLE_1) // jd64
#define FAT_ARM_EXTENSION_SHORT (32) // jd64
#define FAT_ARM_EXTENSION_LONG  (16) // jd64
#define FAT_ARM_HEIGHT          (64) // jd64
#define SKULLSPEED              (20)

#define TRACEANGLE              (0xc000000)


// Eight directional movement speeds.
#define MOVESPEED_DIAGONAL      (0.71716309f)

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

    if(actor->type == MT_CYBORG || actor->type == MT_SKULL)
    {
        dist /= 2;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if((coord_t)P_Random() < dist)
        return false;

    return true;
}

/**
 * Move in the current direction. $dropoff_fix
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

// MOBJ Enumeration
// ==============================================================================================================

typedef struct {
    mobjtype_t          type;
    size_t              count;
} countmobjoftypeparams_t;

typedef struct {
    mobj_t*     excludedMobj;
    mobjtype_t  type;
    int         minHealth;
    int         count;
} countmobjworker_params_t;

static int countMobjWorker(thinker_t* th, void* parms)
{
    mobj_t* mo = (mobj_t*) th;
    countmobjworker_params_t* p = (countmobjworker_params_t*) parms;

    if(p->excludedMobj == mo)       return false;   // Exluded Mobj Check
    if(p->type != mo->type)          return false;   // Type Check
    if(mo->health < p->minHealth)   return false;   // Minimum Health Check

    if(p->count < 0)                return true;    // Early out

    ++p->count;

    return false;
}

static int countMobjs(countmobjworker_params_t* parm)
{
    DE_ASSERT(parm != 0);
    parm->count = 0;
    Thinker_Iterate(P_MobjThinker, countMobjWorker, parm);
    return parm->count;
}

/*
 * Helper function for 100% of the countMobjs use cases in this file
 * Creates parameters for countMobjs, passes them to countMobjs,
 * and returns the result.
 */
static int countMobjsWithType(mobjtype_t type)
{
    countmobjworker_params_t params = { .type = type };
    return countMobjs(&params);
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
    coord_t pos[3];
    mobj_t* mo;
    int sound;

    switch(actor->info->deathSound)
    {
    case 0: return;

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
    {
        S_StartSound(sound, actor);
    }

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - 128) << 11);
    pos[VY] += FIX2FLT((P_Random() - 128) << 11);
    pos[VZ] += actor->height / 2;

    if((mo = P_SpawnMobj(MT_KABOOM, pos, P_Random() << 24, 0)))
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

    if(countMobjsWithType(actor->type) > 0)
    {   // No Bitches left alive.
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4459; // jd64 was 666.
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST); // jd64 was open.
        P_FreeDummyLine(dummyLine);
    }
}
/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_TrooSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = (mo->type == MT_TROOP? 4446 : 4447);
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}
/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_HeadSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4450;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SkulSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4452;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Bos2Special(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4453;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BossSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4454;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_PainSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4455;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_FattSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4456;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BabySpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4457;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_CybrSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4458;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SposSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4445;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

/**
 * kaiser - Used for special stuff. works only per monster!!!
 */
void C_DECL A_PossSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4444;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

// END CLUSTER FUCK

// Shared Actions
// ==============================================================================================================

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

void C_DECL A_PosAttack(mobj_t* actor)
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
void C_DECL A_Chase(mobj_t *actor)
{
    int delta;
    statenum_t state;

    // jd64 >
    if(actor->flags & MF_FLOAT)
    {
        int r = P_Random();

        if(r < 64)
            actor->mom[MZ] += 1;
        else if(r < 128)
            actor->mom[MZ] -= 1;
    }
    // < d64tc

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
        if(!gfw_Rule(fast))
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
        if(gfw_Rule(fast) || !actor->moveCount || gfw_Rule(skill) == SM_HARD)
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

// Zombie
// ==============================================================================================================

// Shotgun Guy
// ==============================================================================================================

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

// Seargent
// ==============================================================================================================

void C_DECL A_SargAttack(mobj_t* actor)
{
    int damage;

    if(!actor->target) return;

    A_FaceTarget(actor);
    if(checkMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage, false);
    }
}

/**
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SargSpecial(mobj_t* mo)
{
    A_Fall(mo);

    if(countMobjsWithType(mo->type) > 0)
    {
        Line*               dummyLine = P_AllocDummyLine();

        P_ToXLine(dummyLine)->tag = 4448;
        EV_DoFloor(dummyLine, FT_LOWERTOLOWEST);
        P_FreeDummyLine(dummyLine);
    }
}

// Player
// ==============================================================================================================

void C_DECL A_EMarineAttack2(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

// Arachnotron
// ==============================================================================================================

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

// Baby Arachnotron
// ==============================================================================================================

void C_DECL A_BspiFaceTarget(mobj_t* actor)
{
    A_FaceTarget(actor);
}

/**
 * d64tc: BspiAttack. Throw projectile.
 */
static void BabyFire(mobj_t* actor, int type, dd_bool right)
{
#define BSPISPREAD                  (ANG90/8) //its cheap but it works
#define BABY_DELTAANGLE             (85*ANGLE_1)
#define BABY_ARM_EXTENSION_SHORT    (18)
#define BABY_ARM_HEIGHT             (24)

    mobj_t* mo;
    angle_t ang;
    coord_t pos[3];

    ang = actor->angle;
    if(right)
        ang += BABY_DELTAANGLE;
    else
        ang -= BABY_DELTAANGLE;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->origin, sizeof(pos));
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

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(SFX_BSPWLK, mo);
    A_Chase(mo);
}

// Imp & Nightmare Imp
// ==============================================================================================================

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
 *
 * Correctly assumed, noticed this while doing side-by-side for upgrade - RH
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

// Mother Demon
// ==============================================================================================================

/**
 * Mother Demon: Floorfire attack.
 */
void C_DECL A_MotherFloorFire(mobj_t* actor)
{
/*
#define FIRESPREAD      (ANG90 / 8 * 4)

    mobj_t* mo;
    int an;
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

static void motherFire(mobj_t* actor, int type, angle_t angle,
    coord_t distance, float height)
{
    angle_t ang;
    coord_t pos[3];

    ang = actor->angle + angle;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += distance * FIX2FLT(finecosine[ang]);
    pos[VY] += distance * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + height;

    P_SpawnMotherMissile(type, pos[VX], pos[VY], pos[VZ], actor, actor->target);
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
 * d64tc
 */
void C_DECL A_MotherBallExplode(mobj_t* spread)
{
    uint i;

    for(i = 0; i < 8; ++i)
    {
        angle_t angle = i * ANG45;
        mobj_t* shard;

        if((shard = P_SpawnMobj(MT_HEADSHOT, spread->origin, angle, 0)))
        {
            unsigned int an = angle >> ANGLETOFINESHIFT;

            shard->target = spread->target;
            shard->mom[MX] = shard->info->speed * FIX2FLT(finecosine[an]);
            shard->mom[MY] = shard->info->speed * FIX2FLT(finesine[an]);
        }
    }
}

// Cacodemon
// ==============================================================================================================

void C_DECL A_HeadAttack(mobj_t* actor)
{
    int damage;

    if(!actor->target) return;

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

// Cyberdemon
// ==============================================================================================================

/**
 * Cyber Demon: Missile Attack.
 *
 * Heavily modified for d64tc.
 */
void C_DECL A_CyberAttack(mobj_t* actor)
{
#define CYBER_DELTAANGLE            (85*ANGLE_1)
#define CYBER_ARM_EXTENSION_SHORT   (35)
#define CYBER_ARM1_HEIGHT           (68)

    angle_t ang;
    coord_t pos[3];

    // This aligns the rocket to the d64tc cyberdemon's rocket launcher.
    ang = (actor->angle + CYBER_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finecosine[ang]);
    pos[VY] += CYBER_ARM_EXTENSION_SHORT * FIX2FLT(finesine[ang]);
    pos[VZ] += -actor->floorClip + CYBER_ARM1_HEIGHT;

    P_SpawnMotherMissile(MT_CYBERROCKET, pos[VX], pos[VY], pos[VZ],
                         actor, actor->target);

#undef CYBER_DELTAANGLE
#undef CYBER_ARM_EXTENSION_SHORT
#undef CYBER_ARM1_HEIGHT
}

/*
 * Used for field `special` in `BossTrigger`
 */
typedef enum {
    ST_SPAWN_FLOOR,
    ST_SPAWN_DOOR,
    ST_LEAVEMAP
} BossTriggerType;

/*
 * Used by A_CyberDeath
 *
 * @TODO Should be defined in MapInfo
 */
typedef struct {
    //int gameModeBits;
    char const *mapPath;
    //dd_bool compatAnyBoss; ///< @c true= type requirement optional by compat option.
    mobjtype_t bossType;
    //dd_bool massacreOnDeath;
    BossTriggerType special;
    int tag;
    int type;
} BossTrigger;

/*
 * Called by A_CyberDeath
 */
static void cyberKaboom(mobj_t *actor)
{
    coord_t pos[3];
    mobj_t *mo;

    DE_ASSERT(actor != 0);

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - 128) << 11);
    pos[VY] += FIX2FLT((P_Random() - 128) << 11);
    pos[VZ] += actor->height / 2;

    if((mo = P_SpawnMobj(MT_KABOOM, pos, P_Random() << 24, 0)))
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
}

/**
 * d64tc
 */
void C_DECL A_CyberDeath(mobj_t *mo)
{
    static BossTrigger const bossTriggers[] =
    {
        { "MAP32", MT_NONE, ST_SPAWN_DOOR, 666, (int)DT_BLAZERAISE },
        { "MAP33", MT_NONE, ST_SPAWN_DOOR, 666, (int)DT_BLAZERAISE },
        { "MAP35", MT_NONE, ST_LEAVEMAP,   0,   0 }
    };
    static int const numBossTriggers = sizeof(bossTriggers) / sizeof(bossTriggers[0]);

    AutoStr *currentMapPath = G_CurrentMapUriPath();
    int i;

    // Cyber deaths cause a rather spectacular kaboom.
    cyberKaboom(mo);

    // Make sure there is a player alive.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame && players[i].health > 0)
            break;
    }
    if(i == MAXPLAYERS) return;

    for(i = 0; i < numBossTriggers; ++i)
    {
        BossTrigger const *trigger = &bossTriggers[i];

        //if(!(trigger->gameModeBits & gameModeBits)) continue;

        // Mobj type requirement change in DOOM ver 1.9
        //if(!(cfg.anyBossDeath && trigger->compatAnyBoss))
        {
            // Not a boss on this map?
            if(mo->type != MT_NONE && mo->type != trigger->bossType) continue;
        }

        if(Str_CompareIgnoreCase(currentMapPath, trigger->mapPath)) continue;

        // Scan the remaining thinkers to determine if this is indeed the last boss.
        if(countMobjsWithType(mo->type)) continue;

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

        default: DE_ASSERT_FAIL("A_CyberDeath: Unknown trigger special type");
        }
    }
}

void C_DECL A_Hoof(mobj_t *mo)
{
    /**
     * @todo Kludge: Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_HOOF | (!Str_CompareIgnoreCase(G_CurrentMapUriPath(), "MAP08")? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    /**
     * @todo Kludge: Only play very loud sounds in map 8.
     * \todo: Implement a MAPINFO option for this.
     */
    S_StartSound(SFX_MEAL | (!Str_CompareIgnoreCase(G_CurrentMapUriPath(), "MAP08")? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

// Baron of Hell
// ==============================================================================================================

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

// SPRITE = DART
// ==============================================================================================================

void C_DECL A_SkelMissile(mobj_t* actor)
{
    if(!actor->target) return;

    A_FaceTarget(actor);

    mobj_t* mo = P_SpawnMissile(MT_TRACER, actor, actor->target);
    if(mo)
    {
        mo->origin[VX] += mo->mom[MX];
        mo->origin[VY] += mo->mom[MY];
        mo->tracer = actor->target;
    }
}

// XXX Unreferenced in objects.ded
void C_DECL A_SkelWhoosh(mobj_t* actor)
{
    if(!actor->target) return;

    A_FaceTarget(actor);
    S_StartSound(SFX_SKESWG, actor);
}

// XXX Unreferenced in objects.ded
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

// SPRITE = MANF
// ==============================================================================================================

void C_DECL A_Tracer(mobj_t* actor)
{
    angle_t exact;
    coord_t dist;
    float slope;
    mobj_t* dest, *th;

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
    exact = M_PointToAngle2(actor->origin, dest->origin);
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
    dist = M_ApproxDistance(dest->origin[VX] - actor->origin[VX],
                            dest->origin[VY] - actor->origin[VY]);

    dist /= FIX2FLT(actor->info->speed);
    if(dist < 1) dist = 1;

    slope = (dest->origin[VZ] + 40 - actor->origin[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1 / 8;
    else
        actor->mom[MZ] += 1 / 8;
}

// Mancubus
// ==============================================================================================================

void C_DECL A_FatRaise(mobj_t* actor)
{
    A_FaceTarget(actor);
    S_StartSound(SFX_MANATK, actor);
}

/**
 * d64tc: Used for mancubus projectile.
 * Called by A_FatAttack1, A_FatAttack2, and A_FatAttack3
 */
static void fatFire(mobj_t* actor, int type, angle_t spread, angle_t angle,
    coord_t distance, float height)
{
    mobj_t* mo;
    angle_t an;
    coord_t pos[3];

    an = (actor->angle + angle) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->origin, sizeof(pos));
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
    fatFire(actor, MT_FATSHOT, (angle_t)(-(FATSPREAD * 1.5f)), -FAT_DELTAANGLE,
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

// Lost Soul
// ==============================================================================================================

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
    if(dist < 1) dist = 1;

    actor->mom[MZ] = (dest->origin[VZ] + (dest->height / 2) - actor->origin[VZ]) / dist;
}

// Pain Elemental
// ==============================================================================================================

/**
 * PainElemental Attack: Spawn a lost soul and launch it at the target.
 */
void C_DECL A_PainShootSkull(mobj_t* actor, angle_t angle)
{
    coord_t pos[3], prestep;
    mobj_t* newmobj;
    Sector* sec;
    uint an;

    if(cfg.maxSkulls && (countMobjsWithType(MT_SKULL) > 20))
    {
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
    {
        // Use the original DOOM method.
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
void C_DECL A_PainAttack(mobj_t* actor)
{
    if(!actor->target) return;

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

// Missile
// ==============================================================================================================

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
    coord_t pos[3], prestep;
    mobj_t* mo;
    uint an;

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 + 3 * (actor->info->radius + MOBJINFO[MT_ROCKETPUFF].radius) / 2;

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    if((mo = P_SpawnMobj(MT_ROCKETPUFF, pos, angle, 0)))
    {
        // Check for movements $dropoff_fix.
        if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY], false, false))
        {
            // Kill it immediately.
            P_DamageMobj(mo, actor, actor, 10000, false);
        }
    }
}

/**
 * d64tc: Spawns a smoke sprite during the missle attack
 */
void C_DECL A_Rocketpuff(mobj_t *actor)
{
    if(!actor)
        return;

    P_SpawnMobj(MT_ROCKETPUFF, actor->origin, P_Random() << 24, 0);
}

void C_DECL A_Explode(mobj_t *mo)
{
    P_RadiusAttack(mo, mo->target, 128, 127);
}

/**
 * Mother Demon: Spawns a smoke sprite during the missle attack
 */
void C_DECL A_RectTracerPuff(mobj_t* smoke)
{
    if(!smoke) return;
    P_SpawnMobj(MT_MOTHERPUFF, smoke->origin, P_Random() << 24, 0);
}

void C_DECL A_RectChase(mobj_t* actor)
{
    A_Chase(actor);
}

// Generic Actions
// ==============================================================================================================

/*
 * Generic scream.
 * Used by more than one object
 */
void C_DECL A_Scream(mobj_t* actor)
{
    int sound;

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
    {
        S_StartSound(sound, actor);
    }
}

/*
 * Generic scream action
 */
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

// Laser
// ==============================================================================================================

/*
 * Emit smoke when firing the laser
 */
void C_DECL A_Lasersmoke(mobj_t *mo)
{
    if(!mo)
        return;

    P_SpawnMobj(MT_LASERDUST, mo->origin, P_Random() << 24, 0);
}

// Barrel
// ==============================================================================================================

void C_DECL A_BarrelExplode(mobj_t *actor)
{
    int i;

    S_StartSound(actor->info->deathSound, actor);
    P_RadiusAttack(actor, actor->target, 128, 127);

    if(Str_CompareIgnoreCase(G_CurrentMapUriPath(), "MAP01"))
        return;

    if(actor->type != MT_BARREL)
        return;

    // Make sure there is a player alive.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame && players[i].health > 0)
            break;
    }

    if(i == MAXPLAYERS) return;

    // Other boss not dead?
    if(countMobjsWithType(actor->type)) return;

    {
        Line *dummyLine = P_AllocDummyLine();
        P_ToXLine(dummyLine)->tag = 666;
        EV_DoDoor(dummyLine, DT_BLAZERAISE);
        P_FreeDummyLine(dummyLine);
    }
}

// TODO Unreferenced throughout entire source base except for in a DED file
//      this looks like it is either being used to stub a callback, or was meant to be implemented
//      it is used by the sprite SARG when running (state id SHAD_RUN in objects.ded)
void C_DECL A_ShadowsAction1(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

// XXX  See comment for A_ShadowsAction1
void C_DECL A_ShadowsAction2(mobj_t* actor)
{
    // STUB: What is this meant to do?
}

/**
 * d64tc - Unused?
 */
void C_DECL A_SetFloorFire(mobj_t* actor)
{
/*
    mobj_t* mo;
    coord_t pos[3];

    actor->origin[VZ] = actor->floorZ;

    memcpy(pos, actor->origin, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);
    pos[VZ]  = ONFLOORZ;

    if((mo = P_SpawnMobj(MT_SPAWNFIRE, pos)))
        mo->target = actor->target;
*/
}

// XXX Unreferenced in objects.ded
/**
 * Possibly trigger special effects if on first boss level
 *
 * kaiser - Removed exit special at end to allow MT_FATSO to properly
 *          work in Map33 for d64tc.
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    int i;

    if(mo->type != MT_BITCH)
        return;

    if(Str_CompareIgnoreCase(G_CurrentMapUriPath(), "MAP30"))
        return;

    // Make sure there is a player alive for victory.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame && players[i].health > 0)
            break;
    }

    if(i == MAXPLAYERS) return;

    // Other boss not dead?
    if(countMobjsWithType(mo->type)) return;

    G_SetGameActionMapCompletedAndSetNextMap();
}
