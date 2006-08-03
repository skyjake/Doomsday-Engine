/* DE1: $Id$
 * Copyright (C) 1999- Activision
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
 */

/*
 *  Movement, collision handling.
 *  Shooting and aiming.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

#define topslope    (*gi.topslope)
#define bottomslope (*gi.bottomslope)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void CheckForPushSpecial(line_t *line, int side, mobj_t *mobj);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t tmbbox[4];
mobj_t *tmthing, *tsthing;
int     tmflags;
fixed_t tm[3];



// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
fixed_t tmdropoffz;




// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t *ceilingline;








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







mobj_t *bombsource;
mobj_t *bombspot;
int     bombdamage;

int     crushchange;
boolean nofit;

int     bombdistance;
boolean DamageSource;

int     tmfloorpic;
mobj_t *PuffSpawned;
mobj_t *onmobj;                 // generic global onmobj...used for landing on pods/players
mobj_t *BlockingMobj;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

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

    // Not allowed to stomp things?
    if(!(tmthing->flags2 & MF2_TELESTOMP))
        return false;

    P_DamageMobj(mo, tmthing, tmthing, 10000);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean alwaysstomp)
{
    int     xl, xh, yl, yh, bx, by;
    int     stomping;
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
    ceilingline = NULL;

    // $unstuck
    //tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    tmfloorz = tmdropoffz = P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_TEXTURE);

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
    //thing->dropoffz = tmdropoffz;   // killough $unstuck
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_SetThingPosition(thing);

    return true;
}

boolean PIT_ThrustStompThing(mobj_t *thing, void *data)
{
    fixed_t blockdist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    blockdist = thing->radius + tsthing->radius;
    if(abs(thing->pos[VX] - tsthing->pos[VX]) >= blockdist ||
       abs(thing->pos[VY] - tsthing->pos[VY]) >= blockdist ||
       (thing->pos[VZ] > tsthing->pos[VZ] + tsthing->height))
        return true;            // didn't hit it

    if(thing == tsthing)
        return true;            // don't clip against self

    P_DamageMobj(thing, tsthing, tsthing, 10001);
    tsthing->args[1] = 1;       // Mark thrust thing as bloody

    return true;
}

void PIT_ThrustSpike(mobj_t *actor)
{
    int     xl, xh, yl, yh, bx, by;
    int     x0, x2, y0, y2;

    tsthing = actor;

    x0 = actor->pos[VX] - actor->info->radius;
    x2 = actor->pos[VX] + actor->info->radius;
    y0 = actor->pos[VY] - actor->info->radius;
    y2 = actor->pos[VY] + actor->info->radius;

    P_PointToBlock(x0 - MAXRADIUS, y0 - MAXRADIUS, &xl, &yl);
    P_PointToBlock(x2 + MAXRADIUS, y2 + MAXRADIUS, &xh, &yh);

    // stomp on any things contacted
    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            P_BlockThingsIterator(bx, by, PIT_ThrustStompThing, 0);
}

/*
 * Adjusts tmfloorz and tmceilingz as lines are contacted
 */
boolean PIT_CheckLine(line_t *ld, void *data)
{


    fixed_t* bbox = P_GetPtrp(ld, DMU_BOUNDING_BOX);

    if(tmbbox[BOXRIGHT] <= bbox[BOXLEFT] ||
       tmbbox[BOXLEFT] >= bbox[BOXRIGHT] ||
       tmbbox[BOXTOP] <= bbox[BOXBOTTOM] ||
       tmbbox[BOXBOTTOM] >= bbox[BOXTOP])
        return true;

    if(P_BoxOnLineSide(tmbbox, ld) != -1)
        return true;

    // A line has been hit






    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.

    // $unstuck: allow player to move out of 1s wall, to prevent sticking
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR))
    {                           // One sided line
        if(tmthing->flags2 & MF2_BLASTED)
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        CheckForPushSpecial(ld, 0, tmthing);
        return false;
    }

    if(!(tmthing->flags & MF_MISSILE))
    {
        // explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKING)
        {
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
            CheckForPushSpecial(ld, 0, tmthing);
            return false;
        }

        // Block monsters only?
        if(!tmthing->player && tmthing->type != MT_CAMERA &&
           P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS)
        {
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
            return false;
        }
    }

    // set openrange, opentop, openbottom
    P_LineOpening(ld);

    // adjust floor / ceiling heights
    if(opentop < tmceilingz)
    {
        tmceilingz = opentop;
        ceilingline = ld;

    }

    if(openbottom > tmfloorz)
    {
        tmfloorz = openbottom;
        // killough $unstuck: remember floor linedef


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
             spechit = realloc(spechit, sizeof(*spechit) * spechit_max);
        }
        spechit[numspechit++] = ld;
    }


    return true;
}

//---------------------------------------------------------------------------
//
// FUNC PIT_CheckThing
//
//---------------------------------------------------------------------------

