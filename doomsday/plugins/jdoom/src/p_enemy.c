/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * Action Pointer Functions that are associated with states/frames.
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

// MACROS ------------------------------------------------------------------

#define FATSPREAD               (ANG90/8)
#define SKULLSPEED              (20)

// TYPES -------------------------------------------------------------------

typedef enum dirtype_s {
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void C_DECL A_ReFire(player_t *player, pspdef_t *psp);
void C_DECL A_Fall(mobj_t *actor);
void C_DECL A_Fire(mobj_t *actor);
void C_DECL A_SpawnFly(mobj_t *mo);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean felldown; //$dropoff_fix: used to flag pushed off ledge
extern linedef_t *blockline; // $unstuck: blocking linedef
extern float tmbbox[4]; // for line intersection checks

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;
mobj_t *soundtarget;

mobj_t *corpsehit;
mobj_t *vileobj;
float   vileTry[3];

mobj_t **braintargets;
int     numbraintargets;
int     numbraintargets_alloc;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

int     TRACEANGLE = 0xc000000;

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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float dropoffDelta[2], floorz;

// CODE --------------------------------------------------------------------

/**
 * Recursively traverse adjacent sectors, sound blocking lines cut off
 * traversal. Called by P_NoiseAlert.
 */
void P_RecursiveSound(sector_t *sec, int soundblocks)
{
    int                 i;
    linedef_t             *check;
    xline_t            *xline;
    sector_t           *frontsector, *backsector, *other;
    xsector_t          *xsec = P_ToXSector(sec);

    // Wake up all monsters in this sector.
    if(P_GetIntp(sec, DMU_VALID_COUNT) == VALIDCOUNT &&
       xsec->soundTraversed <= soundblocks + 1)
        return; // Already flooded.

    P_SetIntp(sec, DMU_VALID_COUNT, VALIDCOUNT);

    xsec->soundTraversed = soundblocks + 1;
    xsec->soundTarget = soundtarget;

    for(i = 0; i < P_GetIntp(sec, DMU_LINEDEF_COUNT); ++i)
    {
        check = P_GetPtrp(sec, DMU_LINEDEF_OF_SECTOR | i);

        frontsector = P_GetPtrp(check, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(check, DMU_BACK_SECTOR);

        if(!(P_GetIntp(check, DMU_FLAGS) & DDLF_TWOSIDED))
            continue;

        P_LineOpening(check);

        if(openrange <= 0)
            continue; // Closed door?

        if(frontsector == sec)
            other = backsector;
        else
            other = frontsector;

        xline = P_ToXLine(check);
        if(xline->flags & ML_SOUNDBLOCK)
        {
            if(!soundblocks)
                P_RecursiveSound(other, 1);
        }
        else
            P_RecursiveSound(other, soundblocks);
    }
}

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player's whereabouts.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    soundtarget = target;
    VALIDCOUNT++;
    P_RecursiveSound(P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

boolean P_CheckMeleeRange(mobj_t *actor)
{
    mobj_t     *pl;
    float       dist, range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!(cfg.netNoMaxZMonsterMeleeAttack))
    {
        // Account for Z height difference.
        dist =
            P_ApproxDistance(dist, pl->pos[VZ] + (pl->height /2) -
                                   actor->pos[VZ] + (actor->height /2));
    }

    range = MELEERANGE - 20 + pl->info->radius;
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

boolean P_CheckMissileRange(mobj_t *actor)
{
    float       dist;

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

    if(!actor->info->meleeState)
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
boolean P_Move(mobj_t *actor, boolean dropoff)
{
    float       pos[3], step[3];
    linedef_t     *ld;
    boolean     good;

    if(actor->moveDir == DI_NODIR)
        return false;

    if((unsigned) actor->moveDir >= 8)
        Con_Error("Weird actor->moveDir!");

    step[VX] = actor->info->speed * dirSpeed[actor->moveDir][VX];
    step[VY] = actor->info->speed * dirSpeed[actor->moveDir][VY];
    pos[VX] = actor->pos[VX] + step[VX];
    pos[VY] = actor->pos[VY] + step[VY];

    // killough $dropoff_fix
    if(!P_TryMove(actor, pos[VX], pos[VY], dropoff, false))
    {
        // Open any specials.
        if((actor->flags & MF_FLOAT) && floatok)
        {
            // Must adjust height.
            if(actor->pos[VZ] < tmfloorz)
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
                good |= ld == blockline ? 1 : 2;
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

    // $dropoff_fix: fall more slowly, under gravity, if felldown==true
    if(!(actor->flags & MF_FLOAT) && !felldown)
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
boolean P_TryWalk(mobj_t *actor)
{
    // killough $dropoff_fix
    if(!P_Move(actor, false))
    {
        return false;
    }

    actor->moveCount = P_Random() & 15;
    return true;
}

static void P_DoNewChaseDir(mobj_t *actor, float deltaX, float deltaY)
{
    dirtype_t xdir, ydir, tdir;
    dirtype_t olddir = actor->moveDir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR) // Find reverse direction.
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
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
    }
    else
    {
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround &&
               (actor->moveDir = tdir, P_TryWalk(actor)))
                return;
    }

    if((actor->moveDir = turnaround) != DI_NODIR && !P_TryWalk(actor))
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
static boolean PIT_AvoidDropoff(linedef_t *line, void *data)
{
    sector_t   *backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    float      *bbox = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       tmbbox[BOXRIGHT]  > bbox[BOXLEFT] &&
       tmbbox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       tmbbox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       tmbbox[BOXBOTTOM] < bbox[BOXTOP]    &&
       P_BoxOnLineSide(tmbbox, line) == -1)
    {
        sector_t   *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
        float       front = P_GetFloatp(frontsector, DMU_FLOOR_HEIGHT);
        float       back = P_GetFloatp(backsector, DMU_FLOOR_HEIGHT);
        float       dx = P_GetFloatp(line, DMU_DX);
        float       dy = P_GetFloatp(line, DMU_DY);
        angle_t     angle;

        // The monster must contact one of the two floors, and the other
        // must be a tall drop off (more than 24).
        if(back == floorz && front < floorz - 24)
        {
            angle = R_PointToAngle2(0, 0, dx, dy); // Front side drop off.
        }
        else
        {
            if(front == floorz && back < floorz - 24)
                angle = R_PointToAngle2(dx, dy, 0, 0); // Back side drop off.
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        dropoffDelta[VX] -= FIX2FLT(finesine[angle >> ANGLETOFINESHIFT] * 32);
        dropoffDelta[VY] += FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT] * 32);
    }

    return true;
}

/**
 * Driver for above.
 */
static boolean P_AvoidDropoff(mobj_t *actor)
{
    floorz = actor->pos[VZ]; // Remember floor height.

    dropoffDelta[VX] = dropoffDelta[VY] = 0;

    VALIDCOUNT++;

    // Check lines.
    P_MobjLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed.
    return !(dropoffDelta[VX] == 0 || dropoffDelta[VY] == 0);
}

void P_NewChaseDir(mobj_t *actor)
{
    mobj_t     *target = actor->target;
    float       deltax = target->pos[VX] - actor->pos[VX];
    float       deltay = target->pos[VY] - actor->pos[VY];

    if(actor->floorZ - actor->dropOffZ > 24 &&
       actor->pos[VZ] <= actor->floorZ &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && P_AvoidDropoff(actor))
    {
        // Move away from dropoff.
        P_DoNewChaseDir(actor, dropoffDelta[VX], dropoffDelta[VY]);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->moveCount = 1;
        return;
    }

    P_DoNewChaseDir(actor, deltax, deltay);
}

/**
 * If allaround is false, only look 180 degrees in front.
 *
 * @return              @c true, if a player is targeted.
 */
boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
    int         c, stop, playerCount;
    player_t   *player;
    angle_t     an;
    float       dist;

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
        {
            return false; // Done looking.
        }

        player = &players[actor->lastLook];

        if(player->health <= 0)
            continue; // Player is already dead.

        if(!P_CheckSight(actor, player->plr->mo))
            continue; // Player is out of sight.

        if(!allaround)
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

int P_Massacre(void)
{
    int         count = 0;
    mobj_t     *mo;
    thinker_t  *think;

    // Only massacre when in a level.
    if(G_GetGameState() != GS_LEVEL)
        return 0;

    for(think = thinkerCap.next; think != &thinkerCap; think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue; // Not a mobj thinker.

        mo = (mobj_t *) think;
        if(mo->type == MT_SKULL ||
           ((mo->flags & MF_COUNTKILL) && mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, 10000);
            count++;
        }
    }

    return count;
}

/**
 * DOOM II special, map 32. Uses special tag 666.
 */
void C_DECL A_KeenDie(mobj_t *mo)
{
    thinker_t  *th;
    mobj_t     *mo2;
    linedef_t     *dummyLine;

    A_Fall(mo);

    // Scan the remaining thinkers to see if all Keens are dead.
    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {   // Other Keen not dead.
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_ToXLine(dummyLine)->tag = 666;
    EV_DoDoor(dummyLine, open);
    P_FreeDummyLine(dummyLine);
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    sector_t   *sec = NULL;
    mobj_t     *targ;

    sec = P_GetPtrp(actor->subsector, DMU_SECTOR);

    if(!sec)
        return;

    if(actor->type == 11)
    {
        int d = 1;
        d=d;
    }

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

    if(!P_LookForPlayers(actor, false))
        return;

    // Go into chase state.
  seeyou:
    if(actor->info->seeSound)
    {
        int         sound;

        switch(actor->info->seeSound)
        {
        case sfx_posit1:
        case sfx_posit2:
        case sfx_posit3:
            sound = sfx_posit1 + P_Random() % 3;
            break;

        case sfx_bgsit1:
        case sfx_bgsit2:
            sound = sfx_bgsit1 + P_Random() % 2;
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

    P_MobjChangeState(actor, actor->info->seeState);
}

/**
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int         delta;

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
    if(actor->moveDir < 8)
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
        if(P_LookForPlayers(actor, true))
        {   // Got a new target.
        }
        else
        {
            P_MobjChangeState(actor, actor->info->spawnState);
        }

        return;
    }

    // Do not attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE && !fastparm)
            P_NewChaseDir(actor);

        return;
    }

    // Check for melee attack.
    if(actor->info->meleeState && P_CheckMeleeRange(actor))
    {
        if(actor->info->attackSound)
            S_StartSound(actor->info->attackSound, actor);

        P_MobjChangeState(actor, actor->info->meleeState);
        return;
    }

    // Check for missile attack.
    if(actor->info->missileState)
    {
        if(!(gameskill < SM_NIGHTMARE && !fastparm && actor->moveCount))
        {
            if(P_CheckMissileRange(actor))
            {
                P_MobjChangeState(actor, actor->info->missileState);
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
        P_NewChaseDir(actor);
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
    int         damage;
    angle_t     angle;
    float       slope;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    angle = actor->angle;
    slope = P_AimLineAttack(actor, angle, MISSILERANGE);

    S_StartSound(sfx_pistol, actor);
    angle += (P_Random() - P_Random()) << 20;
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_SPosAttack(mobj_t *actor)
{
    int         i, damage;
    angle_t     angle, bangle;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(sfx_shotgn, actor);
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

void C_DECL A_CPosAttack(mobj_t *actor)
{
    int         angle, bangle, damage;
    float       slope;

    if(!actor->target)
        return;

    S_StartSound(sfx_shotgn, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;
    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire(mobj_t *actor)
{
    // Keep firing unless target got out of sight.
    A_FaceTarget(actor);

    if(P_Random() < 40)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_MobjChangeState(actor, actor->info->seeState);
    }
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
        P_MobjChangeState(actor, actor->info->seeState);
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
    int         damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 3;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    P_SpawnMissile(MT_TROOPSHOT, actor, actor->target);
}

void C_DECL A_SargAttack(mobj_t *actor)
{
    int         damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 4;
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void C_DECL A_HeadAttack(mobj_t *actor)
{
    int         damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_HEADSHOT, actor, actor->target);
}

void C_DECL A_CyberAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(MT_ROCKET, actor, actor->target);
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int         damage;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // Launch a missile.
    P_SpawnMissile(MT_BRUISERSHOT, actor, actor->target);
}

void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t     *mo;

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
    angle_t     exact;
    float       dist;
    float       slope;
    mobj_t     *dest;
    mobj_t     *th;

    if(gametic & 3)
        return;

    // Spawn a puff of smoke behind the rocket.
    P_SpawnCustomPuff(MT_ROCKETPUFF, actor->pos[VX],
                      actor->pos[VY],
                      actor->pos[VZ]);

    th = P_SpawnMobj3f(MT_SMOKE,
                       actor->pos[VX] - actor->mom[MX],
                       actor->pos[VY] - actor->mom[MY],
                       actor->pos[VZ]);

    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

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
    actor->mom[MX] = actor->info->speed * FIX2FLT(finecosine[exact]);
    actor->mom[MY] = actor->info->speed * FIX2FLT(finesine[exact]);

    // Change slope.
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);

    dist /= actor->info->speed;

    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 - actor->pos[VZ]) / dist;

    if(slope < actor->mom[MZ])
        actor->mom[MZ] -= 1.0f / 8;
    else
        actor->mom[MZ] += 1.0f / 8;
}

void C_DECL A_SkelWhoosh(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    S_StartSound(sfx_skeswg, actor);
}

void C_DECL A_SkelFist(mobj_t *actor)
{
    int         damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 10) + 1) * 6;
        S_StartSound(sfx_skepch, actor);
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

/**
 * Detect a corpse that could be raised.
 */
boolean PIT_VileCheck(mobj_t *thing, void *data)
{
    float       maxdist;
    boolean     check;

    if(!(thing->flags & MF_CORPSE))
        return true; // Not a monster.

    if(thing->tics != -1)
        return true; // Not lying still yet.

    if(thing->info->raiseState == S_NULL)
        return true; // Monster doesn't have a raise state.

    maxdist = thing->info->radius + mobjInfo[MT_VILE].radius;

    if(fabs(thing->pos[VX] - vileTry[VX]) > maxdist ||
       fabs(thing->pos[VY] - vileTry[VY]) > maxdist)
        return true; // Not actually touching.

    corpsehit = thing;
    corpsehit->mom[MX] = corpsehit->mom[MY] = 0;

    // DJS - Used the PRBoom method to fix archvile raising ghosts
    // If !raiseghosts then ressurect a "normal" MF_SOLID one.
    if(cfg.raiseghosts)
    {
        corpsehit->height *= 2*2;
        check = P_CheckPosition2f(corpsehit, corpsehit->pos[VX], corpsehit->pos[VY]);
        corpsehit->height /= 2*2;
    }
    else
    {
        float       radius, height;

        height = corpsehit->height; // Save temporarily.
        radius = corpsehit->radius; // Save temporarily.
        corpsehit->height = corpsehit->info->height;
        corpsehit->radius = corpsehit->info->radius;
        corpsehit->flags |= MF_SOLID;

        check = P_CheckPosition2f(corpsehit, corpsehit->pos[VX], corpsehit->pos[VY]);

        corpsehit->height = height; // Restore.
        corpsehit->radius = radius; // Restore.
        corpsehit->flags &= ~MF_SOLID;
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
    mobjinfo_t *info;
    mobj_t     *temp;
    float       box[4];

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

        vileobj = actor;
        // Call PIT_VileCheck to check whether object is a corpse
        // that can be raised.
        if(!P_MobjsBoxIterator(box, PIT_VileCheck, 0))
        {
            // Got one!
            temp = actor->target;
            actor->target = corpsehit;
            A_FaceTarget(actor);
            actor->target = temp;

            P_MobjChangeState(actor, S_VILE_HEAL1);
            S_StartSound(sfx_slop, corpsehit);
            info = corpsehit->info;

            P_MobjChangeState(corpsehit, info->raiseState);

            if(cfg.raiseghosts)
            {
                corpsehit->height *= 2*2;
            }
            else
            {
                corpsehit->height = info->height;
                corpsehit->radius = info->radius;
            }

            corpsehit->flags = info->flags;
            corpsehit->health = info->spawnHealth;
            corpsehit->target = NULL;
            corpsehit->corpseTics = 0;
            return;
        }
    }

    // Return to normal attack.
    A_Chase(actor);
}

void C_DECL A_VileStart(mobj_t *actor)
{
    S_StartSound(sfx_vilatk, actor);
}

void C_DECL A_StartFire(mobj_t *actor)
{
    S_StartSound(sfx_flamst, actor);
    A_Fire(actor);
}

void C_DECL A_FireCrackle(mobj_t *actor)
{
    S_StartSound(sfx_flame, actor);
    A_Fire(actor);
}

/**
 * Keep fire in front of player unless out of sight.
 */
void C_DECL A_Fire(mobj_t *actor)
{
    mobj_t     *dest;
    uint        an;

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
    mobj_t     *fog;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    fog = P_SpawnMobj3fv(MT_FIRE, actor->target->pos);

    actor->tracer = fog;
    fog->target = actor;
    fog->tracer = actor->target;
    A_Fire(fog);
}

void C_DECL A_VileAttack(mobj_t *actor)
{
    mobj_t     *fire;
    uint        an;

    if(!actor->target)
        return;

    A_FaceTarget(actor);

    if(!P_CheckSight(actor, actor->target))
        return;

    S_StartSound(sfx_barexp, actor);
    P_DamageMobj(actor->target, actor, actor, 20);
    actor->target->mom[MZ] = 1000 / actor->target->info->mass;

    an = actor->angle >> ANGLETOFINESHIFT;
    fire = actor->tracer;

    if(!fire)
        return;

    // Move the fire between the vile and the player.
    fire->pos[VX] = actor->target->pos[VX] - (24 * FIX2FLT(finecosine[an]));
    fire->pos[VY] = actor->target->pos[VY] - (24 * FIX2FLT(finesine[an]));
    P_RadiusAttack(fire, actor, 70, 69);
}

/**
 * Mancubus attack:
 * Firing three missiles (bruisers) in three different directions?
 * ...Doesn't look like it.
 */
void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(sfx_manatk, actor);
}

void C_DECL A_FatAttack1(mobj_t *actor)
{
    mobj_t     *mo;
    uint        an;

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
    mobj_t     *mo;
    uint        an;

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
    mobj_t     *mo;
    uint        an;

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
    mobj_t     *dest;
    uint        an;
    float       dist;

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
void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    float       pos[3];
    mobj_t     *newmobj;
    uint        an;
    float       prestep;
    int         count;
    sector_t   *sec;
    thinker_t  *currentthinker;

    // DJS - Compat option for unlimited lost soul spawns
    if(cfg.maxskulls)
    {
        // Count total number of skull currently on the level.
        count = 0;

        currentthinker = thinkerCap.next;
        while(currentthinker != &thinkerCap)
        {
            if((currentthinker->function == P_MobjThinker) &&
               ((mobj_t *) currentthinker)->type == MT_SKULL)
                count++;

            currentthinker = currentthinker->next;
        }

        // If there are already 20 skulls on the level, don't spit another.
        if(count > 20)
            return;
    }

    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 +
        3 * ((actor->info->radius + mobjInfo[MT_SKULL].radius) / 2);

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += prestep * FIX2FLT(finecosine[an]);
    pos[VY] += prestep * FIX2FLT(finesine[an]);
    pos[VZ] += 8;

    // Compat option to prevent spawning lost souls inside walls /from prBoom
    if(cfg.allowskullsinwalls)
    {
        newmobj = P_SpawnMobj3fv(MT_SKULL, pos);
    }
    else
    {
        /**
         * Check whether the Lost Soul is being fired through a 1-sided
         * wall or an impassible line, or a "monsters can't cross" line.
         * If it is, then we don't allow the spawn.
         */

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        newmobj = P_SpawnMobj3fv(MT_SKULL, pos);
        sec = P_GetPtrp(newmobj->subsector, DMU_SECTOR);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->pos[VZ] >
              (P_GetFloatp(sec, DMU_CEILING_HEIGHT) - newmobj->height)) ||
           (newmobj->pos[VZ] < P_GetFloatp(sec, DMU_FLOOR_HEIGHT)))
        {
            // Kill it immediately.
            P_DamageMobj(newmobj, actor, actor, 10000);
            return;
        }
    }

    // Check for movements $dropoff_fix.
    if(!P_TryMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false, false))
    {
        // Kill it immediately.
        P_DamageMobj(newmobj, actor, actor, 10000);
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
    int         sound;

    switch(actor->info->deathSound)
    {
    case 0:
        return;

    case sfx_podth1:
    case sfx_podth2:
    case sfx_podth3:
        sound = sfx_podth1 + P_Random() % 3;
        break;

    case sfx_bgdth1:
    case sfx_bgdth2:
        sound = sfx_bgdth1 + P_Random() % 2;
        break;

    default:
        sound = actor->info->deathSound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_SPIDER || actor->type == MT_CYBORG)
    {
        // Full volume.
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
    }
    else
        S_StartSound(sound, actor);
}

void C_DECL A_XScream(mobj_t *actor)
{
    S_StartSound(sfx_slop, actor);
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

/**
 * Possibly trigger special effects if on first boss level
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    thinker_t  *th;
    mobj_t     *mo2;
    linedef_t     *dummyLine;
    int         i;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    if(gamemode == commercial)
    {
        if(gamemap != 7)
            return;
        if((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
    }
    else
    {
        switch(gameepisode)
        {
        case 1:
            if(gamemap != 8)
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
            if(!cfg.anybossdeath)
                if(mo->type != MT_BRUISER)
                    return;
            break;

        case 2:
            if(gamemap != 8)
                return;

            if(mo->type != MT_CYBORG)
                return;
            break;

        case 3:
            if(gamemap != 8)
                return;

            if(mo->type != MT_SPIDER)
                return;

            break;

        case 4:
            switch(gamemap)
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
            if(gamemap != 8)
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
    for(th = thinkerCap.next; th != &thinkerCap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {   // Other boss not dead.
            return;
        }
    }

    // Victory!
    if(gamemode == commercial)
    {
        if(gamemap == 7)
        {
            if(mo->type == MT_FATSO)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                return;
            }

            if(mo->type == MT_BABY)
            {
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 667;
                EV_DoFloor(dummyLine, raiseToTexture);
                P_FreeDummyLine(dummyLine);

                // Only activate once (rare, "DOOM2::MAP07-Dead Simple" bug).
                bossKilled = true;
                return;
            }
        }
    }
    else
    {
        switch(gameepisode)
        {
        case 1:
            dummyLine = P_AllocDummyLine();
            P_ToXLine(dummyLine)->tag = 666;
            EV_DoFloor(dummyLine, lowerFloorToLowest);
            P_FreeDummyLine(dummyLine);
            bossKilled = true;
            return;
            break;

        case 4:
            switch(gamemap)
            {
            case 6:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoDoor(dummyLine, blazeOpen);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;

            case 8:
                dummyLine = P_AllocDummyLine();
                P_ToXLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;
            }
        }
    }

    G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
}

void C_DECL A_Hoof(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    // \todo: Implement a MAPINFO option for this.
    S_StartSound(sfx_hoof |
                 (gamemode != commercial &&
                  gamemap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    // \todo: Implement a MAPINFO option for this.
    S_StartSound(sfx_metal |
                 (gamemode != commercial &&
                  gamemap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_BabyMetal(mobj_t *mo)
{
    S_StartSound(sfx_bspwlk, mo);
    A_Chase(mo);
}

void C_DECL A_OpenShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(sfx_dbopn, player->plr->mo);
}

void C_DECL A_LoadShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(sfx_dbload, player->plr->mo);
}

void C_DECL A_CloseShotgun2(player_t *player, pspdef_t *psp)
{
    S_StartSound(sfx_dbcls, player->plr->mo);
    A_ReFire(player, psp);
}

/**
 * Initialize boss brain targets at level startup, rather than at boss
 * wakeup, to prevent savegame-related crashes.
 *
 * \todo Does not belong in this file, find it a better home.
 */
void P_SpawnBrainTargets(void)
{
    thinker_t  *thinker;
    mobj_t     *m;

    // Find all the target spots.
    for(thinker = thinkerCap.next; thinker != &thinkerCap;
        thinker = thinker->next)
    {
        if(thinker->function != P_MobjThinker)
            continue; // Not a mobj.

        m = (mobj_t *) thinker;

        if(m->type == MT_BOSSTARGET)
        {
            if(numbraintargets >= numbraintargets_alloc)
            {
                // Do we need to alloc more targets?
                if(numbraintargets == numbraintargets_alloc)
                {
                    numbraintargets_alloc *= 2;
                    braintargets =
                        Z_Realloc(braintargets,
                                  numbraintargets_alloc * sizeof(*braintargets),
                                  PU_LEVEL);
                }
                else
                {
                    numbraintargets_alloc = 32;
                    braintargets =
                        Z_Malloc(numbraintargets_alloc * sizeof(*braintargets),
                                 PU_LEVEL, NULL);
                }
            }

            braintargets[numbraintargets++] = m;
        }
    }
}

void C_DECL A_BrainAwake(mobj_t *mo)
{
    S_StartSound(sfx_bossit, NULL);
}

void C_DECL A_BrainPain(mobj_t *mo)
{
    S_StartSound(sfx_bospn, NULL);
}

void C_DECL A_BrainScream(mobj_t *mo)
{
    float       pos[3];
    mobj_t     *th;

    for(pos[VX] = mo->pos[VX] - 196; pos[VX] < mo->pos[VX] + 320;
        pos[VX] += 8)
    {
        pos[VY] = mo->pos[VY] - 320;
        pos[VZ] = 128 + (P_Random() * 2);

        th = P_SpawnMobj3fv(MT_ROCKET, pos);
        th->mom[MZ] = FIX2FLT(P_Random() * 512);

        P_MobjChangeState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(sfx_bosdth, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    float       pos[3];
    mobj_t     *th;

    pos[VX] = mo->pos[VX] + ((P_Random() - P_Random()) * 2048);
    pos[VY] = mo->pos[VY];
    pos[VZ] = 128 + (P_Random() * 2);

    th = P_SpawnMobj3fv(MT_ROCKET, pos);
    th->mom[MZ] = FIX2FLT(P_Random() * 512);

    P_MobjChangeState(th, S_BRAINEXPLODE1);

    th->tics -= P_Random() & 7;
    if(th->tics < 1)
        th->tics = 1;
}

void C_DECL A_BrainDie(mobj_t *mo)
{
    G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
}

void C_DECL A_BrainSpit(mobj_t *mo)
{
    mobj_t     *targ;
    mobj_t     *newmobj;

    if(!numbraintargets)
        return; // Ignore if no targets.

    brain.easy ^= 1;
    if(gameskill <= SM_EASY && (!brain.easy))
        return;

    // Shoot a cube at current target.
    targ = braintargets[brain.targetOn++];
    brain.targetOn %= numbraintargets;

    // Spawn brain missile.
    newmobj = P_SpawnMissile(MT_SPAWNSHOT, mo, targ);
    if(newmobj)
    {
        newmobj->target = targ;
        newmobj->reactionTime =
            ((targ->pos[VY] - mo->pos[VY]) / newmobj->mom[MY]) / newmobj->state->tics;
    }

    S_StartSound(sfx_bospit, NULL);
}

/**
 * Travelling cube sound.
 */
void C_DECL A_SpawnSound(mobj_t *mo)
{
    S_StartSound(sfx_boscub, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly(mobj_t *mo)
{
    mobj_t     *newmobj;
    mobj_t     *fog;
    mobj_t     *targ;
    int         r;
    mobjtype_t  type;

    if(--mo->reactionTime)
        return; // Still flying.

    targ = mo->target;

    // First spawn teleport fog.
    fog = P_SpawnMobj3fv(MT_SPAWNFIRE, targ->pos);
    S_StartSound(sfx_telept, fog);

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

    newmobj = P_SpawnMobj3fv(type, targ->pos);

    if(P_LookForPlayers(newmobj, true))
        P_MobjChangeState(newmobj, newmobj->info->seeState);

    // Telefrag anything in this spot.
    P_TeleportMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false);

    // Remove self (i.e., cube).
    P_MobjRemove(mo);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    int         sound = sfx_pldeth; // Default death sound.

    if((gamemode == commercial) && (mo->health < -50))
    {
        // If the player dies less with less than -50% without gibbing.
        sound = sfx_pdiehi;
    }

    S_StartSound(sound, mo);
}
