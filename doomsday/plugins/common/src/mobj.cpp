/** @file mobj.cpp  Common playsim map object (mobj) functionality.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "common.h"
#include "mobj.h"

#include "p_actor.h"
#include "p_player.h"
#include "p_map.h"
#include "dmu_lib.h"

#include <de/mathutil.h>
#include <cmath>
#include <cassert>

#define DROPOFFMOMENTUM_THRESHOLD (1.0 / 4)

/// Threshold for killing momentum of a freely moving object affected by friction.
#define WALKSTOP_THRESHOLD      (0.062484741) // FIX2FLT(0x1000-1)

/// Threshold for stopping walk animation.
#define STANDSPEED              (1.0 / 2) // FIX2FLT(0x8000)

static coord_t getFriction(mobj_t *mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->origin[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        // Airborne "friction".
        return FRICTION_FLY;
    }

#ifdef __JHERETIC__
    if(P_ToXSector(Mobj_Sector(mo))->special == 15)
    {
        // Low friction.
        return FRICTION_LOW;
    }
#endif

    return P_MobjGetFriction(mo);
}

dd_bool Mobj_IsVoodooDoll(mobj_t const *mo)
{
    if(!mo) return false;
    return (mo->player && mo->player->plr->mo != mo);
}

void Mobj_XYMoveStopping(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);

    player_t *player = mo->player;

    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
    {
        // No friction for missiles.
        return;
    }

    if(mo->origin[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
    {
        // No friction when falling.
        return;
    }

#ifndef __JHEXEN__
    if(cfg.slidingCorpses)
    {
        // $dropoff_fix: Add objects falling off ledges. Does not apply to players!
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) && !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(!INRANGE_OF(mo->mom[MX], 0, DROPOFFMOMENTUM_THRESHOLD) ||
               !INRANGE_OF(mo->mom[MY], 0, DROPOFFMOMENTUM_THRESHOLD))
            {
                if(!FEQUAL(mo->floorZ, P_GetDoublep(Mobj_Sector(mo), DMU_FLOOR_HEIGHT)))
                    return;
            }
        }
    }
#endif

    bool isVoodooDoll = Mobj_IsVoodooDoll(mo);
    bool belowWalkStop = (INRANGE_OF(mo->mom[MX], 0, WALKSTOP_THRESHOLD) &&
                          INRANGE_OF(mo->mom[MY], 0, WALKSTOP_THRESHOLD));

    bool belowStandSpeed = false;
    bool isMovingPlayer  = false;
    if(player)
    {
        belowStandSpeed = (INRANGE_OF(mo->mom[MX], 0, STANDSPEED) &&
                           INRANGE_OF(mo->mom[MY], 0, STANDSPEED));
        isMovingPlayer = (!FEQUAL(player->plr->forwardMove, 0) ||
                        !FEQUAL(player->plr->sideMove, 0));
    }

    // Stop player walking animation (only real players).
    if(!isVoodooDoll && player && belowStandSpeed && !isMovingPlayer &&
        !IS_NETWORK_SERVER) // Netgame servers use logic elsewhere for player animation.
    {
        // If in a walking frame, stop moving.
        if(P_PlayerInWalkState(player))
        {
            P_MobjChangeState(player->plr->mo, statenum_t(PCLASS_INFO(player->class_)->normalState));
        }
    }

    // Apply friction.
    if(belowWalkStop && !isMovingPlayer)
    {
        // $voodoodolls: Do not zero mom for voodoo dolls!
        if(!isVoodooDoll)
        {
            // Momentum is below the walkstop threshold; stop it completely.
            mo->mom[MX] = mo->mom[MY] = 0;

            // $voodoodolls: Stop view bobbing if this isn't a voodoo doll.
            if(player) player->bob = 0;
        }
    }
    else
    {
        coord_t friction = getFriction(mo);
        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}

dd_bool Mobj_IsPlayerClMobj(mobj_t *mo)
{
    if(IS_CLIENT)
    {
        for(int i = 0; i < MAXPLAYERS; i++)
        {
            if(ClPlayer_ClMobj(i) == mo)
            {
                return true;
            }
        }
    }
    return false;
}

dd_bool Mobj_IsPlayer(mobj_t const *mo)
{
    if(!mo) return false;
    return (mo->player != 0);
}

dd_bool Mobj_LookForPlayers(mobj_t *mo, dd_bool allAround)
{
    int const playerCount = P_CountPlayersInGame();

    // Nobody to target?
    if(!playerCount) return false;

    int const from = mo->lastLook % MAXPLAYERS;
    int const to   = (from + MAXPLAYERS - 1) % MAXPLAYERS;

    int cand         = from;
    int tries        = 0;
    bool foundTarget = false;
    for(; cand != to; cand = (cand < (MAXPLAYERS - 1)? cand + 1 : 0))
    {
        player_t *player = players + cand;

        // Is player in the game?
        if(!player->plr->inGame) continue;

        mobj_t *plrmo = player->plr->mo;
        if(!plrmo) continue;

        // Do not target camera players.
        if(P_MobjIsCamera(plrmo)) continue;

        // Only look ahead a fixed number of times.
        if(tries++ == 2) break;

        // Do not target dead players.
        if(player->health <= 0) continue;

        // Within sight?
        if(!P_CheckSight(mo, plrmo)) continue;

        if(!allAround)
        {
            angle_t an = M_PointToAngle2(mo->origin, plrmo->origin);
            an -= mo->angle;

            if(an > ANG90 && an < ANG270)
            {
                // If real close, react anyway.
                coord_t dist = M_ApproxDistance(plrmo->origin[VX] - mo->origin[VX],
                                                plrmo->origin[VY] - mo->origin[VY]);
                // Behind us?
                if(dist > MELEERANGE) continue;
            }
        }

#if __JHERETIC__ || __JHEXEN__
        // If player is invisible we may not detect if too far or randomly.
        if(plrmo->flags & MF_SHADOW)
        {
            if((M_ApproxDistance(plrmo->origin[VX] - mo->origin[VX],
                                 plrmo->origin[VY] - mo->origin[VY]) > 2 * MELEERANGE) &&
               M_ApproxDistance(plrmo->mom[MX], plrmo->mom[MY]) < 5)
            {
                // Too far; can't detect.
                continue;
            }

            // Randomly overlook the player regardless.
            if(P_Random() < 225) continue;
        }
#endif

#if __JHEXEN__
        // Minotaurs do not target their master.
        if(mo->type == MT_MINOTAUR && mo->tracer &&
           mo->tracer->player == player) continue;
#endif

        // Found our quarry.
        mo->target = plrmo;
        foundTarget = true;
    }

    // Start looking from here next time.
    mo->lastLook = cand;
    return foundTarget;
}

/**
 * Determines if it is allowed to execute the action function of @a mo.
 * @return @c true, if allowed.
 */
