/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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

/*
 * Enemy thinking, AI.
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "doom64tc.h"

#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define FATSPREAD               (ANG90/8)
#define FAT_DELTAANGLE          (85*ANGLE_1) // d64tc
#define FAT_ARM_EXTENSION_SHORT (32*FRACUNIT) // d64tc
#define FAT_ARM_EXTENSION_LONG  (16*FRACUNIT) // d64tc
#define FAT_ARM_HEIGHT          (64*FRACUNIT) // d64tc
#define SKULLSPEED              (20*FRACUNIT)

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

void C_DECL A_ReFire(player_t *player, pspdef_t * psp);
void C_DECL A_Fall(mobj_t *actor);
void C_DECL A_Fire(mobj_t *actor);
void C_DECL A_SpawnFly(mobj_t *mo);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean felldown;        //$dropoff_fix: used to flag pushed off ledge
extern line_t *blockline;       // $unstuck: blocking linedef
extern fixed_t   tmbbox[4];     // for line intersection checks

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean bossKilled;
mobj_t *soundtarget;

mobj_t *corpsehit;

mobj_t **braintargets;
int     numbraintargets;
int     numbraintargets_alloc;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

int     TRACEANGLE = 0xc000000;

fixed_t xspeed[8] =
    { FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000 };
fixed_t yspeed[8] =
    { 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

// CODE --------------------------------------------------------------------

/*
 * Recursively traverse adjacent sectors,
 * sound blocking lines cut off traversal.
 * Called by P_NoiseAlert.
 */
void P_RecursiveSound(sector_t *sec, int soundblocks)
{
    int     i;
    line_t *check;
    sector_t *other;
    sector_t *frontsector, *backsector;
    xsector_t *xsec = P_XSector(sec);

    // wake up all monsters in this sector
    if(P_GetIntp(sec, DMU_VALID_COUNT) == validCount &&
       xsec->soundtraversed <= soundblocks + 1)
        return;                 // already flooded

    P_SetIntp(sec, DMU_VALID_COUNT, validCount);

    xsec->soundtraversed = soundblocks + 1;
    xsec->soundtarget = soundtarget;

    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

        frontsector = P_GetPtrp(check, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(check, DMU_BACK_SECTOR);

        if(!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED))
            continue;

        P_LineOpening(check);

        if(openrange <= 0)
            continue;           // closed door

        if(frontsector == sec)
            other = backsector;
        else
            other = frontsector;

        if(P_GetIntp(check, DMU_FLAGS) & ML_SOUNDBLOCK)
        {
            if(!soundblocks)
                P_RecursiveSound(other, 1);
        }
        else
            P_RecursiveSound(other, soundblocks);
    }
}

/*
 * If a monster yells at a player,
 * it will alert other monsters to the player.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
    soundtarget = target;
    validCount++;
    P_RecursiveSound(P_GetPtrp(emitter->subsector, DMU_SECTOR), 0);
}

boolean P_CheckMeleeRange(mobj_t *actor)
{
    mobj_t *pl;
    fixed_t dist;
    fixed_t range;

    if(!actor->target)
        return false;

    pl = actor->target;
    dist = P_ApproxDistance(pl->pos[VX] - actor->pos[VX],
                            pl->pos[VY] - actor->pos[VY]);
    if(!(cfg.netNoMaxZMonsterMeleeAttack))
        dist =
            P_ApproxDistance(dist, (pl->pos[VZ] + (pl->height >> 1)) -
                                   (actor->pos[VZ] + (actor->height >> 1)));

    //range = (MELEERANGE - 20 * FRACUNIT + pl->info->radius);
    range = (MELEERANGE - 14 * FRACUNIT + pl->info->radius); // d64tc
    if(dist >= range)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

boolean P_CheckMissileRange(mobj_t *actor)
{
    fixed_t dist;

    if(!P_CheckSight(actor, actor->target))
        return false;

    if(actor->flags & MF_JUSTHIT)
    {
        // the target just hit the enemy,
        // so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactiontime)
        return false;           // do not attack yet

    // OPTIMIZE: get this from a global checksight
    dist =
        P_ApproxDistance(actor->pos[VX] - actor->target->pos[VX],
                         actor->pos[VY] - actor->target->pos[VY]) - 64 * FRACUNIT;

    if(!actor->info->meleestate)
        dist -= 128 * FRACUNIT; // no melee attack, so fire more

    dist >>= 16;

    if(actor->type == MT_CYBORG || actor->type == MT_SPIDER ||
       actor->type == MT_SKULL)
    {
        dist >>= 1;
    }

    if(dist > 200)
        dist = 200;

    if(actor->type == MT_CYBORG && dist > 160)
        dist = 160;

    if(P_Random() < dist)
        return false;

    return true;
}

/*
 * Move in the current direction,
 * returns false if the move is blocked.
 * killough $dropoff_fix
 */
