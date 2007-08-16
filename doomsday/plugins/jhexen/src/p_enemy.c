/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 *
 */

/*
 * Enemy thinking, AI.
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapspec.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define MONS_LOOK_RANGE (16*64*FRACUNIT)
#define MONS_LOOK_LIMIT 64

#define MINOTAUR_LOOK_DIST      (16*54*FRACUNIT)

#define CORPSEQUEUESIZE 64
#define BODYQUESIZE 32

#define SORCBALL_INITIAL_SPEED      7
#define SORCBALL_TERMINAL_SPEED     25
#define SORCBALL_SPEED_ROTATIONS    5
#define SORC_DEFENSE_TIME           255
#define SORC_DEFENSE_HEIGHT         45
#define BOUNCE_TIME_UNIT            (35/2)
#define SORCFX4_RAPIDFIRE_TIME      (6*3)   // 3 seconds
#define SORCFX4_SPREAD_ANGLE        20

#define SORC_DECELERATE     0
#define SORC_ACCELERATE     1
#define SORC_STOPPING       2
#define SORC_FIRESPELL      3
#define SORC_STOPPED        4
#define SORC_NORMAL         5
#define SORC_FIRING_SPELL   6

#define BALL1_ANGLEOFFSET   0
#define BALL2_ANGLEOFFSET   (ANGLE_MAX/3)
#define BALL3_ANGLEOFFSET   ((ANGLE_MAX/3)*2)

#define KORAX_SPIRIT_LIFETIME   (5*(35/5))  // 5 seconds
#define KORAX_COMMAND_HEIGHT    (120*FRACUNIT)
#define KORAX_COMMAND_OFFSET    (27*FRACUNIT)

#define KORAX_TID                   (245)
#define KORAX_FIRST_TELEPORT_TID    (248)
#define KORAX_TELEPORT_TID          (249)

#define KORAX_DELTAANGLE            (85*ANGLE_1)
#define KORAX_ARM_EXTENSION_SHORT   (40*FRACUNIT)
#define KORAX_ARM_EXTENSION_LONG    (55*FRACUNIT)

#define KORAX_ARM1_HEIGHT           (108*FRACUNIT)
#define KORAX_ARM2_HEIGHT           (82*FRACUNIT)
#define KORAX_ARM3_HEIGHT           (54*FRACUNIT)
#define KORAX_ARM4_HEIGHT           (104*FRACUNIT)
#define KORAX_ARM5_HEIGHT           (86*FRACUNIT)
#define KORAX_ARM6_HEIGHT           (53*FRACUNIT)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

boolean     P_TestMobjLocation(mobj_t *mobj);
void C_DECL A_FSwordAttack2(mobj_t *actor);
void C_DECL A_CHolyAttack3(mobj_t *actor);
void C_DECL A_MStaffAttack2(mobj_t *actor);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void C_DECL A_SorcBallOrbit(mobj_t *actor);
void C_DECL A_SorcSpinBalls(mobj_t *actor);
void C_DECL A_SpeedBalls(mobj_t *actor);
void C_DECL A_SlowBalls(mobj_t *actor);
void C_DECL A_StopBalls(mobj_t *actor);
void C_DECL A_AccelBalls(mobj_t *actor);
void C_DECL A_DecelBalls(mobj_t *actor);
void C_DECL A_SorcBossAttack(mobj_t *actor);
void C_DECL A_SpawnFizzle(mobj_t *actor);
void C_DECL A_CastSorcererSpell(mobj_t *actor);
void C_DECL A_SorcUpdateBallAngle(mobj_t *actor);
void C_DECL A_BounceCheck(mobj_t *actor);
void C_DECL A_SorcFX1Seek(mobj_t *actor);
void C_DECL A_SorcOffense1(mobj_t *actor);
void C_DECL A_SorcOffense2(mobj_t *actor);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void    KoraxFire1(mobj_t *actor, int type);
void    KoraxFire2(mobj_t *actor, int type);
void    KoraxFire3(mobj_t *actor, int type);
void    KoraxFire4(mobj_t *actor, int type);
void    KoraxFire5(mobj_t *actor, int type);
void    KoraxFire6(mobj_t *actor, int type);
void    KSpiritInit(mobj_t *spirit, mobj_t *korax);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t FloatBobOffsets[64];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     MaulatorSeconds = 25;
boolean fastMonsters = false;

mobj_t *soundtarget;

fixed_t xspeed[8] =
    { FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000 };
fixed_t yspeed[8] =
    { 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000 };

dirtype_t opposite[] =
    { DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
    DI_NORTH, DI_NORTHWEST, DI_NODIR
};

dirtype_t diags[] = { DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST };

mobj_t *corpseQueue[CORPSEQUEUESIZE];
int     corpseQueueSlot;
mobj_t *bodyque[BODYQUESIZE];
int     bodyqueslot;


// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_RecursiveSound(sector_t *sec, int soundblocks)
{
    int         i;
    line_t     *check;
    sector_t   *other;

    // Wake up all monsters in this sector.
    if(P_GetIntp(sec, DMU_VALID_COUNT) == VALIDCOUNT &&
       P_XSector(sec)->soundtraversed <= soundblocks + 1)
    {   // Already flooded.
        return;
    }

    P_SetIntp(sec, DMU_VALID_COUNT, VALIDCOUNT);
    P_XSector(sec)->soundtraversed = soundblocks + 1;
    P_XSector(sec)->soundtarget = soundtarget;
    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); ++i)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);
        if(!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED))
            continue;

        P_LineOpening(check);
        if(openrange <= 0)
            continue; // Closed door.

        if(P_GetPtrp(P_GetPtrp(check, DMU_SIDE0), DMU_SECTOR) == sec)
        {
            other = P_GetPtrp( P_GetPtrp(check, DMU_SIDE1), DMU_SECTOR );
        }
        else
        {
            other = P_GetPtrp( P_GetPtrp(check, DMU_SIDE0), DMU_SECTOR );
        }

        if(P_GetIntp(check, DMU_FLAGS) & ML_SOUNDBLOCK)
        {
            if(!soundblocks)
            {
                P_RecursiveSound(other, 1);
            }
        }
        else
        {
            P_RecursiveSound(other, soundblocks);
        }
    }
}

/**
 * If a monster yells at a player, it will alert other monsters to the
 * player.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    soundtarget = target;
    VALIDCOUNT++;
    P_RecursiveSound(P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

boolean P_CheckMeleeRange(mobj_t *actor, boolean midrange)
{
    mobj_t     *pl;
    fixed_t     dist;
    fixed_t     range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);

    if(!(cfg.netNoMaxZMonsterMeleeAttack))
        dist = P_ApproxDistance(dist, (pl->pos[VZ] + FLT2FIX(pl->height /2)) -
                                    (actor->pos[VZ] + FLT2FIX(actor->height /2)));

    range = (MELEERANGE - 20 * FRACUNIT + pl->info->radius);
    if(midrange)
    {
        if(dist >= FixedMul(range, 2) || dist < range)
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

boolean P_CheckMissileRange(mobj_t *actor)
{
    fixed_t     dist;

    if(!P_CheckSight(actor, actor->target))
    {
        return false;
    }

    if(actor->flags & MF_JUSTHIT)
    {   // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactiontime)                
        return false; // Don't attack yet.

    dist =
        (P_ApproxDistance
         (actor->pos[VX] - actor->target->pos[VX],
          actor->pos[VY] - actor->target->pos[VY]) >> FRACBITS) - 64;
    if(!actor->info->meleestate)
    {   // No melee attack, so fire more frequently.
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
 * @return              <code>false</code> if the move is blocked.
 */
boolean P_Move(mobj_t *actor)
{
    fixed_t     tryx, tryy, stepx, stepy;
    line_t     *ld;
    boolean     good;

    if(actor->flags2 & MF2_BLASTED)
        return true;

    if(actor->movedir == DI_NODIR)
    {
        return false;
    }

    stepx = actor->info->speed / FRACUNIT * xspeed[actor->movedir];
    stepy = actor->info->speed / FRACUNIT * yspeed[actor->movedir];
    tryx = actor->pos[VX] + stepx;
    tryy = actor->pos[VY] + stepy;

    if(!P_TryMove(actor, tryx, tryy))
    {   // Open any specials.
        if(actor->flags & MF_FLOAT && floatok)
        {   // Must adjust height.
            if(actor->pos[VZ] < FLT2FIX(tmfloorz))
            {
                actor->pos[VZ] += FLOATSPEED;
            }
            else
            {
                actor->pos[VZ] -= FLOATSPEED;
            }

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(spechit))
            return false;

        actor->movedir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            // If the special isn't a door that can be opened,
            // return false.
            if(P_ActivateLine(ld, actor, 0, SPAC_USE))
            {
                good = true;
            }
        }
        return good;
    }
    else
    {
        P_SetThingSRVO(actor, stepx, stepy);
        actor->flags &= ~MF_INFLOAT;
    }

    if(!(actor->flags & MF_FLOAT))
    {
        if(actor->pos[VZ] > FLT2FIX(actor->floorz))
        {
            P_HitFloor(actor);
        }
        actor->pos[VZ] = FLT2FIX(actor->floorz);
    }

    return true;
}

/**
 * Attempts to move actor in its current (ob->moveangle) direction.
 * If a door is in the way, an OpenDoor call is made to start it opening.
 *
 * @return                  <code>false</code> if blocked by either a
 *                          wall or an actor.
 */
boolean P_TryWalk(mobj_t *actor)
{
    if(!P_Move(actor))
    {
        return false;
    }

    actor->movecount = P_Random() & 15;
    return true;
}

void P_NewChaseDir(mobj_t *actor)
{
    fixed_t     deltax, deltay;
    dirtype_t   d[3];
    dirtype_t   olddir, turnaround;
    int         tdir;

    if(!actor->target)
        Con_Error("P_NewChaseDir: called with no target");

    olddir = actor->movedir;
    turnaround = opposite[olddir];

    deltax = actor->target->pos[VX] - actor->pos[VX];
    deltay = actor->target->pos[VY] - actor->pos[VY];

    if(deltax > 10 * FRACUNIT)
        d[1] = DI_EAST;
    else if(deltax < -10 * FRACUNIT)
        d[1] = DI_WEST;
    else
        d[1] = DI_NODIR;

    if(deltay < -10 * FRACUNIT)
        d[2] = DI_SOUTH;
    else if(deltay > 10 * FRACUNIT)
        d[2] = DI_NORTH;
    else
        d[2] = DI_NODIR;

    // Try direct route.
    if(d[1] != DI_NODIR && d[2] != DI_NODIR)
    {
        actor->movedir = diags[((deltay < 0) << 1) + (deltax > 0)];
        if(actor->movedir != turnaround && P_TryWalk(actor))
            return;
    }

    // try other directions
    if(P_Random() > 200 || abs(deltay) > abs(deltax))
    {
        tdir = d[1];
        d[1] = d[2];
        d[2] = tdir;
    }

    if(d[1] == turnaround)
        d[1] = DI_NODIR;
    if(d[2] == turnaround)
        d[2] = DI_NODIR;

    if(d[1] != DI_NODIR)
    {
        actor->movedir = d[1];
        if(P_TryWalk(actor))
            return; // Either moved forward or attacked.
    }

    if(d[2] != DI_NODIR)
    {
        actor->movedir = d[2];
        if(P_TryWalk(actor))
            return;
    }

    // There is no direct path to the player, so pick another direction.

    if(olddir != DI_NODIR)
    {
        actor->movedir = olddir;
        if(P_TryWalk(actor))
            return;
    }

    if(P_Random() & 1) // Randomly determine direction of search.
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
        {
            if(tdir != turnaround)
            {
                actor->movedir = tdir;
                if(P_TryWalk(actor))
                    return;
            }
        }
    }
    else
    {
        for(tdir = DI_SOUTHEAST; tdir >= DI_EAST; tdir--)
        {
            if(tdir != turnaround)
            {
                actor->movedir = tdir;
                if(P_TryWalk(actor))
                    return;
            }
        }
    }

    if(turnaround != DI_NODIR)
    {
        actor->movedir = turnaround;
        if(P_TryWalk(actor))
            return;
    }

    actor->movedir = DI_NODIR; // Can't move.
}

