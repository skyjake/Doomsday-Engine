/*
 *  Movement, collision handling.
 *  Shooting and aiming.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_config.h"
#include "jHeretic/P_local.h"
#include "Common/g_common.h"
#include "jHeretic/Soundst.h"
#include "jHeretic/h_stat.h"

#include "Common/dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t tmbbox[4];
mobj_t *tmthing;
int     tmflags;
fixed_t tm[3];
fixed_t tmheight;
line_t *tmhitline;

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
fixed_t tmdropoffz;

// killough $dropoff_fix
boolean felldown;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t *ceilingline;

// Used to prevent player getting stuck in monster
// Based on solution derived by Lee Killough
// See also floorline and static int untouched

line_t *floorline; // $unstuck: Highest touched floor
line_t *blockline; // $unstuck: blocking linedef

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid
line_t **spechit;
static int spechit_max;
int     numspechit;

fixed_t bestslidefrac, secondslidefrac;
line_t *bestslideline, *secondslideline;

mobj_t *slidemo;

fixed_t tmxmove;
fixed_t tmymove;

mobj_t *linetarget; // who got hit (or NULL)
mobj_t *shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t shootz;

int     la_damage;
fixed_t attackrange;

fixed_t aimslope;

// slopes to top and bottom of target
fixed_t topslope;
fixed_t bottomslope;

mobj_t *usething;

mobj_t *bombsource;
mobj_t *bombspot;
int     bombdamage;

boolean crushchange;
boolean nofit;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int tmunstuck; // $unstuck: used to check unsticking

// CODE --------------------------------------------------------------------

boolean PIT_StompThing(mobj_t *mo, void *data)
{
    int stompAnyway;
    fixed_t blockdist;

    if(!(mo->flags & MF_SHOOTABLE))
        return true;

    blockdist = mo->radius + tmthing->radius;

    if(abs(mo->pos[VX] - tm[VX]) >= blockdist ||
       abs(mo->pos[VY] - tm[VY]) >= blockdist)
        return true; // didn't hit it

    // don't clip against self
    if(mo == tmthing)
        return true;

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self
    if(mo != tmthing && stompAnyway)
    {
        P_DamageMobj(mo, tmthing, tmthing, 10000);
        return true;
    }

    if(!(tmthing->flags2 & MF2_TELESTOMP))
        return false; // Not allowed to stomp things

    // Do stomp damage (unless self)
    if(mo != tmthing)
        P_DamageMobj(mo, tmthing, tmthing, 10000);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, int alwaysstomp)
{
    int     xl, xh, yl, yh, bx, by;
    int stomping;
    subsector_t *newsubsec;

    // kill anything occupying the position
    tmthing = thing;
    tmflags = thing->flags;

    stomping = alwaysstomp;

    tm[VX] = x;
    tm[VY] = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector(x, y);

    // $unstuck: floorline used with tmunstuck
    blockline = floorline = ceilingline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);


    validCount++;
    numspechit = 0;

    // stomp on any things contacted
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockThingsIterator(bx, by, PIT_StompThing, &stomping))
                return false;

    // the move is ok,
    // so link the thing into its new position
    P_UnsetThingPosition(thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->dropoffz = tmdropoffz;   // killough $unstuck
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_SetThingPosition(thing);

    return true;
}

/*
 * $unstuck: used to test intersection between thing and line
 * assuming NO movement occurs -- used to avoid sticky situations.
 */
static int untouched(line_t *ld)
{
    fixed_t x, y, box[4];
    fixed_t bbox[4];

    P_GetFixedpv(ld, DMU_BOUNDING_BOX, bbox);

    /*return (box[BOXRIGHT] =
        (x = tmthing->pos[VX]) + tmthing->radius) <= ld->bbox[BOXLEFT] ||
        (box[BOXLEFT] = x - tmthing->radius) >= ld->bbox[BOXRIGHT] ||
        (box[BOXTOP] =
         (y = tmthing->pos[VY]) + tmthing->radius) <= ld->bbox[BOXBOTTOM] ||
        (box[BOXBOTTOM] = y - tmthing->radius) >= ld->bbox[BOXTOP] ||
        P_BoxOnLineSide(box, ld) != -1; */

    if(((box[BOXRIGHT] = (x = tmthing->pos[VX]) + tmthing->radius) <= bbox[BOXLEFT]) ||
       ((box[BOXLEFT] = x - tmthing->radius) >= bbox[BOXRIGHT]) ||
       ((box[BOXTOP] = (y = tmthing->pos[VY]) + tmthing->radius) <= bbox[BOXBOTTOM]) ||
       ((box[BOXBOTTOM] = y - tmthing->radius) >= bbox[BOXTOP]) ||
       P_BoxOnLineSide(box, ld) != -1)
        return true;
    else
        return false;
}

/*
 * Adjusts tmfloorz and tmceilingz as lines are contacted
 */
