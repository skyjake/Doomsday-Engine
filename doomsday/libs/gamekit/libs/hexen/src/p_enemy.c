/** @file p_enemy.c  Enemy thinking, AI.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include <string.h>
#include <math.h>
#include <assert.h>

#include "jhexen.h"
#include "p_enemy.h"

#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "g_common.h"
#include "p_map.h"
#include "p_mapspec.h"

#define MONS_LOOK_RANGE             (16*64)
#define MONS_LOOK_LIMIT             64

#define MINOTAUR_LOOK_DIST          (16*54)

#define CORPSEQUEUESIZE             64
#define BODYQUESIZE                 32

#define SORCBALL_INITIAL_SPEED      7
#define SORCBALL_TERMINAL_SPEED     25
#define SORCBALL_SPEED_ROTATIONS    5
#define SORC_DEFENSE_TIME           255
#define SORC_DEFENSE_HEIGHT         45
#define BOUNCE_TIME_UNIT            (TICSPERSEC/2)
#define SORCFX4_RAPIDFIRE_TIME      (6*3) // 3 seconds
#define SORCFX4_SPREAD_ANGLE        20

#define SORC_DECELERATE             0
#define SORC_ACCELERATE             1
#define SORC_STOPPING               2
#define SORC_FIRESPELL              3
#define SORC_STOPPED                4
#define SORC_NORMAL                 5
#define SORC_FIRING_SPELL           6

#define BALL1_ANGLEOFFSET           0
#define BALL2_ANGLEOFFSET           (ANGLE_MAX/3)
#define BALL3_ANGLEOFFSET           ((ANGLE_MAX/3)*2)

#define KORAX_SPIRIT_LIFETIME       (5*(TICSPERSEC/5))  // 5 seconds
#define KORAX_COMMAND_HEIGHT        (120)
#define KORAX_COMMAND_OFFSET        (27)

#define KORAX_TID                   (245)
#define KORAX_FIRST_TELEPORT_TID    (248)
#define KORAX_TELEPORT_TID          (249)

/**
 * Describes a relative spawn point for a missile (POD).
 */
typedef struct MissileSpawnPoint_s {
    angle_t angle;
    coord_t distance;
    coord_t height;
} MissileSpawnPoint;

int maulatorSeconds = 25;
//dd_bool fastMonsters = false;

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

mobj_t *corpseQueue[CORPSEQUEUESIZE];
int corpseQueueSlot;
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
void P_NoiseAlert(mobj_t* target, mobj_t *emitter)
{
    VALIDCOUNT++;
    P_RecursiveSound(target, Mobj_Sector(emitter), 0);
}

dd_bool P_CheckMeleeRange(mobj_t* actor, dd_bool midrange)
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

    range = MELEERANGE - 20 + (pl->info? pl->info->radius : 0); // When is `info` null? -jk
    if(midrange)
    {
        if(dist >= range * 2 || dist < range)
            return false;
    }
    else
    {
        if(dist >= range)
            return false;
    }

    if(!P_CheckSight(actor, pl))
        return false;

    return true;
}

dd_bool P_CheckMissileRange(mobj_t* mo)
{
    coord_t dist;

    if(!P_CheckSight(mo, mo->target))
    {
        return false;
    }

    if(mo->flags & MF_JUSTHIT)
    {
        // The target just hit the enemy, so fight back!
        mo->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(mo->reactionTime)
        return false; // Don't attack yet.

    dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                            mo->origin[VY] - mo->target->origin[VY]) - 64;

    if(P_GetState(mo->type, SN_MELEE) == S_NULL)
    {
        // No melee attack, so fire more frequently.
        dist -= 128;
    }

    if(dist > 200)
    {
        dist = 200;
    }

    if(P_Random() < dist)
    {
        return false;
    }

    return true;
}

/**
 * Move in the current direction.
 *
 * @return              @c false, if the move is blocked.
 */
dd_bool P_Move(mobj_t* mo)
{
    coord_t tryPos[2], step[2];
    Line* ld;
    dd_bool good;

    if(mo->flags2 & MF2_BLASTED)
        return true;

    if(mo->moveDir == DI_NODIR)
        return false;

    if(!VALID_MOVEDIR(mo->moveDir))
        Con_Error("Weird actor->moveDir!");

    step[VX] = mo->info->speed * dirSpeed[mo->moveDir][VX];
    step[VY] = mo->info->speed * dirSpeed[mo->moveDir][VY];
    tryPos[VX] = mo->origin[VX] + step[VX];
    tryPos[VY] = mo->origin[VY] + step[VY];

    if(!P_TryMoveXY(mo, tryPos[VX], tryPos[VY]))
    {
        // Open any specials.
        if((mo->flags & MF_FLOAT) && tmFloatOk)
        {
            // Must adjust height.
            if(mo->origin[VZ] < tmFloorZ)
            {
                mo->origin[VZ] += FLOATSPEED;
            }
            else
            {
                mo->origin[VZ] -= FLOATSPEED;
            }

            mo->flags |= MF_INFLOAT;
            return true;
        }

        if(IterList_Empty(spechit)) return false;

        mo->moveDir = DI_NODIR;
        good = false;
        while((ld = IterList_Pop(spechit)) != NULL)
        {
            // If the special isn't a door that can be opened, return false.
            if(P_ActivateLine(ld, mo, 0, SPAC_USE))
            {
                good = true;
            }
        }
        return good;
    }
    else
    {
        P_MobjSetSRVO(mo, step[VX], step[VY]);
        mo->flags &= ~MF_INFLOAT;
    }

    if(!(mo->flags & MF_FLOAT))
    {
        if(mo->origin[VZ] > mo->floorZ)
        {
            P_HitFloor(mo);
        }
        mo->origin[VZ] = mo->floorZ;
    }

    return true;
}

/**
 * Attempts to move actor in its current (ob->moveangle) direction.
 * If a door is in the way, an OpenDoor call is made to start it opening.
 *
 * @return              @c false, if blocked by either a wall or an actor.
 */
dd_bool P_TryWalk(mobj_t *actor)
{
    if(!P_Move(actor))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void newChaseDir(mobj_t *actor, coord_t deltaX, coord_t deltaY)
{
    dirtype_t xdir, ydir;
    dirtype_t olddir = actor->moveDir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR)  // find reverse direction
        turnaround ^= 4;

    xdir = (deltaX > 10 ? DI_EAST : deltaX < -10 ? DI_WEST : DI_NODIR);
    ydir = (deltaY < -10 ? DI_SOUTH : deltaY > 10 ? DI_NORTH : DI_NODIR);

    // Try direct route.
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->moveDir =
                      deltaY < 0 ? deltaX >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltaX >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
        return;

    // Try other directions.
    if(P_Random() > 200 || fabs(deltaY) > fabs(deltaX))
    {
        dirtype_t temp = xdir;

        xdir = ydir;
        ydir = temp;
    }

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->moveDir = xdir, P_TryWalk(actor)))
        return; // Either moved forward or attacked.

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->moveDir = ydir, P_TryWalk(actor)))
        return;

    // There is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->moveDir = olddir, P_TryWalk(actor)))
        return;

    // Randomly determine direction of search.
    if(P_Random() & 1)
    {
        int tdir;
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        {
            if((dirtype_t)tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
        }
    }
    else
    {
        int tdir;
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
        {
            if((dirtype_t)tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
        }
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !P_TryWalk(actor))
        actor->moveDir = DI_NODIR;
}

void P_NewChaseDir(mobj_t* actor)
{
    coord_t delta[2];

    if(!actor->target)
        Con_Error("P_NewChaseDir: called with no target");

    delta[VX] = actor->target->origin[VX] - actor->origin[VX];
    delta[VY] = actor->target->origin[VY] - actor->origin[VY];

    newChaseDir(actor, delta[VX], delta[VY]);
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
    mobj_t*             checkMinotaurTracer;
    byte                randomSkip;
} findmobjparams_t;

static int findMobj(thinker_t* th, void* context)
{
    findmobjparams_t* params = (findmobjparams_t*) context;
    mobj_t* mo = (mobj_t *) th;

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

    // Check the special case of a minotaur looking at it's master.
    if(params->checkMinotaurTracer)
        if(mo->type == MT_MINOTAUR &&
           mo->target != params->checkMinotaurTracer)
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
    params.checkMinotaurTracer = (mo->type == MT_MINOTAUR)?
        ((player_t *) mo->tracer)->plr->mo : NULL;
    Thinker_Iterate(P_MobjThinker, findMobj, &params);

    if(params.foundMobj)
    {
        mo->target = params.foundMobj;
        return true;
    }

    return false;
}

/**
 * @param allaround     @c false = only look 180 degrees in front of
 *                      the actor.
 *
 * @return              @c true, if a player was targeted.
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

    actor->threshold = 0; // Any shot will wake up.
    targ = P_ToXSector(Mobj_Sector(actor))->soundTarget;
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
        {   // Full volume
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
        actor->reactionTime--;

    // Modify target threshold
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gfw_Rule(skill) == SM_NIGHTMARE /*|| gameRules.fast*/)
    {
        // Monsters move faster in nightmare mode
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
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

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        if(P_LookForPlayers(actor, true))
        {   // Got a new target.
            return;
        }

        P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor, false))
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
    if(--actor->moveCount < 0 || !P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        if(actor->type == MT_BISHOP && P_Random() < 128)
        {
            S_StartSound(actor->info->seeSound, actor);
        }
        else if(actor->type == MT_PIG)
        {
            S_StartSound(SFX_PIG_ACTIVE1 + (P_Random() & 1), actor);
        }
        else if(actor->flags2 & MF2_BOSS)
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

    if(actor->target->flags & MF_SHADOW)
    {
        // Target is a ghost
        actor->angle += (P_Random() - P_Random()) << 21;
    }
}

void C_DECL A_Pain(mobj_t* actor)
{
    if(actor->info->painSound)
    {
        S_StartSound(actor->info->painSound, actor);
    }
}

void C_DECL A_SetInvulnerable(mobj_t *actor)
{
    actor->flags2 |= MF2_INVULNERABLE;
}

void C_DECL A_UnSetInvulnerable(mobj_t *actor)
{
    actor->flags2 &= ~MF2_INVULNERABLE;
}

void C_DECL A_SetReflective(mobj_t *actor)
{
    actor->flags2 |= MF2_REFLECTIVE;

    if(actor->type == MT_CENTAUR || actor->type == MT_CENTAURLEADER)
    {
        A_SetInvulnerable(actor);
    }
}

void C_DECL A_UnSetReflective(mobj_t *actor)
{
    actor->flags2 &= ~MF2_REFLECTIVE;

    if(actor->type == MT_CENTAUR || actor->type == MT_CENTAURLEADER)
    {
        A_UnSetInvulnerable(actor);
    }
}

/**
 * @return              @c true, if the pig morphs.
 */
dd_bool P_UpdateMorphedMonster(mobj_t* actor, int tics)
{
    mobj_t* fog;
    coord_t pos[3];
    mobjtype_t moType;
    mobj_t* mo, oldMonster;

    actor->special1 -= tics;
    if(actor->special1 > 0)
    {
        return false;
    }

    moType = actor->special2;
    switch(moType)
    {
    case MT_WRAITHB: // These must remain morphed.
    case MT_SERPENT:
    case MT_SERPENTLEADER:
    case MT_MINOTAUR:
        return false;

    default:
        break;
    }
    memcpy(pos, actor->origin, sizeof(pos));

    /// @todo Do this properly!
    oldMonster = *actor; // Save pig vars.

    P_MobjRemoveFromTIDList(actor);
    P_MobjChangeState(actor, S_FREETARGMOBJ);
    if(!(mo = P_SpawnMobj(moType, pos, oldMonster.angle, 0)))
        return false;

    if(!P_TestMobjLocation(mo))
    {
        // Didn't fit.
        P_MobjRemove(mo, true);
        if((mo = P_SpawnMobj(oldMonster.type, pos, oldMonster.angle, 0)))
        {
            mo->flags = oldMonster.flags;
            mo->health = oldMonster.health;
            mo->target = oldMonster.target;
            mo->special = oldMonster.special;
            mo->special1 = 5 * TICSPERSEC; // Next try in 5 seconds.
            mo->special2 = moType;
            mo->tid = oldMonster.tid;
            memcpy(mo->args, oldMonster.args, 5);

            P_MobjInsertIntoTIDList(mo, oldMonster.tid);
        }
        return false;
    }

    mo->target = oldMonster.target;
    mo->tid = oldMonster.tid;
    mo->special = oldMonster.special;
    memcpy(mo->args, oldMonster.args, 5);

    P_MobjInsertIntoTIDList(mo, oldMonster.tid);
    if((fog = P_SpawnMobjXYZ(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                            oldMonster.angle + ANG180, 0)))
        S_StartSound(SFX_TELEPORT, fog);
    return true;
}

void C_DECL A_PigLook(mobj_t *actor)
{
    if(P_UpdateMorphedMonster(actor, 10))
    {
        return;
    }

    A_Look(actor);
}

void C_DECL A_PigChase(mobj_t *actor)
{
    if(P_UpdateMorphedMonster(actor, 3))
    {
        return;
    }

    A_Chase(actor);
}

void C_DECL A_PigAttack(mobj_t *actor)
{
    if(P_UpdateMorphedMonster(actor, 18))
    {
        return;
    }

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, 2 + (P_Random() & 1), false);
        S_StartSound(SFX_PIG_ATTACK, actor);
    }
}

void C_DECL A_PigPain(mobj_t *actor)
{
    A_Pain(actor);

    if(actor->origin[VZ] <= actor->floorZ)
    {
        actor->mom[MZ] = 3.5f;
    }
}

void FaceMovementDirection(mobj_t *actor)
{
    switch(actor->moveDir)
    {
    case DI_EAST:
        actor->angle = 0 << 24;
        break;

    case DI_NORTHEAST:
        actor->angle = 32 << 24;
        break;

    case DI_NORTH:
        actor->angle = 64 << 24;
        break;

    case DI_NORTHWEST:
        actor->angle = 96 << 24;
        break;

    case DI_WEST:
        actor->angle = 128 << 24;
        break;

    case DI_SOUTHWEST:
        actor->angle = 160 << 24;
        break;

    case DI_SOUTH:
        actor->angle = 192 << 24;
        break;

    case DI_SOUTHEAST:
        actor->angle = 224 << 24;
        break;
    }
}

/**
 * Minotaur variables
 *
 * special1        pointer to player that spawned it (mobj_t)
 * special2        internal to minotaur AI
 * args[0]         args[0]-args[3] together make up minotaur start time
 * args[1]         |
 * args[2]         |
 * args[3]         V
 * args[4]         charge duration countdown
 */