boolean P_LookForMonsters(mobj_t *actor)
{
    int         count;
    mobj_t     *mo;
    thinker_t  *think;

    if(!P_CheckSight(players[0].plr->mo, actor))
    {   // Player can't see monster
        return false;
    }

    count = 0;
    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;
        if(!(mo->flags & MF_COUNTKILL) || (mo == actor) || (mo->health <= 0))
        {   // Not a valid monster
            continue;
        }

        if(P_ApproxDistance(actor->pos[VX] - mo->pos[VX],
                            actor->pos[VY] - mo->pos[VY]) >
           MONS_LOOK_RANGE)
        {    // Out of range
            continue;
        }

        if(P_Random() < 16) // Random chance of skiping it.
            continue;

        if(count++ > MONS_LOOK_LIMIT)
        {   // Stop searching
            return false;
        }

        if(!P_CheckSight(actor, mo))
        {   // Out of sight
            continue;
        }

        if(actor->type == MT_MINOTAUR)
        {
            if((mo->type == MT_MINOTAUR) &&
               (mo->target != ((player_t *) actor->tracer)->plr->mo))
            {
                continue;
            }
        }

        // Found a target monster
        actor->target = mo;
        return true;
    }

    return false;
}

/**
 * @param allaround         If <code>false</code> only look 180 degrees
 *                          in front of the actor.
 *
 * @return                  <code>true</code> if a player is targeted.
 */
boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
    int         c;
    int         stop;
    player_t   *player;
    sector_t   *sector;
    angle_t     an;
    fixed_t     dist;

    if(!IS_NETGAME && players[0].health <= 0)
    {   // Single player game and player is dead, look for monsters
        return (P_LookForMonsters(actor));
    }

    sector = P_GetPtrp(actor->subsector, DMU_SECTOR);
    c = 0;
    stop = (actor->lastlook - 1) & 3;
    for(;; actor->lastlook = (actor->lastlook + 1) & 3)
    {
        if(actor->lastlook == stop)
            return false; // Time to stop looking. -jk

        if(!players[actor->lastlook].plr->ingame)
            continue;

        if(c++ == 2)
            return false; // Done looking.

        player = &players[actor->lastlook];
        if(player->health <= 0)
            continue; // Dead.
        if(!P_CheckSight(actor, player->plr->mo))
            continue; // Out of sight.

        if(!allaround)
        {
            an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                 player->plr->mo->pos[VX], player->plr->mo->pos[VY]) -
                                 actor->angle;
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

        if(player->plr->mo->flags & MF_SHADOW)
        {   // Player is invisible.
            if((P_ApproxDistance
                (player->plr->mo->pos[VX] - actor->pos[VX],
                 player->plr->mo->pos[VY] - actor->pos[VY]) > 2 * MELEERANGE) &&
               P_ApproxDistance(player->plr->mo->mom[MX],
                                player->plr->mo->mom[MY]) < 5 * FRACUNIT)
            {   // Player is sneaking - can't detect.
                return false;
            }

            if(P_Random() < 225)
            {   // Player isn't sneaking, but still didn't detect.
                return false;
            }
        }

        if(actor->type == MT_MINOTAUR)
        {
            if(((player_t *) (actor->tracer)) == player)
                continue; // Don't target master.
        }

        actor->target = player->plr->mo;
        return true;
    }
}

/**
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    mobj_t     *targ;

    actor->threshold = 0; // Any shot will wake up.
    targ = P_XSectorOfSubsector(actor->subsector)->soundtarget;
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
    if(actor->info->seesound)
    {
        int     sound;

        sound = actor->info->seesound;
        if(actor->flags2 & MF2_BOSS)
        {   // Full volume
            S_StartSound(sound, NULL);
        }
        else
        {
            S_StartSound(sound, actor);
        }
    }
    P_SetMobjState(actor, actor->info->seestate);
}

/**
 * Actor has a melee attack, so it tries to close as fast as possible.
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int     delta;

    if(actor->reactiontime)
        actor->reactiontime--;

    // Modify target threshold
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gameskill == SM_NIGHTMARE || (fastMonsters /*&& INCOMPAT_OK */ ))
    {   // Monsters move faster in nightmare mode
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
        }
    }

    // Turn towards movement direction if not there yet.
    if(actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);
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

        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if(actor->info->meleestate && P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attacksound)
        {
            S_StartSound(actor->info->attacksound, actor);
        }
        P_SetMobjState(actor, actor->info->meleestate);
        return;
    }

    // Check for missile attack.
    if(actor->info->missilestate)
    {
        if(gameskill < SM_NIGHTMARE && actor->movecount)
            goto nomissile;
        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_SetMobjState(actor, actor->info->missilestate);
        actor->flags |= MF_JUSTATTACKED;
        return;
    }
  nomissile:

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold &&
       !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(--actor->movecount < 0 || !P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activesound && P_Random() < 3)
    {
        if(actor->type == MT_BISHOP && P_Random() < 128)
        {
            S_StartSound(actor->info->seesound, actor);
        }
        else if(actor->type == MT_PIG)
        {
            S_StartSound(SFX_PIG_ACTIVE1 + (P_Random() & 1), actor);
        }
        else if(actor->flags2 & MF2_BOSS)
        {
            S_StartSound(actor->info->activesound, NULL);
        }
        else
        {
            S_StartSound(actor->info->activesound, actor);
        }
    }
}

void C_DECL A_FaceTarget(mobj_t *actor)
{
    if(!actor->target)
        return;

    actor->turntime = true;     // $visangle-facetarget
    actor->flags &= ~MF_AMBUSH;
    actor->angle =
        R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                        actor->target->pos[VX], actor->target->pos[VY]);
    if(actor->target->flags & MF_SHADOW)
    {                           // Target is a ghost
        actor->angle += (P_Random() - P_Random()) << 21;
    }
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painsound)
    {
        S_StartSound(actor->info->painsound, actor);
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

    if((actor->type == MT_CENTAUR) || (actor->type == MT_CENTAURLEADER))
    {
        A_SetInvulnerable(actor);
    }
}

void C_DECL A_UnSetReflective(mobj_t *actor)
{
    actor->flags2 &= ~MF2_REFLECTIVE;

    if((actor->type == MT_CENTAUR) || (actor->type == MT_CENTAURLEADER))
    {
        A_UnSetInvulnerable(actor);
    }
}

/**
 * @return              <code>true</code> if the pig morphs.
 */
boolean P_UpdateMorphedMonster(mobj_t *actor, int tics)
{
    mobj_t     *fog;
    fixed_t     pos[3];
    mobjtype_t moType;
    mobj_t     *mo;
    mobj_t      oldMonster;

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
    memcpy(pos, actor->pos, sizeof(pos));

    oldMonster = *actor; // Save pig vars.

    P_RemoveMobjFromTIDList(actor);
    P_SetMobjState(actor, S_FREETARGMOBJ);
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], moType);

    if(P_TestMobjLocation(mo) == false)
    {   // Didn't fit.
        P_RemoveMobj(mo);
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], oldMonster.type);

        mo->angle = oldMonster.angle;
        mo->flags = oldMonster.flags;
        mo->health = oldMonster.health;
        mo->target = oldMonster.target;
        mo->special = oldMonster.special;
        mo->special1 = 5 * 35; // Next try in 5 seconds.
        mo->special2 = moType;
        mo->tid = oldMonster.tid;
        memcpy(mo->args, oldMonster.args, 5);

        P_InsertMobjIntoTIDList(mo, oldMonster.tid);
        return false;
    }

    mo->angle = oldMonster.angle;
    mo->target = oldMonster.target;
    mo->tid = oldMonster.tid;
    mo->special = oldMonster.special;
    memcpy(mo->args, oldMonster.args, 5);

    P_InsertMobjIntoTIDList(mo, oldMonster.tid);
    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
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
        P_DamageMobj(actor->target, actor, actor, 2 + (P_Random() & 1));
        S_StartSound(SFX_PIG_ATTACK, actor);
    }
}

void C_DECL A_PigPain(mobj_t *actor)
{
    A_Pain(actor);
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz))
    {
        actor->mom[MZ] = (int) (3.5 * FRACUNIT);
    }
}

void FaceMovementDirection(mobj_t *actor)
{
    switch(actor->movedir)
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
    unsigned int *starttime = (unsigned int *) actor->args;

    actor->flags &= ~MF_SHADOW; // In case pain caused him to
    actor->flags &= ~MF_ALTSHADOW;  // Skip his fade in.

    if((leveltime - *starttime) >= MAULATORTICS)
    {
        P_DamageMobj(actor, NULL, NULL, 10000);
        return;
    }

    if(P_Random() < 30)
        A_MinotaurLook(actor);  // adjust to closest target.

    if(P_Random() < 6)
    {   // Choose new direction
        actor->movedir = P_Random() % 8;
        FaceMovementDirection(actor);
    }

    if(!P_Move(actor))
    {   // Turn
        if(P_Random() & 1)
            actor->movedir = (++actor->movedir) % 8;
        else
            actor->movedir = (actor->movedir + 7) % 8;
        FaceMovementDirection(actor);
    }
}

/**
 * Look for enemy of player.
 */
void C_DECL A_MinotaurLook(mobj_t *actor)
{
    mobj_t     *mo = NULL;
    player_t   *player;
    thinker_t  *think;
    fixed_t     dist;
    int         i;
    mobj_t     *master = actor->tracer;

    actor->target = NULL;
    if(deathmatch) // Quick search for players.
    {
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(!players[i].plr->ingame)
                continue;

            player = &players[i];
            mo = player->plr->mo;
            if(mo == master)
                continue;

            if(mo->health <= 0)
                continue;

            dist = P_ApproxDistance(actor->pos[VX] - mo->pos[VX],
                                    actor->pos[VY] - mo->pos[VY]);
            if(dist > MINOTAUR_LOOK_DIST)
                continue;

            actor->target = mo;
            break;
        }
    }

    if(!actor->target) // Near player monster search.
    {
        if(master && (master->health > 0) && (master->player))
            mo = P_RoughMonsterSearch(master, 20);
        else
            mo = P_RoughMonsterSearch(actor, 20);
        actor->target = mo;
    }

    if(!actor->target) // Normal monster search.
    {
        for(think = thinkercap.next; think != &thinkercap && think;
            think = think->next)
        {
            if(think->function != P_MobjThinker)
                continue;

            mo = (mobj_t *) think;
            if(!(mo->flags & MF_COUNTKILL))
                continue;

            if(mo->health <= 0)
                continue;

            if(!(mo->flags & MF_SHOOTABLE))
                continue;

            dist = P_ApproxDistance(actor->pos[VX] - mo->pos[VX],
                                    actor->pos[VY] - mo->pos[VY]);

            if(dist > MINOTAUR_LOOK_DIST)
                continue;

            if((mo == master) || (mo == actor))
                continue;

            if((mo->type == MT_MINOTAUR) && (mo->tracer == actor->tracer))
                continue;

            actor->target = mo;
            break; // Found mobj to attack.
        }
    }

    if(actor->target)
    {
        P_SetMobjStateNF(actor, S_MNTR_WALK1);
    }
    else
    {
        P_SetMobjStateNF(actor, S_MNTR_ROAM1);
    }
}

void C_DECL A_MinotaurChase(mobj_t *actor)
{
    unsigned int *starttime = (unsigned int *) actor->args;

    actor->flags &= ~MF_SHADOW; // In case pain caused him to.
    actor->flags &= ~MF_ALTSHADOW;  // Skip his fade in.

    if((leveltime - *starttime) >= MAULATORTICS)
    {
        P_DamageMobj(actor, NULL, NULL, 10000);
        return;
    }

    if(P_Random() < 30)
        A_MinotaurLook(actor);  // Adjust to closest target.

    if(!actor->target || (actor->target->health <= 0) ||
       !(actor->target->flags & MF_SHOOTABLE))
    {   // Look for a new target.
        P_SetMobjState(actor, S_MNTR_LOOK1);
        return;
    }

    FaceMovementDirection(actor);
    actor->reactiontime = 0;

    // Melee attack.
    if(actor->info->meleestate && P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attacksound)
            S_StartSound(actor->info->attacksound, actor);

        P_SetMobjState(actor, actor->info->meleestate);
        return;
    }

    // Missile attack.
    if(actor->info->missilestate && P_CheckMissileRange(actor))
    {
        P_SetMobjState(actor, actor->info->missilestate);
        return;
    }

    // Chase towards target.
    if(!P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    // Active sound.
    if(actor->info->activesound && P_Random() < 6)
        S_StartSound(actor->info->activesound, actor);
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
        P_DamageMobj(actor->target, actor, actor, HITDICE(4));
    }
}

