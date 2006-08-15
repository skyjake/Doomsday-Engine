/* $Id: p_start.c 3509 2006-08-09 15:06:59Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Compiles for jDoom/jHeretic/jHexen
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#if   __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_common.h"
#include "dmu_lib.h"
#include "p_spechit.h"

// MACROS ------------------------------------------------------------------

#if __JHEXEN__
#define USE_PUZZLE_ITEM_SPECIAL 129
#endif

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

#if __JHERETIC__
static void     CheckMissileImpact(mobj_t *mobj);
#elif __JHEXEN__
static void     P_FakeZMovement(mobj_t *mo);
static void     CheckForPushSpecial(line_t *line, int side, mobj_t *mobj);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t tmbbox[4];
mobj_t *tmthing;

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
int     tmfloorpic;

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

mobj_t *linetarget; // who got hit (or NULL)

fixed_t attackrange;

#if __JHEXEN__
mobj_t *PuffSpawned;
mobj_t *BlockingMobj;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static fixed_t tm[3];
static fixed_t tmheight;
static line_t *tmhitline;
static fixed_t tmdropoffz;
static fixed_t bestslidefrac, secondslidefrac;
static line_t *bestslideline, *secondslideline;

static mobj_t *slidemo;

static fixed_t tmxmove, tmymove;
static mobj_t *shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
static fixed_t shootz;

static int     la_damage;
static fixed_t aimslope;

// slopes to top and bottom of target
static fixed_t topslope, bottomslope;

static mobj_t *usething;

static mobj_t *bombsource, *bombspot;
static int     bombdamage;

static boolean crushchange;
static boolean nofit;

static fixed_t startPos[3]; // start position for trajectory line checks
static fixed_t endPos[3]; // end position for trajectory checks

#if __JHEXEN__
static mobj_t *tsthing;
static int     bombdistance;
static boolean DamageSource;
static mobj_t *onmobj; // generic global onmobj...used for landing on pods/players

static mobj_t *PuzzleItemUser;
static int PuzzleItemType;
static boolean PuzzleActivated;
#endif

#if !__JHEXEN__
static int tmunstuck; // $unstuck: used to check unsticking
#endif

// CODE --------------------------------------------------------------------

static boolean PIT_StompThing(mobj_t *mo, void *data)
{
    int stompAnyway;
    fixed_t blockdist;

    if(!(mo->flags & MF_SHOOTABLE))
        return true;

    blockdist = mo->radius + tmthing->radius;
    if(abs(mo->pos[VX] - tm[VX]) >= blockdist ||
       abs(mo->pos[VY] - tm[VY]) >= blockdist)
        return true; // didn't hit it

    if(mo == tmthing)
        return true; // Don't clip against self

    stompAnyway = *(int*) data;

    // Should we stomp anyway? unless self
    if(mo != tmthing && stompAnyway)
    {
        P_DamageMobj2(mo, tmthing, tmthing, 10000, true);
        return true;
    }

#if __JDOOM__
    // monsters don't stomp things except on boss level
    if(!tmthing->player && gamemap != 30)
        return false;
#endif

    if(!(tmthing->flags2 & MF2_TELESTOMP))
        return false; // Not allowed to stomp things

    // Do stomp damage (unless self)
    if(mo != tmthing)
        P_DamageMobj2(mo, tmthing, tmthing, 10000, true);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean alwaysstomp)
{
    int     xl, xh, yl, yh, bx, by;
    int     stomping;
    subsector_t *newsubsec;

    // kill anything occupying the position
    tmthing = thing;

    stomping = alwaysstomp;

    tm[VX] = x;
    tm[VY] = y;

    tmbbox[BOXTOP]    = tm[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = tm[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = tm[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = tm[VX] - tmthing->radius;

    newsubsec = R_PointInSubsector(tm[VX], tm[VY]);

    // $unstuck: floorline used with tmunstuck
    ceilingline = NULL;
#if !__JHEXEN__
    blockline = floorline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;
#endif

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_TEXTURE);

    validCount++;
    P_EmptySpecHit();

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
#if !__JHEXEN__
    thing->dropoffz = tmdropoffz;   // killough $unstuck
#endif
    thing->pos[VX] = x;
    thing->pos[VY] = y;

    P_SetThingPosition(thing);
#if __JDOOM__
    P_ClearThingSRVO(thing); // Why just jDoom?
#endif
    return true;
}

/*
 * Checks to see if a start->end trajectory line crosses a blocking
 * line. Returns false if it does.
 *
 * tmbbox holds the bounding box of the trajectory. If that box
 * does not touch the bounding box of the line in question,
 * then the trajectory is not blocked. If the start is on one side
 * of the line and the end is on the other side, then the
 * trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite
 * correct. A more correct solution would be to check for an
 * intersection of the trajectory and the line, but that takes
 * longer and probably really isn't worth the effort.
 *
 * @param data      Unused
 */
static boolean PIT_CrossLine(line_t* ld, void *data)
{
    if(!(P_GetIntp(ld, DMU_FLAGS) & ML_TWOSIDED) ||
      (P_GetIntp(ld, DMU_FLAGS) & (ML_BLOCKING | ML_BLOCKMONSTERS)))
    {
        fixed_t bbox[4];

        P_GetFixedpv(ld, DMU_BOUNDING_BOX, bbox);

        if(!(tmbbox[BOXLEFT] > bbox[BOXRIGHT] ||
             tmbbox[BOXRIGHT] < bbox[BOXLEFT] ||
             tmbbox[BOXTOP] < bbox[BOXBOTTOM] ||
             tmbbox[BOXBOTTOM] > bbox[BOXTOP]))
        {
            if(P_PointOnLineSide(startPos[VX], startPos[VY], ld) !=
               P_PointOnLineSide(endPos[VX], endPos[VY], ld))
                // line blocks trajectory
                return false;
        }
    }

    // line doesn't block trajectory
    return true;
}