void C_DECL A_MinotaurFade0(mobj_t *actor)
{
    actor->flags &= ~MF_ALTSHADOW;
    actor->flags |= MF_SHADOW;
}

void C_DECL A_MinotaurFade1(mobj_t *actor)
{
    // Second level of transparency
    actor->flags &= ~MF_SHADOW;
    actor->flags |= MF_ALTSHADOW;
}

void C_DECL A_MinotaurFade2(mobj_t *actor)
{
    // Make fully visible
    actor->flags &= ~MF_SHADOW;
    actor->flags &= ~MF_ALTSHADOW;
}

void C_DECL A_MinotaurRoam(mobj_t *actor)
{
    actor->flags &= ~MF_SHADOW; // In case pain caused him to
    actor->flags &= ~MF_ALTSHADOW; // Skip his fade in.

    if((mapTime - actor->argsUInt) >= MAULATORTICS)
    {
        P_DamageMobj(actor, NULL, NULL, 10000, false);
        return;
    }

    if(P_Random() < 30)
        A_MinotaurLook(actor); // Adjust to closest target.

    if(P_Random() < 6)
    {   // Choose new direction
        actor->moveDir = P_Random() % 8;
        FaceMovementDirection(actor);
    }

    if(!P_Move(actor))
    {   // Turn
        if(P_Random() & 1)
            actor->moveDir = (actor->moveDir + 1) % 8;
        else
            actor->moveDir = (actor->moveDir + 7) % 8;
        FaceMovementDirection(actor);
    }
}

typedef struct {
    mobj_t*             notThis, *notThis2;
    mobj_t*             checkMinotaurTracer;
    coord_t             origin[2];
    coord_t             maxDistance;
    int                 minHealth;
    mobj_t*             foundMobj;
} findmonsterparams_t;

static int findMonster(thinker_t* th, void* context)
{
    findmonsterparams_t* params = (findmonsterparams_t*) context;
    mobj_t* mo = (mobj_t*) th;

    if(!(mo->flags & MF_COUNTKILL))
        return false; // Continue iteration.

    // Health requirement?
    if(!(params->minHealth < 0) && mo->health < params->minHealth)
        return false; // Continue iteration.

    if(!(mo->flags & MF_SHOOTABLE))
        return false; // Continue iteration.

    // Within range?
    if(params->maxDistance > 0)
    {
        coord_t dist = M_ApproxDistance(params->origin[VX] - mo->origin[VX],
                                        params->origin[VY] - mo->origin[VY]);
        if(dist > params->maxDistance)
            return false; // Continue iteration.
    }

    if(params->notThis && params->notThis == mo)
        return false; // Continue iteration.
    if(params->notThis2 && params->notThis2 == mo)
        return false; // Continue iteration.

    // Check the special case for minotaurs.
    if(params->checkMinotaurTracer)
        if(mo->type == MT_MINOTAUR && params->checkMinotaurTracer == mo->tracer)
            return false; // Continue iteration.

    // Found one!
    params->foundMobj = mo;
    return true; // Stop iteration.
}

/**
 * Look for enemy of player.
 */
void C_DECL A_MinotaurLook(mobj_t *actor)
{
    mobj_t *master = actor->tracer;

    actor->target = 0;

    if(gfw_Rule(deathmatch))
    {
        // Quick search for players.
        int i;
        coord_t dist;
        player_t *player;
        mobj_t *mo;

        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->inGame) continue;

            player = &players[i];
            mo = player->plr->mo;
            if(mo == master) continue;
            if(mo->health <= 0) continue;

            dist = M_ApproxDistance(actor->origin[VX] - mo->origin[VX],
                                    actor->origin[VY] - mo->origin[VY]);

            if(dist > MINOTAUR_LOOK_DIST) continue;

            actor->target = mo;
            break;
        }
    }

    if(!actor->target) // Near player monster search.
    {
        if(master && (master->health > 0) && (master->player))
            actor->target = P_RoughMonsterSearch(master, 20*128);
        else
            actor->target = P_RoughMonsterSearch(actor, 20*128);
    }

    if(!actor->target) // Normal monster search.
    {
        findmonsterparams_t     params;

        params.notThis = actor;
        params.notThis2 = master;
        params.origin[VX] = actor->origin[VX];
        params.origin[VY] = actor->origin[VY];
        params.maxDistance = MINOTAUR_LOOK_DIST;
        params.foundMobj = NULL;
        params.minHealth = 1;
        params.checkMinotaurTracer = actor->tracer;
        if(Thinker_Iterate(P_MobjThinker, findMonster, &params))
            actor->target = params.foundMobj;
    }

    if(actor->target)
    {
        P_MobjChangeStateNoAction(actor, S_MNTR_WALK1);
    }
    else
    {
        P_MobjChangeStateNoAction(actor, S_MNTR_ROAM1);
    }
}

void C_DECL A_MinotaurChase(mobj_t* actor)
{
    statenum_t state;

    actor->flags &= ~MF_SHADOW; // In case pain caused him to.
    actor->flags &= ~MF_ALTSHADOW;  // Skip his fade in.

    if((mapTime - actor->argsUInt) >= MAULATORTICS)
    {
        P_DamageMobj(actor, NULL, NULL, 10000, false);
        return;
    }

    if(P_Random() < 30)
        A_MinotaurLook(actor);  // Adjust to closest target.

    if(!actor->target || (actor->target->health <= 0) ||
       !(actor->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        P_MobjChangeState(actor, S_MNTR_LOOK1);
        return;
    }

    FaceMovementDirection(actor);
    actor->reactionTime = 0;

    // Melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, state);
        return;
    }

    // Missile attack.
    if((state = P_GetState(actor->type, SN_MISSILE)) != S_NULL &&
       P_CheckMissileRange(actor))
    {
        P_MobjChangeState(actor, state);
        return;
    }

    // Chase towards target.
    if(!P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    // Active sound.
    if(actor->info->activeSound && P_Random() < 6)
        S_StartSound(actor->info->activeSound, actor);
}

/**
 * Minotaur: Melee attack.
 */
void C_DECL A_MinotaurAtk1(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(SFX_MAULATOR_HAMMER_SWING, actor);
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);
    }
}

/**
 * Minotaur: Choose a missile attack.
 */
void C_DECL A_MinotaurDecide(mobj_t* actor)
{
#define MNTR_CHARGE_SPEED       (23)

    uint an;
    mobj_t* target = actor->target;
    coord_t dist;

    if(!target) return;

    dist = M_ApproxDistance(actor->origin[VX] - target->origin[VX],
                            actor->origin[VY] - target->origin[VY]);

    if(target->origin[VZ] + target->height > actor->origin[VZ] &&
       target->origin[VZ] + target->height < actor->origin[VZ] + actor->height &&
       dist < 16 * 64 && dist > 1 *64)
    {
        if(P_Random() < 230)
        {
            // Charge attack.
            // Don't call the state function right away.
            P_MobjChangeStateNoAction(actor, S_MNTR_ATK4_1);
            actor->flags |= MF_SKULLFLY;
            A_FaceTarget(actor);

            an = actor->angle >> ANGLETOFINESHIFT;
            actor->mom[MX] = MNTR_CHARGE_SPEED * FIX2FLT(finecosine[an]);
            actor->mom[MY] = MNTR_CHARGE_SPEED * FIX2FLT(finesine[an]);
            actor->args[4] = TICRATE / 2; // Charge duration.
            return;
        }
    }

    if(target->origin[VZ] == target->floorZ && dist < 9 * 64)
    {
        if(P_Random() < 100)
        {
            // Floor fire attack.
            P_MobjChangeState(actor, S_MNTR_ATK3_1);
            actor->special2 = 0;
            return;
        }
    }

    // Swing attack.
    A_FaceTarget(actor);
    // Don't need to call P_MobjChangeState because the current state
    // falls through to the swing attack.

#undef MNTR_CHARGE_SPEED
}

/**
 * Minotaur: Charge attack.
 */
void C_DECL A_MinotaurCharge(mobj_t* actor)
{
    mobj_t* puff;

    if(!actor->target) return;

    if(actor->args[4] > 0)
    {
        if((puff = P_SpawnMobj(MT_PUNCHPUFF, actor->origin, P_Random() << 24, 0)))
        {
            puff->mom[MZ] = 2;
        }

        actor->args[4]--;
    }
    else
    {
        actor->flags &= ~MF_SKULLFLY;
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SEE));
    }
}

/**
 * Minotaur: Swing attack.
 */