/**
 * Minotaur: Choose a missile attack.
 */
void C_DECL A_MinotaurDecide(mobj_t *actor)
{
#define MNTR_CHARGE_SPEED (23*FRACUNIT)

    angle_t angle;
    mobj_t *target = actor->target;
    int     dist;

    if(!target)
        return;
    dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                            actor->pos[VY] - target->pos[VY]);

    if(target->pos[VZ] + FLT2FIX(target->height) > actor->pos[VZ] &&
       target->pos[VZ] + FLT2FIX(target->height) < actor->pos[VZ] + FLT2FIX(actor->height) &&
       dist < 16 * 64 * FRACUNIT && dist > 1 * 64 * FRACUNIT &&
       P_Random() < 230)
    {   // Charge attack.
        // Don't call the state function right away.
        P_SetMobjStateNF(actor, S_MNTR_ATK4_1);
        actor->flags |= MF_SKULLFLY;
        A_FaceTarget(actor);

        angle = actor->angle >> ANGLETOFINESHIFT;
        actor->mom[MX] = FixedMul(MNTR_CHARGE_SPEED, finecosine[angle]);
        actor->mom[MY] = FixedMul(MNTR_CHARGE_SPEED, finesine[angle]);
        actor->args[4] = 35 / 2; // Charge duration.
    }
    else if(target->pos[VZ] == FLT2FIX(target->floorz) &&
            dist < 9 * 64 * FRACUNIT && P_Random() < 100)
    {   // Floor fire attack.
        P_SetMobjState(actor, S_MNTR_ATK3_1);
        actor->special2 = 0;
    }
    else
    {   // Swing attack.
        A_FaceTarget(actor);
        // Don't need to call P_SetMobjState because the current state
        // falls through to the swing attack.
    }

#undef MNTR_CHARGE_SPEED
}

/**
 * Minotaur: Charge attack.
 */
void C_DECL A_MinotaurCharge(mobj_t *actor)
{
    mobj_t     *puff;

    if(!actor->target)
        return;

    if(actor->args[4] > 0)
    {
        puff = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                           MT_PUNCHPUFF);
        puff->mom[MZ] = 2 * FRACUNIT;
        actor->args[4]--;
    }
    else
    {
        actor->flags &= ~MF_SKULLFLY;
        P_SetMobjState(actor, actor->info->seestate);
    }
}

/**
 * Minotaur: Swing attack.
 */
void C_DECL A_MinotaurAtk2(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;
    fixed_t     mom[MZ];

    if(!actor->target)
        return;

    S_StartSound(SFX_MAULATOR_HAMMER_SWING, actor);
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3));
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX1);
    if(mo)
    {
        mom[MZ] = mo->mom[MZ];
        angle = mo->angle;
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle - (ANG45 / 8), mom[MZ]);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle + (ANG45 / 8), mom[MZ]);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle - (ANG45 / 16), mom[MZ]);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle + (ANG45 / 16), mom[MZ]);
    }
}

/**
 * Minotaur: Floor fire attack.
 */
void C_DECL A_MinotaurAtk3(mobj_t *actor)
{
    mobj_t     *mo;
    player_t   *player;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3));
        if((player = actor->target->player) != NULL)
        {   // Squish the player.
            player->plr->deltaviewheight = -16;
        }
    }
    else
    {
        mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX2);
        if(mo != NULL)
        {
            S_StartSound(SFX_MAULATOR_HAMMER_HIT, mo);
        }
    }

    if(P_Random() < 192 && actor->special2 == 0)
    {
        P_SetMobjState(actor, S_MNTR_ATK3_4);
        actor->special2 = 1;
    }
}

void C_DECL A_MntrFloorFire(mobj_t *actor)
{
    mobj_t     *mo;

    actor->pos[VZ] = FLT2FIX(actor->floorz);
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - P_Random()) << 10),
                     actor->pos[VY] + ((P_Random() - P_Random()) << 10),
                     ONFLOORZ, MT_MNTRFX3);
    mo->target = actor->target;
    mo->mom[MX] = 1; // Force block checking.
    P_CheckMissileSpawn(mo);
}

void C_DECL A_Scream(mobj_t *actor)
{
    int         sound;

    S_StopSound(0, actor);
    if(actor->player)
    {
        if(actor->player->morphTics)
        {
            S_StartSound(actor->info->deathsound, actor);
        }
        else
        {
            // Handle the different player death screams.
            if(actor->mom[MZ] <= -39 * FRACUNIT)
            {   // Falling splat.
                sound = SFX_PLAYER_FALLING_SPLAT;
            }
            else if(actor->health > -50)
            {   // Normal death sound.
                switch(actor->player->class)
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
                switch(actor->player->class)
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
                switch(actor->player->class)
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
        S_StartSound(actor->info->deathsound, actor);
    }
}

void C_DECL A_NoBlocking(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;
}

void C_DECL A_Explode(mobj_t *actor)
{
    int         damage;
    int         distance;
    boolean     damageSelf;

    damage = 128;
    distance = 128;
    damageSelf = true;
    switch(actor->type)
    {
    case MT_FIREBOMB: // Time Bombs.
        actor->pos[VZ] += 32 * FRACUNIT;
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
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz) + (distance << FRACBITS) &&
       actor->type != MT_POISONCLOUD)
    {
        P_HitFloor(actor);
    }
}

/**
 * Kill all monsters.
 */
int P_Massacre(void)
{
    int         count;
    mobj_t     *mo;
    thinker_t  *think;

    // Only massacre when in a level.
    if(G_GetGameState() != GS_LEVEL)
        return 0;

    count = 0;
    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;
        if((mo->flags & MF_COUNTKILL) && (mo->health > 0))
        {
            mo->flags2 &= ~(MF2_NONSHOOTABLE + MF2_INVULNERABLE);
            mo->flags |= MF_SHOOTABLE;
            P_DamageMobj(mo, NULL, NULL, 10000);
            count++;
        }
    }
    return count;
}

void C_DECL A_SkullPop(mobj_t *actor)
{
    mobj_t     *mo;
    player_t   *player;

    if(!actor->player)
        return;

    actor->flags &= ~MF_SOLID;
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ] + 48 * FRACUNIT,
                     MT_BLOODYSKULL);

    mo->mom[MX] = (P_Random() - P_Random()) << 9;
    mo->mom[MY] = (P_Random() - P_Random()) << 9;
    mo->mom[MZ] = FRACUNIT * 2 + (P_Random() << 6);

    // Attach player mobj to bloody skull
    player = actor->player;
    actor->player = NULL;
    actor->dplayer = NULL;
    actor->special1 = player->class;
    mo->player = player;
    mo->dplayer = player->plr;
    mo->health = actor->health;
    mo->angle = actor->angle;
    player->plr->mo = mo;
    player->plr->lookdir = 0;
    player->damagecount = 32;
}

void C_DECL A_CheckSkullFloor(mobj_t *actor)
{
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz))
    {
        P_SetMobjState(actor, S_BLOODYSKULLX1);
        S_StartSound(SFX_DRIP, actor);
    }
}

void C_DECL A_CheckSkullDone(mobj_t *actor)
{
    if(actor->special2 == 666)
    {
        P_SetMobjState(actor, S_BLOODYSKULLX2);
    }
}

void C_DECL A_CheckBurnGone(mobj_t *actor)
{
    if(actor->special2 == 666)
    {
        P_SetMobjState(actor, S_PLAY_FDTH20);
    }
}

void C_DECL A_FreeTargMobj(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;
    mo->pos[VZ] = FLT2FIX(mo->ceilingz + 4);

    mo->flags &=
        ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_SOLID | MF_COUNTKILL);
    mo->flags |= MF_CORPSE | MF_DROPOFF | MF_NOGRAVITY;
    mo->flags2 &= ~(MF2_PASSMOBJ | MF2_LOGRAV);
    mo->flags2 |= MF2_DONTDRAW;
    mo->player = NULL;
    mo->dplayer = NULL;
    mo->health = -1000; // Don't resurrect.
}

/**
 * Throw another corpse on the queue.
 */
void C_DECL A_QueueCorpse(mobj_t *actor)
{
    mobj_t *corpse;

    if(corpseQueueSlot >= CORPSEQUEUESIZE)
    {                           // Too many corpses - remove an old one
        corpse = corpseQueue[corpseQueueSlot % CORPSEQUEUESIZE];
        if(corpse)
            P_RemoveMobj(corpse);
    }
    corpseQueue[corpseQueueSlot % CORPSEQUEUESIZE] = actor;
    corpseQueueSlot++;
}

/**
 * Remove a mobj from the queue (for resurrection).
 */
void C_DECL A_DeQueueCorpse(mobj_t *actor)
{
    int         slot;

    for(slot = 0; slot < CORPSEQUEUESIZE; ++slot)
    {
        if(corpseQueue[slot] == actor)
        {
            corpseQueue[slot] = NULL;
            break;
        }
    }
}

void P_InitCreatureCorpseQueue(boolean corpseScan)
{
    thinker_t *think;
    mobj_t *mo;

    // Initialize queue
    corpseQueueSlot = 0;
    memset(corpseQueue, 0, sizeof(mobj_t *) * CORPSEQUEUESIZE);

    if(!corpseScan)
        return;

    // Search mobj list for corpses and place them in this queue.
    for(think = thinkercap.next; think != &thinkercap && think;
        think = think->next)
    {
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;
        if(!(mo->flags & MF_CORPSE))
            continue; // Must be a corpse.
        if(mo->flags & MF_ICECORPSE)
            continue; // Not ice corpses.

        // Only corpses that call A_QueueCorpse from death routine.
        switch(mo->type)
        {
        case MT_CENTAUR:
        case MT_CENTAURLEADER:
        case MT_DEMON:
        case MT_DEMON2:
        case MT_WRAITH:
        case MT_WRAITHB:
        case MT_BISHOP:
        case MT_ETTIN:
        case MT_PIG:
        case MT_CENTAUR_SHIELD:
        case MT_CENTAUR_SWORD:
        case MT_DEMONCHUNK1:
        case MT_DEMONCHUNK2:
        case MT_DEMONCHUNK3:
        case MT_DEMONCHUNK4:
        case MT_DEMONCHUNK5:
        case MT_DEMON2CHUNK1:
        case MT_DEMON2CHUNK2:
        case MT_DEMON2CHUNK3:
        case MT_DEMON2CHUNK4:
        case MT_DEMON2CHUNK5:
        case MT_FIREDEMON_SPLOTCH1:
        case MT_FIREDEMON_SPLOTCH2:
            A_QueueCorpse(mo); // Add corpse to queue.
            break;

        default:
            break;
        }
    }
}

void C_DECL A_AddPlayerCorpse(mobj_t *actor)
{
    if(bodyqueslot >= BODYQUESIZE)
    {   // Too many player corpses - remove an old one.
        P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]);
    }
    bodyque[bodyqueslot % BODYQUESIZE] = actor;
    bodyqueslot++;
}

void C_DECL A_SerpentUnHide(mobj_t *actor)
{
    actor->flags2 &= ~MF2_DONTDRAW;
    actor->floorclip = 24;
}

void C_DECL A_SerpentHide(mobj_t *actor)
{
    actor->flags2 |= MF2_DONTDRAW;
    actor->floorclip = 0;
}

void C_DECL A_SerpentChase(mobj_t *actor)
{
    int         delta;
    int         oldpos[3], oldFloor;

    if(actor->reactiontime)
    {
        actor->reactiontime--;
    }

    // Modify target threshold.
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gameskill == SM_NIGHTMARE || (fastMonsters))
    {   // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
        }
    }

    // Turn towards movement direction if not there yet.
    if(actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);
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
        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if(actor->info->meleestate && P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attacksound)
        {
            S_StartSound(actor->info->attacksound, actor);
        }
        P_SetMobjState(actor, actor->info->meleestate);
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
    memcpy(oldpos, actor->pos, sizeof(oldpos));

    oldFloor = P_GetIntp(actor->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_TEXTURE);
    if(--actor->movecount < 0 || !P_Move(actor))
    {
        P_NewChaseDir(actor);
    }

    if(P_GetIntp(actor->subsector, DMU_SECTOR_OF_SUBSECTOR | DMU_FLOOR_TEXTURE) != oldFloor)
    {
        P_TryMove(actor, oldpos[VX], oldpos[VY]);
        P_NewChaseDir(actor);
    }

    // Make active sound.
    if(actor->info->activesound && P_Random() < 3)
    {
        S_StartSound(actor->info->activesound, actor);
    }
}