boolean P_Move(mobj_t *actor, boolean dropoff)
{
    fixed_t tryx, stepx;
    fixed_t tryy, stepy;
    line_t *ld;
    boolean good;

    if(actor->movedir == DI_NODIR)
        return false;

    if((unsigned) actor->movedir >= 8)
        Con_Error("Weird actor->movedir!");

    stepx = actor->info->speed / FRACUNIT * xspeed[actor->movedir];
    stepy = actor->info->speed / FRACUNIT * yspeed[actor->movedir];
    tryx = actor->pos[VX] + stepx;
    tryy = actor->pos[VY] + stepy;

    // killough $dropoff_fix
    if(!P_TryMove(actor, tryx, tryy, dropoff, false))
    {
        // open any specials
        if(actor->flags & MF_FLOAT && floatok)
        {
            // must adjust height
            if(actor->pos[VZ] < tmfloorz)
                actor->pos[VZ] += FLOATSPEED;
            else
                actor->pos[VZ] -= FLOATSPEED;

            actor->flags |= MF_INFLOAT;
            return true;
        }

        if(!P_IterListSize(spechit))
            return false;

        actor->movedir = DI_NODIR;
        good = false;
        while((ld = P_PopIterList(spechit)) != NULL)
        {
            // if the special is not a door that can be opened, return false
            //
            // killough $unstuck: this is what caused monsters to get stuck in
            // doortracks, because it thought that the monster freed itself
            // by opening a door, even if it was moving towards the doortrack,
            // and not the door itself.
            //
            // If a line blocking the monster is activated, return true 90%
            // of the time. If a line blocking the monster is not activated,
            // but some other line is, return false 90% of the time.
            // A bit of randomness is needed to ensure it's free from
            // lockups, but for most cases, it returns the correct result.
            //
            // Do NOT simply return false 1/4th of the time (causes monsters to
            // back out when they shouldn't, and creates secondary stickiness).

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
        P_SetThingSRVO(actor, stepx, stepy);
        actor->flags &= ~MF_INFLOAT;
    }

    // $dropoff_fix: fall more slowly, under gravity, if felldown==true
    if(!(actor->flags & MF_FLOAT) && !felldown)
    {
        if(actor->pos[VZ] > actor->floorz)
            P_HitFloor(actor);

        actor->pos[VZ] = actor->floorz;
    }
    return true;
}

/*
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

    actor->movecount = P_Random() & 15;
    return true;
}

static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
    dirtype_t xdir, ydir, tdir;
    dirtype_t olddir = actor->movedir;
    dirtype_t turnaround = olddir;

    if(turnaround != DI_NODIR)  // find reverse direction
        turnaround ^= 4;

    xdir =
        deltax > 10 * FRACUNIT ? DI_EAST : deltax <
        -10 * FRACUNIT ? DI_WEST : DI_NODIR;

    ydir =
        deltay < -10 * FRACUNIT ? DI_SOUTH : deltay >
        10 * FRACUNIT ? DI_NORTH : DI_NODIR;

    // try direct route
    if(xdir != DI_NODIR && ydir != DI_NODIR &&
       turnaround != (actor->movedir =
                      deltay < 0 ? deltax >
                      0 ? DI_SOUTHEAST : DI_SOUTHWEST : deltax >
                      0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
        return;

    // try other directions
    if(P_Random() > 200 || abs(deltay) > abs(deltax))
        tdir = xdir, xdir = ydir, ydir = tdir;

    if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
       (actor->movedir = xdir, P_TryWalk(actor)))
        return;                 // either moved forward or attacked

    if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
       (actor->movedir = ydir, P_TryWalk(actor)))
        return;

    // there is no direct path to the player, so pick another direction.
    if(olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
        return;

    // randomly determine direction of search
    if(P_Random() & 1)
    {
        for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
            if(tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
                return;
    }
    else
        for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; tdir--)
            if(tdir != turnaround && (actor->movedir = tdir, P_TryWalk(actor)))
                return;

    if((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
        actor->movedir = DI_NODIR;
}

/*
 * Monsters try to move away from tall dropoffs.
 *
 * In Doom, they were never allowed to hang over dropoffs,
 * and would remain stuck if involuntarily forced over one.
 * This logic, combined with p_map.c (P_TryMove), allows
 * monsters to free themselves without making them tend to
 * hang over dropoffs.
 */
static boolean PIT_AvoidDropoff(line_t *line, void *data)
{
    sector_t *frontsector = P_GetPtrp(line, DMU_FRONT_SECTOR);
    sector_t *backsector = P_GetPtrp(line, DMU_BACK_SECTOR);
    fixed_t* bbox = P_GetPtrp(line, DMU_BOUNDING_BOX);

    if(backsector &&
       tmbbox[BOXRIGHT]  > bbox[BOXLEFT] &&
       tmbbox[BOXLEFT]   < bbox[BOXRIGHT]  &&
       tmbbox[BOXTOP]    > bbox[BOXBOTTOM] && // Linedef must be contacted
       tmbbox[BOXBOTTOM] < bbox[BOXTOP]    &&
       P_BoxOnLineSide(tmbbox, line) == -1)
    {
        fixed_t front = P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT);
        fixed_t back = P_GetFixedp(backsector, DMU_FLOOR_HEIGHT);
        fixed_t dx = P_GetFixedp(line, DMU_DX);
        fixed_t dy = P_GetFixedp(line, DMU_DY);
        angle_t angle;

        // The monster must contact one of the two floors,
        // and the other must be a tall drop off (more than 24).

        if(back == floorz && front < floorz - FRACUNIT * 24)
        {
            angle = R_PointToAngle2(0, 0, dx, dy);  // front side drop off
        }
        else
        {
            if(front == floorz && back < floorz - FRACUNIT * 24)
                angle = R_PointToAngle2(dx, dy, 0, 0);  // back side drop off
            else
                return true;
        }

        // Move away from drop off at a standard speed.
        // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
        dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT] * 32;
        dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT] * 32;
    }
    return true;
}

/*
 * Driver for above
 */
static fixed_t P_AvoidDropoff(mobj_t *actor)
{
    floorz = actor->pos[VZ];          // remember floor height

    dropoff_deltax = dropoff_deltay = 0;

    validCount++;

    // check lines
    P_ThingLinesIterator(actor, PIT_AvoidDropoff, 0);

    // Non-zero if movement prescribed
    return dropoff_deltax | dropoff_deltay;
}

void P_NewChaseDir(mobj_t *actor)
{
    mobj_t *target = actor->target;
    fixed_t deltax = target->pos[VX] - actor->pos[VX];
    fixed_t deltay = target->pos[VY] - actor->pos[VY];

    if(actor->floorz - actor->dropoffz > FRACUNIT * 24 &&
       actor->pos[VZ] <= actor->floorz &&
       !(actor->flags & (MF_DROPOFF | MF_FLOAT)) &&
       !cfg.avoidDropoffs && P_AvoidDropoff(actor))
    {
        // Move away from dropoff
        P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);

        // $dropoff_fix
        // If moving away from drop off, set movecount to 1 so that
        // small steps are taken to get monster away from drop off.

        actor->movecount = 1;
        return;
    }

    P_DoNewChaseDir(actor, deltax, deltay);
}

/*
 * If allaround is false, only look 180 degrees in front.
 * Returns true if a player is targeted.
 */
boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
    int     c;
    int     stop;
    player_t *player;
    angle_t an;
    fixed_t dist;
    int     playerCount;

    for(c = playerCount = 0; c < MAXPLAYERS; c++)
        if(players[c].plr->ingame)
            playerCount++;

    // Are there any players?
    if(!playerCount)
        return false;

    c = 0;
    stop = (actor->lastlook - 1) & 3;

    for(;; actor->lastlook = (actor->lastlook + 1) & 3)
    {
        if(!players[actor->lastlook].plr->ingame)
            continue;

        if(c++ == 2 || actor->lastlook == stop)
        {
            // done looking
            return false;
        }

        player = &players[actor->lastlook];

        if(player->powers[pw_unsee]) // d64tc
            continue;

        if(player->health <= 0)
            continue;           // dead

        if(!P_CheckSight(actor, player->plr->mo))
            continue;           // out of sight

        if(!allaround)
        {
            an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                 player->plr->mo->pos[VX], player->plr->mo->pos[VY]);
            an -= actor->angle;

            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(player->plr->mo->pos[VX] - actor->pos[VX],
                                     player->plr->mo->pos[VY] - actor->pos[VY]);
                // if real close, react anyway
                if(dist > MELEERANGE)
                    continue;   // behind back
            }
        }

        actor->target = player->plr->mo;
        return true;
    }
}

int P_Massacre(void)
{
    int     count = 0;
    mobj_t *mo;
    thinker_t *think;

    // Only massacre when in a level.
    if(G_GetGameState() != GS_LEVEL)
        return 0;

    for(think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        if(think->function != P_MobjThinker)
        {                       // Not a mobj thinker
            continue;
        }
        mo = (mobj_t *) think;
        if(mo->type == MT_SKULL ||
           (mo->flags & MF_COUNTKILL && mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, 10000);
            count++;
        }
    }
    return count;
}