boolean PIT_CheckLine(line_t *ld, void *data)
{
    fixed_t dx = P_GetFixedp(ld, DMU_DX);
    fixed_t dy = P_GetFixedp(ld, DMU_DY);
    fixed_t* bbox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

    if(tmbbox[BOXRIGHT] <= bbox[BOXLEFT] ||
       tmbbox[BOXLEFT] >= bbox[BOXRIGHT] ||
       tmbbox[BOXTOP] <= bbox[BOXBOTTOM] ||
       tmbbox[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(P_BoxOnLineSide(tmbbox, ld) != -1)
        return true;

    // A line has been hit
    tmthing->wallhit = true;

    // A Hit event will be sent to special lines.
    if(P_XLine(ld)->special)
        tmhitline = ld;

    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    // $unstuck: allow player to move out of 1s wall, to prevent sticking
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {
        blockline = ld;
        return tmunstuck && !untouched(ld) &&
            FixedMul(tm[VX] - tmthing->pos[VX], dy) > FixedMul(tm[VY] - tmthing->pos[VY], dx);
    }

    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {                           // One sided line
        if(tmthing->flags & MF_MISSILE)
        {                       // Missiles can trigger impact specials
            if(P_XLine(ld)->special)
            {
                spechit[numspechit] = ld;
                numspechit++;
            }
        }
        return false;
    }

    if(!(tmthing->flags & MF_MISSILE))
    {
        // explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKING)
            // killough $unstuck: allow escape
            return tmunstuck && !untouched(ld);

        // block monsters only?
        if(!tmthing->player &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS) &&
           tmthing->type != MT_POD)
            return false;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(ld);

    // adjust floor / ceiling heights
    if(opentop < tmceilingz)
    {
        tmceilingz = opentop;
        ceilingline = ld;
        blockline = ld;
    }
    if(openbottom > tmfloorz)
    {
        tmfloorz = openbottom;
        // killough $unstuck: remember floor linedef
        floorline = ld;
        blockline = ld;
    }
    /*
       if (openbottom > tmfloorz)
       tmfloorz = openbottom;   */

    if(lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    // if contacted a special line, add it to the list
    if(P_XLine(ld)->special)
    {
        if(numspechit >= spechit_max)
        {
             spechit_max = spechit_max ? spechit_max * 2 : 8;
             spechit = realloc(spechit, sizeof *spechit*spechit_max);
        }
        spechit[numspechit++] = ld;
    }

    tmthing->wallhit = false;
    return true;
}

boolean PIT_CheckThing(mobj_t *thing, void *data)
{
    fixed_t blockdist;
    boolean solid, overlap = false;
    int     damage;

    // don't clip against self
    if(thing == tmthing)
        return true;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_IsCamera(thing) || P_IsCamera(tmthing))
        return true;

    blockdist = thing->radius + tmthing->radius;

    // Player only
    if(tmthing->player && tm[VZ] != DDMAXINT &&
       (cfg.moveCheckZ || (tmthing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->pos[VZ] > tm[VZ] + tmheight) || (thing->pos[VZ] + thing->height < tm[VZ]))
            return true;        // under or over it
        overlap = true;
    }
    if(abs(thing->pos[VX] - tm[VX]) >= blockdist || abs(thing->pos[VY] - tm[VY]) >= blockdist)
    {
        // didn't hit it
        return true;
    }

    if(!tmthing->player && (tmthing->flags2 & MF2_PASSMOBJ))
    {
#if __JHERETIC__
        // check if a mobj passed over/under another object
        if((tmthing->type == MT_IMP || tmthing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            // don't let imps/wizards fly over other imps/wizards
            return false;
        }
#endif

        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height &&
           !(thing->flags & MF_SPECIAL))
        {
            return true;
        }
        else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ] &&
                !(thing->flags & MF_SPECIAL))
        {
            // under thing
            return true;
        }
    }

    // Check for skulls slamming into things
    if(tmthing->flags & MF_SKULLFLY)
    {
#if __JDOOM__
        if(tmthing->damage == DDMAXINT) // Kludge to support old save games.
            damage = tmthing->info->damage;
        else
#endif
            damage = tmthing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;

        P_SetMobjState(tmthing, tmthing->info->seestate);

        return false;           // stop moving
    }

    // missiles can hit other things
    if(tmthing->flags & MF_MISSILE)
    {
        // Check for passing through a ghost
        if((thing->flags & MF_SHADOW) && (tmthing->flags2 & MF2_THRUGHOST))
            return true;

        // see if it went over / under
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
            return true;        // overhead

        if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
            return true;        // underneath

        // Don't hit same species as originator.
        if(tmthing->target && tmthing->target->type == thing->type)
        {
            if(thing == tmthing->target)
                return true; // Don't missile self

            if(!monsterinfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            // Didn't do any damage
            return !(thing->flags & MF_SOLID);
        }
        if(tmthing->flags2 & MF2_RIP)
        {
            if(!(thing->flags & MF_NOBLOOD))
            {                   // Ok to spawn some blood
                P_RipperBlood(tmthing);
            }
#if __JHERETIC__
            S_StartSound(sfx_ripslop, tmthing);
#endif
#if __JDOOM__
            if(tmthing->damage == DDMAXINT) // Kludge to support old save games.
                damage = tmthing->info->damage;
            else
#endif
                damage = tmthing->damage;

            damage *= (P_Random() & 3) + 2;

            P_DamageMobj(thing, tmthing, tmthing->target, damage);

            if(thing->flags2 & MF2_PUSHABLE &&
               !(tmthing->flags2 & MF2_CANNOTPUSH))
            {                   // Push thing
                thing->momx += tmthing->momx >> 2;
                thing->momy += tmthing->momy >> 2;
                if(thing->dplayer)
                    thing->dplayer->flags |= DDPF_FIXMOM;
            }
            numspechit = 0;
            return true;
        }

        // Do damage
#if __JDOOM__
        if(tmthing->damage == DDMAXINT) // Kludge to support old save games.
            damage = tmthing->info->damage;
        else
#endif
            damage = tmthing->damage;

        damage *= (P_Random() % 8) + 1;
        if(damage)
        {
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
                P_BloodSplatter(tmthing->pos[VX], tmthing->pos[VY], tmthing->pos[VZ], thing);

            P_DamageMobj(thing, tmthing, tmthing->target, damage);
        }

        // don't traverse any more
        return false;
    }
    if(thing->flags2 & MF2_PUSHABLE && !(tmthing->flags2 & MF2_CANNOTPUSH))
    {                           // Push thing
        thing->momx += tmthing->momx >> 2;
        thing->momy += tmthing->momy >> 2;
        if(thing->dplayer)
            thing->dplayer->flags |= DDPF_FIXMOM;
    }
    // Check for special pickup
    if(thing->flags & MF_SPECIAL)
    {
        solid = thing->flags & MF_SOLID;
        if(tmflags & MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialThing(thing, tmthing);
        }
        return !solid;
    }

    if(overlap && thing->flags & MF_SOLID)
    {
        // How are we positioned?
        if(tm[VZ] > thing->pos[VZ] + thing->height - 24 * FRACUNIT)
        {
            tmthing->onmobj = thing;
            if(thing->pos[VZ] + thing->height > tmfloorz)
                tmfloorz = thing->pos[VZ] + thing->height;
            return true;
        }
    }

    return !(thing->flags & MF_SOLID);
}

/*
boolean PIT_CheckOnmobjZ(mobj_t *thing, void *data)
{
    fixed_t blockdist;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
    {                           // Can't hit thing
        return (true);
    }
    blockdist = thing->radius + tmthing->radius;
    if(abs(thing->pos[VX] - tm[VX]) >= blockdist || abs(thing->pos[VY] - tm[VY]) >= blockdist)
    {                           // Didn't hit thing
        return (true);
    }
    if(thing == tmthing)
    {                           // Don't clip against self
        return (true);
    }
    if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
    {
        return (true);
    }
    else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
    {                           // under thing
        return (true);
    }
    if(thing->flags & MF_SOLID)
    {
        onmobj = thing;
    }
    return (!(thing->flags & MF_SOLID));
}*/

/*
 * Returns true if the mobj is not blocked by anything at its current
 * location, otherwise returns false.
 */
boolean P_TestMobjLocation(mobj_t *mobj)
{
    int     flags;

    flags = mobj->flags;
    mobj->flags &= ~MF_PICKUP;
    if(P_CheckPosition(mobj, mobj->pos[VX], mobj->pos[VY]))
    {                           // XY is ok, now check Z
        mobj->flags = flags;
        if((mobj->pos[VZ] < mobj->floorz) ||
           (mobj->pos[VZ] + mobj->height > mobj->ceilingz))
        {                       // Bad Z
            return (false);
        }
        return (true);
    }
    mobj->flags = flags;
    return (false);
}

/*
 * This is purely informative, nothing is modified
 * (except things picked up).
 *
 * in:
 *  a mobj_t (can be valid or invalid)
 *  a position to be checked
 *   (doesn't need to be related to the mobj_t->x,y)
 *
 * during:
 *  special things are touched if MF_PICKUP
 *  early out on solid lines?
 *
 * out:
 *  newsubsec
 *  floorz
 *  ceilingz
 *  tmdropoffz
 *   the lowest point contacted
 *   (monsters won't move to a drop off)
 *  speciallines[]
 *  numspeciallines
 */
boolean P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z)
{
    int     xl, xh, yl, yh, bx, by;
    sector_t *newsec;

    tmthing = thing;
    tmflags = thing->flags;

    thing->onmobj = NULL;
    thing->wallhit = false;

    tmhitline = NULL;

    tm[VX] = x;
    tm[VY] = y;
    tm[VZ] = z;
    tmheight = thing->height;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(x, y), DMU_SECTOR);

    // $unstuck: floorline used with tmunstuck
    blockline = floorline = ceilingline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFixedp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsec, DMU_CEILING_HEIGHT);

    validCount++;
    numspechit = 0;

    if(tmflags & MF_NOCLIP)
        return true;

    // check things first, possibly picking things up
    // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into adjacent
    // blocks by up to MAXRADIUS units
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    // The camera goes through all objects -- jk
// __JHEXEN__
//    if(thing->type != MT_CAMERA)
//    {

        for(bx = xl; bx <= xh; bx++)
            for(by = yl; by <= yh; by++)
                if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, 0))
                    return false;