void C_DECL A_SpeedFade(mobj_t *actor)
{
    actor->flags |= MF_SHADOW;
    actor->flags &= ~MF_ALTSHADOW;
    actor->sprite = actor->target->sprite;
}

/**
 * Raises the hump above the surface by raising the floorclip level.
 */
void C_DECL A_SerpentRaiseHump(mobj_t *actor)
{
    actor->floorclip -= 4;
}

void C_DECL A_SerpentLowerHump(mobj_t *actor)
{
    actor->floorclip += 4;
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
            P_SetMobjState(actor, S_SERPENT_SURFACE1);
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
            P_SetMobjState(actor, S_SERPENT_SURFACE1);
        }
        else
        {
            P_SetMobjState(actor, S_SERPENT_HUMP1);
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
    int         delta;

    if(actor->reactiontime)
    {
        actor->reactiontime--;
    }

    // Modify target threshold.
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gameskill == SM_NIGHTMARE || (fastMonsters))
    {   // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
        }
    }

    // Turn towards movement direction if not there yet.
    if(actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);
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
        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Check for melee attack.
    if(actor->info->meleestate && P_CheckMeleeRange(actor, false))
    {
        if(actor->info->attacksound)
        {
            S_StartSound(actor->info->attacksound, actor);
        }
        P_SetMobjState(actor, S_SERPENT_ATK1);
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
    if(--actor->movecount < 0 || !P_Move(actor))
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
            P_SetMobjState(actor, S_SERPENT_ATK1);
            return;
        }
    }

    if(P_CheckMeleeRange(actor, true))
    {
        P_SetMobjState(actor, S_SERPENT_WALK1);
    }
    else if(P_CheckMeleeRange(actor, false))
    {
        if(P_Random() < 32)
        {
            P_SetMobjState(actor, S_SERPENT_WALK1);
        }
        else
        {
            P_SetMobjState(actor, S_SERPENT_ATK1);
        }
    }
}

void C_DECL A_SerpentChooseAttack(mobj_t *actor)
{
    if(!actor->target || P_CheckMeleeRange(actor, false))
        return;

    if(actor->type == MT_SERPENTLEADER)
    {
        P_SetMobjState(actor, S_SERPENT_MISSILE1);
    }
}

void C_DECL A_SerpentMeleeAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5));
        S_StartSound(SFX_SERPENT_MELEEHIT, actor);
    }

    if(P_Random() < 96)
    {
        A_SerpentCheckForAttack(actor);
    }
}

void C_DECL A_SerpentMissileAttack(mobj_t *actor)
{
    mobj_t     *mo;

    if(!actor->target)
        return;

    mo = P_SpawnMissile(actor, actor->target, MT_SERPENTFX);
}

void C_DECL A_SerpentHeadPop(mobj_t *actor)
{
    P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                actor->pos[VZ] + 45 * FRACUNIT,
                MT_SERPENT_HEAD);
}

void C_DECL A_SerpentSpawnGibs(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) << 12),
                     actor->pos[VY] + ((P_Random() - 128) << 12),
                     FLT2FIX(actor->floorz + 1), MT_SERPENT_GIB1);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 6;
        mo->mom[MY] = (P_Random() - 128) << 6;
        mo->floorclip = 6;
    }
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) << 12),
                     actor->pos[VY] + ((P_Random() - 128) << 12),
                     FLT2FIX(actor->floorz + 1), MT_SERPENT_GIB2);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 6;
        mo->mom[MY] = (P_Random() - 128) << 6;
        mo->floorclip = 6;
    }
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) << 12),
                     actor->pos[VY] + ((P_Random() - 128) << 12),
                     FLT2FIX(actor->floorz + 1), MT_SERPENT_GIB3);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 6;
        mo->mom[MY] = (P_Random() - 128) << 6;
        mo->floorclip = 6;
    }
}

void C_DECL A_FloatGib(mobj_t *actor)
{
    actor->floorclip -= 1;
}

void C_DECL A_SinkGib(mobj_t *actor)
{
    actor->floorclip += 1;
}

void C_DECL A_DelayGib(mobj_t *actor)
{
    actor->tics -= P_Random() >> 2;
}

void C_DECL A_SerpentHeadCheck(mobj_t *actor)
{
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz))
    {
        if(P_GetThingFloorType(actor) >= FLOOR_LIQUID)
        {
            P_HitFloor(actor);
            P_SetMobjState(actor, S_NULL);
        }
        else
        {
            P_SetMobjState(actor, S_SERPENT_HEAD_X1);
        }
    }
}

void C_DECL A_CentaurAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, P_Random() % 7 + 3);
    }
}

void C_DECL A_CentaurAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    P_SpawnMissile(actor, actor->target, MT_CENTAUR_FX);
    S_StartSound(SFX_CENTAURLEADER_ATTACK, actor);
}

/**
 * Spawn shield/sword sprites when the centaur pulps.
 */
void C_DECL A_CentaurDropStuff(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_CENTAUR_SHIELD);
    if(mo)
    {
        angle = actor->angle + ANG90;
        mo->mom[MZ] = FRACUNIT * 8 + (P_Random() << 10);
        mo->mom[MX] =
            FixedMul(((P_Random() - 128) << 11) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul(((P_Random() - 128) << 11) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_CENTAUR_SWORD);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = FRACUNIT * 8 + (P_Random() << 10);
        mo->mom[MX] =
            FixedMul(((P_Random() - 128) << 11) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul(((P_Random() - 128) << 11) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
}

void C_DECL A_CentaurDefend(mobj_t *actor)
{
    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor, false) && P_Random() < 32)
    {
        A_UnSetInvulnerable(actor);
        P_SetMobjState(actor, actor->info->meleestate);
    }
}

void C_DECL A_BishopAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4));
        return;
    }
    actor->special1 = (P_Random() & 3) + 5;
}

/**
 * Spawns one of a string of bishop missiles.
 */
void C_DECL A_BishopAttack2(mobj_t *actor)
{
    mobj_t     *mo;

    if(!actor->target || !actor->special1)
    {
        actor->special1 = 0;
        P_SetMobjState(actor, S_BISHOP_WALK1);
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, MT_BISH_FX);
    if(mo)
    {
        mo->tracer = actor->target;
        mo->special2 = 16; // High word == x/y, Low word == z.
    }
    actor->special1--;
}

void C_DECL A_BishopMissileWeave(mobj_t *actor)
{
    fixed_t     newpos[3];
    int         weaveXY, weaveZ;
    int         angle;

    weaveXY = actor->special2 >> 16;
    weaveZ = actor->special2 & 0xFFFF;
    angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;

    memcpy(newpos, actor->pos, sizeof(newpos));
    newpos[VX] -= FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 1);
    newpos[VY] -= FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 1);

    weaveXY = (weaveXY + 2) & 63;
    newpos[VX] += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 1);
    newpos[VY] += FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 1);

    P_TryMove(actor, newpos[VX], newpos[VY]);

    actor->pos[VZ] -= FloatBobOffsets[weaveZ];
    weaveZ = (weaveZ + 2) & 63;
    actor->pos[VZ] += FloatBobOffsets[weaveZ];
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
        P_SetMobjState(actor, S_BISHOP_BLUR1);
    }
}

void C_DECL A_BishopDoBlur(mobj_t *actor)
{
    actor->special1 = (P_Random() & 3) + 3; // Random number of blurs.
    if(P_Random() < 120)
    {
        P_ThrustMobj(actor, actor->angle + ANG90, 11 * FRACUNIT);
    }
    else if(P_Random() > 125)
    {
        P_ThrustMobj(actor, actor->angle - ANG90, 11 * FRACUNIT);
    }
    else
    {   // Thrust forward
        P_ThrustMobj(actor, actor->angle, 11 * FRACUNIT);
    }
    S_StartSound(SFX_BISHOP_BLUR, actor);
}

void C_DECL A_BishopSpawnBlur(mobj_t *actor)
{
    mobj_t *mo;

    if(!--actor->special1)
    {
        actor->mom[MX] = 0;
        actor->mom[MY] = 0;
        if(P_Random() > 96)
        {
            P_SetMobjState(actor, S_BISHOP_WALK1);
        }
        else
        {
            P_SetMobjState(actor, S_BISHOP_ATK1);
        }
    }

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_BISHOPBLUR);
    if(mo)
    {
        mo->angle = actor->angle;
    }
}

void C_DECL A_BishopChase(mobj_t *actor)
{
    actor->pos[VZ] -= FloatBobOffsets[actor->special2] >> 1;
    actor->special2 = (actor->special2 + 4) & 63;
    actor->pos[VZ] += FloatBobOffsets[actor->special2] >> 1;
}

void C_DECL A_BishopPuff(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 40 * FRACUNIT,
                     MT_BISHOP_PUFF);
    if(mo)
    {
        mo->mom[MZ] = FRACUNIT / 2;
    }
}

void C_DECL A_BishopPainBlur(mobj_t *actor)
{
    mobj_t     *mo;

    if(P_Random() < 64)
    {
        P_SetMobjState(actor, S_BISHOP_BLUR1);
        return;
    }
 
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - P_Random()) << 12),
                     actor->pos[VY] + ((P_Random() - P_Random()) << 12),
                     actor->pos[VZ] + ((P_Random() - P_Random()) << 11),
                     MT_BISHOPPAINBLUR);
    if(mo)
    {
        mo->angle = actor->angle;
    }
}

static void DragonSeek(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int         dir;
    int         dist;
    angle_t     delta;
    angle_t     angle;
    mobj_t     *target;
    int         search;
    int         i;
    int         bestArg;
    angle_t     bestAngle;
    angle_t     angleToSpot, angleToTarget;
    mobj_t     *mo;

    target = actor->tracer;
    if(target == NULL)
        return;

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir)
    {   // Turn clockwise.
        actor->angle += delta;
    }
    else
    {   // Turn counter clockwise.
        actor->angle -= delta;
    }

    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(actor->info->speed, finecosine[angle]);
    actor->mom[MY] = FixedMul(actor->info->speed, finesine[angle]);

    if(actor->pos[VZ] + FLT2FIX(actor->height) < target->pos[VZ] ||
       target->pos[VZ] + FLT2FIX(target->height) < actor->pos[VZ])
    {
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] = (target->pos[VZ] - actor->pos[VZ]) / dist;
    }
    else
    {
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
    }

    if(target->flags & MF_SHOOTABLE && P_Random() < 64)
    {   // Attack the destination mobj if it's attackable.
        mobj_t *oldTarget;

        if(abs
           (actor->angle -
            R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            target->pos[VX], target->pos[VY])) < ANGLE_45 / 2)
        {
            oldTarget = actor->target;
            actor->target = target;
            if(P_CheckMeleeRange(actor, false))
            {
                P_DamageMobj(actor->target, actor, actor, HITDICE(10));
                S_StartSound(SFX_DRAGON_ATTACK, actor);
            }
            else if(P_Random() < 128 && P_CheckMissileRange(actor))
            {
                P_SpawnMissile(actor, target, MT_DRAGON_FX);
                S_StartSound(SFX_DRAGON_ATTACK, actor);
            }
            actor->target = oldTarget;
        }
    }

    if(dist < 4)
    {   // Hit the target thing.
        if(actor->target && P_Random() < 200)
        {
            bestArg = -1;
            bestAngle = ANGLE_MAX;
            angleToTarget =
                R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                actor->target->pos[VX], actor->target->pos[VY]);
            for(i = 0; i < 5; ++i)
            {
                if(!target->args[i])
                    continue;

                search = -1;
                mo = P_FindMobjFromTID(target->args[i], &search);
                angleToSpot =
                    R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                    mo->pos[VX], mo->pos[VY]);
                if(abs(angleToSpot - angleToTarget) < (int) bestAngle)
                {
                    bestAngle = abs(angleToSpot - angleToTarget);
                    bestArg = i;
                }
            }

            if(bestArg != -1)
            {
                search = -1;
                actor->tracer =
                    P_FindMobjFromTID(target->args[bestArg], &search);
            }
        }
        else
        {
            do
            {
                i = (P_Random() >> 2) % 5;
            } while(!target->args[i]);

            search = -1;
            actor->tracer =
                P_FindMobjFromTID(target->args[i], &search);
        }
    }
}