void C_DECL A_MinotaurAtk2(mobj_t* mo)
{
    mobj_t* pmo;
    angle_t angle;
    coord_t momZ;

    if(!mo->target) return;

    S_StartSound(SFX_MAULATOR_HAMMER_SWING, mo);
    if(P_CheckMeleeRange(mo, false))
    {
        P_DamageMobj(mo->target, mo, mo, HITDICE(3), false);
        return;
    }

    pmo = P_SpawnMissile(MT_MNTRFX1, mo, mo->target);
    if(pmo)
    {
        momZ = pmo->mom[MZ];
        angle = pmo->angle;
        P_SpawnMissileAngle(MT_MNTRFX1, mo, angle - (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, mo, angle + (ANG45 / 8), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, mo, angle - (ANG45 / 16), momZ);
        P_SpawnMissileAngle(MT_MNTRFX1, mo, angle + (ANG45 / 16), momZ);
    }
}

/**
 * Minotaur: Floor fire attack.
 */
void C_DECL A_MinotaurAtk3(mobj_t *actor)
{
    mobj_t         *mo;
    player_t       *player;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3), false);
        if((player = actor->target->player) != NULL)
        {   // Squish the player.
            player->viewHeightDelta = -16;
        }
    }
    else
    {
        mo = P_SpawnMissile(MT_MNTRFX2, actor, actor->target);
        if(mo != NULL)
        {
            S_StartSound(SFX_MAULATOR_HAMMER_HIT, mo);
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

void C_DECL A_Scream(mobj_t* actor)
{
    int             sound;

    S_StopSound(0, actor);
    if(actor->player)
    {
        if(actor->player->morphTics)
        {
            S_StartSound(actor->info->deathSound, actor);
        }
        else
        {
            // Handle the different player death screams.
            if(actor->mom[MZ] <= -39)
            {   // Falling splat.
                sound = SFX_PLAYER_FALLING_SPLAT;
            }
            else if(actor->health > -50)
            {   // Normal death sound.
                //// \todo pull these from the class def.
                switch(actor->player->class_)
                {
                case PCLASS_FIGHTER:
                    sound = SFX_PLAYER_FIGHTER_NORMAL_DEATH;
                    break;

                case PCLASS_CLERIC:
                    sound = SFX_PLAYER_CLERIC_NORMAL_DEATH;
                    break;

                case PCLASS_MAGE:
                    sound = SFX_PLAYER_MAGE_NORMAL_DEATH;
                    break;

                default:
                    sound = SFX_NONE;
                    break;
                }
            }
            else if(actor->health > -100)
            {   // Crazy death sound.
                //// \todo pull these from the class def.
                switch(actor->player->class_)
                {
                case PCLASS_FIGHTER:
                    sound = SFX_PLAYER_FIGHTER_CRAZY_DEATH;
                    break;

                case PCLASS_CLERIC:
                    sound = SFX_PLAYER_CLERIC_CRAZY_DEATH;
                    break;

                case PCLASS_MAGE:
                    sound = SFX_PLAYER_MAGE_CRAZY_DEATH;
                    break;

                default:
                    sound = SFX_NONE;
                    break;
                }
            }
            else
            {   // Extreme death sound.
                //// \todo pull these from the class def.
                switch(actor->player->class_)
                {
                case PCLASS_FIGHTER:
                    sound = SFX_PLAYER_FIGHTER_EXTREME1_DEATH;
                    break;

                case PCLASS_CLERIC:
                    sound = SFX_PLAYER_CLERIC_EXTREME1_DEATH;
                    break;

                case PCLASS_MAGE:
                    sound = SFX_PLAYER_MAGE_EXTREME1_DEATH;
                    break;

                default:
                    sound = SFX_NONE;
                    break;
                }

                sound += P_Random() % 3; // Three different extreme deaths.
            }

            S_StartSound(sound, actor);
        }
    }
    else
    {
        S_StartSound(actor->info->deathSound, actor);
    }
}

void C_DECL A_NoBlocking(mobj_t* actor)
{
    actor->flags &= ~MF_SOLID;
}

void C_DECL A_Explode(mobj_t* actor)
{
    int damage;
    coord_t distance;
    dd_bool damageSelf;

    damage = 128;
    distance = 128;
    damageSelf = true;
    switch(actor->type)
    {
    case MT_FIREBOMB: // Time Bombs.
        actor->origin[VZ] += 32;
        actor->flags &= ~MF_SHADOW;
        break;

    case MT_MNTRFX2: // Minotaur floor fire.
        damage = 24;
        break;

    case MT_BISHOP: // Bishop radius death.
        damage = 25 + (P_Random() & 15);
        break;

    case MT_HAMMER_MISSILE: // Fighter Hammer.
        damage = 128;
        damageSelf = false;
        break;

    case MT_FSWORD_MISSILE: // Fighter Runesword.
        damage = 64;
        damageSelf = false;
        break;

    case MT_CIRCLEFLAME: // Cleric Flame secondary flames.
        damage = 20;
        damageSelf = false;
        break;

    case MT_SORCBALL1: // Sorcerer balls.
    case MT_SORCBALL2:
    case MT_SORCBALL3:
        distance = 255;
        damage = 255;
        actor->args[0] = 1; // Don't play bounce.
        break;

    case MT_SORCFX1: // Sorcerer spell 1.
        damage = 30;
        break;

    case MT_SORCFX4: // Sorcerer spell 4.
        damage = 20;
        break;

    case MT_TREEDESTRUCTIBLE:
        damage = 10;
        break;

    case MT_DRAGON_FX2:
        damage = 80;
        damageSelf = false;
        break;

    case MT_MSTAFF_FX:
        damage = 64;
        distance = 192;
        damageSelf = false;
        break;

    case MT_MSTAFF_FX2:
        damage = 80;
        distance = 192;
        damageSelf = false;
        break;

    case MT_POISONCLOUD:
        damage = 4;
        distance = 40;
        break;

    case MT_ZXMAS_TREE:
    case MT_ZSHRUB2:
        damage = 30;
        distance = 64;
        break;

    default:
        break;
    }

    P_RadiusAttack(actor, actor->target, damage, distance, damageSelf);
    if(actor->origin[VZ] <= actor->floorZ + distance &&
       actor->type != MT_POISONCLOUD)
    {
        P_HitFloor(actor);
    }
}

static int massacreMobj(thinker_t* th, void* context)
{
    int*                count = (int*) context;
    mobj_t*             mo = (mobj_t *) th;

    if(!mo->player && mo->type == MT_WRAITHB)
    {
        // Get rid of buried Wraiths.
        P_MobjRemove(mo, true);
        (*count)++;
    }
    else if(!mo->player && sentient(mo) && (mo->flags & (MF_SHOOTABLE | MF_COUNTKILL)))
    {
        mo->flags2 &= ~(MF2_NONSHOOTABLE + MF2_INVULNERABLE);
        mo->flags |= MF_SHOOTABLE;
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

    // Only massacre when actually in a map.
    if(G_GameState() == GS_MAP)
    {
        Thinker_Iterate(P_MobjThinker, massacreMobj, &count);
    }

    return count;
}

void C_DECL A_SkullPop(mobj_t *actor)
{
    mobj_t*             mo;
    player_t*           plr;

    if(!actor->player)
        return;

    actor->flags &= ~MF_SOLID;

    if((mo = P_SpawnMobjXYZ(MT_BLOODYSKULL, actor->origin[VX],
                           actor->origin[VY], actor->origin[VZ] + 48,
                           actor->angle, 0)))
    {
        mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
        mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 6);

        // Attach player mobj to bloody skull.
        plr = actor->player;
        actor->player = NULL;
        actor->dPlayer = NULL;
        actor->special1 = plr->class_;
        mo->player = plr;
        mo->dPlayer = plr->plr;
        mo->health = actor->health;
        plr->plr->mo = mo;
        plr->plr->lookDir = 0;
        plr->damageCount = 32;
    }
}

void C_DECL A_CheckSkullFloor(mobj_t *actor)
{
    if(actor->origin[VZ] <= actor->floorZ)
    {
        P_MobjChangeState(actor, S_BLOODYSKULLX1);
        S_StartSound(SFX_DRIP, actor);
    }
}

void C_DECL A_CheckSkullDone(mobj_t *actor)
{
    if(actor->special2 == 666)
    {
        P_MobjChangeState(actor, S_BLOODYSKULLX2);
    }
}

void C_DECL A_CheckBurnGone(mobj_t *actor)
{
    if(actor->special2 == 666)
    {
        P_MobjChangeState(actor, S_PLAY_FDTH20);
    }
}

void C_DECL A_FreeTargMobj(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    mo->origin[VZ] = mo->ceilingZ + 4;

    mo->flags &=
        ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_SOLID | MF_COUNTKILL);
    mo->flags |= MF_CORPSE | MF_DROPOFF | MF_NOGRAVITY;
    mo->flags2 &= ~(MF2_PASSMOBJ | MF2_LOGRAV);
    mo->flags2 |= MF2_DONTDRAW;
    mo->player = NULL;
    mo->dPlayer = NULL;
    mo->health = -1000; // Don't resurrect.
}

void P_InitCorpseQueue(void)
{
    corpseQueueSlot = 0;
    memset(corpseQueue, 0, sizeof(mobj_t *) * CORPSEQUEUESIZE);
}

void P_RemoveCorpseInQueue(mobj_t* mo)
{
    int slot;

    if(!mo) return;

    for(slot = 0; slot < CORPSEQUEUESIZE; ++slot)
    {
        if(corpseQueue[slot] == mo)
        {
            corpseQueue[slot] = NULL;
            break;
        }
    }
}

void P_AddCorpseToQueue(mobj_t* mo)
{
    if(!mo) return;

    /// @todo fixme: Shouldn't we ensure its not already queued?
    //P_RemoveCorpseInQueue(mo);

    if(corpseQueueSlot >= CORPSEQUEUESIZE)
    {
        // Too many corpses - remove an old one
        mobj_t* corpse = corpseQueue[corpseQueueSlot % CORPSEQUEUESIZE];
        if(corpse)
        {
            P_MobjRemove(corpse, false);
        }
    }
    corpseQueue[corpseQueueSlot % CORPSEQUEUESIZE] = mo;
    corpseQueueSlot++;
}

/**
 * Throw another corpse on the queue.
 */
void C_DECL A_QueueCorpse(mobj_t* actor)
{
    P_AddCorpseToQueue(actor);
}

/**
 * Remove a mobj from the queue (for resurrection).
 */
#if 0
void C_DECL A_DeQueueCorpse(mobj_t* actor)
{
    P_RemoveCorpseInQueue(actor);
}
#endif

void C_DECL A_AddPlayerCorpse(mobj_t *actor)
{
    if(bodyqueslot >= BODYQUESIZE)
    {
        // Too many player corpses - remove an old one.
        P_MobjRemove(bodyque[bodyqueslot % BODYQUESIZE], true);
    }
    bodyque[bodyqueslot % BODYQUESIZE] = actor;
    bodyqueslot++;
}

void C_DECL A_SerpentUnHide(mobj_t* actor)
{
    actor->flags2 &= ~MF2_DONTDRAW;
    actor->floorClip = 24;
}

void C_DECL A_SerpentHide(mobj_t* actor)
{
    actor->flags2 |= MF2_DONTDRAW;
    actor->floorClip = 0;
}

void C_DECL A_SerpentChase(mobj_t *actor)
{
    int delta;
    coord_t oldpos[3];
    world_Material* oldMaterial;
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

    if(gfw_Rule(skill) == SM_NIGHTMARE /*|| gameRules.fast*/)
    {
        // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
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

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        if(P_LookForPlayers(actor, true))
        {   // Got a new target.
            return;
        }
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attackSound)
        {
            S_StartSound(actor->info->attackSound, actor);
        }
        P_MobjChangeState(actor, state);
        return;
    }

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    memcpy(oldpos, actor->origin, sizeof(oldpos));

    oldMaterial = P_GetPtrp(Mobj_Sector(actor), DMU_FLOOR_MATERIAL);
    if(--actor->moveCount < 0 || !P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    if(P_GetPtrp(Mobj_Sector(actor), DMU_FLOOR_MATERIAL) != oldMaterial)
    {
        P_TryMoveXY(actor, oldpos[VX], oldpos[VY]);
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

void C_DECL A_SpeedFade(mobj_t *actor)
{
    actor->flags |= MF_SHADOW;
    actor->flags &= ~MF_ALTSHADOW;

    // Target should have been set (or restored).
    DE_ASSERT(actor->target != NULL);

    if (actor->target)
    {
        actor->sprite = actor->target->sprite;
    }
}

/**
 * Raises the hump above the surface by raising the floorclip level.
 */
void C_DECL A_SerpentRaiseHump(mobj_t *actor)
{
    actor->floorClip -= 4;
}

void C_DECL A_SerpentLowerHump(mobj_t *actor)
{
    actor->floorClip += 4;
}

/**
 * Decide whether to hump up, or if the mobj is a serpentleader, to
 * missile attack.
 */
void C_DECL A_SerpentHumpDecide(mobj_t *actor)
{
    if(actor->type == MT_SERPENTLEADER)
    {
        if(P_Random() > 30)
        {
            return;
        }
        else if(P_Random() < 40)
        {   // Missile attack.
            P_MobjChangeState(actor, S_SERPENT_SURFACE1);
            return;
        }
    }
    else if(P_Random() > 3)
    {
        return;
    }

    if(!P_CheckMeleeRange(actor, false))
    {   // The hump shouldn't occur when within melee range.
        if(actor->type == MT_SERPENTLEADER && P_Random() < 128)
        {
            P_MobjChangeState(actor, S_SERPENT_SURFACE1);
        }
        else
        {
            P_MobjChangeState(actor, S_SERPENT_HUMP1);
            S_StartSound(SFX_SERPENT_ACTIVE, actor);
        }
    }
}

void C_DECL A_SerpentBirthScream(mobj_t *actor)
{
    S_StartSound(SFX_SERPENT_BIRTH, actor);
}

void C_DECL A_SerpentDiveSound(mobj_t *actor)
{
    S_StartSound(SFX_SERPENT_ACTIVE, actor);
}

/**
 * Similar to A_Chase, only has a hardcoded entering of meleestate.
 */
void C_DECL A_SerpentWalk(mobj_t *actor)
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

    if(gfw_Rule(skill) == SM_NIGHTMARE /*|| gameRules.fast*/)
    {
        // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
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

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        if(P_LookForPlayers(actor, true))
        {   // Got a new target.
            return;
        }
        P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if((state = P_GetState(actor->type, SN_MELEE)) != S_NULL &&
       P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attackSound)
        {
            S_StartSound(actor->info->attackSound, actor);
        }
        P_MobjChangeState(actor, S_SERPENT_ATK1);
        return;
    }

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->moveCount < 0 || !P_Move(actor))
    {
        P_NewChaseDir(actor);
    }
}

void C_DECL A_SerpentCheckForAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(actor->type == MT_SERPENTLEADER)
    {
        if(!P_CheckMeleeRange(actor, false))
        {
            P_MobjChangeState(actor, S_SERPENT_ATK1);
            return;
        }
    }

    if(P_CheckMeleeRange(actor, true))
    {
        P_MobjChangeState(actor, S_SERPENT_WALK1);
    }
    else if(P_CheckMeleeRange(actor, false))
    {
        if(P_Random() < 32)
        {
            P_MobjChangeState(actor, S_SERPENT_WALK1);
        }
        else
        {
            P_MobjChangeState(actor, S_SERPENT_ATK1);
        }
    }
}

void C_DECL A_SerpentChooseAttack(mobj_t *actor)
{
    if(!actor->target || P_CheckMeleeRange(actor, false))
        return;

    if(actor->type == MT_SERPENTLEADER)
    {
        P_MobjChangeState(actor, S_SERPENT_MISSILE1);
    }
}

void C_DECL A_SerpentMeleeAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5), false);
        S_StartSound(SFX_SERPENT_MELEEHIT, actor);
    }

    if(P_Random() < 96)
    {
        A_SerpentCheckForAttack(actor);
    }
}

void C_DECL A_SerpentMissileAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    P_SpawnMissile(MT_SERPENTFX, actor, actor->target);
}

void C_DECL A_SerpentHeadPop(mobj_t* actor)
{
    P_SpawnMobjXYZ(MT_SERPENT_HEAD,
                  actor->origin[VX], actor->origin[VY], actor->origin[VZ] + 45,
                  actor->angle, 0);
}

static void spawnSerpentGib(mobjtype_t type, mobj_t* mo)
{
    mobj_t* pmo;
    coord_t pos[3];

    if(!mo) return;

    pos[VX] = mo->origin[VX];
    pos[VY] = mo->origin[VY];
    pos[VZ] = 1;

    pos[VX] += FIX2FLT((P_Random() - 128) << 12);
    pos[VY] += FIX2FLT((P_Random() - 128) << 12);

    if((pmo = P_SpawnMobj((type), pos, P_Random() << 24, MSF_Z_FLOOR)))
    {
        pmo->mom[MX] = FIX2FLT((P_Random() - 128) << 6);
        pmo->mom[MY] = FIX2FLT((P_Random() - 128) << 6);
        pmo->floorClip = 6;
    }
}

void C_DECL A_SerpentSpawnGibs(mobj_t* mo)
{
    // Order is important - P_Randoms!
    spawnSerpentGib(MT_SERPENT_GIB1, mo);
    spawnSerpentGib(MT_SERPENT_GIB2, mo);
    spawnSerpentGib(MT_SERPENT_GIB3, mo);
}

void C_DECL A_FloatGib(mobj_t *actor)
{
    actor->floorClip -= 1;
}

void C_DECL A_SinkGib(mobj_t *actor)
{
    actor->floorClip += 1;
}

void C_DECL A_DelayGib(mobj_t *actor)
{
    actor->tics -= P_Random() >> 2;
}

void C_DECL A_SerpentHeadCheck(mobj_t *actor)
{
    if(actor->origin[VZ] <= actor->floorZ)
    {
        const terraintype_t* tt = P_MobjFloorTerrain(actor);

        if(tt->flags & TTF_NONSOLID)
        {
            P_HitFloor(actor);
            P_MobjChangeState(actor, S_NULL);
        }
        else
        {
            P_MobjChangeState(actor, S_SERPENT_HEAD_X1);
        }
    }
}

void C_DECL A_CentaurAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, P_Random() % 7 + 3, false);
    }
}

void C_DECL A_CentaurAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    P_SpawnMissile(MT_CENTAUR_FX, actor, actor->target);
    S_StartSound(SFX_CENTAURLEADER_ATTACK, actor);
}

static void spawnCentaurStuff(mobjtype_t type, angle_t angle, mobj_t* mo)
{
    mobj_t*             pmo;
    unsigned int        an;
    byte                momRand[3];

    if((pmo = P_SpawnMobjXYZ(type, mo->origin[VX], mo->origin[VY], mo->origin[VZ] + 45,
                            angle, 0)))
    {
        an = angle >> ANGLETOFINESHIFT;

        /* Order of randoms is important! */
        momRand[MZ] = P_Random();
        momRand[MX] = P_Random();
        momRand[MY] = P_Random();

        pmo->mom[MX] = (FIX2FLT((momRand[MX] - 128) << 11) + 1) *
                      FIX2FLT(finecosine[an]);
        pmo->mom[MY] = (FIX2FLT((momRand[MY] - 128) << 11) + 1) *
                     FIX2FLT(finesine[an]);
        pmo->mom[MZ] = 8 + FIX2FLT(momRand[MZ] << 10);
        pmo->target = mo;
    }
}

/**
 * Spawn shield/sword sprites when the centaur pulps.
 */
void C_DECL A_CentaurDropStuff(mobj_t* mo)
{
    // Order is important P_Randoms!
    spawnCentaurStuff(MT_CENTAUR_SHIELD, mo->angle + ANG90, mo);
    spawnCentaurStuff(MT_CENTAUR_SWORD, mo->angle - ANG90, mo);
}

void C_DECL A_CentaurDefend(mobj_t* actor)
{
    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor, false) && P_Random() < 32)
    {
        A_UnSetInvulnerable(actor);
        P_MobjChangeState(actor, P_GetState(actor->type, SN_MELEE));
    }
}

void C_DECL A_BishopAttack(mobj_t* actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attackSound, actor);
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4), false);
        return;
    }
    actor->special1 = (P_Random() & 3) + 5;

    if(IS_NETWORK_SERVER && actor->target)
    {
        /// @todo fixme: Do not assume that this action has been triggered by the named
        /// state because this breaks mod compatibility. Why are we communicating a state
        /// change here in a way that is incompatible with the rest of the playsim? -ds
        NetSv_SendLocalMobjState(actor, "BISHOP_ATK5");
    }
}

/**
 * Spawns one of a string of bishop missiles.
 */
