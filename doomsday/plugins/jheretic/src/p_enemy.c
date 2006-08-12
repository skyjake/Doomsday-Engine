/*
 * Enemy thinking, AI.
 * Action Pointer Functions that are associated with states/frames.
 *
 * Enemies are allways spawned with targetplayer = -1, threshold = 0
 * Most monsters are spawned unaware of all players,
 * but some can be made preaware
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  pragma optimize("g", off)
#endif

#include "jheretic.h"

#include "dmu_lib.h"
#include "p_spechit.h"

// MACROS ------------------------------------------------------------------

#define MAX_BOSS_SPOTS      8

#define MONS_LOOK_RANGE     (20*64*FRACUNIT)
#define MONS_LOOK_LIMIT     64

#define MNTR_CHARGE_SPEED   (13*FRACUNIT)

#define MAX_GEN_PODS        16

#define BODYQUESIZE         32

// TYPES -------------------------------------------------------------------

typedef struct bossspot_e {
    fixed_t x;
    fixed_t y;
    angle_t angle;
} bossspot_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean felldown;        //$dropoff_fix: used to flag pushed off ledge
extern line_t *blockline;       // $unstuck: blocking linedef
extern fixed_t   tmbbox[4];     // for line intersection checks

// PUBLIC DATA DEFINITIONS -------------------------------------------------

mobj_t *soundtarget;

fixed_t xspeed[8] =
    { FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000, 0, 47000 };
fixed_t yspeed[8] =
    { 0, 47000, FRACUNIT, 47000, 0, -47000, -FRACUNIT, -47000 };

dirtype_t opposite[] =
    { DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST, DI_EAST, DI_NORTHEAST,
    DI_NORTH, DI_NORTHWEST, DI_NODIR };

dirtype_t diags[] = { DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST };

mobj_t *bodyque[BODYQUESIZE];
int     bodyqueslot;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int BossSpotCount;
static bossspot_t BossSpots[MAX_BOSS_SPOTS];

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

// CODE --------------------------------------------------------------------

/*
 * Called at level load.
 */
void P_InitMonsters(void)
{
    BossSpotCount = 0;
}

void P_AddBossSpot(fixed_t x, fixed_t y, angle_t angle)
{
    if(BossSpotCount == MAX_BOSS_SPOTS)
        Con_Error("Too many boss spots.");

    BossSpots[BossSpotCount].x = x;
    BossSpots[BossSpotCount].y = y;
    BossSpots[BossSpotCount].angle = angle;
    BossSpotCount++;
}

/*
 * Wakes up all monsters in this sector
 */
void P_RecursiveSound(sector_t *sec, int soundblocks)
{
    int     i;
    line_t *check;
    xsector_t *xsec = P_XSector(sec);
    sector_t *other;

    // Have we already flooded this sector?
    if(P_GetIntp(sec, DMU_VALID_COUNT) == validCount &&
       xsec->soundtraversed <= soundblocks + 1)
        return;

    P_SetIntp(sec, DMU_VALID_COUNT, validCount);

    xsec->soundtraversed = soundblocks + 1;
    xsec->soundtarget = soundtarget;
    for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
    {
        check = P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i);

        if(!(P_GetIntp(check, DMU_FLAGS) & ML_TWOSIDED))
            continue;

        P_LineOpening(check);

        // Closed door?
        if(openrange <= 0)
            continue;

        if(P_GetPtrp(check, DMU_FRONT_SECTOR) == sec)
        {
            other = P_GetPtrp(check, DMU_BACK_SECTOR);
        }
        else
        {
            other = P_GetPtrp(check, DMU_FRONT_SECTOR);
        }

        if(P_GetIntp(check, DMU_FLAGS) & ML_SOUNDBLOCK)
        {
            if(!soundblocks)
                P_RecursiveSound(other, 1);
        }
        else
        {
            P_RecursiveSound(other, soundblocks);
        }
    }
}

/*
 * If a monster yells at a player, it will alert other monsters to the
 * player.
 */
void P_NoiseAlert(mobj_t *target, mobj_t *emmiter)
{
    soundtarget = target;
    validCount++;

    P_RecursiveSound(P_GetPtrp(emmiter->subsector,
                     DMU_SECTOR), 0);
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
        dist = P_ApproxDistance(dist, (pl->pos[VZ] + (pl->height >> 1)) -
                                   (actor->pos[VZ] + (actor->height >> 1)));

    range = (MELEERANGE - 20 * FRACUNIT + pl->info->radius);
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
        // The target just hit the enemy, so fight back!
        actor->flags &= ~MF_JUSTHIT;
        return true;
    }

    if(actor->reactiontime)
        return false; // Don't attack yet

    dist =
        (P_ApproxDistance
         (actor->pos[VX] - actor->target->pos[VX],
          actor->pos[VY] - actor->target->pos[VY]) >> FRACBITS) - 64;

    // No melee attack, so fire more frequently
    if(!actor->info->meleestate)
        dist -= 128;

    // Imp's fly attack from far away
    if(actor->type == MT_IMP)
        dist >>= 1;

    if(dist > 200)
        dist = 200;

    if(P_Random() < dist)
        return false;

    return true;
}

/*
 * Move in the current direction
 * returns false if the move is blocked
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

        if(!P_SpecHitSize())
            return false;

        actor->movedir = DI_NODIR;
        good = false;
        while((ld = P_PopSpecHit()) != NULL)
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

            if(P_UseSpecialLine(actor, ld, 0))
                good |= ld == blockline ? 1 : 2;
        }

        if(!good || cfg.monstersStuckInDoors)
            return good;
        else
            return (P_Random() >= 230) || (good & 1);
    }
    else
    {
        // "servo": movement smoothing
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
 * Attempts to move actor in its current (ob->moveangle) direction.
 * If blocked by either a wall or an actor returns FALSE.
 * If move is either clear of block only by a door, returns TRUE and sets.
 * If a door is in the way, an OpenDoor call is made to start it opening.
 */
