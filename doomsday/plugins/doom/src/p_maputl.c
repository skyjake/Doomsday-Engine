/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
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
 * p_maputl.c: Movement/collision map utility functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "jdoom.h"

#include "p_map.h"
#include "dmu_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Apply "torque" to objects hanging off of ledges, so that they fall off.
 * It's not really torque, since Doom has no concept of rotation, but it's
 * a convincing effect which avoids anomalies such as lifeless objects
 * hanging more than halfway off of ledges, and allows objects to roll off
 * of the edges of moving lifts, or to slide up and then back down stairs,
 * or to fall into a ditch.
 *
 * If more than one linedef is contacted, the effects are cumulative, so
 * balancing is possible.
 */
static int PIT_ApplyTorque(LineDef* ld, void* parameters)
{
    mobj_t* mo = tmThing;
    coord_t dist;
    Sector* frontsec, *backsec;
    coord_t ffloor, bfloor;
    coord_t d1[2], vtx[2];

    if(tmThing->player)
        return false; // Skip players!

    if(!(frontsec = P_GetPtrp(ld, DMU_FRONT_SECTOR)) ||
       !(backsec  = P_GetPtrp(ld, DMU_BACK_SECTOR)))
        return false; // Shouldn't ever happen.

    ffloor = P_GetDoublep(frontsec, DMU_FLOOR_HEIGHT);
    bfloor = P_GetDoublep(backsec, DMU_FLOOR_HEIGHT);
    P_GetDoublepv(ld, DMU_DXY, d1);
    P_GetDoublepv(P_GetPtrp(ld, DMU_VERTEX0), DMU_XY, vtx);

    // Lever-arm:
    dist = +d1[0] * mo->origin[VY] - d1[1] * mo->origin[VX] -
            d1[0] * vtx[VY]     + d1[1] * vtx[VX];

    if((dist < 0  && ffloor < mo->origin[VZ] && bfloor >= mo->origin[VZ]) ||
       (dist >= 0 && bfloor < mo->origin[VZ] && ffloor >= mo->origin[VZ]))
    {
        // At this point, we know that the object straddles a two-sided
        // linedef, and that the object's center of mass is above-ground.
        coord_t x = fabs(d1[0]), y = fabs(d1[1]);

        if(y > x)
        {
            coord_t tmp = x;
            x = y;
            y = tmp;
        }

        y = FIX2FLT(finesine[(tantoangle[FLT2FIX(y / x) >> DBITS] +
                             ANG90) >> ANGLETOFINESHIFT]);

        /**
         * Momentum is proportional to distance between the object's center
         * of mass and the pivot linedef.
         *
         * It is scaled by 2^(OVERDRIVE - gear). When gear is increased, the
         * momentum gradually decreases to 0 for the same amount of
         * pseudotorque, so that oscillations are prevented, yet it has a
         * chance to reach equilibrium.
         */

        if(mo->gear < OVERDRIVE)
            dist = (dist * FIX2FLT(FLT2FIX(y) << -(mo->gear - OVERDRIVE))) / x;
        else
            dist = (dist * FIX2FLT(FLT2FIX(y) >> +(mo->gear - OVERDRIVE))) / x;

        // Apply momentum away from the pivot linedef.
        x = d1[1] * dist;
        y = d1[0] * dist;

        // Avoid moving too fast all of a sudden (step into "overdrive").
        dist = (x * x) + (y * y);
        while(dist > 4 && mo->gear < MAXGEAR)
        {
            ++mo->gear;
            x /= 2;
            y /= 2;
            dist /= 2;
        }

        mo->mom[MX] -= x;
        mo->mom[MY] += y;
    }

    return false;
}

/**
 * Applies "torque" to objects, based on all contacted linedefs.
 */
void P_ApplyTorque(mobj_t *mo)
{
    int                 flags = mo->intFlags;

    // Corpse sliding anomalies, made configurable.
    if(!cfg.slidingCorpses)
        return;

    tmThing = mo;

    // Use VALIDCOUNT to prevent checking the same line twice.
    VALIDCOUNT++;

    P_MobjLinesIterator(mo, PIT_ApplyTorque, 0);

    // If any momentum, mark object as 'falling' using engine-internal
    // flags.
    if(!FEQUAL(mo->mom[MX], 0) || !FEQUAL(mo->mom[MY], 0))
        mo->intFlags |= MIF_FALLING;
    else
        // Clear the engine-internal flag indicating falling object.
        mo->intFlags &= ~MIF_FALLING;

    /**
     * If the object has been moving, step up the gear. This helps reach
     * equilibrium and avoid oscillations.
     *
     * DOOM has no concept of potential energy, much less of rotation, so we
     * have to creatively simulate these systems somehow :)
     */

    // If not falling for a while, reset it to full strength.
    if(!((mo->intFlags | flags) & MIF_FALLING))
        mo->gear = 0;
    else if(mo->gear < MAXGEAR) // Else if not at max gear, move up a gear.
        mo->gear++;
}