void C_DECL A_BishopAttack2(mobj_t* actor)
{
    mobj_t*             mo;

    if(!actor->target || !actor->special1)
    {
        if(IS_CLIENT)
        {
            // End the local action mode.
            ClMobj_EnableLocalActions(actor, false);
        }
        actor->special1 = 0;
        P_MobjChangeState(actor, S_BISHOP_WALK1);
        return;
    }

    mo = P_SpawnMissile(MT_BISH_FX, actor, actor->target);
    if(mo)
    {
        mo->tracer = actor->target;
        mo->special2 = 16; // High word == x/y, Low word == z.
    }
    actor->special1--;
}

void C_DECL A_BishopMissileWeave(mobj_t* actor)
{
    coord_t pos[3];
    uint weaveXY, weaveZ;
    uint an;

    // Unpack the weave vector.
    weaveXY = actor->special2 >> 16;
    weaveZ = actor->special2 & 0xFFFF;
    an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;

    pos[VX] = actor->origin[VX];
    pos[VY] = actor->origin[VY];
    pos[VZ] = actor->origin[VZ];

    pos[VX] -= FIX2FLT(finecosine[an]) * (FLOATBOBOFFSET(weaveXY) * 2);
    pos[VY] -= FIX2FLT(finesine[an]) * (FLOATBOBOFFSET(weaveXY) * 2);
    pos[VZ] -= FLOATBOBOFFSET(weaveZ);

    weaveXY = (weaveXY + 2) & 63;
    weaveZ = (weaveZ + 2) & 63;

    pos[VX] += FIX2FLT(finecosine[an]) * (FLOATBOBOFFSET(weaveXY) * 2);
    pos[VY] += FIX2FLT(finesine[an]) * (FLOATBOBOFFSET(weaveXY) * 2);
    pos[VZ] += FLOATBOBOFFSET(weaveZ);

    P_TryMoveXY(actor, pos[VX], pos[VY]);

    // P_TryMoveXY won't have set the z compontent so do it manually.
    actor->origin[VZ] = pos[VZ];

    // Store the weave angle.
    actor->special2 = weaveZ + (weaveXY << 16);
}

void C_DECL A_BishopMissileSeek(mobj_t *actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 2, ANGLE_1 * 3);
}

void C_DECL A_BishopDecide(mobj_t *actor)
{
    if(P_Random() < 220)
    {
        return;
    }
    else
    {
        P_MobjChangeState(actor, S_BISHOP_BLUR1);
    }
}

void C_DECL A_BishopDoBlur(mobj_t *mo)
{
    mo->special1 = (P_Random() & 3) + 3; // Random number of blurs.
    if(P_Random() < 120)
    {
        P_ThrustMobj(mo, mo->angle + ANG90, 11);
    }
    else if(P_Random() > 125)
    {
        P_ThrustMobj(mo, mo->angle - ANG90, 11);
    }
    else
    {   // Thrust forward
        P_ThrustMobj(mo, mo->angle, 11);
    }
    S_StartSound(SFX_BISHOP_BLUR, mo);
}

void C_DECL A_BishopSpawnBlur(mobj_t *mo)
{
    if(!--mo->special1)
    {
        mo->mom[MX] = mo->mom[MY] = 0;
        if(P_Random() > 96)
        {
            P_MobjChangeState(mo, S_BISHOP_WALK1);
        }
        else
        {
            P_MobjChangeState(mo, S_BISHOP_ATK1);
        }
    }

    P_SpawnMobj(MT_BISHOPBLUR, mo->origin, mo->angle, 0);
}

void C_DECL A_BishopChase(mobj_t* mo)
{
    mo->origin[VZ] -= FLOATBOBOFFSET(mo->special2) / 2;
    mo->special2 = (mo->special2 + 4) & 63;
    mo->origin[VZ] += FLOATBOBOFFSET(mo->special2) / 2;
}

void C_DECL A_BishopPuff(mobj_t* mo)
{
    mobj_t*             pmo;

    if((pmo = P_SpawnMobjXYZ(MT_BISHOP_PUFF,
                            mo->origin[VX], mo->origin[VY], mo->origin[VZ] + 40,
                            P_Random() << 24, 0)))
    {
        pmo->mom[MZ] = 1.0f / 2;
    }
}

void C_DECL A_BishopPainBlur(mobj_t* actor)
{
    coord_t pos[3];

    if(P_Random() < 64)
    {
        P_MobjChangeState(actor, S_BISHOP_BLUR1);
        return;
    }

    pos[VX] = actor->origin[VX];
    pos[VY] = actor->origin[VY];
    pos[VZ] = actor->origin[VZ];

    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 11);

    P_SpawnMobj(MT_BISHOPPAINBLUR, pos, actor->angle, 0);
}

static void dragonSeek(mobj_t* actor, angle_t thresh, angle_t turnMax)
{
    angle_t delta, bestAngle, angleToSpot, angleToTarget;
    int i, search, dir, bestArg;
    mobj_t* target, *mo;
    coord_t dist;
    uint an;

    target = actor->tracer;
    if(!target) return;

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta /= 2;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir)
    {
        // Turn clockwise.
        actor->angle += delta;
    }
    else
    {
        // Turn counter clockwise.
        actor->angle -= delta;
    }

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[an]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[an]);

    dist = M_ApproxDistance(target->origin[VX] - actor->origin[VX],
                            target->origin[VY] - actor->origin[VY])
         / actor->info->speed;

    if(actor->origin[VZ] + actor->height < target->origin[VZ] ||
       target->origin[VZ] + target->height < actor->origin[VZ])
    {
        if(dist < 1) dist = 1;
        actor->mom[MZ] = (target->origin[VZ] - actor->origin[VZ]) / dist;
    }

    if((target->flags & MF_SHOOTABLE) && P_Random() < 64)
    {
        // Attack the destination mobj if it's attackable.
        mobj_t* oldTarget;

        if(abs((int32_t)(actor->angle - M_PointToAngle2(actor->origin, target->origin))) < ANGLE_45 / 2)
        {
            oldTarget = actor->target;
            actor->target = target;

            if(P_CheckMeleeRange(actor, false))
            {
                P_DamageMobj(actor->target, actor, actor, HITDICE(10), false);
                S_StartSound(SFX_DRAGON_ATTACK, actor);
            }
            else if(P_Random() < 128 && P_CheckMissileRange(actor))
            {
                P_SpawnMissile(MT_DRAGON_FX, actor, target);
                S_StartSound(SFX_DRAGON_ATTACK, actor);
            }

            actor->target = oldTarget;
        }
    }

    // Have we reached the target? (or it's dead)
    if(dist < 4 || target->health <= 0)
    {
        // Hit the target thing.
        if(dist < 4 && actor->target && P_Random() < 200)
        {
            bestArg = -1;
            bestAngle = ANGLE_MAX;
            angleToTarget = M_PointToAngle2(actor->origin, actor->target->origin);

            for(i = 0; i < 5; ++i)
            {
                if(!target->args[i]) continue;

                search = -1;
                mo = P_FindMobjFromTID(target->args[i], &search);
                angleToSpot = M_PointToAngle2(actor->origin, mo->origin);

                if(abs((int32_t)(angleToSpot - angleToTarget)) < (int32_t) bestAngle)
                {
                    bestAngle = abs((int32_t)(angleToSpot - angleToTarget));
                    bestArg = i;
                }
            }

            if(bestArg != -1)
            {
                search = -1;
                actor->tracer = P_FindMobjFromTID(target->args[bestArg], &search);
            }
        }
        else
        {
            // Find another flight destination.
            do {
                i = (P_Random() >> 2) % 5;
            } while(!target->args[i]);

            search = -1;
            actor->tracer = P_FindMobjFromTID(target->args[i], &search);
        }
    }
}

void C_DECL A_DragonInitFlight(mobj_t* actor)
{
    int             search;

    search = -1;
    do
    {   // find the first tid identical to the dragon's tid
        actor->tracer = P_FindMobjFromTID(actor->tid, &search);
        if(search == -1)
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_SPAWN));
            return;
        }
    } while(actor->tracer == actor);

    P_MobjRemoveFromTIDList(actor);
}

void C_DECL A_DragonFlight(mobj_t* actor)
{
    angle_t angle;

    dragonSeek(actor, 4 * ANGLE_1, 8 * ANGLE_1);
    if(actor->target)
    {
        if(!(actor->target->flags & MF_SHOOTABLE))
        {
            // Target died.
            actor->target = NULL;
            return;
        }

        angle = M_PointToAngle2(actor->origin, actor->target->origin);

        if(abs((int32_t)(actor->angle - angle)) < ANGLE_45 / 2 &&
           P_CheckMeleeRange(actor, false))
        {
            P_DamageMobj(actor->target, actor, actor, HITDICE(8), false);
            S_StartSound(SFX_DRAGON_ATTACK, actor);
        }
        else if(abs((int32_t)(actor->angle - angle)) <= ANGLE_1 * 20)
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_MISSILE));
            S_StartSound(SFX_DRAGON_ATTACK, actor);
        }
    }
    else
    {
        P_LookForPlayers(actor, true);
    }
}

void C_DECL A_DragonFlap(mobj_t* actor)
{
    A_DragonFlight(actor);
    if(P_Random() < 240)
    {
        S_StartSound(SFX_DRAGON_WINGFLAP, actor);
    }
    else
    {
        S_StartSound(actor->info->activeSound, actor);
    }
}

void C_DECL A_DragonAttack(mobj_t* mo)
{
    P_SpawnMissile(MT_DRAGON_FX, mo, mo->target);
}

void C_DECL A_DragonFX2(mobj_t* mo)
{
    int i, delay;

    delay = 16 + (P_Random() >> 3);
    for(i = 1 + (P_Random() & 3); i; i--)
    {
        mobj_t* pmo;
        coord_t pos[3];

        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 14);
        pos[VY] += FIX2FLT((P_Random() - 128) << 14);
        pos[VZ] += FIX2FLT((P_Random() - 128) << 12);

        if((pmo = P_SpawnMobj(MT_DRAGON_FX2, pos, P_Random() << 24, 0)))
        {
            pmo->tics = delay + (P_Random() & 3) * i * 2;
            pmo->target = mo->target;
        }
    }
}

void C_DECL A_DragonPain(mobj_t *mo)
{
    A_Pain(mo);
    if(!mo->tracer)
    {   // No destination spot yet.
        P_MobjChangeState(mo, S_DRAGON_INIT);
    }
}

void C_DECL A_DragonCheckCrash(mobj_t *mo)
{
    if(mo->origin[VZ] <= mo->floorZ)
    {
        P_MobjChangeState(mo, S_DRAGON_CRASH1);
    }
}

/**
 * Demon: Melee attack.
 */
void C_DECL A_DemonAttack1(mobj_t *mo)
{
    if(P_CheckMeleeRange(mo, false))
    {
        P_DamageMobj(mo->target, mo, mo, HITDICE(2), false);
    }
}

/**
 * Demon: Missile attack.
 */
void C_DECL A_DemonAttack2(mobj_t* mo)
{
    mobj_t*             pmo;
    int                 fireBall;

    if(mo->type == MT_DEMON)
    {
        fireBall = MT_DEMONFX1;
    }
    else
    {
        fireBall = MT_DEMON2FX1;
    }

    if((pmo = P_SpawnMissile(fireBall, mo, mo->target)))
    {
        pmo->origin[VZ] += 30;
        S_StartSound(SFX_DEMON_MISSILE_FIRE, mo);
    }
}

static mobj_t *spawnDemonChunk(mobjtype_t type, angle_t angle, mobj_t *mo)
{
    mobj_t*             pmo;
    unsigned int        an;

    if((pmo = P_SpawnMobjXYZ(type, mo->origin[VX], mo->origin[VY], mo->origin[VZ] + 45,
                            angle, 0)))
    {
        an = angle >> ANGLETOFINESHIFT;
        pmo->mom[MX] = (FIX2FLT(P_Random() << 10) + 1) *
                     FIX2FLT(finecosine[an]);
        pmo->mom[MY] = (FIX2FLT(P_Random() << 10) + 1) *
                     FIX2FLT(finesine[an]);
        pmo->mom[MZ] = 8;
        pmo->target = mo;
    }

    return pmo;
}

void C_DECL A_DemonDeath(mobj_t *mo)
{
    // Order is important, P_Randoms!
    spawnDemonChunk(MT_DEMONCHUNK1, mo->angle + ANG90, mo);
    spawnDemonChunk(MT_DEMONCHUNK2, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMONCHUNK3, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMONCHUNK4, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMONCHUNK5, mo->angle - ANG90, mo);
}

void C_DECL A_Demon2Death(mobj_t *mo)
{
    // Order is important, P_Randoms!
    spawnDemonChunk(MT_DEMON2CHUNK1, mo->angle + ANG90, mo);
    spawnDemonChunk(MT_DEMON2CHUNK2, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMON2CHUNK3, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMON2CHUNK4, mo->angle - ANG90, mo);
    spawnDemonChunk(MT_DEMON2CHUNK5, mo->angle - ANG90, mo);
}

/**
 * Sink a mobj incrementally into the floor.
 */
dd_bool A_SinkMobj(mobj_t *mo)
{
    if(mo->floorClip < mo->info->height)
    {
        switch(mo->type)
        {
        case MT_THRUSTFLOOR_DOWN:
        case MT_THRUSTFLOOR_UP:
            mo->floorClip += 6;
            break;

        default:
            mo->floorClip += 1;
            break;
        }

        return false;
    }

    return true;
}

/**
 * Raise a mobj incrementally from the floor to.
 */
dd_bool A_RaiseMobj(mobj_t *mo)
{
    dd_bool         done = true;

    // Raise a mobj from the ground.
    if(mo->floorClip > 0)
    {
        switch (mo->type)
        {
        case MT_WRAITHB:
            mo->floorClip -= 2;
            break;

        case MT_THRUSTFLOOR_DOWN:
        case MT_THRUSTFLOOR_UP:
            mo->floorClip -= mo->special2;
            break;

        default:
            mo->floorClip -= 2;
            break;
        }

        if(mo->floorClip <= 0)
        {
            mo->floorClip = 0;
            done = true;
        }
        else
        {
            done = false;
        }
    }

    return done; // Reached target height.
}

/**
 * Wraith Variables
 *
 * special1                Internal index into floatbob.
 * special2
 */

void C_DECL A_WraithInit(mobj_t *mo)
{
    mo->origin[VZ] += 48;
    mo->special1 = 0; // Index into floatbob.
}

