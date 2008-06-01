/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 * p_ticker.c: Timed Playsim Events
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

#include "r_sky.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

boolean P_MobjTicker(thinker_t* th, void* context)
{
    int                 i;
    mobj_t*             mo;
    lumobj_t*           lum;

    if(!P_IsMobjThinker(th->function))
        return true; // Continue iteration.

    mo = (mobj_t*) th;
    lum = LO_GetLuminous(mo->light);

    // Set the high bit of halofactor if the light is clipped. This will
    // make P_Ticker diminish the factor to zero. Take the first step here
    // and now, though.
    if(!lum || lum->flags & LUMF_CLIPPED)
    {
        if(mo->haloFactor & 0x80)
        {
            i = (mo->haloFactor & 0x7f); // - haloOccludeSpeed;
            if(i < 0)
                i = 0;
            mo->haloFactor = i;
        }
    }
    else
    {
        if(!(mo->haloFactor & 0x80))
        {
            i = (mo->haloFactor & 0x7f); // + haloOccludeSpeed;
            if(i > 127)
                i = 127;
            mo->haloFactor = 0x80 | i;
        }
    }

    // Handle halofactor.
    i = mo->haloFactor & 0x7f;
    if(mo->haloFactor & 0x80)
    {
        // Going up.
        i += haloOccludeSpeed;
        if(i > 127)
            i = 127;
    }
    else
    {
        // Going down.
        i -= haloOccludeSpeed;
        if(i < 0)
            i = 0;
    }

    mo->haloFactor &= ~0x7f;
    mo->haloFactor |= i;

    return true; // Continue iteration.
}

boolean PIT_ClientMobjTicker(clmobj_t *cmo, void *parm)
{
    P_MobjTicker((thinker_t*) &cmo->mo, NULL);

    // Continue iteration.
    return true;
}

/**
 * Doomsday's own play-ticker.
 */
void P_Ticker(timespan_t time)
{
    static trigger_t    fixed = { 1.0 / 35, 0 };

    P_ControlTicker(time);

    if(!P_ThinkerListInited())
        return; // Not initialized yet.

    if(!M_RunTrigger(&fixed, time))
        return;

    // New ptcgens for planes?
    P_CheckPtcPlanes();
    R_AnimateAnimGroups();
    R_SkyTicker();

    // Check all mobjs.
    P_IterateThinkers(NULL, P_MobjTicker, NULL);

    // Check all client mobjs.
    Cl_MobjIterator(PIT_ClientMobjTicker, NULL);
}