//    }

    // check lines
//    if(tmflags & MF_NOCLIP)
//        return true;

//    BlockingMobj = NULL;
    P_PointToBlock(tmbbox[BOXLEFT], tmbbox[BOXBOTTOM], &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT], tmbbox[BOXTOP], &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockLinesIterator(bx, by, PIT_CheckLine, 0))
                return false;

    return true;
}

boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
    return P_CheckPosition2(thing, x, y, DDMAXINT);
}

/*
 * Checks if the new Z position is legal
 */
/*
mobj_t *P_CheckOnmobj(mobj_t *thing)
{
    int     xl, xh, yl, yh, bx, by;
    sector_t *newsec;
    fixed_t x;
    fixed_t y;
    mobj_t  oldmo;

    x = thing->pos[VX];
    y = thing->pos[VY];
    tmthing = thing;
    tmflags = thing->flags;
    oldmo = *thing;             // save the old mobj before the fake zmovement
    P_FakeZMovement(tmthing);

    tm[VX] = x;
    tm[VY] = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(x, y), DMU_SECTOR);
    ceilingline = NULL;

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFixedp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsec, DMU_CEILING_HEIGHT);

    validCount++;
    numspechit = 0;

    if(tmflags & MF_NOCLIP)
        return NULL;

    // check things first, possibly picking things up
    // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into adjacent
    // blocks by up to MAXRADIUS units
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            if(!P_BlockThingsIterator(bx, by, PIT_CheckOnmobjZ, 0))
            {
                *tmthing = oldmo;
                return onmobj;
            }
    *tmthing = oldmo;
    return NULL;
}*/