void C_DECL A_DragonInitFlight(mobj_t *actor)
{
    int         search;

    search = -1;
    do
    {   // find the first tid identical to the dragon's tid
        actor->tracer = P_FindMobjFromTID(actor->tid, &search);
        if(search == -1)
        {
            P_SetMobjState(actor, actor->info->spawnstate);
            return;
        }
    } while(actor->tracer == actor);

    P_RemoveMobjFromTIDList(actor);
}

void C_DECL A_DragonFlight(mobj_t *actor)
{
    angle_t     angle;

    DragonSeek(actor, 4 * ANGLE_1, 8 * ANGLE_1);
    if(actor->target)
    {
        if(!(actor->target->flags & MF_SHOOTABLE))
        {    // Target died.
            actor->target = NULL;
            return;
        }

        angle =
            R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                            actor->target->pos[VX], actor->target->pos[VY]);
        if(abs(actor->angle - angle) < ANGLE_45 / 2 &&
           P_CheckMeleeRange(actor, false))
        {
            P_DamageMobj(actor->target, actor, actor, HITDICE(8));
            S_StartSound(SFX_DRAGON_ATTACK, actor);
        }
        else if(abs(actor->angle - angle) <= ANGLE_1 * 20)
        {
            P_SetMobjState(actor, actor->info->missilestate);
            S_StartSound(SFX_DRAGON_ATTACK, actor);
        }
    }
    else
    {
        P_LookForPlayers(actor, true);
    }
}

void C_DECL A_DragonFlap(mobj_t *actor)
{
    A_DragonFlight(actor);
    if(P_Random() < 240)
    {
        S_StartSound(SFX_DRAGON_WINGFLAP, actor);
    }
    else
    {
        S_StartSound(actor->info->activesound, actor);
    }
}

void C_DECL A_DragonAttack(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMissile(actor, actor->target, MT_DRAGON_FX);
}

void C_DECL A_DragonFX2(mobj_t *actor)
{
    mobj_t     *mo;
    int         i;
    int         delay;

    delay = 16 + (P_Random() >> 3);
    for(i = 1 + (P_Random() & 3); i; i--)
    {
        mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) << 14),
                         actor->pos[VY] + ((P_Random() - 128) << 14),
                         actor->pos[VZ] + ((P_Random() - 128) << 12),
                         MT_DRAGON_FX2);
        if(mo)
        {
            mo->tics = delay + (P_Random() & 3) * i * 2;
            mo->target = actor->target;
        }
    }
}

void C_DECL A_DragonPain(mobj_t *actor)
{
    A_Pain(actor);
    if(!actor->tracer)
    {   // No destination spot yet.
        P_SetMobjState(actor, S_DRAGON_INIT);
    }
}

void C_DECL A_DragonCheckCrash(mobj_t *actor)
{
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz))
    {
        P_SetMobjState(actor, S_DRAGON_CRASH1);
    }
}

/**
 * Demon: Melee attack.
 */
void C_DECL A_DemonAttack1(mobj_t *actor)
{
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2));
    }
}

/**
 * Demon: Missile attack.
 */
void C_DECL A_DemonAttack2(mobj_t *actor)
{
    mobj_t     *mo;
    int         fireBall;

    if(actor->type == MT_DEMON)
    {
        fireBall = MT_DEMONFX1;
    }
    else
    {
        fireBall = MT_DEMON2FX1;
    }

    mo = P_SpawnMissile(actor, actor->target, fireBall);
    if(mo)
    {
        mo->pos[VZ] += 30 * FRACUNIT;
        S_StartSound(SFX_DEMON_MISSILE_FIRE, actor);
    }
}

void C_DECL A_DemonDeath(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMONCHUNK1);
    if(mo)
    {
        angle = actor->angle + ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMONCHUNK2);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMONCHUNK3);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMONCHUNK4);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMONCHUNK5);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
}

void C_DECL A_Demon2Death(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMON2CHUNK1);
    if(mo)
    {
        angle = actor->angle + ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMON2CHUNK2);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMON2CHUNK3);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMON2CHUNK4);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + 45 * FRACUNIT,
                     MT_DEMON2CHUNK5);
    if(mo)
    {
        angle = actor->angle - ANG90;
        mo->mom[MZ] = 8 * FRACUNIT;
        mo->mom[MX] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finecosine[angle >> ANGLETOFINESHIFT]);
        mo->mom[MY] =
            FixedMul((P_Random() << 10) + FRACUNIT,
                     finesine[angle >> ANGLETOFINESHIFT]);
        mo->target = actor;
    }
}

/**
 * Sink a mobj incrementally into the floor.
 */
boolean A_SinkMobj(mobj_t *actor)
{
    if(actor->floorclip < FIX2FLT(actor->info->height))
    {
        switch(actor->type)
        {
        case MT_THRUSTFLOOR_DOWN:
        case MT_THRUSTFLOOR_UP:
            actor->floorclip += 6;
            break;

        default:
            actor->floorclip += 1;
            break;
        }

        return false;
    }

    return true;
}

/**
 * Raise a mobj incrementally from the floor to.
 */
boolean A_RaiseMobj(mobj_t *actor)
{
    int         done = true;

    // Raise a mobj from the ground.
    if(actor->floorclip > 0)
    {
        switch (actor->type)
        {
        case MT_WRAITHB:
            actor->floorclip -= 2;
            break;

        case MT_THRUSTFLOOR_DOWN:
        case MT_THRUSTFLOOR_UP:
            actor->floorclip -= actor->special2;
            break;

        default:
            actor->floorclip -= 2;
            break;
        }

        if(actor->floorclip <= 0)
        {
            actor->floorclip = 0;
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

void C_DECL A_WraithInit(mobj_t *actor)
{
    actor->pos[VZ] += 48 << FRACBITS;
    actor->special1 = 0; // Index into floatbob.
}

void C_DECL A_WraithRaiseInit(mobj_t *actor)
{
    actor->flags2 &= ~MF2_DONTDRAW;
    actor->flags2 &= ~MF2_NONSHOOTABLE;
    actor->flags |= MF_SHOOTABLE | MF_SOLID;
    actor->floorclip = FIX2FLT(actor->info->height);
}

void C_DECL A_WraithRaise(mobj_t *actor)
{
    if(A_RaiseMobj(actor))
    {
        // Reached it's target height.
        P_SetMobjState(actor, S_WRAITH_CHASE1);
    }

    P_SpawnDirt(actor, actor->radius);
}

void C_DECL A_WraithMelee(mobj_t *actor)
{
    int         amount;

    // Steal health from target and give to player.
    if(P_CheckMeleeRange(actor, false) && (P_Random() < 220))
    {
        amount = HITDICE(2);
        P_DamageMobj(actor->target, actor, actor, amount);
        actor->health += amount;
    }
}

void C_DECL A_WraithMissile(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMissile(actor, actor->target, MT_WRAITHFX1);
    if(mo)
    {
        S_StartSound(SFX_WRAITH_MISSILE_FIRE, actor);
    }
}

/**
 * Wraith: Spawn sparkle tail of missile.
 */
void C_DECL A_WraithFX2(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     angle;
    int         i;

    for(i = 0; i < 2; ++i)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                         MT_WRAITHFX2);
        if(mo)
        {
            if(P_Random() < 128)
            {
                angle = actor->angle + (P_Random() << 22);
            }
            else
            {
                angle = actor->angle - (P_Random() << 22);
            }
            mo->mom[MZ] = 0;
            mo->mom[MX] =
                FixedMul((P_Random() << 7) + FRACUNIT,
                         finecosine[angle >> ANGLETOFINESHIFT]);
            mo->mom[MY] =
                FixedMul((P_Random() << 7) + FRACUNIT,
                         finesine[angle >> ANGLETOFINESHIFT]);
            mo->target = actor;
            mo->floorclip = 10;
        }
    }
}

/**
 * Wraith: Spawn an FX3 around during attacks.
 */
void C_DECL A_WraithFX3(mobj_t *actor)
{
    mobj_t     *mo;
    int         numdropped = P_Random() % 15;
    int         i;

    for(i = 0; i < numdropped; ++i)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                         MT_WRAITHFX3);
        if(mo)
        {
            mo->pos[VX] += (P_Random() - 128) << 11;
            mo->pos[VY] += (P_Random() - 128) << 11;
            mo->pos[VZ] += (P_Random() << 10);
            mo->target = actor;
        }
    }
}

/**
 * Wraith: Spawn an FX4 during movement.
 */
void C_DECL A_WraithFX4(mobj_t *actor)
{
    mobj_t     *mo;
    int         chance = P_Random();
    int         spawn4, spawn5;

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
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                         MT_WRAITHFX4);
        if(mo)
        {
            mo->pos[VX] += (P_Random() - 128) << 12;
            mo->pos[VY] += (P_Random() - 128) << 12;
            mo->pos[VZ] += (P_Random() << 10);
            mo->target = actor;
        }
    }

    if(spawn5)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                         MT_WRAITHFX5);
        if(mo)
        {
            mo->pos[VX] += (P_Random() - 128) << 11;
            mo->pos[VY] += (P_Random() - 128) << 11;
            mo->pos[VZ] += (P_Random() << 10);
            mo->target = actor;
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

    actor->pos[VZ] += FloatBobOffsets[weaveindex];
    actor->special1 = (weaveindex + 2) & 63;

    A_Chase(actor);
    A_WraithFX4(actor);
}

void C_DECL A_EttinAttack(mobj_t *actor)
{
    if(P_CheckMeleeRange(actor, false))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2));
    }
}

void C_DECL A_DropMace(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] + FLT2FIX(actor->height /2),
                     MT_ETTIN_MACE);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 11;
        mo->mom[MY] = (P_Random() - 128) << 11;
        mo->mom[MZ] = FRACUNIT * 10 + (P_Random() << 10);
        mo->target = actor;
    }
}

/**
 * Fire Demon variables.
 *
 * special1         Index into floatbob.
 * special2         whether strafing or not.
 */

void C_DECL A_FiredSpawnRock(mobj_t *actor)
{
    mobj_t     *mo;
    fixed_t     pos[3];
    int         rtype = 0;

    switch(P_Random() % 5)
    {
    case 0:
        rtype = MT_FIREDEMON_FX1;
        break;

    case 1:
        rtype = MT_FIREDEMON_FX2;
        break;

    case 2:
        rtype = MT_FIREDEMON_FX3;
        break;

    case 3:
        rtype = MT_FIREDEMON_FX4;
        break;

    case 4:
        rtype = MT_FIREDEMON_FX5;
        break;
    }

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += ((P_Random() - 128) << 12);
    pos[VY] += ((P_Random() - 128) << 12);
    pos[VZ] += ((P_Random()) << 11);

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], rtype);
    if(mo)
    {
        mo->target = actor;
        mo->mom[MX] = (P_Random() - 128) << 10;
        mo->mom[MY] = (P_Random() - 128) << 10;
        mo->mom[MZ] = (P_Random() << 10);
        mo->special1 = 2; // Number of bounces.
    }

    // Initialize fire demon.
    actor->special2 = 0;
    actor->flags &= ~MF_JUSTATTACKED;
}

void C_DECL A_FiredRocks(mobj_t *actor)
{
    A_FiredSpawnRock(actor);
    A_FiredSpawnRock(actor);
    A_FiredSpawnRock(actor);
    A_FiredSpawnRock(actor);
    A_FiredSpawnRock(actor);
}

void C_DECL A_FiredAttack(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMissile(actor, actor->target, MT_FIREDEMON_FX6);
    if(mo)
        S_StartSound(SFX_FIRED_ATTACK, actor);
}

void C_DECL A_SmBounce(mobj_t *actor)
{
    // Give some more momentum (x,y,&z).
    actor->pos[VZ] = FLT2FIX(actor->floorz + 1);
    actor->mom[MZ] = (2 * FRACUNIT) + (P_Random() << 10);
    actor->mom[MX] = P_Random() % 3 << FRACBITS;
    actor->mom[MY] = P_Random() % 3 << FRACBITS;
}