boolean PIT_CheckThing(mobj_t *thing, void *data)
{
    fixed_t blockdist;
    boolean solid;
    int     damage;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) || P_IsCamera(thing) || P_IsCamera(tmthing))    // $democam)
    {                           // Can't hit thing
        return (true);
    }
    blockdist = thing->radius + tmthing->radius;
    if(abs(thing->pos[VX] - tm[VX]) >= blockdist ||
       abs(thing->pos[VY] - tm[VY]) >= blockdist)
    {                           // Didn't hit thing
        return (true);
    }
    if(thing == tmthing)
    {                           // Don't clip against self
        return (true);
    }
    // Stop here if we are a client.
    if(IS_CLIENT)
        return false;

    BlockingMobj = thing;
    if(tmthing->flags2 & MF2_PASSMOBJ)
    {                           // check if a mobj passed over/under another object
        if(tmthing->type == MT_BISHOP && thing->type == MT_BISHOP)
        {                       // don't let bishops fly over other bishops
            return false;
        }
        if(tmthing->pos[VZ] >= thing->pos[VZ] + thing->height &&
           !(thing->flags & MF_SPECIAL))
        {
            return (true);
        }
        else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ] &&
                !(thing->flags & MF_SPECIAL))
        {                       // under thing
            return (true);
        }
    }
    // Check for skulls slamming into things
    if(tmthing->flags & MF_SKULLFLY)
    {
        if(tmthing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures
            if(!(thing->flags & MF_COUNTKILL))
            {
                return (false);
            }
        }
        else if(tmthing->type == MT_HOLY_FX)
        {
            if(thing->flags & MF_SHOOTABLE && thing != tmthing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                {               // don't attack other co-op players
                    return true;
                }
                if(thing->flags2 & MF2_REFLECTIVE &&
                   (thing->player || thing->flags2 & MF2_BOSS))
                {
                    tmthing->tracer = tmthing->target;
                    tmthing->target = thing;
                    return true;
                }
                if(thing->flags & MF_COUNTKILL || thing->player)
                {
                    tmthing->tracer = thing;
                }
                if(P_Random() < 96)
                {
                    damage = 12;
                    if(thing->player || thing->flags2 & MF2_BOSS)
                    {
                        damage = 3;
                        // ghost burns out faster when attacking players/bosses
                        tmthing->health -= 6;
                    }
                    P_DamageMobj(thing, tmthing, tmthing->target, damage);
                    if(P_Random() < 128)
                    {
                        P_SpawnMobj(tmthing->pos[VX], tmthing->pos[VY],
                                    tmthing->pos[VZ], MT_HOLY_PUFF);
                        S_StartSound(SFX_SPIRIT_ATTACK, tmthing);
                        if(thing->flags & MF_COUNTKILL && P_Random() < 128 &&
                           !S_IsPlaying(SFX_PUPPYBEAT, thing))
                        {
                            if((thing->type == MT_CENTAUR) ||
                               (thing->type == MT_CENTAURLEADER) ||
                               (thing->type == MT_ETTIN))
                            {
                                S_StartSound(SFX_PUPPYBEAT, thing);
                            }
                        }
                    }
                }
                if(thing->health <= 0)
                {
                    tmthing->tracer = NULL;
                }
            }
            return true;
        }
        damage = ((P_Random() % 8) + 1) * tmthing->damage;
        P_DamageMobj(thing, tmthing, tmthing, damage);
        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;
        P_SetMobjState(tmthing, tmthing->info->seestate);
        return (false);
    }
    // Check for blasted thing running into another
    if(tmthing->flags2 & MF2_BLASTED && thing->flags & MF_SHOOTABLE)
    {
        if(!(thing->flags2 & MF2_BOSS) && (thing->flags & MF_COUNTKILL))
        {
            thing->momx += tmthing->momx;
            thing->momy += tmthing->momy;
            if(thing->dplayer)
                thing->dplayer->flags |= DDPF_FIXMOM;
            if((thing->momx + thing->momy) > 3 * FRACUNIT)
            {
                damage = (tmthing->info->mass / 100) + 1;
                P_DamageMobj(thing, tmthing, tmthing, damage);
                damage = (thing->info->mass / 100) + 1;
                P_DamageMobj(tmthing, thing, thing, damage >> 2);
            }
            return (false);
        }
    }
    // Check for missile
    if(tmthing->flags & MF_MISSILE)
    {
        // Check for a non-shootable mobj
        if(thing->flags2 & MF2_NONSHOOTABLE)
        {
            return true;
        }
        // Check if it went over / under
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
        {                       // Over thing
            return (true);
        }
        if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
        {                       // Under thing
            return (true);
        }
        if(tmthing->flags2 & MF2_FLOORBOUNCE)
        {
            if(tmthing->target == thing || !(thing->flags & MF_SOLID))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        if(tmthing->type == MT_LIGHTNING_FLOOR ||
           tmthing->type == MT_LIGHTNING_CEILING)
        {
            if(thing->flags & MF_SHOOTABLE && thing != tmthing->target)
            {
                if(thing->info->mass != DDMAXINT)
                {
                    thing->momx += tmthing->momx >> 4;
                    thing->momy += tmthing->momy >> 4;
                    if(thing->dplayer)
                        thing->dplayer->flags |= DDPF_FIXMOM;
                }
                if((!thing->player && !(thing->flags2 & MF2_BOSS)) ||
                   !(leveltime & 1))
                {
                    if(thing->type == MT_CENTAUR ||
                       thing->type == MT_CENTAURLEADER)
                    {           // Lightning does more damage to centaurs
                        P_DamageMobj(thing, tmthing, tmthing->target, 9);
                    }
                    else
                    {
                        P_DamageMobj(thing, tmthing, tmthing->target, 3);
                    }
                    if(!(S_IsPlaying(SFX_MAGE_LIGHTNING_ZAP, tmthing)))
                    {
                        S_StartSound(SFX_MAGE_LIGHTNING_ZAP, tmthing);
                    }
                    if(thing->flags & MF_COUNTKILL && P_Random() < 64 &&
                       !S_IsPlaying(SFX_PUPPYBEAT, thing))
                    {
                        if((thing->type == MT_CENTAUR) ||
                           (thing->type == MT_CENTAURLEADER) ||
                           (thing->type == MT_ETTIN))
                        {
                            S_StartSound(SFX_PUPPYBEAT, thing);
                        }
                    }
                }
                tmthing->health--;
                if(tmthing->health <= 0 || thing->health <= 0)
                {
                    return false;
                }
                if(tmthing->type == MT_LIGHTNING_FLOOR)
                {
                    if(tmthing->special2 &&
                       !((mobj_t *) tmthing->special2)->tracer)
                    {
                        ((mobj_t *) tmthing->special2)->tracer = thing;
                    }
                }
                else if(!tmthing->tracer)
                {
                    tmthing->tracer = thing;
                }
            }
            return true;        // lightning zaps through all sprites
        }
        else if(tmthing->type == MT_LIGHTNING_ZAP)
        {
            mobj_t *lmo;

            if(thing->flags & MF_SHOOTABLE && thing != tmthing->target)
            {
                lmo = (mobj_t *) tmthing->special2;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->special2 &&
                           !((mobj_t *) lmo->special2)->tracer)
                        {
                            ((mobj_t *) lmo->special2)->tracer = thing;
                        }
                    }
                    else if(!lmo->tracer)
                    {
                        lmo->tracer = thing;
                    }
                    if(!(leveltime & 3))
                    {
                        lmo->health--;
                    }
                }
            }
        }
        else if(tmthing->type == MT_MSTAFF_FX2 && thing != tmthing->target)
        {
            if(!thing->player && !(thing->flags2 & MF2_BOSS))
            {
                switch (thing->type)
                {
                case MT_FIGHTER_BOSS:   // these not flagged boss
                case MT_CLERIC_BOSS:    // so they can be blasted
                case MT_MAGE_BOSS:
                    break;
                default:
                    P_DamageMobj(thing, tmthing, tmthing->target, 10);
                    return true;
                    break;
                }
            }
        }
        if(tmthing->target && tmthing->target->type == thing->type)
        {                       // Don't hit same species as originator
            if(thing == tmthing->target)
            {                   // Don't missile self
                return (true);
            }
            if(!thing->player)
            {                   // Hit same species as originator, explode, no damage
                return (false);
            }
        }
        if(!(thing->flags & MF_SHOOTABLE))
        {                       // Didn't do any damage
            return !(thing->flags & MF_SOLID);
        }
        if(tmthing->flags2 & MF2_RIP)
        {
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
            {                   // Ok to spawn some blood
                P_RipperBlood(tmthing);
            }
            //S_StartSound(sfx_ripslop, tmthing);
            damage = ((P_Random() & 3) + 2) * tmthing->damage;
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
            return (true);
        }
        // Do damage
        damage = ((P_Random() % 8) + 1) * tmthing->damage;
        if(damage)
        {
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(tmthing->type == MT_TELOTHER_FX1) &&
               !(tmthing->type == MT_TELOTHER_FX2) &&
               !(tmthing->type == MT_TELOTHER_FX3) &&
               !(tmthing->type == MT_TELOTHER_FX4) &&
               !(tmthing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
            {
                P_BloodSplatter(tmthing->pos[VX], tmthing->pos[VY], tmthing->pos[VZ], thing);
            }
            P_DamageMobj(thing, tmthing, tmthing->target, damage);
        }
        return (false);
    }
    if(thing->flags2 & MF2_PUSHABLE && !(tmthing->flags2 & MF2_CANNOTPUSH))
    {                           // Push thing
        thing->momx += tmthing->momx >> 2;
        thing->momy += tmthing->momy >> 2;
        if(thing->dplayer)
            thing->dplayer->flags |= DDPF_FIXMOM;
    }
    // Check for special thing
    if(thing->flags & MF_SPECIAL)
    {
        solid = thing->flags & MF_SOLID;
        if(tmflags & MF_PICKUP)
        {                       // Can be picked up by tmthing
            P_TouchSpecialThing(thing, tmthing);    // Can remove thing
        }
        return (!solid);
    }
    return (!(thing->flags & MF_SOLID));
}

