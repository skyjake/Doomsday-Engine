/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/**
 * p_mobj.c: Moving object handling. Spawn functions.
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jdoom.h"

#include "hu_stuff.h"
#include "g_common.h"
#include "p_map.h"
#include "p_player.h"
#include "p_tick.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define STOPSPEED               (1.0f/1.6/10)
#define DROPOFFMOMENTUM_THRESHOLD (1.0f / 4)

#define ITEMQUESIZE             (128)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static spawnspot_t itemRespawnQueue[ITEMQUESIZE];
static int itemRespawnTime[ITEMQUESIZE];
static int itemRespawnQueueHead, itemRespawnQueueTail;

// CODE --------------------------------------------------------------------

/**
 * @return              @c true, if the mobj is still present.
 */
boolean P_MobjChangeState(mobj_t *mobj, statenum_t state)
{
    state_t            *st;

    do
    {
        if(state == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        P_MobjSetState(mobj, state);
        st = &states[state];

        mobj->turnTime = false; // $visangle-facetarget

        // Modified handling.
        // Call action functions when the state is set.
        if(st->action)
            st->action(mobj);

        state = st->nextState;
    } while(!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        // Clients won't explode missiles.
        P_MobjChangeState(mo, S_NULL);
        return;
    }

    mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

    P_MobjChangeState(mo, mobjInfo[mo->type].deathState);

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
    P_MobjChangeState(mo, mobjInfo[mo->type].deathState);
}

/**
 * @return              The ground friction factor for the mobj.
 */
float P_MobjGetFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        return FRICTION_FLY;
    }

    return XS_Friction(P_GetPtrp(mo->subsector, DMU_SECTOR));
}

static boolean isInWalkState(player_t* pl)
{
    return pl->plr->mo->state - states -
                PCLASS_INFO(pl->class)->runState < 4;
}

static float getFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) &&
       !mo->onMobj)
    {   // Airborne friction.
        return FRICTION_FLY;
    }

#if __JHERETIC__
    if(P_ToXSector(P_GetPtrp(mo->subsector, DMU_SECTOR))->special == 15)
    {   // Friction_Low
        return FRICTION_LOW;
    }
#endif

    return P_MobjGetFriction(mo);
}

