/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include "mobj.h"
#include "dmu_lib.h"

#define VANISHTICS              (2*TICSPERSEC)

#define MAX_BOB_OFFSET          (8)

#define NOMOMENTUM_THRESHOLD    (0.000001f)
#define WALKSTOP_THRESHOLD      (0.062484741f) // FIX2FLT(0x1000-1)
#define DROPOFFMOMENTUM_THRESHOLD (1.0f / 4)

static float getFriction(mobj_t* mo)
{
    if((mo->flags2 & MF2_FLY) && !(mo->pos[VZ] <= mo->floorZ) && !mo->onMobj)
    {   // Airborne friction.
        return FRICTION_FLY;
    }

#ifdef __JHERETIC__
    if(P_ToXSector(P_GetPtrp(mo->subsector, DMU_SECTOR))->special == 15)
    {   // Friction_Low
        return FRICTION_LOW;
    }
#endif

    return P_MobjGetFriction(mo);
}
/**
 * Handles the stopping of mobj movement. Also stops player walking animation.
 *
 * @param mo  Mobj.
 */
void Mobj_XYMoveStopping(mobj_t* mo)
{
    player_t* player = mo->player;
    boolean isVoodooDoll = false;
    boolean belowWalkStop = false;

    assert(mo != 0);

    if(player && (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
    {
        // Debug option for no sliding at all.
        mo->mom[MX] = mo->mom[MY] = 0;
        return;
    }

    if(mo->flags & (MF_MISSILE | MF_SKULLFLY))
    {   // No friction for missiles.
        return;
    }

    if(mo->pos[VZ] > mo->floorZ && !mo->onMobj && !(mo->flags2 & MF2_FLY))
    {   // No friction when falling.
        return;
    }

#if __JDOOM__ || __JDOOM64__
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
                if(mo->floorZ != P_GetFloatp(mo->subsector, DMU_FLOOR_HEIGHT))
                    return;
            }
        }
    }
#endif

    isVoodooDoll = (player && player->plr->mo != mo);
    belowWalkStop = (INRANGE_OF(mo->mom[MX], 0, WALKSTOP_THRESHOLD) &&
                     INRANGE_OF(mo->mom[MY], 0, WALKSTOP_THRESHOLD));

    // Stop player walking animation.
    if(isVoodooDoll
       || (player && belowWalkStop &&
           FEQUAL(player->plr->forwardMove, 0) &&
           FEQUAL(player->plr->sideMove, 0)))
    {
        /// @todo Voodoo doll animation is not stopped, should it be?

        // If in a walking frame, stop moving.
        if(player && P_PlayerInWalkState(player) && player->plr->mo == mo)
            P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->class_)->normalState);

        // $voodoodolls: Do not zero mom!
        if(!isVoodooDoll)
        {
            mo->mom[MX] = mo->mom[MY] = 0;

            // $voodoodolls: Stop view bobbing if this isn't a voodoo doll.
            player->bob = 0;
        }
    }
    else
    {
        float friction = getFriction(mo);
        mo->mom[MX] *= friction;
        mo->mom[MY] *= friction;
    }
}