//---------------------------------------------------------------------------
//
// PIT_CheckOnmobjZ
//
//---------------------------------------------------------------------------

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
}

/*
   ===============================================================================

   MOVEMENT CLIPPING

   ===============================================================================
 */

//----------------------------------------------------------------------------
//
// FUNC P_TestMobjLocation
//
// Returns true if the mobj is not blocked by anything at its current
// location, otherwise returns false.
//
//----------------------------------------------------------------------------

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






    tm[VX] = x;
    tm[VY] = y;
    tm[VZ] = z;


    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(x, y), DMU_SECTOR);

    // $unstuck: floorline used with tmunstuck
    ceilingline = NULL;

    // $unstuck
    //tmunstuck = thing->dplayer && thing->dplayer->mo == thing;

    // The base floor / ceiling is from the subsector
    // that contains the point. Any contacted lines the
    // step closer together will adjust them.
    tmfloorz = tmdropoffz = P_GetFixedp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsec, DMU_FLOOR_TEXTURE);
    validCount++;
    numspechit = 0;

    if(tmflags & MF_NOCLIP && !(tmflags & MF_SKULLFLY))
        return true;

    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    // The camera goes through all objects -- jk
    if(thing->type != MT_CAMERA)
    {
        BlockingMobj = NULL;
        for(bx = xl; bx <= xh; bx++)
            for(by = yl; by <= yh; by++)
                if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, 0))
                    return false;
    }

    // check lines
    if(tmflags & MF_NOCLIP)
        return true;

    BlockingMobj = NULL;
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

//=============================================================================
//
// P_CheckOnmobj(mobj_t *thing)
//
//              Checks if the new Z position is legal
//=============================================================================

mobj_t *P_CheckOnmobj(mobj_t *thing)
{
    int     xl, xh, yl, yh, bx, by;
    subsector_t *newsubsec;
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

    newsubsec = R_PointInSubsector(x, y);
    ceilingline = NULL;

    //
    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    //
    tmfloorz = tmdropoffz = P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_TEXTURE);

    validCount++;
    numspechit = 0;

    if(tmflags & MF_NOCLIP)
        return NULL;

    //
    // check things first, possibly picking things up
    // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into adjacent
    // blocks by up to MAXRADIUS units
    //
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
}

//=============================================================================
//
// P_FakeZMovement
//
//              Fake the zmovement so that we can check if a move is legal
//=============================================================================