/*
 * Fake the zmovement so that we can check if a move is legal
 */
/*
void P_FakeZMovement(mobj_t *mo)
{
    int     dist;
    int     delta;

    if(P_IsCamera(mo))
        return;

    // adjust height
    mo->pos[VZ] += mo->momz;
    if(mo->flags & MF_FLOAT && mo->target)
    {                           // float down towards target if too close
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                    mo->pos[VY] - mo->target->pos[VY]);
            delta = (mo->target->pos[VZ] + (mo->height >> 1)) - mo->pos[VZ];
            if(delta < 0 && dist < -(delta * 3))
                mo->pos[VZ] -= FLOATSPEED;
            else if(delta > 0 && dist < (delta * 3))
                mo->pos[VZ] += FLOATSPEED;
        }
    }

    if(mo->player && mo->flags2 & MF2_FLY && !(mo->pos[VZ] <= mo->floorz) &&
       leveltime & 2)
    {
        mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // clip movement
    if(mo->pos[VZ] <= mo->floorz)
    {
        // Hit the floor
        mo->pos[VZ] = mo->floorz;
        if(mo->momz < 0)
            mo->momz = 0;

        // The skull slammed into something?
        if(mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz;

        if(mo->info->crashstate && (mo->flags & MF_CORPSE))
            return;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->momz == 0)
            mo->momz = -(GRAVITY >> 3) * 2;
        else
            mo->momz -= GRAVITY >> 3;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->momz == 0)
            mo->momz = -GRAVITY * 2;
        else
            mo->momz -= GRAVITY;
    }

    if(mo->pos[VZ] + mo->height > mo->ceilingz)
    {
        // hit the ceiling
        if(mo->momz > 0)
            mo->momz = 0;

        mo->pos[VZ] = mo->ceilingz - mo->height;

        // the skull slammed into something?
        if(mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz;
    }
}
*/

void CheckMissileImpact(mobj_t *mobj)
{
    int     i;

    if(!numspechit || !(mobj->flags & MF_MISSILE) || !mobj->target)
        return;

    if(!mobj->target->player)
        return;

    for(i = numspechit - 1; i >= 0; i--)
    {
        P_ShootSpecialLine(mobj->target, spechit[i]);
    }
}

/*
 * Attempt to move to a new position, crossing special lines
 * unless MF_TELEPORT is set.
 *
 * killough $dropoff_fix
 */