void C_DECL A_FiredChase(mobj_t *actor)
{
#define FIREDEMON_ATTACK_RANGE  64*8*FRACUNIT

    int         weaveindex = actor->special1;
    mobj_t     *target = actor->target;
    angle_t     ang;
    fixed_t     dist;

    if(actor->reactiontime)
        actor->reactiontime--;
    if(actor->threshold)
        actor->threshold--;

    // Float up and down.
    actor->pos[VZ] += FloatBobOffsets[weaveindex];
    actor->special1 = (weaveindex + 2) & 63;

    // Insure it stays above certain height.
    if(actor->pos[VZ] < FLT2FIX(actor->floorz + 64))
    {
        actor->pos[VZ] += 2 * FRACUNIT;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {   // Invalid target.
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
        dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                                actor->pos[VY] - target->pos[VY]);
        if(dist < FIREDEMON_ATTACK_RANGE)
        {
            if(P_Random() < 30)
            {
                ang =
                    R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                    target->pos[VX], target->pos[VY]);
                if(P_Random() < 128)
                    ang += ANGLE_90;
                else
                    ang -= ANGLE_90;
                ang >>= ANGLETOFINESHIFT;
                actor->mom[MX] = FixedMul(8 * FRACUNIT, finecosine[ang]);
                actor->mom[MY] = FixedMul(8 * FRACUNIT, finesine[ang]);
                actor->special2 = 3; // Strafe time.
            }
        }
    }

    FaceMovementDirection(actor);

    // Normal movement.
    if(!actor->special2)
    {
        if(--actor->movecount < 0 || !P_Move(actor))
        {
            P_NewChaseDir(actor);
        }
    }

    // Do missile attack.
    if(!(actor->flags & MF_JUSTATTACKED))
    {
        if(P_CheckMissileRange(actor) && (P_Random() < 20))
        {
            P_SetMobjState(actor, actor->info->missilestate);
            actor->flags |= MF_JUSTATTACKED;
            return;
        }
    }
    else
    {
        actor->flags &= ~MF_JUSTATTACKED;
    }

    // Make active sound.
    if(actor->info->activesound && P_Random() < 3)
    {
        S_StartSound(actor->info->activesound, actor);
    }

#undef FIREDEMON_ATTACK_RANGE
}

void C_DECL A_FiredSplotch(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_FIREDEMON_SPLOTCH1);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 11;
        mo->mom[MY] = (P_Random() - 128) << 11;
        mo->mom[MZ] = FRACUNIT * 3 + (P_Random() << 10);
    }

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_FIREDEMON_SPLOTCH2);
    if(mo)
    {
        mo->mom[MX] = (P_Random() - 128) << 11;
        mo->mom[MY] = (P_Random() - 128) << 11;
        mo->mom[MZ] = FRACUNIT * 3 + (P_Random() << 10);
    }
}

void C_DECL A_IceGuyLook(mobj_t *actor)
{
    fixed_t     dist;
    fixed_t     an;

    A_Look(actor);
    if(P_Random() < 64)
    {
        dist = ((P_Random() - 128) * actor->radius) >> 7;
        an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;

        P_SpawnMobj(actor->pos[VX] + FixedMul(dist, finecosine[an]),
                    actor->pos[VY] + FixedMul(dist, finesine[an]),
                    actor->pos[VZ] + 60 * FRACUNIT,
                    MT_ICEGUY_WISP1 + (P_Random() & 1));
    }
}

void C_DECL A_IceGuyChase(mobj_t *actor)
{
    fixed_t     dist;
    fixed_t     an;
    mobj_t     *mo;

    A_Chase(actor);
    if(P_Random() < 128)
    {
        dist = ((P_Random() - 128) * actor->radius) >> 7;
        an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;

        mo = P_SpawnMobj(actor->pos[VX] + FixedMul(dist, finecosine[an]),
                         actor->pos[VY] + FixedMul(dist, finesine[an]),
                         actor->pos[VZ] + 60 * FRACUNIT,
                         MT_ICEGUY_WISP1 + (P_Random() & 1));
        if(mo)
        {
            mo->mom[MX] = actor->mom[MX];
            mo->mom[MY] = actor->mom[MY];
            mo->mom[MZ] = actor->mom[MZ];
            mo->target = actor;
        }
    }
}

void C_DECL A_IceGuyAttack(mobj_t *actor)
{
    fixed_t     an;

    if(!actor->target)
        return;

    an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
    P_SpawnMissileXYZ(actor->pos[VX] + FixedMul(actor->radius >> 1, finecosine[an]),
                      actor->pos[VY] + FixedMul(actor->radius >> 1, finesine[an]),
                      actor->pos[VZ] + 40 * FRACUNIT, actor, actor->target,
                      MT_ICEGUY_FX);
    an = (actor->angle - ANG90) >> ANGLETOFINESHIFT;
    P_SpawnMissileXYZ(actor->pos[VX] + FixedMul(actor->radius >> 1, finecosine[an]),
                      actor->pos[VY] + FixedMul(actor->radius >> 1, finesine[an]),
                      actor->pos[VZ] + 40 * FRACUNIT, actor, actor->target,
                      MT_ICEGUY_FX);
    S_StartSound(actor->info->attacksound, actor);
}

void C_DECL A_IceGuyMissilePuff(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ] + 2 * FRACUNIT,
                     MT_ICEFX_PUFF);
}

void C_DECL A_IceGuyDie(mobj_t *actor)
{
    actor->mom[MX] = 0;
    actor->mom[MY] = 0;
    actor->mom[MZ] = 0;
    actor->height *= 2*2;
    A_FreezeDeathChunks(actor);
}

void C_DECL A_IceGuyMissileExplode(mobj_t *actor)
{
    mobj_t *mo;
    int     i;

    for(i = 0; i < 8; i++)
    {
        mo = P_SpawnMissileAngle(actor, MT_ICEGUY_FX2, i * ANG45,
                                 (int) (-0.3 * FRACUNIT));
        if(mo)
        {
            mo->target = actor->target;
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
void C_DECL A_SorcSpinBalls(mobj_t *actor)
{
    mobj_t     *mo;
    fixed_t     z;

    A_SlowBalls(actor);
    actor->args[0] = 0; // Currently no defense.
    actor->args[3] = SORC_NORMAL;
    actor->args[4] = SORCBALL_INITIAL_SPEED; // Initial orbit speed.
    actor->special1 = ANGLE_1;
    z = actor->pos[VZ] - FLT2FIX(actor->floorclip) + actor->info->height;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], z, MT_SORCBALL1);
    if(mo)
    {
        mo->target = actor;
        mo->special2 = SORCFX4_RAPIDFIRE_TIME;
    }

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], z, MT_SORCBALL2);
    if(mo)
        mo->target = actor;
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], z, MT_SORCBALL3);
    if(mo)
        mo->target = actor;
}

void C_DECL A_SorcBallOrbit(mobj_t *actor)
{
    fixed_t     pos[3];
    angle_t     angle = 0, baseangle;
    int         mode = actor->target->args[3];
    mobj_t     *parent = (mobj_t *) actor->target;
    int         dist = parent->radius - (actor->radius << 1);
    angle_t     prevangle = actor->special1;

    if(actor->target->health <= 0)
        P_SetMobjState(actor, actor->info->painstate);

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
    angle >>= ANGLETOFINESHIFT;

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
           (abs(angle - (parent->angle >> ANGLETOFINESHIFT)) < (30 << 5)))
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
                P_SetMobjStateNF(parent, S_SORC_ATTACK1);

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
                    P_SetMobjStateNF(parent, S_SORC_ATTACK4);
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

    memcpy(pos, parent->pos, sizeof(pos));
    pos[VX] += FixedMul(dist, finecosine[angle]);
    pos[VY] += FixedMul(dist, finesine[angle]);
    pos[VZ] += parent->info->height;
    pos[VZ] -= FLT2FIX(parent->floorclip);
    memcpy(actor->pos, pos, sizeof(pos));
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
    else if((actor->health < (actor->info->spawnhealth >> 1)) &&
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
void C_DECL A_CastSorcererSpell(mobj_t *actor)
{
    mobj_t     *mo;
    int         spell = actor->type;
    angle_t     ang1, ang2;
    fixed_t     z;
    mobj_t     *parent = actor->target;

    S_StartSound(SFX_SORCERER_SPELLCAST, NULL);

    // Put sorcerer into throw spell animation.
    if(parent->health > 0)
        P_SetMobjStateNF(parent, S_SORC_ATTACK4);

    switch(spell)
    {
    case MT_SORCBALL1: // Offensive.
        A_SorcOffense1(actor);
        break;

    case MT_SORCBALL2: // Defensive.
        z = parent->pos[VZ] - FLT2FIX(parent->floorclip + SORC_DEFENSE_HEIGHT);
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], z, MT_SORCFX2);
        parent->flags2 |= MF2_REFLECTIVE | MF2_INVULNERABLE;
        parent->args[0] = SORC_DEFENSE_TIME;
        if(mo)
            mo->target = parent;
        break;

    case MT_SORCBALL3: // Reinforcements.
        ang1 = actor->angle - ANGLE_45;
        ang2 = actor->angle + ANGLE_45;
        if(actor->health < (actor->info->spawnhealth / 3))
        {   // Spawn 2 at a time.
            mo = P_SpawnMissileAngle(parent, MT_SORCFX3, ang1, 4 * FRACUNIT);
            if(mo)
                mo->target = parent;
            mo = P_SpawnMissileAngle(parent, MT_SORCFX3, ang2, 4 * FRACUNIT);
            if(mo)
                mo->target = parent;
        }
        else
        {
            if(P_Random() < 128)
                ang1 = ang2;
            mo = P_SpawnMissileAngle(parent, MT_SORCFX3, ang1, 4 * FRACUNIT);
            if(mo)
                mo->target = parent;
        }
        break;

    default:
        break;
    }
}

/**
 * Actor is ball.
 */
void C_DECL A_SorcOffense1(mobj_t *actor)
{
    mobj_t     *mo;
    angle_t     ang1, ang2;
    mobj_t     *parent = (mobj_t *) actor->target;

    ang1 = actor->angle + ANGLE_1 * 70;
    ang2 = actor->angle - ANGLE_1 * 70;
    mo = P_SpawnMissileAngle(parent, MT_SORCFX1, ang1, 0);
    if(mo)
    {
        mo->target = parent;
        mo->tracer = parent->target;
        mo->args[4] = BOUNCE_TIME_UNIT;
        mo->args[3] = 15; // Bounce time in seconds.
    }

    mo = P_SpawnMissileAngle(parent, MT_SORCFX1, ang2, 0);
    if(mo)
    {
        mo->target = parent;
        mo->tracer = parent->target;
        mo->args[4] = BOUNCE_TIME_UNIT;
        mo->args[3] = 15; // Bounce time in seconds.
    }
}

/**
 * Actor is ball.
 */
void C_DECL A_SorcOffense2(mobj_t *actor)
{
    angle_t     ang1;
    mobj_t     *mo;
    int         delta, index;
    mobj_t     *parent = actor->target;
    mobj_t     *dest = parent->target;
    int         dist;

    index = actor->args[4] << 5;
    actor->args[4] += 15;
    delta = (finesine[index]) * SORCFX4_SPREAD_ANGLE;
    delta = (delta >> FRACBITS) * ANGLE_1;
    ang1 = actor->angle + delta;
    mo = P_SpawnMissileAngle(parent, MT_SORCFX4, ang1, 0);
    if(mo)
    {
        mo->special2 = TICSPERSEC * 5 / 2; // 5 seconds.
        dist = P_ApproxDistance(dest->pos[VX] - mo->pos[VX],
                                dest->pos[VY] - mo->pos[VY]);
        dist = dist / mo->info->speed;
        if(dist < 1)
            dist = 1;
        mo->mom[MZ] = (dest->pos[VZ] - mo->pos[VZ]) / dist;
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
void C_DECL A_SpawnFizzle(mobj_t *actor)
{
    fixed_t     pos[3];
    fixed_t     dist = 5 * FRACUNIT;
    angle_t     angle = actor->angle >> ANGLETOFINESHIFT;
    fixed_t     speed = actor->info->speed;
    angle_t     rangle;
    mobj_t     *mo;
    int         ix;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(dist, finecosine[angle]);
    pos[VY] += FixedMul(dist, finesine[angle]);
    pos[VZ] += FLT2FIX(actor->height /2);
    pos[VZ] -= FLT2FIX(actor->floorclip);

    for(ix = 0; ix < 5; ++ix)
    {
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SORCSPARK1);
        if(mo)
        {
            rangle = angle + ((P_Random() % 5) << 1);
            mo->mom[MX] = FixedMul(P_Random() % speed, finecosine[rangle]);
            mo->mom[MY] = FixedMul(P_Random() % speed, finesine[rangle]);
            mo->mom[MZ] = FRACUNIT * 2;
        }
    }
}

/**
 * Yellow spell - offense.
 */
void C_DECL A_SorcFX1Seek(mobj_t *actor)
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
void C_DECL A_SorcFX2Split(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_SORCFX2);
    if(mo)
    {
        mo->target = actor->target;
        mo->args[0] = 0; // CW.
        mo->special1 = actor->angle; // Set angle.
        P_SetMobjStateNF(mo, S_SORCFX2_ORBIT1);
    }
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_SORCFX2);
    if(mo)
    {
        mo->target = actor->target;
        mo->args[0] = 1; // CCW.
        mo->special1 = actor->angle; // Set angle.
        P_SetMobjStateNF(mo, S_SORCFX2_ORBIT1);
    }
    P_SetMobjStateNF(actor, S_NULL);
}