/*
 * This routine checks for Lost Souls trying to be spawned
 * across 1-sided lines, impassible lines, or "monsters can't
 * cross" lines. Draw an imaginary line between the PE
 * and the new Lost Soul spawn spot. If that line crosses
 * a 'blocking' line, then disallow the spawn. Only search
 * lines in the blocks of the blockmap where the bounding box
 * of the trajectory line resides. Then check bounding box
 * of the trajectory vs. the bounding box of each blocking
 * line to see if the trajectory and the blocking line cross.
 * Then check the PE and LS to see if they're on different
 * sides of the blocking line. If so, return true, otherwise
 * false.
 */
boolean P_CheckSides(mobj_t* actor, int x, int y)
{
    int bx, by, xl, xh, yl, yh;

    memcpy(startPos, actor->pos, sizeof(startPos));

    endPos[VX] = x;
    endPos[VY] = y;
    endPos[VZ] = DDMININT; // just initialize with *something*

    // Here is the bounding box of the trajectory

    tmbbox[BOXLEFT]   = startPos[VX] < endPos[VX] ? startPos[VX] : endPos[VX];
    tmbbox[BOXRIGHT]  = startPos[VX] > endPos[VX] ? startPos[VX] : endPos[VX];
    tmbbox[BOXTOP]    = startPos[VY] > endPos[VY] ? startPos[VY] : endPos[VY];
    tmbbox[BOXBOTTOM] = startPos[VY] < endPos[VY] ? startPos[VY] : endPos[VY];

    // Determine which blocks to look in for blocking lines
    P_PointToBlock(tmbbox[BOXLEFT], tmbbox[BOXBOTTOM], &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT], tmbbox[BOXTOP], &xh, &yh);

    // xl->xh, yl->yh determine the mapblock set to search

    for(bx = xl ; bx <= xh ; bx++)
        for(by = yl ; by <= yh ; by++)
            if(!P_BlockLinesIterator(bx,by,PIT_CrossLine, 0))
                return true;

    return false;
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

static boolean PIT_CheckThing(mobj_t *thing, void *data)
{
    fixed_t blockdist;
    boolean solid;
#if !__JHEXEN__
    boolean overlap = false;
#endif
    int     damage;

    // don't clip against self
    if(thing == tmthing)
        return true;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_IsCamera(thing) || P_IsCamera(tmthing))
        return true;

#if !__JHEXEN__
    // Player only
    if(tmthing->player && tm[VZ] != DDMAXINT &&
       (cfg.moveCheckZ || (tmthing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->pos[VZ] > tm[VZ] + tmheight) ||
           (thing->pos[VZ] + thing->height < tm[VZ]))
            return true; // under or over it

        overlap = true;
    }
#endif

    blockdist = thing->radius + tmthing->radius;
    if(abs(thing->pos[VX] - tm[VX]) >= blockdist ||
       abs(thing->pos[VY] - tm[VY]) >= blockdist)
        return true; // Didn't hit thing

#if __JHEXEN__
    // Stop here if we are a client.
    if(IS_CLIENT)
        return false;
#endif

#if !__JHEXEN__
    if(!tmthing->player && (tmthing->flags2 & MF2_PASSMOBJ))
#else
    BlockingMobj = thing;
    if(tmthing->flags2 & MF2_PASSMOBJ)
#endif
    {   // check if a mobj passed over/under another object
#if __JHERETIC__
        if((tmthing->type == MT_IMP || tmthing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            return false; // don't let imps/wizards fly over other imps/wizards
        }
#elif __JHEXEN__
        if(tmthing->type == MT_BISHOP && thing->type == MT_BISHOP)
            return false; // don't let bishops fly over other bishops
#endif
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height &&
           !(thing->flags & MF_SPECIAL))
        {
            return true;
        }
        else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ] &&
                !(thing->flags & MF_SPECIAL))
        {
            return true; // under thing
        }
    }

    // check for skulls slamming into things
    if(tmthing->flags & MF_SKULLFLY)
    {
#if __JHEXEN__
        if(tmthing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures
            if(!(thing->flags & MF_COUNTKILL))
            {
                return false;
            }
        }
        else if(tmthing->type == MT_HOLY_FX)
        {
            if(thing->flags & MF_SHOOTABLE && thing != tmthing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                    return true; // don't attack other co-op players

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
                    tmthing->tracer = NULL;
            }
            return true;
        }
#endif
        if(tmthing->damage == DDMAXINT) // Kludge to support old save games.
            damage = tmthing->info->damage;
        else
            damage = tmthing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;

        P_SetMobjState(tmthing, tmthing->info->spawnstate);

        return false;           // stop moving
    }

#if __JHEXEN__
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

            return false;
        }
    }