boolean P_TryMove2(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
{
    fixed_t oldpos[3];
    int     side;
    int     oldside;
    line_t *ld;

    // $dropoff_fix: felldown
    floatok = felldown = false;

    if(!P_CheckPosition2(thing, x, y, thing->pos[VZ]))
    {
        CheckMissileImpact(thing);
        // Would we hit another thing or a solid wall?
        if(!thing->onmobj || thing->wallhit)
            return false;
    }

    // GMJ 02/02/02
    if(!(thing->flags & MF_NOCLIP))
    {
        // killough 7/26/98: reformatted slightly
        // killough 8/1/98: Possibly allow escape if otherwise stuck

        if(tmceilingz - tmfloorz < thing->height || // doesn't fit
           // mobj must lower to fit
           (floatok = true, !(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY) &&
            tmceilingz - thing->pos[VZ] < thing->height) ||
           // too big a step up
           (!(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY) &&
#if __JHERETIC__
            thing->type != MT_MNTRFX2 && // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
#endif
            tmfloorz - thing->pos[VZ] > 24 * FRACUNIT))
        {
            CheckMissileImpact(thing);
            return tmunstuck && !(ceilingline && untouched(ceilingline)) &&
                !(floorline && untouched(floorline));
        }

        if((thing->flags & MF_MISSILE) && tmfloorz > thing->pos[VZ])
        {
            CheckMissileImpact(thing);
        }

        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > tmceilingz)
            {
                thing->momz = -8 * FRACUNIT;
                return false;
            }
            else if(thing->pos[VZ] < tmfloorz &&
                    tmfloorz - tmdropoffz > 24 * FRACUNIT)
            {
                thing->momz = 8 * FRACUNIT;
                return false;
            }
        }

        // killough 3/15/98: Allow certain objects to drop off
        // killough 7/24/98, 8/1/98:
        // Prevent monsters from getting stuck hanging off ledges
        // killough 10/98: Allow dropoffs in controlled circumstances
        // killough 11/98: Improve symmetry of clipping on stairs

        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit
            if(cfg.avoidDropoffs)
            {
                if(tmfloorz - tmdropoffz > 24 * FRACUNIT)
                    return false;
            }
            else
            {
                if(!dropoff)
                {
                   if(thing->floorz - tmfloorz > 24*FRACUNIT ||
                      thing->dropoffz - tmdropoffz > 24*FRACUNIT)
                      return false;
                }
                else
                {   // set felldown if drop > 24
                    felldown = !(thing->flags & MF_NOGRAVITY) &&
                        thing->pos[VZ] - tmfloorz > 24 * FRACUNIT;
                }
            }
        }

        // killough $dropoff: prevent falling objects from going up too many steps
        if(!thing->player && thing->intflags & MIF_FALLING &&
           tmfloorz - thing->pos[VZ] > FixedMul(thing->momx,
                                          thing->momx) + FixedMul(thing->momy,
                                                                  thing->momy))
        {
            return false;
        }
    }

    // the move is ok, so link the thing into its new position
    P_UnsetThingPosition(thing);

    memcpy(oldpos, thing->pos, sizeof(oldpos));
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    // killough $dropoff_fix: keep track of dropoffs
    thing->dropoffz = tmdropoffz;

    thing->pos[VX] = x;
    thing->pos[VY] = y;
    P_SetThingPosition(thing);

    if(thing->flags2 & MF2_FLOORCLIP)
    {
        if(thing->pos[VZ] == P_GetFixedp(thing->subsector, DMU_FLOOR_HEIGHT) &&
           P_GetThingFloorType(thing) >= FLOOR_LIQUID)
        {
            thing->floorclip = 10 * FRACUNIT;
        }
        else
        {
            thing->floorclip = 0;
        }
    }

    // if any special lines were hit, do the effect
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while(numspechit--)
        {
            // see if the line was crossed
            ld = spechit[numspechit];

            if(P_XLine(ld)->special)
            {
                side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
                oldside = P_PointOnLineSide(oldpos[VX], oldpos[VY], ld);
                if(side != oldside)
                    P_CrossSpecialLine(P_ToIndex(ld), oldside, thing);
            }
        }
    }

    return true;
}

boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff, boolean slide)
{
    // killough $dropoff_fix
    boolean res = P_TryMove2(thing, x, y, dropoff);

    if(!res && tmhitline)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(tmhitline, P_PointOnLineSide(thing->pos[VX], thing->pos[VY], tmhitline),
                   thing);
    }

    if(res && slide)
        thing->wallrun = true;

    return res;
}

/*
 * Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
 * and possibly thing->z.
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 */
boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean onfloor;

    onfloor = (thing->pos[VZ] == thing->floorz);
    P_CheckPosition2(thing, thing->pos[VX], thing->pos[VY], thing->pos[VZ]);

    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    // killough $dropoff_fix: remember dropoffs
    thing->dropoffz = tmdropoffz;

    if(onfloor)
    {
        // walking monsters rise and fall with the floor
        thing->pos[VZ] = thing->floorz;
        // killough $dropoff_fix:
        // Possibly upset balance of objects hanging off ledges
        if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
            thing->gear = 0;
    }
    else
    {
        // don't adjust a floating monster unless forced to
        if(thing->pos[VZ] + thing->height > thing->ceilingz)
            thing->pos[VZ] = thing->ceilingz - thing->height;
    }

    if((thing->ceilingz - thing->floorz) >= thing->height)
        return true;
    else
        return false;
}

/*
 * Allows the player to slide along any angled walls by
 * adjusting the xmove / ymove so that the NEXT move will
 * slide along the wall.
 */
void P_HitSlideLine(line_t *ld)
{
    int     side;
    angle_t lineangle, moveangle, deltaangle;
    fixed_t movelen, newlen;
    fixed_t dx = P_GetFixedp(ld, DMU_DX);
    fixed_t dy = P_GetFixedp(ld, DMU_DY);

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], ld);

    lineangle = R_PointToAngle2(0, 0, dx, dy);
    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