/**
 * Orbit FX2 about sorcerer.
 */
void C_DECL A_SorcFX2Orbit(mobj_t *actor)
{
    angle_t     angle;
    fixed_t     pos[3];
    mobj_t     *parent = actor->target;
    fixed_t     dist = parent->info->radius;

    if((parent->health <= 0) || // Sorcerer is dead.
       (!parent->args[0])) // Time expired.
    {
        P_SetMobjStateNF(actor, actor->info->deathstate);
        parent->args[0] = 0;
        parent->flags2 &= ~MF2_REFLECTIVE;
        parent->flags2 &= ~MF2_INVULNERABLE;
    }

    if(actor->args[0] && (parent->args[0]-- <= 0)) // Time expired.
    {
        P_SetMobjStateNF(actor, actor->info->deathstate);
        parent->args[0] = 0;
        parent->flags2 &= ~MF2_REFLECTIVE;
    }

    // Move to new position based on angle.
    if(actor->args[0]) // Counter clock-wise.
    {
        actor->special1 += ANGLE_1 * 10;
        angle = ((angle_t) actor->special1) >> ANGLETOFINESHIFT;

        memcpy(pos, parent->pos, sizeof(pos));
        pos[VX] += FixedMul(dist, finecosine[angle]);
        pos[VY] += FixedMul(dist, finesine[angle]);
        pos[VZ] += SORC_DEFENSE_HEIGHT * FRACUNIT + FixedMul(15 * FRACUNIT, finecosine[angle]);
        pos[VZ] -= FLT2FIX(parent->floorclip);

        // Spawn trailer.
        P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SORCFX2_T1);
    }
    else // Clock wise.
    {
        actor->special1 -= ANGLE_1 * 10;
        angle = ((angle_t) actor->special1) >> ANGLETOFINESHIFT;

        memcpy(pos, parent->pos, sizeof(pos));
        pos[VX] += FixedMul(dist, finecosine[angle]);
        pos[VY] += FixedMul(dist, finesine[angle]);
        pos[VZ] += (SORC_DEFENSE_HEIGHT * FRACUNIT) + FixedMul(20 * FRACUNIT, finesine[angle]);
        pos[VZ] -= FLT2FIX(parent->floorclip);

        // Spawn trailer
        P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SORCFX2_T1);
    }

    memcpy(actor->pos, pos, sizeof(actor->pos));
}

/**
 * Green spell - spawn bishops.
 */
void C_DECL A_SpawnBishop(mobj_t *actor)
{
    mobj_t     *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                     MT_BISHOP);
    if(mo)
    {
        if(!P_TestMobjLocation(mo))
        {
            P_SetMobjState(mo, S_NULL);
        }
    }
    P_SetMobjState(actor, S_NULL);
}

void C_DECL A_SmokePuffExit(mobj_t *actor)
{
    P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                MT_MNTRSMOKEEXIT);
}

void C_DECL A_SorcererBishopEntry(mobj_t *actor)
{
    P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ],
                MT_SORCFX3_EXPLOSION);
    S_StartSound(actor->info->seesound, actor);
}

/**
 * FX4 - rapid fire balls.
 */
void C_DECL A_SorcFX4Check(mobj_t *actor)
{
    if(actor->special2-- <= 0)
    {
        P_SetMobjStateNF(actor, actor->info->deathstate);
    }
}

/**
 * Ball death - spawn stuff.
 */
void C_DECL A_SorcBallPop(mobj_t *actor)
{
    S_StartSound(SFX_SORCERER_BALLPOP, NULL);
    actor->flags &= ~MF_NOGRAVITY;
    actor->flags2 |= MF2_LOGRAV;
    actor->mom[MX] = ((P_Random() % 10) - 5) << FRACBITS;
    actor->mom[MY] = ((P_Random() % 10) - 5) << FRACBITS;
    actor->mom[MZ] = (2 + (P_Random() % 3)) << FRACBITS;
    actor->special2 = 4 * FRACUNIT; // Initial bounce factor.
    actor->args[4] = BOUNCE_TIME_UNIT; // Bounce time unit.
    actor->args[3] = 5; // Bounce time in seconds.
}

void C_DECL A_BounceCheck(mobj_t *actor)
{
    if(actor->args[4]-- <= 0)
    {
        if(actor->args[3]-- <= 0)
        {
            P_SetMobjState(actor, actor->info->deathstate);
            switch(actor->type)
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
            actor->args[4] = BOUNCE_TIME_UNIT;
        }
    }
}

void C_DECL A_FastChase(mobj_t *actor)
{
#define CLASS_BOSS_STRAFE_RANGE (64 * 10 * FRACUNIT)

    int     delta;
    fixed_t dist;
    angle_t ang;
    mobj_t *target;

    if(actor->reactiontime)
    {
        actor->reactiontime--;
    }

    // Modify target threshold.
    if(actor->threshold)
    {
        actor->threshold--;
    }

    if(gameskill == SM_NIGHTMARE || (fastMonsters))
    {   // Monsters move faster in nightmare mode.
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
        {
            actor->tics = 3;
        }
    }

    // Turn towards movement direction if not there yet.
    if(actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);
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
            return; // Got a new target

        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // Don't attack twice in a row.
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != SM_NIGHTMARE)
            P_NewChaseDir(actor);
        return;
    }

    // Strafe.
    if(actor->special2 > 0)
    {
        actor->special2--;
    }
    else
    {
        target = actor->target;
        actor->special2 = 0;
        actor->mom[MX] = actor->mom[MY] = 0;
        dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                                actor->pos[VY] - target->pos[VY]);
        if(dist < CLASS_BOSS_STRAFE_RANGE)
        {
            if(P_Random() < 100)
            {
                ang =
                    R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                    target->pos[VX], target->pos[VY]);
                if(P_Random() < 128)
                    ang += ANGLE_90;
                else
                    ang -= ANGLE_90;
                ang >>= ANGLETOFINESHIFT;
                actor->mom[MX] = FixedMul(13 * FRACUNIT, finecosine[ang]);
                actor->mom[MY] = FixedMul(13 * FRACUNIT, finesine[ang]);
                actor->special2 = 3; // strafe time.
            }
        }
    }

    // Check for missile attack.
    if(actor->info->missilestate)
    {
        if(gameskill < SM_NIGHTMARE && actor->movecount)
            goto nomissile;
        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_SetMobjState(actor, actor->info->missilestate);
        actor->flags |= MF_JUSTATTACKED;
        return;
    }

   nomissile:

    // Possibly choose another target.
    if(IS_NETGAME && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return; // Got a new target.
    }

    // Chase towards player.
    if(!actor->special2)
    {
        if(--actor->movecount < 0 || !P_Move(actor))
        {
            P_NewChaseDir(actor);
        }
    }

#undef CLASS_BOSS_STRAFE_RANGE
}

void C_DECL A_FighterAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FSwordAttack2(actor);
}

void C_DECL A_ClericAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_CHolyAttack3(actor);
}

void C_DECL A_MageAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_MStaffAttack2(actor);
}

void C_DECL A_ClassBossHealth(mobj_t *actor)
{
    if(IS_NETGAME && !deathmatch) // Co-op only.
    {
        if(!actor->special1)
        {
            actor->health *= 5;
            actor->special1 = true; // Has been initialized.
        }
    }
}

/**
 * Checks if an object hit the floor.
 */
void C_DECL A_CheckFloor(mobj_t *actor)
{
    if(actor->pos[VZ] <= FLT2FIX(actor->floorz))
    {
        actor->pos[VZ] = FLT2FIX(actor->floorz);
        actor->flags2 &= ~MF2_LOGRAV;
        P_SetMobjState(actor, actor->info->deathstate);
    }
}

void C_DECL A_FreezeDeath(mobj_t *actor)
{
    actor->tics = 75 + P_Random() + P_Random();
    actor->flags |= MF_SOLID | MF_SHOOTABLE | MF_NOBLOOD;
    actor->flags2 |= MF2_PUSHABLE | MF2_TELESTOMP | MF2_PASSMOBJ | MF2_SLIDE;
    actor->height *= 2*2;
    S_StartSound(SFX_FREEZE_DEATH, actor);

    if(actor->player)
    {
        actor->player->damagecount = 0;
        actor->player->poisoncount = 0;
        actor->player->bonuscount = 0;
        if(actor->player == &players[consoleplayer])
        {
            ST_doPaletteStuff(false);
        }
    }
    else if(actor->flags & MF_COUNTKILL && actor->special)
    {   // Initiate monster death actions.
        P_ExecuteLineSpecial(actor->special, actor->args, NULL, 0, actor);
    }
}

void C_DECL A_IceSetTics(mobj_t *actor)
{
    int         floor;

    actor->tics = 70 + (P_Random() & 63);
    floor = P_GetThingFloorType(actor);
    if(floor == FLOOR_LAVA)
    {
        actor->tics >>= 2;
    }
    else if(floor == FLOOR_ICE)
    {
        actor->tics <<= 1;
    }
}

void C_DECL A_IceCheckHeadDone(mobj_t *actor)
{
    if(actor->special2 == 666)
    {
        P_SetMobjState(actor, S_ICECHUNK_HEAD2);
    }
}

#ifdef MSVC
#  pragma optimize("g", off)
#endif
void C_DECL A_FreezeDeathChunks(mobj_t *actor)
{
    int         i;
    mobj_t     *mo;

    if(actor->mom[MX] || actor->mom[MY] || actor->mom[MZ])
    {
        actor->tics = 105;
        return;
    }
    S_StartSound(SFX_FREEZE_SHATTER, actor);

    for(i = 12 + (P_Random() & 15); i >= 0; i--)
    {
        mo = P_SpawnMobj(actor->pos[VX] +
                         (((P_Random() - 128) * actor->radius) >> 7),
                         actor->pos[VY] +
                         (((P_Random() - 128) * actor->radius) >> 7),
                         actor->pos[VZ] + (P_Random() * FLT2FIX(actor->height) / 255),
                         MT_ICECHUNK);
        P_SetMobjState(mo, mo->info->spawnstate + (P_Random() % 3));
        if(mo)
        {
            mo->mom[MZ] = FixedDiv(mo->pos[VZ] - actor->pos[VZ], FLT2FIX(actor->height)) << 2;
            mo->mom[MX] = (P_Random() - P_Random()) << (FRACBITS - 7);
            mo->mom[MY] = (P_Random() - P_Random()) << (FRACBITS - 7);
            A_IceSetTics(mo); // Set a random tic wait.
        }
    }

    for(i = 12 + (P_Random() & 15); i >= 0; i--)
    {
        mo = P_SpawnMobj(actor->pos[VX] +
                         (((P_Random() - 128) * actor->radius) >> 7),
                         actor->pos[VY] +
                         (((P_Random() - 128) * actor->radius) >> 7),
                         actor->pos[VZ] + (P_Random() * FLT2FIX(actor->height) / 255),
                         MT_ICECHUNK);
        P_SetMobjState(mo, mo->info->spawnstate + (P_Random() % 3));
        if(mo)
        {
            mo->mom[MZ] = FixedDiv(mo->pos[VZ] - actor->pos[VZ], FLT2FIX(actor->height)) << 2;
            mo->mom[MX] = (P_Random() - P_Random()) << (FRACBITS - 7);
            mo->mom[MY] = (P_Random() - P_Random()) << (FRACBITS - 7);
            A_IceSetTics(mo); // Set a random tic wait.
        }
    }

    if(actor->player)
    {   // Attach the player's view to a chunk of ice.
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ] + VIEWHEIGHT,
                         MT_ICECHUNK);
        P_SetMobjState(mo, S_ICECHUNK_HEAD);
        mo->mom[MZ] = FixedDiv(mo->pos[VZ] - actor->pos[VZ], FLT2FIX(actor->height)) << 2;
        mo->mom[MX] = (P_Random() - P_Random()) << (FRACBITS - 7);
        mo->mom[MY] = (P_Random() - P_Random()) << (FRACBITS - 7);

        mo->flags2 |= MF2_ICEDAMAGE; // Used to force blue palette.
        mo->flags2 &= ~MF2_FLOORCLIP;
        mo->player = actor->player;
        mo->dplayer = actor->dplayer;
        actor->player = NULL;
        actor->dplayer = NULL;

        mo->health = actor->health;
        mo->angle = actor->angle;
        mo->player->plr->mo = mo;
        mo->player->plr->lookdir = 0;
    }

    P_RemoveMobjFromTIDList(actor);
    P_SetMobjState(actor, S_FREETARGMOBJ);
    actor->flags2 |= MF2_DONTDRAW;
}
#ifdef MSVC
#  pragma optimize("", on)
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