void P_FakeZMovement(mobj_t *mo)
{
    int     dist;
    int     delta;

    if(P_IsCamera(mo))
        return;                 // $democam

    //
    // adjust height
    //
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

    //
    // clip movement
    //
    if(mo->pos[VZ] <= mo->floorz)
    {                           // Hit the floor
        mo->pos[VZ] = mo->floorz;
        if(mo->momz < 0)
            mo->momz = 0;

        if(mo->flags & MF_SKULLFLY)
        {                       // The skull slammed into something
            mo->momz = -mo->momz;
        }

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
    {                           // hit the ceiling
        if(mo->momz > 0)
            mo->momz = 0;
        mo->pos[VZ] = mo->ceilingz - mo->height;
        if(mo->flags & MF_SKULLFLY)
        {                       // the skull slammed into something
            mo->momz = -mo->momz;
        }
    }
}

//===========================================================================
//
// CheckForPushSpecial
//
//===========================================================================

static void CheckForPushSpecial(line_t *line, int side, mobj_t *mobj)
{
    if(P_XLine(line)->special)
    {
        if(mobj->flags2 & MF2_PUSHWALL)
        {
            P_ActivateLine(line, mobj, side, SPAC_PUSH);
        }
        else if(mobj->flags2 & MF2_IMPACT)
        {
            P_ActivateLine(line, mobj, side, SPAC_IMPACT);
        }
    }
}

/*
   ===================
   =
   = P_TryMove
   =
   = Attempt to move to a new position, crossing special lines unless MF_TELEPORT
   = is set
   =
   ===================
 */

boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y)
{
    fixed_t oldpos[3];
    int     side, oldside;
    line_t *ld;

    floatok = false;
    if(!P_CheckPosition(thing, x, y))
    {                           // Solid wall or thing
        if(!BlockingMobj || BlockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(BlockingMobj->pos[VZ] + BlockingMobj->height - thing->pos[VZ] >
                24 * FRACUNIT ||
                (P_GetFixedp(BlockingMobj->subsector, DMU_CEILING_HEIGHT) -
                 (BlockingMobj->pos[VZ] + BlockingMobj->height) < thing->height) ||
                (tmceilingz - (BlockingMobj->pos[VZ] + BlockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
    }
    if(!(thing->flags & MF_NOCLIP))
    {
        if(tmceilingz - tmfloorz < thing->height)
        {                       // Doesn't fit
            goto pushline;
        }
        floatok = true;
        if(!(thing->flags & MF_TELEPORT) &&
           tmceilingz - thing->pos[VZ] < thing->height &&
           thing->type != MT_LIGHTNING_CEILING && !(thing->flags2 & MF2_FLY))
        {                       // mobj must lower itself to fit
            goto pushline;
        }
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > tmceilingz)
            {
                thing->momz = -8 * FRACUNIT;
                goto pushline;
            }
            else if(thing->pos[VZ] < tmfloorz &&
                    tmfloorz - tmdropoffz > 24 * FRACUNIT)
            {
                thing->momz = 8 * FRACUNIT;
                goto pushline;
            }
        }
        if(!(thing->flags & MF_TELEPORT)
           // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
           && thing->type != MT_MNTRFX2 && thing->type != MT_LIGHTNING_FLOOR &&
           tmfloorz - thing->pos[VZ] > 24 * FRACUNIT)
        {
            goto pushline;
        }
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (tmfloorz - tmdropoffz > 24 * FRACUNIT) &&
           !(thing->flags2 & MF2_BLASTED))
        {                       // Can't move over a dropoff unless it's been blasted
            return (false);
        }
        if(thing->flags2 & MF2_CANTLEAVEFLOORPIC &&
           (tmfloorpic != P_GetIntp(thing->subsector, DMU_FLOOR_TEXTURE) ||
            tmfloorz - thing->pos[VZ] != 0))
        {                       // must stay within a sector of a certain floor type
            return false;
        }
    }

    //
    // the move is ok, so link the thing into its new position
    //
    P_UnsetThingPosition(thing);

    memcpy(oldpos, thing->pos, sizeof(oldpos));

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->floorpic = tmfloorpic;
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

    //
    // if any special lines were hit, do the effect
    //
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
                {
                    if(thing->player)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_CROSS);
                    }
                    else if(thing->flags2 & MF2_MCROSS)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_MCROSS);
                    }
                    else if(thing->flags2 & MF2_PCROSS)
                    {
                        P_ActivateLine(ld, thing, oldside, SPAC_PCROSS);
                    }
                }
            }
        }
    }
    return true;

  pushline:
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        int     numSpecHitTemp;

        if(tmthing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        }
        numSpecHitTemp = numspechit;
        while(numSpecHitTemp--)
        {
            // see if the line was crossed
            ld = spechit[numSpecHitTemp];
            side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
            CheckForPushSpecial(ld, side, thing);
        }
    }
    return false;
}

/*
   ==================
   =
   = P_ThingHeightClip
   =
   = Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
   = anf possibly thing->z
   =
   = This is called for all nearby monsters whenever a sector changes height
   =
   = If the thing doesn't fit, the z will be set to the lowest value and
   = false will be returned
   ==================
 */

boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean onfloor;

    onfloor = (thing->pos[VZ] == thing->floorz);

    P_CheckPosition(thing, thing->pos[VX], thing->pos[VY]);
    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->floorpic = tmfloorpic;

    if(onfloor)
    {                           // walking monsters rise and fall with the floor
        if((thing->pos[VZ] - thing->floorz < 9 * FRACUNIT) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->pos[VZ] = thing->floorz;
        }
    }
    else
    {                           // don't adjust a floating monster unless forced to
        if(thing->pos[VZ] + thing->height > thing->ceilingz)
            thing->pos[VZ] = thing->ceilingz - thing->height;
    }

    if(thing->ceilingz - thing->floorz < thing->height)
        return false;

    return true;
}

/*
   ==============================================================================

   SLIDE MOVE

   Allows the player to slide along any angled walls

   ==============================================================================
 */