// > DOOM64TC Specific ---------------------------------------------------------

/*
 * DJS - This stuff all appears to be duplications of the exact same routine...
 *       We can do better than this.
 * TODO: Without having yet looked at the state definitions - my initial thoughts
 *       are to replace all of this with a single XG line definition.
 */

/*
 * used for special stuff. works only per monster!!! samuel...
 */
void C_DECL A_PossSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4444;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SposSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4445;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_TrooSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4446;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_NtroSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4447;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SargSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4448;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Sar2Special(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4449;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_HeadSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4450;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Hed2Special(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4451;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_SkulSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4452;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_Bos2Special(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4453;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BossSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4454;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_PainSpecial(mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4455;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_FattSpecial (mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4456;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BabySpecial (mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4457;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_CybrSpecial (mobj_t* mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 4458;
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

// < DOOM64TC Specific ----------------------------------------------------

/*
 * d64tc: Formerly A_KeenDie(mobj_t *mo).
 * kaiser - used for special stuff. works only per monster!!!
 */
void C_DECL A_BitchSpecial(mobj_t *mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t *dummyLine;

    A_Fall(mo);

    // scan the remaining thinkers
    // to see if all Keens are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other Keen not dead
            return;
        }
    }

    dummyLine = P_AllocDummyLine();
    //P_XLine(dummyLine)->tag = 666;
    P_XLine(dummyLine)->tag = 4459;
    //EV_DoDoor(dummyLine, open);
    EV_DoDoor(dummyLine, lowerFloorToLowest);
    P_FreeDummyLine(dummyLine);
}

/*
 * Stay in state until a player is sighted.
 */
void C_DECL A_Look(mobj_t *actor)
{
    sector_t* sec = NULL;
    mobj_t *targ;

    actor->threshold = 0;       // any shot will wake up
    sec = P_GetPtrp(actor->subsector, DMU_SECTOR);
    targ = P_XSector(sec)->soundtarget;

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

        switch (actor->info->seesound)
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
            sound = actor->info->seesound;
            break;
        }

        if(actor->flags2 & MF2_BOSS)
            S_StartSound(sound | DDSF_NO_ATTENUATION, actor); // full volume
        else
            S_StartSound(sound, actor);
    }

    P_SetMobjState(actor, actor->info->seestate);
}

/*
 * Actor has a melee attack,
 * so it tries to close as fast as possible
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int     delta;

    // d64tc >
    if(actor->flags & MF_FLOAT)
    {
        int r = P_Random();

        if(r < 64)
            actor->momz += FRACUNIT;
        else if(r < 128)
            actor->momz -= FRACUNIT;
    }
    // < d64tc

    if(actor->reactiontime)
        actor->reactiontime--;

    // modify target threshold
    if(actor->threshold)
    {
        if(!actor->target || actor->target->health <= 0)
        {
            actor->threshold = 0;
        }
        else
            actor->threshold--;
    }

    // turn towards movement direction if not there yet
    if(actor->movedir < 8)
    {
        actor->angle &= (7 << 29);
        delta = actor->angle - (actor->movedir << 29);

        if(delta > 0)
            actor->angle -= ANG90 / 2;
        else if(delta < 0)
            actor->angle += ANG90 / 2;
    }

    if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
    {
        // look for a new target
        if(P_LookForPlayers(actor, true))
            return;             // got a new target

        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // do not attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;
        if(gameskill != sk_nightmare && !fastparm)
            P_NewChaseDir(actor);
        return;
    }

    // check for melee attack
    if(actor->info->meleestate && P_CheckMeleeRange(actor))
    {
        if(actor->info->attacksound)
            S_StartSound(actor->info->attacksound, actor);

        P_SetMobjState(actor, actor->info->meleestate);
        return;
    }

    // check for missile attack
    if(actor->info->missilestate)
    {
        if(gameskill < sk_nightmare && !fastparm && actor->movecount)
        {
            goto nomissile;
        }

        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_SetMobjState(actor, actor->info->missilestate);
        actor->flags |= MF_JUSTATTACKED;
        return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if(IS_NETGAME && !actor->threshold && !P_CheckSight(actor, actor->target))
    {
        if(P_LookForPlayers(actor, true))
            return;             // got a new target
    }

    // chase towards player
    if(--actor->movecount < 0 || !P_Move(actor, false))
    {
        P_NewChaseDir(actor);
    }

    // make active sound
    if(actor->info->activesound && P_Random() < 3)
    {
        S_StartSound(actor->info->activesound, actor);
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
        actor->angle += (P_Random() - P_Random()) << 21;
}

void C_DECL A_PosAttack(mobj_t *actor)
{
    int     angle;
    int     damage;
    int     slope;

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
    int     i;
    int     angle;
    int     bangle;
    int     damage;
    int     slope;

    if(!actor->target)
        return;

    S_StartSound(sfx_shotgn, actor);
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    for(i = 0; i < 3; i++)
    {
        angle = bangle + ((P_Random() - P_Random()) << 20);
        damage = ((P_Random() % 5) + 1) * 3;
        P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
    }
}

/**
 * d64tc
 */
void C_DECL A_CposPanLeft(mobj_t* actor)
{
    actor->angle = actor->angle + ANG90/4;
}

/**
 * d64tc
 */
void C_DECL A_CposPanRight(mobj_t* actor)
{
    actor->angle = actor->angle - ANG90/4;
}

void C_DECL A_CPosAttack(mobj_t *actor)
{
    int     angle;
    int     bangle;
    int     damage;
    int     slope;
    int     r; // d64tc

    if(!actor->target)
        return;

    //S_StartSound(sfx_shotgn, actor);
    S_StartSound(sfx_pistol, actor); // d64tc
    A_FaceTarget(actor);
    bangle = actor->angle;
    slope = P_AimLineAttack(actor, bangle, MISSILERANGE);

    angle = bangle + ((P_Random() - P_Random()) << 20);
    damage = ((P_Random() % 5) + 1) * 3;

    // d64tc >
    r = P_Random();
    if(r < 64)
        A_CposPanLeft(actor);
    else if(r < 128)
        A_CposPanRight(actor);
    // < d64tc

    P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void C_DECL A_CPosRefire(mobj_t *actor)
{
    if(!actor->target) // d64tc
        return;

    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    if(P_Random() < 30) // d64tc: was "if(P_Random() < 40)"
        return;

    // d64tc: Added "|| P_Random() < 40"
    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target) || P_Random() < 40)
    {
        P_SetMobjState(actor, actor->info->seestate);
    }
}

void C_DECL A_SpidRefire(mobj_t *actor)
{
    // keep firing unless target got out of sight
    A_FaceTarget(actor);

    if(P_Random() < 10)
        return;

    if(!actor->target || actor->target->health <= 0 ||
       !P_CheckSight(actor, actor->target))
    {
        P_SetMobjState(actor, actor->info->seestate);
    }
}

/**
 * d64tc: BspiAttack. Throw projectile.
 */
void BabyFire(mobj_t *actor, int type, boolean right)
{
#define BSPISPREAD                  (ANG90/8) //its cheap but it works
#define BABY_DELTAANGLE             (85*ANGLE_1)
#define BABY_ARM_EXTENSION_SHORT    (18*FRACUNIT)
#define BABY_ARM_HEIGHT             (24*FRACUNIT)

    mobj_t *mo;
    angle_t ang;
    fixed_t pos[3];

    ang = actor->angle;
    if(right)
        ang += BABY_DELTAANGLE;
    else
        ang -= BABY_DELTAANGLE;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(BABY_ARM_EXTENSION_SHORT, finecosine[ang]);
    pos[VY] += FixedMul(BABY_ARM_EXTENSION_SHORT, finesine[ang]);
    pos[VZ] -= actor->floorclip + BABY_ARM_HEIGHT;

    mo = P_SpawnMotherMissile(pos[VX], pos[VY], pos[VZ], actor, actor->target,
                              type);

    if(right)
        mo->angle += BSPISPREAD/6;
    else
        mo->angle -= BSPISPREAD/6;

    ang = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[ang]);
    mo->momy = FixedMul(mo->info->speed, finesine[ang]);

#undef BSPISPREAD
#undef BABY_DELTAANGLE
#undef BABY_ARM_EXTENSION_SHORT
#undef BABY_ARM_HEIGHT
}

/*
void C_DECL A_BspiAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_ARACHPLAZ);
}
*/

/**
 * d64tc: Modified to shoot two plasmaballs while aligned to cannon
 *        should of been like this in Doom 2!
 */
void C_DECL A_BspiAttack(mobj_t* actor)
{
    int type = P_Random() % 2;

    switch(type)
    {
    case 0:
        if(actor->type == MT_BABY || actor->info->doomednum == 234)
            type = MT_ARACHPLAZ;
        else if(actor->type == MT_NIGHTCRAWLER)
            type = MT_GRENADE;
        break;

    case 1:
        if(actor->type == MT_BABY || actor->info->doomednum == 234)
            type = MT_ARACHPLAZ;
        else if(actor->type == MT_NIGHTCRAWLER)
            type = MT_GRENADE;
        break;
    }

    BabyFire(actor, type, false);
    BabyFire(actor, type, true);
}

/*
void C_DECL A_TroopAttack(mobj_t *actor)
{
    int     damage;

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

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_TROOPSHOT);
}
*/

/**
 * d64tc: Modified version.
 */
void C_DECL A_TroopAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_TROOPSHOT);
}

