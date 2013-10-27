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

/*
 * Try move variables:
 */
static AABoxd tmBox;
static mobj_t *tmThing;
boolean tmFloatOk; ///< @c true= move would be ok if within "tmFloorZ - tmCeilingZ".
coord_t tmFloorZ;
coord_t tmCeilingZ;
#if __JHEXEN__
static Material *tmFloorMaterial;
#endif
boolean tmFellDown; // $dropoff_fix
static coord_t tm[3];
static coord_t tmDropoffZ;
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
static Line *tmHitLine;
static int tmUnstuck; ///< $unstuck: used to check unsticking
#endif

// The following is used to keep track of the lines that clip the open
// height range e.g. PIT_CheckLine. They in turn are used with the &unstuck
// logic and to prevent missiles from exploding against sky hack walls.
Line *tmCeilingLine;
Line *tmFloorLine;

/*
 * Line aim/attack variables:
 */
mobj_t *lineTarget; // Who got hit (or NULL).
Line *blockLine; // $unstuck: blocking line

static coord_t attackRange;

mobjtype_t PuffType;
#if __JDOOM__ || __JDOOM64__
boolean PuffNoSpark;
#endif
#if __JHEXEN__
mobj_t *PuffSpawned;
mobj_t *BlockingMobj;
#endif

static coord_t shootZ; ///< Height if not aiming up or down.
static mobj_t *shootThing;

static float aimSlope;
static float topSlope, bottomSlope; ///< Slopes to top and bottom of target.

static byte *rejectMatrix; ///< For fast sight rejection.

coord_t P_GetGravity()
{
    if(cfg.netGravity != -1)
    {
        return (coord_t) cfg.netGravity / 100;
    }
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

boolean P_CheckSight(mobj_t const *beholder, mobj_t const *target)
{
    if(!beholder || !target) return false;

    // If either is unlinked, they can't see each other.
    if(!Mobj_Sector(beholder)) return false;
    if(!Mobj_Sector(target)) return false;

    // Cameramen are invisible.
    if(P_MobjIsCamera(target)) return false;

    // Does a reject table exist and if so, should this line-of-sight fail?
    if(!checkReject(Mobj_Sector(beholder), Mobj_Sector(target)))
    {
        return false;
    }

    // The line-of-sight is from the "eyes" of the beholder.
    vec3d_t from = { beholder->origin[VX], beholder->origin[VY], beholder->origin[VZ] };
    if(!P_MobjIsCamera(beholder))
    {
        from[VZ] += beholder->height + -(beholder->height / 4);
    }

    return P_CheckLineSight(from, target->origin, 0, target->height, 0);
}

struct pit_stompthing_params_t
{
    mobj_t *stompMobj; ///< Mobj doing the stomping.
    vec2d_t location;  ///< Map space point being stomped.
    bool alwaysStomp;  ///< Disable per-type/monster stomp exclussions.
};

/// @return  Non-zero when the first unstompable mobj is found; otherwise @c 0.
int PIT_StompThing(mobj_t *mo, void *context)
{
    pit_stompthing_params_t &parm = *static_cast<pit_stompthing_params_t *>(context);

    // Don't ever attempt to stomp oneself.
    if(mo == parm.stompMobj) return false;
    // ...or non-shootables.
    if(!(mo->flags & MF_SHOOTABLE)) return false;

    if(!parm.alwaysStomp)
    {
        // Is "this" mobj allowed to stomp?
        if(!(parm.stompMobj->flags2 & MF2_TELESTOMP)) return true;
#if __JDOOM64__
        // Monsters don't stomp.
        if(!Mobj_IsPlayer(parm.stompMobj)) return true;
#elif __JDOOM__
        // Monsters only stomp on a boss map.
        if(!Mobj_IsPlayer(parm.stompMobj) && gameMap != 29) return true;
#endif
    }

    // Within stomping distance?
    coord_t const dist = mo->radius + parm.stompMobj->radius;
    if(fabs(mo->origin[VX] - parm.location[VX]) < dist &&
       fabs(mo->origin[VY] - parm.location[VY]) < dist)
    {
        // Stomp!
        P_DamageMobj(mo, parm.stompMobj, parm.stompMobj, 10000, true);
    }

    return false; // Continue iteration.
}

boolean P_TeleportMove(mobj_t *mobj, coord_t x, coord_t y, boolean alwaysStomp)
{
    if(!mobj) return false;

    IterList_Clear(spechit); /// @todo necessary? -ds

    // Attempt to stomp any mobjs in the way.
    pit_stompthing_params_t parm;
    parm.stompMobj = mobj;
    V2d_Set(parm.location, x, y);
    parm.alwaysStomp = CPP_BOOL(alwaysStomp);

    coord_t const dist = mobj->radius + MAXRADIUS;
    AABoxd const box(x - dist, y - dist, x + dist, y + dist);

    VALIDCOUNT++;
    if(Mobj_BoxIterator(&box, PIT_StompThing, &parm))
        return false;

    // The destination is clear.
    P_MobjUnlink(mobj);
    mobj->origin[VX] = parm.location[VX];
    mobj->origin[VY] = parm.location[VY];
    P_MobjLink(mobj);

    mobj->floorZ     = P_GetDoublep(Mobj_Sector(mobj), DMU_FLOOR_HEIGHT);
    mobj->ceilingZ   = P_GetDoublep(Mobj_Sector(mobj), DMU_CEILING_HEIGHT);
#if !__JHEXEN__
    mobj->dropOffZ   = mobj->floorZ;
#endif

    // Reset movement interpolation.
    P_MobjClearSRVO(mobj);

    return true; // Success.
}

void P_TelefragMobjsTouchingPlayers()
{
    for(uint i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = players + i;
        ddplayer_t *ddplr = plr->plr;
        if(!ddplr->inGame) continue;

        P_TeleportMove(ddplr->mo, ddplr->mo->origin[VX], ddplr->mo->origin[VY], true);
    }
}

struct pit_crossline_params_t
{
    mobj_t *crossMobj;   ///< Mobj attempting to cross.
    AABoxd crossAABox;   ///< Bounding box of the trajectory.
    vec2d_t destination; ///< Would-be destination point.
};

int PIT_CrossLine(Line *line, void *context)
{
    pit_crossline_params_t &parm = *static_cast<pit_crossline_params_t *>(context);

    if((P_GetIntp(line, DMU_FLAGS) & DDLF_BLOCKING) ||
       (P_ToXLine(line)->flags & ML_BLOCKMONSTERS) ||
       (!P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR)))
    {
        AABoxd *aaBox = (AABoxd *)P_GetPtrp(line, DMU_BOUNDING_BOX);

        if(!(parm.crossAABox.minX > aaBox->maxX ||
             parm.crossAABox.maxX < aaBox->minX ||
             parm.crossAABox.maxY < aaBox->minY ||
             parm.crossAABox.minY > aaBox->maxY))
        {
            // Line blocks trajectory?
            return    Line_PointOnSide(line, parm.crossMobj->origin) < 0
                   != Line_PointOnSide(line, parm.destination)       < 0;
        }
    }

    return false; // Continue iteration.
}

