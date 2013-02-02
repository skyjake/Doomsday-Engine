/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include <math.h>
#include <assert.h>
#include <de/mathutil.h>

#include "mobj.h"
#include "p_actor.h"
#include "p_player.h"
#include "p_map.h"
#include "dmu_lib.h"

#define DROPOFFMOMENTUM_THRESHOLD (1.0 / 4)

/// Threshold for killing momentum of a freely moving object affected by friction.
#define WALKSTOP_THRESHOLD      (0.062484741) // FIX2FLT(0x1000-1)

/// Threshold for stopping walk animation.
#define STANDSPEED              (1.0 / 2) // FIX2FLT(0x8000)

static coord_t getFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->origin[VZ] <= mo->floorZ) && !mo->onMobj)
    {
        // Airborne "friction".
        return FRICTION_FLY;
    }

#ifdef __JHERETIC__
    if(P_ToXSector(P_GetPtrp(mo->bspLeaf, DMU_SECTOR))->special == 15)
    {
        // Low friction.
        return FRICTION_LOW;
    }
#endif

    return P_MobjGetFriction(mo);
}

boolean Mobj_IsVoodooDoll(mobj_t const *mo)
{
    if(!mo) return false;
    return (mo->player && mo->player->plr->mo != mo);
}

void Mobj_XYMoveStopping(mobj_t* mo)
{
    player_t* player = mo->player;
    boolean isVoodooDoll = false;
    boolean belowWalkStop = false;
    boolean belowStandSpeed = false;
    boolean isMovingPlayer = false;

    assert(mo != 0);

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
        /**
         * $dropoff_fix:
         * Add objects falling off ledges. Does not apply to players!
         */
        if(((mo->flags & MF_CORPSE) || (mo->intFlags & MIF_FALLING)) && !mo->player)
        {
            // Do not stop sliding if halfway off a step with some momentum.
            if(!INRANGE_OF(mo->mom[MX], 0, DROPOFFMOMENTUM_THRESHOLD) ||
               !INRANGE_OF(mo->mom[MY], 0, DROPOFFMOMENTUM_THRESHOLD))
            {
                if(!FEQUAL(mo->floorZ, P_GetDoublep(mo->bspLeaf, DMU_FLOOR_HEIGHT)))
                    return;
            }
        }
    }
#endif

    isVoodooDoll = Mobj_IsVoodooDoll(mo);
    belowWalkStop = (INRANGE_OF(mo->mom[MX], 0, WALKSTOP_THRESHOLD) &&
                     INRANGE_OF(mo->mom[MY], 0, WALKSTOP_THRESHOLD));
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
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->normalState);
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

boolean Mobj_IsPlayerClMobj(mobj_t* mo)
{
    if(IS_CLIENT)
    {
        int i;

        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(ClPlayer_ClMobj(i) == mo)
                return true;
        }
    }
    return false;
}

boolean Mobj_IsPlayer(mobj_t const *mo)
{
    if(!mo) return false;
    return (mo->player != 0);
}

boolean Mobj_LookForPlayers(mobj_t* mo, boolean allAround)
{
    const int playerCount = P_CountPlayersInGame();
    boolean foundTarget = false;
    int from, to, cand, tries = 0;

    // Nobody to target?
    if(!playerCount) return false;

    from = mo->lastLook % MAXPLAYERS;
    to   = (from+MAXPLAYERS-1) % MAXPLAYERS;

    for(cand = from; cand != to; cand = (cand < (MAXPLAYERS-1)? cand + 1 : 0))
    {
        player_t* player = players + cand;
        mobj_t* plrmo;

        // Is player in the game?
        if(!player->plr->inGame || !player->plr->mo) continue;
        plrmo = player->plr->mo;

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

boolean Mobj_ActionFunctionAllowed(mobj_t* mo)
{
    if(IS_CLIENT)
    {
        if(ClMobj_LocalActionsEnabled(mo))
            return true;
    }
    if(!(mo->ddFlags & DDMF_REMOTE) ||    // only for local mobjs
       (mo->flags3 & MF3_CLIENTACTION))   // action functions allowed?
    {
        return true;
    }
    return false;
}
