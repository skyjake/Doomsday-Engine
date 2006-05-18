/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * Common code relating to actors, or "monsters".
 * Actor movement smoothing; the "Servo".
 */

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#  include "doomdef.h"
#  include "p_local.h"
#elif __JHERETIC__
#  include "jHeretic/Doomdef.h"
#  include "jHeretic/P_local.h"
#elif __JHEXEN__
#  include "jHexen/h2def.h"
#elif __JSTRIFE__
#  include "jStrife/h2def.h"
#endif

// MACROS ------------------------------------------------------------------

#define MIN_STEP    ((10 * ANGLE_1) >> 16)  // degrees per tic
#define MAX_STEP    (ANG90 >> 16)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_SetThingSRVO(mobj_t *mo, int stepx, int stepy)
{
    // Shift to 8.8 fixed point.
    mo->srvo[0] = (-stepx) >> 8;
    mo->srvo[1] = (-stepy) >> 8;
}

/*
 * The actor has taken a step, set the corresponding short-range visual
 * offset.
 */
void P_SetThingSRVOZ(mobj_t *mo, int stepz)
{
    // Shift to 8.8 fixed point.
    mo->srvo[2] = (-stepz) >> 8;
}

/*
 * Turn visual angle towards real angle. An engine cvar controls whether
 * the visangle or the real angle is used in rendering.
 * Real-life analogy: angular momentum (you can't suddenly just take a
 * 90 degree turn in zero time).
 */
void P_SRVOAngleTicker(mobj_t *mo)
{
    short   target, step, diff;
    int     lstep, hgt;

    // Check requirements.
    if(mo->flags & MF_MISSILE || !(mo->flags & MF_COUNTKILL))
    {
        mo->visangle = mo->angle >> 16;
        return; // This is not for us.
    }

    target = mo->angle >> 16;
    diff = target - mo->visangle;

    if(mo->turntime)
    {
        if(mo->tics)
            step = abs(diff) / mo->tics;
        else
            step = abs(diff);
        if(!step)
            step = 1;
    }
    else
    {
        // Calculate a good step size.
        // Thing height and diff taken into account.
        hgt = mo->height >> 16;
        if(hgt < 30)
            hgt = 30;
        if(hgt > 60)
            hgt = 60;
        lstep = abs(diff) * 8 / hgt;
        if(lstep < MIN_STEP)
            lstep = MIN_STEP;
        if(lstep > MAX_STEP)
            lstep = MAX_STEP;
        step = lstep;
    }

    // Do the step.
    if(abs(diff) <= step)
        mo->visangle = target;
    else if(diff > 0)
        mo->visangle += step;
    else if(diff < 0)
        mo->visangle -= step;
}

/*
 * The thing's timer has run out, which means the thing has completed
 * its step. Or there has been a teleport.
 */
void P_ClearThingSRVO(mobj_t *mo)
{
    memset(mo->srvo, 0, sizeof(mo->srvo));
}

/*
 * The first three bits of the selector special byte contain a
 * relative health level.
 */
void P_UpdateHealthBits(mobj_t *mobj)
{
    int     i;

    if(mobj->info && mobj->info->spawnhealth > 0)
    {
        mobj->selector &= DDMOBJ_SELECTOR_MASK; // Clear high byte.
        i = (mobj->health << 3) / mobj->info->spawnhealth;
        if(i > 7)
            i = 7;
        if(i < 0)
            i = 0;
        mobj->selector |= i << DDMOBJ_SELECTOR_SHIFT;
    }
}