boolean P_TryWalk(mobj_t *actor)
{
    // killough $dropoff_fix
    if(!P_Move(actor, false))
        return false;

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

boolean P_LookForMonsters(mobj_t *actor)
{
    int     count;
    mobj_t *mo;
    thinker_t *think;

    if(!P_CheckSight(players[0].plr->mo, actor))
        return false;  // Player can't see the monster

    count = 0;
    for(think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        // Not a mobj thinker?
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;

        // Not a valid monster?
        if(!(mo->flags & MF_COUNTKILL) || (mo == actor) || (mo->health <= 0))
            continue;

        // Out of range?
        if(P_ApproxDistance(actor->pos[VX] - mo->pos[VX],
                            actor->pos[VY] - mo->pos[VY]) >
           MONS_LOOK_RANGE)
            continue;

        if(P_Random() < 16)
            continue;  // Skip

        if(count++ > MONS_LOOK_LIMIT)
            return false;  // Stop searching

        // Out of sight?
        if(!P_CheckSight(actor, mo))
            continue;

        // Found a target monster
        actor->target = mo;
        return true;
    }
    return false;
}

/*
 * If allaround is false, only look 180 degrees in front
 * returns true if a player is targeted
 */
boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
    int     c;
    int     stop;
    player_t *player;
    sector_t *sector;
    angle_t an;
    fixed_t dist;
    mobj_t *plrmo;
    int     playerCount;

    // If in single player and player is dead, look for monsters
    if(!IS_NETGAME && players[0].health <= 0)
        return P_LookForMonsters(actor);

    for(c = playerCount = 0; c < MAXPLAYERS; c++)
        if(players[c].plr->ingame)
            playerCount++;

    // Are there any players?
    if(!playerCount)
        return false;

    sector = P_GetPtrp(actor->subsector, DMU_SECTOR);
    c = 0;
    stop = (actor->lastlook - 1) & 3;
    for(;; actor->lastlook = (actor->lastlook + 1) & 3)
    {
        if(!players[actor->lastlook].plr->ingame)
            continue;

        if(c++ == 2 || actor->lastlook == stop)
            return false;  // Done looking

        player = &players[actor->lastlook];
        plrmo = player->plr->mo;

        // Dead?
        if(player->health <= 0)
            continue;

        // Out of sight?
        if(!P_CheckSight(actor, plrmo))
            continue;

        if(!allaround)
        {
            an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                 plrmo->pos[VX], plrmo->pos[VY]) - actor->angle;
            if(an > ANG90 && an < ANG270)
            {
                dist =
                    P_ApproxDistance(plrmo->pos[VX] - actor->pos[VX],
                                     plrmo->pos[VY] - actor->pos[VY]);
                // if real close, react anyway
                if(dist > MELEERANGE)
                    continue;   // behind back
            }
        }

        // Is player invisible?
        if(plrmo->flags & MF_SHADOW)
        {
            if((P_ApproxDistance(plrmo->pos[VX] - actor->pos[VX],
                                 plrmo->pos[VY] - actor->pos[VY]) >
                2 * MELEERANGE) &&
               P_ApproxDistance(plrmo->momx, plrmo->momy) < 5 * FRACUNIT)
            {
                // Player is sneaking - can't detect
                return false;
            }

            if(P_Random() < 225)
            {
                // Player isn't sneaking, but still didn't detect
                return false;
            }
        }

        actor->target = plrmo;
        return true;
    }
    return false;
}

/*
 * Stay in state until a player is sighted
 */
void C_DECL A_Look(mobj_t *actor)
{
    mobj_t *targ;

    // Any shot will wake up
    actor->threshold = 0;
    targ = P_XSector(P_GetPtrp(actor->subsector,
                     DMU_SECTOR))->soundtarget;
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
            S_StartSound(sound, NULL);  // Full volume
        else
            S_StartSound(sound, actor);
    }
    P_SetMobjState(actor, actor->info->seestate);
}

/*
 * Actor has a melee attack, so it tries to close as fast as possible
 */
void C_DECL A_Chase(mobj_t *actor)
{
    int     delta;

    if(actor->reactiontime)
        actor->reactiontime--;

    // Modify target threshold
    if(actor->threshold)
        actor->threshold--;

    if(gameskill == sk_nightmare || cfg.fastMonsters )
    {
        // Monsters move faster in nightmare mode
        actor->tics -= actor->tics / 2;
        if(actor->tics < 3)
            actor->tics = 3;
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
            return;  // got a new target

        P_SetMobjState(actor, actor->info->spawnstate);
        return;
    }

    // don't attack twice in a row
    if(actor->flags & MF_JUSTATTACKED)
    {
        actor->flags &= ~MF_JUSTATTACKED;

        if(gameskill != sk_nightmare)
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
        if(gameskill < sk_nightmare && actor->movecount)
            goto nomissile;

        if(!P_CheckMissileRange(actor))
            goto nomissile;

        P_SetMobjState(actor, actor->info->missilestate);

        actor->flags |= MF_JUSTATTACKED;
        return;
    }
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
        if(actor->type == MT_WIZARD && P_Random() < 128)
        {
            S_StartSound(actor->info->seesound, actor);
        }
        else if(actor->type == MT_SORCERER2)
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

    // Is target a ghost?
    if(actor->target->flags & MF_SHADOW)
    {
        actor->angle += (P_Random() - P_Random()) << 21;
    }
}

void C_DECL A_Pain(mobj_t *actor)
{
    if(actor->info->painsound)
        S_StartSound(actor->info->painsound, actor);
}

void C_DECL A_DripBlood(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - P_Random()) << 11),
                     actor->pos[VY] + ((P_Random() - P_Random()) << 11),
                     actor->pos[VZ], MT_BLOOD);

    mo->momx = (P_Random() - P_Random()) << 10;
    mo->momy = (P_Random() - P_Random()) << 10;

    mo->flags2 |= MF2_LOGRAV;
}

void C_DECL A_KnightAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3));
        S_StartSound(sfx_kgtat2, actor);
        return;
    }

    // Throw axe
    S_StartSound(actor->info->attacksound, actor);
    if(actor->type == MT_KNIGHTGHOST || P_Random() < 40)
    {
        // Red axe
        P_SpawnMissile(actor, actor->target, MT_REDAXE);
        return;
    }

    // Green axe
    P_SpawnMissile(actor, actor->target, MT_KNIGHTAXE);
}