void C_DECL A_KoraxChase(mobj_t *actor)
{
    mobj_t     *spot;
    int         lastfound;
    byte        args[3] = { 0, 0, 0 };

    if((!actor->special2) &&
       (actor->health <= (actor->info->spawnhealth / 2)))
    {
        lastfound = 0;
        spot = P_FindMobjFromTID(KORAX_FIRST_TELEPORT_TID, &lastfound);
        if(spot)
        {
            P_Teleport(actor, spot->pos[VX], spot->pos[VY], spot->angle, true);
        }

        P_StartACS(249, 0, args, actor, NULL, 0);
        actor->special2 = 1; // Don't run again.

        return;
    }

    if(!actor->target)
        return;

    if(P_Random() < 30)
    {
        P_SetMobjState(actor, actor->info->missilestate);
    }
    else if(P_Random() < 30)
    {
        S_StartSound(SFX_KORAX_ACTIVE, NULL);
    }

    // Teleport away.
    if(actor->health < (actor->info->spawnhealth >> 1))
    {
        if(P_Random() < 10)
        {
            lastfound = actor->special1;
            spot = P_FindMobjFromTID(KORAX_TELEPORT_TID, &lastfound);
            actor->tracer = spot;
            if(spot)
            {
                P_Teleport(actor, spot->pos[VX], spot->pos[VY],
                           spot->angle, true);
            }
        }
    }
}

void C_DECL A_KoraxStep(mobj_t *actor)
{
    A_Chase(actor);
}

void C_DECL A_KoraxStep2(mobj_t *actor)
{
    S_StartSound(SFX_KORAX_STEP, NULL);
    A_Chase(actor);
}

void C_DECL A_KoraxBonePop(mobj_t *actor)
{
    mobj_t     *mo;
    byte        args[5];

    args[0] = args[1] = args[2] = args[3] = args[4] = 0;

    // Spawn 6 spirits equalangularly.
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT1, ANGLE_60 * 0,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT2, ANGLE_60 * 1,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT3, ANGLE_60 * 2,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT4, ANGLE_60 * 3,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT5, ANGLE_60 * 4,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);
    mo = P_SpawnMissileAngle(actor, MT_KORAX_SPIRIT6, ANGLE_60 * 5,
                             5 * FRACUNIT);
    if(mo)
        KSpiritInit(mo, actor);

    P_StartACS(255, 0, args, actor, NULL, 0); // Death script.
}

void KSpiritInit(mobj_t *spirit, mobj_t *korax)
{
    int         i;
    mobj_t     *tail, *next;

    spirit->health = KORAX_SPIRIT_LIFETIME;

    spirit->tracer = korax; // Swarm around korax.
    spirit->special2 = 32 + (P_Random() & 7);   // Float bob index.
    spirit->args[0] = 10; // Initial turn value.
    spirit->args[1] = 0;  // Initial look angle.

    // Spawn a tail for spirit.
    tail = P_SpawnMobj(spirit->pos[VX], spirit->pos[VY], spirit->pos[VZ],
                       MT_HOLY_TAIL);
    tail->target = spirit;  // Parent.
    for(i = 1; i < 3; ++i)
    {
        next = P_SpawnMobj(spirit->pos[VX], spirit->pos[VY],
                           spirit->pos[VZ],
                           MT_HOLY_TAIL);
        P_SetMobjState(next, next->info->spawnstate + 1);
        tail->tracer = next;
        tail = next;
    }
    tail->tracer = NULL; // Last tail bit.
}

void C_DECL A_KoraxDecide(mobj_t *actor)
{
    if(P_Random() < 220)
    {
        P_SetMobjState(actor, S_KORAX_MISSILE1);
    }
    else
    {
        P_SetMobjState(actor, S_KORAX_COMMAND1);
    }
}

void C_DECL A_KoraxMissile(mobj_t *actor)
{
    int         type = P_Random() % 6;
    int         sound = 0;

    S_StartSound(SFX_KORAX_ATTACK, actor);

    switch(type)
    {
    case 0:
        type = MT_WRAITHFX1;
        sound = SFX_WRAITH_MISSILE_FIRE;
        break;

    case 1:
        type = MT_DEMONFX1;
        sound = SFX_DEMON_MISSILE_FIRE;
        break;

    case 2:
        type = MT_DEMON2FX1;
        sound = SFX_DEMON_MISSILE_FIRE;
        break;

    case 3:
        type = MT_FIREDEMON_FX6;
        sound = SFX_FIRED_ATTACK;
        break;

    case 4:
        type = MT_CENTAUR_FX;
        sound = SFX_CENTAURLEADER_ATTACK;
        break;

    case 5:
        type = MT_SERPENTFX;
        sound = SFX_CENTAURLEADER_ATTACK;
        break;
    }

    // Fire all 6 missiles at once.
    S_StartSound(sound, NULL);
    KoraxFire1(actor, type);
    KoraxFire2(actor, type);
    KoraxFire3(actor, type);
    KoraxFire4(actor, type);
    KoraxFire5(actor, type);
    KoraxFire6(actor, type);
}

/**
 * Call action code scripts (250-254).
 */
void C_DECL A_KoraxCommand(mobj_t *actor)
{
    byte        args[5];
    fixed_t     pos[3];
    angle_t     ang;
    int         numcommands;

    S_StartSound(SFX_KORAX_COMMAND, actor);

    // Shoot stream of lightning to ceiling.
    ang = (actor->angle - ANGLE_90) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_COMMAND_OFFSET, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_COMMAND_OFFSET, finesine[ang]);
    pos[VZ] += KORAX_COMMAND_HEIGHT;
    P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_KORAX_BOLT);

    args[0] = args[1] = args[2] = args[3] = args[4] = 0;

    if(actor->health <= (actor->info->spawnhealth >> 1))
    {
        numcommands = 5;
    }
    else
    {
        numcommands = 4;
    }

    switch(P_Random() % numcommands)
    {
    case 0:
        P_StartACS(250, 0, args, actor, NULL, 0);
        break;

    case 1:
        P_StartACS(251, 0, args, actor, NULL, 0);
        break;

    case 2:
        P_StartACS(252, 0, args, actor, NULL, 0);
        break;

    case 3:
        P_StartACS(253, 0, args, actor, NULL, 0);
        break;

    case 4:
        P_StartACS(254, 0, args, actor, NULL, 0);
        break;
    }
}

/**
 * Arm projectiles.
 * Arm positions numbered:
 *
 * 1   top left
 * 2   middle left
 * 3   lower left
 * 4   top right
 * 5   middle right
 * 6   lower right
 */

/**
 * Korax: Arm 1 projectile.
 */
void KoraxFire1(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle - KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_SHORT, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_SHORT, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM1_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

/**
 * Korax: Arm 2 projectile.
 */
void KoraxFire2(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle - KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_LONG, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_LONG, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM2_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

/**
 * Korax: Arm 3 projectile.
 */
void KoraxFire3(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle - KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_LONG, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_LONG, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM3_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

/**
 * Korax: Arm 4 projectile.
 */
void KoraxFire4(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle + KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_SHORT, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_SHORT, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM4_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

/**
 * Korax: Arm 5 projectile.
 */
void KoraxFire5(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle + KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_LONG, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_LONG, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM5_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

/**
 * Korax: Arm 6 projectile.
 */
void KoraxFire6(mobj_t *actor, int type)
{
    mobj_t     *mo;
    angle_t     ang;
    fixed_t     pos[3];

    ang = (actor->angle + KORAX_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(KORAX_ARM_EXTENSION_LONG, finecosine[ang]);
    pos[VY] += FixedMul(KORAX_ARM_EXTENSION_LONG, finesine[ang]);
    pos[VZ] -= FLT2FIX(actor->floorclip);
    pos[VZ] += KORAX_ARM6_HEIGHT;
    mo = P_SpawnKoraxMissile(pos[VX], pos[VY], pos[VZ], actor,
                             actor->target, type);
}

void C_DECL A_KSpiritWeave(mobj_t *actor)
{
    fixed_t     newpos[3];
    int         weaveXY, weaveZ;
    int         angle;

    weaveXY = actor->special2 >> 16;
    weaveZ = actor->special2 & 0xFFFF;
    angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;

    memcpy(newpos, actor->pos, sizeof(newpos));
    newpos[VX] -= FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newpos[VY] -= FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);

    weaveXY = (weaveXY + (P_Random() % 5)) & 63;
    newpos[VX] += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newpos[VY] += FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);

    P_TryMove(actor, newpos[VX], newpos[VY]);
    actor->pos[VZ] -= FloatBobOffsets[weaveZ] << 1;

    weaveZ = (weaveZ + (P_Random() % 5)) & 63;
    actor->pos[VZ] += FloatBobOffsets[weaveZ] << 1;
    actor->special2 = weaveZ + (weaveXY << 16);
}

void C_DECL A_KSpiritSeeker(mobj_t *actor, angle_t thresh,
                            angle_t turnMax)
{
    int         dir;
    int         dist;
    angle_t     delta;
    angle_t     angle;
    mobj_t     *target;
    fixed_t     newZ;
    fixed_t     deltaZ;

    target = actor->tracer;
    if(target == NULL)
        return;

    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }

    if(dir) // Turn clockwise.
        actor->angle += delta;
    else // Turn counter clockwise.
        actor->angle -= delta;

    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->mom[MX] = FixedMul(actor->info->speed, finecosine[angle]);
    actor->mom[MY] = FixedMul(actor->info->speed, finesine[angle]);

    if(!(leveltime & 15) ||
       (actor->pos[VZ] > target->pos[VZ] + (target->info->height)) ||
       (actor->pos[VZ] + FLT2FIX(actor->height) < target->pos[VZ]))
    {
        newZ = target->pos[VZ] +
                    ((P_Random() * target->info->height) >> 8);
        deltaZ = newZ - actor->pos[VZ];
        if(abs(deltaZ) > 15 * FRACUNIT)
        {
            if(deltaZ > 0)
            {
                deltaZ = 15 * FRACUNIT;
            }
            else
            {
                deltaZ = -15 * FRACUNIT;
            }
        }

        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VY] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
            dist = 1;

        actor->mom[MZ] = deltaZ / dist;
    }
}

void C_DECL A_KSpiritRoam(mobj_t *actor)
{
    if(actor->health-- <= 0)
    {
        S_StartSound(SFX_SPIRIT_DIE, actor);
        P_SetMobjState(actor, S_KSPIRIT_DEATH1);
    }
    else
    {
        if(actor->tracer)
        {
            A_KSpiritSeeker(actor, actor->args[0] * ANGLE_1,
                            actor->args[0] * ANGLE_1 * 2);
        }

        A_KSpiritWeave(actor);
        if(P_Random() < 50)
        {
            S_StartSound(SFX_SPIRIT_ACTIVE, NULL);
        }
    }
}

void C_DECL A_KBolt(mobj_t *actor)
{
    // Countdown lifetime.
    if(actor->special1-- <= 0)
    {
        P_SetMobjState(actor, S_NULL);
    }
}

void C_DECL A_KBoltRaise(mobj_t *actor)
{
#define KORAX_BOLT_HEIGHT       (48 * FRACUNIT)
#define KORAX_BOLT_LIFETIME     (3)

    mobj_t     *mo;
    fixed_t     z;

    // Spawn a child upward.
    z = actor->pos[VZ] + KORAX_BOLT_HEIGHT;

    if((z + KORAX_BOLT_HEIGHT) < FLT2FIX(actor->ceilingz))
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], z, MT_KORAX_BOLT);
        if(mo)
        {
            mo->special1 = KORAX_BOLT_LIFETIME;
        }
    }

#undef KORAX_BOLT_HEIGHT
#undef KORAX_BOLT_LIFETIME
}