boolean PTR_SlideTraverse(intercept_t * in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
        {
            // don't hit the back side
            return true;
        }
        goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening(li);

    if(openrange < slidemo->height)
        goto isblocking;        // doesn't fit

    if(opentop - slidemo->pos[VZ] < slidemo->height)
        goto isblocking;        // mobj is too high

    if(openbottom - slidemo->pos[VZ] > 24 * FRACUNIT)
        goto isblocking;        // too big a step up

    // this line doesn't block movement
    return true;

    // the line does block movement,
    // see if it is closer than best so far
  isblocking:
    if(in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    return false;               // stop
}

/*
 * The momx / momy move is bad, so try to slide
 * along a wall.
 * Find the first line hit, move flush to it,
 * and slide along it
 *
 * This is a kludgy mess.
 */
void P_SlideMove(mobj_t *mo)
{
    fixed_t leadpos[3];
    fixed_t trailpos[3];
    fixed_t newx, newy;
    int     hitcount;

    slidemo = mo;
    hitcount = 0;

  retry:
    if(++hitcount == 3)
        goto stairstep; // don't loop forever

    // trace along the three leading corners
    memcpy(leadpos, mo->pos, sizeof(leadpos));
    memcpy(trailpos, mo->pos, sizeof(trailpos));
    if(mo->momx > 0)
    {
        leadpos[VX] += mo->radius;
        trailpos[VX] -= mo->radius;
    }
    else
    {
        leadpos[VX] -= mo->radius;
        trailpos[VX] += mo->radius;
    }

    if(mo->momy > 0)
    {
        leadpos[VY] += mo->radius;
        trailpos[VY] -= mo->radius;
    }
    else
    {
        leadpos[VY] -= mo->radius;
        trailpos[VY] += mo->radius;
    }

    bestslidefrac = FRACUNIT + 1;

    P_PathTraverse(leadpos[VX], leadpos[VY], leadpos[VX] + mo->momx, leadpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailpos[VX], leadpos[VY], trailpos[VX] + mo->momx, leadpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadpos[VX], trailpos[VY], leadpos[VX] + mo->momx, trailpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);

    // move up to the wall
    if(bestslidefrac == FRACUNIT + 1)
    {
        // the move most have hit the middle, so stairstep
      stairstep:
        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->momy, true, true))
            P_TryMove(mo, mo->pos[VX] + mo->momx, mo->pos[VY], true, true);
            return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if(bestslidefrac > 0)
    {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->pos[VX] + newx, mo->pos[VY] + newy, true, true))
            goto stairstep;
    }

    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

    if(bestslidefrac > FRACUNIT)
        bestslidefrac = FRACUNIT;
    if(bestslidefrac <= 0)
        return;

    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    P_HitSlideLine(bestslideline);  // clip the moves

    mo->momx = tmxmove;
    mo->momy = tmymove;

    // killough $dropoff_fix
    if(!P_TryMove(mo, mo->pos[VX] + tmxmove, mo->pos[VY] + tmymove, true, true))
    {
        goto retry;
    }
}

/*
 * Sets linetaget and aimslope when a target is aimed at.
 */