/**
 * d64tc
 */
void C_DECL A_TroopClaw(mobj_t *actor)
{
    int     damage;

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

}

void C_DECL A_NtroopAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_NTROSHOT);
}

void C_DECL A_MotherFloorFire(mobj_t *actor)
{
#define FIRESPREAD  (ANG90/8) //its cheap but it works
    mobj_t *mo;
    int     an;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
#if 0
    // DJS - Fixme!
    S_StartSound(sfx_mthatk, actor);
    mo = P_SpawnMissile(actor, actor->target, MT_FIREEND);
    mo = P_SpawnMissile(actor, actor->target, MT_FIREEND);
    mo->angle -= FIRESPREAD*4;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);

    mo = P_SpawnMissile (actor, actor->target, MT_FIREEND);
    mo->angle += FIRESPREAD*4;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
#endif
#undef FIRESPREAD
}

/**
 * d64tc: Mother Demon, projectile attack. Used for all four fireballs.
 */
void MotherFire(mobj_t *actor, int type, angle_t angle, fixed_t distance,
                fixed_t height)
{
    angle_t ang;
    fixed_t pos[3];

    ang = actor->angle + angle;
    ang >>= ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(distance, finecosine[ang]);
    pos[VY] += FixedMul(distance, finesine[ang]);
    pos[VZ] += -actor->floorclip + height;

    P_SpawnMotherMissile(pos[VX], pos[VY], pos[VZ], actor, actor->target, type);
}

/**
 * d64tc: MotherDemon's Missle Attack code
 */
void C_DECL A_MotherMissle(mobj_t *actor)
{
#define MOTHER_DELTAANGLE           (85*ANGLE_1)
#define MOTHER_ARM_EXTENSION_SHORT  (40*FRACUNIT)
#define MOTHER_ARM_EXTENSION_LONG   (55*FRACUNIT)
#define MOTHER_ARM1_HEIGHT          (128*FRACUNIT)
#define MOTHER_ARM2_HEIGHT          (128*FRACUNIT)
#define MOTHER_ARM3_HEIGHT          (64*FRACUNIT)
#define MOTHER_ARM4_HEIGHT          (64*FRACUNIT)

    // Fire 4 missiles at once.
    MotherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE, MOTHER_ARM_EXTENSION_SHORT,
               MOTHER_ARM1_HEIGHT);
    MotherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE, MOTHER_ARM_EXTENSION_SHORT,
               MOTHER_ARM2_HEIGHT);
    MotherFire(actor, MT_BITCHBALL, -MOTHER_DELTAANGLE, MOTHER_ARM_EXTENSION_LONG,
               MOTHER_ARM3_HEIGHT);
    MotherFire(actor, MT_BITCHBALL, MOTHER_DELTAANGLE, MOTHER_ARM_EXTENSION_LONG,
               MOTHER_ARM4_HEIGHT);

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
void C_DECL A_SetFloorFire(mobj_t *actor)
{
    mobj_t *mo;
    fixed_t pos[3];

    actor->pos[VZ] = actor->floorz; // DJS - why?
#if 0
    // DJS - Fixme!
    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += (P_Random() - P_Random()) << 10;
    pos[VY] += (P_Random() - P_Random()) << 10;
    pos[VZ]  = ONFLOORZ;

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SPAWNFIRE);
    mo->target = actor->target;
#endif
}

/**
 * d64tc
 */
void C_DECL A_MotherBallExplode(mobj_t *spread)
{
    int     i;
    angle_t angle;
    mobj_t *shard;

    for(i = 0; i < 8; ++i)
    {
        shard = P_SpawnMobj(spread->pos[VX], spread->pos[VY], spread->pos[VZ],
                            MT_HEADSHOT);
        angle = i * ANG45;
        shard->target = spread->target;
        shard->angle  = angle;

        angle >>= ANGLETOFINESHIFT;
        shard->momx = FixedMul(shard->info->speed, finecosine[angle]);
        shard->momy = FixedMul(shard->info->speed, finesine[angle]);
    }
}

/**
 * d64tc: spawns a smoke sprite during the missle attack
 */