void C_DECL A_ImpExplode(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_IMPCHUNK1);
    mo->momx = (P_Random() - P_Random()) << 10;
    mo->momy = (P_Random() - P_Random()) << 10;
    mo->momz = 9 * FRACUNIT;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_IMPCHUNK2);
    mo->momx = (P_Random() - P_Random()) << 10;
    mo->momy = (P_Random() - P_Random()) << 10;
    mo->momz = 9 * FRACUNIT;

    if(actor->special1 == 666)
        P_SetMobjState(actor, S_IMP_XCRASH1);  // Extreme death crash
}

void C_DECL A_BeastPuff(mobj_t *actor)
{
    if(P_Random() > 64)
    {
        P_SpawnMobj(actor->pos[VX] + ((P_Random() - P_Random()) << 10),
                    actor->pos[VY] + ((P_Random() - P_Random()) << 10),
                    actor->pos[VZ] + ((P_Random() - P_Random()) << 10), MT_PUFFY);
    }
}

void C_DECL A_ImpMeAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7));
    }
}

void C_DECL A_ImpMsAttack(mobj_t *actor)
{
    mobj_t *dest;
    angle_t an;
    int     dist;

    if(!actor->target || P_Random() > 64)
    {
        P_SetMobjState(actor, actor->info->seestate);
        return;
    }

    dest = actor->target;

    actor->flags |= MF_SKULLFLY;

    S_StartSound(actor->info->attacksound, actor);

    A_FaceTarget(actor);

    an = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(12 * FRACUNIT, finecosine[an]);
    actor->momy = FixedMul(12 * FRACUNIT, finesine[an]);

    dist = P_ApproxDistance(dest->pos[VX] - actor->pos[VX],
                            dest->pos[VY] - actor->pos[VY]);
    dist = dist / (12 * FRACUNIT);
    if(dist < 1)
        dist = 1;

    actor->momz = (dest->pos[VZ] + (dest->height >> 1) - actor->pos[VZ]) / dist;
}

/*
 * Fireball attack of the imp leader.
 */
void C_DECL A_ImpMsAttack2(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, 5 + (P_Random() & 7));
        return;
    }

    P_SpawnMissile(actor, actor->target, MT_IMPBALL);
}

void C_DECL A_ImpDeath(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags2 |= MF2_FLOORCLIP;

    if(actor->pos[VZ] <= actor->floorz)
        P_SetMobjState(actor, S_IMP_CRASH1);
}

void C_DECL A_ImpXDeath1(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;
    actor->flags |= MF_NOGRAVITY;
    actor->flags2 |= MF2_FLOORCLIP;

    actor->special1 = 666;      // Flag the crash routine
}

void C_DECL A_ImpXDeath2(mobj_t *actor)
{
    actor->flags &= ~MF_NOGRAVITY;

    if(actor->pos[VZ] <= actor->floorz)
        P_SetMobjState(actor, S_IMP_CRASH1);
}

/*
 * Returns true if the chicken morphs.
 */
boolean P_UpdateChicken(mobj_t *actor, int tics)
{
    mobj_t *fog;
    fixed_t pos[3];
    mobjtype_t moType;
    mobj_t *mo;
    mobj_t  oldChicken;

    actor->special1 -= tics;

    if(actor->special1 > 0)
        return false;

    moType = actor->special2;

    memcpy(pos, actor->pos, sizeof(pos));

    oldChicken = *actor;
    P_SetMobjState(actor, S_FREETARGMOBJ);

    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], moType);
    if(P_TestMobjLocation(mo) == false)
    {
        // Didn't fit
        P_RemoveMobj(mo);

        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_CHICKEN);

        mo->angle = oldChicken.angle;
        mo->flags = oldChicken.flags;
        mo->health = oldChicken.health;
        mo->target = oldChicken.target;

        mo->special1 = 5 * 35;  // Next try in 5 seconds
        mo->special2 = moType;
        return (false);
    }

    mo->angle = oldChicken.angle;
    mo->target = oldChicken.target;

    fog = P_SpawnMobj(pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(sfx_telept, fog);

    return true;
}

void C_DECL A_ChicAttack(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 18))
        return;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
        P_DamageMobj(actor->target, actor, actor, 1 + (P_Random() & 1));
}

void C_DECL A_ChicLook(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    A_Look(actor);
}

void C_DECL A_ChicChase(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 3))
        return;

    A_Chase(actor);
}

void C_DECL A_ChicPain(mobj_t *actor)
{
    if(P_UpdateChicken(actor, 10))
        return;

    S_StartSound(actor->info->painsound, actor);
}

void C_DECL A_Feathers(mobj_t *actor)
{
    int     i;
    int     count;
    mobj_t *mo;

    // In Pain?
    if(actor->health > 0)
        count = P_Random() < 32 ? 2 : 1;
    else // Death
        count = 5 + (P_Random() & 3);

    for(i = 0; i < count; i++)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                         actor->pos[VZ] + 20 * FRACUNIT, MT_FEATHER);
        mo->target = actor;

        mo->momx = (P_Random() - P_Random()) << 8;
        mo->momy = (P_Random() - P_Random()) << 8;
        mo->momz = FRACUNIT + (P_Random() << 9);

        P_SetMobjState(mo, S_FEATHER1 + (P_Random() & 7));
    }
}

void C_DECL A_MummyAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2));
        S_StartSound(sfx_mumat2, actor);
        return;
    }

    S_StartSound(sfx_mumat1, actor);
}

/*
 * Mummy leader missile attack.
 */
void C_DECL A_MummyAttack2(mobj_t *actor)
{
    mobj_t *mo;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(2));
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, MT_MUMMYFX1);

    if(mo != NULL)
        mo->target = actor->target;
}

void C_DECL A_MummyFX1Seek(mobj_t *actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 20);
}

