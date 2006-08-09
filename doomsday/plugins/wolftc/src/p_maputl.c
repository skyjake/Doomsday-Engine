/* $Id: p_maputl.c 3300 2006-06-11 15:31:13Z skyjake $
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
 * Movement/collision utility functions, as used by function in p_map.c.
 * BLOCKMAP Iterator functions, and some PIT_* functions to use for iteration.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "d_config.h"
#include "p_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mobj_t *tmthing;

// CODE --------------------------------------------------------------------

/*
 * Unlinks a thing from block map and sectors.
 * On each position change, BLOCKMAP and other
 * lookups maintaining lists ot things inside
 * these structures need to be updated.
 */
void P_UnsetThingPosition(mobj_t *thing)
{
    P_UnlinkThing(thing);
}

/*
 * Links a thing into both a block and a subsector based on it's x,y.
 * Sets thing->subsector properly.
 */
void P_SetThingPosition(mobj_t *thing)
{
    int flags = 0;

    if(!(thing->flags & MF_NOSECTOR))
        flags |= DDLINK_SECTOR;

    if(!(thing->flags & MF_NOBLOCKMAP))
        flags |= DDLINK_BLOCKMAP;

    P_LinkThing(thing, flags);
}

/*
 * Apply "torque" to objects hanging off of ledges, so that they
 * fall off. It's not really torque, since Doom has no concept of
 * rotation, but it's a convincing effect which avoids anomalies
 * such as lifeless objects hanging more than halfway off of ledges,
 * and allows objects to roll off of the edges of moving lifts, or
 * to slide up and then back down stairs, or to fall into a ditch.
 * If more than one linedef is contacted, the effects are cumulative,
 * so balancing is possible.
 * killough $dropoff_fix
 */
static boolean PIT_ApplyTorque(line_t *ld, void *data)
{
    mobj_t *mo = tmthing;
    fixed_t dist;
    sector_t *frontsec = P_GetPtrp(ld, DMU_FRONT_SECTOR);
    sector_t *backsec = P_GetPtrp(ld, DMU_BACK_SECTOR);
    fixed_t ffloor;
    fixed_t bfloor;
    fixed_t dx = P_GetFixedp(ld, DMU_DX);
    fixed_t dy = P_GetFixedp(ld, DMU_DY);

    ffloor = P_GetFixedp(frontsec, DMU_FLOOR_HEIGHT);
    bfloor = P_GetFixedp(backsec, DMU_FLOOR_HEIGHT);

    if(tmthing->player)
        return true;            // skip players!

    dist =                      // lever arm
        +(dx >> FRACBITS) * (mo->pos[VY] >> FRACBITS) -
        (dy >> FRACBITS) * (mo->pos[VX] >> FRACBITS) -
        (dx >> FRACBITS) * (P_GetFixedp(P_GetPtrp(ld, DMU_VERTEX1),
                                        DMU_Y) >> FRACBITS) +
        (dy >> FRACBITS) * (P_GetFixedp(P_GetPtrp(ld, DMU_VERTEX1),
                                        DMU_X) >> FRACBITS);

    if((dist < 0 && ffloor < mo->pos[VZ] && bfloor >= mo->pos[VZ]) ||
       (dist >= 0 && bfloor < mo->pos[VZ] && ffloor >= mo->pos[VZ]))
    {
        // At this point, we know that the object straddles a two-sided
        // linedef, and that the object's center of mass is above-ground.

        fixed_t x = abs(dx), y = abs(dy);

        if(y > x)
        {
            fixed_t t = x;

            x = y;
            y = t;
        }

        y = finesine[(tantoangle[FixedDiv(y, x) >> DBITS] +
                      ANG90) >> ANGLETOFINESHIFT];

        // Momentum is proportional to distance between the
        // object's center of mass and the pivot linedef.
        //
        // It is scaled by 2^(OVERDRIVE - gear). When gear is
        // increased, the momentum gradually decreases to 0 for
        // the same amount of pseudotorque, so that oscillations
        // are prevented, yet it has a chance to reach equilibrium.

        if(mo->gear < OVERDRIVE)
            dist = FixedDiv(FixedMul(dist, y << -(mo->gear - OVERDRIVE)), x);
        else
            dist = FixedDiv(FixedMul(dist, y >> +(mo->gear - OVERDRIVE)), x);

        // Apply momentum away from the pivot linedef.

        x = FixedMul(dy, dist);
        y = FixedMul(dx, dist);

        // Avoid moving too fast all of a sudden (step into "overdrive")

        dist = FixedMul(x, x) + FixedMul(y, y);

        while(dist > FRACUNIT * 4 && mo->gear < MAXGEAR)
        {
            ++mo->gear;
            x >>= 1;
            y >>= 1;
            dist >>= 1;
        }

        mo->momx -= x;
        mo->momy += y;
    }

    return true;
}

/*
 * Applies "torque" to objects, based on all contacted linedefs.
 * killough $dropoff_fix
 */
void P_ApplyTorque(mobj_t *mo)
{
    // Remember the current state, for gear-change
    int     flags = mo->intflags;

    // 2003-4-16 (jk): corpse sliding anomalies, made configurable
    if(!cfg.slidingCorpses)
        return;

    tmthing = mo;

    // Use validCount to prevent checking the same line twice
    validCount++;

    P_ThingLinesIterator(mo, PIT_ApplyTorque, 0);

    // If any momentum, mark object as 'falling' using engine-internal flags
    if(mo->momx | mo->momy)
        mo->intflags |= MIF_FALLING;
    else
        // Clear the engine-internal flag indicating falling object.
        mo->intflags &= ~MIF_FALLING;

    // If the object has been moving, step up the gear.
    // This helps reach equilibrium and avoid oscillations.
    //
    // Doom has no concept of potential energy, much less
    // of rotation, so we have to creatively simulate these
    // systems somehow :)

    // If not falling for a while, reset it to full strength
    if(!((mo->intflags | flags) & MIF_FALLING))
        mo->gear = 0;
    else if(mo->gear < MAXGEAR)
        // Else if not at max gear, move up a gear
        mo->gear++;
}
