/** @file p_mobj.c World map object interactions.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "dmu_lib.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "player.h"
#include "p_tick.h"
#include "p_actor.h"
#include "p_start.h"

#define VANISHTICS              (2*TICSPERSEC)
#define SPAWNFADETICS           (1*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

void P_ExplodeMissile(mobj_t *mo)
{
    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));

    mo->tics -= P_Random() & 3;

    if(mo->tics < 1)
        mo->tics = 1;

    if(mo->flags & MF_MISSILE)
    {
        mo->flags &= ~MF_MISSILE;
        mo->flags |= MF_VIEWALIGN;
        // Remove the brightshadow flag.
        if(mo->flags & MF_BRIGHTSHADOW)
            mo->flags &= ~MF_BRIGHTSHADOW;
        if(mo->flags & MF_BRIGHTEXPLODE)
            mo->flags |= MF_BRIGHTSHADOW;
    }

    if(mo->info->deathSound)
        S_StartSound(mo->info->deathSound, mo);
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->mom[MZ] = -mo->mom[MZ];
    P_MobjChangeState(mo, P_GetState(mo->type, SN_DEATH));
}

void P_MobjMoveXY(mobj_t *mo)
{
    coord_t pos[3], mom[3];
    //player_t *player;
    dd_bool largeNegative;

    // $democam: cameramen have their own movement code
    if(P_CameraXYMovement(mo))
        return;

    if(INRANGE_OF(mo->mom[MX], 0, NOMOM_THRESHOLD) &&
       INRANGE_OF(mo->mom[MY], 0, NOMOM_THRESHOLD))
    {
        if(mo->flags & MF_SKULLFLY)
        {
            // The skull slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

            P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
        }

        return;
    }

    mom[MX] = MINMAX_OF(-MAXMOM, mo->mom[MX], MAXMOM);
    mom[MY] = MINMAX_OF(-MAXMOM, mo->mom[MY], MAXMOM);
    mo->mom[MX] = mom[MX];
    mo->mom[MY] = mom[MY];

    //player = mo->player;

    do
    {
        /**
         * DOOM.exe bug fix:
         * Large negative displacements were never considered. This explains the
         * tendency for Mancubus fireballs to pass through walls.
         */

        largeNegative = false;
        if(!cfg.moveBlock && (mom[MX] < -MAXMOMSTEP || mom[MY] < -MAXMOMSTEP))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(largeNegative || mom[MX] > MAXMOMSTEP || mom[MY] > MAXMOMSTEP)
        {
            pos[VX] = mo->origin[VX] + mom[VX] / 2;
            pos[VY] = mo->origin[VY] + mom[VY] / 2;
            mom[VX] /= 2;
            mom[VY] /= 2;
        }
        else
        {
            pos[VX] = mo->origin[VX] + mom[MX];
            pos[VY] = mo->origin[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // $dropoff_fix.
        if(!P_TryMoveXY(mo, pos[VX], pos[VY], true, false))
        {
            // Blocked move.
            if(mo->flags2 & MF2_SLIDE)
            {
                // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                Sector* backSec;

                /// @kludge: Prevent missiles exploding against the sky.
                if(tmCeilingLine &&
                   (backSec = P_GetPtrp(tmCeilingLine, DMU_BACK_SECTOR)))
                {
                    world_Material* mat = P_GetPtrp(backSec, DMU_CEILING_MATERIAL);

                    if((P_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->origin[VZ] > P_GetDoublep(backSec, DMU_CEILING_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }

                if(tmFloorLine &&
                   (backSec = P_GetPtrp(tmFloorLine, DMU_BACK_SECTOR)))
                {
                    world_Material* mat = P_GetPtrp(backSec, DMU_FLOOR_MATERIAL);

                    if((P_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK) &&
                       mo->origin[VZ] < P_GetDoublep(backSec, DMU_FLOOR_HEIGHT))
                    {
                        P_MobjRemove(mo, false);
                        return;
                    }
                }
                // kludge end.

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGE_OF(mom[MX], 0, NOMOM_THRESHOLD) ||
            !INRANGE_OF(mom[MY], 0, NOMOM_THRESHOLD));

    // Slow down.
    Mobj_XYMoveStopping(mo);
}

/*
static int PIT_Splash(Sector* sector, void* parameters)
{
    mobj_t* mo = (mobj_t*)parameters;
    coord_t floorheight = P_GetDoublep(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->origin[VZ] < floorheight &&
       mo->origin[VZ] + mo->height / 2 > floorheight)
    {
        /// @todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return false;
}
*/

void P_HitFloor(mobj_t* mo)
{
    //Mobj_TouchedSectorsIterator(mo, PIT_Splash, mo);
}

void P_MobjMoveZ(mobj_t *mo)
{
    coord_t gravity = XS_Gravity(Mobj_Sector(mo));
    coord_t dist, delta;

    // $democam: cameramen get special z movement.
    if(P_CameraZMovement(mo))
        return;

    // $voodoodolls: Check for smooth step up unless a voodoo doll.
    if(mo->player && mo->player->plr->mo == mo &&
       mo->origin[VZ] < mo->floorZ)
    {
        mo->player->viewHeight -= mo->floorZ - mo->origin[VZ];
        mo->player->viewHeightDelta =
            (cfg.common.plrViewHeight - mo->player->viewHeight) / 8;
    }

    // Adjust height.
    mo->origin[VZ] += mo->mom[MZ];

    if((mo->flags2 & MF2_FLY) &&
       mo->onMobj && mo->origin[VZ] > mo->onMobj->origin[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
       mo->target && !P_MobjIsCamera(mo->target))
    {
        // Float down towards target if too close.
        dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                mo->origin[VY] - mo->target->origin[VY]);

        //delta = (mo->target->origin[VZ] + (mo->height / 2)) - mo->origin[VZ];
        delta = (mo->target->origin[VZ] + mo->target->height / 2) -
                (mo->origin[VZ] + mo->height / 2);

        // Don't go INTO the target.
        if(!(dist < mo->radius + mo->target->radius &&
             fabs(delta) < mo->height + mo->target->height))
        {
            if(delta < 0 && dist < -(delta * 3))
            {
                mo->origin[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->origin[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->origin[VZ] > mo->floorZ &&
       !mo->onMobj && (mapTime & 2))
    {
        mo->origin[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    // jd64 >
    if(mo->origin[VZ] <= mo->floorZ && (mo->flags & MF_MISSILE))
    {
        // Hit the floor
        mo->origin[VZ] = mo->floorZ;
        P_ExplodeMissile(mo);
        return;
    }
    // < jd64

    // Clip movement. Another thing?
    if(mo->onMobj && mo->origin[VZ] <= mo->onMobj->origin[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 &&
               !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }

            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->origin[VZ] = mo->onMobj->origin[VZ] + mo->onMobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // jd64 >
    // MotherDemon's Fire attacks can climb up/down stairs
    // DJS - FIXME!
#if 0
    if((mo->flags & MF_MISSILE) && (mo->type == MT_FIREEND))
    {
        mo->origin[VZ] = mo->floorZ;

        if(mo->type == MT_FIREEND)
        {
            return;
        }
        else
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
#endif
    // < d64tc

    // The floor.
    if(mo->origin[VZ] <= mo->floorZ)
    {   // Hit the floor.
        dd_bool             movingDown;

        // Note (id):
        //  somebody left this after the setting momz to 0,
        //  kinda useless there.
        //
        // cph - This was the a bug in the linuxdoom-1.10 source which
        //  caused it not to sync Doom 2 v1.9 demos. Someone
        //  added the above comment and moved up the following code. So
        //  demos would desync in close lost soul fights.
        // Note that this only applies to original Doom 1 or Doom2 demos - not
        //  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
        //  gameMission. (Note we assume that Doom1 is always Ult Doom, which
        //  seems to hold for most published demos.)
        //
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
        int correctLostSoulBounce = true;

        if(correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
        {
            // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if((movingDown = (mo->mom[MZ] < 0)))
        {
            if(mo->player && mo->player->plr->mo == mo &&
               mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->player->viewHeightDelta = mo->mom[MZ] / 8;
                mo->player->jumpTics = 10;

                /**
                 * DOOM bug:
                 * Dead players would grunt when hitting the ground (e.g.,
                 * after an archvile attack).
                 */
                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }
        }

        mo->origin[VZ] = mo->floorZ;

        if(movingDown)
            P_HitFloor(mo);

        /**
         * See lost soul bouncing comment above. We need this here for bug
         * compatibility with original Doom2 v1.9 - if a soul is charging
         * and hit by a raising floor this would incorrectly reverse it's
         * Y momentum.
         */
        if(!correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
            mo->mom[MZ] = -mo->mom[MZ];

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            if(mo->flags2 & MF2_FLOORBOUNCE)
            {
                P_FloorBounceMissile(mo);
                return;
            }
            else
            {
                P_ExplodeMissile(mo);
                return;
            }
        }

        if(movingDown && mo->mom[MZ] < 0)
            mo->mom[MZ] = 0;
    }
    else if(mo->flags2 & MF2_LOGRAV)
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -(gravity / 8) * 2;
        else
            mo->mom[MZ] -= (gravity / 8);
    }
    else if(!(mo->flags & MF_NOGRAVITY))
    {
        if(mo->mom[MZ] == 0)
            mo->mom[MZ] = -gravity * 2;
        else
            mo->mom[MZ] -= gravity;
    }

    if(mo->origin[VZ] + mo->height > mo->ceilingZ)
    {   // Hit the ceiling.
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->origin[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            // Don't explode against sky.
            if(P_GetIntp(P_GetPtrp(Mobj_Sector(mo), DMU_CEILING_MATERIAL),
                         DMU_FLAGS) & MATF_SKYMASK)
            {
                P_MobjRemove(mo, false);
            }
            else
            {
                P_ExplodeMissile(mo);
            }
        }
    }
}

void P_NightmareRespawn(mobj_t* mobj)
{
    mobj_t*             mo;

    // Something is occupying it's position?
    if(!P_CheckPositionXY(mobj, mobj->spawnSpot.origin[VX],
                          mobj->spawnSpot.origin[VY]))
        return; // No respwan.

    if((mo = P_SpawnMobj(mobj->type, mobj->spawnSpot.origin,
                            mobj->spawnSpot.angle, mobj->spawnSpot.flags)))
    {
        mo->reactionTime = 18;

        // Spawn a teleport fog at old spot.
        if((mo = P_SpawnMobjXYZ(MT_TFOG, mobj->origin[VX], mobj->origin[VY], 0,
                               mobj->angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);

        // Spawn a teleport fog at the new spot.
        if((mo = P_SpawnMobj(MT_TFOG, mobj->spawnSpot.origin,
                                mobj->spawnSpot.angle,
                                mobj->spawnSpot.flags)))
            S_StartSound(SFX_TELEPT, mo);
    }

    // Remove the old monster.
    P_MobjRemove(mobj, true);
}

void P_MobjThinker(void *mobjThinkerPtr)
{
    mobj_t *mobj = mobjThinkerPtr;

    if(mobj->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mobj->type == MT_LIGHTSOURCE)
    {
        if(mobj->moveDir > 0)
            mobj->origin[VZ] = P_GetDoublep(Mobj_Sector(mobj), DMU_FLOOR_HEIGHT);
        else
            mobj->origin[VZ] = P_GetDoublep(Mobj_Sector(mobj), DMU_CEILING_HEIGHT);

        mobj->origin[VZ] += FIX2FLT(mobj->moveDir);
        return;
    }
#endif

    // Handle X and Y momentums.
    if(NON_ZERO(mobj->mom[MX]) || NON_ZERO(mobj->mom[MY]) || (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        //// @todo decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == (thinkfunc_t) NOPFUNC)
            return; // Mobj was removed.
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Floating item bobbing motion.
        // Keep it on the floor.
        mobj->origin[VZ] = mobj->floorZ;
        mobj->floorClip = 0;

        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(!FEQUAL(mobj->origin[VZ], mobj->floorZ) || NON_ZERO(mobj->mom[MZ]))
    {
        P_MobjMoveZ(mobj);
        if(mobj->thinker.function != (thinkfunc_t) P_MobjThinker) // Must've been removed.
            return; // Mobj was removed.
    }
    // Non-sentient objects at rest.
    else if(!(mobj->mom[MX] == 0 && mobj->mom[MY] == 0) && !sentient(mobj) &&
            !(mobj->player) && !((mobj->flags & MF_CORPSE) &&
            cfg.slidingCorpses))
    {
        // Objects fall off ledges if they are hanging off slightly push
        // off of ledge if hanging more than halfway off

        if(mobj->origin[VZ] > mobj->dropOffZ && // Only objects contacting dropoff.
           !(mobj->flags & MF_NOGRAVITY) && cfg.fallOff)
        {
            P_ApplyTorque(mobj);
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    if(cfg.slidingCorpses)
    {
        if(((mobj->flags & MF_CORPSE)? mobj->origin[VZ] > mobj->dropOffZ :
                                       mobj->origin[VZ] - mobj->dropOffZ > 24) && // Only objects contacting drop off
           !(mobj->flags & MF_NOGRAVITY)) // Only objects which fall.
        {
            P_ApplyTorque(mobj); // Apply torque.
        }
        else
        {
            mobj->intFlags &= ~MIF_FALLING;
            mobj->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if(cfg.corpseTime && (mobj->flags & MF_CORPSE) && mobj->corpseTics != -1)
    {
        if(++mobj->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mobj->translucency = 0; // Opaque.
        }
        else if(mobj->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mobj->translucency =
                ((mobj->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mobj->corpseTics = -1;
            return;
        }
    }

    // jd64 >
    // Fade monsters upon spawning.
    if(mobj->intFlags & MIF_FADE)
    {
        if(++mobj->spawnFadeTics < SPAWNFADETICS)
        {
            mobj->translucency = MINMAX_OF(0, 255 -
                255 * mobj->spawnFadeTics / SPAWNFADETICS, 255);
        }
        else
        {
            mobj->intFlags &= ~MIF_FADE;
            mobj->translucency = 0;
        }
    }
    // < d64tc

    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_MobjAngleSRVOTicker(mobj); // "angle-servo"; smooth actor turning.

        // You can cycle through multiple STATES in a tic.
        if(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            if(!P_MobjChangeState(mobj, mobj->state->nextState))
                return; // Freed itself.
        }
    }
    else if(!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!gfw_Rule(respawnMonsters))
            return;

        mobj->moveCount++;

        if(mobj->moveCount < 12 * 35)
            return;

        if(mapTime & 31)
            return;

        if(P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

mobj_t *P_SpawnMobjXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle,
    int spawnFlags)
{
    mobjinfo_t *info;
    int ddflags = 0;
    coord_t space;
    mobj_t *mo;

    if(type < MT_FIRST || type >= Get(DD_NUMMOBJTYPES))
    {
        App_Log(DE2_MAP_ERROR, "Attempt to spawn unknown mobj type %i", type);
        return NULL;
    }

    info = &MOBJINFO[type];

    // Clients only spawn local objects.
    if(!(info->flags & MF_LOCAL) && IS_CLIENT)
        return NULL;

    // Not for deathmatch?
    if(gfw_Rule(deathmatch) && (info->flags & MF_NOTDMATCH))
        return NULL;

    // Check for specific disabled objects.
    if(IS_NETGAME)
    {
        // Cooperative weapons?
        if(cfg.noCoopWeapons && !gfw_Rule(deathmatch) && type >= MT_CLIP &&
           type <= MT_SUPERSHOTGUN)
            return NULL;

        /**
         * @todo cfg.noCoopAnything: Exactly which objects is this supposed to
         * prevent spawning? (at least not MT_PLAYER*...). -jk
         */
#if 0
        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return NULL;
#endif

        // BFG disabled in netgames?
        if(cfg.noNetBFG && type == MT_MISC25)
            return NULL;
    }

    // Don't spawn any monsters?
    if(gfw_Rule(noMonsters) && ((info->flags & MF_COUNTKILL) || type == MT_SKULL))
        return NULL;

    if(info->flags & MF_SOLID)
        ddflags |= DDMF_SOLID;
    if(info->flags2 & MF2_DONTDRAW)
        ddflags |= DDMF_DONTDRAW;

    mo = Mobj_CreateXYZ(P_MobjThinker, x, y, z, angle, info->radius,
                         info->height, ddflags);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health = info->spawnHealth * (IS_NETGAME ? cfg.common.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    // Spectres get selector = 1.
    mo->selector = (type == MT_SHADOWS)? 1 : 0;
    P_UpdateHealthBits(mo); // Set the health bits of the selector.

    mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Must link before setting state (ID assigned for the mo).
    Mobj_SetState(mo, P_GetState(mo->type, SN_SPAWN));

    // Set BSP leaf and/or block links.
    P_MobjLink(mo);

    mo->floorZ   = P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = P_GetDoublep(Mobj_Sector(mo), DMU_CEILING_HEIGHT);

    if((spawnFlags & MSF_Z_CEIL) || (info->flags & MF_SPAWNCEILING))
    {
        mo->origin[VZ] = mo->ceilingZ - mo->info->height - z;
    }
    else if((spawnFlags & MSF_Z_RANDOM) || (info->flags2 & MF2_SPAWNFLOAT))
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->origin[VZ] = ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->origin[VZ] = mo->floorZ;
        }
    }
    else if(spawnFlags & MSF_Z_FLOOR)
    {
        mo->origin[VZ] = mo->floorZ + z;
    }

    /*if(spawnFlags & MTF_FLOAT)
    {
        mo->origin[VZ] += 96;
        mo->flags |= (MF_FLOAT | MF_NOGRAVITY);
    }*/

    if(spawnFlags & MSF_DEAF)
        mo->flags |= MF_AMBUSH;

    mo->floorClip = 0;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       FEQUAL(mo->origin[VZ], P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
    {
        terraintype_t const *tt = P_MobjFloorTerrain(mo);
        if(tt->flags & TTF_FLOORCLIP)
        {
            mo->floorClip = 10;
        }
    }

    /*if(spawnFlags & MTF_WALKOFF)
        mo->flags |= (MF_FLOAT | MF_DROPOFF);

    if(spawnFlags & MTF_TRANSLUCENT)
        mo->flags |= MF_SHADOW;*/

    // Copy spawn attributes to the new mobj.
    mo->spawnSpot.origin[VX] = x;
    mo->spawnSpot.origin[VY] = y;
    mo->spawnSpot.origin[VZ] = z;
    mo->spawnSpot.angle = angle;
    mo->spawnSpot.flags = spawnFlags;

    return mo;
}

mobj_t *P_SpawnMobj(mobjtype_t type, coord_t const pos[3], angle_t angle, int spawnFlags)
{
    return P_SpawnMobjXYZ(type, pos[VX], pos[VY], pos[VZ], angle, spawnFlags);
}

void P_SpawnBlood(coord_t x, coord_t y, coord_t z, int damage, angle_t angle)
{
    mobj_t* mo;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    if((mo = P_SpawnMobjXYZ(MT_BLOOD, x, y, z, angle, 0)))
    {
        mo->mom[MZ] = 2;
        mo->tics -= P_Random() & 3;

        if(mo->tics < 1)
            mo->tics = 1;

        if(damage <= 12 && damage >= 9)
            P_MobjChangeState(mo, S_BLOOD2);
        else if(damage < 9)
            P_MobjChangeState(mo, S_BLOOD3);
    }
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 *
 * @param th            The missile to be checked.
 *
 * @return              @c true, if the missile is at a valid location else
 *                      @c false
 */
dd_bool P_CheckMissileSpawn(mobj_t* mo)
{
    // Move forward slightly so an angle can be computed if it explodes
    // immediately.
    mo->origin[VX] += mo->mom[MX] / 2;
    mo->origin[VY] += mo->mom[MY] / 2;
    mo->origin[VZ] += mo->mom[MZ] / 2;

    if(!P_TryMoveXY(mo, mo->origin[VX], mo->origin[VY], false, false))
    {
        P_ExplodeMissile(mo);
        return false;
    }

    return true;
}

mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest)
{
    coord_t pos[3], spawnZOff = 0, dist = 0;
    angle_t angle = 0;
    //float slope = 0;
    mobj_t *th = 0;
    uint an;

    memcpy(pos, source->origin, sizeof(pos));

    if(source->player)
    {
        // See which target is to be aimed at.
        angle = source->angle;
        /*slope =*/ P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.common.noAutoAim)
            if(!lineTarget)
            {
                angle += 1 << 26;
                /*slope =*/ P_AimLineAttack(source, angle, 16 * 64);

                if(!lineTarget)
                {
                    angle -= 2 << 26;
                    /*slope =*/ P_AimLineAttack(source, angle, 16 * 64);
                }

                if(!lineTarget)
                {
                    angle = source->angle;
                    //slope = tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!P_MobjIsCamera(source->player->plr->mo))
            spawnZOff = cfg.common.plrViewHeight - 9 +
                source->player->plr->lookDir / 173;
    }
    else
    {
        spawnZOff = 32;
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    angle = M_PointToAngle2(pos, dest->origin);

    if(!source->player)
    {
        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            angle += (P_Random() - P_Random()) << 20;
    }

    if(!(th = P_SpawnMobj(type, pos, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Where it came from.
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    /*if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = th->info->speed * slope;
    }
    else*/
    {
        dist = M_ApproxDistance(dest->origin[VX] - pos[VX],
                                dest->origin[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->origin[VZ] - source->origin[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = M_ApproxDistance(M_ApproxDistance(th->mom[MX], th->mom[MY]), th->mom[MZ]);
    if(dist < 1) dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}

/**
 * d64tc
 * dj - It would appear this routine has been stolen from HEXEN and then
 *      butchered...
 * @todo: Make sure this still works correctly.
 */
mobj_t* P_SPMAngle(mobjtype_t type, mobj_t* source, angle_t sourceAngle)
{
    coord_t pos[3], spawnZOff;
    float fangle = LOOKDIR2RAD(source->player->plr->lookDir), movfactor = 1;
    angle_t angle;
    float slope;
    mobj_t* th;
    uint an;

    pos[VX] = source->origin[VX];
    pos[VY] = source->origin[VY];
    pos[VZ] = source->origin[VZ];

    // See which target is to be aimed at.
    angle = sourceAngle;
    slope = P_AimLineAttack(source, angle, 16 * 64);
    if(!lineTarget)
    {
        angle += 1 << 26;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!lineTarget)
        {
            angle -= 2 << 26;
            slope = P_AimLineAttack(source, angle, 16 * 64);
        }

        if(!lineTarget)
        {
            angle = sourceAngle;

            slope = sin(fangle) / 1.2;
            movfactor = cos(fangle);
        }
    }

    if(!P_MobjIsCamera(source->player->plr->mo))
        spawnZOff = cfg.common.plrViewHeight - 9 +
                        source->player->plr->lookDir / 173;
    else
        spawnZOff = 0;

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    if((th = P_SpawnMobj(type, pos, angle, 0)))
    {
        th->target = source;
        an = angle >> ANGLETOFINESHIFT;
        th->mom[MX] = movfactor * th->info->speed * FIX2FLT(finecosine[an]);
        th->mom[MY] = movfactor * th->info->speed * FIX2FLT(finesine[an]);
        th->mom[MZ] = th->info->speed * slope;

        if(th->info->seeSound)
            S_StartSound(th->info->seeSound, th);

        th->tics -= P_Random() & 3;
        if(th->tics < 1)
            th->tics = 1;

        P_CheckMissileSpawn(th);
    }

    return th;
}

/**
 * d64tc
 */
mobj_t* P_SpawnMotherMissile(mobjtype_t type, coord_t x, coord_t y, coord_t z,
    mobj_t* source, mobj_t* dest)
{
    angle_t angle;
    coord_t dist;
    mobj_t* th;
    uint an;

    z -= source->floorClip;

    angle = M_PointXYToAngle2(x, y, dest->origin[VX], dest->origin[VY]);
    if(dest->flags & MF_SHADOW) // Invisible target
    {
        angle += (P_Random() - P_Random()) << 21;
    }

    if(!(th = P_SpawnMobjXYZ(type, x, y, z, angle, 0)))
        return NULL;

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    th->target = source; // Originator
    an = angle >> ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    dist = M_ApproxDistance(dest->origin[VX] - x, dest->origin[VY] - y);
    dist /= th->info->speed;

    if(dist < 1)
        dist = 1;
    th->mom[MZ] = (dest->origin[VZ] - z + 30) / dist;

    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    P_CheckMissileSpawn(th);
    return th;
}