void C_DECL A_MummySoul(mobj_t *mummy)
{
    mobj_t *mo;

    mo = P_SpawnMobj(mummy->pos[VX], mummy->pos[VY], mummy->pos[VZ] + 10 * FRACUNIT,
                     MT_MUMMYSOUL);
    mo->momz = FRACUNIT;
}

void C_DECL A_Sor1Pain(mobj_t *actor)
{
    actor->special1 = 20;       // Number of steps to walk fast
    A_Pain(actor);
}

void C_DECL A_Sor1Chase(mobj_t *actor)
{
    if(actor->special1)
    {
        actor->special1--;
        actor->tics -= 3;
    }

    A_Chase(actor);
}

/*
 * Sorcerer demon attack.
 */
void C_DECL A_Srcr1Attack(mobj_t *actor)
{
    mobj_t *mo;
    fixed_t momz;
    angle_t angle;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(8));
        return;
    }

    if(actor->health > (actor->info->spawnhealth / 3) * 2)
    {
        // Spit one fireball
        P_SpawnMissile(actor, actor->target, MT_SRCRFX1);
    }
    else
    {
        // Spit three fireballs
        mo = P_SpawnMissile(actor, actor->target, MT_SRCRFX1);
        if(mo)
        {
            momz = mo->momz;
            angle = mo->angle;
            P_SpawnMissileAngle(actor, MT_SRCRFX1, angle - ANGLE_1 * 3, momz);
            P_SpawnMissileAngle(actor, MT_SRCRFX1, angle + ANGLE_1 * 3, momz);
        }

        if(actor->health < actor->info->spawnhealth / 3)
        {
            // Maybe attack again
            if(actor->special1)
            {
                // Just attacked, so don't attack again
                actor->special1 = 0;
            }
            else
            {
                // Set state to attack again
                actor->special1 = 1;
                P_SetMobjState(actor, S_SRCR1_ATK4);
            }
        }
    }
}

void C_DECL A_SorcererRise(mobj_t *actor)
{
    mobj_t *mo;

    actor->flags &= ~MF_SOLID;
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SORCERER2);

    P_SetMobjState(mo, S_SOR2_RISE1);

    mo->angle = actor->angle;
    mo->target = actor->target;
}

void P_DSparilTeleport(mobj_t *actor)
{
    int     i;
    fixed_t x;
    fixed_t y;
    fixed_t prevpos[3];
    mobj_t *mo;

    // No spots?
    if(!BossSpotCount)
        return;

    i = P_Random();
    do
    {
        i++;
        x = BossSpots[i % BossSpotCount].x;
        y = BossSpots[i % BossSpotCount].y;
    } while(P_ApproxDistance(actor->pos[VX] - x, actor->pos[VY] - y) < 128 * FRACUNIT);

    memcpy(prevpos, actor->pos, sizeof(prevpos));

    if(P_TeleportMove(actor, x, y, false))
    {
        mo = P_SpawnMobj(prevpos[VX], prevpos[VY], prevpos[VZ], MT_SOR2TELEFADE);

        S_StartSound(sfx_telept, mo);

        P_SetMobjState(actor, S_SOR2_TELE1);

        S_StartSound(sfx_telept, actor);

        actor->pos[VZ] = actor->floorz;
        actor->angle = BossSpots[i % BossSpotCount].angle;
        actor->momx = actor->momy = actor->momz = 0;
    }
}

void C_DECL A_Srcr2Decide(mobj_t *actor)
{
    static int chance[] = {
        192, 120, 120, 120, 64, 64, 32, 16, 0
    };

    // No spots?
    if(!BossSpotCount)
        return;

    if(P_Random() < chance[actor->health / (actor->info->spawnhealth / 8)])
    {
        P_DSparilTeleport(actor);
    }
}

void C_DECL A_Srcr2Attack(mobj_t *actor)
{
    int     chance;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, NULL);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(20));
        return;
    }

    chance = actor->health < actor->info->spawnhealth / 2 ? 96 : 48;
    if(P_Random() < chance)
    {
        // Wizard spawners
        P_SpawnMissileAngle(actor, MT_SOR2FX2, actor->angle - ANG45,
                            FRACUNIT / 2);
        P_SpawnMissileAngle(actor, MT_SOR2FX2, actor->angle + ANG45,
                            FRACUNIT / 2);
    }
    else
    {
        // Blue bolt
        P_SpawnMissile(actor, actor->target, MT_SOR2FX1);
    }
}

void C_DECL A_BlueSpark(mobj_t *actor)
{
    int     i;
    mobj_t *mo;

    for(i = 0; i < 2; i++)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_SOR2FXSPARK);

        mo->momx = (P_Random() - P_Random()) << 9;
        mo->momy = (P_Random() - P_Random()) << 9;
        mo->momz = FRACUNIT + (P_Random() << 8);
    }
}

void C_DECL A_GenWizard(mobj_t *actor)
{
    mobj_t *mo;
    mobj_t *fog;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                     actor->pos[VZ] - mobjinfo[MT_WIZARD].height / 2, MT_WIZARD);

    if(P_TestMobjLocation(mo) == false)
    {
        // Didn't fit
        P_RemoveMobj(mo);
        return;
    }

    actor->momx = actor->momy = actor->momz = 0;

    P_SetMobjState(actor, mobjinfo[actor->type].deathstate);

    actor->flags &= ~MF_MISSILE;

    fog = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_TFOG);
    S_StartSound(sfx_telept, fog);
}

void C_DECL A_Sor2DthInit(mobj_t *actor)
{
    // Set the animation loop counter
    actor->special1 = 7;

    // Kill monsters early
    P_Massacre();
}

void C_DECL A_Sor2DthLoop(mobj_t *actor)
{
    if(--actor->special1)
    {
        // Need to loop
        P_SetMobjState(actor, S_SOR2_DIE4);
    }
}

/*
 * D'Sparil Sound Routines
 */
void C_DECL A_SorZap(mobj_t *actor)
{
    S_StartSound(sfx_sorzap, NULL);
}

void C_DECL A_SorRise(mobj_t *actor)
{
    S_StartSound(sfx_sorrise, NULL);
}