#endif

    // missiles can hit other things
    if(tmthing->flags & MF_MISSILE)
    {
#if __JHEXEN__
        // Check for a non-shootable mobj
        if(thing->flags2 & MF2_NONSHOOTABLE)
            return true;
#else
        // Check for passing through a ghost
        if((thing->flags & MF_SHADOW) && (tmthing->flags2 & MF2_THRUGHOST))
            return true;
#endif

        // see if it went over / under
        if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
            return true;        // overhead

        if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
            return true;        // underneath

#if __JHEXEN__
        if(tmthing->flags2 & MF2_FLOORBOUNCE)
        {
            if(tmthing->target == thing || !(thing->flags & MF_SOLID))
                return true;
            else
                return false;
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
                    if(tmthing->lastenemy &&
                       !tmthing->lastenemy->tracer)
                    {
                        tmthing->lastenemy->tracer = thing;
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
                lmo = tmthing->lastenemy;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->lastenemy &&
                           !lmo->lastenemy->tracer)
                        {
                            lmo->lastenemy->tracer = thing;
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
#endif

        // Don't hit same species as originator.
#if __JDOOM__
        if(tmthing->target &&
           (tmthing->target->type == thing->type ||
           (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
           (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
#else
        if(tmthing->target && tmthing->target->type == thing->type)
#endif
        {
            if(thing == tmthing->target)
                return true;

#if __JHEXEN__
            if(!thing->player)
                return false; // Hit same species as originator, explode, no damage
#else
            if(!monsterinfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return false;
            }
#endif
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            return !(thing->flags & MF_SOLID); // didn't do any damage
        }

        if(tmthing->flags2 & MF2_RIP)
        {
#if __JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
#else
            if(!(thing->flags & MF_NOBLOOD))
#endif
            {                   // Ok to spawn some blood
                P_RipperBlood(tmthing);
            }
#if __JHERETIC__
            S_StartSound(sfx_ripslop, tmthing);
#endif

            if(tmthing->damage == DDMAXINT) // Kludge to support old save games.
                damage = tmthing->info->damage;
            else
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
            P_EmptySpecHit();
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
#if __JDOOM__
        P_DamageMobj(thing, tmthing, tmthing->target, damage);
#else
        if(damage)
        {
# if __JHERETIC__
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
# else //__JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(tmthing->type == MT_TELOTHER_FX1) &&
               !(tmthing->type == MT_TELOTHER_FX2) &&
               !(tmthing->type == MT_TELOTHER_FX3) &&
               !(tmthing->type == MT_TELOTHER_FX4) &&
               !(tmthing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
# endif
                P_BloodSplatter(tmthing->pos[VX], tmthing->pos[VY], tmthing->pos[VZ], thing);

            P_DamageMobj(thing, tmthing, tmthing->target, damage);
        }
#endif
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

    // check for special pickup
    if(thing->flags & MF_SPECIAL)
    {
        solid = thing->flags & MF_SOLID;
        if(tmthing->flags & MF_PICKUP)
        {
            // can remove thing
            P_TouchSpecialThing(thing, tmthing);
        }
        return (!solid);
    }

#if !__JHEXEN__
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
#endif

    return !(thing->flags & MF_SOLID);
}

/*
 * Adjusts tmfloorz and tmceilingz as lines are contacted
 */
static boolean PIT_CheckLine(line_t *ld, void *data)
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
#if !__JHEXEN__
    tmthing->wallhit = true;

    // A Hit event will be sent to special lines.
    if(P_XLine(ld)->special)
        tmhitline = ld;
#endif

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
#if __JHEXEN__
        if(tmthing->flags2 & MF2_BLASTED)
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        CheckForPushSpecial(ld, 0, tmthing);
        return false;
#else
        fixed_t dx = P_GetFixedp(ld, DMU_DX);
        fixed_t dy = P_GetFixedp(ld, DMU_DY);

        blockline = ld;
        return tmunstuck && !untouched(ld) &&
            FixedMul(tm[VX] - tmthing->pos[VX], dy) > FixedMul(tm[VY] - tmthing->pos[VY], dx);
#endif
    }

#if __JHERETIC__
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {                           // One sided line
        if(tmthing->flags & MF_MISSILE)
        {                       // Missiles can trigger impact specials
            if(P_XLine(ld)->special)
                P_AddLineToSpecHit(ld);
        }
        return false;
    }
#endif

    if(!(tmthing->flags & MF_MISSILE))
    {
        // explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKING)
        {
#if __JHEXEN__
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
            CheckForPushSpecial(ld, 0, tmthing);
            return false;
#else
            // killough $unstuck: allow escape
            return tmunstuck && !untouched(ld);
#endif
        }

        // Block monsters only?
#if __JHEXEN__
        if(!tmthing->player && tmthing->type != MT_CAMERA &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#elif __JHERETIC__
        if(!tmthing->player && tmthing->type != MT_POD &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#else
        if(!tmthing->player &&
           (P_GetIntp(ld, DMU_FLAGS) & ML_BLOCKMONSTERS))
#endif
        {
#if __JHEXEN__
            if(tmthing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
#endif
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
#if !__JHEXEN__
        blockline = ld;
#endif
    }

    if(openbottom > tmfloorz)
    {
        tmfloorz = openbottom;
#if !__JHEXEN__
        // killough $unstuck: remember floor linedef
        floorline = ld;
        blockline = ld;
#endif
    }

    if(lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    // if contacted a special line, add it to the list
    if(P_XLine(ld)->special)
        P_AddLineToSpecHit(ld);
#if !__JHEXEN__
    tmthing->wallhit = false;
#endif
    return true;
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

#if !__JHEXEN__
    thing->onmobj = NULL;
    thing->wallhit = false;

    tmhitline = NULL;
    tmheight = thing->height;
#endif
    tm[VX] = x;
    tm[VY] = y;
    tm[VZ] = z;

    tmbbox[BOXTOP]    = tm[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = tm[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = tm[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = tm[VX] - tmthing->radius;

    newsec = P_GetPtrp(R_PointInSubsector(tm[VX], tm[VY]), DMU_SECTOR);

    // $unstuck: floorline used with tmunstuck
    ceilingline = NULL;
#if !__JHEXEN__
    blockline = floorline = NULL;

    // $unstuck
    tmunstuck = thing->dplayer && thing->dplayer->mo == thing;
#endif
    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them
    tmfloorz = tmdropoffz = P_GetFixedp(newsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsec, DMU_FLOOR_TEXTURE);

    validCount++;
    P_EmptySpecHit();

#if __JHEXEN__
    if((tmthing->flags & MF_NOCLIP) && !(tmthing->flags & MF_SKULLFLY))
        return true;
#else
    if((tmthing->flags & MF_NOCLIP))
        return true;
#endif

    // check things first, possibly picking things up
    // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
    // into mapblocks based on their origin point, and can overlap into adjacent
    // blocks by up to MAXRADIUS units
    P_PointToBlock(tmbbox[BOXLEFT] - MAXRADIUS, tmbbox[BOXBOTTOM] - MAXRADIUS,
                   &xl, &yl);
    P_PointToBlock(tmbbox[BOXRIGHT] + MAXRADIUS, tmbbox[BOXTOP] + MAXRADIUS,
                   &xh, &yh);

    // The camera goes through all objects -- jk
#if __JHEXEN__
    if(thing->type != MT_CAMERA)
    {
        BlockingMobj = NULL;
#endif
        for(bx = xl; bx <= xh; bx++)
            for(by = yl; by <= yh; by++)
                if(!P_BlockThingsIterator(bx, by, PIT_CheckThing, 0))
                    return false;
#if __JHEXEN__
    }
#endif
     // check lines
#if __JHEXEN__
    if(tmthing->flags & MF_NOCLIP)
        return true;

    BlockingMobj = NULL;
#endif

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
 * Attempt to move to a new position, crossing special lines
 * unless MF_TELEPORT is set.
 *
 * killough $dropoff_fix
 */
#if __JHEXEN__
static boolean P_TryMove2(mobj_t *thing, fixed_t x, fixed_t y)
#else
static boolean P_TryMove2(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff)
#endif
{
    fixed_t oldpos[3];
    int     side, oldside;
    line_t *ld;

    // $dropoff_fix: felldown
    floatok = false;
#if !__JHEXEN__
    felldown = false;
#endif
#if __JHEXEN__
    if(!P_CheckPosition(thing, x, y))
#else
    if(!P_CheckPosition2(thing, x, y, thing->pos[VZ]))
#endif
    {
#if __JHEXEN__
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
#else
# if __JHERETIC__
        CheckMissileImpact(thing);
# endif
        // Would we hit another thing or a solid wall?
        if(!thing->onmobj || thing->wallhit)
            return false;
#endif
    }

    // GMJ 02/02/02
    if(!(thing->flags & MF_NOCLIP))
    {
#if __JHEXEN__
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
#else
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
# if __JHERETIC__
            thing->type != MT_MNTRFX2 && // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
# endif
            tmfloorz - thing->pos[VZ] > 24 * FRACUNIT))
        {
# if __JHERETIC__
            CheckMissileImpact(thing);
# endif
            return tmunstuck && !(ceilingline && untouched(ceilingline)) &&
                !(floorline && untouched(floorline));
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && tmfloorz > thing->pos[VZ])
        {
            CheckMissileImpact(thing);
        }
# endif
#endif
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->pos[VZ] + thing->height > tmceilingz)
            {
                thing->momz = -8 * FRACUNIT;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
            else if(thing->pos[VZ] < tmfloorz &&
                    tmfloorz - tmdropoffz > 24 * FRACUNIT)
            {
                thing->momz = 8 * FRACUNIT;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
        }

#if __JHEXEN__
        if(!(thing->flags & MF_TELEPORT)
           // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
           && thing->type != MT_MNTRFX2 && thing->type != MT_LIGHTNING_FLOOR &&
           tmfloorz - thing->pos[VZ] > 24 * FRACUNIT)
        {
            goto pushline;
        }
#endif

#if __JHEXEN__
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (tmfloorz - tmdropoffz > 24 * FRACUNIT) &&
           !(thing->flags2 & MF2_BLASTED))
        {                       // Can't move over a dropoff unless it's been blasted
            return false;
        }
#else
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
#endif

#if __JHEXEN__
        // must stay within a sector of a certain floor type?
        if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
           (tmfloorpic != P_GetIntp(thing->subsector, DMU_FLOOR_TEXTURE) ||
            tmfloorz - thing->pos[VZ] != 0))
        {
            return false;
        }
#endif

#if !__JHEXEN__
        // killough $dropoff: prevent falling objects from going up too many steps
        if(!thing->player && (thing->intflags & MIF_FALLING) &&
           tmfloorz - thing->pos[VZ] > FixedMul(thing->momx, thing->momx) +
                                       FixedMul(thing->momy, thing->momy))
        {
            return false;
        }
#endif
    }

    // the move is ok, so link the thing into its new position
    P_UnsetThingPosition(thing);

    memcpy(oldpos, thing->pos, sizeof(oldpos));
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
#if __JHEXEN__
    thing->floorpic = tmfloorpic;
#else
    // killough $dropoff_fix: keep track of dropoffs
    thing->dropoffz = tmdropoffz;
#endif
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
        while((ld = P_PopSpecHit()) != NULL)
        {
            // see if the line was crossed
            if(P_XLine(ld)->special)
            {
                side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
                oldside = P_PointOnLineSide(oldpos[VX], oldpos[VY], ld);
                if(side != oldside)
                {
#if __JHEXEN__
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
#else
                    P_ActivateLine(ld, thing, oldside, SPAC_CROSS);
#endif
                }
            }
        }
    }

    return true;

#if __JHEXEN__
  pushline:
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        if(tmthing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmthing, NULL, NULL, tmthing->info->mass >> 5);
        }

        P_SpecHitResetIterator();
        while((ld = P_SpecHitIterator()) != NULL)
        {
            // see if the line was crossed
            side = P_PointOnLineSide(thing->pos[VX], thing->pos[VY], ld);
            CheckForPushSpecial(ld, side, thing);
        }
    }
    return false;
#endif
}

#if __JHEXEN__
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y)
#else
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, boolean dropoff, boolean slide)
#endif
{
#if __JHEXEN__
    return P_TryMove2(thing, x, y);
#else
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
#endif
}

/*
 * FIXME: DJS - This routine has gotten way too big, split if(in->isaline)
 *        to a seperate routine?
 */
static boolean PTR_ShootTraverse(intercept_t * in)
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
#if __JHEXEN__
    extern mobj_t LavaInflictor;
#endif
    xline_t *xline;

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
        xline = P_XLine(li);
        if(xline->special)
            P_ActivateLine(li, shootthing, 0, SPAC_IMPACT);

        if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
            goto hitline;

        // crosses a two sided line
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

        // This is the subsector where the trace originates.
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
                while((d[VZ] > 0 && pos[VZ] <= ctop) ||
                      (d[VZ] < 0 && pos[VZ] >= cbottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootthing);
        }
#endif
        // don't go any farther
        return false;
    }
    // shoot a thing
    th = in->d.thing;
    if(th == shootthing)
        return true;            // can't shoot self

    if(!(th->flags & MF_SHOOTABLE))
        return true;            // corpse or something

#if __JHERETIC__
    // check for physical attacks on a ghost
    if((th->flags & MF_SHADOW) && shootthing->player->readyweapon == WP_FIRST)
        return true;
#endif

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
#if __JHERETIC__
    if(PuffType == MT_BLASTERPUFF1)
    {
        // Make blaster big puff
        mobj_t *mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_BLASTERPUFF2);
        S_StartSound(sfx_blshit, mo);
    }
    else
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#elif __JHEXEN__
    P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#endif

    if(la_damage)
    {
#if __JHEXEN__
        if(!(in->d.thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(in->d.thing->flags & MF_NOBLOOD))
            {
#if __JDOOM__
                P_SpawnBlood(pos[VX], pos[VY], pos[VZ], la_damage);
#elif __JHEXEN__
                if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
                {
                    P_BloodSplatter2(pos[VX], pos[VY], pos[VZ], in->d.thing);
                }
#else
                if(P_Random() < 192)
                    P_BloodSplatter(pos[VX], pos[VY], pos[VZ], in->d.thing);
#endif
            }
#if __JDOOM__
            else
                P_SpawnPuff(pos[VX], pos[VY], pos[VZ]);
#endif
        }

#if __JHEXEN__
        if(PuffType == MT_FLAMEPUFF2)
        {                       // Cleric FlameStrike does fire damage
            P_DamageMobj(th, &LavaInflictor, shootthing, la_damage);
        }
        else
#endif
        {
            P_DamageMobj(th, shootthing, shootthing, la_damage);
        }
    }

    // don't go any farther
    return false;
}

/*
 * Sets linetaget and aimslope when a target is aimed at.
 */
static boolean PTR_AimTraverse(intercept_t *in)
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

#if __JHERETIC__
    if(th->type == MT_POD)
        return true; // Can't auto-aim at pods
#endif

#if __JDOOM__ || __JHEXEN__
    if(th->player && IS_NETGAME && !deathmatch)
        return true; // don't aim at fellow co-op players
#endif

    // check angles to see if the thing can be aimed at
    dist = FixedMul(attackrange, in->frac);

    thingtopslope = FixedDiv(th->pos[VZ] + th->height - shootz, dist);
    if(thingtopslope < bottomslope)
        return true;            // shot over the thing

    // Too far below?
// $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->pos[VZ] + th->height < shootz - FixedDiv(attackrange, 1.2 * FRACUNIT))
        return true;
#endif

    thingbottomslope = FixedDiv(th->pos[VZ] - shootz, dist);
    if(thingbottomslope > topslope)
        return true;            // shot under the thing

    // Too far above?
// $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->pos[VZ] > shootz + FixedDiv(attackrange, 1.2 * FRACUNIT))
        return true;
#endif

    // this thing can be hit!
    if(thingtopslope > topslope)
        thingtopslope = topslope;

    if(thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;

    return false;               // don't go any farther
}

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
    fixed_t x2, y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->pos[VX] + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->pos[VY] + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->pos[VZ] + (t1->height >> 1) + 8 * FRACUNIT;

#if __JDOOM__
    topslope = 60 * FRACUNIT;
    bottomslope = -topslope;
#else
    topslope = 100 * FRACUNIT;
    bottomslope = -100 * FRACUNIT;
#endif

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->pos[VX], t1->pos[VY], x2, y2, PT_ADDLINES | PT_ADDTHINGS,
                   PTR_AimTraverse);

    if(linetarget)
    {   // While autoaiming, we accept this slope.
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
    fixed_t x2, y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;

    x2 = t1->pos[VX] + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->pos[VY] + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->pos[VZ] + (t1->height >> 1) + 8 * FRACUNIT;

#if __JHEXEN__
    if(t1->player &&
      (t1->player->class == PCLASS_FIGHTER ||
       t1->player->class == PCLASS_CLERIC ||
       t1->player->class == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        shootz = t1->pos[VZ] + (cfg.plrViewHeight - 5) * FRACUNIT;
    }

    shootz -= t1->floorclip;
    attackrange = distance;
    aimslope = slope;

    if(P_PathTraverse(t1->pos[VX], t1->pos[VY], x2, y2,
                      PT_ADDLINES | PT_ADDTHINGS, PTR_ShootTraverse))
    {
#if __JHEXEN__
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
#endif
    }
}

/*
 * "bombsource" is the creature that caused the explosion at "bombspot".
 */
static boolean PIT_RadiusAttack(mobj_t *thing, void *data)
{
    fixed_t dx, dy, dz, dist;

    if(!(thing->flags & MF_SHOOTABLE))
        return true;

    // Boss spider and cyborg
    // take no damage from concussion.
#if __JHERETIC__
    if(thing->type == MT_MINOTAUR || thing->type == MT_SORCERER1 ||
       thing->type == MT_SORCERER2)
        return true;
#elif __JDOOM__
    if(thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;
#else
    if(!DamageSource && thing == bombsource) // don't damage the source of the explosion
        return true;
#endif

    dx = abs(thing->pos[VX] - bombspot->pos[VX]);
    dy = abs(thing->pos[VY] - bombspot->pos[VY]);
    dz = abs(thing->pos[VZ] - bombspot->pos[VZ]);

    dist = dx > dy ? dx : dy;
#if __JHEXEN__
    if(!cfg.netNoMaxZRadiusAttack)
        dist = dz > dist ? dz : dist;
#else
    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
        dist = dz > dist ? dz : dist;
#endif

    dist = (dist - thing->radius) >> FRACBITS;

    if(dist < 0)
        dist = 0;

    if(dist >= bombdamage)
        return true;            // out of range

    if(P_CheckSight(thing, bombspot))
    {
        int     damage;
#if __JHEXEN__
        damage = (bombdamage * (bombdistance - dist) / bombdistance) + 1;
        if(thing->player)
        {
            damage >>= 2;
        }
#else
        damage = bombdamage - dist;
#endif
        // must be in direct path
        P_DamageMobj(thing, bombspot, bombsource, damage);
    }

    return true;
}

/*
 * Source is the creature that caused the explosion at spot.
 */
#if __JHEXEN__
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance,
                    boolean damageSource)
#else
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
#endif
{
    int     x, y, xl, xh, yl, yh;
    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    P_PointToBlock(spot->pos[VX] - dist, spot->pos[VY] - dist, &xl, &yl);
    P_PointToBlock(spot->pos[VX] + dist, spot->pos[VY] + dist, &xh, &yh);

    bombspot = spot;
    bombdamage = damage;
#if __JHERETIC__
    if(spot->type == MT_POD && spot->target)
        bombsource = spot->target;
    else
#endif
        bombsource = source;

#if __JHEXEN__
    bombdistance = distance;
    DamageSource = damageSource;
#endif

    for(y = yl; y <= yh; y++)
        for(x = xl; x <= xh; x++)
            P_BlockThingsIterator(x, y, PIT_RadiusAttack, 0);
}

static boolean PTR_UseTraverse(intercept_t * in)
{
    int     side;

    if(!P_XLine(in->d.line)->special)
    {
        P_LineOpening(in->d.line);
        if(openrange <= 0)
        {
            if(usething->player)
                S_StartSound(PCLASS_INFO(usething->player->class)->failUseSound,
                             usething);

            return false; // can't use through a wall
        }

#if __JHEXEN__
        if(usething->player)
        {
            fixed_t pheight = usething->pos[VZ] + (usething->height / 2);

            if((opentop < pheight) || (openbottom > pheight))
                S_StartSound(PCLASS_INFO(usething->player->class)->failUseSound,
                             usething);
        }
#endif
        // not a special line, but keep checking
        return true;
    }

    side = 0;
    if(P_PointOnLineSide(usething->pos[VX], usething->pos[VY], in->d.line) == 1)
        side = 1;

#if !__JDOOM__
    if(side == 1)
        return false;       // don't use back side
#endif


    P_ActivateLine(in->d.line, usething, side, SPAC_USE);

#if !__JHEXEN__
    // can use multiple line specials in a row with the PassThru flag
    if(P_GetIntp(in->d.line, DMU_FLAGS) & ML_PASSUSE)
        return true;
#endif
    // can't use more than one special line in a row
    return false;
}

/*
 * Looks for special lines in front of the player to activate.
 *
 * @param player        The player to test.
 */
void P_UseLines(player_t *player)
{
    int     angle;
    fixed_t pos1[3], pos2[3];

    usething = player->plr->mo;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos1, player->plr->mo->pos, sizeof(pos1));
    memcpy(pos2, player->plr->mo->pos, sizeof(pos2));

    pos2[VX] += (USERANGE >> FRACBITS) * finecosine[angle];
    pos2[VY] += (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(pos1[VX], pos1[VY], pos2[VX], pos2[VY],
                   PT_ADDLINES, PTR_UseTraverse);
}

/*
 * Takes a valid thing and adjusts the thing->floorz, thing->ceilingz,
 * and possibly thing->pos[VZ].
 *
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 *
 * @param thing         The mobj whoose position to adjust.
 * @return boolean      (True) if the thing did fit.
 */
static boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean onfloor;

    if(P_IsCamera(thing))
        return false; // Don't height clip cameras.

    onfloor = (thing->pos[VZ] == thing->floorz);
    P_CheckPosition2(thing, thing->pos[VX], thing->pos[VY], thing->pos[VZ]);

    // what about stranding a monster partially off an edge?

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
#if __JHEXEN__
    thing->floorpic = tmfloorpic;
#else
    // killough $dropoff_fix: remember dropoffs
    thing->dropoffz = tmdropoffz;
#endif

    if(onfloor)
    {
#if __JHEXEN__
        if((thing->pos[VZ] - thing->floorz < 9 * FRACUNIT) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->pos[VZ] = thing->floorz;
        }
#else
        // walking monsters rise and fall with the floor
        thing->pos[VZ] = thing->floorz;

        // killough $dropoff_fix:
        // Possibly upset balance of objects hanging off ledges
        if(thing->intflags & MIF_FALLING && thing->gear >= MAXGEAR)
            thing->gear = 0;
#endif
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
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param ld            The line being slid along.
 */
static void P_HitSlideLine(line_t *ld)
{
    int     side;
    angle_t lineangle, moveangle, deltaangle;
    fixed_t movelen, newlen;

    if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }
    else if(P_GetIntp(ld, DMU_SLOPE_TYPE) == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], ld);

    lineangle =
        R_PointToAngle2(0, 0, P_GetFixedp(ld, DMU_DX), P_GetFixedp(ld, DMU_DY));

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;

    if(deltaangle > ANG180)
        deltaangle += ANG180;

    lineangle  >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

static boolean PTR_SlideTraverse(intercept_t * in)
{
    line_t *li;

    if(!in->isaline)
        Con_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;
    if(!(P_GetIntp(li, DMU_FLAGS) & ML_TWOSIDED))
    {
        if(P_PointOnLineSide(slidemo->pos[VX], slidemo->pos[VY], li))
            return true; // don't hit the back side

        goto isblocking;
    }

    P_LineOpening(li); // set openrange, opentop, openbottom

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
 * The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * FIXME: This is a kludgy mess.
 *
 * @param mo            The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t *mo)
{
    int     hitcount;
    fixed_t leadpos[3], trailpos[3], newPos[3];

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

    P_PathTraverse(leadpos[VX], leadpos[VY],
                   leadpos[VX] + mo->momx, leadpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailpos[VX], leadpos[VY],
                   trailpos[VX] + mo->momx, leadpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadpos[VX], trailpos[VY],
                   leadpos[VX] + mo->momx, trailpos[VY] + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);

    // move up to the wall
    if(bestslidefrac == FRACUNIT + 1)
    {
        // the move must have hit the middle, so stairstep
      stairstep:
        // killough $dropoff_fix
#if __JHEXEN__
        if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->momy))
            P_TryMove(mo, mo->pos[VX] + mo->momx, mo->pos[VY]);
#else
        if(!P_TryMove(mo, mo->pos[VX], mo->pos[VY] + mo->momy, true, true))
            P_TryMove(mo, mo->pos[VX] + mo->momx, mo->pos[VY], true, true);
#endif
            return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;
    if(bestslidefrac > 0)
    {
        newPos[VX] = FixedMul(mo->momx, bestslidefrac);
        newPos[VY] = FixedMul(mo->momy, bestslidefrac);
        newPos[VZ] = DDMAXINT; // just initialize with *something*

        // killough $dropoff_fix
#if __JHEXEN__
        if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY]))
#else
        if(!P_TryMove(mo, mo->pos[VX] + newPos[VX], mo->pos[VY] + newPos[VY], true, true))
#endif
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
#if __JHEXEN__
    if(!P_TryMove(mo, mo->pos[VX] + tmxmove, mo->pos[VY] + tmymove))
#else
    if(!P_TryMove(mo, mo->pos[VX] + tmxmove, mo->pos[VY] + tmymove, true, true))
#endif
    {
        goto retry;
    }
}