void C_DECL A_BitchTracerPuff(mobj_t *smoke)
{
    if(!smoke)
        return;

    P_SpawnMobj(smoke->pos[VX], smoke->pos[VY], smoke->pos[VZ], MT_MOTHERPUFF);
}

void C_DECL A_SargAttack(mobj_t *actor)
{
    int     damage;

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
    int     damage;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = (P_Random() % 6 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_HEADSHOT);
}

/*
void C_DECL A_CyberAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    P_SpawnMissile(actor, actor->target, MT_ROCKET);
}
*/

/**
 * d64tc
 */
void C_DECL A_CyberAttack(mobj_t *actor)
{
#define CYBER_DELTAANGLE            (85*ANGLE_1)
#define CYBER_ARM_EXTENSION_SHORT   (35*FRACUNIT)
#define CYBER_ARM1_HEIGHT           (68*FRACUNIT)

    angle_t ang;
    fixed_t pos[3];

    // this aligns the rocket to the d64tc cyberdemon's rocket launcher.
    ang = (actor->angle + CYBER_DELTAANGLE) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(CYBER_ARM_EXTENSION_SHORT, finecosine[ang]);
    pos[VY] += FixedMul(CYBER_ARM_EXTENSION_SHORT, finesine[ang]);
    pos[VZ] += -actor->floorclip + CYBER_ARM1_HEIGHT;

    P_SpawnMotherMissile(pos[VX], pos[VY], pos[VZ], actor, actor->target,
                         MT_CYBERROCKET);

#undef CYBER_DELTAANGLE
#undef CYBER_ARM_EXTENSION_SHORT
#undef CYBER_ARM1_HEIGHT
}

void C_DECL A_BruisAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_BRUISERSHOT);
}

/**
 * d64tc Special Bruiser shot for Baron
 */
void C_DECL A_BruisredAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        S_StartSound(sfx_claw, actor);
        damage = (P_Random() % 8 + 1) * 10;
        P_DamageMobj(actor->target, actor, actor, damage);
        return;
    }

    // launch a missile
    P_SpawnMissile(actor, actor->target, MT_BRUISERSHOTRED);
}

/*
void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    A_FaceTarget(actor);
    actor->pos[VZ] += 16 * FRACUNIT;  // so missile spawns higher
    mo = P_SpawnMissile(actor, actor->target, MT_TRACER);
    actor->pos[VZ] -= 16 * FRACUNIT;  // back to normal

    mo->pos[VX] += mo->momx;
    mo->pos[VY] += mo->momy;
    mo->tracer = actor->target;
}
*/

/**
 * kaiser - Too lazy to add a new action, instead I'll just borrow this one.
 * DJS - yup you are lazy :P
 *
 * TODO:    Implement this properly as two seperate actions.
 */
void C_DECL A_SkelMissile(mobj_t *actor)
{
    mobj_t  *mo;

    if(actor->type == MT_STALKER)
    {
        if(!((actor->flags & MF_SOLID) && (actor->flags & MF_SHOOTABLE)))
        {
            actor->flags |= MF_SOLID;
            actor->flags |= MF_SHOOTABLE;

            P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_HFOG);
            S_StartSound(sfx_stlktp, actor);
            return;
        }

        if(!actor->target)
            return;

        if(P_Random() < 64)
        {
            P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_HFOG);

            S_StartSound(sfx_stlktp, actor);
            P_SetMobjState(actor, S_STALK_HIDE);
            actor->flags &= ~MF_SOLID;
            actor->flags &= ~MF_SHOOTABLE;

            memcpy(actor->pos, actor->target->pos, sizeof(actor->pos));
            actor->pos[VZ] += 32;
        }
        else
        {
            A_FaceTarget(actor);
            mo = P_SpawnMissile(actor, actor->target, MT_TRACER);

            mo->pos[VX] += mo->momx;
            mo->pos[VY] += mo->momy;
            mo->tracer = actor->target;
        }
    }
    else
    {
        if(!actor->target)
            return;

        A_FaceTarget(actor);
        mo = P_SpawnMissile(actor, actor->target, MT_TRACER);

        mo->pos[VX] += mo->momx;
        mo->pos[VY] += mo->momy;
        mo->tracer = actor->target;
    }
}

void C_DECL A_Tracer(mobj_t *actor)
{
    angle_t exact;
    fixed_t dist;
    fixed_t slope;
    mobj_t *dest;
    mobj_t *th;

    if(gametic & 3)
        return;

    // spawn a puff of smoke behind the rocket
    P_SpawnCustomPuff(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_ROCKETPUFF);

    th = P_SpawnMobj(actor->pos[VX] - actor->momx,
                     actor->pos[VY] - actor->momy,
                     actor->pos[VZ], MT_SMOKE);

    th->momz = FRACUNIT;
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // adjust direction
    dest = actor->tracer;

    if(!dest || dest->health <= 0)
        return;

    // change angle
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
    actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
    actor->momy = FixedMul(actor->info->speed, finesine[exact]);

    // change slope
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);

    dist = dist / actor->info->speed;

    if(dist < 1)
        dist = 1;
    slope = (dest->pos[VZ] + 40 * FRACUNIT - actor->pos[VZ]) / dist;

    if(slope < actor->momz)
        actor->momz -= FRACUNIT / 8;
    else
        actor->momz += FRACUNIT / 8;
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
    int     damage;

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

/*
 * Mancubus attack:
 * Firing three missiles (bruisers) in three different directions?
 * ...Doesn't look like it.
 */
void C_DECL A_FatRaise(mobj_t *actor)
{
    A_FaceTarget(actor);
    S_StartSound(sfx_manatk, actor);
}

/*
 * d64tc: Used for mancubus projectile.
 */
void FatFire(mobj_t *actor, int type, int spread, int angle,
             fixed_t distance, fixed_t height)
{
    mobj_t *mo;
    angle_t ang;
    fixed_t pos[3];

    ang = (actor->angle + angle) >> ANGLETOFINESHIFT;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(distance, finecosine[ang]);
    pos[VY] += FixedMul(distance, finesine[ang]);
    pos[VZ] += -actor->floorclip + height;

    mo = P_SpawnMotherMissile(pos[VX], pos[VY], pos[VZ], actor, actor->target,
                              type);

    mo->angle += spread;
    ang = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[ang]);
    mo->momy = FixedMul(mo->info->speed, finesine[ang]);
}

/*
void C_DECL A_FatAttack1(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Change direction  to ...
    actor->angle += FATSPREAD;
    P_SpawnMissile(actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}
*/

/**
 * d64tc
 */