void C_DECL A_SorDSph(mobj_t *actor)
{
    S_StartSound(sfx_sordsph, NULL);
}

void C_DECL A_SorDExp(mobj_t *actor)
{
    S_StartSound(sfx_sordexp, NULL);
}

void C_DECL A_SorDBon(mobj_t *actor)
{
    S_StartSound(sfx_sordbon, NULL);
}

void C_DECL A_SorSightSnd(mobj_t *actor)
{
    S_StartSound(sfx_sorsit, NULL);
}

/*
 * Minotaur Melee attack.
 */
void C_DECL A_MinotaurAtk1(mobj_t *actor)
{
    player_t *player;

    if(!actor->target)
        return;

    S_StartSound(sfx_stfpow, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4));

        if((player = actor->target->player) != NULL)
        {
            // Squish the player
            player->plr->deltaviewheight = -16 * FRACUNIT;
        }
    }
}

/*
 * Minotaur : Choose a missile attack.
 */
void C_DECL A_MinotaurDecide(mobj_t *actor)
{
    angle_t angle;
    mobj_t *target;
    int     dist;

    target = actor->target;
    if(!target)
        return;

    S_StartSound(sfx_minsit, actor);

    dist = P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                            actor->pos[VY] - target->pos[VY]);

    if(target->pos[VZ] + target->height > actor->pos[VZ] &&
       target->pos[VZ] + target->height < actor->pos[VZ] + actor->height &&
       dist < 8 * 64 * FRACUNIT && dist > 1 * 64 * FRACUNIT &&
       P_Random() < 150)
    {
        // Charge attack
        // Don't call the state function right away
        P_SetMobjStateNF(actor, S_MNTR_ATK4_1);
        actor->flags |= MF_SKULLFLY;

        A_FaceTarget(actor);

        angle = actor->angle >> ANGLETOFINESHIFT;
        actor->momx = FixedMul(MNTR_CHARGE_SPEED, finecosine[angle]);
        actor->momy = FixedMul(MNTR_CHARGE_SPEED, finesine[angle]);

        // Charge duration
        actor->special1 = 35 / 2;
    }
    else if(target->pos[VZ] == target->floorz && dist < 9 * 64 * FRACUNIT &&
            P_Random() < 220)
    {
        // Floor fire attack
        P_SetMobjState(actor, S_MNTR_ATK3_1);
        actor->special2 = 0;
    }
    else
    {
        // Swing attack
        A_FaceTarget(actor);
        // NOTE: Don't need to call P_SetMobjState because the current
        //       state falls through to the swing attack
    }
}

void C_DECL A_MinotaurCharge(mobj_t *actor)
{
    mobj_t *puff;

    if(actor->special1)
    {
        puff = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PHOENIXPUFF);
        puff->momz = 2 * FRACUNIT;

        actor->special1--;
    }
    else
    {
        actor->flags &= ~MF_SKULLFLY;

        P_SetMobjState(actor, actor->info->seestate);
    }
}

/*
 * Minotaur : Swing attack.
 */
void C_DECL A_MinotaurAtk2(mobj_t *actor)
{
    mobj_t *mo;
    angle_t angle;
    fixed_t momz;

    if(!actor->target)
        return;

    S_StartSound(sfx_minat2, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5));
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX1);
    if(mo)
    {
        S_StartSound(sfx_minat2, mo);

        momz = mo->momz;
        angle = mo->angle;

        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle - (ANG45 / 8), momz);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle + (ANG45 / 8), momz);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle - (ANG45 / 16), momz);
        P_SpawnMissileAngle(actor, MT_MNTRFX1, angle + (ANG45 / 16), momz);
    }
}

/*
 * Minotaur : Floor fire attack.
 */
void C_DECL A_MinotaurAtk3(mobj_t *actor)
{
    mobj_t *mo;
    player_t *player;

    if(!actor->target)
        return;

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(5));

        if((player = actor->target->player) != NULL)
        {
            // Squish the player
            player->plr->deltaviewheight = -16 * FRACUNIT;
        }
    }
    else
    {
        mo = P_SpawnMissile(actor, actor->target, MT_MNTRFX2);
        if(mo != NULL)
            S_StartSound(sfx_minat1, mo);
    }

    if(P_Random() < 192 && actor->special2 == 0)
    {
        P_SetMobjState(actor, S_MNTR_ATK3_4);
        actor->special2 = 1;
    }
}

void C_DECL A_MntrFloorFire(mobj_t *actor)
{
    mobj_t *mo;

    actor->pos[VZ] = actor->floorz;
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - P_Random()) << 10),
                     actor->pos[VY] + ((P_Random() - P_Random()) << 10), ONFLOORZ,
                     MT_MNTRFX3);

    mo->target = actor->target;

    // Force block checking
    mo->momx = 1;

    P_CheckMissileSpawn(mo);
}

void C_DECL A_BeastAttack(mobj_t *actor)
{
    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(3));
        return;
    }

    P_SpawnMissile(actor, actor->target, MT_BEASTBALL);
}

