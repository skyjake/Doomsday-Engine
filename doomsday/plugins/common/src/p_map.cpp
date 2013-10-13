/** @file p_map.cpp Common map routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>
#include <cstdio>
#include <cstring>

#include "common.h"

#include "d_net.h"
#include "g_common.h"
#include "dmu_lib.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_player.h"
#include "p_mapsetup.h"

#include "p_map.h"

#if __JHEXEN__
#define USE_PUZZLE_ITEM_SPECIAL     129
#endif

#if __JDOOM64__ || __JHERETIC__
static void CheckMissileImpact(mobj_t *mobj);
#endif

#if __JHEXEN__
static void P_FakeZMovement(mobj_t *mo);
static void checkForPushSpecial(Line *line, int side, mobj_t *mobj);
#endif

AABoxd tmBox;
mobj_t *tmThing;

// If "floatOk" true, move would be ok if within "tmFloorZ - tmCeilingZ".
boolean floatOk;

coord_t tmFloorZ;
coord_t tmCeilingZ;
#if __JHEXEN__
Material *tmFloorMaterial;
#endif

boolean fellDown; // $dropoff_fix

// The following is used to keep track of the lines that clip the open
// height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
// logic and to prevent missiles from exploding against sky hack walls.
Line *ceilingLine;
Line *floorLine;

mobj_t *lineTarget; // Who got hit (or NULL).
Line *blockLine; // $unstuck: blocking line

coord_t attackRange;

#if __JHEXEN__
mobj_t *puffSpawned;
mobj_t *blockingMobj;
#endif

static coord_t tm[3];
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
static coord_t tmHeight;
static Line *tmHitLine;
#endif
static coord_t tmDropoffZ;
static coord_t bestSlideDistance, secondSlideDistance;
static Line *bestSlideLine, *secondSlideLine;

static mobj_t *slideMo;

static coord_t tmMove[3];
static mobj_t *shootThing;

// Height if not aiming up or down.
static coord_t shootZ;

static int lineAttackDamage;
static float aimSlope;

// slopes to top and bottom of target
static float topSlope, bottomSlope;

static mobj_t *useThing;

static mobj_t *bombSource, *bombSpot;
static int bombDamage;
static int bombDistance;

static boolean crushChange;
static boolean noFit;

static coord_t startPos[3]; // start position for trajectory line checks
static coord_t endPos[3]; // end position for trajectory checks

#if __JHEXEN__
static mobj_t *tsThing;
static boolean damageSource;
static mobj_t *onMobj; // generic global onMobj...used for landing on pods/players

static mobj_t *puzzleItemUser;
static int puzzleItemType;
static boolean puzzleActivated;
#endif

#if !__JHEXEN__
static int tmUnstuck; // $unstuck: used to check unsticking
#endif

static byte *rejectMatrix; // For fast sight rejection.

coord_t P_GetGravity()
{
    if(cfg.netGravity != -1)
        return (coord_t) cfg.netGravity / 100;

    return *((coord_t*) DD_GetVariable(DD_GRAVITY));
}

/**
 * Checks the reject matrix to find out if the two sectors are visible
 * from each other.
 */
static boolean checkReject(Sector *sec1, Sector *sec2)
{
    if(rejectMatrix)
    {
        // Determine BSP leaf entries in REJECT table.
        int const pnum = P_ToIndex(sec1) * numsectors + P_ToIndex(sec2);
        int const bytenum = pnum >> 3;
        int const bitnum = 1 << (pnum & 7);

        // Check in REJECT table.
        if(rejectMatrix[bytenum] & bitnum)
        {
            // Can't possibly be connected.
            return false;
        }
    }

    return true;
}

boolean P_CheckSight(mobj_t const *from, mobj_t const *to)
{
    coord_t fPos[3];

    if(!from || !to) return false;

    // If either is unlinked, they can't see each other.
    if(!Mobj_Sector(from) || !Mobj_Sector(to))
        return false;

    if(to->dPlayer && (to->dPlayer->flags & DDPF_CAMERA))
        return false; // Cameramen don't exist!

    // Check for trivial rejection.
    if(!checkReject(Mobj_Sector(from), Mobj_Sector(to)))
        return false;

    fPos[VX] = from->origin[VX];
    fPos[VY] = from->origin[VY];
    fPos[VZ] = from->origin[VZ];

    if(!P_MobjIsCamera(from))
        fPos[VZ] += from->height + -(from->height / 4);

    return P_CheckLineSight(fPos, to->origin, 0, to->height, 0);
}

int PIT_StompThing(mobj_t *mo, void *context)
{
    if(!(mo->flags & MF_SHOOTABLE))
        return false;

    coord_t blockdist = mo->radius + tmThing->radius;
    if(fabs(mo->origin[VX] - tm[VX]) >= blockdist ||
       fabs(mo->origin[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit it.

    if(mo == tmThing)
        return false; // Don't clip against self.

    int stompAnyway = *(int *) context;

    // Should we stomp anyway? unless self.
    if(mo != tmThing && stompAnyway)
    {
        P_DamageMobj(mo, tmThing, tmThing, 10000, true);
        return false;
    }

#if __JDOOM64__
    // monsters don't stomp things
    if(!tmThing->player)
        return true;
#elif __JDOOM__
    // Monsters don't stomp things except on a boss map.
    if(!tmThing->player && gameMap != 29)
        return true;
#endif

    if(!(tmThing->flags2 & MF2_TELESTOMP))
        return true; // Not allowed to stomp things.

    // Do stomp damage (unless self).
    if(mo != tmThing)
        P_DamageMobj(mo, tmThing, tmThing, 10000, true);

    return false;
}

// Kills anything occupying the position.
boolean P_TeleportMove(mobj_t *thing, coord_t x, coord_t y, boolean alwaysStomp)
{
    int stomping = alwaysStomp;

    tmThing = thing;

    tm[VX] = x;
    tm[VY] = y;

    tmBox.minX = tm[VX] - tmThing->radius;
    tmBox.minY = tm[VY] - tmThing->radius;
    tmBox.maxX = tm[VX] + tmThing->radius;
    tmBox.maxY = tm[VY] + tmThing->radius;

    Sector *newSector = Sector_AtPoint_FixedPrecision(tm);

    ceilingLine = floorLine = NULL;
#if !__JHEXEN__
    blockLine = NULL;
    tmUnstuck = thing->dPlayer && thing->dPlayer->mo == thing;
#endif

    // The base floor / ceiling is from the BSP leaf that contains the
    // point. Any contacted lines the step closer together will adjust them.
    tmFloorZ = tmDropoffZ = P_GetDoublep(newSector, DMU_FLOOR_HEIGHT);
    tmCeilingZ = P_GetDoublep(newSector, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    tmFloorMaterial = (Material *)P_GetPtrp(newSector, DMU_FLOOR_MATERIAL);
#endif

    IterList_Clear(spechit);

    AABoxd tmBoxExpanded(tmBox.minX - MAXRADIUS, tmBox.minY - MAXRADIUS,
                         tmBox.maxX + MAXRADIUS, tmBox.maxY + MAXRADIUS);

    // Stomp on any things contacted.
    VALIDCOUNT++;
    if(Mobj_BoxIterator(&tmBoxExpanded, PIT_StompThing, &stomping))
        return false;

    // The move is ok, so link the thing into its new position.
    P_MobjUnlink(thing);

    thing->floorZ = tmFloorZ;
    thing->ceilingZ = tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = tmDropoffZ;
#endif
    thing->origin[VX] = x;
    thing->origin[VY] = y;

    P_MobjLink(thing);
    P_MobjClearSRVO(thing);

    return true;
}

void P_TelefragMobjsTouchingPlayers(void)
{
    uint i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = players + i;
        ddplayer_t* ddplr = plr->plr;
        if(!ddplr->inGame) continue;

        P_TeleportMove(ddplr->mo, ddplr->mo->origin[VX], ddplr->mo->origin[VY], true);
    }
}

/**
 * Checks to see if a start->end trajectory line crosses a blocking line.
 * Returns false if it does.
 *
 * tmBox holds the bounding box of the trajectory. If that box does not
 * touch the bounding box of the line in question, then the trajectory is
 * not blocked. If the start is on one side of the line and the end is on
 * the other side, then the trajectory is blocked.
 *
 * Currently this assumes an infinite line, which is not quite correct.
 * A more correct solution would be to check for an intersection of the
 * trajectory and the line, but that takes longer and probably really isn't
 * worth the effort.
 */
int PIT_CrossLine(Line *line, void * /*context*/)
{
    int flags = P_GetIntp(line, DMU_FLAGS);

    if((flags & DDLF_BLOCKING) ||
       (P_ToXLine(line)->flags & ML_BLOCKMONSTERS) ||
       (!P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR)))
    {
        AABoxd *aaBox = (AABoxd *)P_GetPtrp(line, DMU_BOUNDING_BOX);

        if(!(tmBox.minX > aaBox->maxX ||
             tmBox.maxX < aaBox->minX ||
             tmBox.maxY < aaBox->minY ||
             tmBox.minY > aaBox->maxY))
        {
            // Line blocks trajectory?
            if(Line_PointOnSide(line, startPos) < 0 !=
               Line_PointOnSide(line,   endPos) < 0)
            {
                return true;
            }
        }
    }

    // Line doesn't block trajectory.
    return false;
}

/**
 * This routine checks for Lost Souls trying to be spawned across 1-sided
 * lines, impassible lines, or "monsters can't cross" lines.
 *
 * Draw an imaginary line between the PE and the new Lost Soul spawn spot.
 * If that line crosses a 'blocking' line, then disallow the spawn. Only
 * search lines in the blocks of the blockmap where the bounding box of the
 * trajectory line resides. Then check bounding box of the trajectory vs
 * the bounding box of each blocking line to see if the trajectory and the
 * blocking line cross. Then check the PE and LS to see if they are on
 * different sides of the blocking line. If so, return true otherwise
 * false.
 */
boolean P_CheckSides(mobj_t* actor, coord_t x, coord_t y)
{
    startPos[VX] = actor->origin[VX];
    startPos[VY] = actor->origin[VY];
    startPos[VZ] = actor->origin[VZ];

    endPos[VX] = x;
    endPos[VY] = y;
    endPos[VZ] = DDMINFLOAT; // Initialize with *something*.

    // The bounding box of the trajectory
    tmBox.minX = (startPos[VX] < endPos[VX]? startPos[VX] : endPos[VX]);
    tmBox.minY = (startPos[VY] < endPos[VY]? startPos[VY] : endPos[VY]);
    tmBox.maxX = (startPos[VX] > endPos[VX]? startPos[VX] : endPos[VX]);
    tmBox.maxY = (startPos[VY] > endPos[VY]? startPos[VY] : endPos[VY]);

    VALIDCOUNT++;
    return Line_BoxIterator(&tmBox, LIF_ALL, PIT_CrossLine, 0);
}

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
/**
 * $unstuck: used to test intersection between thing and line assuming NO
 * movement occurs -- used to avoid sticky situations.
 */
static int untouched(Line *line)
{
    coord_t const x      = tmThing->origin[VX];
    coord_t const y      = tmThing->origin[VY];
    coord_t const radius = tmThing->radius;
    AABoxd const *ldBox  = (AABoxd *)P_GetPtrp(line, DMU_BOUNDING_BOX);
    AABoxd moBox;

    if(((moBox.minX = x - radius) >= ldBox->maxX) ||
       ((moBox.minY = y - radius) >= ldBox->maxY) ||
       ((moBox.maxX = x + radius) <= ldBox->minX) ||
       ((moBox.maxY = y + radius) <= ldBox->minY) ||
       Line_BoxOnSide(line, &moBox))
    {
        return true;
    }
    return false;
}
#endif

int PIT_CheckThing(mobj_t* thing, void* data)
{
    int damage;
    coord_t blockdist;
    boolean solid;
#if !__JHEXEN__
    boolean overlap = false;
#endif

    // Don't clip against self.
    if(thing == tmThing)
        return false;

#if __JHEXEN__
    // Don't clip on something we are stood on.
    if(thing == tmThing->onMobj)
        return false;
#endif

    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)) ||
       P_MobjIsCamera(thing) || P_MobjIsCamera(tmThing))
        return false;