static bool shouldCallAction(mobj_t *mobj)
{
    if(IS_CLIENT)
    {
        if(ClMobj_LocalActionsEnabled(mobj))
            return true;
    }
    if(!(mobj->ddFlags & DDMF_REMOTE) ||    // only for local mobjs
       (mobj->flags3 & MF3_CLIENTACTION))   // action functions allowed?
    {
        return true;
    }
    return false;
}

static bool changeMobjState(mobj_t *mobj, statenum_t stateNum, bool doCallAction)
{
    DENG_ASSERT(mobj != 0);

    // Skip zero-tic states -- call their action but then advance to the next.
    do
    {
        if(stateNum == S_NULL)
        {
            mobj->state = (state_t *) S_NULL;
            P_MobjRemove(mobj, false);
            return false;
        }

        Mobj_SetState(mobj, stateNum);
        mobj->turnTime = false; // $visangle-facetarget

        state_t *st = &STATES[stateNum];

        // Call the action function?
        if(doCallAction && st->action)
        {
            if(shouldCallAction(mobj))
            {
                void (*actioner)(mobj_t *);
                actioner = de::function_cast<void (*)(mobj_t *)>(st->action);
                actioner(mobj);
            }
        }

        stateNum = statenum_t(st->nextState);
    } while(!mobj->tics);

    // Return false if an action function removed the mobj.
    return mobj->thinker.function != (thinkfunc_t) NOPFUNC;
}

dd_bool P_MobjChangeState(mobj_t *mobj, statenum_t stateNum)
{
    return changeMobjState(mobj, stateNum, true /*call action functions*/);
}

dd_bool P_MobjChangeStateNoAction(mobj_t *mobj, statenum_t stateNum)
{
    return changeMobjState(mobj, stateNum, false /*don't call action functions*/);
}