/*
   ==================
   =
   = P_HitSlideLine
   =
   = Adjusts the xmove / ymove so that the next move will slide along the wall
   ==================
 */

void P_HitSlideLine(line_t *ld)
{
    int     side;
    angle_t lineangle, moveangle, deltaangle;
    fixed_t movelen, newlen;

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

    lineangle = R_PointToAngle2(0, 0, P_GetFixedp(ld, DMU_DX),
                                P_GetFixedp(ld, DMU_DY));
    if(side == 1)
        lineangle += ANG180;
    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;
    if(deltaangle > ANG180)
        deltaangle += ANG180;
    //              Con_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);
    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

/*
   ==============
   =
   = PTR_SlideTraverse
   =
   ==============
 */

boolean PTR_SlideTraverse(intercept_t * in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;
    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
            return true;        // don't hit the back side
        goto isblocking;
    }

    P_LineOpening(li);          // set openrange, opentop, openbottom
    if(openrange < slidemo->height)
        goto isblocking;        // doesn't fit

    if(opentop - slidemo->pos[VZ] < slidemo->height)
        goto isblocking;        // mobj is too high

    if(openbottom - slidemo->pos[VZ] > 24 * FRACUNIT)
        goto isblocking;        // too big a step up

    return true;                // this line doesn't block movement

    // the line does block movement, see if it is closer than best so far
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
   ==================
   =
   = P_SlideMove
   =
   = The momx / momy move is bad, so try to slide along a wall
   =
   = Find the first line hit, move flush to it, and slide along it
   =
   = This is a kludgy mess.
   ==================
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
        // the move must have hit the middle, so stairstep
      stairstep:
        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->momy))
            P_TryMove(mo, mo->pos[VX] + mo->momx, mo->pos[VY]);
            return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if(bestslidefrac > 0)
    {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        // killough $dropoff_fix
        if(!P_TryMove(mo, mo->pos[VX] + newx, mo->pos[VY] + newy))
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
    if(!P_TryMove(mo, mo->pos[VX] + tmxmove, mo->pos[VY] + tmymove))
    {
        goto retry;
    }
}

//============================================================================
//
// PTR_BounceTraverse
//
//============================================================================

boolean PTR_BounceTraverse(intercept_t * in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_BounceTraverse: not a line?");

    if(!in->isaline)
        Con_Error("PTR_BounceTraverse: not a line?");

    li = in->d.line;
    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
            return true;        // don't hit the back side
        goto bounceblocking;
    }

    P_LineOpening(li);          // set openrange, opentop, openbottom
    if(openrange < slidemo->height)
        goto bounceblocking;    // doesn't fit

    if(opentop - slidemo->pos[VZ] < slidemo->height)
        goto bounceblocking;    // mobj is too high
    return true;                // this line doesn't block movement

    // the line does block movement, see if it is closer than best so far
  bounceblocking:
    if(in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }
    return false;               // stop
}

//============================================================================
//
// P_BounceWall
//
//============================================================================

void P_BounceWall(mobj_t *mo)
{
    fixed_t leadx, leady;
    int     side;
    angle_t lineangle, moveangle, deltaangle;
    fixed_t movelen;

    slidemo = mo;

    //
    // trace along the three leading corners
    //
    if(mo->momx > 0)
    {
        leadx = mo->pos[VX] + mo->radius;
    }
    else
    {
        leadx = mo->pos[VX] - mo->radius;
    }
    if(mo->momy > 0)
    {
        leady = mo->pos[VY] + mo->radius;
    }
    else
    {
        leady = mo->pos[VY] - mo->radius;
    }
    bestslidefrac = FRACUNIT + 1;
    P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_BounceTraverse);

    if(!bestslideline)
        return;                 // We don't want to crash.

    side = P_PointOnLineSide(mo->pos[VX], mo->pos[VY], bestslideline);
    lineangle = R_PointToAngle2(0, 0,
        P_GetFixedp(bestslideline, DMU_DX),
        P_GetFixedp(bestslideline, DMU_DY));
    if(side == 1)
        lineangle += ANG180;
    moveangle = R_PointToAngle2(0, 0, mo->momx, mo->momy);
    deltaangle = (2 * lineangle) - moveangle;
    //  if (deltaangle > ANG180)
    //      deltaangle += ANG180;
    //              Con_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(mo->momx, mo->momy);
    movelen = FixedMul(movelen, 0.75 * FRACUNIT);   // friction
    if(movelen < FRACUNIT)
        movelen = 2 * FRACUNIT;
    mo->momx = FixedMul(movelen, finecosine[deltaangle]);
    mo->momy = FixedMul(movelen, finesine[deltaangle]);
}

/*
 * Sets linetaget and aimslope when a target is aimed at.
 */
boolean PTR_AimTraverse(intercept_t *in)
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

    if(th->player && IS_NETGAME && !deathmatch)
        return true; // don't aim at fellow co-op players

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);

    thingtopslope = FixedDiv(th->pos[VZ] + th->height - shootz, dist);
    if(thingtopslope < bottomslope)
        return true;            // shot over the thing

    // Too far below?
// $addtocfg $limitautoaimZ:
    if(th->pos[VZ] + th->height < shootz - FixedDiv(attackrange, 1.2 * FRACUNIT))
        return true;

    thingbottomslope = FixedDiv(th->pos[VZ] - shootz, dist);
    if(thingbottomslope > topslope)
        return true;            // shot under the thing

    // Too far above?