void C_DECL A_WraithRaiseInit(mobj_t *mo)
{
    mo->flags2 &= ~MF2_DONTDRAW;
    mo->flags2 &= ~MF2_NONSHOOTABLE;
    mo->flags |= MF_SHOOTABLE | MF_SOLID;
    mo->floorClip = mo->info->height;
}

void C_DECL A_WraithRaise(mobj_t *mo)
{
    if(A_RaiseMobj(mo))
    {
        // Reached it's target height.
        P_MobjChangeState(mo, S_WRAITH_CHASE1);
    }

    P_SpawnDirt(mo, mo->radius);
}

void C_DECL A_WraithMelee(mobj_t* mo)
{
    if(P_CheckMeleeRange(mo, false) && (P_Random() < 220))
    {   // Steal health from target.
        mo->health += P_DamageMobj(mo->target, mo, mo, HITDICE(2), false);
    }
}

void C_DECL A_WraithMissile(mobj_t *mo)
{
    if(P_SpawnMissile(MT_WRAITHFX1, mo, mo->target))
    {
        S_StartSound(SFX_WRAITH_MISSILE_FIRE, mo);
    }
}

/**
 * Wraith: Spawn sparkle tail of missile.
 */
void C_DECL A_WraithFX2(mobj_t* mo)
{
    int                 i;

    for(i = 0; i < 2; ++i)
    {
        uint                an;
        angle_t             angle;
        mobj_t*             pmo;

        if(P_Random() < 128)
        {
            angle = mo->angle + (P_Random() << 22);
        }
        else
        {
            angle = mo->angle - (P_Random() << 22);
        }

        if((pmo = P_SpawnMobj(MT_WRAITHFX2, mo->origin, angle, 0)))
        {
            an = angle >> ANGLETOFINESHIFT;

            pmo->mom[MX] = FIX2FLT((P_Random() << 7) + 1) *
                         FIX2FLT(finecosine[an]);
            pmo->mom[MY] = FIX2FLT((P_Random() << 7) + 1) *
                         FIX2FLT(finesine[an]);
            pmo->mom[MZ] = 0;

            pmo->target = mo;
            pmo->floorClip = 10;
        }
    }
}

/**
 * Wraith: Spawn an FX3 around during attacks.
 */
void C_DECL A_WraithFX3(mobj_t* mo)
{
    int i, numdropped = P_Random() % 15;

    for(i = 0; i < numdropped; ++i)
    {
        mobj_t* pmo;
        coord_t pos[3];

        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 11);
        pos[VY] += FIX2FLT((P_Random() - 128) << 11);
        pos[VZ] += FIX2FLT(P_Random() << 10);

        if((pmo = P_SpawnMobj(MT_WRAITHFX3, pos, P_Random() << 24, 0)))
        {
            pmo->target = mo;
        }
    }
}

/**
 * Wraith: Spawn an FX4 during movement.
 */
void C_DECL A_WraithFX4(mobj_t* mo)
{
    mobj_t* pmo;
    int spawn4, spawn5, chance = P_Random();

    if(chance < 10)
    {
        spawn4 = true;
        spawn5 = false;
    }
    else if(chance < 20)
    {
        spawn4 = false;
        spawn5 = true;
    }
    else if(chance < 25)
    {
        spawn4 = true;
        spawn5 = true;
    }
    else
    {
        spawn4 = false;
        spawn5 = false;
    }

    if(spawn4)
    {
        coord_t pos[3];

        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 12);
        pos[VY] += FIX2FLT((P_Random() - 128) << 12);
        pos[VZ] += FIX2FLT(P_Random() << 10);

        if((pmo = P_SpawnMobj(MT_WRAITHFX4, pos, P_Random() << 24, 0)))
        {
            pmo->target = mo;
        }
    }

    if(spawn5)
    {
        coord_t pos[3];

        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT((P_Random() - 128) << 11);
        pos[VY] += FIX2FLT((P_Random() - 128) << 11);
        pos[VZ] += FIX2FLT(P_Random() << 10);

        if((pmo = P_SpawnMobj(MT_WRAITHFX5, pos, P_Random() << 24, 0)))
        {
            pmo->target = mo;
        }
    }
}

void C_DECL A_WraithLook(mobj_t *actor)
{
    A_Look(actor);
}

void C_DECL A_WraithChase(mobj_t *actor)
{
    int         weaveindex = actor->special1;

    actor->origin[VZ] += FLOATBOBOFFSET(weaveindex);
    actor->special1 = (weaveindex + 2) & 63;

    A_Chase(actor);
    A_WraithFX4(actor);
}

void C_DECL A_EttinAttack(mobj_t *actor)
{
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2), false);
    }
}

void C_DECL A_DropMace(mobj_t *mo)
{
    mobj_t*             pmo;

    if((pmo = P_SpawnMobjXYZ(MT_ETTIN_MACE, mo->origin[VX], mo->origin[VY],
                            mo->origin[VZ] + mo->height /2, mo->angle, 0)))
    {
        pmo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MZ] = 10 + FIX2FLT(P_Random() << 10);
        pmo->target = mo;
    }
}

/**
 * Fire Demon variables.
 *
 * special1         Index into floatbob.
 * special2         whether strafing or not.
 */

void C_DECL A_FiredSpawnRock(mobj_t* mo)
{
    mobj_t* pmo;
    coord_t pos[3];
    int rtype = 0;

    switch(P_Random() % 5)
    {
    case 0: rtype = MT_FIREDEMON_FX1; break;
    case 1: rtype = MT_FIREDEMON_FX2; break;
    case 2: rtype = MT_FIREDEMON_FX3; break;
    case 3: rtype = MT_FIREDEMON_FX4; break;
    case 4: rtype = MT_FIREDEMON_FX5; break;
    }

    pos[VX] = mo->origin[VX];
    pos[VY] = mo->origin[VY];
    pos[VZ] = mo->origin[VZ];

    pos[VX] += FIX2FLT((P_Random() - 128) << 12);
    pos[VY] += FIX2FLT((P_Random() - 128) << 12);
    pos[VZ] += FIX2FLT((P_Random()) << 11);

    if((pmo = P_SpawnMobj(rtype, pos, P_Random() << 24, 0)))
    {
        pmo->mom[MX] = FIX2FLT((P_Random() - 128) << 10);
        pmo->mom[MY] = FIX2FLT((P_Random() - 128) << 10);
        pmo->mom[MZ] = FIX2FLT(P_Random() << 10);
        pmo->special1 = 2; // Number of bounces.
        pmo->target = mo;
    }

    // Initialize fire demon.
    mo->special2 = 0;
    mo->flags &= ~MF_JUSTATTACKED;
}

void C_DECL A_FiredRocks(mobj_t* mo)
{
    A_FiredSpawnRock(mo);
    A_FiredSpawnRock(mo);
    A_FiredSpawnRock(mo);
    A_FiredSpawnRock(mo);
    A_FiredSpawnRock(mo);
}

void C_DECL A_FiredAttack(mobj_t* mo)
{
    mobj_t* pmo = P_SpawnMissile(MT_FIREDEMON_FX6, mo, mo->target);
    if(pmo)
    {
        S_StartSound(SFX_FIRED_ATTACK, mo);
    }
}

void C_DECL A_SmBounce(mobj_t* mo)
{
    // Give some more momentum (x,y,&z).
    mo->origin[VZ] = mo->floorZ + 1;
    mo->mom[MZ] = 2 + FIX2FLT(P_Random() << 10);
    mo->mom[MX] = (coord_t) (P_Random() % 3);
    mo->mom[MY] = (coord_t) (P_Random() % 3);
}

void C_DECL A_FiredChase(mobj_t* actor)
{
#define FIREDEMON_ATTACK_RANGE  (64*8)

    int weaveindex = actor->special1;
    mobj_t* target = actor->target;
    angle_t angle;
    coord_t dist;
    uint an;

    if(actor->reactionTime)
        actor->reactionTime--;
    if(actor->threshold)
        actor->threshold--;

    // Float up and down.
    actor->origin[VZ] += FLOATBOBOFFSET(weaveindex);
    actor->special1 = (weaveindex + 2) & 63;

    // Insure it stays above certain height.
    if(actor->origin[VZ] < actor->floorZ + 64)
    {
        actor->origin[VZ] += 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // Invalid target.
        P_LookForPlayers(actor, true);
        return;
    }

    // Strafe.
    if(actor->special2 > 0)
    {
        actor->special2--;
    }
    else
    {
        actor->special2 = 0;
        actor->mom[MX] = actor->mom[MY] = 0;
        dist = M_ApproxDistance(actor->origin[VX] - target->origin[VX],
                                actor->origin[VY] - target->origin[VY]);
        if(dist < FIREDEMON_ATTACK_RANGE)
        {
            if(P_Random() < 30)
            {
                angle = M_PointToAngle2(actor->origin, target->origin);
                if(P_Random() < 128)
                    angle += ANGLE_90;
                else
                    angle -= ANGLE_90;
                an = angle >>ANGLETOFINESHIFT;

                actor->mom[MX] = 8 * FIX2FLT(finecosine[an]);
                actor->mom[MY] = 8 * FIX2FLT(finesine[an]);
                actor->special2 = 3; // Strafe time.
            }
        }
    }

    FaceMovementDirection(actor);

    // Normal movement.
    if(!actor->special2)
    {
        if(--actor->moveCount < 0 || !P_Move(actor))
        {
            P_NewChaseDir(actor);
        }
    }

    // Do missile attack.
    if(!(actor->flags & MF_JUSTATTACKED))
    {
        if(P_CheckMissileRange(actor) && (P_Random() < 20))
        {
            P_MobjChangeState(actor, P_GetState(actor->type, SN_MISSILE));
            actor->flags |= MF_JUSTATTACKED;
            return;
        }
    }
    else
    {
        actor->flags &= ~MF_JUSTATTACKED;
    }

    // Make active sound.
    if(actor->info->activeSound && P_Random() < 3)
    {
        S_StartSound(actor->info->activeSound, actor);
    }

#undef FIREDEMON_ATTACK_RANGE
}

void C_DECL A_FiredSplotch(mobj_t* actor)
{
    mobj_t*             pmo;

    if((pmo = P_SpawnMobj(MT_FIREDEMON_SPLOTCH1, actor->origin,
                             P_Random() << 24, 0)))
    {
        pmo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MZ] = 3 + FIX2FLT(P_Random() << 10);
    }

    if((pmo = P_SpawnMobj(MT_FIREDEMON_SPLOTCH2, actor->origin,
                             P_Random() << 24, 0)))
    {
        pmo->mom[MX] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MY] = FIX2FLT((P_Random() - 128) << 11);
        pmo->mom[MZ] = 3 + FIX2FLT((P_Random() << 10));
    }
}

void C_DECL A_IceGuyLook(mobj_t* mo)
{
    coord_t dist;
    angle_t angle;
    unsigned int an;

    A_Look(mo);
    if(P_Random() < 64)
    {
        dist = FIX2FLT(((P_Random() - 128) * FLT2FIX(mo->radius)) >> 7);
        angle = mo->angle + ANG90;
        an = angle >> ANGLETOFINESHIFT;

        /**
         * @todo We should not be selecting mobj types by their original
         *        indices! Instead, use a fixed table here.
         */
        P_SpawnMobjXYZ(MT_ICEGUY_WISP1 + (P_Random() & 1),
                       mo->origin[VX] + dist * FIX2FLT(finecosine[an]),
                       mo->origin[VY] + dist * FIX2FLT(finesine[an]),
                       mo->origin[VZ] + 60, angle, 0);
    }
}

void C_DECL A_IceGuyChase(mobj_t* actor)
{
    coord_t dist;
    angle_t angle;
    unsigned int an;
    mobj_t* mo;

    A_Chase(actor);
    if(P_Random() < 128)
    {
        dist = FIX2FLT(((P_Random() - 128) * FLT2FIX(actor->radius)) >> 7);
        angle = actor->angle + ANG90;
        an = angle >> ANGLETOFINESHIFT;

        /**
         * @todo We should not be selecting mobj types by their original
         *        indices! Instead, use a fixed table here.
         */
        if((mo = P_SpawnMobjXYZ(MT_ICEGUY_WISP1 + (P_Random() & 1),
                                actor->origin[VX] + dist * FIX2FLT(finecosine[an]),
                                actor->origin[VY] + dist * FIX2FLT(finesine[an]),
                                actor->origin[VZ] + 60, angle, 0)))
        {
            mo->mom[MX] = actor->mom[MX];
            mo->mom[MY] = actor->mom[MY];
            mo->mom[MZ] = actor->mom[MZ];
            mo->target = actor;
        }
    }
}

void C_DECL A_IceGuyAttack(mobj_t *mob)
{
    mobj_t *target;
    vec3d_t pos, offset;
    uint an;

    target = mob->target;
    if(!target) return;

    // Right FX:
    an = (mob->angle + ANG90) >> ANGLETOFINESHIFT;
    V3d_Set(offset, (mob->radius / 2) * FIX2FLT(finecosine[an]),
                    (mob->radius / 2) * FIX2FLT(finesine  [an]),
                    40 - mob->floorClip);
    V3d_Sum(pos, mob->origin, offset);
    Mobj_LaunchMissile(mob, P_SpawnMobj(MT_ICEGUY_FX, pos, Mobj_AimAtTarget(mob), 0),
                       target->origin, mob->origin);

    // Left FX:
    an = (mob->angle - ANG90) >> ANGLETOFINESHIFT;
    V3d_Set(offset, (mob->radius / 2) * FIX2FLT(finecosine[an]),
                    (mob->radius / 2) * FIX2FLT(finesine  [an]),
                    40 - mob->floorClip);
    V3d_Sum(pos, mob->origin, offset);
    Mobj_LaunchMissile(mob, P_SpawnMobj(MT_ICEGUY_FX, pos, Mobj_AimAtTarget(mob), 0),
                       target->origin, mob->origin);

    S_StartSound(mob->info->attackSound, mob);
}

void C_DECL A_IceGuyMissilePuff(mobj_t *mo)
{
    P_SpawnMobjXYZ(MT_ICEFX_PUFF, mo->origin[VX], mo->origin[VY], mo->origin[VZ] + 2,
                  P_Random() << 24, 0);
}

void C_DECL A_IceGuyDie(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    mo->height *= 2*2;

    A_FreezeDeathChunks(mo);
}

void C_DECL A_IceGuyMissileExplode(mobj_t *mo)
{
    mobj_t         *pmo;
    uint            i;

    for(i = 0; i < 8; ++i)
    {
        pmo = P_SpawnMissileAngle(MT_ICEGUY_FX2, mo, i * ANG45, -0.3f);
        if(pmo)
        {
            pmo->target = mo->target;
        }
    }
}