void P_MobjMoveXY(mobj_t *mo)
{
    float               pos[3], mom[3];
    player_t           *player;
    boolean             largeNegative;

    // $democam: cameramen have their own movement code.
    if(P_CameraXYMovement(mo))
        return;

    if(INRANGEOF(mo->mom[MX], 0, NOMOMENTUM_THRESHOLD) &&
       INRANGEOF(mo->mom[MY], 0, NOMOMENTUM_THRESHOLD))
    {
        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->flags &= ~MF_SKULLFLY;
            mo->mom[MX] = mo->mom[MY] = mo->mom[MZ] = 0;

            P_MobjChangeState(mo, mo->info->spawnState);
        }

        return;
    }

    mom[MX] = mo->mom[MX] = CLAMP(mo->mom[MX], -MAXMOVE, MAXMOVE);
    mom[MY] = mo->mom[MY] = CLAMP(mo->mom[MY], -MAXMOVE, MAXMOVE);

    player = mo->player;

    do
    {
        /**
         * DOOM.exe bug fix:
         * Large negative displacements were never considered. This explains
         * the tendency for Mancubus fireballs to pass through walls.
         */

        largeNegative = false;
        if(!cfg.moveBlock &&
           (mom[MX] < -MAXMOVE / 2 || mom[MY] < -MAXMOVE / 2))
        {
            // Make an exception for "north-only wallrunning".
            if(!(cfg.wallRunNorthOnly && mo->wallRun))
                largeNegative = true;
        }

        if(largeNegative || mom[MX] > MAXMOVE / 2 || mom[MY] > MAXMOVE / 2)
        {
            pos[VX] = mo->pos[VX] + mom[MX] / 2;
            pos[VY] = mo->pos[VY] + mom[MY] / 2;
            mom[VX] /= 2;
            mom[VY] /= 2;
        }
        else
        {
            pos[VX] = mo->pos[VX] + mom[MX];
            pos[VY] = mo->pos[VY] + mom[MY];
            mom[MX] = mom[MY] = 0;
        }

        // If mobj was wallrunning - stop.
        if(mo->wallRun)
            mo->wallRun = false;

        // $dropoff_fix.
        if(!P_TryMove(mo, pos[VX], pos[VY], true, false))
        {   // Blocked move.
            if(mo->flags2 & MF2_SLIDE)
            {   // Try to slide along it.
                P_SlideMove(mo);
            }
            else if(mo->flags & MF_MISSILE)
            {
                if(ceilingLine)
                {
                    sector_t   *backsector =
                        P_GetPtrp(ceilingLine, DMU_BACK_SECTOR);

                    if(backsector)
                    {
                        if(P_GetIntp(backsector,
                                     DMU_CEILING_MATERIAL) == SKYMASKMATERIAL &&
                           mo->pos[VZ] > P_GetFloatp(backsector, DMU_CEILING_HEIGHT))
                        {
                            // Hack to prevent missiles exploding against
                            // the sky. Does not handle sky floors.
                            P_MobjRemove(mo, false);
                            return;
                        }
                    }
                }

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->mom[MX] = mo->mom[MY] = 0;
            }
        }
    } while(!INRANGEOF(mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
            !INRANGEOF(mom[MY], 0, NOMOMENTUM_THRESHOLD));

    // Slow down.
    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
        return; // No friction for missiles ever.

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj /*&& !(mo->flags2 & MF2_FLY)*/)
        return; // No friction when falling.

    if(cfg.slidingCorpses)
    {
        // $dropoff_fix: Add objects falling off ledges, does not apply to
        // players!
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) &&
           !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(!INRANGEOF(mo->mom[MX], 0, DROPOFFMOMENTUM_THRESHOLD) ||
               !INRANGEOF(mo->mom[MY], 0, DROPOFFMOMENTUM_THRESHOLD))
            {
                if(mo->floorZ !=
                   P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }

    // Stop player walking animation.
    if((!player || !(player->plr->cmd.forwardMove | player->plr->cmd.sideMove) ||
         player->plr->mo != mo /* $voodoodolls: Stop also. */) &&
       INRANGEOF(mo->mom[MX], 0, STOPSPEED) &&
       INRANGEOF(mo->mom[MY], 0, STOPSPEED))
    {
        // If in a walking frame, stop moving.
        if(player && isInWalkState(player) && player->plr->mo == mo)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class)->normalState);

        mo->mom[MX] = mo->mom[MY] = 0;

        // $voodoodolls: Stop view bobbing if this isn't a voodoo doll.
        if(player && player->plr->mo == mo)
            player->bob = 0;
    }
    else
    {
        float       friction = getFriction(mo);

        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

/*
static boolean PIT_Splash(sector_t *sector, void *data)
{
    mobj_t             *mo = data;
    float               floorHeight;

    floorHeight = P_GetFloatp(sector, DMU_FLOOR_HEIGHT);

    // Is the mobj touching the floor of this sector?
    if(mo->pos[VZ] < floorHeight &&
       mo->pos[VZ] + mo->height / 2 > floorHeight)
    {
        //// \todo Play a sound, spawn a generator, etc.
    }

    // Continue checking.
    return true;
}
*/

void P_RipperBlood(mobj_t *mo)
{
    mobj_t             *th;
    float               pos[3];

    memcpy(pos, mo->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 12);
    pos[VZ] += FIX2FLT((P_Random() - P_Random()) << 12);

    th = P_SpawnMobj3fv(MT_BLOOD, pos);
    th->flags |= MF_NOGRAVITY;
    th->mom[MX] /= 2;
    th->mom[MY] /= 2;

    th->tics += P_Random() & 3;
}

int P_MobjGetFloorTerrainType(mobj_t *thing)
{
    return P_GetTerrainType(P_GetPtrp(thing->subsector, DMU_SECTOR), PLN_FLOOR);
}

void P_HitFloor(mobj_t *mo)
{
    //P_MobjSectorsIterator(mo, PIT_Splash, mo);
}

void P_MobjMoveZ(mobj_t *mo)
{
    float               gravity, dist, delta;
    boolean             movingDown;

    gravity = XS_Gravity(P_GetPtrp(mo->subsector, DMU_SECTOR));

    // $democam: cameramen get special z movement.
    if(P_CameraZMovement(mo))
        return;

    // $voodoodolls: Check for smooth step up unless a voodoo doll.
    if(mo->player && mo->player->plr->mo == mo &&
       mo->pos[VZ] < mo->floorZ)
    {
        mo->dPlayer->viewHeight -= mo->floorZ - mo->pos[VZ];
        mo->dPlayer->viewHeightDelta =
            (cfg.plrViewHeight - mo->dPlayer->viewHeight) / 8;
    }

    // Adjust height.
    mo->pos[VZ] += mo->mom[MZ];

    if((mo->flags2 & MF2_FLY) &&
       mo->onMobj && mo->pos[VZ] > mo->onMobj->pos[VZ] + mo->onMobj->height)
        mo->onMobj = NULL; // We were on a mobj, we are NOT now.

    if(!((mo->flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) &&
       mo->target && !P_IsCamera(mo->target))
    {
        // Float down towards target if too close.
        dist = P_ApproxDistance(mo->pos[VX] - mo->target->pos[VX],
                                mo->pos[VY] - mo->target->pos[VY]);

        //delta = (mo->target->pos[VZ] + (mo->height / 2)) - mo->pos[VZ];
        delta = (mo->target->pos[VZ] + mo->target->height / 2) -
                (mo->pos[VZ] + mo->height / 2);

        // Don't go INTO the target.
        if(!(dist < mo->radius + mo->target->radius &&
             fabs(delta) < mo->height + mo->target->height))
        {
            if(delta < 0 && dist < -(delta * 3))
            {
                mo->pos[VZ] -= FLOATSPEED;
                P_MobjSetSRVOZ(mo, -FLOATSPEED);
            }
            else if(delta > 0 && dist < (delta * 3))
            {
                mo->pos[VZ] += FLOATSPEED;
                P_MobjSetSRVOZ(mo, FLOATSPEED);
            }
        }
    }

    // Do some fly-bobbing.
    if(mo->player && (mo->flags2 & MF2_FLY) && mo->pos[VZ] > mo->floorZ &&
       !mo->onMobj && (levelTime & 2))
    {
        mo->pos[VZ] += FIX2FLT(finesine[(FINEANGLES / 20 * levelTime >> 2) & FINEMASK]);
    }

    // Clip movement. Another thing?
    if(mo->onMobj && mo->pos[VZ] <= mo->onMobj->pos[VZ] + mo->onMobj->height)
    {
        if(mo->mom[MZ] < 0)
        {
            if(mo->player && mo->mom[MZ] < -gravity * 8 &&
               !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->dPlayer->viewHeightDelta = mo->mom[MZ] / 8;

                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }

            mo->mom[MZ] = 0;
        }

        if(mo->mom[MZ] == 0)
            mo->pos[VZ] = mo->onMobj->pos[VZ] + mo->onMobj->height;

        if((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }

    // The floor.
    if(mo->pos[VZ] <= mo->floorZ)
    {   // Hit the floor.
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
        int correctLostSoulBounce
                = (gameMode == retail || gameMode == commercial) &&
                    gameMission != GM_DOOM2;

        if(correctLostSoulBounce && (mo->flags & MF_SKULLFLY))
        {
            // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(movingDown = (mo->mom[MZ] < 0))
        {
            if(mo->player && mo->player->plr->mo == mo &&
               mo->mom[MZ] < -gravity * 8 && !(mo->flags2 & MF2_FLY))
            {
                // Squat down.
                // Decrease viewheight for a moment after hitting the ground
                // (hard), and utter appropriate sound.
                mo->dPlayer->viewHeightDelta = mo->mom[MZ] / 8;
                mo->player->jumpTics = 10;

                /**
                 * DOOM bug:
                 * Dead players would grunt when hitting the ground (e.g.,
                 * after an archvile attack).
                 */
                if(mo->player->health > 0)
                    S_StartSound(SFX_OOF, mo);
            }

            mo->mom[MZ] = 0;
        }

        mo->pos[VZ] = mo->floorZ;

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

    if(mo->pos[VZ] + mo->height > mo->ceilingZ)
    {   // Hit the ceiling.
        if(mo->mom[MZ] > 0)
            mo->mom[MZ] = 0;

        mo->pos[VZ] = mo->ceilingZ - mo->height;

        if(mo->flags & MF_SKULLFLY)
        {   // The skull slammed into something.
            mo->mom[MZ] = -mo->mom[MZ];
        }

        if(!((mo->flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            // Don't explode against sky.
            if(P_GetIntp(mo->subsector, DMU_CEILING_MATERIAL) ==
               SKYMASKMATERIAL)
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

void P_NightmareRespawn(mobj_t *mobj)
{
    float               pos[3];
    subsector_t        *ss;
    mobj_t             *mo;

    pos[VX] = mobj->spawnSpot.pos[VX];
    pos[VY] = mobj->spawnSpot.pos[VY];
    pos[VZ] = mobj->spawnSpot.pos[VZ];

    // Something is occupying it's position?
    if(!P_CheckPosition2f(mobj, pos[VX], pos[VY]))
        return; // No respwan.

    // Spawn a teleport fog at old spot.
    mo = P_SpawnMobj3f(MT_TFOG, mobj->pos[VX], mobj->pos[VY],
                       P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT));
    S_StartSound(SFX_TELEPT, mo);

    // Spawn a teleport fog at the new spot.
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    mo = P_SpawnMobj3f(MT_TFOG, pos[VX], pos[VY],
                       P_GetFloatp(ss, DMU_FLOOR_HEIGHT));
    S_StartSound(SFX_TELEPT, mo);

    if(mobj->info->flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    // Inherit attributes from deceased one.
    mo = P_SpawnMobj3fv(mobj->type, pos);
    memcpy(&mo->spawnSpot, &mobj->spawnSpot, sizeof(mo->spawnSpot));
    mo->angle = mobj->spawnSpot.angle;

    if(mobj->spawnSpot.flags & MTF_DEAF)
        mo->flags |= MF_AMBUSH;

    mo->reactionTime = 18;

    // Remove the old monster.
    P_MobjRemove(mobj, true);
}

void P_MobjThinker(mobj_t *mobj)
{
    if(mobj->ddFlags & DDMF_REMOTE)
        return; // Remote mobjs are handled separately.

    // Spectres get selector = 1.
    if(mobj->type == MT_SHADOWS)
        mobj->selector = (mobj->selector & ~DDMOBJ_SELECTOR_MASK) | 1;

    // The first three bits of the selector special byte contain a
    // relative health level.
    P_UpdateHealthBits(mobj);

#if __JHERETIC__
    // Lightsources must stay where they're hooked.
    if(mobj->type == MT_LIGHTSOURCE)
    {
        if(mobj->moveDir > 0)
            mobj->pos[VZ] =
                P_GetFloatp(mobj->subsector, DMU_FLOOR_HEIGHT);
        else
            mobj->pos[VZ] =
                P_GetFloatp(mobj->subsector, DMU_CEILING_HEIGHT);

        mobj->pos[VZ] += FIX2FLT(mobj->moveDir);
        return;
    }
#endif

    // Handle X and Y momentums.
    if(!INRANGEOF(mobj->mom[MX], 0, NOMOMENTUM_THRESHOLD) ||
       !INRANGEOF(mobj->mom[MY], 0, NOMOMENTUM_THRESHOLD) ||
       (mobj->flags & MF_SKULLFLY))
    {
        P_MobjMoveXY(mobj);

        //// \fixme decent NOP/NULL/Nil function pointer please.
        if(mobj->thinker.function == NOPFUNC)
            return; // Mobj was removed.
    }

    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Floating item bobbing motion.
        // Keep it on the floor.
        mobj->pos[VZ] = mobj->floorZ;
#if __JHERETIC__
        // Negative floorclip raises the mobj off the floor.
        mobj->floorClip = -mobj->special1;
#elif __JDOOM__
        mobj->floorClip = 0;
#endif
        if(mobj->floorClip < -MAX_BOB_OFFSET)
        {
            // We don't want it going through the floor.
            mobj->floorClip = -MAX_BOB_OFFSET;
        }

        // Old floatbob used health as index, let's still increase it
        // as before (in case somebody wants to use it).
        mobj->health++;
    }
    else if(mobj->pos[VZ] != mobj->floorZ || mobj->mom[MZ] != 0)
    {
        P_MobjMoveZ(mobj);
        if(mobj->thinker.function != P_MobjThinker) // Must've been removed.
            return; // Mobj was removed.
    }
    // Non-sentient objects at rest.
    else if(!sentient(mobj) && !mobj->player &&
            !(INRANGEOF(mobj->mom[MX], 0, NOMOMENTUM_THRESHOLD) &&
              INRANGEOF(mobj->mom[MY], 0, NOMOMENTUM_THRESHOLD)))
    {
        // Objects fall off ledges if they are hanging off. Slightly push
        // off of ledge if hanging more than halfway off.

        if(mobj->pos[VZ] > mobj->dropOffZ && // Only objects contacting dropoff.
           !(mobj->flags & MF_NOGRAVITY) &&
           !(mobj->flags2 & MF2_FLOATBOB) && cfg.fallOff)
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
        if(((mobj->flags & MF_CORPSE)? mobj->pos[VZ] > mobj->dropOffZ :
                                       mobj->pos[VZ] - mobj->dropOffZ > 24) && // Only objects contacting drop off
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

    // Cycle through states, calling action functions at transitions.
    if(mobj->tics != -1)
    {
        mobj->tics--;

        P_MobjAngleSRVOTicker(mobj); // "angle-servo"; smooth actor turning.

        // You can cycle through multiple states in a tic.
        if(!mobj->tics)
        {
            P_MobjClearSRVO(mobj);
            P_MobjChangeState(mobj, mobj->state->nextState);
        }
    }
    else if(!IS_CLIENT)
    {
        // Check for nightmare respawn.
        if(!(mobj->flags & MF_COUNTKILL))
            return;

        if(!respawnMonsters)
            return;

        mobj->moveCount++;

        if(mobj->moveCount >= 12 * 35 && !(levelTime & 31) &&
           P_Random() <= 4)
        {
            P_NightmareRespawn(mobj);
        }
    }
}

/**
 * Spawns a mobj of "type" at the specified position.
 */
mobj_t *P_SpawnMobj3f(mobjtype_t type, float x, float y, float z)
{
    mobj_t             *mo;
    mobjinfo_t         *info = &mobjInfo[type];
    float               space;

    mo = P_MobjCreate(P_MobjThinker, x, y, z, 0, info->radius, info->height,
                      0);
    mo->type = type;
    mo->info = info;
    mo->flags = info->flags;
    mo->flags2 = info->flags2;
    mo->flags3 = info->flags3;
    mo->damage = info->damage;
    mo->health =
        info->spawnHealth * (IS_NETGAME ? cfg.netMobHealthModifier : 1);
    mo->moveDir = DI_NODIR;

    // Let the engine know about solid objects.
    P_SetDoomsdayFlags(mo);

    if(gameSkill != SM_NIGHTMARE)
        mo->reactionTime = info->reactionTime;

    mo->lastLook = P_Random() % MAXPLAYERS;

    // Do not set the state with P_MobjChangeState, because action routines
    // can not be called yet.

    // Must link before setting state (ID assigned for the mo).
    P_MobjSetState(mo, info->spawnState);
    P_MobjSetPosition(mo);

    mo->floorZ   = P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT);
    mo->dropOffZ = mo->floorZ;
    mo->ceilingZ = P_GetFloatp(mo->subsector, DMU_CEILING_HEIGHT);

    if(mo->pos[VZ] == ONFLOORZ)
    {
        mo->pos[VZ] = mo->floorZ;
    }
    else if(mo->pos[VZ] == ONCEILINGZ)
    {
        mo->pos[VZ] = mo->ceilingZ - mo->info->height;
    }
    else if(mo->pos[VZ] == FLOATRANDZ)
    {
        space = mo->ceilingZ - mo->info->height - mo->floorZ;
        if(space > 48)
        {
            space -= 40;
            mo->pos[VZ] = ((space * P_Random()) / 256) + mo->floorZ + 40;
        }
        else
        {
            mo->pos[VZ] = mo->floorZ;
        }
    }

    if((mo->flags2 & MF2_FLOORCLIP) &&
       P_MobjGetFloorTerrainType(mo) >= FLOOR_LIQUID &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        mo->floorClip = 10;
    }
    else
    {
        mo->floorClip = 0;
    }

    return mo;
}

mobj_t *P_SpawnMobj3fv(mobjtype_t type, float pos[3])
{
    return P_SpawnMobj3f(type, pos[VX], pos[VY], pos[VZ]);
}

/**
 * Queue up a spawn from the specified spot.
 */
void P_RespawnEnqueue(spawnspot_t *spot)
{
    spawnspot_t        *spawnobj = &itemRespawnQueue[itemRespawnQueueHead];

    memcpy(spawnobj, spot, sizeof(*spawnobj));

    itemRespawnTime[itemRespawnQueueHead] = levelTime;
    itemRespawnQueueHead = (itemRespawnQueueHead + 1) & (ITEMQUESIZE - 1);

    // Lose one off the end?
    if(itemRespawnQueueHead == itemRespawnQueueTail)
        itemRespawnQueueTail =
        (itemRespawnQueueTail + 1) & (ITEMQUESIZE - 1);
}

void P_CheckRespawnQueue(void)
{
    int                 i;
    float               pos[3];
    subsector_t        *ss;
    mobj_t             *mo;
    spawnspot_t        *sobj;

    // Only respawn items in deathmatch 2 and optionally in coop.
    if(deathmatch != 2 &&
       (!cfg.coopRespawnItems || !IS_NETGAME || deathmatch))
        return;

    // Nothing left to respawn?
    if(itemRespawnQueueHead == itemRespawnQueueTail)
        return;

    // Time to respawn yet?
    if(levelTime - itemRespawnTime[itemRespawnQueueTail] < 30 * TICSPERSEC)
        return;

    // Get the attributes of the mobj from spawn parameters.
    sobj = &itemRespawnQueue[itemRespawnQueueTail];

    memcpy(pos, sobj->pos, sizeof(pos));
    ss = R_PointInSubsector(pos[VX], pos[VY]);
    pos[VZ] = P_GetFloatp(ss, DMU_FLOOR_HEIGHT);

    // Spawn a teleport fog at the new spot.
    mo = P_SpawnMobj3fv(MT_IFOG, pos);
    S_StartSound(SFX_ITMBK, mo);

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(sobj->type == mobjInfo[i].doomedNum)
            break;
    }

    if(mobjInfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else
        pos[VZ] = ONFLOORZ;

    mo = P_SpawnMobj3fv(i, pos);
    mo->angle = sobj->angle;

    if((mo->flags2 & MF2_FLOORCLIP) &&
       P_MobjGetFloorTerrainType(mo) >= FLOOR_LIQUID &&
       mo->pos[VZ] == P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
    {
        mo->floorClip = 10;
    }
    else
    {
        mo->floorClip = 0;
    }

    // Copy spawn attributes to the new mobj.
    memcpy(&mo->spawnSpot, sobj, sizeof(mo->spawnSpot));

    // Pull it from the que.
    itemRespawnQueueTail = (itemRespawnQueueTail + 1) & (ITEMQUESIZE - 1);
}

void P_EmptyRespawnQueue(void)
{
    itemRespawnQueueHead = itemRespawnQueueTail = 0;
}

/**
 * Called when a player is spawned on the level.
 * Most of the player structure stays unchanged between levels.
 */
void P_SpawnPlayer(spawnspot_t *spot, int pnum)
{
    int                 i;
    player_t           *p;
    float               pos[3];
    mobj_t             *mobj;

    if(pnum < 0)
        pnum = 0;
    if(pnum >= MAXPLAYERS - 1)
        pnum = MAXPLAYERS - 1;

    // Not playing?
    if(!players[pnum].plr->inGame)
        return;

    p = &players[pnum];

    if(p->playerState == PST_REBORN)
        G_PlayerReborn(pnum);

    if(spot)
    {
        pos[VX] = spot->pos[VX];
        pos[VY] = spot->pos[VY];
        pos[VZ] = ONFLOORZ;
    }
    else
    {
        pos[VX] = pos[VY] = pos[VZ] = 0;
    }

    mobj = P_SpawnMobj3fv(MT_PLAYER, pos);

    // With clients all player mobjs are remote, even the CONSOLEPLAYER.
    if(IS_CLIENT)
    {
        mobj->flags &= ~MF_SOLID;
        mobj->ddFlags = DDMF_REMOTE | DDMF_DONTDRAW;
        // The real flags are received from the server later on.
    }

    // Set color translations for player sprites.
    i = cfg.playerColor[pnum];
    if(i > 0)
        mobj->flags |= i << MF_TRANSSHIFT;

    mobj->angle = (spot? spot->angle : 0); /* $unifiedangles */
    p->plr->lookDir = 0; /* $unifiedangles */
    p->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    mobj->player = p;
    mobj->dPlayer = p->plr;
    mobj->health = p->health;

    p->plr->mo = mobj;
    p->playerState = PST_LIVE;
    p->refire = 0;
    p->damageCount = 0;
    p->bonusCount = 0;
    p->plr->extraLight = 0;
    p->plr->fixedColorMap = 0;
    p->plr->lookDir = 0;

    if(!spot)
        p->plr->flags |= DDPF_CAMERA;

    if(p->plr->flags & DDPF_CAMERA)
    {
        p->plr->mo->pos[VZ] += (float) cfg.plrViewHeight;
        p->plr->viewHeight = 0;
    }
    else
        p->plr->viewHeight = (float) cfg.plrViewHeight;

    p->class = PCLASS_PLAYER;

    // Setup gun psprite.
    P_SetupPsprites(p);

    // Give all cards in death match mode.
    if(deathmatch)
    {
        for(i = 0; i < NUM_KEY_TYPES; ++i)
            p->keys[i] = true;
    }

    if(pnum == CONSOLEPLAYER)
    {
        // Wake up the status bar.
        ST_Start();
        // Wake up the heads up text.
        HU_Start();
    }
}

/**
 * Spawns the passed thing into the world.
 */
void P_SpawnMapThing(spawnspot_t *th)
{
    int                 i, bit;
    mobj_t             *mobj;
    float               pos[3];

    //Con_Message("x = %g, y = %g, height = %i, angle = %i, type = %i, flags = %i\n",
    //            th->pos[VX], th->pos[VY], th->height, th->angle, th->type, th->flags);

    // Count deathmatch start positions.
    if(th->type == 11)
    {
        if(deathmatchP < &deathmatchStarts[MAX_DM_STARTS])
        {
            memcpy(deathmatchP, th, sizeof(*th));
            deathmatchP++;
        }
        return;
    }

    // Check for players specially.
    if(th->type >= 1 && th->type <= 4)
    {
        // Register this player start.
        P_RegisterPlayerStart(th);
        return;
    }

    // Don't spawn things flagged for Multiplayer if we're not in a netgame.
    if(!IS_NETGAME && (th->flags & MTF_NOTSINGLE))
        return;

    // Don't spawn things flagged for Not Deathmatch if we're deathmatching.
    if(deathmatch && (th->flags & MTF_NOTDM))
        return;

    // Don't spawn things flagged for Not Coop if we're coop'in.
    if(IS_NETGAME && !deathmatch && (th->flags & MTF_NOTCOOP))
        return;

    // Check for apropriate skill level.
    if(gameSkill == SM_BABY)
        bit = 1;
    else if(gameSkill == SM_NIGHTMARE)
        bit = 4;
    else
        bit = 1 << (gameSkill - 1);

    if(!(th->flags & bit))
        return;

    // Find which type to spawn.
    for(i = 0; i < Get(DD_NUMMOBJTYPES); ++i)
    {
        if(th->type == mobjInfo[i].doomedNum)
            break;
    }

    // Clients only spawn local objects.
    if(IS_CLIENT)
    {
        if(!(mobjInfo[i].flags & MF_LOCAL))
            return;
    }

    if(i == Get(DD_NUMMOBJTYPES))
        return;

    // Don't spawn keycards in deathmatch.
    if(deathmatch && mobjInfo[i].flags & MF_NOTDMATCH)
        return;

    // Check for specific disabled objects.
    if(IS_NETGAME && (th->flags & MTF_NOTSINGLE))
    {
        // Cooperative weapons?
        if(cfg.noCoopWeapons && !deathmatch && i >= MT_CLIP &&
           i <= MT_SUPERSHOTGUN)
            return;

        // Don't spawn any special objects in coop?
        if(cfg.noCoopAnything && !deathmatch)
            return;

        // BFG disabled in netgames?
        if(cfg.noNetBFG && i == MT_MISC25)
            return;
    }

    // Don't spawn any monsters if -noMonstersParm.
    if(noMonstersParm && (i == MT_SKULL || (mobjInfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    pos[VX] = th->pos[VX];
    pos[VY] = th->pos[VY];

    if(mobjInfo[i].flags & MF_SPAWNCEILING)
        pos[VZ] = ONCEILINGZ;
    else if(mobjInfo[i].flags2 & MF2_SPAWNFLOAT)
        pos[VZ] = FLOATRANDZ;
    else
        pos[VZ] = ONFLOORZ;

    mobj = P_SpawnMobj3fv(i, pos);
    if(mobj->flags2 & MF2_FLOATBOB)
    {   // Seed random starting index for bobbing motion.
        mobj->health = P_Random();
    }

    mobj->angle = th->angle;
    if(mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);
    if(mobj->flags & MF_COUNTKILL)
        totalKills++;
    if(mobj->flags & MF_COUNTITEM)
        totalItems++;

    mobj->visAngle = mobj->angle >> 16; // "angle-servo"; smooth actor turning
    if(th->flags & MTF_DEAF)
        mobj->flags |= MF_AMBUSH;

    // Set the spawn info for this mobj.
    mobj->spawnSpot.pos[VX] = pos[VX];
    mobj->spawnSpot.pos[VY] = pos[VY];
    mobj->spawnSpot.pos[VZ] = pos[VZ];
    mobj->spawnSpot.angle = th->angle;
    mobj->spawnSpot.type = th->type;
    mobj->spawnSpot.flags = th->flags;
}

mobj_t *P_SpawnCustomPuff(mobjtype_t type, float x, float y, float z)
{
    mobj_t             *th;

    // Clients do not spawn puffs.
    if(IS_CLIENT)
        return NULL;

    z += FIX2FLT((P_Random() - P_Random()) << 10);

    th = P_SpawnMobj3f(type, x, y, z);
    th->mom[MZ] = 1;
    th->tics -= P_Random() & 3;

    // Make it last at least one tic.
    if(th->tics < 1)
        th->tics = 1;

    return th;
}

void P_SpawnPuff(float x, float y, float z)
{
    mobj_t             *th = P_SpawnCustomPuff(MT_PUFF, x, y, z);

    // Don't make punches spark on the wall.
    if(th && attackRange == MELEERANGE)
        P_MobjChangeState(th, S_PUFF3);
}

void P_SpawnBlood(float x, float y, float z, int damage)
{
    mobj_t             *th;

    z += FIX2FLT((P_Random() - P_Random()) << 10);
    th = P_SpawnMobj3f(MT_BLOOD, x, y, z);
    th->mom[MZ] = 2;
    th->tics -= P_Random() & 3;

    if(th->tics < 1)
        th->tics = 1;

    if(damage <= 12 && damage >= 9)
        P_MobjChangeState(th, S_BLOOD2);
    else if(damage < 9)
        P_MobjChangeState(th, S_BLOOD3);
}

/**
 * Moves the missile forward a bit and possibly explodes it right there.
 *
 * @param th            The missile to be checked.
 *
 * @return              @c true, if the missile is at a valid location else
 *                      @c false
 */
boolean P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if(th->tics < 1)
        th->tics = 1;

    // Move forward slightly so an angle can be computed if it explodes
    // immediately.
    th->pos[VX] += th->mom[MX] / 2;
    th->pos[VY] += th->mom[MY] / 2;
    th->pos[VZ] += th->mom[MZ] / 2;

    if(!P_TryMove(th, th->pos[VX], th->pos[VY], false, false))
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
mobj_t *P_SpawnMissile(mobjtype_t type, mobj_t *source, mobj_t *dest)
{
    float               pos[3];
    mobj_t             *th = 0;
    angle_t             an = 0;
    float               dist = 0;
    float               slope = 0;
    float               spawnZOff = 0;

    memcpy(pos, source->pos, sizeof(pos));

    if(source->player)
    {
        // See which target is to be aimed at.
        an = source->angle;
        slope = P_AimLineAttack(source, an, 16 * 64);
        if(!cfg.noAutoAim)
            if(!lineTarget)
            {
                an += 1 << 26;
                slope = P_AimLineAttack(source, an, 16 * 64);

                if(!lineTarget)
                {
                    an -= 2 << 26;
                    slope = P_AimLineAttack(source, an, 16 * 64);
                }

                if(!lineTarget)
                {
                    an = source->angle;
                    slope =
                        tan(LOOKDIR2RAD(source->dPlayer->lookDir)) / 1.2f;
                }
            }

        if(!(source->player->plr->flags & DDPF_CAMERA))
            spawnZOff = cfg.plrViewHeight - 9 +
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

    th = P_SpawnMobj3fv(type, pos);

    if(th->info->seeSound)
        S_StartSound(th->info->seeSound, th);

    if(!source->player)
    {
        an = R_PointToAngle2(pos[VX], pos[VY], dest->pos[VX], dest->pos[VY]);

        // Fuzzy player.
        if(dest->flags & MF_SHADOW)
            an += (P_Random() - P_Random()) << 20;
    }

    th->target = source; // Where it came from.
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->mom[MX] = th->info->speed * FIX2FLT(finecosine[an]);
    th->mom[MY] = th->info->speed * FIX2FLT(finesine[an]);

    if(source->player)
    {   // Allow free-aim with the BFG in deathmatch?
        if(deathmatch && cfg.netBFGFreeLook == 0 && type == MT_BFG)
            th->mom[MZ] = 0;
        else
            th->mom[MZ] = th->info->speed * slope;
    }
    else
    {
        dist = P_ApproxDistance(dest->pos[VX] - pos[VX],
                                dest->pos[VY] - pos[VY]);
        dist /= th->info->speed;
        if(dist < 1)
            dist = 1;
        th->mom[MZ] = (dest->pos[VZ] - source->pos[VZ]) / dist;
    }

    // Make sure the speed is right (in 3D).
    dist = P_ApproxDistance(P_ApproxDistance(th->mom[MX], th->mom[MY]),
                            th->mom[MZ]);
    if(dist < 1)
        dist = 1;
    dist = th->info->speed / dist;

    th->mom[MX] *= dist;
    th->mom[MY] *= dist;
    th->mom[MZ] *= dist;

    if(P_CheckMissileSpawn(th))
        return th;

    return NULL;
}