// $addtocfg $limitautoaimZ:
    if(th->pos[VZ] > shootz + FixedDiv(attackrange, 1.2 * FRACUNIT))
        return true;

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
    line_t *li;
    mobj_t *th;
    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope, thingbottomslope;
    divline_t *trace = (divline_t *) DD_GetVariable(DD_TRACE_ADDRESS);
    sector_t *frontsector = NULL;
    sector_t *backsector = NULL;

    extern mobj_t LavaInflictor;

    // These were added for the plane-hitpoint algo.
    // FIXME: This routine is getting rather bloated.
    boolean lineWasHit;
    subsector_t *contact, *originSub;
    fixed_t ctop, cbottom, d[3], step, stepv[3], tracepos[3];
    fixed_t cfloor, cceil;
    int     divisor;

    tracepos[VX] = trace->x;
    tracepos[VY] = trace->y;
    tracepos[VZ] = shootz;

    if(in->isaline)
    {
        li = in->d.line;
        if(P_XLine(li)->special)
        {
            P_ActivateLine(li, shootthing, 0, SPAC_IMPACT);
            //          P_ShootSpecialLine (shootthing, li);
        }
        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            goto hitline;

        //
        // crosses a two sided line
        //
        P_LineOpening(li);

        dist = FixedMul(attackrange, in->frac);

        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        slope = FixedDiv(openbottom - tracepos[VZ], dist);
        if(slope > aimslope)
            goto hitline;

        slope = FixedDiv(opentop - tracepos[VZ], dist);
        if(slope < aimslope)
            goto hitline;

        // shot continues
        return true;

        // hit line
      hitline:
        lineWasHit = true;

        // Position a bit closer.
        frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
        pos[VX] = tracepos[VX] + FixedMul(trace->dx, frac);
        pos[VY] = tracepos[VY] + FixedMul(trace->dy, frac);
        pos[VZ] = tracepos[VZ] + FixedMul(aimslope, FixedMul(frac, attackrange));

        // Is it a sky hack wall? If the hitpoint is above the visible
        // line, no puff must be shown.
        frontsector = P_GetPtrp(li, DMU_FRONT_SECTOR);
        backsector = P_GetPtrp(li, DMU_BACK_SECTOR);

        if(backsector &&
           P_GetIntp(frontsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           P_GetIntp(backsector, DMU_CEILING_TEXTURE) == skyflatnum &&
           (pos[VZ] > P_GetFixedp(frontsector, DMU_CEILING_HEIGHT) ||
            pos[VZ] > P_GetFixedp(backsector, DMU_CEILING_HEIGHT)))
            return false;

        // This is subsector where the trace originates.
        originSub = R_PointInSubsector(tracepos[VX], tracepos[VY]);

        d[VX] = pos[VX] - tracepos[VX];
        d[VY] = pos[VY] - tracepos[VY];
        d[VZ] = pos[VZ] - tracepos[VZ];

        if(d[VZ] != 0)
        {
            contact = R_PointInSubsector(pos[VX], pos[VY]);
            step = P_ApproxDistance3(d[VX], d[VY], d[VZ]);
            stepv[VX] = FixedDiv(d[VX], step);
            stepv[VY] = FixedDiv(d[VY], step);
            stepv[VZ] = FixedDiv(d[VZ], step);

            cfloor = P_GetFixedp(contact, DMU_FLOOR_HEIGHT);
            cceil = P_GetFixedp(contact, DMU_CEILING_HEIGHT);
            // Backtrack until we find a non-empty sector.
            while(cceil <= cfloor && contact != originSub)
            {
                d[VX] -= 8 * stepv[VX];
                d[VY] -= 8 * stepv[VY];
                d[VZ] -= 8 * stepv[VZ];
                pos[VX] = tracepos[VX] + d[VX];
                pos[VY] = tracepos[VY] + d[VY];
                pos[VZ] = tracepos[VZ] + d[VZ];
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
                pos[VX] -= d[VX] / divisor;
                pos[VY] -= d[VY] / divisor;
                pos[VZ] -= d[VZ] / divisor;

                // Divisor grows.
                divisor <<= 1;

                // Move forward until limits breached.
                while((d[VZ] > 0 && pos[VZ] <= ctop) || (d[VZ] < 0 && pos[VZ] >= cbottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

/*      // JHEXEN doesn't have XG yet!
        if(lineWasHit && li->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootthing);
        }
*/
        // don't go any farther
        return false;
    }
    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->pos[VZ] + th->height - tracepos[VZ], dist);

    if(thingtopslope < aimslope)
        return true;            // shot over the thing

    thingbottomslope = FixedDiv(th->pos[VZ] - tracepos[VZ], dist);
    if(thingbottomslope > aimslope)
        return true;            // shot under the thing

    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

    pos[VX] = tracepos[VX] + FixedMul(trace->dx, frac);
    pos[VY] = tracepos[VY] + FixedMul(trace->dy, frac);
    pos[VZ] = tracepos[VZ] + FixedMul(aimslope, FixedMul(frac, attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

    if(la_damage)
    {
        if(!(in->d.thing->flags & MF_NOBLOOD) &&
           !(in->d.thing->flags2 & MF2_INVULNERABLE))
        {
            if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
            {
                P_BloodSplatter2(pos[VX], pos[VY], pos[VZ], in->d.thing);
            }
            if(P_Random() < 192)
            {
                P_BloodSplatter(pos[VX], pos[VY], pos[VZ], in->d.thing);
            }
        }
        if(PuffType == MT_FLAMEPUFF2)
        {                       // Cleric FlameStrike does fire damage
            P_DamageMobj(th, &LavaInflictor, shootthing, la_damage);
        }
        else
        {
            P_DamageMobj(th, shootthing, shootthing, la_damage);
        }
    }

    // don't go any farther
    return false;
}

/*
   =================
   =
   = P_AimLineAttack
   =
   =================
 */

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
    fixed_t x2, y2;

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
        // While autoaiming, we accept this slope.
        if(!t1->player || !cfg.noAutoAim)
            return aimslope;

    if(t1->player && cfg.noAutoAim)
    {
        // We are aiming manually, so the slope is determined by lookdir.
        return FRACUNIT * (tan(LOOKDIR2RAD(t1->dplayer->lookdir)) / 1.2);
    }

    return 0;
}

/*
   =================
   =
   = P_LineAttack
   =
   = if damage == 0, it is just a test trace that will leave linetarget set
   =
   =================
 */

void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage)
{
    fixed_t x2, y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->pos[VX] + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->pos[VY] + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->pos[VZ] + (t1->height >> 1) + 8 * FRACUNIT;

    if(t1->player &&
      (t1->player->class == PCLASS_FIGHTER ||
       t1->player->class == PCLASS_CLERIC ||
       t1->player->class == PCLASS_MAGE))
    {
        shootz = t1->pos[VZ] + (cfg.plrViewHeight - 5) * FRACUNIT;
    }

    shootz -= t1->floorclip;
    attackrange = distance;
    aimslope = slope;

    if(P_PathTraverse
       (t1->pos[VX], t1->pos[VY], x2, y2, PT_ADDLINES | PT_ADDTHINGS, PTR_ShootTraverse))
    {
        switch (PuffType)
        {
        case MT_PUNCHPUFF:
            S_StartSound(SFX_FIGHTER_PUNCH_MISS, t1);
            break;
        case MT_HAMMERPUFF:
        case MT_AXEPUFF:
        case MT_AXEPUFF_GLOW:
            S_StartSound(SFX_FIGHTER_HAMMER_MISS, t1);
            break;
        case MT_FLAMEPUFF:
            P_SpawnPuff(x2, y2, shootz + FixedMul(slope, distance));
            break;
        default:
            break;
        }
    }
}

/*
   ==============================================================================

   USE LINES

   ==============================================================================
 */

mobj_t *usething;

boolean PTR_UseTraverse(intercept_t * in)
{
    int     sound;
    fixed_t pheight;

    if(!P_XLine(in->d.line)->special)
    {
        P_LineOpening(in->d.line);
        if(openrange <= 0)
        {
            if(usething->player)
            {
                switch (usething->player->class)
                {
                case PCLASS_FIGHTER:
                    sound = SFX_PLAYER_FIGHTER_FAILED_USE;
                    break;
                case PCLASS_CLERIC:
                    sound = SFX_PLAYER_CLERIC_FAILED_USE;
                    break;
                case PCLASS_MAGE:
                    sound = SFX_PLAYER_MAGE_FAILED_USE;
                    break;
                case PCLASS_PIG:
                    sound = SFX_PIG_ACTIVE1;
                    break;
                default:
                    sound = SFX_NONE;
                    break;
                }
                S_StartSound(sound, usething);
            }
            return false;       // can't use through a wall
        }
        if(usething->player)
        {
            pheight = usething->pos[VZ] + (usething->height / 2);
            if((opentop < pheight) || (openbottom > pheight))
            {
                switch (usething->player->class)
                {
                case PCLASS_FIGHTER:
                    sound = SFX_PLAYER_FIGHTER_FAILED_USE;
                    break;
                case PCLASS_CLERIC:
                    sound = SFX_PLAYER_CLERIC_FAILED_USE;
                    break;
                case PCLASS_MAGE:
                    sound = SFX_PLAYER_MAGE_FAILED_USE;
                    break;
                case PCLASS_PIG:
                    sound = SFX_PIG_ACTIVE1;
                    break;
                default:
                    sound = SFX_NONE;
                    break;
                }
                S_StartSound(sound, usething);
            }
        }
        return true;            // not a special line, but keep checking
    }

    if(P_PointOnLineSide(usething->pos[VX], usething->pos[VY], in->d.line) == 1)
        return false;           // don't use back sides

    //  P_UseSpecialLine (usething, in->d.line);
    P_ActivateLine(in->d.line, usething, 0, SPAC_USE);

    return false;               // can't use for than one special line in a row
}

/*
   ================
   =
   = P_UseLines
   =
   = Looks for special lines in front of the player to activate
   ================
 */

void P_UseLines(player_t *player)
{
    int     angle;
    fixed_t x1, y1, x2, y2;

    usething = player->plr->mo;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
    x1 = player->plr->mo->pos[VX];
    y1 = player->plr->mo->pos[VY];
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}

//==========================================================================
//
// PTR_PuzzleItemTraverse
//
//==========================================================================

#define USE_PUZZLE_ITEM_SPECIAL 129

static mobj_t *PuzzleItemUser;
static int PuzzleItemType;
static boolean PuzzleActivated;

boolean PTR_PuzzleItemTraverse(intercept_t * in)
{
    mobj_t *mobj;
    int     sound;

    if(in->isaline)
    {                           // Check line
        if(P_XLine(in->d.line)->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            P_LineOpening(in->d.line);
            if(openrange <= 0)
            {
                sound = SFX_NONE;
                if(PuzzleItemUser->player)
                {
                    switch (PuzzleItemUser->player->class)
                    {
                    case PCLASS_FIGHTER:
                        sound = SFX_PUZZLE_FAIL_FIGHTER;
                        break;
                    case PCLASS_CLERIC:
                        sound = SFX_PUZZLE_FAIL_CLERIC;
                        break;
                    case PCLASS_MAGE:
                        sound = SFX_PUZZLE_FAIL_MAGE;
                        break;
                    default:
                        sound = SFX_NONE;
                        break;
                    }
                }
                S_StartSound(sound, PuzzleItemUser);
                return false;   // can't use through a wall
            }
            return true;        // Continue searching
        }
        if(P_PointOnLineSide(PuzzleItemUser->pos[VX], PuzzleItemUser->pos[VY], in->d.line)
           == 1)
        {                       // Don't use back sides
            return false;
        }
        if(PuzzleItemType != P_XLine(in->d.line)->arg1)
        {                       // Item type doesn't match
            return false;
        }
        P_StartACS(P_XLine(in->d.line)->arg2, 0, &P_XLine(in->d.line)->arg3,
                   PuzzleItemUser, in->d.line, 0);
        P_XLine(in->d.line)->special = 0;
        PuzzleActivated = true;
        return false;           // Stop searching
    }
    // Check thing
    mobj = in->d.thing;
    if(mobj->special != USE_PUZZLE_ITEM_SPECIAL)
    {                           // Wrong special
        return true;
    }
    if(PuzzleItemType != mobj->args[0])
    {                           // Item type doesn't match
        return true;
    }
    P_StartACS(mobj->args[1], 0, &mobj->args[2], PuzzleItemUser, NULL, 0);
    mobj->special = 0;
    PuzzleActivated = true;
    return false;               // Stop searching
}

//==========================================================================
//
// P_UsePuzzleItem
//
// Returns true if the puzzle item was used on a line or a thing.
//
//==========================================================================

boolean P_UsePuzzleItem(player_t *player, int itemType)
{
    int     angle;
    fixed_t x1, y1, x2, y2;

    PuzzleItemType = itemType;
    PuzzleItemUser = player->plr->mo;
    PuzzleActivated = false;
    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
    x1 = player->plr->mo->pos[VX];
    y1 = player->plr->mo->pos[VY];
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];
    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_PuzzleItemTraverse);
    return PuzzleActivated;
}

/*
   ==============================================================================

   RADIUS ATTACK

   ==============================================================================
 */

/*
   =================
   =
   = PIT_RadiusAttack
   =
   = Source is the creature that casued the explosion at spot
   =================
 */

boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
    fixed_t dx, dy, dz, dist;
    int     damage;

    if(!(thing->flags & MF_SHOOTABLE))
    {
        return true;
    }
    //  if(thing->flags2&MF2_BOSS)
    //  {   // Bosses take no damage from PIT_RadiusAttack
    //      return(true);
    //  }
    if(!DamageSource && thing == bombsource)
    {                           // don't damage the source of the explosion
        return true;
    }

    dx = abs(thing->pos[VX] - bombspot->pos[VX]);
    dy = abs(thing->pos[VY] - bombspot->pos[VY]);
    dz = abs(thing->pos[VZ] - bombspot->pos[VZ]);

    dist = dx > dy ? dx : dy;

    if(!cfg.netNoMaxZRadiusAttack)
        dist = dz > dist ? dz : dist;

    dist = (dist - thing->radius) >> FRACBITS;

    if(dist < 0)
        dist = 0;

    if(dist >= bombdistance)
    {                           // Out of range
        return true;
    }

    if(P_CheckSight(thing, bombspot))
    {                           // OK to damage, target is in direct path
        damage = (bombdamage * (bombdistance - dist) / bombdistance) + 1;
        if(thing->player)
        {
            damage >>= 2;
        }
        P_DamageMobj(thing, bombspot, bombsource, damage);
    }
    return (true);
}