#if !__JHEXEN__
    // Player only.
    if(tmThing->player && !FEQUAL(tm[VZ], DDMAXFLOAT) &&
       (cfg.moveCheckZ || (tmThing->flags2 & MF2_PASSMOBJ)))
    {
        if((thing->origin[VZ] > tm[VZ] + tmHeight) ||
           (thing->origin[VZ] + thing->height < tm[VZ]))
            return false; // Under or over it.

        overlap = true;
    }
#endif

    blockdist = thing->radius + tmThing->radius;
    if(fabs(thing->origin[VX] - tm[VX]) >= blockdist ||
       fabs(thing->origin[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit thing.

    if(IS_CLIENT)
    {
        // On clientside, missiles don't collide with mobjs.
        if(tmThing->ddFlags & DDMF_MISSILE)
        {
            return false;
        }

        // Players can't hit their own clmobjs.
        if(tmThing->player && ClPlayer_ClMobj(tmThing->player - players) == thing)
        {
            return false;
        }
    }

/*
#if __JHEXEN__
    // Stop here if we are a client.
    if(IS_CLIENT)
        return true;
#endif
*/

#if !__JHEXEN__
    if(!tmThing->player && (tmThing->flags2 & MF2_PASSMOBJ))
#else
    blockingMobj = thing;
    if(tmThing->flags2 & MF2_PASSMOBJ)
#endif
    {   // Check if a mobj passed over/under another object.
#if __JHERETIC__
        if((tmThing->type == MT_IMP || tmThing->type == MT_WIZARD) &&
           (thing->type == MT_IMP || thing->type == MT_WIZARD))
        {
            return true; // Don't let imps/wizards fly over other imps/wizards.
        }
#elif __JHEXEN__
        if(tmThing->type == MT_BISHOP && thing->type == MT_BISHOP)
            return true; // Don't let bishops fly over other bishops.
#endif

        if(!(thing->flags & MF_SPECIAL))
        {
            if(tmThing->origin[VZ] > thing->origin[VZ] + thing->height ||
               tmThing->origin[VZ] + tmThing->height < thing->origin[VZ])
                return false; // Over/under thing.
        }
    }

    // Check for skulls slamming into things.
    if((tmThing->flags & MF_SKULLFLY) && (thing->flags & MF_SOLID))
    {
#if __JHEXEN__
        blockingMobj = NULL;
        if(tmThing->type == MT_MINOTAUR)
        {
            // Slamming minotaurs shouldn't move non-creatures.
            if(!(thing->flags & MF_COUNTKILL))
            {
                return true;
            }
        }
        else if(tmThing->type == MT_HOLY_FX)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                if(IS_NETGAME && !deathmatch && thing->player)
                    return false; // don't attack other co-op players

                if((thing->flags2 & MF2_REFLECTIVE) &&
                   (thing->player || (thing->flags2 & MF2_BOSS)))
                {
                    tmThing->tracer = tmThing->target;
                    tmThing->target = thing;
                    return false;
                }

                if(thing->flags & MF_COUNTKILL || thing->player)
                {
                    tmThing->tracer = thing;
                }

                if(P_Random() < 96)
                {
                    damage = 12;
                    if(thing->player || (thing->flags2 & MF2_BOSS))
                    {
                        damage = 3;
                        // Ghost burns out faster when attacking players/bosses.
                        tmThing->health -= 6;
                    }

                    P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
                    if(P_Random() < 128)
                    {
                        P_SpawnMobj(MT_HOLY_PUFF, tmThing->origin,
                                       P_Random() << 24, 0);
                        S_StartSound(SFX_SPIRIT_ATTACK, tmThing);
                        if((thing->flags & MF_COUNTKILL) && P_Random() < 128 &&
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
                    tmThing->tracer = NULL;
            }
            return false;
        }
#endif
#if __JDOOM__
        /// @attention Kludge:
        /// Older save versions did not serialize the damage property,
        /// so here we take the damage from the current Thing definition.
        /// @fixme Do this during map state deserialization.
        if(tmThing->damage == DDMAXINT)
            damage = tmThing->info->damage;
        else
#endif
            damage = tmThing->damage;

        damage *= (P_Random() % 8) + 1;
        P_DamageMobj(thing, tmThing, tmThing, damage, false);

        tmThing->flags &= ~MF_SKULLFLY;
        tmThing->mom[MX] = tmThing->mom[MY] = tmThing->mom[MZ] = 0;

#if __JHERETIC__ || __JHEXEN__
        P_MobjChangeState(tmThing, P_GetState(mobjtype_t(tmThing->type), SN_SEE));
#else
        P_MobjChangeState(tmThing, P_GetState(mobjtype_t(tmThing->type), SN_SPAWN));
#endif

        return true; // Stop moving.
    }

#if __JHEXEN__
    // Check for blasted thing running into another
    if((tmThing->flags2 & MF2_BLASTED) && (thing->flags & MF_SHOOTABLE))
    {
        if(!(thing->flags2 & MF2_BOSS) && (thing->flags & MF_COUNTKILL))
        {
            thing->mom[MX] += tmThing->mom[MX];
            thing->mom[MY] += tmThing->mom[MY];

            NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX], tmThing->mom[VY], 0);

            if((thing->mom[MX] + thing->mom[MY]) > 3)
            {
                damage = (tmThing->info->mass / 100) + 1;
                P_DamageMobj(thing, tmThing, tmThing, damage, false);

                damage = (thing->info->mass / 100) + 1;
                P_DamageMobj(tmThing, thing, thing, damage >> 2, false);
            }

            return true;
        }
    }
#endif

    // Missiles can hit other things.
    if(tmThing->flags & MF_MISSILE)
    {
#if __JHEXEN__
        // Check for a non-shootable mobj.
        if(thing->flags2 & MF2_NONSHOOTABLE)
            return false;
#else
        // Check for passing through a ghost.
        if((thing->flags & MF_SHADOW) && (tmThing->flags2 & MF2_THRUGHOST))
            return false;
#endif

        // See if it went over / under.
        if(tmThing->origin[VZ] > thing->origin[VZ] + thing->height)
            return false; // Overhead.

        if(tmThing->origin[VZ] + tmThing->height < thing->origin[VZ])
            return false; // Underneath.

#if __JHEXEN__
        if(tmThing->flags2 & MF2_FLOORBOUNCE)
        {
            if(tmThing->target == thing || !(thing->flags & MF_SOLID))
                return false;
            else
                return true;
        }

        if(tmThing->type == MT_LIGHTNING_FLOOR ||
           tmThing->type == MT_LIGHTNING_CEILING)
        {
            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                if(thing->info->mass != DDMAXINT)
                {
                    thing->mom[MX] += tmThing->mom[MX] / 16;
                    thing->mom[MY] += tmThing->mom[MY] / 16;

                    NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX] / 16, tmThing->mom[MY] / 16, 0);
                }

                if((!thing->player && !(thing->flags2 & MF2_BOSS)) ||
                   !(mapTime & 1))
                {
                    if(thing->type == MT_CENTAUR ||
                       thing->type == MT_CENTAURLEADER)
                    {   // Lightning does more damage to centaurs.
                        P_DamageMobj(thing, tmThing, tmThing->target, 9, false);
                    }
                    else
                    {
                        P_DamageMobj(thing, tmThing, tmThing->target, 3, false);
                    }

                    if(!(S_IsPlaying(SFX_MAGE_LIGHTNING_ZAP, tmThing)))
                    {
                        S_StartSound(SFX_MAGE_LIGHTNING_ZAP, tmThing);
                    }

                    if((thing->flags & MF_COUNTKILL) && P_Random() < 64 &&
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

                tmThing->health--;
                if(tmThing->health <= 0 || thing->health <= 0)
                {
                    return true;
                }

                if(tmThing->type == MT_LIGHTNING_FLOOR)
                {
                    if(tmThing->lastEnemy &&
                       !tmThing->lastEnemy->tracer)
                    {
                        tmThing->lastEnemy->tracer = thing;
                    }
                }
                else if(!tmThing->tracer)
                {
                    tmThing->tracer = thing;
                }
            }

            return false; // Lightning zaps through all sprites.
        }
        else if(tmThing->type == MT_LIGHTNING_ZAP)
        {
            mobj_t *lmo;

            if((thing->flags & MF_SHOOTABLE) && thing != tmThing->target)
            {
                lmo = tmThing->lastEnemy;
                if(lmo)
                {
                    if(lmo->type == MT_LIGHTNING_FLOOR)
                    {
                        if(lmo->lastEnemy &&
                           !lmo->lastEnemy->tracer)
                        {
                            lmo->lastEnemy->tracer = thing;
                        }
                    }
                    else if(!lmo->tracer)
                    {
                        lmo->tracer = thing;
                    }

                    if(!(mapTime & 3))
                    {
                        lmo->health--;
                    }
                }
            }
        }
        else if(tmThing->type == MT_MSTAFF_FX2 && thing != tmThing->target)
        {
            if(!thing->player && !(thing->flags2 & MF2_BOSS))
            {
                switch(thing->type)
                {
                case MT_FIGHTER_BOSS:   // these not flagged boss
                case MT_CLERIC_BOSS:    // so they can be blasted
                case MT_MAGE_BOSS:
                    break;
                default:
                    P_DamageMobj(thing, tmThing, tmThing->target, 10, false);
                    return false;
                    break;
                }
            }
        }
#endif

        // Don't hit same species as originator.
#if __JDOOM__ || __JDOOM64__
        if(tmThing->target &&
           (tmThing->target->type == thing->type ||
           (tmThing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
           (tmThing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
#else
        if(tmThing->target && tmThing->target->type == thing->type)
#endif
        {
            if(thing == tmThing->target)
                return false;

#if __JHEXEN__
            if(!thing->player)
                return true; // Hit same species as originator, explode, no damage
#else
            if(!monsterInfight && thing->type != MT_PLAYER)
            {
                // Explode, but do no damage.
                // Let players missile other players.
                return true;
            }
#endif
        }

        if(!(thing->flags & MF_SHOOTABLE))
        {
            return !!(thing->flags & MF_SOLID); // Didn't do any damage.
        }

        if(tmThing->flags2 & MF2_RIP)
        {
#if __JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE))
#else
            if(!(thing->flags & MF_NOBLOOD))
#endif
            {   // Ok to spawn some blood.
                P_RipperBlood(tmThing);
            }

#if __JHERETIC__
            S_StartSound(SFX_RIPSLOP, tmThing);
#endif

#if __JDOOM__
            /// @attention Kludge:
            /// Older save versions did not serialize the damage property,
            /// so here we take the damage from the current Thing definition.
            /// @fixme Do this during map state deserialization.
            if(tmThing->damage == DDMAXINT)
            {
                damage = tmThing->info->damage;
            }
            else
#endif
            {
                damage = tmThing->damage;
            }
            damage *= (P_Random() & 3) + 2;

            P_DamageMobj(thing, tmThing, tmThing->target, damage, false);

            if((thing->flags2 & MF2_PUSHABLE) && !(tmThing->flags2 & MF2_CANNOTPUSH))
            {
                // Push thing
                thing->mom[MX] += tmThing->mom[MX] / 4;
                thing->mom[MY] += tmThing->mom[MY] / 4;
                NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX]/4, tmThing->mom[MY]/4, 0);
            }

            IterList_Clear(spechit);
            return false;
        }

        // Do damage
#if __JDOOM__
        /// @attention Kludge:
        /// Older save versions did not serialize the damage property,
        /// so here we take the damage from the current Thing definition.
        /// @fixme Do this during map state deserialization.
        if(tmThing->damage == DDMAXINT)
        {
            damage = tmThing->info->damage;
        }
        else
#endif
        {
            damage = tmThing->damage;
        }
        damage *= (P_Random() % 8) + 1;

#if __JDOOM__ || __JDOOM64__
        P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
#else
        if(damage)
        {
# if __JHERETIC__
            if(!(thing->flags & MF_NOBLOOD) && P_Random() < 192)
# else //__JHEXEN__
            if(!(thing->flags & MF_NOBLOOD) &&
               !(thing->flags2 & MF2_REFLECTIVE) &&
               !(thing->flags2 & MF2_INVULNERABLE) &&
               !(tmThing->type == MT_TELOTHER_FX1) &&
               !(tmThing->type == MT_TELOTHER_FX2) &&
               !(tmThing->type == MT_TELOTHER_FX3) &&
               !(tmThing->type == MT_TELOTHER_FX4) &&
               !(tmThing->type == MT_TELOTHER_FX5) && (P_Random() < 192))
# endif
            {
                P_SpawnBloodSplatter(tmThing->origin[VX], tmThing->origin[VY], tmThing->origin[VZ], thing);
            }

            P_DamageMobj(thing, tmThing, tmThing->target, damage, false);
        }
#endif
        // Don't traverse anymore.
        return true;
    }

    if((thing->flags2 & MF2_PUSHABLE) && !(tmThing->flags2 & MF2_CANNOTPUSH))
    {
        // Push thing
        thing->mom[MX] += tmThing->mom[MX] / 4;
        thing->mom[MY] += tmThing->mom[MY] / 4;
        NetSv_PlayerMobjImpulse(thing, tmThing->mom[MX]/4, tmThing->mom[MY]/4, 0);
    }

    // @fixme Kludge: Always treat blood as a solid.
    if(tmThing->type == MT_BLOOD)
    {
        solid = true;
    }
    else
    {
        solid = (thing->flags & MF_SOLID) && !(thing->flags & MF_NOCLIP) &&
                (tmThing->flags & MF_SOLID);
    }
    // Kludge end.

#if __JHEXEN__
    if(tmThing->player && tmThing->onMobj && solid)
    {
        /// @todo Unify Hexen's onMobj logic with the other games.

        // We may be standing on more than one thing.
        if(tmThing->origin[VZ] > thing->origin[VZ] + thing->height - 24)
        {
            // Stepping up on this is possible.
            tmFloorZ = MAX_OF(tmFloorZ, thing->origin[VZ] + thing->height);
            solid = false;
        }
    }
#endif

    // Check for special pickup.
    if((thing->flags & MF_SPECIAL) && (tmThing->flags & MF_PICKUP))
    {
        P_TouchSpecialMobj(thing, tmThing); // Can remove thing.
    }
#if !__JHEXEN__
    else if(overlap && solid)
    {
        // How are we positioned, allow step up?
        if(!(thing->flags & MF_CORPSE) && tm[VZ] > thing->origin[VZ] + thing->height - 24)
        {
            tmThing->onMobj = thing;
            if(thing->origin[VZ] + thing->height > tmFloorZ)
            {
                tmFloorZ = thing->origin[VZ] + thing->height;
            }
            return false;
        }
    }
    else if(!tmThing->player && solid)
    {
        // A non-player object is contacting a solid object.
        if(cfg.allowMonsterFloatOverBlocking && (tmThing->flags & MF_FLOAT) && !thing->player)
        {
            coord_t top = thing->origin[VZ] + thing->height;
            tmThing->onMobj = thing;
            tmFloorZ = MAX_OF(tmFloorZ, top);
            return false;
        }
    }
#endif

    return solid;
}

/**
 * Adjusts tmFloorZ and tmCeilingZ as lines are contacted.
 */
int PIT_CheckLine(Line *ld, void * /*context*/)
{
    AABoxd const *aaBox = (AABoxd *)P_GetPtrp(ld, DMU_BOUNDING_BOX);
    if(tmBox.minX >= aaBox->maxX ||
       tmBox.minY >= aaBox->maxY ||
       tmBox.maxX <= aaBox->minX ||
       tmBox.maxY <= aaBox->minY)
        return false;

    /*
     * Real player mobjs are allowed to use high-precision, non-vanilla
     * collision testing -- the rest of the playsim uses coord_t, and we don't
     * want conflicting results (e.g., getting stuck in tight spaces).
     */
    if(Mobj_IsPlayer(tmThing) && !Mobj_IsVoodooDoll(tmThing))
    {
        if(Line_BoxOnSide(ld, &tmBox)) // double precision floats
        {
            return false;
        }
    }
    else
    {
        // Fixed-precision math gives better compatibility with vanilla DOOM.
        if(Line_BoxOnSide_FixedPrecision(ld, &tmBox))
        {
            return false;
        }
    }

    // A line has been hit.
    xline_t *xline = P_ToXLine(ld);

#if !__JHEXEN__
    tmThing->wallHit = true;

    // A Hit event will be sent to special lines.
    if(xline->special)
        tmHitLine = ld;
#endif

    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // One sided line.
    {
#if __JHEXEN__
        if(tmThing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
        }

        checkForPushSpecial(ld, 0, tmThing);
        return true;
#else
        coord_t d1[2];

        P_GetDoublepv(ld, DMU_DXY, d1);

        /**
         * $unstuck: allow player to move out of 1s wall, to prevent
         * sticking. The moving thing's destination position will cross the
         * given line.
         * If this should not be allowed, return false.
         * If the line is special, keep track of it to process later if the
         * move is proven ok.
         *
         * @note Specials are NOT sorted by order, so two special lines that
         *       are only 8 units apart could be crossed in either order.
         */

        blockLine = ld;
        return !(tmUnstuck && !untouched(ld) &&
            ((tm[VX] - tmThing->origin[VX]) * d1[1]) >
            ((tm[VY] - tmThing->origin[VY]) * d1[0]));
#endif
    }

    /// @todo Will never pass this test due to above. Is the previous check
    ///       supposed to qualify player mobjs only?
#if __JHERETIC__
    if(!P_GetPtrp(ld, DMU_BACK_SECTOR)) // one sided line
    {
        // One sided line
        if(tmThing->flags & MF_MISSILE)
        {
            // Missiles can trigger impact specials
            if(xline->special)
            {
                IterList_PushBack(spechit, ld);
            }
        }
        return true;
    }
#endif

    if(!(tmThing->flags & MF_MISSILE))
    {
        // Explicitly blocking everything?
        if(P_GetIntp(ld, DMU_FLAGS) & DDLF_BLOCKING)
        {
#if __JHEXEN__
            if(tmThing->flags2 & MF2_BLASTED)
            {
                P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
            }

            checkForPushSpecial(ld, 0, tmThing);
            return true;
#else
            // $unstuck: allow escape.
            return !(tmUnstuck && !untouched(ld));
#endif
        }

        // Block monsters only?
#if __JHEXEN__
        if(!tmThing->player && tmThing->type != MT_CAMERA &&
           (xline->flags & ML_BLOCKMONSTERS))
#elif __JHERETIC__
        if(!tmThing->player && tmThing->type != MT_POD &&
           (xline->flags & ML_BLOCKMONSTERS))
#else
        if(!tmThing->player &&
            (xline->flags & ML_BLOCKMONSTERS))
#endif
        {
#if __JHEXEN__
            if(tmThing->flags2 & MF2_BLASTED)
                P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
#endif
            return true;
        }
    }

#if __JDOOM64__
    if((tmThing->flags & MF_MISSILE))
    {
        if(xline->flags & ML_BLOCKALL) // Explicitly blocking everything.
            return !(tmUnstuck && !untouched(ld));  // $unstuck: allow escape.
    }
#endif

    LineOpening opening; Line_Opening(ld, &opening);

    // Adjust floor / ceiling heights.
    if(opening.top < tmCeilingZ)
    {
        tmCeilingZ = opening.top;
        ceilingLine = ld;
#if !__JHEXEN__
        blockLine = ld;
#endif
    }

    if(opening.bottom > tmFloorZ)
    {
        tmFloorZ = opening.bottom;
        floorLine = ld;
#if !__JHEXEN__
        blockLine = ld;
#endif
    }

    if(opening.lowFloor < tmDropoffZ)
        tmDropoffZ = opening.lowFloor;

    // If contacted a special line, add it to the list.
    if(P_ToXLine(ld)->special)
    {
        IterList_PushBack(spechit, ld);
    }

#if !__JHEXEN__
    tmThing->wallHit = false;
#endif

    return false; // Continue iteration.
}

boolean P_CheckPositionXYZ(mobj_t *thing, coord_t x, coord_t y, coord_t z)
{
#if !__JHEXEN__
    thing->onMobj = 0;
#endif
    thing->wallHit = false;

    tmThing         = thing;
    V3d_Set(tm, x, y, z);
    tmBox           = AABoxd(tm[VX] - tmThing->radius, tm[VY] - tmThing->radius,
                             tm[VX] + tmThing->radius, tm[VY] + tmThing->radius);
#if !__JHEXEN__
    tmHitLine       = NULL;
    tmHeight        = thing->height;
#endif

    // The base floor/ceiling is from the BSP leaf that contains the point.
    // Any contacted lines the step closer together will adjust them.
    Sector *newSector = Sector_AtPoint_FixedPrecision(tm);

    ceilingLine     = floorLine = 0;
    tmFloorZ        = tmDropoffZ = P_GetDoublep(newSector, DMU_FLOOR_HEIGHT);
    tmCeilingZ      = P_GetDoublep(newSector, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    tmFloorMaterial = (Material *)P_GetPtrp(newSector, DMU_FLOOR_MATERIAL);
#else
    blockLine       = 0;
    tmUnstuck       = (thing->dPlayer && thing->dPlayer->mo == thing);
#endif

    IterList_Clear(spechit);

    if(tmThing->flags & MF_NOCLIP)
    {
#if __JHEXEN__
        if(!(tmThing->flags & MF_SKULLFLY))
            return true;
#else
        return true;
#endif
    }

    // Check things first, possibly picking things up the bounding box is
    // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap into adjacent blocks by
    // up to MAXRADIUS units.
    AABoxd tmBoxExpanded(tmBox.minX - MAXRADIUS, tmBox.minY - MAXRADIUS,
                         tmBox.maxX + MAXRADIUS, tmBox.maxY + MAXRADIUS);

    VALIDCOUNT++;

    // The camera goes through all objects.
    if(!P_MobjIsCamera(thing))
    {
#if __JHEXEN__
        blockingMobj = 0;
#endif
        if(Mobj_BoxIterator(&tmBoxExpanded, PIT_CheckThing, 0))
        {
            return false;
        }

#if _DEBUG
        VERBOSE2(
            if(thing->onMobj)
                Con_Message("thing->onMobj = %p/%i (solid:%i) [thing:%p/%i]", thing->onMobj,
                            thing->onMobj->thinker.id,
                            (thing->onMobj->flags & MF_SOLID)!=0,
                            thing, thing->thinker.id)
        );
#endif
    }

#if __JHEXEN__
    if(tmThing->flags & MF_NOCLIP)
    {
        return true;
    }
#endif

    // Check lines.
    tmBoxExpanded = tmBox;
#if __JHEXEN__
    blockingMobj  = 0;
#endif

    return !Line_BoxIterator(&tmBoxExpanded, LIF_ALL, PIT_CheckLine, 0);
}

boolean P_CheckPosition(mobj_t *thing, coord_t const pos[3])
{
    return P_CheckPositionXYZ(thing, pos[VX], pos[VY], pos[VZ]);
}

boolean P_CheckPositionXY(mobj_t *thing, coord_t x, coord_t y)
{
    return P_CheckPositionXYZ(thing, x, y, DDMAXFLOAT);
}

boolean Mobj_IsRemotePlayer(mobj_t *mo)
{
    return (mo && ((IS_DEDICATED && mo->dPlayer) ||
                   (IS_CLIENT && mo->player && mo->player - players != CONSOLEPLAYER)));
}

/**
 * Attempt to move to a new position, crossing special lines unless
 * MF_TELEPORT is set. $dropoff_fix
 */
#if __JHEXEN__
static boolean P_TryMove2(mobj_t *thing, coord_t x, coord_t y)
#else
static boolean P_TryMove2(mobj_t *thing, coord_t x, coord_t y, boolean dropoff)
#endif
{
    boolean const isRemotePlayer = Mobj_IsRemotePlayer(thing);

    // $dropoff_fix: fellDown.
    floatOk  = false;
#if !__JHEXEN__
    fellDown = false;
#endif

#if __JHEXEN__
    if(!P_CheckPositionXY(thing, x, y))
#else
    if(!P_CheckPositionXYZ(thing, x, y, thing->origin[VZ]))
#endif
    {
#if __JHEXEN__
        if(!blockingMobj || blockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(blockingMobj->origin[VZ] + blockingMobj->height - thing->origin[VZ] > 24 ||
                (P_GetDoublep(Mobj_Sector(blockingMobj), DMU_CEILING_HEIGHT) -
                 (blockingMobj->origin[VZ] + blockingMobj->height) < thing->height) ||
                (tmCeilingZ - (blockingMobj->origin[VZ] + blockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
#else
#  if __JHERETIC__
        CheckMissileImpact(thing);
#  endif
        // Would we hit another thing or a solid wall?
        if(!thing->onMobj || thing->wallHit)
            return false;
#endif
    }

    if(!(thing->flags & MF_NOCLIP))
    {
#if __JHEXEN__
        if(tmCeilingZ - tmFloorZ < thing->height)
        {   // Doesn't fit.
            goto pushline;
        }

        floatOk = true;

        if(!(thing->flags & MF_TELEPORT) &&
           tmCeilingZ - thing->origin[VZ] < thing->height &&
           thing->type != MT_LIGHTNING_CEILING && !(thing->flags2 & MF2_FLY))
        {
            // Mobj must lower itself to fit.
            goto pushline;
        }
#else
        // Possibly allow escape if otherwise stuck.
        boolean ret = (tmUnstuck &&
            !(ceilingLine && untouched(ceilingLine)) &&
            !(floorLine   && untouched(floorLine)));

        if(tmCeilingZ - tmFloorZ < thing->height)
        {
            return ret; // Doesn't fit.
        }

        // Mobj must lower to fit.
        floatOk = true;
        if(!(thing->flags & MF_TELEPORT) && !(thing->flags2 & MF2_FLY) &&
           tmCeilingZ - thing->origin[VZ] < thing->height)
        {
            return ret;
        }

        // Too big a step up.
        if(!(thing->flags & MF_TELEPORT) &&
            !(thing->flags2 & MF2_FLY)
# if __JHERETIC__
            && thing->type != MT_MNTRFX2 // The Minotaur floor fire (MT_MNTRFX2) can step up any amount
# endif
            )
        {
            if(!isRemotePlayer && tmFloorZ - thing->origin[VZ] > 24)
            {
# if __JHERETIC__
                CheckMissileImpact(thing);
# endif
                return ret;
            }
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && tmFloorZ > thing->origin[VZ])
        {
            CheckMissileImpact(thing);
        }
# endif
#endif
        if(thing->flags2 & MF2_FLY)
        {
            if(thing->origin[VZ] + thing->height > tmCeilingZ)
            {
                thing->mom[MZ] = -8;
#if __JHEXEN__
                goto pushline;
#else
                return false;
#endif
            }
            else if(thing->origin[VZ] < tmFloorZ &&
                    tmFloorZ - tmDropoffZ > 24)
            {
                thing->mom[MZ] = 8;
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
           && thing->type != MT_MNTRFX2 && thing->type != MT_LIGHTNING_FLOOR
           && !isRemotePlayer
           && tmFloorZ - thing->origin[VZ] > 24)
        {
            goto pushline;
        }
#endif

#if __JHEXEN__
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)) &&
           (tmFloorZ - tmDropoffZ > 24) &&
           !(thing->flags2 & MF2_BLASTED))
        {
            // Can't move over a dropoff unless it's been blasted.
            return false;
        }
#else

        /*
         * Allow certain objects to drop off.
         * Prevent monsters from getting stuck hanging off ledges.
         * Allow dropoffs in controlled circumstances.
         * Improve symmetry of clipping on stairs.
         */
        if(!(thing->flags & (MF_DROPOFF | MF_FLOAT)))
        {
            // Dropoff height limit.
            if(cfg.avoidDropoffs)
            {
                if(tmFloorZ - tmDropoffZ > 24)
                {
                    return false; // Don't stand over dropoff.
                }
            }
            else
            {
                coord_t floorZ = tmFloorZ;

                if(thing->onMobj)
                {
                    // Thing is stood on something so use our z position as the floor.
                    floorZ = (thing->origin[VZ] > tmFloorZ? thing->origin[VZ] : tmFloorZ);
                }

                if(!dropoff)
                {
                    if(thing->floorZ - floorZ > 24 || thing->dropOffZ - tmDropoffZ > 24)
                        return false;
                }
                else
                {
                    fellDown = !(thing->flags & MF_NOGRAVITY) && thing->origin[VZ] - floorZ > 24;
                }
            }
        }
#endif

#if __JDOOM64__
        /// @todo D64 Mother demon fire attack.
        if(!(thing->flags & MF_TELEPORT) /*&& thing->type != MT_SPAWNFIRE*/
            && !isRemotePlayer
            && tmFloorZ - thing->origin[VZ] > 24)
        {
            // Too big a step up
            CheckMissileImpact(thing);
            return false;
        }
#endif

#if __JHEXEN__
        // Must stay within a sector of a certain floor type?
        if((thing->flags2 & MF2_CANTLEAVEFLOORPIC) &&
           (tmFloorMaterial != P_GetPtrp(Mobj_Sector(thing), DMU_FLOOR_MATERIAL) ||
            !FEQUAL(tmFloorZ, thing->origin[VZ])))
        {
            return false;
        }
#endif

#if !__JHEXEN__
        // $dropoff: prevent falling objects from going up too many steps.
        if(!thing->player && (thing->intFlags & MIF_FALLING) &&
           tmFloorZ - thing->origin[VZ] > (thing->mom[MX] * thing->mom[MX]) +
                                          (thing->mom[MY] * thing->mom[MY]))
        {
            return false;
        }
#endif
    }

    vec3d_t oldPos; V3d_Copy(oldPos, thing->origin);

    // The move is ok, so link the thing into its new position.
    P_MobjUnlink(thing);

    thing->origin[VX] = x;
    thing->origin[VY] = y;
    thing->floorZ     = tmFloorZ;
    thing->ceilingZ   = tmCeilingZ;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    thing->dropOffZ   = tmDropoffZ; // $dropoff_fix: keep track of dropoffs.
#endif

    P_MobjLink(thing);

    if(thing->flags2 & MF2_FLOORCLIP)
    {
        thing->floorClip = 0;

        if(FEQUAL(thing->origin[VZ], P_GetDoublep(Mobj_Sector(thing), DMU_FLOOR_HEIGHT)))
        {
            terraintype_t const *tt = P_MobjFloorTerrain(thing);
            if(tt->flags & TTF_FLOORCLIP)
            {
                thing->floorClip = 10;
            }
        }
    }

    // If any special lines were hit, do the effect.
    if(!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        Line *line;
        while(line = (Line *)IterList_Pop(spechit))
        {
            // See if the line was crossed.
            if(P_ToXLine(line)->special)
            {
                int side    = Line_PointOnSide(line, thing->origin) < 0;
                int oldSide = Line_PointOnSide(line, oldPos) < 0;

                if(side != oldSide)
                {
#if __JHEXEN__
                    if(thing->player)
                    {
                        P_ActivateLine(line, thing, oldSide, SPAC_CROSS);
                    }
                    else if(thing->flags2 & MF2_MCROSS)
                    {
                        P_ActivateLine(line, thing, oldSide, SPAC_MCROSS);
                    }
                    else if(thing->flags2 & MF2_PCROSS)
                    {
                        P_ActivateLine(line, thing, oldSide, SPAC_PCROSS);
                    }
#else
#ifdef _DEBUG
                    if(!IS_CLIENT && thing->player)
                    {
                        Con_Message("P_TryMove2: Mobj %i crossing line %i from %f,%f to %f,%f",
                                    thing->thinker.id, P_ToIndex(line),
                                    oldPos[VX], oldPos[VY],
                                    thing->origin[VX], thing->origin[VY]);
                    }
#endif
                    P_ActivateLine(line, thing, oldSide, SPAC_CROSS);
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
        if(tmThing->flags2 & MF2_BLASTED)
        {
            P_DamageMobj(tmThing, NULL, NULL, tmThing->info->mass >> 5, false);
        }

        IterList_SetIteratorDirection(spechit, ITERLIST_BACKWARD);
        IterList_RewindIterator(spechit);

        Line *line;
        while(line = (Line *)IterList_MoveIterator(spechit))
        {
            // See if the line was crossed.
            int side = Line_PointOnSide(line, thing->origin) < 0;
            checkForPushSpecial(line, side, thing);
        }
    }
    return false;
#endif
}

#if __JHEXEN__
boolean P_TryMoveXY(mobj_t* thing, coord_t x, coord_t y)
#else
boolean P_TryMoveXY(mobj_t* thing, coord_t x, coord_t y, boolean dropoff, boolean slide)
#endif
{
#if __JHEXEN__
    return P_TryMove2(thing, x, y);
#else
    // $dropoff_fix
    boolean res = P_TryMove2(thing, x, y, dropoff);

    if(!res && tmHitLine)
    {
        // Move not possible, see if the thing hit a line and send a Hit
        // event to it.
        XL_HitLine(tmHitLine, Line_PointOnSide(tmHitLine, thing->origin) < 0,
                   thing);
    }

    if(res && slide)
        thing->wallRun = true;

    return res;
#endif
}

boolean P_TryMoveXYZ(mobj_t* thing, coord_t x, coord_t y, coord_t z)
{
    coord_t const oldZ = thing->origin[VZ];

    // Go to the new Z height.
    thing->origin[VZ] = z;

#ifdef __JHEXEN__
    if(P_TryMoveXY(thing, x, y))
#else
    if(P_TryMoveXY(thing, x, y, false, false))
#endif
    {
        // The move was successful.
        return true;
    }

    // The move failed, so restore the original position.
    thing->origin[VZ] = oldZ;
    return false;
}

/**
 * @todo This routine has gotten way too big, split if(in->isaline)
 *       to a seperate routine?
 */
int PTR_ShootTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    vec3d_t const tracePos = {
        FIX2FLT(Interceptor_Origin(trace)[VX]), FIX2FLT(Interceptor_Origin(trace)[VY]), shootZ
    };

    if(in->type == ICPT_LINE)
    {
        bool lineWasHit = false;

        Line *li = in->line;
        xline_t *xline = P_ToXLine(li);

        Sector *backSec = (Sector *)P_GetPtrp(li, DMU_BACK_SECTOR);

        if(!backSec || !(xline->flags & ML_TWOSIDED))
        {
            if(Line_PointOnSide(li, tracePos) < 0)
            {
                return false; // Continue traversal.
            }
        }

        if(xline->special)
        {
            P_ActivateLine(li, shootThing, 0, SPAC_IMPACT);
        }

        if(!backSec) goto hitline;

#if __JDOOM64__
        if(xline->flags & ML_BLOCKALL) goto hitline;
#endif

        // Crosses a two sided line.
        Interceptor_AdjustOpening(trace, li);

        Sector *frontSec = (Sector *)P_GetPtrp(li, DMU_FRONT_SECTOR);

        coord_t dist = attackRange * in->distance;
        coord_t slope = 0;
        if(!FEQUAL(P_GetDoublep(frontSec, DMU_FLOOR_HEIGHT),
                   P_GetDoublep(backSec,  DMU_FLOOR_HEIGHT)))
        {
            slope = (Interceptor_Opening(trace)->bottom - tracePos[VZ]) / dist;

            if(slope > aimSlope) goto hitline;
        }

        if(!FEQUAL(P_GetDoublep(frontSec, DMU_CEILING_HEIGHT),
                   P_GetDoublep(backSec,  DMU_CEILING_HEIGHT)))
        {
            slope = (Interceptor_Opening(trace)->top - tracePos[VZ]) / dist;

            if(slope < aimSlope) goto hitline;
        }

        // Shot continues...
        return false;

        // Hit a line.
      hitline:

        // Position a bit closer.
        coord_t frac = in->distance - (4 / attackRange);
        vec3d_t pos = { tracePos[VX] + FIX2FLT(Interceptor_Direction(trace)[VX]) * frac,
                        tracePos[VY] + FIX2FLT(Interceptor_Direction(trace)[VY]) * frac,
                        tracePos[VZ] + aimSlope * (frac * attackRange) };

        if(backSec)
        {
            // Is it a sky hack wall? If the hitpoint is beyond the visible
            // surface, no puff must be shown.
            if((P_GetIntp(P_GetPtrp(frontSec, DMU_CEILING_MATERIAL),
                          DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] > P_GetDoublep(frontSec, DMU_CEILING_HEIGHT) ||
                pos[VZ] > P_GetDoublep(backSec,  DMU_CEILING_HEIGHT)))
            {
                return true;
            }

            if((P_GetIntp(P_GetPtrp(backSec, DMU_FLOOR_MATERIAL),
                          DMU_FLAGS) & MATF_SKYMASK) &&
               (pos[VZ] < P_GetDoublep(frontSec, DMU_FLOOR_HEIGHT) ||
                pos[VZ] < P_GetDoublep(backSec,  DMU_FLOOR_HEIGHT)))
            {
                return true;
            }
        }

        lineWasHit = true;

        // This is the sector where the trace originates.
        Sector *originSector = Sector_AtPoint_FixedPrecision(tracePos);

        vec3d_t d; V3d_Subtract(d, pos, tracePos);

        if(!INRANGE_OF(d[VZ], 0, .0001f)) // Epsilon
        {
            Sector *contact = Sector_AtPoint_FixedPrecision(pos);
            coord_t step    = M_ApproxDistance3(d[VX], d[VY], d[VZ] * 1.2/*aspect ratio*/);
            vec3d_t stepv   = { d[VX] / step, d[VY] / step, d[VZ] / step };

            // Backtrack until we find a non-empty sector.
            coord_t cFloor = P_GetDoublep(contact, DMU_FLOOR_HEIGHT);
            coord_t cCeil  = P_GetDoublep(contact, DMU_CEILING_HEIGHT);
            while(cCeil <= cFloor && contact != originSector)
            {
                d[VX] -= 8 * stepv[VX];
                d[VY] -= 8 * stepv[VY];
                d[VZ] -= 8 * stepv[VZ];
                pos[VX] = tracePos[VX] + d[VX];
                pos[VY] = tracePos[VY] + d[VY];
                pos[VZ] = tracePos[VZ] + d[VZ];
                contact = Sector_AtPoint_FixedPrecision(pos);
            }

            // Should we backtrack to hit a plane instead?
            coord_t cTop    = cCeil - 4;
            coord_t cBottom = cFloor + 4;
            int divisor     = 2;

            // We must not hit a sky plane.
            if(pos[VZ] > cTop &&
               (P_GetIntp(P_GetPtrp(contact, DMU_CEILING_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
            {
                return true;
            }

            if(pos[VZ] < cBottom &&
               (P_GetIntp(P_GetPtrp(contact, DMU_FLOOR_MATERIAL),
                            DMU_FLAGS) & MATF_SKYMASK))
            {
                return true;
            }

            // Find the approximate hitpoint by stepping back and
            // forth using smaller and smaller steps.
            while((pos[VZ] > cTop || pos[VZ] < cBottom) && divisor <= 128)
            {
                // We aren't going to hit a line any more.
                lineWasHit = false;

                // Take a step backwards.
                pos[VX] -= d[VX] / divisor;
                pos[VY] -= d[VY] / divisor;
                pos[VZ] -= d[VZ] / divisor;

                // Divisor grows.
                divisor *= 2;

                // Can we get any closer?
                if(FEQUAL(d[VZ] / divisor, 0))
                {
                    break; // No.
                }

                // Move forward until limits breached.
                while((d[VZ] > 0 && pos[VZ] <= cTop) ||
                      (d[VZ] < 0 && pos[VZ] >= cBottom))
                {
                    pos[VX] += d[VX] / divisor;
                    pos[VY] += d[VY] / divisor;
                    pos[VZ] += d[VZ] / divisor;
                }
            }
        }

        // Spawn bullet puffs.
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually
            // hits the line.
            XL_ShootLine(li, 0, shootThing);
        }

        /*if(lineWasHit)
        {
            Con_Message("Hit line [%i,%i]", P_GetIntp(li, DMU_FRONT), P_GetIntp(li, DMU_BACK));
        }*/
#endif
        // Don't go any farther.
        return true;
    }

    // Intercepted a mobj.
    mobj_t *th = in->mobj;

    if(th == shootThing) return false; // Can't shoot self.
    if(!(th->flags & MF_SHOOTABLE)) return false; // Corpse or something.

#if __JHERETIC__
    // Check for physical attacks on a ghost.
    if((th->flags & MF_SHADOW) && shootThing->player->readyWeapon == WT_FIRST)
    {
        return false;
    }
#endif

    // Check angles to see if the thing can be aimed at
    coord_t dist = attackRange * in->distance;
    coord_t dz   = th->origin[VZ];
    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
    {
        dz += th->height;
    }
    dz -= tracePos[VZ];

    coord_t thingTopSlope = dz / dist;
    if(thingTopSlope < aimSlope)
    {
        return false; // Shot over the thing.
    }

    coord_t thingBottomSlope = (th->origin[VZ] - tracePos[VZ]) / dist;
    if(thingBottomSlope > aimSlope)
    {
        return false; // Shot under the thing.
    }

    // Hit thing.

    // Position a bit closer.
    coord_t frac = in->distance - (10 / attackRange);
    vec3d_t pos  = { tracePos[VX] + FIX2FLT(Interceptor_Direction(trace)[VX]) * frac,
                     tracePos[VY] + FIX2FLT(Interceptor_Direction(trace)[VY]) * frac,
                     tracePos[VZ] + aimSlope * (frac * attackRange) };

    // Spawn bullet puffs or blood spots, depending on target type.
#if __JHERETIC__
    if(puffType == MT_BLASTERPUFF1)
    {
        // Make blaster big puff.
        if(mobj_t *mo = P_SpawnMobj(MT_BLASTERPUFF2, pos, P_Random() << 24, 0))
        {
            S_StartSound(SFX_BLSHIT, mo);
        }
    }
    else
    {
        P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
    }
#elif __JHEXEN__
    P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
#endif

    if(lineAttackDamage)
    {
#if __JDOOM__ || __JDOOM64__
        angle_t attackAngle = M_PointToAngle2(shootThing->origin, pos);
#endif

        int damageDone;
#if __JHEXEN__
        if(PuffType == MT_FLAMEPUFF2)
        {
            // Cleric FlameStrike does fire damage.
            damageDone = P_DamageMobj(th, &lavaInflictor, shootThing,
                                      lineAttackDamage, false);
        }
        else
#endif
        {
            damageDone = P_DamageMobj(th, shootThing, shootThing,
                                      lineAttackDamage, false);
        }

#if __JHEXEN__
        if(!(in->mobj->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(in->mobj->flags & MF_NOBLOOD))
            {
                if(damageDone > 0)
                {
                    // Damage was inflicted, so shed some blood.
#if __JDOOM__ || __JDOOM64__
                    P_SpawnBlood(pos[VX], pos[VY], pos[VZ], lineAttackDamage,
                                 attackAngle + ANG180);
#else
# if __JHEXEN__
                    if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
                    {
                        P_SpawnBloodSplatter2(pos[VX], pos[VY], pos[VZ], in->mobj);
                    }
                    else
# endif
                    if(P_Random() < 192)
                    {
                        P_SpawnBloodSplatter(pos[VX], pos[VY], pos[VZ], in->mobj);
                    }
#endif
                }
            }
#if __JDOOM__ || __JDOOM64__
            else
            {
                P_SpawnPuff(pos[VX], pos[VY], pos[VZ], P_Random() << 24);
            }
#endif
        }
    }

    // Don't go any farther.
    return true;
}

/**
 * Sets linetarget and aimSlope when a target is aimed at.
 */
int PTR_AimTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    vec3d_t const tracePos = {
        FIX2FLT(Interceptor_Origin(trace)[VX]), FIX2FLT(Interceptor_Origin(trace)[VY]), shootZ
    };

    if(in->type == ICPT_LINE)
    {
        Line *li = in->line;
        Sector *backSec, *frontSec;

        if(!(P_ToXLine(li)->flags & ML_TWOSIDED) ||
           !(frontSec = (Sector *)P_GetPtrp(li, DMU_FRONT_SECTOR)) ||
           !(backSec  = (Sector *)P_GetPtrp(li, DMU_BACK_SECTOR)))
        {
            return !(Line_PointOnSide(li, tracePos) < 0);
        }

        // Crosses a two sided line.
        // A two sided line will restrict the possible target ranges.
        Interceptor_AdjustOpening(trace, li);

        if(Interceptor_Opening(trace)->bottom >= Interceptor_Opening(trace)->top)
        {
            return true; // Stop.
        }

        coord_t dist   = attackRange * in->distance;
        coord_t fFloor = P_GetDoublep(frontSec, DMU_FLOOR_HEIGHT);
        coord_t fCeil  = P_GetDoublep(frontSec, DMU_CEILING_HEIGHT);
        coord_t bFloor = P_GetDoublep(backSec, DMU_FLOOR_HEIGHT);
        coord_t bCeil  = P_GetDoublep(backSec, DMU_CEILING_HEIGHT);

        coord_t slope;
        if(!FEQUAL(fFloor, bFloor))
        {
            slope = (Interceptor_Opening(trace)->bottom - shootZ) / dist;
            if(slope > bottomSlope)
                bottomSlope = slope;
        }

        if(!FEQUAL(fCeil, bCeil))
        {
            slope = (Interceptor_Opening(trace)->top - shootZ) / dist;
            if(slope < topSlope)
                topSlope = slope;
        }

        return topSlope <= bottomSlope;
    }

    // Intercepted a mobj.

    mobj_t *th = in->mobj;

    if(th == shootThing) return false; // Can't shoot self.
    if(!(th->flags & MF_SHOOTABLE)) return false; // Corpse or something?
#if __JHERETIC__
    if(th->type == MT_POD) return false; // Can't auto-aim at pods.
#endif

#if __JDOOM__ || __JHEXEN__ || __JDOOM64__
    if(th->player && IS_NETGAME && !deathmatch)
    {
        return false; // Don't aim at fellow co-op players.
    }
#endif

    // Check angles to see if the thing can be aimed at.
    coord_t dist = attackRange * in->distance;
    coord_t posZ = th->origin[VZ];

    if(!(th->player && (th->player->plr->flags & DDPF_CAMERA)))
    {
        posZ += th->height;
    }

    coord_t thingTopSlope = (posZ - shootZ) / dist;
    if(thingTopSlope < bottomSlope)
    {
        return false; // Shot over the thing.
    }

    // Too far below?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(posZ < shootZ - attackRange / 1.2f)
    {
        return false;
    }
#endif

    coord_t thingBottomSlope = (th->origin[VZ] - shootZ) / dist;
    if(thingBottomSlope > topSlope)
    {
        return false; // Shot under the thing.
    }

    // Too far above?
    // $addtocfg $limitautoaimZ:
#if __JHEXEN__
    if(th->origin[VZ] > shootZ + attackRange / 1.2f)
    {
        return false;
    }
#endif

    // This thing can be hit!
    if(thingTopSlope > topSlope)
    {
        thingTopSlope = topSlope;
    }
    if(thingBottomSlope < bottomSlope)
    {
        thingBottomSlope = bottomSlope;
    }

    aimSlope = (thingTopSlope + thingBottomSlope) / 2;
    lineTarget = th;

    return true; // Don't go any farther.
}

float P_AimLineAttack(mobj_t *t1, angle_t angle, coord_t distance)
{
    uint an = angle >> ANGLETOFINESHIFT;
    vec2d_t target = { t1->origin[VX] + distance * FIX2FLT(finecosine[an]),
                       t1->origin[VY] + distance * FIX2FLT(finesine[an]) };

    // Determine the z trace origin.
    shootZ = t1->origin[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class_ == PCLASS_FIGHTER ||
       t1->player->class_ == PCLASS_CLERIC ||
       t1->player->class_ == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            shootZ += (cfg.plrViewHeight - 5);
    }
    else
    {
        shootZ += (t1->height / 2) + 8;
    }

    topSlope    = 100.0/160;
    bottomSlope = -100.0/160;
    attackRange = distance;
    lineTarget  = NULL;
    shootThing  = t1;

    P_PathTraverse(t1->origin, target, PTR_AimTraverse, 0);

    if(lineTarget)
    {
        // While autoaiming, we accept this slope.
        if(!t1->player || !cfg.noAutoAim)
            return aimSlope;
    }

    if(t1->player && cfg.noAutoAim)
    {
        // The slope is determined by lookdir.
        return tan(LOOKDIR2RAD(t1->dPlayer->lookDir)) / 1.2;
    }

    return 0;
}

/**
 * If damage == 0, it is just a test trace that will leave lineTarget set.
 */
void P_LineAttack(mobj_t* t1, angle_t angle, coord_t distance, coord_t slope, int damage)
{
    uint an = angle >> ANGLETOFINESHIFT;
    vec2d_t target = { t1->origin[VX] + distance * FIX2FLT(finecosine[an]),
                       t1->origin[VY] + distance * FIX2FLT(finesine[an]) };

    // Determine the z trace origin.
    shootZ = t1->origin[VZ];
#if __JHEXEN__
    if(t1->player &&
      (t1->player->class_ == PCLASS_FIGHTER ||
       t1->player->class_ == PCLASS_CLERIC ||
       t1->player->class_ == PCLASS_MAGE))
#else
    if(t1->player && t1->type == MT_PLAYER)
#endif
    {
        if(!(t1->player->plr->flags & DDPF_CAMERA))
            shootZ += cfg.plrViewHeight - 5;
    }
    else
    {
        shootZ += (t1->height / 2) + 8;
    }
    shootZ -= t1->floorClip;

    shootThing       = t1;
    lineAttackDamage = damage;
    attackRange      = distance;
    aimSlope         = slope;

    if(!P_PathTraverse(t1->origin, target, PTR_ShootTraverse, 0))
    {
#if __JHEXEN__
        switch(PuffType)
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
            P_SpawnPuff(target[VX], target[VY],
                        shootZ + (slope * distance), P_Random() << 24);
            break;

        default:
            break;
        }
#endif
    }
}

/**
 * "bombSource" is the creature that caused the explosion at "bombSpot".
 */
int PIT_RadiusAttack(mobj_t *thing, void * /*context*/)
{
    if(!(thing->flags & MF_SHOOTABLE))
        return false;

    // Boss spider and cyborg take no damage from concussion.
#if __JHERETIC__
    if(thing->type == MT_MINOTAUR) return false;
    if(thing->type == MT_SORCERER1) return false;
    if(thing->type == MT_SORCERER2) return false;
#elif __JDOOM__ || __JDOOM64__
    if(thing->type == MT_CYBORG) return false;
# if __JDOOM__
    if(thing->type == MT_SPIDER) return false;
# endif
#endif

#if __JHEXEN__
    if(!damageSource && thing == bombSource) // Don't damage the source of the explosion.
        return false;
#endif

    coord_t dx = fabs(thing->origin[VX] - bombSpot->origin[VX]);
    coord_t dy = fabs(thing->origin[VY] - bombSpot->origin[VY]);
    coord_t dz = fabs((thing->origin[VZ] + thing->height / 2) - bombSpot->origin[VZ]);

    coord_t dist = (dx > dy? dx : dy);
#if __JHEXEN__
    if(!cfg.netNoMaxZRadiusAttack)
    {
        dist = (dz > dist? dz : dist);
    }
#else
    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
    {
        dist = (dz > dist? dz : dist);
    }
#endif

    dist = MAX_OF(dist - thing->radius, 0);
    if(dist >= bombDistance)
    {
        return false; // Out of range.
    }

    // Must be in direct path.
    if(P_CheckSight(thing, bombSpot))
    {
        int damage = (bombDamage * (bombDistance - dist) / bombDistance) + 1;
#if __JHEXEN__
        if(thing->player) damage /= 4;
#endif

        P_DamageMobj(thing, bombSpot, bombSource, damage, false);
    }

    return false;
}

#if __JHEXEN__
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance, boolean canDamageSource)
#else
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance)
#endif
{
    coord_t dist = distance + MAXRADIUS;
    AABoxd box(spot->origin[VX] - dist, spot->origin[VY] - dist,
               spot->origin[VX] + dist, spot->origin[VY] + dist);

#if __JHEXEN__
    damageSource = canDamageSource;
#endif
    bombSpot     = spot;
    bombDamage   = damage;
    bombDistance = distance;
#if __JHERETIC__
    if(spot->type == MT_POD && spot->target)
    {
        bombSource = spot->target;
    }
    else
#endif
    {
        bombSource = source;
    }

    VALIDCOUNT++;
    Mobj_BoxIterator(&box, PIT_RadiusAttack, 0);
}

int PTR_UseTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    if(in->type != ICPT_LINE)
        return false; // Continue iteration.

    xline_t *xline = P_ToXLine(in->line);
    if(!xline->special)
    {
        Interceptor_AdjustOpening(trace, in->line);

        if(Interceptor_Opening(trace)->range <= 0)
        {
            if(useThing->player)
            {
                S_StartSound(PCLASS_INFO(useThing->player->class_)->failUseSound, useThing);
            }

            return true; // Can't use through a wall.
        }

#if __JHEXEN__
        if(useThing->player)
        {
            coord_t pheight = useThing->origin[VZ] + useThing->height/2;

            if(Interceptor_Opening(trace)->top < pheight || Interceptor_Opening(trace)->bottom > pheight)
            {
                S_StartSound(PCLASS_INFO(useThing->player->class_)->failUseSound, useThing);
            }
        }
#endif
        // Not a special line, but keep checking.
        return false;
    }

    int side = Line_PointOnSide(in->line, useThing->origin) < 0;

#if __JHERETIC__ || __JHEXEN__
    if(side == 1) return true; // Don't use back side.
#endif

    P_ActivateLine(in->line, useThing, side, SPAC_USE);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Can use multiple line specials in a row with the PassThru flag.
    if(xline->flags & ML_PASSUSE) return false;
#endif

    // Can't use more than one special line in a row.
    return true;
}

void P_UseLines(player_t *player)
{
    if(IS_CLIENT)
    {
#ifdef _DEBUG
        Con_Message("P_UseLines: Sending a use request for player %i.", int(player - players));
#endif
        NetCl_PlayerActionRequest(player, GPA_USE, 0);
        return;
    }

    mobj_t *mo  = mo = player->plr->mo;
    uint an     = mo->angle >> ANGLETOFINESHIFT;
    vec2d_t pos = { mo->origin[VX] + USERANGE * FIX2FLT(finecosine[an]),
                    mo->origin[VY] + USERANGE * FIX2FLT(finesine  [an]) };

    useThing = mo;

    P_PathTraverse2(mo->origin, pos, PTF_LINE, PTR_UseTraverse, 0);
}

/**
 * Takes a valid thing and adjusts the thing->floorZ, thing->ceilingZ,
 * and possibly thing->origin[VZ].
 *
 * This is called for all nearby monsters whenever a sector changes height
 * If the thing doesn't fit, the z will be set to the lowest value and
 * false will be returned
 *
 * @param thing  The mobj whoose position to adjust.
 *
 * @return  @c true iff the thing did fit.
 */
static boolean P_ThingHeightClip(mobj_t *thing)
{
    // Don't height clip cameras.
    if(P_MobjIsCamera(thing)) return false;

    bool const onfloor = (thing->origin[VZ] == thing->floorZ);
    P_CheckPosition(thing, thing->origin);

    thing->floorZ = tmFloorZ;
    thing->ceilingZ = tmCeilingZ;
#if !__JHEXEN__
    thing->dropOffZ = tmDropoffZ; // $dropoff_fix: remember dropoffs.
#endif

    if(onfloor)
    {
#if __JHEXEN__
        if((thing->origin[VZ] - thing->floorZ < 9) ||
           (thing->flags & MF_NOGRAVITY))
        {
            thing->origin[VZ] = thing->floorZ;
        }
#else
        // Update view offset of real players $voodoodolls.
        if(thing->player && thing->player->plr->mo == thing)
            thing->player->viewZ += thing->floorZ - thing->origin[VZ];

        // Walking monsters rise and fall with the floor.
        thing->origin[VZ] = thing->floorZ;

        // $dropoff_fix: Possibly upset balance of objects hanging off ledges.
        if((thing->intFlags & MIF_FALLING) && thing->gear >= MAXGEAR)
            thing->gear = 0;
#endif
    }
    else
    {
        // Don't adjust a floating monster unless forced to.
        if(thing->origin[VZ] + thing->height > thing->ceilingZ)
            thing->origin[VZ] = thing->ceilingZ - thing->height;
    }

    return (thing->ceilingZ - thing->floorZ) >= thing->height;
}

/**
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param line  The line being slid along.
 */
static void P_HitSlideLine(Line *line)
{
    DENG_ASSERT(line != 0);

    slopetype_t slopeType = slopetype_t(P_GetIntp(line, DMU_SLOPETYPE));
    if(slopeType == ST_HORIZONTAL)
    {
        tmMove[MY] = 0;
        return;
    }
    else if(slopeType == ST_VERTICAL)
    {
        tmMove[MX] = 0;
        return;
    }

    bool side = Line_PointOnSide(line, slideMo->origin) < 0;
    vec2d_t d1; P_GetDoublepv(line, DMU_DXY, d1);

    angle_t moveAngle = M_PointXYToAngle2(0, 0, tmMove[MX], tmMove[MY]);
    angle_t lineAngle = M_PointXYToAngle2(0, 0, d1[0], d1[1]) + (side? ANG180 : 0);

    angle_t deltaAngle = moveAngle - lineAngle;
    if(deltaAngle > ANG180) deltaAngle += ANG180;

    coord_t moveLen = M_ApproxDistance(tmMove[MX], tmMove[MY]);
    coord_t newLen  = moveLen * FIX2FLT(finecosine[deltaAngle >> ANGLETOFINESHIFT]);

    uint an = lineAngle >> ANGLETOFINESHIFT;
    V2d_Set(tmMove, newLen * FIX2FLT(finecosine[an]),
                    newLen * FIX2FLT(finesine  [an]));
}

int PTR_SlideTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    DENG_ASSERT(in->type == ICPT_LINE);

    Line *line = in->line;
    if(!(P_ToXLine(line)->flags & ML_TWOSIDED) ||
       !P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR))
    {
        if(Line_PointOnSide(line, slideMo->origin) < 0)
        {
            return false; // Don't hit the back side.
        }

        goto isblocking;
    }

#if __JDOOM64__
    if(P_ToXLine(line)->flags & ML_BLOCKALL) // jd64
        goto isblocking;
#endif

    Interceptor_AdjustOpening(trace, line);

    if(Interceptor_Opening(trace)->range < slideMo->height)
        goto isblocking; // Doesn't fit.

    if(Interceptor_Opening(trace)->top - slideMo->origin[VZ] < slideMo->height)
        goto isblocking; // mobj is too high.

    if(Interceptor_Opening(trace)->bottom - slideMo->origin[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return false;

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(in->distance < bestSlideDistance)
    {
        secondSlideDistance = bestSlideDistance;
        secondSlideLine     = bestSlideLine;
        bestSlideDistance   = in->distance;
        bestSlideLine       = line;
    }

    return true; // Stop.
}

/**
 * @todo The momx / momy move is bad, so try to slide along a wall.
 * Find the first line hit, move flush to it, and slide along it
 *
 * This is a kludgy mess.
 *
 * @param mo            The mobj to attempt the slide move.
 */
void P_SlideMove(mobj_t *mo)
{
    if(!mo) return; // Huh?

#ifdef _DEBUG
    vec2d_t oldOrigin; V2d_Copy(oldOrigin, mo->origin);
#endif

    int hitCount = 3;
    do
    {
        if(--hitCount == 0)
            goto stairstep; // Don't loop forever.

        // Trace along the three leading corners.
        vec2d_t leadPos = { mo->origin[VX] + (mo->mom[MX] > 0? mo->radius : -mo->radius),
                            mo->origin[VY] + (mo->mom[MY] > 0? mo->radius : -mo->radius) };

        vec2d_t trailPos = { mo->origin[VX] - (mo->mom[MX] > 0? mo->radius : -mo->radius),
                             mo->origin[VY] - (mo->mom[MY] > 0? mo->radius : -mo->radius) };

        slideMo           = mo;
        bestSlideDistance = 1;

        P_PathXYTraverse2(leadPos[VX], leadPos[VY],
                          leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, 0);

        P_PathXYTraverse2(trailPos[VX], leadPos[VY],
                          trailPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, 0);

        P_PathXYTraverse2(leadPos[VX], trailPos[VY],
                          leadPos[VX] + mo->mom[MX], trailPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, 0);

        // Move up to the wall.
        if(bestSlideDistance == 1)
        {
            // The move must have hit the middle, so stairstep. $dropoff_fix
          stairstep:

            /*
             * Ideally we would set the directional momentum of the mobj to zero
             * here should a move fail (to prevent noticeable stuttering against
             * the blocking surface/thing). However due to the mechanics of the
             * wall side algorithm this is not possible as it results in highly
             * unpredictable behaviour and resulting in the player sling-shoting
             * away from the wall.
             */
#if __JHEXEN__
            if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY] + mo->mom[MY]))
                P_TryMoveXY(mo, mo->origin[VX] + mo->mom[MX], mo->origin[VY]);
#else
            if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY] + mo->mom[MY], true, true))
                P_TryMoveXY(mo, mo->origin[VX] + mo->mom[MX], mo->origin[VY], true, true);
#endif
            break;
        }

        // Fudge a bit to make sure it doesn't hit.
        bestSlideDistance -= (1.0f / 32);
        if(bestSlideDistance > 0)
        {
            vec2d_t newPos = { mo->origin[VX] + mo->mom[MX] * bestSlideDistance,
                               mo->origin[VY] + mo->mom[MY] * bestSlideDistance };

            // $dropoff_fix: Allow objects to drop off ledges
#if __JHEXEN__
            if(!P_TryMoveXY(mo, newPos[VX], newPos[VY]))
#else
            if(!P_TryMoveXY(mo, newPos[VX], newPos[VY], true, true))
#endif
            {
                goto stairstep;
            }
        }

        // Now continue along the wall.
        // First calculate remainder.
        bestSlideDistance = MIN_OF(1 - (bestSlideDistance + (1.0f / 32)), 1);
        if(bestSlideDistance <= 0)
        {
            break;
        }

        V2d_Set(tmMove, mo->mom[VX] * bestSlideDistance,
                        mo->mom[VY] * bestSlideDistance);

        P_HitSlideLine(bestSlideLine); // Clip the move.

        V2d_Copy(mo->mom, tmMove);

    // $dropoff_fix: Allow objects to drop off ledges:
#if __JHEXEN__
    } while(!P_TryMoveXY(mo, mo->origin[VX] + tmMove[MX],
                             mo->origin[VY] + tmMove[MY]));