boolean P_CheckSides(mobj_t *mobj, coord_t x, coord_t y)
{
    /*
     * Check to see if the trajectory crosses a blocking map line.
     *
     * Currently this assumes an infinite line, which is not quite correct.
     * A more correct solution would be to check for an intersection of the
     * trajectory and the line, but that takes longer and probably really isn't
     * worth the effort.
     */
    pit_crossline_params_t parm;
    parm.crossMobj  = mobj;
    parm.crossAABox =
        AABoxd(MIN_OF(mobj->origin[VX], x), MIN_OF(mobj->origin[VY], y),
               MAX_OF(mobj->origin[VX], x), MAX_OF(mobj->origin[VY], y));
    V2d_Set(parm.destination, x, y);

    VALIDCOUNT++;
    return Line_BoxIterator(&parm.crossAABox, LIF_ALL, PIT_CrossLine, &parm);
}

#if __JHEXEN__
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
#endif // __JHEXEN__

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
        if((thing->origin[VZ] > tm[VZ] + tmThing->height) ||
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
    BlockingMobj = thing;
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
        BlockingMobj = NULL;
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
        tmCeilingLine = ld;
#if !__JHEXEN__
        blockLine = ld;
#endif
    }

    if(opening.bottom > tmFloorZ)
    {
        tmFloorZ = opening.bottom;
        tmFloorLine = ld;
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
    thing->onMobj  = 0;
#endif
    thing->wallHit = false;

    tmThing         = thing;
    V3d_Set(tm, x, y, z);
    tmBox           = AABoxd(tm[VX] - tmThing->radius, tm[VY] - tmThing->radius,
                             tm[VX] + tmThing->radius, tm[VY] + tmThing->radius);
#if !__JHEXEN__
    tmHitLine       = 0;
#endif

    // The base floor/ceiling is from the BSP leaf that contains the point.
    // Any contacted lines the step closer together will adjust them.
    Sector *newSector = Sector_AtPoint_FixedPrecision(tm);

    tmCeilingLine     = tmFloorLine = 0;
    tmFloorZ        = tmDropoffZ = P_GetDoublep(newSector, DMU_FLOOR_HEIGHT);
    tmCeilingZ      = P_GetDoublep(newSector, DMU_CEILING_HEIGHT);
#if __JHEXEN__
    tmFloorMaterial = (Material *)P_GetPtrp(newSector, DMU_FLOOR_MATERIAL);
#else
    blockLine       = 0;
    tmUnstuck       = Mobj_IsPlayer(thing) && !Mobj_IsVoodooDoll(thing);
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
        BlockingMobj = 0;
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
    BlockingMobj  = 0;
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

#if __JDOOM64__ || __JHERETIC__
static void checkMissileImpact(mobj_t &mobj)
{
    if(IS_CLIENT) return;

    if(!(mobj.flags & MF_MISSILE)) return;
    if(!mobj.target || !mobj.target->player) return;

    if(IterList_Empty(spechit)) return;

    IterList_SetIteratorDirection(spechit, ITERLIST_BACKWARD);
    IterList_RewindIterator(spechit);

    Line *line;
    while((line = (Line *)IterList_MoveIterator(spechit)) != 0)
    {
        P_ActivateLine(line, mobj.target, 0, SPAC_IMPACT);
    }
}
#endif

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

    // $dropoff_fix: tmFellDown.
    tmFloatOk  = false;
#if !__JHEXEN__
    tmFellDown = false;
#endif

#if __JHEXEN__
    if(!P_CheckPositionXY(thing, x, y))
#else
    if(!P_CheckPositionXYZ(thing, x, y, thing->origin[VZ]))
#endif
    {
#if __JHEXEN__
        if(!BlockingMobj || BlockingMobj->player || !thing->player)
        {
            goto pushline;
        }
        else if(BlockingMobj->origin[VZ] + BlockingMobj->height - thing->origin[VZ] > 24 ||
                (P_GetDoublep(Mobj_Sector(BlockingMobj), DMU_CEILING_HEIGHT) -
                 (BlockingMobj->origin[VZ] + BlockingMobj->height) < thing->height) ||
                (tmCeilingZ - (BlockingMobj->origin[VZ] + BlockingMobj->height) <
                 thing->height))
        {
            goto pushline;
        }
#else
#  if __JHERETIC__
        checkMissileImpact(*thing);
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

        tmFloatOk = true;

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
            !(tmCeilingLine && untouched(tmCeilingLine)) &&
            !(tmFloorLine   && untouched(tmFloorLine)));

        if(tmCeilingZ - tmFloorZ < thing->height)
        {
            return ret; // Doesn't fit.
        }

        // Mobj must lower to fit.
        tmFloatOk = true;
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
                checkMissileImpact(*thing);
# endif
                return ret;
            }
        }