/*
   =================
   =
   = P_RadiusAttack
   =
   = Source is the creature that caused the explosion at spot
   =================
 */

void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance,
                    boolean damageSource)
{
    int     x, y, xl, xh, yl, yh;
    fixed_t dist;

    dist = (distance + MAXRADIUS) << FRACBITS;

    P_PointToBlock(spot->pos[VX] - dist, spot->pos[VY] - dist, &xl, &yl);
    P_PointToBlock(spot->pos[VX] + dist, spot->pos[VY] + dist, &xh, &yh);

    bombspot = spot;
    bombsource = source;
    bombdamage = damage;
    bombdistance = distance;
    DamageSource = damageSource;
    for(y = yl; y <= yh; y++)
    {
        for(x = xl; x <= xh; x++)
        {
            P_BlockThingsIterator(x, y, PIT_RadiusAttack, 0);
        }
    }
}

/*
   ==============================================================================

   SECTOR HEIGHT CHANGING

   = After modifying a sectors floor or ceiling height, call this
   = routine to adjust the positions of all things that touch the
   = sector.
   =
   = If anything doesn't fit anymore, true will be returned.
   = If crunch is true, they will take damage as they are being crushed
   = If Crunch is false, you should set the sector height back the way it
   = was and call P_ChangeSector again to undo the changes
   ==============================================================================
 */