boolean PTR_AimTraverse(intercept_t * in)
{
    line_t *li;
    mobj_t *th;
    fixed_t slope, thingtopslope, thingbottomslope;
    fixed_t dist;
    sector_t *backsector, *frontsector;

    if(in->isaline)
    {
        fixed_t ffloor, bfloor;
        fixed_t fceil, bceil;
        li = in->d.line;

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            return false;       // stop

        // Crosses a two sided line.
        // A two sided line will restrict
        // the possible target ranges.
        P_LineOpening(li);

        if(openbottom >= opentop)
            return false;       // stop

        dist = FixedMul(attackrange, in->frac);

        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        ffloor = P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT);
        fceil = P_GetFixedp(frontsector, DMU_CEILING_HEIGHT);

        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);
        bfloor = P_GetFixedp(backsector, DMU_FLOOR_HEIGHT);
        bceil = P_GetFixedp(backsector, DMU_CEILING_HEIGHT);

        if(ffloor != bfloor)
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if(slope > bottomslope)
                bottomslope = slope;
        }

        if(fceil != bceil)
        {
            slope = FixedDiv(opentop - shootz, dist);
            if(slope < topslope)
                topslope = slope;
        }

        if(topslope <= bottomslope)
            return false;       // stop

        return true;            // shot continues
    }

    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

    if(th->type == MT_POD)
    {                           // Can't auto-aim at pods
        return (true);
    }

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->pos[VZ] + th->height - shootz, dist);

    if(thingtopslope < bottomslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv(th->pos[VZ] - shootz, dist);
    if(thingbottomslope > topslope)
        return true;            // shot under the thing

    // this thing can be hit!
    if(thingtopslope > topslope)
        thingtopslope = topslope;

    if(thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;

    return false;               // don't go any farther
}

boolean PTR_ShootTraverse(intercept_t * in)
{
    fixed_t pos[3];
    fixed_t frac;
    sector_t *backsector = NULL;
    sector_t *frontsector = NULL;
    line_t *li;
    xline_t *xline;

    mobj_t *th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;

    divline_t *trace = (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);

    // These were added for the plane-hitpoint algo.
    // FIXME: This routine is getting rather bloated.
    boolean lineWasHit;
    subsector_t *contact, *originSub;
    fixed_t ctop, cbottom, dx, dy, dz, step, stepx, stepy, stepz;
    fixed_t cfloor, cceil;
    int     divisor;

    if(in->isaline)
    {
        li = in->d.line;
        xline = P_XLine(li);

        if(xline->special)
            P_ShootSpecialLine(shootthing, li);

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            goto hitline;

        // crosses a two sided line
        P_LineOpening(li);

        dist = FixedMul(attackrange, in->frac);
        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(P_GetFixedp(frontsector, DMU_FLOOR_HEIGHT) !=
           P_GetFixedp(backsector, DMU_FLOOR_HEIGHT))
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if(slope > aimslope)
                goto hitline;
        }

        if(P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) !=
           P_GetFixedp(backsector, DMU_CEILING_HEIGHT))
        {
            slope = FixedDiv(opentop - shootz, dist);
            if(slope < aimslope)
                goto hitline;
        }

        // shot continues
        return true;

        // hit line
      hitline:
        lineWasHit = true;

        // Position a bit closer.
        frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
        pos[VX] = trace->x + FixedMul(trace->dx, frac);
        pos[VY] = trace->y + FixedMul(trace->dy, frac);
        pos[VZ] = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

        // Is it a sky hack wall? If the hitpoint is above the visible
        // line, no puff must be shown.
        if(backsector &&
           P_GetIntp(frontsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           P_GetIntp(backsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           (pos[VZ] > P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) ||
            pos[VZ] > P_GetFixedp(backsector, DMU_CEILING_HEIGHT) ))
            return false;

        // This is subsector where the trace originates.
        originSub = R_PointInSubsector(trace->x, trace->y);

        dx = pos[VX] - trace->x;
        dy = pos[VY] - trace->y;
        dz = pos[VZ] - shootz;

        if(dz != 0)
        {
            contact = R_PointInSubsector(pos[VX], pos[VY]);
            step = P_ApproxDistance3(dx, dy, dz);
            stepx = FixedDiv(dx, step);
            stepy = FixedDiv(dy, step);
            stepz = FixedDiv(dz, step);

            cfloor = P_GetFixedp(contact, DMU_FLOOR_HEIGHT);
            cceil = P_GetFixedp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cceil <= cfloor && contact != originSub)
            {
                dx -= 8 * stepx;
                dy -= 8 * stepy;
                dz -= 8 * stepz;
                pos[VX] = trace->x + dx;
                pos[VY] = trace->y + dy;
                pos[VZ] = shootz + dz;
                contact = R_PointInSubsector(pos[VX], pos[VY]);
            }

            // Should we backtrack to hit a plane instead?
            ctop = cceil - 4 * FRACUNIT;
            cbottom = cfloor + 4 * FRACUNIT;
            divisor = 2;

            // We must not hit a sky plane.
            if((pos[VZ] > ctop &&
                P_GetIntp(contact, DMU_CEILING_TEXTURE) == skyflatnum) ||
               (pos[VZ] < cbottom &&
                P_GetIntp(contact, DMU_FLOOR_TEXTURE) == skyflatnum))
                return false;

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((pos[VZ] > ctop || pos[VZ] < cbottom) && divisor <= 128)
            {
                // We aren't going to hit a line any more.
                lineWasHit = false;

                // Take a step backwards.
                pos[VX] -= dx / divisor;
                pos[VY] -= dy / divisor;
                pos[VZ] -= dz / divisor;

                // Divisor grows.
                divisor <<= 1;

                // Move forward until limits breached.
                while((dz > 0 && pos[VZ] <= ctop) || (dz < 0 && pos[VZ] >= cbottom))
                {
                    pos[VX] += dx / divisor;
                    pos[VY] += dy / divisor;
                    pos[VZ] += dz / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootthing);
        }

        // don't go any farther
        return false;
    }

    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

    // check for physical attacks on a ghost
    if(th->flags & MF_SHADOW && shootthing->player->readyweapon == WP_FIRST)
    {
        return (true);
    }

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->pos[VZ] + th->height - shootz, dist);

    if(thingtopslope < aimslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv(th->pos[VZ] - shootz, dist);

    if(thingbottomslope > aimslope)
        return true;            // shot under the thing

    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

    pos[VX] = trace->x + FixedMul(trace->dx, frac);
    pos[VY] = trace->y + FixedMul(trace->dy, frac);
    pos[VZ] = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    if(PuffType == MT_BLASTERPUFF1)
    {
        // Make blaster big puff
        mobj_t *mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLASTERPUFF2);
        S_StartSound(sfx_blshit, mo);
    }
    else
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

    if(la_damage)
    {
        if(!(in->d.thing->flags & MF_NOBLOOD) && P_Random() < 192)
        {
            P_BloodSplatter(pos[VX], pos[VY], pos[VZ], in->d.thing);
        }
        P_DamageMobj(th, shootthing, shootthing, la_damage);
    }

    // don't go any farther
    return false;
}

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->pos[VX] + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->pos[VY] + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->pos[VZ] + (t1->height >> 1) + 8 * FRACUNIT;

    topslope = 100 * FRACUNIT;
    bottomslope = -100 * FRACUNIT;

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->pos[VX], t1->pos[VY], x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_AimTraverse);

    if(linetarget)
    {
        if(!t1->player || !cfg.noAutoAim)
            return aimslope;
    }

    if(t1->player && cfg.noAutoAim)
    {
        // The slope is determined by lookdir.
        return FRACUNIT * (tan(LOOKDIR2RAD(t1->dplayer->lookdir)) / 1.2);
    }

    return 0;
}