# if __JHERETIC__
        if((thing->flags & MF_MISSILE) && tmFloorZ > thing->origin[VZ])
        {
            checkMissileImpact(*thing);
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
                    tmFellDown = !(thing->flags & MF_NOGRAVITY) && thing->origin[VZ] - floorZ > 24;
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
            checkMissileImpact(*thing);
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
        while((line = (Line *)IterList_Pop(spechit)) != 0)
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
        while((line = (Line *)IterList_MoveIterator(spechit)) != 0)
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

static mobj_t *spawnPuff(coord_t x, coord_t y, coord_t z)
{
    angle_t const angle = P_Random() << 24;

#if !__JHEXEN__
    // Clients do not spawn puffs.
    if(IS_CLIENT) return 0;
#endif

    mobjtype_t type = PuffType;
#if __JHERETIC__
    if(PuffType == MT_BLASTERPUFF1)
    {
        type = MT_BLASTERPUFF2;
    }
    else
#endif
    {
        z += FIX2FLT((P_Random() - P_Random()) << 10);
    }

    mobj_t *puff = P_SpawnMobjXYZ(type, x, y, z, angle, 0);
    if(puff)
    {
#if __JHEXEN__
        if(lineTarget && puff->info->seeSound)
        {
            // Hit thing sound.
            S_StartSound(puff->info->seeSound, puff);
        }
        else if(puff->info->attackSound)
        {
            S_StartSound(puff->info->attackSound, puff);
        }

        switch(PuffType)
        {
        case MT_PUNCHPUFF:  puff->mom[MZ] = 1;   break;
        case MT_HAMMERPUFF: puff->mom[MZ] = .8f; break;

        default: break;
        }
#elif __JHERETIC__
        if(PuffType == MT_BLASTERPUFF1)
        {
            S_StartSound(SFX_BLSHIT, puff);
        }
        else
        {
            if(puff->info->attackSound)
            {
                S_StartSound(puff->info->attackSound, puff);
            }

            switch(type)
            {
            case MT_BEAKPUFF:
            case MT_STAFFPUFF:     puff->mom[MZ] = 1;   break;

            case MT_GAUNTLETPUFF1:
            case MT_GAUNTLETPUFF2: puff->mom[MZ] = .8f; break;

            default: break;
            }
        }
#else
        puff->mom[MZ] = FIX2FLT(FRACUNIT);

        puff->tics -= P_Random() & 3;
        if(puff->tics < 1) puff->tics = 1; // Always at least one tic.

        // Don't make punches spark on the wall.
        if(PuffNoSpark)
        {
            P_MobjChangeState(puff, S_PUFF3);
        }
#endif
    }

#if __JHEXEN__
    PuffSpawned = puff;
#endif

    return puff;
}

struct ptr_shoottraverse_params_t
{
    mobj_t *shooterMobj; ///< Mobj doing the shooting.
    int damage;          ///< Damage to inflict.
    coord_t range;       ///< Maximum effective range from the trace origin.
};

/**
 * @todo This routine has gotten way too big, split if(in->isaline)
 *       to a seperate routine?
 */
int PTR_ShootTraverse(Intercept const *icpt, void *context)
{
    vec3d_t const tracePos = {
        Interceptor_Origin(icpt->trace)[VX], Interceptor_Origin(icpt->trace)[VY], shootZ
    };

    ptr_shoottraverse_params_t &parm = *static_cast<ptr_shoottraverse_params_t *>(context);

    if(icpt->type == ICPT_LINE)
    {
        bool lineWasHit = false;

        Line *line = icpt->line;
        xline_t *xline = P_ToXLine(line);

        Sector *backSec = (Sector *)P_GetPtrp(line, DMU_BACK_SECTOR);

        if(!backSec || !(xline->flags & ML_TWOSIDED))
        {
            if(Line_PointOnSide(line, tracePos) < 0)
            {
                return false; // Continue traversal.
            }
        }

        if(xline->special)
        {
            P_ActivateLine(line, parm.shooterMobj, 0, SPAC_IMPACT);
        }

        Sector *frontSec = 0;
        coord_t dist = 0;
        coord_t slope = 0;

        if(!backSec) goto hitline;

#if __JDOOM64__
        if(xline->flags & ML_BLOCKALL) goto hitline;
#endif

        // Crosses a two sided line.
        Interceptor_AdjustOpening(icpt->trace, line);

        frontSec = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR);

        dist = parm.range * icpt->distance;
        slope = 0;
        if(!FEQUAL(P_GetDoublep(frontSec, DMU_FLOOR_HEIGHT),
                   P_GetDoublep(backSec,  DMU_FLOOR_HEIGHT)))
        {
            slope = (Interceptor_Opening(icpt->trace)->bottom - tracePos[VZ]) / dist;

            if(slope > aimSlope) goto hitline;
        }

        if(!FEQUAL(P_GetDoublep(frontSec, DMU_CEILING_HEIGHT),
                   P_GetDoublep(backSec,  DMU_CEILING_HEIGHT)))
        {
            slope = (Interceptor_Opening(icpt->trace)->top - tracePos[VZ]) / dist;

            if(slope < aimSlope) goto hitline;
        }
        // Shot continues...
        return false;

        // Hit a line.
      hitline:

        // Position a bit closer.
        coord_t frac = icpt->distance - (4 / parm.range);
        vec3d_t pos = { tracePos[VX] + Interceptor_Direction(icpt->trace)[VX] * frac,
                        tracePos[VY] + Interceptor_Direction(icpt->trace)[VY] * frac,
                        tracePos[VZ] + aimSlope * (frac * parm.range) };

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
        spawnPuff(pos[VX], pos[VY], pos[VZ]);

#if !__JHEXEN__
        if(lineWasHit && xline->special)
        {
            // Extended shoot events only happen when the bullet actually hits the line.
            XL_ShootLine(line, 0, parm.shooterMobj);
        }
#endif
        // Don't go any farther.
        return true;
    }

    // Intercepted a mobj.
    mobj_t *th = icpt->mobj;

    if(th == parm.shooterMobj) return false; // Can't shoot oneself.
    if(!(th->flags & MF_SHOOTABLE)) return false; // Corpse or something.

#if __JHERETIC__
    // Check for physical attacks on a ghost.
    if((th->flags & MF_SHADOW) && Mobj_IsPlayer(parm.shooterMobj) &&
       parm.shooterMobj->player->readyWeapon == WT_FIRST)
    {
        return false;
    }
#endif

    // Check angles to see if the thing can be aimed at
    coord_t dist = parm.range * icpt->distance;
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
    coord_t frac = icpt->distance - (10 / parm.range);
    vec3d_t pos  = { tracePos[VX] + Interceptor_Direction(icpt->trace)[VX] * frac,
                     tracePos[VY] + Interceptor_Direction(icpt->trace)[VY] * frac,
                     tracePos[VZ] + aimSlope * (frac * parm.range) };

    // Spawn bullet puffs or blood spots, depending on target type.
#if !__JDOOM__
    spawnPuff(pos[VX], pos[VY], pos[VZ]);
#endif

    if(parm.damage)
    {
#if __JDOOM__ || __JDOOM64__
        angle_t attackAngle = M_PointToAngle2(parm.shooterMobj->origin, pos);
#endif

        mobj_t *inflictor = parm.shooterMobj;
#if __JHEXEN__
        if(PuffType == MT_FLAMEPUFF2)
        {
            // Cleric FlameStrike does fire damage.
            inflictor = &lavaInflictor;
        }
#endif

        int damageDone =
            P_DamageMobj(th, inflictor, parm.shooterMobj, parm.damage, false);

#if __JHEXEN__
        if(!(icpt->mobj->flags2 & MF2_INVULNERABLE))
#endif
        {
            if(!(icpt->mobj->flags & MF_NOBLOOD))
            {
                if(damageDone > 0)
                {
                    // Damage was inflicted, so shed some blood.
#if __JDOOM__ || __JDOOM64__
                    P_SpawnBlood(pos[VX], pos[VY], pos[VZ], parm.damage, attackAngle + ANG180);
#else
# if __JHEXEN__
                    if(PuffType == MT_AXEPUFF || PuffType == MT_AXEPUFF_GLOW)
                    {
                        P_SpawnBloodSplatter2(pos[VX], pos[VY], pos[VZ], icpt->mobj);
                    }
                    else
# endif
                    if(P_Random() < 192)
                    {
                        P_SpawnBloodSplatter(pos[VX], pos[VY], pos[VZ], icpt->mobj);
                    }
#endif
                }
            }
#if __JDOOM__ || __JDOOM64__
            else
            {
                spawnPuff(pos[VX], pos[VY], pos[VZ]);
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
int PTR_AimTraverse(Intercept const *icpt, void * /*context*/)
{
    vec3d_t const tracePos = {
        Interceptor_Origin(icpt->trace)[VX], Interceptor_Origin(icpt->trace)[VY], shootZ
    };

    if(icpt->type == ICPT_LINE)
    {
        Line *line = icpt->line;
        Sector *backSec, *frontSec;

        if(!(P_ToXLine(line)->flags & ML_TWOSIDED) ||
           !(frontSec = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR)) ||
           !(backSec  = (Sector *)P_GetPtrp(line, DMU_BACK_SECTOR)))
        {
            return !(Line_PointOnSide(line, tracePos) < 0);
        }

        // Crosses a two sided line.
        // A two sided line will restrict the possible target ranges.
        if(!Interceptor_AdjustOpening(icpt->trace, line))
        {
            return true; // Stop.
        }

        coord_t dist   = attackRange * icpt->distance;
        coord_t fFloor = P_GetDoublep(frontSec, DMU_FLOOR_HEIGHT);
        coord_t fCeil  = P_GetDoublep(frontSec, DMU_CEILING_HEIGHT);
        coord_t bFloor = P_GetDoublep(backSec, DMU_FLOOR_HEIGHT);
        coord_t bCeil  = P_GetDoublep(backSec, DMU_CEILING_HEIGHT);

        coord_t slope;
        if(!FEQUAL(fFloor, bFloor))
        {
            slope = (Interceptor_Opening(icpt->trace)->bottom - shootZ) / dist;
            if(slope > bottomSlope)
                bottomSlope = slope;
        }

        if(!FEQUAL(fCeil, bCeil))
        {
            slope = (Interceptor_Opening(icpt->trace)->top - shootZ) / dist;
            if(slope < topSlope)
                topSlope = slope;
        }

        return topSlope <= bottomSlope;
    }

    // Intercepted a mobj.

    mobj_t *th = icpt->mobj;

    if(th == shootThing) return false; // Can't aim at oneself.
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
    coord_t dist = attackRange * icpt->distance;
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

void P_LineAttack(mobj_t *t1, angle_t angle, coord_t distance, coord_t slope, int damage)
{
    uint an = angle >> ANGLETOFINESHIFT;
    vec2d_t target = { t1->origin[VX] + distance * FIX2FLT(finecosine[an]),
                       t1->origin[VY] + distance * FIX2FLT(finesine[an]) };

    aimSlope = slope;

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

#if __JDOOM__ || __JDOOM64__
    PuffType = MT_PUFF;
    PuffNoSpark = attackRange == MELEERANGE;
#endif

    ptr_shoottraverse_params_t parm;
    parm.shooterMobj = t1;
    parm.range       = distance;
    parm.damage      = damage;

    if(!P_PathTraverse(t1->origin, target, PTR_ShootTraverse, &parm))
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
            spawnPuff(target[VX], target[VY], shootZ + (slope * distance));
            break;

        default: break;
        }
#endif
    }
}

struct pit_radiusattack_params_t
{
    mobj_t *source;     ///< Mobj which caused the attack.
    mobj_t *bomb;       ///< Epicenter of the attack.
    int damage;         ///< Maximum damage to inflict.
    int distance;       ///< Maximum distance within which to afflict.
#ifdef __JHEXEN__
    bool afflictSource; ///< @c true= Afflict the source, also.
#endif
};

int PIT_RadiusAttack(mobj_t *thing, void *context)
{
    pit_radiusattack_params_t &parm = *static_cast<pit_radiusattack_params_t *>(context);

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
    // Is the source of the explosion immune to damage?
    if(thing == parm.source && !parm.afflictSource)
        return false;
#endif

    vec3d_t delta = { fabs(thing->origin[VX] - parm.bomb->origin[VX]),
                      fabs(thing->origin[VY] - parm.bomb->origin[VY]),
                      fabs((thing->origin[VZ] + thing->height / 2) - parm.bomb->origin[VZ]) };

    coord_t dist = (delta[VX] > delta[VY]? delta[VX] : delta[VY]);
#if __JHEXEN__
    if(!cfg.netNoMaxZRadiusAttack)
    {
        dist = (delta[VZ] > dist? delta[VZ] : dist);
    }
#else
    if(!(cfg.netNoMaxZRadiusAttack || (thing->info->flags2 & MF2_INFZBOMBDAMAGE)))
    {
        dist = (delta[VZ] > dist? delta[VZ] : dist);
    }
#endif

    dist = MAX_OF(dist - thing->radius, 0);
    if(dist >= parm.distance)
    {
        return false; // Out of range.
    }

    // Must be in direct path.
    if(P_CheckSight(thing, parm.bomb))
    {
        int damage = (parm.damage * (parm.distance - dist) / parm.distance) + 1;
#if __JHEXEN__
        if(thing->player) damage /= 4;
#endif

        P_DamageMobj(thing, parm.bomb, parm.source, damage, false);
    }

    return false;
}

#if __JHEXEN__
void P_RadiusAttack(mobj_t *bomb, mobj_t *source, int damage, int distance, boolean afflictSource)
#else
void P_RadiusAttack(mobj_t *bomb, mobj_t *source, int damage, int distance)
#endif
{
    coord_t const dist = distance + MAXRADIUS;
    AABoxd const box(bomb->origin[VX] - dist, bomb->origin[VY] - dist,
                     bomb->origin[VX] + dist, bomb->origin[VY] + dist);

    pit_radiusattack_params_t parm;
    parm.bomb          = bomb;
    parm.damage        = damage;
    parm.distance      = distance;
    parm.source        = source;
#if __JHERETIC__
    if(bomb->type == MT_POD && bomb->target)
    {
        // The credit should go to the original source (chain-reaction kills).
        parm.source = bomb->target;
    }
#endif
#if __JHEXEN__
    parm.afflictSource = CPP_BOOL(afflictSource);
#endif

    VALIDCOUNT++;
    Mobj_BoxIterator(&box, PIT_RadiusAttack, &parm);
}

int PTR_UseTraverse(Intercept const *icpt, void *context)
{
    DENG_ASSERT(icpt->type == ICPT_LINE);

    mobj_t *useThing = static_cast<mobj_t *>(context);

    xline_t *xline = P_ToXLine(icpt->line);
    if(!xline->special)
    {
        if(!Interceptor_AdjustOpening(icpt->trace, icpt->line))
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

            if(Interceptor_Opening(icpt->trace)->top < pheight ||
               Interceptor_Opening(icpt->trace)->bottom > pheight)
            {
                S_StartSound(PCLASS_INFO(useThing->player->class_)->failUseSound, useThing);
            }
        }
#endif
        // Not a special line, but keep checking.
        return false;
    }

    int side = Line_PointOnSide(icpt->line, useThing->origin) < 0;

#if __JHERETIC__ || __JHEXEN__
    if(side == 1) return true; // Don't use back side.
#endif

    P_ActivateLine(icpt->line, useThing, side, SPAC_USE);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Can use multiple line specials in a row with the PassThru flag.
    if(xline->flags & ML_PASSUSE) return false;
#endif

    // Can't use more than one special line in a row.
    return true;
}

void P_UseLines(player_t *player)
{
    if(!player) return;

    if(IS_CLIENT)
    {
#ifdef _DEBUG
        Con_Message("P_UseLines: Sending a use request for player %i.", int(player - players));
#endif
        NetCl_PlayerActionRequest(player, GPA_USE, 0);
        return;
    }

    mobj_t *mo  = player->plr->mo;
    if(!mo) return;

    uint an     = mo->angle >> ANGLETOFINESHIFT;
    vec2d_t pos = { mo->origin[VX] + USERANGE * FIX2FLT(finecosine[an]),
                    mo->origin[VY] + USERANGE * FIX2FLT(finesine  [an]) };

    P_PathTraverse2(mo->origin, pos, PTF_LINE, PTR_UseTraverse, mo);
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
    thing->floorZ   = tmFloorZ;
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
        // Update view offset of real players.
        if(Mobj_IsPlayer(thing) && !Mobj_IsVoodooDoll(thing))
        {
            thing->player->viewZ += thing->floorZ - thing->origin[VZ];
        }

        // Walking monsters rise and fall with the floor.
        thing->origin[VZ] = thing->floorZ;

        // $dropoff_fix: Possibly upset balance of objects hanging off ledges.
        if((thing->intFlags & MIF_FALLING) && thing->gear >= MAXGEAR)
        {
            thing->gear = 0;
        }
#endif
    }
    else
    {
        // Don't adjust a floating monster unless forced to.
        if(thing->origin[VZ] + thing->height > thing->ceilingZ)
        {
            thing->origin[VZ] = thing->ceilingZ - thing->height;
        }
    }

    return (thing->ceilingZ - thing->floorZ) >= thing->height;
}