void C_DECL A_FatAttack1(mobj_t *actor)
{
    FatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    FatFire(actor, MT_FATSHOT, FATSPREAD * 1.5, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
}

/*
void C_DECL A_FatAttack2(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);
    // Now here choose opposite deviation.
    actor->angle -= FATSPREAD;
    P_SpawnMissile(actor, actor->target, MT_FATSHOT);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD * 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}
*/

/**
 * d64tc
 */
void C_DECL A_FatAttack2(mobj_t *actor)
{
    FatFire(actor, MT_FATSHOT, -(FATSPREAD * 1.5), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_LONG, FAT_ARM_HEIGHT);
    FatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/*
void C_DECL A_FatAttack3(mobj_t *actor)
{
    mobj_t *mo;
    int     an;

    A_FaceTarget(actor);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle -= FATSPREAD / 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);

    mo = P_SpawnMissile(actor, actor->target, MT_FATSHOT);
    mo->angle += FATSPREAD / 2;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[an]);
    mo->momy = FixedMul(mo->info->speed, finesine[an]);
}
*/

/**
 * d64tc
 */
void C_DECL A_FatAttack3(mobj_t *actor)
{
    FatFire(actor, MT_FATSHOT, FATSPREAD / 4, FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
    FatFire(actor, MT_FATSHOT, -(FATSPREAD / 4), -FAT_DELTAANGLE,
            FAT_ARM_EXTENSION_SHORT, FAT_ARM_HEIGHT);
}

/*
 * LostSoul Attack:
 * Fly at the player like a missile.
 */
void C_DECL A_SkullAttack(mobj_t *actor)
{
    mobj_t *dest;
    angle_t an;
    int     dist;

    if(!actor->target)
        return;

    dest = actor->target;
    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attacksound, actor);
    A_FaceTarget(actor);
    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(SKULLSPEED, finecosine[an]);
    actor->momy = FixedMul(SKULLSPEED, finesine[an]);
    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist = dist / SKULLSPEED;

    if(dist < 1)
        dist = 1;

    actor->momz = (dest->pos[VZ] + (dest->height >> 1) - actor->pos[VZ]) / dist;
}

/*
 * PainElemental Attack:
 * Spawn a lost soul and launch it at the target
 */
void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
    fixed_t pos[3];

    mobj_t *newmobj;
    angle_t an;
    int     prestep;
    int     count;
    sector_t  *sec;
    thinker_t *currentthinker;

    // DJS - Compat option for unlimited lost soul spawns
    if(cfg.maxskulls)
    {
        // count total number of skull currently on the level
        count = 0;

        currentthinker = thinkercap.next;
        while(currentthinker != &thinkercap)
        {
            if((currentthinker->function == P_MobjThinker) &&
               ((mobj_t *) currentthinker)->type == MT_SKULL)
                count++;
            currentthinker = currentthinker->next;
        }

        // if there are already 20 skulls on the level,
        // don't spit another one
        if(count > 20)
            return;
    }

    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;

    prestep =
        4 * FRACUNIT + 3 * (actor->info->radius +
                            mobjinfo[MT_SKULL].radius) / 2;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(prestep, finecosine[an]);
    pos[VY] += FixedMul(prestep, finesine[an]);
    pos[VZ] += 8 * FRACUNIT;

    // DJS - Compat option to prevent spawning lost souls inside walls /from prBoom
    if(cfg.allowskullsinwalls)
    {
        newmobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SKULL);
    }
    else
    {
        // Check whether the Lost Soul is being fired through a 1-sided
        // wall or an impassible line, or a "monsters can't cross" line.
        // If it is, then we don't allow the spawn. This is a bug fix, but
        // it should be considered an enhancement, since it may disturb
        // existing demos, so don't do it in compatibility mode.

        if(P_CheckSides(actor, pos[VX], pos[VY]))
            return;

        newmobj = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_SKULL);
        sec = P_GetPtrp(newmobj->subsector, DMU_SECTOR);

        // Check to see if the new Lost Soul's z value is above the
        // ceiling of its new sector, or below the floor. If so, kill it.
        if((newmobj->pos[VZ] > (P_GetFixedp(sec, DMU_CEILING_HEIGHT)
                                - newmobj->height)) ||
            (newmobj->pos[VZ] < P_GetFixedp(sec, DMU_FLOOR_HEIGHT)))
        {
            // kill it immediately
            P_DamageMobj(newmobj,actor,actor,10000);
            return;
        }
    }

    // Check for movements.
    // killough $dropoff_fix
    if(!P_TryMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false, false))
    {
        // kill it immediately
        P_DamageMobj(newmobj, actor, actor, 10000);
        return;
    }

    newmobj->target = actor->target;
    A_SkullAttack(newmobj);
}

/**
 * PainElemental Attack:
 * Spawn a lost soul and launch it at the target
 */
void C_DECL A_PainAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    A_FaceTarget(actor);
    //A_PainShootSkull(actor, actor->angle);

    // d64tc - Shoots two lost souls from left and right side.
    A_PainShootSkull(actor, actor->angle + ANG270);
    A_PainShootSkull(actor, actor->angle + ANG90);
}

void C_DECL A_PainDie(mobj_t *actor)
{
    A_Fall(actor);
    A_PainShootSkull(actor, actor->angle + ANG90);
    A_PainShootSkull(actor, actor->angle + ANG180);
    A_PainShootSkull(actor, actor->angle + ANG270);
}

/**
 * d64tc: Rocket Trail Puff
 * kaiser - Current Rocket Puff code unknown because I know squat.
 *          A fixed version of the pain attack code.
 */
void A_Rocketshootpuff(mobj_t *actor, angle_t angle)
{
    angle_t     an;
    int         prestep;
    fixed_t     pos[3];
    mobj_t     *mo;

    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;

    prestep = 4 * FRACUNIT + 3 *
              (actor->info->radius + mobjinfo[MT_ROCKETPUFF].radius) / 2;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += FixedMul(prestep, finecosine[an]);
    pos[VY] += FixedMul(prestep, finesine[an]);
    pos[VZ] += 8 * FRACUNIT;

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_ROCKETPUFF);

    // Check for movements.
    // killough $dropoff_fix
    if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY], false, false))
    {
        // kill it immediately
        P_DamageMobj(mo, actor, actor, 10000);
        return;
    }
}

void C_DECL A_Scream(mobj_t *actor)
{
    int     sound;

    switch (actor->info->deathsound)
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
        sound = actor->info->deathsound;
        break;
    }

    // Check for bosses.
    if(actor->type == MT_SPIDER || actor->type == MT_CYBORG ||
       actor->type == MT_BITCH) // d64tc added "|| actor->type == MT_BITCH"
    {
        // full volume
        S_StartSound(sound | DDSF_NO_ATTENUATION, NULL);
        actor->reactiontime += 30;  // d64tc
    }
    else
        S_StartSound(sound, actor);

    // d64tc >
    if(actor->type == MT_ACID)
    {
        int     i;
        mobj_t *mo = NULL;

        for(i = 0; i < 16; ++i)
        {
            mo = P_SpawnMissile(actor, actor, MT_ACIDMISSILE);
            if(mo)
            {
                mo->momx = (P_Random() - 128) << 11;
                mo->momy = (P_Random() - 128) << 11;
                mo->momz = FRACUNIT * 10 + (P_Random() << 10);
                mo->target = actor;
            }
        }
    }
    // < d64tc
}