/**
 * Sorcerer Variables.
 *
 * special1        Angle of ball 1 (all others relative to that).
 * special2        Which ball to stop at in stop mode (MT_???).
 * args[0]         Defense time.
 * args[1]         Number of full rotations since stopping mode.
 * args[2]         Target orbit speed for acceleration/deceleration.
 * args[3]         Movement mode (see SORC_ macros).
 * args[4]         Current ball orbit speed.
 */

/**
 * Sorcerer Ball Variables.
 *
 * special1        Previous angle of ball (for woosh).
 * special2        Countdown of rapid fire (FX4).
 * args[0]         If set, don't play the bounce sound when bouncing.
 */

/**
 * Spawn spinning balls above head - actor is sorcerer.
 */
void C_DECL A_SorcSpinBalls(mobj_t* mo)
{
    mobj_t* pmo;
    coord_t z;
    angle_t angle;

    A_SlowBalls(mo);
    mo->args[0] = 0; // Currently no defense.
    mo->args[3] = SORC_NORMAL;
    mo->args[4] = SORCBALL_INITIAL_SPEED; // Initial orbit speed.
    angle = ANG45 / 45;
    mo->special1 = angle;

    z = mo->origin[VZ] - mo->floorClip + mo->info->height;

    if((pmo = P_SpawnMobjXYZ(MT_SORCBALL1, mo->origin[VX], mo->origin[VY], z, angle, 0)))
    {
        pmo->target = mo;
        pmo->special2 = SORCFX4_RAPIDFIRE_TIME;
    }

    if((pmo = P_SpawnMobjXYZ(MT_SORCBALL2, mo->origin[VX], mo->origin[VY], z, angle, 0)))
    {
        pmo->target = mo;
    }

    if((pmo = P_SpawnMobjXYZ(MT_SORCBALL3, mo->origin[VX], mo->origin[VY], z, angle, 0)))
    {
        pmo->target = mo;
    }
}

void C_DECL A_SorcBallOrbit(mobj_t* actor)
{
    uint an;
    angle_t angle = 0, baseangle;
    int mode = actor->target->args[3];
    mobj_t* parent = actor->target;
    coord_t dist = parent->radius - actor->radius * 2;
    angle_t prevangle = (angle_t) actor->special1;
    statenum_t state;

    if((state = P_GetState(actor->type, SN_PAIN)) != S_NULL &&
       actor->target->health <= 0)
        P_MobjChangeState(actor, state);

    baseangle = (angle_t) parent->special1;
    switch(actor->type)
    {
    case MT_SORCBALL1:
        angle = baseangle + BALL1_ANGLEOFFSET;
        break;

    case MT_SORCBALL2:
        angle = baseangle + BALL2_ANGLEOFFSET;
        break;

    case MT_SORCBALL3:
        angle = baseangle + BALL3_ANGLEOFFSET;
        break;

    default:
        Con_Error("Corrupted sorcerer");
        break;
    }

    actor->angle = angle;
    an = angle >> ANGLETOFINESHIFT;

    switch(mode)
    {
    case SORC_NORMAL: // Balls rotating normally.
        A_SorcUpdateBallAngle(actor);
        break;

    case SORC_DECELERATE: // Balls decelerating.
        A_DecelBalls(actor);
        A_SorcUpdateBallAngle(actor);
        break;

    case SORC_ACCELERATE: // Balls accelerating.
        A_AccelBalls(actor);
        A_SorcUpdateBallAngle(actor);
        break;

    case SORC_STOPPING: // Balls stopping.
        if((parent->special2 == actor->type) &&
           (parent->args[1] > SORCBALL_SPEED_ROTATIONS) &&
           (abs((int)(an - (parent->angle >> ANGLETOFINESHIFT))) < (30 << 5)))
        {
            // Can stop now.
            actor->target->args[3] = SORC_FIRESPELL;
            actor->target->args[4] = 0;
            // Set angle so ball angle == sorcerer angle.
            switch(actor->type)
            {
            case MT_SORCBALL1:
                parent->special1 = (int) (parent->angle - BALL1_ANGLEOFFSET);
                break;

            case MT_SORCBALL2:
                parent->special1 = (int) (parent->angle - BALL2_ANGLEOFFSET);
                break;

            case MT_SORCBALL3:
                parent->special1 = (int) (parent->angle - BALL3_ANGLEOFFSET);
                break;

            default:
                break;
            }
        }
        else
        {
            A_SorcUpdateBallAngle(actor);
        }
        break;

    case SORC_FIRESPELL: // Casting spell.
        if(parent->special2 == actor->type)
        {
            // Put sorcerer into special throw spell anim.
            if(parent->health > 0)
                P_MobjChangeStateNoAction(parent, S_SORC_ATTACK1);

            if(actor->type == MT_SORCBALL1 && P_Random() < 200)
            {
                S_StartSound(SFX_SORCERER_SPELLCAST, NULL);
                actor->special2 = SORCFX4_RAPIDFIRE_TIME;
                actor->args[4] = 128;
                parent->args[3] = SORC_FIRING_SPELL;
            }
            else
            {
                A_CastSorcererSpell(actor);
                parent->args[3] = SORC_STOPPED;
            }
        }
        break;

    case SORC_FIRING_SPELL:
        if(parent->special2 == actor->type)
        {
            if(actor->special2-- <= 0)
            {
                // Done rapid firing.
                parent->args[3] = SORC_STOPPED;
                // Back to orbit balls.
                if(parent->health > 0)
                    P_MobjChangeStateNoAction(parent, S_SORC_ATTACK4);
            }
            else
            {
                // Do rapid fire spell.
                A_SorcOffense2(actor);
            }
        }
        break;

    case SORC_STOPPED: // Balls stopped.
    default:
        break;
    }

    if((angle < prevangle) && (parent->args[4] == SORCBALL_TERMINAL_SPEED))
    {
        parent->args[1]++; // Bump rotation counter.
        // Completed full rotation - make woosh sound.
        S_StartSound(SFX_SORCERER_BALLWOOSH, actor);
    }
    actor->special1 = angle; // Set previous angle.

    P_MobjUnlink(actor);

    actor->origin[VX] = parent->origin[VX];
    actor->origin[VY] = parent->origin[VY];
    actor->origin[VZ] = parent->origin[VZ];

    actor->origin[VX] += dist * FIX2FLT(finecosine[an]);
    actor->origin[VY] += dist * FIX2FLT(finesine[an]);

    actor->origin[VZ] += parent->info->height;
    actor->origin[VZ] -= parent->floorClip;

    P_MobjLink(actor);
}

/**
 * Set balls to speed mode - actor is sorcerer.
 */
void C_DECL A_SpeedBalls(mobj_t *actor)
{
    actor->args[3] = SORC_ACCELERATE; // Speed mode.
    actor->args[2] = SORCBALL_TERMINAL_SPEED; // Target speed.
}

/**
 * Set balls to slow mode - actor is sorcerer.
 */
void C_DECL A_SlowBalls(mobj_t *actor)
{
    actor->args[3] = SORC_DECELERATE; // Slow mode.
    actor->args[2] = SORCBALL_INITIAL_SPEED; // Target speed.
}

/**
 * Instant stop when rotation gets to ball in special2 actor is sorcerer.
 */
void C_DECL A_StopBalls(mobj_t *actor)
{
    int         chance = P_Random();

    actor->args[3] = SORC_STOPPING; // Stopping mode.
    actor->args[1] = 0; // Reset rotation counter.

    if((actor->args[0] <= 0) && (chance < 200))
    {
        actor->special2 = MT_SORCBALL2; // Blue.
    }
    else if((actor->health < (actor->info->spawnHealth >> 1)) &&
            (chance < 200))
    {
        actor->special2 = MT_SORCBALL3; // Green.
    }
    else
    {
        actor->special2 = MT_SORCBALL1; // Yellow.
    }
}

/**
 * Increase ball orbit speed - actor is ball.
 */
void C_DECL A_AccelBalls(mobj_t *actor)
{
    mobj_t     *sorc = actor->target;

    if(sorc->args[4] < sorc->args[2])
    {
        sorc->args[4]++;
    }
    else
    {
        sorc->args[3] = SORC_NORMAL;
        if(sorc->args[4] >= SORCBALL_TERMINAL_SPEED)
        {
            // Reached terminal velocity - stop balls.
            A_StopBalls(sorc);
        }
    }
}

/**
 * Decrease ball orbit speed - actor is ball.
 */
void C_DECL A_DecelBalls(mobj_t *actor)
{
    mobj_t     *sorc = actor->target;

    if(sorc->args[4] > sorc->args[2])
    {
        sorc->args[4]--;
    }
    else
    {
        sorc->args[3] = SORC_NORMAL;
    }
}

/**
 * Update angle if first ball - actor is ball.
 */
void C_DECL A_SorcUpdateBallAngle(mobj_t *actor)
{
    if(actor->type == MT_SORCBALL1)
    {
        actor->target->special1 += ANGLE_1 * actor->target->args[4];
    }
}

/**
 * Actor is ball.
 */
void C_DECL A_CastSorcererSpell(mobj_t* mo)
{
    coord_t z;
    mobj_t* pmo;
    int spell = mo->type;
    angle_t ang1, ang2;
    mobj_t* parent = mo->target;

    S_StartSound(SFX_SORCERER_SPELLCAST, NULL);

    // Put sorcerer into throw spell animation.
    if(parent->health > 0)
        P_MobjChangeStateNoAction(parent, S_SORC_ATTACK4);

    switch(spell)
    {
    case MT_SORCBALL1: // Offensive.
        A_SorcOffense1(mo);
        break;

    case MT_SORCBALL2: // Defensive.
        z = parent->origin[VZ] - parent->floorClip + SORC_DEFENSE_HEIGHT;
        if((pmo = P_SpawnMobjXYZ(MT_SORCFX2, mo->origin[VX], mo->origin[VY], z,
                                             mo->angle, 0)))
            pmo->target = parent;

        parent->flags2 |= MF2_REFLECTIVE | MF2_INVULNERABLE;
        parent->args[0] = SORC_DEFENSE_TIME;
        break;

    case MT_SORCBALL3: // Reinforcements.
        ang1 = mo->angle - ANGLE_45;
        ang2 = mo->angle + ANGLE_45;
        if(mo->health < (mo->info->spawnHealth / 3))
        {   // Spawn 2 at a time.
            if((pmo = P_SpawnMissileAngle(MT_SORCFX3, parent, ang1, 4)))
                pmo->target = parent;

            if((pmo = P_SpawnMissileAngle(MT_SORCFX3, parent, ang2, 4)))
                pmo->target = parent;
        }
        else
        {
            if(P_Random() < 128)
                ang1 = ang2;

            if((pmo = P_SpawnMissileAngle(MT_SORCFX3, parent, ang1, 4)))
                pmo->target = parent;
        }
        break;

    default:
        break;
    }
}

/**
 * Actor is ball.
 */
void C_DECL A_SorcOffense1(mobj_t *mo)
{
    mobj_t     *pmo;
    angle_t     ang1, ang2;
    mobj_t     *parent = mo->target;

    ang1 = mo->angle + ANGLE_1 * 70;
    ang2 = mo->angle - ANGLE_1 * 70;
    pmo = P_SpawnMissileAngle(MT_SORCFX1, parent, ang1, 0);
    if(pmo)
    {
        pmo->target = parent;
        pmo->tracer = parent->target;
        pmo->args[4] = BOUNCE_TIME_UNIT;
        pmo->args[3] = 15; // Bounce time in seconds.
    }

    pmo = P_SpawnMissileAngle(MT_SORCFX1, parent, ang2, 0);
    if(pmo)
    {
        pmo->target = parent;
        pmo->tracer = parent->target;
        pmo->args[4] = BOUNCE_TIME_UNIT;
        pmo->args[3] = 15; // Bounce time in seconds.
    }
}

/**
 * Actor is ball.
 */
void C_DECL A_SorcOffense2(mobj_t* mo)
{
    angle_t ang1;
    mobj_t* pmo;
    int delta, index;
    mobj_t* parent = mo->target;
    mobj_t* target = parent->target;
    int dist;

    index = mo->args[4] << 5;
    mo->args[4] += 15;
    delta = (finesine[index]) * SORCFX4_SPREAD_ANGLE;
    delta = (delta >> FRACBITS) * ANGLE_1;
    ang1 = mo->angle + delta;

    pmo = P_SpawnMissileAngle(MT_SORCFX4, parent, ang1, 0);
    if (pmo && target)
    {
        pmo->special2 = TICSPERSEC * 5 / 2;
        dist = M_ApproxDistance(target->origin[VX] - pmo->origin[VX],
                                target->origin[VY] - pmo->origin[VY]);
        dist /= pmo->info->speed;
        if(dist < 1)
            dist = 1;
        pmo->mom[MZ] = (target->origin[VZ] - pmo->origin[VZ]) / dist;
    }
}

/**
 * Resume ball spinning.
 */
void C_DECL A_SorcBossAttack(mobj_t *actor)
{
    actor->args[3] = SORC_ACCELERATE;
    actor->args[2] = SORCBALL_INITIAL_SPEED;
}

/**
 * Spell cast magic fizzle.
 */
void C_DECL A_SpawnFizzle(mobj_t* mo)
{
    const float speed = mo->info->speed;
    const coord_t dist = 5;
    coord_t pos[3];
    uint an;
    int ix;

    pos[VX] = mo->origin[VX];
    pos[VY] = mo->origin[VY];
    pos[VZ] = mo->origin[VZ];

    an = mo->angle >> ANGLETOFINESHIFT;
    pos[VX] += dist * FIX2FLT(finecosine[an]);
    pos[VY] += dist * FIX2FLT(finesine[an]);
    pos[VZ] += mo->height / 2;
    pos[VZ] -= mo->floorClip;

    for(ix = 0; ix < 5; ++ix)
    {
        mobj_t* pmo;

        if((pmo = P_SpawnMobj(MT_SORCSPARK1, pos, P_Random() << 24, 0)))
        {
            unsigned int randAn = (mo->angle >> ANGLETOFINESHIFT) + ((P_Random() % 5) * 2);

            pmo->mom[MX] = FIX2FLT(FixedMul(P_Random() % FLT2FIX(speed), finecosine[randAn]));
            pmo->mom[MY] = FIX2FLT(FixedMul(P_Random() % FLT2FIX(speed), finesine[randAn]));
            pmo->mom[MZ] = 2;
        }
    }
}

/**
 * Yellow spell - offense.
 */
void C_DECL A_SorcFX1Seek(mobj_t* actor)
{
    A_BounceCheck(actor);
    P_SeekerMissile(actor, ANGLE_1 * 2, ANGLE_1 * 6);
}