/**
 * Allows the player to slide along any angled walls by adjusting the
 * xmove / ymove so that the NEXT move will slide along the wall.
 *
 * @param line  The line being slid along.
 */
static void P_HitSlideLine(mobj_t *slideMo, Line *line, pvec2d_t move)
{
    DENG_ASSERT(slideMo != 0 && line != 0);

    slopetype_t slopeType = slopetype_t(P_GetIntp(line, DMU_SLOPETYPE));
    if(slopeType == ST_HORIZONTAL)
    {
        move[MY] = 0;
        return;
    }
    else if(slopeType == ST_VERTICAL)
    {
        move[MX] = 0;
        return;
    }

    bool side = Line_PointOnSide(line, slideMo->origin) < 0;
    vec2d_t d1; P_GetDoublepv(line, DMU_DXY, d1);

    angle_t moveAngle = M_PointToAngle(move);
    angle_t lineAngle = M_PointToAngle(d1) + (side? ANG180 : 0);

    angle_t deltaAngle = moveAngle - lineAngle;
    if(deltaAngle > ANG180) deltaAngle += ANG180;

    coord_t moveLen = M_ApproxDistance(move[MX], move[MY]);
    coord_t newLen  = moveLen * FIX2FLT(finecosine[deltaAngle >> ANGLETOFINESHIFT]);

    uint an = lineAngle >> ANGLETOFINESHIFT;
    V2d_Set(move, newLen * FIX2FLT(finecosine[an]),
                  newLen * FIX2FLT(finesine  [an]));
}