void C_DECL A_HeadAttack(mobj_t *actor)
{
    int     i;
    mobj_t *fire;
    mobj_t *baseFire;
    mobj_t *mo;
    mobj_t *target;
    int     randAttack;
    static int atkResolve1[] = { 50, 150 };
    static int atkResolve2[] = { 150, 200 };
    int     dist;

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
        P_DamageMobj(target, actor, actor, HITDICE(6));
        return;
    }

    dist =
        P_ApproxDistance(actor->pos[VX] - target->pos[VX],
                         actor->pos[VY] - target->pos[VY]) > 8 * 64 * FRACUNIT;

    randAttack = P_Random();
    if(randAttack < atkResolve1[dist])
    {
        // Ice ball
        P_SpawnMissile(actor, target, MT_HEADFX1);
        S_StartSound(sfx_hedat2, actor);
    }
    else if(randAttack < atkResolve2[dist])
    {
        // Fire column
        baseFire = P_SpawnMissile(actor, target, MT_HEADFX3);
        if(baseFire != NULL)
        {
            P_SetMobjState(baseFire, S_HEADFX3_4);  // Don't grow
            for(i = 0; i < 5; i++)
            {
                fire =
                    P_SpawnMobj(baseFire->pos[VX], baseFire->pos[VY], baseFire->pos[VZ],
                                MT_HEADFX3);
                if(i == 0)
                    S_StartSound(sfx_hedat1, actor);

                fire->target = baseFire->target;

                fire->angle = baseFire->angle;
                fire->momx = baseFire->momx;
                fire->momy = baseFire->momy;
                fire->momz = baseFire->momz;

                fire->damage = 0;
                fire->health = (i + 1) * 2;

                P_CheckMissileSpawn(fire);
            }
        }
    }
    else
    {
        // Whirlwind
        mo = P_SpawnMissile(actor, target, MT_WHIRLWIND);
        if(mo != NULL)
        {
            mo->pos[VZ] -= 32 * FRACUNIT;
            mo->tracer = target;
            mo->special1 = 60;
            mo->special2 = 50;  // Timer for active sound
            mo->health = 20 * TICSPERSEC;   // Duration

            S_StartSound(sfx_hedat3, actor);
        }
    }
}