/*
 * SECTOR HEIGHT CHANGING
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed.
 * If Crunch is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes.
 */

/*
 * @param thing         The thing to check against height changes.
 * @param data          Unused.
 */
static boolean PIT_ChangeSector(mobj_t *thing, void *data)
{
    mobj_t *mo;

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->flags & MF_NOBLOCKMAP)
        return true;

    if(P_ThingHeightClip(thing))
        return true; // keep checking

    // crunch bodies to giblets
#if __JDOOM__
    if(thing->health <= 0 && !(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
    if(thing->health <= 0 && (thing->flags & MF_CORPSE))
#else
    if(thing->health <= 0)
#endif
    {
#if __JHEXEN__
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
#else
# if __JDOOM__
        P_SetMobjState(thing, S_GIBS);
# endif
        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;
#endif
        return true; // keep checking
    }

    // crunch dropped items
#if __JHEXEN__
    if(thing->flags2 & MF2_DROPPED)
#else
    if(thing->flags & MF_DROPPED)
#endif
    {
        P_RemoveMobj(thing);
        return true; // keep checking
    }

    if(!(thing->flags & MF_SHOOTABLE))
        return true; // assume it is bloody gibs or something

    nofit = true;
    if(crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10);
#if __JDOOM__
        if(!(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
        if(!(thing->flags & MF_NOBLOOD) &&
           !(thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            // spray blood in a random direction
            mo = P_SpawnMobj(thing->pos[VX], thing->pos[VY],
                             thing->pos[VZ] + thing->height / 2, MT_BLOOD);

            mo->momx = (P_Random() - P_Random()) << 12;
            mo->momy = (P_Random() - P_Random()) << 12;
        }
    }

    // keep checking (crush other things)
    return true;
}

/*
 * @param sector        The sector to check.
 * @param crunch        (True) = crush any things in the sector.
 */
boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
    nofit = false;
    crushchange = crunch;

    validCount++;
    P_SectorTouchingThingsIterator(sector, PIT_ChangeSector, 0);

    return nofit;
}

/*
 * The following routines originate from the Heretic src.
 */

#if __JHERETIC__ || __JHEXEN__
/*
 * @param mobj          The mobj whoose position to test.
 * @return boolean      (True) if the mobj is not blocked by anything.
 */
boolean P_TestMobjLocation(mobj_t *mobj)
{
    int     flags;

    flags = mobj->flags;
    mobj->flags &= ~MF_PICKUP;

    if(P_CheckPosition(mobj, mobj->pos[VX], mobj->pos[VY]))
    {
        // XY is ok, now check Z
        mobj->flags = flags;
        if((mobj->pos[VZ] < mobj->floorz) ||
           (mobj->pos[VZ] + mobj->height > mobj->ceilingz))
        {
            return false; // Bad Z
        }

        return true;
    }

    mobj->flags = flags;
    return false;
}
#endif

#if __JHERETIC__
static void CheckMissileImpact(mobj_t *mobj)
{
    line_t* ld;
    int     size = P_SpecHitSize();

    if(!size || !(mobj->flags & MF_MISSILE) || !mobj->target)
        return;

    if(!mobj->target->player)
        return;

    P_SpecHitResetIterator();
    while((ld = P_SpecHitIterator()) != NULL)
        P_ActivateLine(ld, mobj->target, 0, SPAC_IMPACT);
}
#endif

/*
 * The following routines originate from the Hexen src
 */

#if __JHEXEN__
static boolean PIT_ThrustStompThing(mobj_t *thing, void *data)
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
    fixed_t pos1[3], pos2[3];

    tsthing = actor;

    memcpy(pos1, actor->pos, sizeof(pos1));
    memcpy(pos2, actor->pos, sizeof(pos2));

    pos1[VX] -= actor->info->radius;
    pos2[VX] += actor->info->radius;
    pos1[VY] -= actor->info->radius;
    pos2[VY] += actor->info->radius;

    P_PointToBlock(pos1[VX] - MAXRADIUS, pos1[VY] - MAXRADIUS, &xl, &yl);
    P_PointToBlock(pos2[VX] + MAXRADIUS, pos2[VY] + MAXRADIUS, &xh, &yh);

    // stomp on any things contacted
    for(bx = xl; bx <= xh; bx++)
        for(by = yl; by <= yh; by++)
            P_BlockThingsIterator(bx, by, PIT_ThrustStompThing, 0);
}

static boolean PIT_CheckOnmobjZ(mobj_t *thing, void *data)
{
    fixed_t blockdist;

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return true; // Can't hit thing

    blockdist = thing->radius + tmthing->radius;
    if(abs(thing->pos[VX] - tm[VX]) >= blockdist ||
       abs(thing->pos[VY] - tm[VY]) >= blockdist)
        return true; // Didn't hit thing

    if(thing == tmthing)
        return true; // Don't clip against self

    if(tmthing->pos[VZ] > thing->pos[VZ] + thing->height)
        return true;
    else if(tmthing->pos[VZ] + tmthing->height < thing->pos[VZ])
        return true; // under thing

    if(thing->flags & MF_SOLID)
        onmobj = thing;

    return (!(thing->flags & MF_SOLID));
}

mobj_t *P_CheckOnmobj(mobj_t *thing)
{
    int     xl, xh, yl, yh, bx, by;
    subsector_t *newsubsec;
    fixed_t pos[3];
    mobj_t  oldmo;

    memcpy(pos, thing->pos, sizeof(pos));

    tmthing = thing;
    oldmo = *thing;             // save the old mobj before the fake zmovement

    P_FakeZMovement(tmthing);

    memcpy(tm, pos, sizeof(tm));

    tmbbox[BOXTOP]    = pos[VY] + tmthing->radius;
    tmbbox[BOXBOTTOM] = pos[VY] - tmthing->radius;
    tmbbox[BOXRIGHT]  = pos[VX] + tmthing->radius;
    tmbbox[BOXLEFT]   = pos[VX] - tmthing->radius;

    newsubsec = R_PointInSubsector(pos[VX], pos[VY]);
    ceilingline = NULL;

    // the base floor / ceiling is from the subsector that contains the
    // point.  Any contacted lines the step closer together will adjust them

    tmfloorz = tmdropoffz = P_GetFixedp(newsubsec, DMU_FLOOR_HEIGHT);
    tmceilingz = P_GetFixedp(newsubsec, DMU_CEILING_HEIGHT);
    tmfloorpic = P_GetIntp(newsubsec, DMU_FLOOR_TEXTURE);

    validCount++;
    P_EmptySpecHit();

    if(tmthing->flags & MF_NOCLIP)
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
}

/*
 * Fake the zmovement so that we can check if a move is legal
 */
static void P_FakeZMovement(mobj_t *mo)
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

    if(mo->player && (mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorz) &&
       (leveltime & 2))
    {
        mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // clip movement
    if(mo->pos[VZ] <= mo->floorz)  // Hit the floor
    {
        mo->pos[VZ] = mo->floorz;
        if(mo->momz < 0)
            mo->momz = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz; // The skull slammed into something

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

    if(mo->pos[VZ] + mo->height > mo->ceilingz) // hit the ceiling
    {
        mo->pos[VZ] = mo->ceilingz - mo->height;

        if(mo->momz > 0)
            mo->momz = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz; // the skull slammed into something
    }
}

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

static boolean PTR_BounceTraverse(intercept_t * in)
{
    line_t *li;

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

void P_BounceWall(mobj_t *mo)
{
    int     side;
    fixed_t  movelen, leadPos[3];
    angle_t lineangle, moveangle, deltaangle;

    slidemo = mo;

    // trace along the three leading corners
    memcpy(leadPos, mo->pos, sizeof(leadPos));
    if(mo->momx > 0)
        leadPos[VX] += mo->radius;
    else
        leadPos[VX] -= mo->radius;

    if(mo->momy > 0)
        leadPos[VY] += mo->radius;
    else
        leadPos[VY] -= mo->radius;

    bestslidefrac = FRACUNIT + 1;
    P_PathTraverse(leadPos[VX], leadPos[VY],
                   leadPos[VX] + mo->momx, leadPos[VY] + mo->momy,
                   PT_ADDLINES, PTR_BounceTraverse);

    if(!bestslideline)
        return; // We don't want to crash.

    side = P_PointOnLineSide(mo->pos[VX], mo->pos[VY], bestslideline);
    lineangle = R_PointToAngle2(0, 0,
                                P_GetFixedp(bestslideline, DMU_DX),
                                P_GetFixedp(bestslideline, DMU_DY));

    if(side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, mo->momx, mo->momy);
    deltaangle = (2 * lineangle) - moveangle;

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_ApproxDistance(mo->momx, mo->momy);
    movelen = FixedMul(movelen, 0.75 * FRACUNIT);   // friction

    if(movelen < FRACUNIT)
        movelen = 2 * FRACUNIT;

    mo->momx = FixedMul(movelen, finecosine[deltaangle]);
    mo->momy = FixedMul(movelen, finesine[deltaangle]);
}

static boolean PTR_PuzzleItemTraverse(intercept_t * in)
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
                return false; // can't use through a wall
            }

            return true; // Continue searching
        }

        if(P_PointOnLineSide(PuzzleItemUser->pos[VX],
                             PuzzleItemUser->pos[VY], in->d.line) == 1)
            return false; // Don't use back sides

        if(PuzzleItemType != P_XLine(in->d.line)->arg1)
            return false; // Item type doesn't match

        P_StartACS(P_XLine(in->d.line)->arg2, 0, &P_XLine(in->d.line)->arg3,
                   PuzzleItemUser, in->d.line, 0);
        P_XLine(in->d.line)->special = 0;
        PuzzleActivated = true;

        return false;           // Stop searching
    }

    // Check thing
    mobj = in->d.thing;
    if(mobj->special != USE_PUZZLE_ITEM_SPECIAL)
        return true; // Wrong special

    if(PuzzleItemType != mobj->args[0])
        return true; // Item type doesn't match

    P_StartACS(mobj->args[1], 0, &mobj->args[2], PuzzleItemUser, NULL, 0);
    mobj->special = 0;
    PuzzleActivated = true;
    return false;               // Stop searching
}

/*
 * See if the specified player can use the specified puzzle item on a
 * thing or line(s) at their current world location.
 *
 * @param player        The player using the puzzle item.
 * @param itemType      The type of item to try to use.
 * @return boolean      true if the puzzle item was used.
 */
boolean P_UsePuzzleItem(player_t *player, int itemType)
{
    int     angle;
    fixed_t pos1[3], pos2[3];

    PuzzleItemType = itemType;
    PuzzleItemUser = player->plr->mo;
    PuzzleActivated = false;

    angle = player->plr->mo->angle >> ANGLETOFINESHIFT;

    memcpy(pos1, player->plr->mo->pos, sizeof(pos1));
    memcpy(pos2, player->plr->mo->pos, sizeof(pos2));

    pos2[VX] += (USERANGE >> FRACBITS) * finecosine[angle];
    pos2[VY] += (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(pos1[VX], pos1[VY], pos2[VX], pos2[VY],
                   PT_ADDLINES | PT_ADDTHINGS, PTR_PuzzleItemTraverse);

    return PuzzleActivated;
}
#endif