#else
    } while(!P_TryMoveXY(mo, mo->origin[VX] + tmMove[MX],
                             mo->origin[VY] + tmMove[MY], true, true));
#endif

#ifdef _DEBUG
    // Didn't move?
    if(mo->player && mo->origin[VX] == oldOrigin[VX] && mo->origin[VY] == oldOrigin[VY])
    {
        Con_Message("P_SlideMove: Mobj pos stays the same.");
    }
#endif
}

/**
 * SECTOR HEIGHT CHANGING
 * After modifying a sectors floor or ceiling height, call this routine
 * to adjust the positions of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed.
 * If Crunch is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes.
 */

/**
 * @param thing         The thing to check against height changes.
 * @param data          Unused.
 */
int PIT_ChangeSector(mobj_t *thing, void * /*context*/)
{
    if(!thing->info) return false; // Invalid thing?

    // Don't check things that aren't blocklinked (supposedly immaterial).
    if(thing->info->flags & MF_NOBLOCKMAP)
    {
        return false;
    }

    if(P_ThingHeightClip(thing))
    {
        return false; // Keep checking...
    }

    // Crunch bodies to giblets.
#if __JDOOM__ || __JDOOM64__
    if(thing->health <= 0 && (cfg.gibCrushedNonBleeders || !(thing->flags & MF_NOBLOOD)))
#elif __JHEXEN__
    if(thing->health <= 0 && (thing->flags & MF_CORPSE))
#else
    if(thing->health <= 0)
#endif
    {
#if __JHEXEN__
        if(thing->flags & MF_NOBLOOD)
        {
            P_MobjRemove(thing, false);
        }
        else
        {
            if(thing->state != &STATES[S_GIBS1])
            {
                P_MobjChangeState(thing, S_GIBS1);
                thing->height = 0;
                thing->radius = 0;
                S_StartSound(SFX_PLAYER_FALLING_SPLAT, thing);
            }
        }
#else
# if __JDOOM64__
        S_StartSound(SFX_SLOP, thing);
# endif

# if __JDOOM__ || __JDOOM64__
        P_MobjChangeState(thing, S_GIBS);
# endif
        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;
#endif
        return false; // Keep checking...
    }

    // Crunch dropped items.
#if __JHEXEN__
    if(thing->flags2 & MF2_DROPPED)
#else
    if(thing->flags & MF_DROPPED)
#endif
    {
        P_MobjRemove(thing, false);
        return false; // Keep checking...
    }

    if(!(thing->flags & MF_SHOOTABLE))
    {
        return false; // Keep checking...
    }

    noFit = true;

    if(crushChange > 0 && !(mapTime & 3))
    {
#if __JHEXEN__
        P_DamageMobj(thing, NULL, NULL, crushChange, false);
#else
        P_DamageMobj(thing, NULL, NULL, 10, false);
#endif

#if __JDOOM__ || __JDOOM64__
        if(!(thing->flags & MF_NOBLOOD))
#elif __JHEXEN__
        if(!(thing->flags & MF_NOBLOOD) &&
           !(thing->flags2 & MF2_INVULNERABLE))
#endif
        {
            // Spray blood in a random direction.
            if(mobj_t *mo = P_SpawnMobjXYZ(MT_BLOOD, thing->origin[VX], thing->origin[VY],
                                           thing->origin[VZ] + (thing->height /2),
                                           P_Random() << 24, 0))
            {
                mo->mom[MX] = FIX2FLT((P_Random() - P_Random()) << 12);
                mo->mom[MY] = FIX2FLT((P_Random() - P_Random()) << 12);
            }
        }
    }

    return false; // Keep checking (crush other things)...
}