void C_DECL A_WhirlwindSeek(mobj_t *actor)
{
    actor->health -= 3;
    if(actor->health < 0)
    {
        actor->momx = actor->momy = actor->momz = 0;
        P_SetMobjState(actor, mobjinfo[actor->type].deathstate);
        actor->flags &= ~MF_MISSILE;
        return;
    }

    if((actor->special2 -= 3) < 0)
    {
        actor->special2 = 58 + (P_Random() & 31);
        S_StartSound(sfx_hedat3, actor);
    }

    if(actor->tracer && actor->tracer->flags & MF_SHADOW)
        return;

    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

void C_DECL A_HeadIceImpact(mobj_t *ice)
{
    int     i;
    angle_t angle;
    mobj_t *shard;

    for(i = 0; i < 8; i++)
    {
        shard = P_SpawnMobj(ice->pos[VX], ice->pos[VY], ice->pos[VZ], MT_HEADFX2);
        angle = i * ANG45;
        shard->target = ice->target;
        shard->angle = angle;

        angle >>= ANGLETOFINESHIFT;
        shard->momx = FixedMul(shard->info->speed, finecosine[angle]);
        shard->momy = FixedMul(shard->info->speed, finesine[angle]);
        shard->momz = (fixed_t) (-.6 * FRACUNIT);

        P_CheckMissileSpawn(shard);
    }
}

void C_DECL A_HeadFireGrow(mobj_t *fire)
{
    fire->health--;
    fire->pos[VZ] += 9 * FRACUNIT;

    if(fire->health == 0)
    {
        fire->damage = fire->info->damage;
        P_SetMobjState(fire, S_HEADFX3_4);
    }
}

void C_DECL A_SnakeAttack(mobj_t *actor)
{
    if(!actor->target)
    {
        P_SetMobjState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attacksound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(actor, actor->target, MT_SNAKEPRO_A);
}

void C_DECL A_SnakeAttack2(mobj_t *actor)
{
    if(!actor->target)
    {
        P_SetMobjState(actor, S_SNAKE_WALK1);
        return;
    }

    S_StartSound(actor->info->attacksound, actor);
    A_FaceTarget(actor);
    P_SpawnMissile(actor, actor->target, MT_SNAKEPRO_B);
}

void C_DECL A_ClinkAttack(mobj_t *actor)
{
    int     damage;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);
    if(P_CheckMeleeRange(actor))
    {
        damage = ((P_Random() % 7) + 3);
        P_DamageMobj(actor->target, actor, actor, damage);
    }
}

void C_DECL A_GhostOff(mobj_t *actor)
{
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk1(mobj_t *actor)
{
    A_FaceTarget(actor);
    actor->flags &= ~MF_SHADOW;
}

void C_DECL A_WizAtk2(mobj_t *actor)
{
    A_FaceTarget(actor);
    actor->flags |= MF_SHADOW;
}

void C_DECL A_WizAtk3(mobj_t *actor)
{
    mobj_t *mo;
    angle_t angle;
    fixed_t momz;

    actor->flags &= ~MF_SHADOW;

    if(!actor->target)
        return;

    S_StartSound(actor->info->attacksound, actor);

    if(P_CheckMeleeRange(actor))
    {
        P_DamageMobj(actor->target, actor, actor, HITDICE(4));
        return;
    }

    mo = P_SpawnMissile(actor, actor->target, MT_WIZFX1);
    if(mo)
    {
        momz = mo->momz;
        angle = mo->angle;

        P_SpawnMissileAngle(actor, MT_WIZFX1, angle - (ANG45 / 8), momz);
        P_SpawnMissileAngle(actor, MT_WIZFX1, angle + (ANG45 / 8), momz);
    }
}

void C_DECL A_Scream(mobj_t *actor)
{
    switch (actor->type)
    {
    case MT_CHICPLAYER:
    case MT_SORCERER1:
    case MT_MINOTAUR:
        // Make boss death sounds full volume
        S_StartSound(actor->info->deathsound, NULL);
        break;

    case MT_PLAYER:
        // Handle the different player death screams
        if(actor->special1 < 10)
        {
            // Wimpy death sound
            S_StartSound(sfx_plrwdth, actor);
        }
        else if(actor->health > -50)
        {
            // Normal death sound
            S_StartSound(actor->info->deathsound, actor);
        }
        else if(actor->health > -100)
        {
            // Crazy death sound
            S_StartSound(sfx_plrcdth, actor);
        }
        else
        {
            // Extreme death sound
            S_StartSound(sfx_gibdth, actor);
        }
        break;

    default:
        S_StartSound(actor->info->deathsound, actor);
        break;
    }
}

void P_DropItem(mobj_t *source, mobjtype_t type, int special, int chance)
{
    mobj_t *mo;

    if(P_Random() > chance)
        return;

    mo = P_SpawnMobj(source->pos[VX], source->pos[VY],
                     source->pos[VZ] + (source->height >> 1), type);
    mo->momx = (P_Random() - P_Random()) << 8;
    mo->momy = (P_Random() - P_Random()) << 8;
    mo->momz = FRACUNIT * 5 + (P_Random() << 10);

    mo->flags |= MF_DROPPED;
    mo->health = special;
}

void C_DECL A_NoBlocking(mobj_t *actor)
{
    actor->flags &= ~MF_SOLID;

    // Check for monsters dropping things
    switch (actor->type)
    {
    case MT_MUMMY:
    case MT_MUMMYLEADER:
    case MT_MUMMYGHOST:
    case MT_MUMMYLEADERGHOST:
        P_DropItem(actor, MT_AMGWNDWIMPY, 3, 84);
        break;

    case MT_KNIGHT:
    case MT_KNIGHTGHOST:
        P_DropItem(actor, MT_AMCBOWWIMPY, 5, 84);
        break;

    case MT_WIZARD:
        P_DropItem(actor, MT_AMBLSRWIMPY, 10, 84);
        P_DropItem(actor, MT_ARTITOMEOFPOWER, 0, 4);
        break;

    case MT_HEAD:
        P_DropItem(actor, MT_AMBLSRWIMPY, 10, 84);
        P_DropItem(actor, MT_ARTIEGG, 0, 51);
        break;

    case MT_BEAST:
        P_DropItem(actor, MT_AMCBOWWIMPY, 10, 84);
        break;

    case MT_CLINK:
        P_DropItem(actor, MT_AMSKRDWIMPY, 20, 84);
        break;

    case MT_SNAKE:
        P_DropItem(actor, MT_AMPHRDWIMPY, 5, 84);
        break;

    case MT_MINOTAUR:
        P_DropItem(actor, MT_ARTISUPERHEAL, 0, 51);
        P_DropItem(actor, MT_AMPHRDWIMPY, 10, 84);
        break;

    default:
        break;
    }
}

void C_DECL A_Explode(mobj_t *actor)
{
    int     damage;

    damage = 128;
    switch (actor->type)
    {
    case MT_FIREBOMB:
        // Time Bombs
        actor->pos[VZ] += 32 * FRACUNIT;
        actor->flags &= ~MF_SHADOW;
        actor->flags |= MF_BRIGHTSHADOW | MF_VIEWALIGN;
        break;

    case MT_MNTRFX2:
        // Minotaur floor fire
        damage = 24;
        break;

    case MT_SOR2FX1:
        // D'Sparil missile
        damage = 80 + (P_Random() & 31);
        break;

    default:
        break;
    }

    P_RadiusAttack(actor, actor->target, damage);
    P_HitFloor(actor);
}

void C_DECL A_PodPain(mobj_t *actor)
{
    int     i;
    int     count;
    int     chance;
    mobj_t *goo;

    chance = P_Random();
    if(chance < 128)
        return;

    count = chance > 240 ? 2 : 1;
    for(i = 0; i < count; i++)
    {
        goo =
            P_SpawnMobj(actor->pos[VX], actor->pos[VY],
                        actor->pos[VZ] + 48 * FRACUNIT, MT_PODGOO);
        goo->target = actor;

        goo->momx = (P_Random() - P_Random()) << 9;
        goo->momy = (P_Random() - P_Random()) << 9;
        goo->momz = FRACUNIT / 2 + (P_Random() << 9);
    }
}

void C_DECL A_RemovePod(mobj_t *actor)
{
    mobj_t *mo;

    if(actor->generator)
    {
        mo = actor->generator;

        if(mo->special1 > 0)
            mo->special1--;
    }
}

void C_DECL A_MakePod(mobj_t *actor)
{
    mobj_t *mo;
    fixed_t pos[3];

    // Too many generated pods?
    if(actor->special1 == MAX_GEN_PODS)
        return;

    memcpy(pos, actor->pos, sizeof(pos));
    pos[VZ] = ONFLOORZ;
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_POD);

    if(P_CheckPosition(mo, pos[VX], pos[VY]) == false)
    {
        // Didn't fit
        P_RemoveMobj(mo);
        return;
    }

    P_SetMobjState(mo, S_POD_GROW1);
    P_ThrustMobj(mo, P_Random() << 24, (fixed_t) (4.5 * FRACUNIT));

    S_StartSound(sfx_newpod, mo);

    // Increment generated pod count
    actor->special1++;

    // Link the generator to the pod
    mo->generator = actor;
    return;
}

/*
 * Kills all monsters.
 */
void P_Massacre(void)
{
    mobj_t *mo;
    thinker_t *think;

    // Only massacre when in a level.
    if(gamestate != GS_LEVEL)
        return;

    for(think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        // Not a mobj thinker?
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;

        if((mo->flags & MF_COUNTKILL) && (mo->health > 0))
            P_DamageMobj(mo, NULL, NULL, 10000);
    }
}

/*
 * Trigger special effects if all bosses are dead.
 */
void C_DECL A_BossDeath(mobj_t *actor)
{
    mobj_t *mo;
    thinker_t *think;
    line_t*  dummyLine;
    static mobjtype_t bossType[6] = {
        MT_HEAD,
        MT_MINOTAUR,
        MT_SORCERER2,
        MT_HEAD,
        MT_MINOTAUR,
        -1
    };

    // Not a boss level?
    if(gamemap != 8)
        return;

    // Not considered a boss in this episode?
    if(actor->type != bossType[gameepisode - 1])
        return;

    // Make sure all bosses are dead
    for(think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        // Not a mobj thinker?
        if(think->function != P_MobjThinker)
            continue;

        mo = (mobj_t *) think;

        // Found a living boss?
        if((mo != actor) && (mo->type == actor->type) && (mo->health > 0))
            return;
    }

    // Kill any remaining monsters
    if(gameepisode > 1)
        P_Massacre();

    dummyLine = P_AllocDummyLine();
    P_XLine(dummyLine)->tag = 666;
    EV_DoFloor(dummyLine, lowerFloor);
    P_FreeDummyLine(dummyLine);
}

void C_DECL A_ESound(mobj_t *mo)
{
    int     sound;

    switch (mo->type)
    {
    case MT_SOUNDWATERFALL:
        sound = sfx_waterfl;
        break;

    case MT_SOUNDWIND:
        sound = sfx_wind;
        break;

    default:
        return;
    }

    S_StartSound(sound, mo);
}

void C_DECL A_SpawnTeleGlitter(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() & 31) - 16) * FRACUNIT,
                     actor->pos[VY] + ((P_Random() & 31) - 16) * FRACUNIT,
                     P_GetFixedp(actor->subsector, DMU_FLOOR_HEIGHT),
                     MT_TELEGLITTER);

    mo->momz = FRACUNIT / 4;
}

