/**\file p_ticker.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Timed Playsim Events.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

#include "render/sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int P_MobjTicker(thinker_t* th, void* context)
{
    DENG_UNUSED(context);

    uint                i;
    mobj_t*             mo = (mobj_t*) th;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int                 f;
        byte*               haloFactor = &mo->haloFactors[i];

#ifdef __CLIENT__
        // Set the high bit of halofactor if the light is clipped. This will
        // make P_Ticker diminish the factor to zero. Take the first step here
        // and now, though.
        if(mo->lumIdx == 0 || LO_IsClipped(mo->lumIdx, i))
        {
            if(*haloFactor & 0x80)
            {
                f = (*haloFactor & 0x7f); // - haloOccludeSpeed;
                if(f < 0)
                    f = 0;
                *haloFactor = f;
            }
        }
        else
        {
            if(!(*haloFactor & 0x80))
            {
                f = (*haloFactor & 0x7f); // + haloOccludeSpeed;
                if(f > 127)
                    f = 127;
                *haloFactor = 0x80 | f;
            }
        }

        // Handle halofactor.
        f = *haloFactor & 0x7f;
        if(*haloFactor & 0x80)
        {
            // Going up.
            f += haloOccludeSpeed;
            if(f > 127)
                f = 127;
        }
        else
        {
            // Going down.
            f -= haloOccludeSpeed;
            if(f < 0)
                f = 0;
        }

        *haloFactor &= ~0x7f;
        *haloFactor |= f;
#endif
    }

    return false; // Continue iteration.
}

/**
 * Doomsday's own play-ticker.
 */
void P_Ticker(timespan_t time)
{
    P_ControlTicker(time);
    Materials_Ticker(time);

    if(!theMap || !GameMap_ThinkerListInited(theMap)) return; // Not initialized yet.

    if(DD_IsSharpTick())
    {
        Sky_Ticker();

        // Check all mobjs (always public).
        GameMap_IterateThinkers(theMap, reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                                0x1, P_MobjTicker, NULL);
    }
}