boolean P_ChangeSector(Sector *sector, boolean crunch)
{
    noFit       = false;
    crushChange = crunch;

    VALIDCOUNT++;
    Sector_TouchingMobjsIterator(sector, PIT_ChangeSector, 0);

    return noFit;
}

void P_HandleSectorHeightChange(int sectorIdx)
{
    P_ChangeSector((Sector *)P_ToPtr(DMU_SECTOR, sectorIdx), false);
}

#if __JHERETIC__ || __JHEXEN__
boolean P_TestMobjLocation(mobj_t *mo)
{
    int const oldFlags = mo->flags;

    mo->flags &= ~MF_PICKUP;
    if(!P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]))
    {
        mo->flags = oldFlags;
        return false;
    }
    mo->flags = oldFlags;

    // XY is ok, now check Z
    return mo->origin[VZ] >= mo->floorZ && (mo->origin[VZ] + mo->height) <= mo->ceilingZ;
}
#endif

#if __JDOOM64__ || __JHERETIC__
static void CheckMissileImpact(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);

    if(IS_CLIENT) return;
    if(!mo->target || !mo->target->player || !(mo->flags & MF_MISSILE)) return;
    if(IterList_Empty(spechit)) return;

    IterList_SetIteratorDirection(spechit, ITERLIST_BACKWARD);
    IterList_RewindIterator(spechit);

    Line *line;
    while(line = (Line *)IterList_MoveIterator(spechit))
    {
        P_ActivateLine(line, mo->target, 0, SPAC_IMPACT);
    }
}
#endif