/**
 * d64tc
 */
void C_DECL A_BossExplode(mobj_t *actor)
{
    mobj_t *mo;
    fixed_t pos[3];

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VX] += (P_Random() - 128) << 11;
    pos[VY] += (P_Random() - 128) << 11;
    pos[VZ] += actor->height >> 1;

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_KABOOM);
    if(mo)
    {
        S_StartSound(sfx_barexp, mo);
        mo->momx = (P_Random() - 128) << 11;
        mo->momy = (P_Random() - 128) << 11;
        mo->target = actor;
    }

    actor->reactiontime--;
    if(actor->reactiontime <= 0)
    {
        P_SetMobjState(actor, actor->info->deathstate + 2);
    }
}

/**
 * d64tc
 */
boolean P_CheckAcidRange(mobj_t *actor)
{
    fixed_t dist;

    if(!actor->target)
        return false;

    dist = P_ApproxDistance(actor->target->pos[VX] - actor->pos[VX],
                            actor->target->pos[VY] - actor->pos[VY]);

    dist = P_ApproxDistance(dist, (actor->target->pos[VZ] + (actor->target->height >> 1)) -
                                  (actor->pos[VZ] + (actor->height >> 1)));

    if(dist >= ACIDRANGE - 14 * FRACUNIT + actor->target->info->radius)
        return false;

    if(!P_CheckSight(actor, actor->target))
        return false;

    return true;
}

/**
 * d64tc
 */
void C_DECL A_SpitAcid(mobj_t* actor)
{
    int     i;
    angle_t an;
    mobj_t *mo;

    if(!actor->target)
        return;

    if(P_CheckAcidRange(actor))
    {
        A_FaceTarget(actor);
        S_StartSound(sfx_sgtatk, actor);

        for(i = 0; i < 16; ++i)
        {
            mo = P_SpawnMissile(actor, actor->target, MT_ACIDMISSILE);
            if(mo)
            {
                mo->angle = actor->angle;
                an = mo->angle >> ANGLETOFINESHIFT;

                mo->momx = FixedMul(mo->info->speed, finecosine[an]) +
                           P_Random() % 3 * FRACUNIT;
                mo->momy = FixedMul(mo->info->speed, finesine[an]) +
                           P_Random() % 3 * FRACUNIT;
                mo->momz = FRACUNIT * 4 + (P_Random() << 10);

                mo->target = actor;
            }
        }

        actor->info->speed = 7 * FRACUNIT;

        for(i= S_ACID_RUN1; i <= S_ACID_RUN8; ++i)
            states[i].tics = 3;
    }
    else
    {
        P_SetMobjState(actor, actor->info->seestate);
    }
}

/**
 * d64tc
 */
void C_DECL A_AcidCharge(mobj_t *actor)
{
    int     i;

    if(!actor->target)
        return;

    if(!(P_CheckAcidRange(actor)))
    {
        A_FaceTarget(actor);
        A_Chase(actor);

        for(i = S_ACID_RUN1; i <= S_ACID_RUN8; ++i)
            states[i].tics = 1;

        actor->info->speed = 15 * FRACUNIT;
    }
    else
    {
        P_SetMobjState(actor, actor->info->missilestate + 1);
    }
}

void C_DECL A_XScream(mobj_t *actor)
{
    S_StartSound(sfx_slop, actor);
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painsound)
        S_StartSound(actor->info->painsound, actor);
}

void C_DECL A_Fall(mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


void C_DECL A_Explode(mobj_t *thingy)
{
    // d64tc >
    int radius;

    if(thingy->type == MT_GRENADE)
        radius = 48;
    else
        radius = 128;
    // < d64tc

    //P_RadiusAttack(thingy, thingy->target, 128); // d64tc
    P_RadiusAttack(thingy, thingy->target, radius);
}

/**
 * Possibly trigger special effects if on first boss level
 *
 * kaiser - Removed exit special at end to allow MT_FATSO to properly
 *          work in Map33 for d64tc.
 */
void C_DECL A_BossDeath(mobj_t *mo)
{
    thinker_t *th;
    mobj_t *mo2;
    line_t*  dummyLine;
    int     i;

    // Has the boss already been killed?
    if(bossKilled)
        return;

    //if(gamemode == commercial) // d64tc
    {
        /* d64tc
        if(gamemap != 7)
            return;
        if((mo->type != MT_FATSO) && (mo->type != MT_BABY))
            return;
        */

        // d64tc >
        if((gamemap != 1) && (gamemap != 30) && (gamemap != 32) &&
           (gamemap != 33) && (gamemap != 35))
            return;

        if((mo->type != MT_BITCH) && (mo->type != MT_CYBORG) &&
           (mo->type != MT_BARREL) && (mo->type != MT_FATSO))
            return;
        // < d64tc
    }
/* d64tc >
    else
    {
        switch (gameepisode)
        {
        case 1:
            if(gamemap != 8)
                return;

            // Ultimate DOOM behavioral change
            // This test was added so that the (variable) effects of the
            // 666 special would only take effect when the last Baron
            // died and not ANY monster.
            // Many classic PWADS such as "Doomsday of UAC" (UAC_DEAD.wad)
            // relied on the old behaviour and cannot be completed.

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
            switch (gamemap)
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
< d64tc */

    // make sure there is a player alive for victory
    for(i = 0; i < MAXPLAYERS; i++)
        if(players[i].plr->ingame && players[i].health > 0)
            break;

    if(i == MAXPLAYERS)
        return;                 // no one left alive, so do not end game

    // scan the remaining thinkers to see
    // if all bosses are dead
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(th->function != P_MobjThinker)
            continue;

        mo2 = (mobj_t *) th;
        if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
        {
            // other boss not dead
            return;
        }
    }

    // d64tc >
    if(gamemap == 1)
    {
        if(mo->type == MT_BARREL)
        {
            dummyLine = P_AllocDummyLine();
            P_XLine(dummyLine)->tag = 666;
            EV_DoDoor(dummyLine, blazeRaise);

            P_FreeDummyLine(dummyLine);
            return;
        }
    }
    else if(gamemap == 30)
    {
        if(mo->type == MT_BITCH)
        {
            G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
        }
    }
    else if(gamemap == 32 || gamemap == 33)
    {
        if(mo->type == MT_CYBORG)
        {
            dummyLine = P_AllocDummyLine();
            P_XLine(dummyLine)->tag = 666;
            EV_DoDoor(dummyLine, blazeRaise);

            P_FreeDummyLine(dummyLine);
            return;
        }

        if(mo->type == MT_FATSO)
        {
            G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
        }
    }
    else if(gamemap == 35)
    {
        if(mo->type == MT_CYBORG)
        {
            G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
        }
    }
    // < d64tc

/* d64tc >
    // victory!
    if(gamemode == commercial)
    {
        if(gamemap == 7)
        {
            if(mo->type == MT_FATSO)
            {
                dummyLine = P_AllocDummyLine();
                P_XLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                return;
            }

            if(mo->type == MT_BABY)
            {
                dummyLine = P_AllocDummyLine();
                P_XLine(dummyLine)->tag = 667;
                EV_DoFloor(dummyLine, raiseToTexture);
                P_FreeDummyLine(dummyLine);

                // Only activate once (rare Dead simple bug)
                bossKilled = true;
                return;
            }
        }
    }
    else
    {
        switch (gameepisode)
        {
        case 1:
            dummyLine = P_AllocDummyLine();
            P_XLine(dummyLine)->tag = 666;
            EV_DoFloor(dummyLine, lowerFloorToLowest);
            P_FreeDummyLine(dummyLine);
            bossKilled = true;
            return;
            break;

        case 4:
            switch (gamemap)
            {
            case 6:
                dummyLine = P_AllocDummyLine();
                P_XLine(dummyLine)->tag = 666;
                EV_DoDoor(dummyLine, blazeOpen);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;

            case 8:
                dummyLine = P_AllocDummyLine();
                P_XLine(dummyLine)->tag = 666;
                EV_DoFloor(dummyLine, lowerFloorToLowest);
                P_FreeDummyLine(dummyLine);
                bossKilled = true;
                return;
                break;
            }
        }
    }

    G_LeaveLevel(G_GetLevelNumber(gameepisode, gamemap), 0, false);
*/
}