/**
 * FX2 Variables.
 * special1         current angle
 * special2
 * args[0]          0 = CW,  1 = CCW
 * args[1]
 */

/**
 * Blue spell - defense (split ball in two).
 */
void C_DECL A_SorcFX2Split(mobj_t* mo)
{
    mobj_t*             pmo;

    if((pmo = P_SpawnMobj(MT_SORCFX2, mo->origin, mo->angle, 0)))
    {
        pmo->target = mo->target;
        pmo->args[0] = 0; // CW.
        pmo->special1 = mo->angle; // Set angle.
        P_MobjChangeStateNoAction(pmo, S_SORCFX2_ORBIT1);
    }

    if((pmo = P_SpawnMobj(MT_SORCFX2, mo->origin, mo->angle, 0)))
    {
        pmo->target = mo->target;
        pmo->args[0] = 1; // CCW.
        pmo->special1 = mo->angle; // Set angle.
        P_MobjChangeStateNoAction(pmo, S_SORCFX2_ORBIT1);
    }

    P_MobjChangeStateNoAction(mo, S_NULL);
}

/**
 * Orbit FX2 about sorcerer.
 */
void C_DECL A_SorcFX2Orbit(mobj_t* mo)
{
    unsigned int an;
    angle_t angle;
    mobj_t* parent;
    coord_t pos[3];
    coord_t dist;

    if(!mo->target) return;

    parent = mo->target;
    dist = parent->info->radius;

    if(parent->health <= 0 || // Sorcerer is dead.
       !parent->args[0]) // Time expired.
    {
        P_MobjChangeStateNoAction(mo, P_GetState(mo->type, SN_DEATH));
        parent->args[0] = 0;
        parent->flags2 &= ~(MF2_REFLECTIVE | MF2_INVULNERABLE);
    }

    if(mo->args[0] && (parent->args[0]-- <= 0)) // Time expired.
    {
        P_MobjChangeStateNoAction(mo, P_GetState(mo->type, SN_DEATH));
        parent->args[0] = 0;
        parent->flags2 &= ~MF2_REFLECTIVE;
    }

    // Move to new position based on angle.
    if(mo->args[0]) // Counter clock-wise.
        mo->special1 += ANGLE_1 * 10;
    else // Clock wise.
        mo->special1 -= ANGLE_1 * 10;

    angle = (angle_t) mo->special1;
    an = angle >> ANGLETOFINESHIFT;

    pos[VX] = parent->origin[VX];
    pos[VY] = parent->origin[VY];
    pos[VZ] = parent->origin[VZ];

    pos[VX] += dist * FIX2FLT(finecosine[an]);
    pos[VY] += dist * FIX2FLT(finesine[an]);
    pos[VZ] += SORC_DEFENSE_HEIGHT +
        (mo->args[0]? 15 : 20) * FIX2FLT(finecosine[an]);
    pos[VZ] -= parent->floorClip;

    // Spawn trailer.
    P_SpawnMobj(MT_SORCFX2_T1, pos, angle, 0);

    P_MobjUnlink(mo);

    mo->origin[VX] = pos[VX];
    mo->origin[VY] = pos[VY];
    mo->origin[VZ] = pos[VZ];

    P_MobjLink(mo);
}

/**
 * Green spell - spawn bishops.
 */
void C_DECL A_SpawnBishop(mobj_t *mo)
{
    mobj_t*             pmo;

    if((pmo = P_SpawnMobj(MT_BISHOP, mo->origin, mo->angle, 0)))
    {
        if(!P_TestMobjLocation(pmo))
        {
            P_MobjChangeState(pmo, S_NULL);
        }
    }

    P_MobjChangeState(mo, S_NULL);
}

void C_DECL A_SmokePuffExit(mobj_t* mo)
{
    P_SpawnMobj(MT_MNTRSMOKEEXIT, mo->origin, mo->angle, 0);
}

void C_DECL A_SorcererBishopEntry(mobj_t* mo)
{
    P_SpawnMobj(MT_SORCFX3_EXPLOSION, mo->origin, mo->angle, 0);
    S_StartSound(mo->info->seeSound, mo);
}

/**
 * FX4 - rapid fire balls.
 */
void C_DECL A_SorcFX4Check(mobj_t* mo)
{
    if(mo->special2-- <= 0)
    {
        P_MobjChangeStateNoAction(mo, P_GetState(mo->type, SN_DEATH));
    }
}

/**
 * Ball death - spawn stuff.
 */
void C_DECL A_SorcBallPop(mobj_t* mo)
{
    S_StartSound(SFX_SORCERER_BALLPOP, NULL);
    mo->flags &= ~MF_NOGRAVITY;
    mo->flags2 |= MF2_LOGRAV;

    mo->mom[MX] = (coord_t) ((P_Random() % 10) - 5);
    mo->mom[MY] = (coord_t) ((P_Random() % 10) - 5);
    mo->mom[MZ] = (coord_t) (2 + (P_Random() % 3));

    mo->special2 = 4 * FRACUNIT; // Initial bounce factor.
    mo->args[4] = BOUNCE_TIME_UNIT; // Bounce time unit.
    mo->args[3] = 5; // Bounce time in seconds.
}

void C_DECL A_BounceCheck(mobj_t* mo)
{
    if(mo->args[4]-- <= 0)
    {
        if(mo->args[3]-- <= 0)
        {
            P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
            switch(mo->type)
            {
            case MT_SORCBALL1:
            case MT_SORCBALL2:
            case MT_SORCBALL3:
                S_StartSound(SFX_SORCERER_BIGBALLEXPLODE, NULL);
                break;

            case MT_SORCFX1:
                S_StartSound(SFX_SORCERER_HEADSCREAM, NULL);
                break;

            default:
                break;
            }
        }
        else
        {
            mo->args[4] = BOUNCE_TIME_UNIT;
        }
    }
}