#if __JHEXEN__
int PIT_ThrustStompThing(mobj_t *thing, void * /*context*/)
{
    if(!(thing->flags & MF_SHOOTABLE))
        return false;

    coord_t blockdist = thing->radius + tsThing->radius;
    if(fabs(thing->origin[VX] - tsThing->origin[VX]) >= blockdist ||
       fabs(thing->origin[VY] - tsThing->origin[VY]) >= blockdist ||
       (thing->origin[VZ] > tsThing->origin[VZ] + tsThing->height))
        return false; // Didn't hit it.

    if(thing == tsThing)
        return false; // Don't clip against self.

    P_DamageMobj(thing, tsThing, tsThing, 10001, false);
    tsThing->args[1] = 1; // Mark thrust thing as bloody.

    return false;
}

// Stomp on any things contacted.
void PIT_ThrustSpike(mobj_t *actor)
{
    coord_t radius = actor->info->radius + MAXRADIUS;
    AABoxd box(actor->origin[VX] - radius, actor->origin[VY] - radius,
               actor->origin[VX] + radius, actor->origin[VY] + radius);

    // We are the stomper.
    tsThing = actor;

    VALIDCOUNT++;
    Mobj_BoxIterator(&box, PIT_ThrustStompThing, 0);
}

int PIT_CheckOnmobjZ(mobj_t* thing, void * /*context*/)
{
    if(!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return false; // Can't hit thing.

    coord_t blockdist = thing->radius + tmThing->radius;
    if(fabs(thing->origin[VX] - tm[VX]) >= blockdist ||
       fabs(thing->origin[VY] - tm[VY]) >= blockdist)
        return false; // Didn't hit thing.

    if(thing == tmThing)
        return false; // Don't clip against self.

    if(tmThing->origin[VZ] > thing->origin[VZ] + thing->height)
        return false;
    else if(tmThing->origin[VZ] + tmThing->height < thing->origin[VZ])
        return false; // Under thing.

    if(IS_CLIENT)
    {
        // Players cannot hit their clmobjs.
        if(tmThing->player)
        {
            if(thing == ClPlayer_ClMobj(tmThing->player - players))
                return false;
        }
    }

    if(thing->flags & MF_SOLID)
        onMobj = thing;

    return (thing->flags & MF_SOLID) != 0;
}

mobj_t *P_CheckOnMobj(mobj_t *mo)
{
    if(!mo || Mobj_IsPlayerClMobj(mo))
    {
        // Players' clmobjs shouldn't do any on-mobj logic; the real player mobj
        // will interact with (cl)mobjs.
        return 0;
    }

    mobj_t oldMo = *mo; // Save the old mobj before the fake z movement.
    /// @todo Do this properly! Consolidate with how jDoom/jHeretic do on-mobj checks?

    tmThing = mo;
    P_FakeZMovement(tmThing);

    V3d_Copy(tm, mo->origin);
    tmBox = AABoxd(mo->origin[VX] - tmThing->radius, mo->origin[VY] - tmThing->radius,
                   mo->origin[VX] + tmThing->radius, mo->origin[VY] + tmThing->radius);

    ceilingLine = floorLine = 0;

    // The base floor/ceiling is from the BSP leaf that contains the point.
    // Any contacted lines the step closer together will adjust them.

    Sector *newSector = Sector_AtPoint_FixedPrecision(mo->origin);

    tmFloorZ        = tmDropoffZ = P_GetDoublep(newSector, DMU_FLOOR_HEIGHT);
    tmCeilingZ      = P_GetDoublep(newSector, DMU_CEILING_HEIGHT);
    tmFloorMaterial = (Material *)P_GetPtrp(newSector, DMU_FLOOR_MATERIAL);

    IterList_Clear(spechit);

    if(!(tmThing->flags & MF_NOCLIP))
    {
        // Check things first, possibly picking things up the bounding box is
        // extended by MAXRADIUS because mobj_ts are grouped into mapblocks
        // based on their origin point, and can overlap into adjacent blocks by
        // up to MAXRADIUS.

        AABoxd tmBoxExpanded(tmBox.minX - MAXRADIUS, tmBox.minY - MAXRADIUS,
                             tmBox.maxX + MAXRADIUS, tmBox.maxY + MAXRADIUS);

        VALIDCOUNT++;
        if(Mobj_BoxIterator(&tmBoxExpanded, PIT_CheckOnmobjZ, 0))
        {
            *tmThing = oldMo;
            return onMobj;
        }
    }

    *tmThing = oldMo;
    return 0;
}

/**
 * Fake the zmovement so that we can check if a move is legal.
 */
static void P_FakeZMovement(mobj_t *mo)
{
    if(P_MobjIsCamera(mo))
        return;

    // Adjust height.
    mo->origin[VZ] += mo->mom[MZ];
    if((mo->flags & MF_FLOAT) && mo->target)
    {
        // Float down towards target if too close.
        if(!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            coord_t dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                            mo->origin[VY] - mo->target->origin[VY]);

            coord_t delta = mo->target->origin[VZ] + (mo->height / 2) - mo->origin[VZ];

            if(delta < 0 && dist < -(delta * 3))
                mo->origin[VZ] -= FLOATSPEED;
            else if(delta > 0 && dist < (delta * 3))
                mo->origin[VZ] += FLOATSPEED;
        }
    }

    if(mo->player && (mo->flags2 & MF2_FLY) && !(mo->origin[VZ] <= mo->floorZ) &&
       (mapTime & 2))
    {
        mo->origin[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement.
    if(mo->origin[VZ] <= mo->floorZ) // Hit the floor.
    {
        mo->origin[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something

        if(P_GetState(mobjtype_t(mo->type), SN_CRASH) && (mo->flags & MF_CORPSE))
            return;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(FEQUAL(mo->mom[MZ], 0))
            mo->mom[MZ] = -(P_GetGravity() / 32) * 2;
        else
            mo->mom[MZ] -= P_GetGravity() / 32;
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(FEQUAL(mo->mom[MZ], 0))
            mo->mom[MZ] = -P_GetGravity() * 2;
        else
            mo->mom[MZ] -= P_GetGravity();
    }

    if(mo->origin[VZ] + mo->height > mo->ceilingZ) // Hit the ceiling.
    {
        mo->origin[VZ] = mo->ceilingZ - mo->height;

        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        if(mo->flags & MF_SKULLFLY)
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something.
    }
}

static void checkForPushSpecial(Line *line, int side, mobj_t *mobj)
{
    if(P_ToXLine(line)->special)
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

int PTR_BounceTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    DENG_ASSERT(in->type == ICPT_LINE);

    Line *line = in->line;
    if(!P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR))
    {
        if(Line_PointOnSide(line, slideMo->origin) < 0)
            return false; // Don't hit the back side.

        goto bounceblocking;
    }

    Interceptor_AdjustOpening(trace, line);

    if(Interceptor_Opening(trace)->range < slideMo->height)
        goto bounceblocking; // Doesn't fit.

    if(Interceptor_Opening(trace)->top - slideMo->origin[VZ] < slideMo->height)
        goto bounceblocking; // Mobj is too high...

    return false; // This line doesn't block movement...

    // the line does block movement, see if it is closer than best so far.
  bounceblocking:
    if(in->distance < bestSlideDistance)
    {
        secondSlideDistance = bestSlideDistance;
        secondSlideLine     = bestSlideLine;
        bestSlideDistance   = in->distance;
        bestSlideLine       = line;
    }

    return true; // Stop.
}

void P_BounceWall(mobj_t *mo)
{
    if(!mo) return;

    // Trace a line from the origin to the would be destination point (which is
    // apparently not reachable) to find a line from which we'll calculate the
    // inverse "bounce" vector.
    vec2d_t leadPos = { mo->origin[VX] + (mo->mom[MX] > 0? mo->radius : -mo->radius),
                        mo->origin[VY] + (mo->mom[MY] > 0? mo->radius : -mo->radius) };
    vec2d_t destPos; V2d_Sum(destPos, leadPos, mo->mom);

    slideMo           = mo;
    bestSlideLine     = 0;
    bestSlideDistance = 1;

    P_PathTraverse2(leadPos, destPos, PTF_LINE, PTR_BounceTraverse, 0);

    if(bestSlideLine)
    {
        int const side = Line_PointOnSide(bestSlideLine, mo->origin) < 0;
        vec2d_t lineDirection; P_GetDoublepv(bestSlideLine, DMU_DXY, lineDirection);

        angle_t lineAngle  = M_PointToAngle(lineDirection) + (side? ANG180 : 0);
        angle_t moveAngle  = M_PointToAngle(mo->mom);
        angle_t deltaAngle = (2 * lineAngle) - moveAngle;

        coord_t moveLen = M_ApproxDistance(mo->mom[MX], mo->mom[MY]) * 0.75f /*Friction*/;
        if(moveLen < 1) moveLen = 2;

        uint an = deltaAngle >> ANGLETOFINESHIFT;
        V2d_Set(mo->mom, moveLen * FIX2FLT(finecosine[an]),
                         moveLen * FIX2FLT(finesine  [an]));
    }
}

int PTR_PuzzleItemTraverse(Interceptor *trace, intercept_t const *in, void * /*context*/)
{
    switch(in->type)
    {
    case ICPT_LINE: { // Line.
        Line *line = in->line;
        xline_t *xline = P_ToXLine(line);

        if(xline->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            Interceptor_AdjustOpening(trace, line);

            if(Interceptor_Opening(trace)->range <= 0)
            {
                sfxenum_t sound = SFX_NONE;

                if(puzzleItemUser->player)
                {
                    switch(puzzleItemUser->player->class_)
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

                S_StartSound(sound, puzzleItemUser);
                return true; // Can't use through a wall.
            }

            return false; // Continue searching...
        }

        if(Line_PointOnSide(line, puzzleItemUser->origin) < 0)
            return true; // Don't use back sides.

        if(puzzleItemType != xline->arg1)
            return true; // Item type doesn't match.

        P_StartACS(xline->arg2, 0, &xline->arg3, puzzleItemUser, line, 0);
        xline->special = 0;
        puzzleActivated = true;

        return true; // Stop searching.
        }

    case ICPT_MOBJ: { // Mobj.
        mobj_t *mo = in->mobj;

        if(mo->special != USE_PUZZLE_ITEM_SPECIAL)
            return false; // Wrong special...

        if(puzzleItemType != mo->args[0])
            return false; // Item type doesn't match...

        P_StartACS(mo->args[1], 0, &mo->args[2], puzzleItemUser, NULL, 0);
        mo->special = 0;
        puzzleActivated = true;

        return true; // Stop searching.
        }

    default:
        Con_Error("PTR_PuzzleItemTraverse: Unknown intercept type %i.", in->type);
        exit(1); // Unreachable.
    }
}

boolean P_UsePuzzleItem(player_t *player, int itemType)
{
    DENG_ASSERT(player != 0);

    mobj_t *mobj = player->plr->mo;
    if(!mobj) return false; // Huh?

    puzzleItemType  = itemType;
    puzzleItemUser  = mobj;
    puzzleActivated = false;

    uint an = mobj->angle >> ANGLETOFINESHIFT;
    vec2d_t farUsePoint = { mobj->origin[VX] + FIX2FLT(USERANGE * finecosine[an]),
                            mobj->origin[VY] + FIX2FLT(USERANGE * finesine  [an]) };

    P_PathTraverse(mobj->origin, farUsePoint, PTR_PuzzleItemTraverse, 0);

    if(!puzzleActivated)
    {
        P_SetYellowMessage(player, 0, TXT_USEPUZZLEFAILED);
    }

    return puzzleActivated;
}
#endif