void C_DECL A_Hoof(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
    S_StartSound(sfx_hoof |
                 (gamemode != commercial &&
                  gamemap == 8 ? DDSF_NO_ATTENUATION : 0), mo);
    A_Chase(mo);
}

void C_DECL A_Metal(mobj_t *mo)
{
    // HACKAMAXIMO: Only play very loud sounds in map 8.
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

void C_DECL A_OpenShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(sfx_dbopn, player->plr->mo);
}

void C_DECL A_LoadShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(sfx_dbload, player->plr->mo);
}


void C_DECL A_CloseShotgun2(player_t *player, pspdef_t * psp)
{
    S_StartSound(sfx_dbcls, player->plr->mo);
    A_ReFire(player, psp);
}

/*
 * killough 3/26/98: initialize icon landings at level startup,
 * rather than at boss wakeup, to prevent savegame-related crashes
 */
void P_SpawnBrainTargets(void)
{
    thinker_t *thinker;
    mobj_t *m;

    // find all the target spots
    for(thinker = thinkercap.next; thinker != &thinkercap;
        thinker = thinker->next)
    {
        if(thinker->function != P_MobjThinker)
            continue;  // not a mobj

        m = (mobj_t *) thinker;

        if(m->type == MT_BOSSTARGET)
        {   // killough 2/7/98: remove limit on icon landings:
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
    int     x;
    int     y;
    int     z;
    mobj_t *th;

    for(x = mo->pos[VX] - 196 * FRACUNIT; x < mo->pos[VX] + 320 * FRACUNIT;
        x += FRACUNIT * 8)
    {
        y = mo->pos[VY] - 320 * FRACUNIT;
        z = 128 + P_Random() * 2 * FRACUNIT;
        th = P_SpawnMobj(x, y, z, MT_ROCKET);
        th->momz = P_Random() * 512;

        P_SetMobjState(th, S_BRAINEXPLODE1);

        th->tics -= P_Random() & 7;
        if(th->tics < 1)
            th->tics = 1;
    }

    S_StartSound(sfx_bosdth, NULL);
}

void C_DECL A_BrainExplode(mobj_t *mo)
{
    int     x;
    int     y;
    int     z;
    mobj_t *th;

    x = mo->pos[VX] + (P_Random() - P_Random()) * 2048;
    y = mo->pos[VY];
    z = 128 + P_Random() * 2 * FRACUNIT;
    th = P_SpawnMobj(x, y, z, MT_ROCKET);
    th->momz = P_Random() * 512;

    P_SetMobjState(th, S_BRAINEXPLODE1);

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
    mobj_t *targ;
    mobj_t *newmobj;

    if(!numbraintargets)     // killough 4/1/98: ignore if no targets
        return;
#if 0
    // DJS - is this even used?
    brain.easy ^= 1;
    if(gameskill <= sk_easy && (!brain.easy))
        return;

    // shoot a cube at current target
    targ = braintargets[brain.targeton++];
    brain.targeton %= numbraintargets;

    // spawn brain missile
    newmobj = P_SpawnMissile(mo, targ, MT_SPAWNSHOT);
    newmobj->target = targ;
    newmobj->reactiontime =
        ((targ->pos[VY] - mo->pos[VY]) / newmobj->momy) / newmobj->state->tics;

    S_StartSound(sfx_bospit, NULL);
#endif
}

/*
 * Travelling cube sound
 */
void C_DECL A_SpawnSound(mobj_t *mo)
{
    S_StartSound(sfx_boscub, mo);
    A_SpawnFly(mo);
}

void C_DECL A_SpawnFly(mobj_t *mo)
{
    mobj_t *newmobj;
//    mobj_t *fog;
    mobj_t *targ;
    int     r;
    mobjtype_t type;

    if(--mo->reactiontime)
        return;                 // still flying

    targ = mo->target;

    // First spawn teleport fog.
//    fog = P_SpawnMobj(targ->pos[VX], targ->pos[VY], targ->pos[VZ], MT_SPAWNFIRE);
//    S_StartSound(sfx_telept, fog);

    // Randomly select monster to spawn.
    r = P_Random();

    // Probability distribution (kind of :),
    // decreasing likelihood.
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
    else if(r < 172)
        type = MT_NIGHTMARECACO; // d64tc was "MT_UNDEAD"
    else if(r < 192)
        type = MT_BABY;
    else if(r < 222)
        type = MT_FATSO;
    else if(r < 246)
        type = MT_KNIGHT;
    else
        type = MT_BRUISER;

    newmobj = P_SpawnMobj(targ->pos[VX], targ->pos[VY], targ->pos[VZ], type);
    if(P_LookForPlayers(newmobj, true))
        P_SetMobjState(newmobj, newmobj->info->seestate);

    // telefrag anything in this spot
    P_TeleportMove(newmobj, newmobj->pos[VX], newmobj->pos[VY], false);

    // remove self (i.e., cube).
    P_RemoveMobj(mo);
}

void C_DECL A_PlayerScream(mobj_t *mo)
{
    // Default death sound.
    int     sound = sfx_pldeth;

    if((gamemode == commercial) && (mo->health < -50))
    {
        // IF THE PLAYER DIES
        // LESS THAN -50% WITHOUT GIBBING
        sound = sfx_pdiehi;
    }

    S_StartSound(sound, mo);
}