/*
   ===============
   =
   = PIT_ChangeSector
   =
   ===============
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
    if((thing->flags & MF_CORPSE) && (thing->health <= 0))
    {
        if(thing->flags & MF_NOBLOOD)
        {
            P_RemoveMobj(thing);
        }
        else
        {
            if(thing->state != &states[S_GIBS1])
            {
                P_SetMobjState(thing, S_GIBS1);
                thing->height = 0;
                thing->radius = 0;
                S_StartSound(SFX_PLAYER_FALLING_SPLAT, thing);
            }
        }
        return true;            // keep checking
    }

    // crunch dropped items
    if(thing->flags2 & MF2_DROPPED)
    {
        P_RemoveMobj(thing);
        return true;            // keep checking
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return true;            // assume it is bloody gibs or something

    nofit = true;
    if(crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, crushchange);
        // spray blood in a random direction
        if((!(thing->flags & MF_NOBLOOD)) &&
           (!(thing->flags2 & MF2_INVULNERABLE)))
        {
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY], thing->pos[VZ] + thing->height / 2,
                             MT_BLOOD);
            mo->momx = (P_Random() - P_Random()) << 12;
            mo->momy = (P_Random() - P_Random()) << 12;
        }
    }

    return true;                // keep checking (crush other things)
}

/*
   ===============
   =
   = P_ChangeSector
   =
   ===============
 */

//===========================================================================
// P_ChangeSector
//===========================================================================
boolean P_ChangeSector(sector_t *sector, int crunch)
{
    //int                     x,y;

    nofit = false;
    crushchange = crunch;

    // recheck heights for all things near the moving sector

    /*for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
       for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
       P_BlockThingsIterator (x, y, PIT_ChangeSector, 0); */

    P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

    return nofit;
}