struct ptr_slidetraverse_params_t
{
    mobj_t *slideMobj;
    Line *bestLine;
    coord_t bestDistance;
};

int PTR_SlideTraverse(Intercept const *icpt, void *context)
{
    DENG_ASSERT(icpt->type == ICPT_LINE);

    ptr_slidetraverse_params_t &parm = *static_cast<ptr_slidetraverse_params_t *>(context);

    Line *line = icpt->line;
    if(!(P_ToXLine(line)->flags & ML_TWOSIDED) ||
       !P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR))
    {
        if(Line_PointOnSide(line, parm.slideMobj->origin) < 0)
        {
            return false; // Don't hit the back side.
        }

        goto isblocking;
    }

#if __JDOOM64__
    if(P_ToXLine(line)->flags & ML_BLOCKALL) // jd64
        goto isblocking;
#endif

    Interceptor_AdjustOpening(icpt->trace, line);

    if(Interceptor_Opening(icpt->trace)->range < parm.slideMobj->height)
        goto isblocking; // Doesn't fit.

    if(Interceptor_Opening(icpt->trace)->top - parm.slideMobj->origin[VZ] < parm.slideMobj->height)
        goto isblocking; // mobj is too high.

    if(Interceptor_Opening(icpt->trace)->bottom - parm.slideMobj->origin[VZ] > 24)
        goto isblocking; // Too big a step up.

    // This line doesn't block movement.
    return false;

    // The line does block movement, see if it is closer than best so far.
  isblocking:
    if(icpt->distance < parm.bestDistance)
    {
        parm.bestDistance = icpt->distance;
        parm.bestLine     = line;
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

    vec2d_t leadPos  = { 0, 0 };
    vec2d_t trailPos = { 0, 0 };
    vec2d_t tmMove   = { 0, 0 };

    int hitCount = 3;
    do
    {
        if(--hitCount == 0)
            goto stairstep; // Don't loop forever.

        // Trace along the three leading corners.
        leadPos[VX] = mo->origin[VX] + (mo->mom[MX] > 0? mo->radius : -mo->radius);
        leadPos[VY] = mo->origin[VY] + (mo->mom[MY] > 0? mo->radius : -mo->radius);

        trailPos[VX] = mo->origin[VX] - (mo->mom[MX] > 0? mo->radius : -mo->radius);
        trailPos[VY] = mo->origin[VY] - (mo->mom[MY] > 0? mo->radius : -mo->radius);

        ptr_slidetraverse_params_t parm;
        parm.slideMobj    = mo;
        parm.bestLine     = 0;
        parm.bestDistance = 1;

        P_PathXYTraverse2(leadPos[VX], leadPos[VY],
                          leadPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, &parm);

        P_PathXYTraverse2(trailPos[VX], leadPos[VY],
                          trailPos[VX] + mo->mom[MX], leadPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, &parm);

        P_PathXYTraverse2(leadPos[VX], trailPos[VY],
                          leadPos[VX] + mo->mom[MX], trailPos[VY] + mo->mom[MY],
                          PTF_LINE, PTR_SlideTraverse, &parm);

        // Move up to the wall.
        if(parm.bestDistance == 1)
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
        parm.bestDistance -= (1.0f / 32);

        if(parm.bestDistance > 0)
        {
            vec2d_t newPos = { mo->origin[VX] + mo->mom[MX] * parm.bestDistance,
                               mo->origin[VY] + mo->mom[MY] * parm.bestDistance };

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
        parm.bestDistance = MIN_OF(1 - (parm.bestDistance + (1.0f / 32)), 1);
        if(parm.bestDistance <= 0)
        {
            break;
        }

        V2d_Set(tmMove, mo->mom[VX] * parm.bestDistance,
                        mo->mom[VY] * parm.bestDistance);

        P_HitSlideLine(mo, parm.bestLine, tmMove); // Clip the move.

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
 * If crushDamage is non-zero, they will take damage as they are being crushed.
 * If crushDamage is false, you should set the sector height back the way it
 * was and call P_ChangeSector again to undo the changes.
 */

struct pit_changesector_params_t
{
    int crushDamage;  ///< Damage amount;
    bool noFit;
};

int PIT_ChangeSector(mobj_t *thing, void *context)
{
    pit_changesector_params_t &parm = *static_cast<pit_changesector_params_t *>(context);

    DENG_ASSERT(thing->info != 0);

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

    parm.noFit = true;

    if(parm.crushDamage > 0 && !(mapTime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, parm.crushDamage, false);

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

boolean P_ChangeSector(Sector *sector, int crush)
{
    pit_changesector_params_t parm;
    parm.noFit       = false;
#if __JHEXEN__
    parm.crushDamage = int(crush);
#else
    parm.crushDamage = 10;
#endif

    VALIDCOUNT++;
    Sector_TouchingMobjsIterator(sector, PIT_ChangeSector, &parm);

    return parm.noFit;
}

void P_HandleSectorHeightChange(int sectorIdx)
{
    P_ChangeSector((Sector *)P_ToPtr(DMU_SECTOR, sectorIdx), false /*don't crush*/);
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

#if __JHEXEN__
int PIT_ThrustStompThing(mobj_t *thing, void *context)
{
    mobj_t *tsThing = static_cast<mobj_t *>(context);

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

void P_ThrustSpike(mobj_t *mobj)
{
    if(!mobj) return;

    coord_t const radius = mobj->info->radius + MAXRADIUS;
    AABoxd const box(mobj->origin[VX] - radius, mobj->origin[VY] - radius,
                     mobj->origin[VX] + radius, mobj->origin[VY] + radius);

    VALIDCOUNT++;
    Mobj_BoxIterator(&box, PIT_ThrustStompThing, mobj);
}

struct pit_checkonmobjz_params_t
{
    mobj_t *riderMobj;
    mobj_t *mountMobj;
};

/// @return  @c false= Continue iteration.
static int PIT_CheckOnMobjZ(mobj_t *cand, void *context)
{
    pit_checkonmobjz_params_t &parm = *static_cast<pit_checkonmobjz_params_t *>(context);

    if(!(cand->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
    {
        return false; // Can't hit thing.
    }

    coord_t blockdist = cand->radius + parm.riderMobj->radius;
    if(fabs(cand->origin[VX] - tm[VX]) >= blockdist ||
       fabs(cand->origin[VY] - tm[VY]) >= blockdist)
    {
        return false; // Didn't hit thing.
    }

    // Don't clip against self.
    if(cand == parm.riderMobj) return false;

    if(IS_CLIENT)
    {
        // Players must not ride their clmobjs.
        if(Mobj_IsPlayer(parm.riderMobj))
        {
            if(cand == ClPlayer_ClMobj(parm.riderMobj->player - players))
            {
                return false;
            }
        }
    }

    // Above or below?
    if(parm.riderMobj->origin[VZ] > cand->origin[VZ] + cand->height)
    {
        return false;
    }
    else if(parm.riderMobj->origin[VZ] + parm.riderMobj->height < cand->origin[VZ])
    {
        return false;
    }

    if(cand->flags & MF_SOLID)
    {
        parm.mountMobj = cand;
    }

    return (cand->flags & MF_SOLID) != 0;
}

mobj_t *P_CheckOnMobj(mobj_t *mo)
{
    if(!mo) return 0;
    if(P_MobjIsCamera(mo)) return 0;

    // Players' clmobjs shouldn't do any on-mobj logic; the real player mobj
    // will interact with (cl)mobjs.
    if(Mobj_IsPlayerClMobj(mo)) return 0;

    /**
     * Fake the zmovement so that we can check if a move is legal.
     *
     * @todo Do this properly! Consolidate with how jDoom/jHeretic do on-mobj checks?
     */
    mobj_t oldMo = *mo; // Save the old mobj before the fake z movement.

    // Adjust Z-origin.
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
            {
                mo->origin[VZ] -= FLOATSPEED;
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->origin[VZ] += FLOATSPEED;
            }
        }
    }

    if(Mobj_IsPlayer(mo) && (mo->flags2 & MF2_FLY) && !(mo->origin[VZ] <= mo->floorZ))
    {
        if(mapTime & 2)
        {
            mo->origin[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
        }
    }

    // Clip momentum.

    // Hit the floor?
    bool hitFloor = mo->origin[VZ] <= mo->floorZ;
    if(hitFloor)
    {
        mo->origin[VZ] = mo->floorZ;
        if(mo->mom[MZ] < 0)
        {
            mo->mom[MZ] = 0;
        }

        if(mo->flags & MF_SKULLFLY)
        {
            mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something
        }
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(FEQUAL(mo->mom[MZ], 0))
        {
            mo->mom[MZ] = -(P_GetGravity() / 32) * 2;
        }
        else
        {
            mo->mom[MZ] -= P_GetGravity() / 32;
        }
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(FEQUAL(mo->mom[MZ], 0))
        {
            mo->mom[MZ] = -P_GetGravity() * 2;
        }
        else
        {
            mo->mom[MZ] -= P_GetGravity();
        }
    }

    if(!(hitFloor && P_GetState(mobjtype_t(mo->type), SN_CRASH) && (mo->flags & MF_CORPSE)))
    {
        if(mo->origin[VZ] + mo->height > mo->ceilingZ)
        {
            mo->origin[VZ] = mo->ceilingZ - mo->height;

            if(mo->mom[MZ] > 0)
            {
                mo->mom[MZ] = 0;
            }

            if(mo->flags & MF_SKULLFLY)
            {
                mo->mom[MZ] = -mo->mom[MZ]; // The skull slammed into something.
            }
        }
    }

    /*tmCeilingLine = tmFloorLine = 0;

    // The base floor/ceiling is from the BSP leaf that contains the point.
    // Any contacted lines the step closer together will adjust them.

    Sector *newSector = Sector_AtPoint_FixedPrecision(mo->origin);

    tmFloorZ        = tmDropoffZ = P_GetDoublep(newSector, DMU_FLOOR_HEIGHT);
    tmCeilingZ      = P_GetDoublep(newSector, DMU_CEILING_HEIGHT);
    tmFloorMaterial = (Material *)P_GetPtrp(newSector, DMU_FLOOR_MATERIAL);

    IterList_Clear(spechit);*/

    if(!(mo->flags & MF_NOCLIP))
    {
        AABoxd aaBox(mo->origin[VX] - mo->radius - MAXRADIUS, mo->origin[VY] - mo->radius - MAXRADIUS,
                     mo->origin[VX] + mo->radius + MAXRADIUS, mo->origin[VY] + mo->radius + MAXRADIUS);

        pit_checkonmobjz_params_t parm;
        parm.riderMobj = mo;
        parm.mountMobj = 0;

        VALIDCOUNT++;
        if(Mobj_BoxIterator(&aaBox, PIT_CheckOnMobjZ, &parm))
        {
            // Restore the mobj back to its previous state.
            *mo = oldMo;

            return parm.mountMobj;
        }
    }

    // Restore the mobj back to its previous state.
    *mo = oldMo;

    return 0;
}

struct ptr_boucetraverse_params_t
{
    mobj_t *bounceMobj;
    Line *bestLine;
    coord_t bestDistance;
};

int PTR_BounceTraverse(Intercept const *icpt, void *context)
{
    DENG_ASSERT(icpt->type == ICPT_LINE);

    ptr_boucetraverse_params_t &parm = *static_cast<ptr_boucetraverse_params_t *>(context);

    Line *line = icpt->line;
    if(!P_GetPtrp(line, DMU_FRONT_SECTOR) || !P_GetPtrp(line, DMU_BACK_SECTOR))
    {
        if(Line_PointOnSide(line, parm.bounceMobj->origin) < 0)
            return false; // Don't hit the back side.

        goto bounceblocking;
    }

    Interceptor_AdjustOpening(icpt->trace, line);

    if(Interceptor_Opening(icpt->trace)->range < parm.bounceMobj->height)
        goto bounceblocking; // Doesn't fit.

    if(Interceptor_Opening(icpt->trace)->top - parm.bounceMobj->origin[VZ] < parm.bounceMobj->height)
        goto bounceblocking; // Mobj is too high...

    return false; // This line doesn't block movement...

    // the line does block movement, see if it is closer than best so far.
  bounceblocking:
    if(icpt->distance < parm.bestDistance)
    {
        parm.bestDistance = icpt->distance;
        parm.bestLine     = line;
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

    ptr_boucetraverse_params_t parm;
    parm.bounceMobj   = mo;
    parm.bestLine     = 0;
    parm.bestDistance = 1; // Intercept distances are normalized [0..1]

    P_PathTraverse2(leadPos, destPos, PTF_LINE, PTR_BounceTraverse, &parm);

    if(parm.bestLine)
    {
        int const side = Line_PointOnSide(parm.bestLine, mo->origin) < 0;
        vec2d_t lineDirection; P_GetDoublepv(parm.bestLine, DMU_DXY, lineDirection);

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

static sfxenum_t usePuzzleItemFailSound(mobj_t *user)
{
    if(Mobj_IsPlayer(user))
    {
        /// @todo Get this from ClassInfo.
        switch(user->player->class_)
        {
        case PCLASS_FIGHTER: return SFX_PUZZLE_FAIL_FIGHTER;
        case PCLASS_CLERIC:  return SFX_PUZZLE_FAIL_CLERIC;
        case PCLASS_MAGE:    return SFX_PUZZLE_FAIL_MAGE;

        default: break;
        }
    }
    return SFX_NONE;
}

struct ptr_puzzleitemtraverse_params_t
{
    mobj_t *useMobj;
    int itemType;
    bool activated;
};

int PTR_PuzzleItemTraverse(Intercept const *icpt, void *context)
{
    int const USE_PUZZLE_ITEM_SPECIAL = 129;

    ptr_puzzleitemtraverse_params_t &parm = *static_cast<ptr_puzzleitemtraverse_params_t *>(context);

    switch(icpt->type)
    {
    case ICPT_LINE: {
        xline_t *xline = P_ToXLine(icpt->line);

        if(xline->special != USE_PUZZLE_ITEM_SPECIAL)
        {
            if(!Interceptor_AdjustOpening(icpt->trace, icpt->line))
            {
                S_StartSound(usePuzzleItemFailSound(parm.useMobj), parm.useMobj);
                return true; // Can't use through a wall.
            }

            return false; // Continue searching...
        }

        if(Line_PointOnSide(icpt->line, parm.useMobj->origin) < 0)
            return true; // Don't use back sides.

        if(parm.itemType != xline->arg1)
            return true; // Item type doesn't match.

        P_StartACS(xline->arg2, 0, &xline->arg3, parm.useMobj, icpt->line, 0);
        xline->special = 0;

        parm.activated = true;

        return true; // Stop searching.
        }

    case ICPT_MOBJ:
        if(icpt->mobj->special != USE_PUZZLE_ITEM_SPECIAL)
            return false; // Wrong special...

        if(parm.itemType != icpt->mobj->args[0])
            return false; // Item type doesn't match...

        P_StartACS(icpt->mobj->args[1], 0, &icpt->mobj->args[2], parm.useMobj, NULL, 0);
        icpt->mobj->special = 0;

        parm.activated = true;

        return true; // Stop searching.

    default:
        DENG_ASSERT(false);
        return false;
    }
}

boolean P_UsePuzzleItem(player_t *player, int itemType)
{
    DENG_ASSERT(player != 0);

    mobj_t *mobj = player->plr->mo;
    if(!mobj) return false; // Huh?

    ptr_puzzleitemtraverse_params_t parm;
    parm.useMobj   = mobj;
    parm.itemType  = itemType;
    parm.activated = false;

    uint an = mobj->angle >> ANGLETOFINESHIFT;
    vec2d_t farUsePoint = { mobj->origin[VX] + FIX2FLT(USERANGE * finecosine[an]),
                            mobj->origin[VY] + FIX2FLT(USERANGE * finesine  [an]) };

    P_PathTraverse(mobj->origin, farUsePoint, PTR_PuzzleItemTraverse, &parm);

    if(!parm.activated)
    {
        P_SetYellowMessage(player, 0, TXT_USEPUZZLEFAILED);
    }

    return parm.activated;
}
#endif