void C_DECL A_FastChase(mobj_t *mo)
{
#define CLASS_BOSS_STRAFE_RANGE     (64*10)

    int delta;
    coord_t dist;
    angle_t angle;
    uint an;
    mobj_t *target;
    statenum_t state;

    if(mo->reactionTime)
    {
        mo->reactionTime--;
    }

    // Modify target threshold.
    if(mo->threshold)
    {
        mo->threshold--;
    }

    if(gfw_Rule(skill) == SM_NIGHTMARE /*|| gameRules.fast*/)
    {
        // Monsters move faster in nightmare mode.
        mo->tics -= mo->tics / 2;
        if(mo->tics < 3)
        {
            mo->tics = 3;
        }
    }

    // Turn towards movement direction if not there yet.
    if(mo->moveDir < DI_NODIR)
    {
        mo->angle &= (7 << 29);
        delta = mo->angle - (mo->moveDir << 29);
        if(delta > 0)
        {
            mo->angle -= ANG90 / 2;
        }
        else if(delta < 0)
        {
            mo->angle += ANG90 / 2;
        }
    }

    if(!mo->target || !(mo->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        if(P_LookForPlayers(mo, true))
            return; // Got a new target

        P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
        return;
    }

    // Don't attack twice in a row.
    if(mo->flags & MF_JUSTATTACKED)
    {
        mo->flags &= ~MF_JUSTATTACKED;
        if(gfw_Rule(skill) != SM_NIGHTMARE)
            P_NewChaseDir(mo);
        return;
    }

    // Strafe.
    if(mo->special2 > 0)
    {
        mo->special2--;
    }
    else
    {
        target = mo->target;
        mo->special2 = 0;
        mo->mom[MX] = mo->mom[MY] = 0;
        dist = M_ApproxDistance(mo->origin[VX] - target->origin[VX],
                                mo->origin[VY] - target->origin[VY]);
        if(dist < CLASS_BOSS_STRAFE_RANGE)
        {
            if(P_Random() < 100)
            {
                angle = M_PointToAngle2(mo->origin, target->origin);
                if(P_Random() < 128)
                    angle += ANGLE_90;
                else
                    angle -= ANGLE_90;

                an = angle >> ANGLETOFINESHIFT;
                mo->mom[MX] = 13 * FIX2FLT(finecosine[an]);
                mo->mom[MY] = 13 * FIX2FLT(finesine[an]);
                mo->special2 = 3; // Strafe time.
            }
        }
    }

    // Check for missile attack.
    if((state = P_GetState(mo->type, SN_MISSILE)) != S_NULL)
    {
        if(gfw_Rule(skill) != SM_NIGHTMARE && mo->moveCount)
            goto nomissile;
        if(!P_CheckMissileRange(mo))
            goto nomissile;

        P_MobjChangeState(mo, state);
        mo->flags |= MF_JUSTATTACKED;
        return;
    }

   nomissile:

    // Possibly choose another target.
    if(IS_NETGAME && !mo->threshold && !P_CheckSight(mo, mo->target))
    {
        if(P_LookForPlayers(mo, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(!mo->special2)
    {
        if(--mo->moveCount < 0 || !P_Move(mo))
        {
            P_NewChaseDir(mo);
        }
    }

#undef CLASS_BOSS_STRAFE_RANGE
}

void C_DECL A_FighterAttack(mobj_t *mo)
{
    if(!mo->target)
        return;

    A_FSwordAttack2(mo);
}

void C_DECL A_ClericAttack(mobj_t *mo)
{
    if(!mo->target)
        return;

    A_CHolyAttack3(mo);
}

void C_DECL A_MageAttack(mobj_t *mo)
{
    if(!mo->target)
        return;

    A_MStaffAttack2(mo);
}

void C_DECL A_ClassBossHealth(mobj_t *mo)
{
    if(IS_NETGAME && !gfw_Rule(deathmatch)) // Co-op only.
    {
        if(!mo->special1)
        {
            mo->health *= 5;
            mo->special1 = true; // Has been initialized.
        }
    }
}

/**
 * Checks if an object hit the floor.
 */
void C_DECL A_CheckFloor(mobj_t* mo)
{
    if(mo->origin[VZ] <= mo->floorZ)
    {
        mo->origin[VZ] = mo->floorZ;
        mo->flags2 &= ~MF2_LOGRAV;
        P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
    }
}

void C_DECL A_FreezeDeath(mobj_t* mo)
{
    mo->tics = 75 + P_Random() + P_Random();
    mo->flags |= MF_SOLID | MF_SHOOTABLE | MF_NOBLOOD;
    mo->flags2 |= MF2_PUSHABLE | MF2_TELESTOMP | MF2_PASSMOBJ | MF2_SLIDE;
    mo->height *= 2*2;
    S_StartSound(SFX_FREEZE_DEATH, mo);

    if(mo->player)
    {
        player_t* plr = mo->player;

        plr->damageCount = 0;
        plr->poisonCount = 0;
        plr->bonusCount = 0;

        R_UpdateViewFilter(plr - players);
    }
    else if(mo->flags & MF_COUNTKILL && mo->special)
    {
        // Initiate monster death actions.
        P_ExecuteLineSpecial(mo->special, mo->args, NULL, 0, mo);
    }
}

void C_DECL A_IceSetTics(mobj_t* mo)
{
    const terraintype_t* tt = P_MobjFloorTerrain(mo);

    mo->tics = 70 + (P_Random() & 63);

    if(tt->flags & TTF_FRICTION_LOW)
    {
        mo->tics *= 2;
    }
    else if(tt->flags & TTF_FRICTION_HIGH)
    {
        mo->tics /= 4;
    }
}

void C_DECL A_IceCheckHeadDone(mobj_t* mo)
{
    if(mo->special2 == 666)
    {
        P_MobjChangeState(mo, S_ICECHUNK_HEAD2);
    }
}

#ifdef MSVC
#  pragma optimize("g", off)
#endif

void C_DECL A_FreezeDeathChunks(mobj_t* mo)
{
    coord_t pos[3];
    mobj_t* pmo;
    int i;

    if(NON_ZERO(mo->mom[MX]) || NON_ZERO(mo->mom[MY]) || NON_ZERO(mo->mom[MZ]))
    {
        mo->tics = 105;
        return;
    }

    S_StartSound(SFX_FREEZE_SHATTER, mo);

    for(i = 12 + (P_Random() & 15); i >= 0; i--)
    {
        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT(((P_Random() - 128) * FLT2FIX(mo->radius)) >> 7);
        pos[VY] += FIX2FLT(((P_Random() - 128) * FLT2FIX(mo->radius)) >> 7);
        pos[VZ] += (P_Random() * mo->height) / 255;

        if((pmo = P_SpawnMobj(MT_ICECHUNK, pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(pmo,
                P_GetState(pmo->type, SN_SPAWN) + (P_Random() % 3));

            pmo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MZ] = ((pmo->origin[VZ] - mo->origin[VZ]) / mo->height) * 4;

            A_IceSetTics(pmo); // Set a random tic wait.
        }
    }

    for(i = 12 + (P_Random() & 15); i >= 0; i--)
    {
        pos[VX] = mo->origin[VX];
        pos[VY] = mo->origin[VY];
        pos[VZ] = mo->origin[VZ];

        pos[VX] += FIX2FLT(((P_Random() - 128) * FLT2FIX(mo->radius)) >> 7);
        pos[VY] += FIX2FLT(((P_Random() - 128) * FLT2FIX(mo->radius)) >> 7);
        pos[VZ] += (P_Random() * mo->height) / 255;

        if((pmo = P_SpawnMobj(MT_ICECHUNK, pos, P_Random() << 24, 0)))
        {
            P_MobjChangeState(pmo,
                P_GetState(pmo->type, SN_SPAWN) + (P_Random() % 3));

            pmo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MZ] = ((pmo->origin[VZ] - mo->origin[VZ]) / mo->height) * 4;

            A_IceSetTics(pmo); // Set a random tic wait.
        }
    }

    if(mo->player)
    {   // Attach the player's view to a chunk of ice.
        if((pmo = P_SpawnMobjXYZ(MT_ICECHUNK, mo->origin[VX], mo->origin[VY],
                                mo->origin[VZ] + VIEWHEIGHT, mo->angle, 0)))
        {
            P_MobjChangeState(pmo, S_ICECHUNK_HEAD);

            pmo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 9);
            pmo->mom[MZ] = ((pmo->origin[VZ] - mo->origin[VZ]) / mo->height) * 4;

            pmo->flags2 |= MF2_ICEDAMAGE; // Used to force blue palette.
            pmo->flags2 &= ~MF2_FLOORCLIP;
            pmo->player = mo->player;
            pmo->dPlayer = mo->dPlayer;
            mo->player = NULL;
            mo->dPlayer = NULL;

            pmo->health = mo->health;
            pmo->player->plr->mo = pmo;
            pmo->player->plr->lookDir = 0;
        }
    }

    P_MobjRemoveFromTIDList(mo);
    P_MobjChangeState(mo, S_FREETARGMOBJ);
    mo->flags2 |= MF2_DONTDRAW;
}

#ifdef MSVC
#  pragma optimize("", off)
#endif

/**
 * Korax Variables.
 *
 * special1     Last teleport destination.
 * special2     Set if "below half" script not yet run.
 */

/**
 * Korax Scripts (reserved).
 *
 * 249          Tell scripts that we are below half health.
 * 250-254      Control scripts.
 * 255          Death script.
 */

/**
 * Korax TIDs (reserved).
 *
 * 245          Reserved for Korax himself.
 * 248          Initial teleport destination.
 * 249          Teleport destination.
 * 250-254      For use in respective control scripts.
 * 255          For use in death script (spawn spots).
 */

void C_DECL A_KoraxChase(mobj_t *mob)
{
    mobj_t *spot;

    if(!mob->special2 && mob->health <= mob->info->spawnHealth / 2)
    {
        int lastFound = 0;
        spot = P_FindMobjFromTID(KORAX_FIRST_TELEPORT_TID, &lastFound);
        if(spot)
        {
            P_Teleport(mob, spot->origin[VX], spot->origin[VY], spot->angle, true);
        }

        P_StartACScript(249, NULL, mob, NULL, 0);
        mob->special2 = 1; // Don't run again.

        return;
    }

    if(!mob->target)
        return;

    if(P_Random() < 30)
    {
        P_MobjChangeState(mob, P_GetState(mob->type, SN_MISSILE));
    }
    else if(P_Random() < 30)
    {
        S_StartSound(SFX_KORAX_ACTIVE, NULL);
    }

    // Teleport away.
    if(mob->health < mob->info->spawnHealth / 2)
    {
        if(P_Random() < 10)
        {
            spot = P_FindMobjFromTID(KORAX_TELEPORT_TID, &mob->special1);
            mob->tracer = spot;
            if(spot)
            {
                P_Teleport(mob, spot->origin[VX], spot->origin[VY],
                           spot->angle, true);
            }
        }
    }
}

void C_DECL A_KoraxStep(mobj_t* actor)
{
    A_Chase(actor);
}

void C_DECL A_KoraxStep2(mobj_t* actor)
{
    S_StartSound(SFX_KORAX_STEP, NULL);
    A_Chase(actor);
}

static void Korax_InitSpirit(mobj_t *spirit, mobj_t *korax)
{
    mobj_t *tail;
    DE_ASSERT(spirit);

    spirit->health   = KORAX_SPIRIT_LIFETIME;
    spirit->tracer   = korax; // Swarm around korax.
    spirit->special2 = 32 + (P_Random() & 7); // Float bob index.
    spirit->args[0]  = 10;  // Initial turn value.
    spirit->args[1]  = 0;   // Initial look angle.

    // Spawn a tail for spirit.
    if((tail = P_SpawnMobj(MT_HOLY_TAIL, spirit->origin, spirit->angle + ANG180, 0)))
    {
        tail->target = spirit;  // Parent.

        { int i;
        for(i = 1; i < 3; ++i)
        {
            mobj_t *next = P_SpawnMobj(MT_HOLY_TAIL, spirit->origin, spirit->angle + ANG180, 0);
            if(next)
            {
                P_MobjChangeState(next, P_GetState(next->type, SN_SPAWN) + 1);
                tail->tracer = next;
                tail = next;
            }
        }}

        tail->tracer = NULL;  // Last tail bit.
    }
}

void C_DECL A_KoraxBonePop(mobj_t *mob)
{
    mobj_t *spit;

    // Spawn 6 spirits equalangularly.
    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT1, mob, ANGLE_60 * 0, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT2, mob, ANGLE_60 * 1, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT3, mob, ANGLE_60 * 2, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT4, mob, ANGLE_60 * 3, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT5, mob, ANGLE_60 * 4, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    spit = P_SpawnMissileAngle(MT_KORAX_SPIRIT6, mob, ANGLE_60 * 5, 5);
    if(spit) Korax_InitSpirit(spit, mob);

    // Start the on-death ACScript.
    P_StartACScript(255, NULL, mob, NULL, 0);
}

void C_DECL A_KoraxDecide(mobj_t *mob)
{
    P_MobjChangeState(mob, P_Random() < 220 ? S_KORAX_MISSILE1 : S_KORAX_COMMAND1);
}

/**
 * Randomly chooses one of the six available missile types.
 *
 * @param fireSound  If not @c nullptr, the sound to play when fired is written here.
 *
 * @return  Map-object type for the choosen missile.
 */
static mobjtype_t Korax_ChooseMissileType(sfxenum_t *fireSound)
{
    struct MissileData { mobjtype_t type; sfxenum_t fireSound; } static const missileData[] =
    {
        { MT_WRAITHFX1,     SFX_WRAITH_MISSILE_FIRE  },
        { MT_DEMONFX1,      SFX_DEMON_MISSILE_FIRE   },
        { MT_DEMON2FX1,     SFX_DEMON_MISSILE_FIRE   },
        { MT_FIREDEMON_FX6, SFX_FIRED_ATTACK         },
        { MT_CENTAUR_FX,    SFX_CENTAURLEADER_ATTACK },
        { MT_SERPENTFX,     SFX_CENTAURLEADER_ATTACK }
    };
    static size_t const numMissileTypes = sizeof(missileData) / sizeof(missileData[0]);

    int const missileNum = P_Random() % numMissileTypes;
    if(fireSound) *fireSound = missileData[missileNum].fireSound;
    return missileData[missileNum].type;
}

/**
 * Determines the relative spawn point @a offset, in world space, for a missile
 * to be launched with the referenced @a arm.
 *
 * @param mob     Map-object (Korax).
 * @param arm     Logical arm number [0..5] where:
 *                [0: top left, 1: mid left, 2: bottom left, 3: top right, 4: mid right, 5: bottom right]
 * @param offset  Relative offset in world space is written here.
 *
 * @return  Same as @a offset for caller convenience.
 */
static pvec3d_t Korax_MissileSpawnPoint(mobj_t const *mob, int arm, pvec3d_t offset)
{
#define ARM_ANGLE            ( 85 * ANGLE_1 )
#define ARM_EXTENSION_SHORT  ( 40 )
#define ARM_EXTENSION_LONG   ( 55 )

    static MissileSpawnPoint const relSpawnPointByArm[] =
    {
        /* top left */     { -ARM_ANGLE, ARM_EXTENSION_SHORT, 108 },
        /* mid left */     { -ARM_ANGLE, ARM_EXTENSION_LONG,   82 },
        /* bottom left */  { -ARM_ANGLE, ARM_EXTENSION_LONG,   54 },
        /* top right */    { ARM_ANGLE,  ARM_EXTENSION_SHORT, 104 },
        /* mid right */    { ARM_ANGLE,  ARM_EXTENSION_LONG,   86 },
        /* bottom right */ { ARM_ANGLE,  ARM_EXTENSION_LONG,   53 }
    };
    static size_t const numSpawnPoints = sizeof(relSpawnPointByArm) / sizeof(relSpawnPointByArm[0]);

    MissileSpawnPoint const *relSpawn = &relSpawnPointByArm[MAX_OF(arm, 0) % numSpawnPoints];
    uint an;
    DE_ASSERT(mob && offset);

    an = (mob->angle + relSpawn->angle) >> ANGLETOFINESHIFT;
    V3d_Set(offset, relSpawn->distance * FIX2FLT(finecosine[an]),
                    relSpawn->distance * FIX2FLT(finesine  [an]),
                    relSpawn->height);

    return offset;  // For caller convenience.

#undef ARM_EXTENSION_LONG
#undef ARM_EXTENSION_SHORT
#undef ARM_ANGLE
}

/**
 * Korax's six missile attack.
 */
void C_DECL A_KoraxMissile(mobj_t *mob)
{
    mobj_t *target = mob->target;
    sfxenum_t fireSound;
    mobjtype_t missileType;
    int arm;

    target = mob->target;
    if(!target) return;

    S_StartSound(SFX_KORAX_ATTACK, mob);

    // Throw a missile with each of our 6 arms, all at once.
    missileType = Korax_ChooseMissileType(&fireSound);
    S_StartSound(fireSound, NULL);
    for(arm = 0; arm < 6; ++arm)
    {
        vec3d_t pos, offset;
        V3d_Sum(pos, mob->origin, Korax_MissileSpawnPoint(mob, arm, offset));
        pos[2] -= mob->floorClip;

        Mobj_LaunchMissile2(mob, P_SpawnMobj(missileType, pos, P_AimAtPoint2(pos, target->origin, target->flags & MF_SHADOW), 0),
                            target->origin, NULL/*use missile origin to calculate speed*/, 30 /*extra z-momentum*/);
    }
}

/**
 * Call action code scripts (250-254).
 */
void C_DECL A_KoraxCommand(mobj_t *mob)
{
    int numScripts, scriptNumber = -1;
    vec3d_t offset, pos;
    uint an;

    S_StartSound(SFX_KORAX_COMMAND, mob);

    // Shoot stream of lightning to ceiling.
    an = (mob->angle - ANGLE_90) >> ANGLETOFINESHIFT;
    V3d_Set(offset, KORAX_COMMAND_OFFSET * FIX2FLT(finecosine[an]),
                    KORAX_COMMAND_OFFSET * FIX2FLT(finesine  [an]),
                    KORAX_COMMAND_HEIGHT);
    V3d_Sum(pos, mob->origin, offset);
    P_SpawnMobj(MT_KORAX_BOLT, pos, mob->angle, 0);

    // Start a randomly chosen script.
    numScripts = (mob->health <= mob->info->spawnHealth / 2)? 5 : 4;
    switch(P_Random() % numScripts)
    {
    case 0: scriptNumber = 250; break;
    case 1: scriptNumber = 251; break;
    case 2: scriptNumber = 252; break;
    case 3: scriptNumber = 253; break;
    case 4: scriptNumber = 254; break;
    }
    P_StartACScript(scriptNumber, NULL, mob, NULL, 0);
}

void C_DECL A_KSpiritWeave(mobj_t *mob)
{
    uint const an = (mob->angle + ANG90) >> ANGLETOFINESHIFT;
    coord_t pos[3];

    // Unpack the last weave vector.
    uint weaveXY = mob->special2 >> 16;
    uint weaveZ  = mob->special2 & 0xFFFF;

    pos[VX] = mob->origin[VX];
    pos[VY] = mob->origin[VY];
    pos[VZ] = mob->origin[VZ];

    pos[VX] -= (FLOATBOBOFFSET(weaveXY) * 4) * FIX2FLT(finecosine[an]);
    pos[VY] -= (FLOATBOBOFFSET(weaveXY) * 4) * FIX2FLT(finesine  [an]);
    pos[VZ] -= (FLOATBOBOFFSET(weaveZ ) * 2);

    weaveXY = (weaveXY + (P_Random() % 5)) & 63;
    weaveZ  = (weaveZ  + (P_Random() % 5)) & 63;

    pos[VX] += (FLOATBOBOFFSET(weaveXY) * 4) * FIX2FLT(finecosine[an]);
    pos[VY] += (FLOATBOBOFFSET(weaveXY) * 4) * FIX2FLT(finesine  [an]);
    pos[VZ] += (FLOATBOBOFFSET(weaveZ ) * 2);

    P_TryMoveXY(mob, pos[VX], pos[VY]);

    // P_TryMoveXY won't update the z height, so set it manually.
    /// @todo Should this not be clipped vs the floor/ceiling? - ds
    mob->origin[VZ] = pos[VZ];

    // Store the weave vector.
    mob->special2 = weaveZ + (weaveXY << 16);
}

void C_DECL A_KSpiritSeeker(mobj_t* mo, angle_t thresh, angle_t turnMax)
{
    int dir, dist;
    angle_t delta;
    uint an;
    mobj_t* target;
    coord_t newZ, deltaZ;

    target = mo->tracer;
    if(!target) return;

    dir = P_FaceMobj(mo, target, &delta);
    if(delta > thresh)
    {
        delta /= 2;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir) // Turn clockwise.
        mo->angle += delta;
    else // Turn counter clockwise.
        mo->angle -= delta;

    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = mo->info->speed * FIX2FLT(finesine[an]);

    if(!(mapTime & 15) ||
       (mo->origin[VZ] > target->origin[VZ] + target->info->height) ||
       (mo->origin[VZ] + mo->height < target->origin[VZ]))
    {
        newZ = target->origin[VZ] + FIX2FLT((P_Random() * FLT2FIX(target->info->height)) >> 8);
        deltaZ = newZ - mo->origin[VZ];

        if(fabs(deltaZ) > 15)
        {
            if(deltaZ > 0)
            {
                deltaZ = 15;
            }
            else
            {
                deltaZ = -15;
            }
        }

        dist = M_ApproxDistance(target->origin[VX] - mo->origin[VX],
                                target->origin[VY] - mo->origin[VY]);
        dist /= mo->info->speed;
        if(dist < 1)
            dist = 1;

        mo->mom[MZ] = deltaZ / dist;
    }
}

void C_DECL A_KSpiritRoam(mobj_t* mo)
{
    if(mo->health-- <= 0)
    {
        S_StartSound(SFX_SPIRIT_DIE, mo);
        P_MobjChangeState(mo, S_KSPIRIT_DEATH1);
    }
    else
    {
        if(mo->tracer)
        {
            A_KSpiritSeeker(mo, mo->args[0] * ANGLE_1,
                            mo->args[0] * ANGLE_1 * 2);
        }

        A_KSpiritWeave(mo);
        if(P_Random() < 50)
        {
            S_StartSound(SFX_SPIRIT_ACTIVE, NULL);
        }
    }
}

void C_DECL A_KBolt(mobj_t* mo)
{
    // Countdown lifetime.
    if(mo->special1-- <= 0)
    {
        P_MobjChangeState(mo, S_NULL);
    }
}

void C_DECL A_KBoltRaise(mobj_t* mo)
{
#define KORAX_BOLT_HEIGHT       (48)
#define KORAX_BOLT_LIFETIME     (3)

    coord_t z;
    mobj_t* pmo;

    // Spawn a child upward.
    z = mo->origin[VZ] + KORAX_BOLT_HEIGHT;

    if(z + KORAX_BOLT_HEIGHT < mo->ceilingZ)
    {
        if((pmo = P_SpawnMobjXYZ(MT_KORAX_BOLT, mo->origin[VX], mo->origin[VY], z, mo->angle, 0)))
        {
            pmo->special1 = KORAX_BOLT_LIFETIME;
        }
    }

#undef KORAX_BOLT_HEIGHT
#undef KORAX_BOLT_LIFETIME
}
