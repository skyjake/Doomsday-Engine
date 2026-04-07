/** @file p_mobj.c World map object interactions.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "jdoom.h"

#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_terraintype.h"
#include "player.h"
#include "p_tick.h"
#include "p_local.h"
#include "dmu_lib.h"

#define VANISHTICS              (2*TICSPERSEC)

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

/*
static __inline dd_bool isInWalkState(player_t *pl)
{
    return pl->plr->mo->state - STATES - PCLASS_INFO(pl->class_)->runState < 4;
}
*/

void P_MobjMoveXY(mobj_t *mo)
{
    coord_t pos[3], mom[3];
    //player_t *player;
    dd_bool largeNegative;

    // $democam: cameramen have their own movement code.
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
         * Large negative displacements were never considered. This explains
         * the tendency for Mancubus fireballs to pass through walls.
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
            pos[VX] = mo->origin[VX] + mom[MX] / 2;
            pos[VY] = mo->origin[VY] + mom[MY] / 2;
            mom[MX] /= 2;
            mom[MY] /= 2;
        }
        else
        {
            pos[VX] = mo->origin[VX] + mom[MX];
            pos[VY] = mo->origin[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
        {
            mo->wallRun = false;
        }

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
                Sector *backSec;

                /// @kludge Prevent missiles exploding against the sky.
                if(tmCeilingLine &&
                   (backSec = P_GetPtrp(tmCeilingLine, DMU_BACK_SECTOR)))
                {
                    world_Material *mat = P_GetPtrp(backSec, DMU_CEILING_MATERIAL);

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
                    world_Material *mat = P_GetPtrp(backSec, DMU_FLOOR_MATERIAL);

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
    coord_t floorHeight = P_GetDoublep(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->origin[VZ] < floorHeight &&
       mo->origin[VZ] + mo->height / 2 > floorHeight)
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

void P_MobjMoveZ(mobj_t* mo)
{
    coord_t gravity, targetZ, floorZ, ceilingZ;

    // $democam: cameramen get special z movement.
    if(P_CameraZMovement(mo))
        return;

    targetZ = mo->origin[VZ] + mo->mom[MZ];
    floorZ = (mo->onMobj? mo->onMobj->origin[VZ] + mo->onMobj->height : mo->floorZ);
    ceilingZ = mo->ceilingZ;
    gravity = XS_Gravity(Mobj_Sector(mo));

    if((mo->flags2 & MF2_FLY) && mo->player &&
       mo->onMobj && mo->origin[VZ] > mo->onMobj->origin[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
       mo->target && !P_MobjIsCamera(mo->target))
    {
        coord_t dist, delta;

        // Float down towards target if too close.
        dist = M_ApproxDistance(mo->origin[VX] - mo->target->origin[VX],
                                mo->origin[VY] - mo->target->origin[VY]);

        delta = (mo->target->origin[VZ] + mo->target->height / 2) -
                (mo->origin[VZ] + mo->height / 2);

        // Don't go INTO the target.
        if(!(dist < mo->radius + mo->target->radius &&
             fabs(delta) < mo->height + mo->target->height))
        {
            if(delta < 0 && dist < -(delta * 3))
            {
                targetZ -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                targetZ += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->origin[VZ] > floorZ &&
       (mapTime & 2))
    {
        targetZ += FIX2FLT(finesine[(FINEANGLES / 20 * mapTime >> 2) & FINEMASK]);
    }

    if(targetZ < floorZ)
    {   // Hit the floor (or another mobj).
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
        dd_bool const correctLostSoulBounce = (gameMode == doom2_plut || gameMode == doom2_tnt);

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

        targetZ = floorZ;

        if(movingDown && !mo->onMobj)
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
            mo->origin[VZ] = targetZ;

            if((mo->flags2 & MF2_FLOORBOUNCE) && !mo->onMobj)
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

        // $voodoodolls: Check for smooth step up unless a voodoo doll.
        if(mo->player && mo->player->plr->mo == mo &&
           mo->origin[VZ] < mo->floorZ)
        {
            mo->player->viewHeight -= (mo->floorZ - mo->origin[VZ]);
            mo->player->viewHeightDelta =
                (cfg.common.plrViewHeight - mo->player->viewHeight) / 8;
        }

        mo->origin[VZ] = floorZ;

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            world_Material *mat = P_GetPtrp(Mobj_Sector(mo), DMU_FLOOR_MATERIAL);

            // Don't explode against sky.
            if(P_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK)
            {
                P_MobjRemove(mo, false);
            }
            else
            {
                P_ExplodeMissile(mo);
            }
        }
    }
    else
    {
        if(targetZ + mo->height > ceilingZ)
        {
            // Hit the ceiling.
            if(mo->mom[MZ] > 0)
                mo->mom[MZ] = 0;

            mo->origin[VZ] = mo->ceilingZ - mo->height;

            if(mo->flags & MF_SKULLFLY)
            {   // The skull slammed into something.
                mo->mom[MZ] = -mo->mom[MZ];
            }

            if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
            {
                world_Material *mat = P_GetPtrp(Mobj_Sector(mo), DMU_CEILING_MATERIAL);

                // Don't explode against sky.
                if(P_GetIntp(mat, DMU_FLAGS) & MATF_SKYMASK)
                {
                    P_MobjRemove(mo, false);
                }
                else
                {
                    P_ExplodeMissile(mo);
                }
            }
        }
        else
        {   // In "free space".
            // Update gravity's effect on momentum.
            if(mo->flags2 & MF2_LOGRAV)
            {
                if(IS_ZERO(mo->mom[MZ]))
                    mo->mom[MZ] = -(gravity / 8) * 2;
                else
                    mo->mom[MZ] -= (gravity / 8);
            }
            else if(!(mo->flags & MF_NOGRAVITY))
            {
                if(IS_ZERO(mo->mom[MZ]))
                    mo->mom[MZ] = -gravity * 2;
                else
                    mo->mom[MZ] -= gravity;
            }
            mo->origin[VZ] = targetZ;
        }
    }
}

void P_NightmareRespawn(mobj_t* corpse)
{
    mobj_t* mo;

    // Something is occupying it's position?
    if(!P_CheckPositionXY(corpse, corpse->spawnSpot.origin[VX],
                                  corpse->spawnSpot.origin[VY]))
        return; // No respwan.

    if((mo = P_SpawnMobj(corpse->type, corpse->spawnSpot.origin,
                                       corpse->spawnSpot.angle,
                                       corpse->spawnSpot.flags)))
    {
        mo->reactionTime = 18;

        // Spawn a teleport fog at old spot.
        if((mo = P_SpawnMobjXYZ(MT_TFOG, corpse->origin[VX], corpse->origin[VY], 0,
                                         corpse->angle, MSF_Z_FLOOR)))
            S_StartSound(SFX_TELEPT, mo);

        // Spawn a teleport fog at the new spot.
        if((mo = P_SpawnMobj(MT_TFOG, corpse->spawnSpot.origin,
                                      corpse->spawnSpot.angle,
                                      corpse->spawnSpot.flags)))
            S_StartSound(SFX_TELEPT, mo);
    }

    // Remove the old monster.
    P_MobjRemove(corpse, true);
}

void P_MobjThinker(void *thinkerPtr)
{
    mobj_t *mo = thinkerPtr;
    coord_t floorZ;

    if(!mo) return; // Wha?

    if(IS_CLIENT && !ClMobj_IsValid(mo))
        return; // We should not touch this right now.

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mo);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mo->type == MT_LIGHTSOURCE)
    {
        mo->origin[VZ] = P_GetDoublep(Mobj_Sector(mo),
                                      mo->moveDir > 0? DMU_FLOOR_HEIGHT : DMU_CEILING_HEIGHT);
                         + FIX2FLT(mo->moveDir);
        return;
    }
#endif

    // Handle X and Y momentums.
    if(!INRANGE_OF(mo->mom[MX], 0, NOMOM_THRESHOLD) ||
       !INRANGE_OF(mo->mom[MY], 0, NOMOM_THRESHOLD) ||
       (mo->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mo);

        /// @todo decent NOP/NULL/Nil function pointer please.
        if(mo->thinker.function == (thinkfunc_t) NOPFUNC)
            return; // Mobj was removed.
    }

    floorZ = (mo->onMobj? mo->onMobj->origin[VZ] + mo->onMobj->height : mo->floorZ);

    if(mo->flags2 & MF2_FLOATBOB)
    {
        // Floating item bobbing motion.
        // Keep it on the floor.
        mo->origin[VZ] = floorZ;
#if __JHERETIC__
        // Negative floorclip raises the mo off the floor.
        mo->floorClip = -mo->special1;
#elif __JDOOM__
        mo->floorClip = 0;
#endif
        if(mo->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mo->floorClip = -MAX_BOB_OFFSET;
        }
    }
    else if(mo->origin[VZ] != floorZ ||
            !INRANGE_OF(mo->mom[MZ], 0, NOMOM_THRESHOLD))
    {
        P_MobjMoveZ(mo);

        //// @todo decent NOP/NULL/Nil function pointer please.
        if(mo->thinker.function == (thinkfunc_t) NOPFUNC)
            return; // Mobj was removed.
    }
    // Non-sentient objects at rest.
    else if(!sentient(mo) && !mo->player &&
            !(INRANGE_OF(mo->mom[MX], 0, NOMOM_THRESHOLD) &&
              INRANGE_OF(mo->mom[MY], 0, NOMOM_THRESHOLD)))
    {
        // Objects fall off ledges if they are hanging off. Slightly push
        // off of ledge if hanging more than halfway off.

        if(mo->origin[VZ] > mo->dropOffZ && // Only objects contacting dropoff.
           !(mo->flags & MF_NOGRAVITY) &&
           !(mo->flags2 & MF2_FLOATBOB) && cfg.fallOff)
        {
            P_ApplyTorque(mo);
        }
        else
        {
            mo->intFlags &= ~MIF_FALLING;
            mo->gear = 0; // Reset torque.
        }
    }

    if(cfg.slidingCorpses)
    {
        if(((mo->flags & MF_CORPSE)? mo->origin[VZ] > mo->dropOffZ :
                                       mo->origin[VZ] - mo->dropOffZ > 24) && // Only objects contacting drop off
           !(mo->flags & MF_NOGRAVITY)) // Only objects which fall.
        {
            P_ApplyTorque(mo); // Apply torque.
        }
        else
        {
            mo->intFlags &= ~MIF_FALLING;
            mo->gear = 0; // Reset torque.
        }
    }

    // $vanish: dead monsters disappear after some time.
    if(cfg.corpseTime && (mo->flags & MF_CORPSE) && mo->corpseTics != -1)
    {
        if(++mo->corpseTics < cfg.corpseTime * TICSPERSEC)
        {
            mo->translucency = 0; // Opaque.
        }
        else if(mo->corpseTics < cfg.corpseTime * TICSPERSEC + VANISHTICS)
        {
            // Translucent during vanishing.
            mo->translucency =
                ((mo->corpseTics -
                  cfg.corpseTime * TICSPERSEC) * 255) / VANISHTICS;
        }
        else
        {
            // Too long; get rid of the corpse.
            mo->corpseTics = -1;
            return;
        }
    }

    // Update "angle-srvo" (smooth actor turning).
    P_MobjAngleSRVOTicker(mo);

    // Cycle through states, calling action functions at transitions.
    if(mo->tics != -1)
    {
        mo->tics--;

        // You can cycle through multiple states in a tic.
        if(!mo->tics)
        {
            P_MobjClearSRVO(mo);
            P_MobjChangeState(mo, mo->state->nextState);
        }
    }
    else if(!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if(!(mo->flags & MF_COUNTKILL))
            return;

        if(!gfw_Rule(respawnMonsters))
            return;

        mo->moveCount++;

        if(mo->moveCount >= 12 * 35 && !(mapTime & 31) &&
           P_Random() <= 4)
        {
            P_NightmareRespawn(mo);
        }
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t* P_SpawnMobjXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle,
    int spawnFlags)
{
    mobj_t* mo;
    mobjinfo_t* info;
    coord_t space;
    int ddflags = 0;

    info = &MOBJINFO[type];

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

    switch(type)
    {
    case MT_BABY: // 68, Arachnotron
    case MT_VILE: // 64, Archvile
    case MT_BOSSBRAIN: // 88, Boss Brain
    case MT_BOSSSPIT: // 89, Boss Shooter
    case MT_KNIGHT: // 69, Hell Knight
    case MT_FATSO: // 67, Mancubus
    case MT_PAIN: // 71, Pain Elemental
    case MT_MEGA: // 74, MegaSphere
    case MT_CHAINGUY: // 65, Former Human Commando
    case MT_UNDEAD: // 66, Revenant
    case MT_WOLFSS: // 84, Wolf SS
        if(!(gameModeBits & GM_ANY_DOOM2))
            return NULL;
        break;

    default:
        break;
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

    // Let the engine know about solid objects.
    P_SetDoomsdayFlags(mo);

    if(gfw_Rule(skill) != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Do not set the state with P_MobjChangeState, because action routines
    // can not be called yet.

    // Must link before setting state (ID assigned for the mo).
    Mobj_SetState(mo, P_GetState(mo->type, SN_SPAWN));
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

    if(type == MT_BOSSTARGET)
    {
        BossBrain_AddTarget(theBossBrain, mo);
    }

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
    mobj_t *blood;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    if((blood = P_SpawnMobjXYZ(MT_BLOOD, x, y, z, angle, 0)))
    {
        blood->mom[MZ] = 2;

        blood->tics -= P_Random() & 3;
        if(blood->tics < 1) blood->tics = 1;

        if(damage <= 12 && damage >= 9)
        {
            P_MobjChangeState(blood, S_BLOOD2);
        }
        else if(damage < 9)
        {
            P_MobjChangeState(blood, S_BLOOD3);
        }
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
dd_bool P_CheckMissileSpawn(mobj_t* th)
{
    // Move forward slightly so an angle can be computed if it explodes
    // immediately.
    /// @todo Optimize: Why not simply spawn at this location? -ds
    P_MobjUnlink(th);
    th->origin[VX] += th->mom[MX] / 2;
    th->origin[VY] += th->mom[MY] / 2;
    th->origin[VZ] += th->mom[MZ] / 2;
    P_MobjLink(th);

    if(!P_TryMoveXY(th, th->origin[VX], th->origin[VY], false, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

/**
 * Tries to aim at a nearby monster if source is a player. Else aim is
 * taken at dest.
 *
 * @param source        The mobj doing the shooting.
 * @param dest          The mobj being shot at. Can be @c NULL if source
 *                      is a player.
 * @param type          The type of mobj to be shot.
 *
 * @return              Pointer to the newly spawned missile.
 */
mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest)
{
    coord_t pos[3];
    mobj_t* th = 0;
    unsigned int an;
    angle_t angle = 0;
    coord_t dist = 0;
    float slope = 0;
    coord_t spawnZOff = 0;

    memcpy(pos, source->origin, sizeof(pos));

    if(source->player)
    {
        // See which target is to be aimed at.
        angle = source->angle;
        slope = P_AimLineAttack(source, angle, 16 * 64);
        if(!cfg.common.noAutoAim)
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
                    angle = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!P_MobjIsCamera(source->player->plr->mo))
            spawnZOff = cfg.common.plrViewHeight - 9 +
                source->player->plr->lookDir / 173;
    }
    else
    {
        // Type specific offset to spawn height z.
        switch(type)
        {
        case MT_TRACER: // Revenant Tracer Missile.
            spawnZOff = 16 + 32;
            break;

        default:
            spawnZOff = 32;
            break;
        }
    }

    pos[VZ] += spawnZOff;
    pos[VZ] -= source->floorClip;

    if(!source->player)
    {
        angle = M_PointToAngle2(pos, dest->origin);

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

    if(source->player)
    {
        // Allow free-aim with the BFG in deathmatch?
        if(gfw_Rule(deathmatch) && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = th->info->speed * slope;
    }
    else
    {
        dist = M_ApproxDistance(dest->origin[VX] - pos[VX],
                                dest->origin[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->origin[VZ] - source->origin[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = M_ApproxDistance(M_ApproxDistance(th->mom[MX], th->mom[MY]),
                            th->mom[MZ]);
    if(dist < 1)
        dist = 1;
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