/*
 * If damage == 0, it is just a test trace that will leave linetarget set.
 */
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->pos[VX] + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->pos[VY] + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->pos[VZ] + (t1->height >> 1) + 8 * FRACUNIT;
    if(t1->player && t1->type == MT_PLAYER)
    {
        // Players shoot at eye height.
        shootz = t1->pos[VZ] + (cfg.plrViewHeight - 5) * FRACUNIT;
    }

    shootz -= t1->floorclip;
    attackrange = distance;
    aimslope = slope;

    P_PathTraverse(t1->pos[VX], t1->pos[VY], x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_ShootTraverse);
}

boolean PTR_UseTraverse(intercept_t * in)
{
    int     side;

    if(!P_XLine(in->d.line)->special)
    {
        P_LineOpening(in->d.line);
        if(openrange <= 0)
        {
            // can't use through a wall
            return false;
        }
        // not a special line, but keep checking
        return true;
    }

    side = 0;
    if(P_PointOnLineSide(usething->pos[VX], usething->pos[VY], in->d.line) == 1)
        side = 1;

#if __JHERETIC__
    if(side == 1)
        return false;       // don't use back side
#endif

    P_UseSpecialLine(usething, in->d.line, side);

    // can use multiple line specials in a row with the PassThru flag
    if(P_GetIntp(in->d.line, DMU_FLAGS) & ML_PASSUSE)
        return true;

    // can't use more than one special line in a row
    return false;
}

/*
 * Looks for special lines in front of the player to activate.
 */
void P_UseLines(player_t *player)
{
    int     angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    usething = player->plr->mo;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->plr->mo->pos[VX];
    y1 = player->plr->mo->pos[VY];
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}

/*
 * Source is the creature that casued the explosion at spot
 */
boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
    fixed_t dx, dy, dz, dist;

    if(!(thing->flags & MF_SHOOTABLE))
    {
        return true;
    }

    if(thing->type == MT_MINOTAUR || thing->type == MT_SORCERER1 ||
       thing->type == MT_SORCERER2)
    {
        // Episode 2 and 3 bosses take no damage from PIT_RadiusAttack
        return (true);
    }

    dx = abs(thing->pos[VX] - bombspot->pos[VX]);
    dy = abs(thing->pos[VY] - bombspot->pos[VY]);
    dz = abs(thing->pos[VZ] - bombspot->pos[VZ]);

    dist = dx > dy ? dx : dy;

    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
        dist = dz > dist ? dz : dist;

    dist = (dist - thing->radius) >> FRACBITS;

    if(dist < 0)
        dist = 0;

    if(dist >= bombdamage)
        return true; // Out of range

    if(P_CheckSight(thing, bombspot))
    {
        // OK to damage, target is in direct path
        P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }
    return (true);
}

/*
 * Source is the creature that casued the explosion at spot
 */
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
{
    int     x, y, xl, xh, yl, yh;
    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    P_PointToBlock(spot->pos[VX] - dist, spot->pos[VY] - dist, &xl, &yl);
    P_PointToBlock(spot->pos[VX] + dist, spot->pos[VY] + dist, &xh, &yh);

    bombspot = spot;
    bombdamage = damage;

    if(spot->type == MT_POD && spot->target)
        bombsource = spot->target;
    else
        bombsource = source;

    for(y = yl; y <= yh; y++)
        for(x = xl; x <= xh; x++)
            P_BlockThingsIterator(x, y, PIT_RadiusAttack, 0);
}

/*
 * SECTOR HEIGHT CHANGING
 * After modifying a sectors floor or ceiling height, call this
 * routine to adjust the positions of all things that touch the
 * sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed
 * If Crunch is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes
 */
boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
    mobj_t *mo;

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->flags & MF_NOBLOCKMAP)
        return true;

    if(P_ThingHeightClip(thing))
        return true;            // keep checking

    // crunch bodies to giblets
    if(thing->health <= 0)
    {
        //P_SetMobjState (thing, S_GIBS);
        thing->height = 0;
        thing->radius = 0;
        return true;            // keep checking
    }

    // crunch dropped items
    if(thing->flags & MF_DROPPED)
    {
        P_RemoveMobj(thing);
        return true;            // keep checking
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return true;            // assume it is bloody gibs or something

    nofit = true;
    if(crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10);
        // spray blood in a random direction
        mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], thing->pos[VZ] + thing->height / 2,
                         MT_BLOOD);
        mo->momx = (P_Random() - P_Random()) << 12;
        mo->momy = (P_Random() - P_Random()) << 12;
    }

    return true; // keep checking (crush other things)
}

boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
    nofit = false;
    crushchange = crunch;

    // recheck heights for all things near the moving sector
    P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

    return nofit;
}