void C_DECL A_SpawnTeleGlitter2(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() & 31) - 16) * FRACUNIT,
                     actor->pos[VY] + ((P_Random() & 31) - 16) * FRACUNIT,
                     P_GetFixedp(actor->subsector, DMU_FLOOR_HEIGHT),
                     MT_TELEGLITTER2);

    mo->momz = FRACUNIT / 4;
}

void C_DECL A_AccTeleGlitter(mobj_t *actor)
{
    if(++actor->health > 35)
        actor->momz += actor->momz / 2;
}

void C_DECL A_InitKeyGizmo(mobj_t *gizmo)
{
    mobj_t *mo;
    statenum_t state;

    switch (gizmo->type)
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

    mo = P_SpawnMobj(gizmo->pos[VX], gizmo->pos[VY], gizmo->pos[VZ] + 60 * FRACUNIT,
                     MT_KEYGIZMOFLOAT);

    P_SetMobjState(mo, state);
}

void C_DECL A_VolcanoSet(mobj_t *volcano)
{
    volcano->tics = 105 + (P_Random() & 127);
}

void C_DECL A_VolcanoBlast(mobj_t *volcano)
{
    int     i;
    int     count;
    mobj_t *blast;
    angle_t angle;

    count = 1 + (P_Random() % 3);
    for(i = 0; i < count; i++)
    {
        blast = P_SpawnMobj(volcano->pos[VX], volcano->pos[VY],
                            volcano->pos[VZ] + 44 * FRACUNIT, MT_VOLCANOBLAST);

        blast->target = volcano;

        angle = P_Random() << 24;
        blast->angle = angle;
        angle >>= ANGLETOFINESHIFT;

        blast->momx = FixedMul(1 * FRACUNIT, finecosine[angle]);
        blast->momy = FixedMul(1 * FRACUNIT, finesine[angle]);
        blast->momz = (fixed_t) ((2.5 * FRACUNIT) + (P_Random() << 10));

        S_StartSound(sfx_volsht, blast);

        P_CheckMissileSpawn(blast);
    }
}

void C_DECL A_VolcBallImpact(mobj_t *ball)
{
    int     i;
    mobj_t *tiny;
    angle_t angle;

    if(ball->pos[VZ] <= ball->floorz)
    {
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        ball->pos[VZ] += 28 * FRACUNIT;
    }

    P_RadiusAttack(ball, ball->target, 25);
    for(i = 0; i < 4; i++)
    {
        tiny = P_SpawnMobj(ball->pos[VX], ball->pos[VY], ball->pos[VZ], MT_VOLCANOTBLAST);

        tiny->target = ball;

        angle = i * ANG90;
        tiny->angle = angle;
        angle >>= ANGLETOFINESHIFT;

        tiny->momx = FixedMul(FRACUNIT * .7, finecosine[angle]);
        tiny->momy = FixedMul(FRACUNIT * .7, finesine[angle]);
        tiny->momz = FRACUNIT + (P_Random() << 9);

        P_CheckMissileSpawn(tiny);
    }
}

void C_DECL A_SkullPop(mobj_t *actor)
{
    mobj_t *mo;
    player_t *player;

    actor->flags &= ~MF_SOLID;
    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ] + 48 * FRACUNIT,
                     MT_BLOODYSKULL);

    mo->momx = (P_Random() - P_Random()) << 9;
    mo->momy = (P_Random() - P_Random()) << 9;
    mo->momz = FRACUNIT * 2 + (P_Random() << 6);

    // Attach player mobj to bloody skull
    player = actor->player;
    actor->player = NULL;
    actor->dplayer = NULL;

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
    if(actor->pos[VZ] <= actor->floorz)
        P_SetMobjState(actor, S_BLOODYSKULLX1);
}

void C_DECL A_CheckSkullDone(mobj_t *actor)
{
    if(actor->special2 == 666)
        P_SetMobjState(actor, S_BLOODYSKULLX2);
}

void C_DECL A_CheckBurnGone(mobj_t *actor)
{
    if(actor->special2 == 666)
        P_SetMobjState(actor, S_PLAY_FDTH20);
}

void C_DECL A_FreeTargMobj(mobj_t *mo)
{
    mo->momx = mo->momy = mo->momz = 0;
    mo->pos[VZ] = mo->ceilingz + 4 * FRACUNIT;

    mo->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_SOLID);
    mo->flags |= MF_CORPSE | MF_DROPOFF | MF_NOGRAVITY;
    mo->flags2 &= ~(MF2_PASSMOBJ | MF2_LOGRAV);

    mo->player = NULL;
    mo->dplayer = NULL;
}

void C_DECL A_AddPlayerCorpse(mobj_t *actor)
{
    // Too many player corpses?
    if(bodyqueslot >= BODYQUESIZE)
        P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]); // remove an old one

    bodyque[bodyqueslot % BODYQUESIZE] = actor;
    bodyqueslot++;
}

void C_DECL A_FlameSnd(mobj_t *actor)
{
    S_StartSound(sfx_hedat1, actor);    // Burn sound
}

void C_DECL A_HideThing(mobj_t *actor)
{
    //P_UnsetThingPosition(actor);
    actor->flags2 |= MF2_DONTDRAW;
}

void C_DECL A_UnHideThing(mobj_t *actor)
{
    //P_SetThingPosition(actor);
    actor->flags2 &= ~MF2_DONTDRAW;
}
