/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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

void P_MobjTicker(mobj_t *mo)
{
    lumobj_t   *lum = NULL;
    int         i;

    if(!mo->usingBias)
        lum = LO_GetLuminous(mo->light);

    // Set the high bit of halofactor if the light is clipped. This will
    // make P_Ticker diminish the factor to zero. Take the first step here
    // and now, though.
    if(!lum || lum->flags & LUMF_CLIPPED)
    {
        if(mo->halofactor & 0x80)
        {
            i = (mo->halofactor & 0x7f);    // - haloOccludeSpeed;
            if(i < 0)
                i = 0;
            mo->halofactor = i;
        }
    }
    else
    {
        if(!(mo->halofactor & 0x80))
        {
            i = (mo->halofactor & 0x7f);    // + haloOccludeSpeed;
            if(i > 127)
                i = 127;
            mo->halofactor = 0x80 | i;
        }
    }

    // Handle halofactor.
    i = mo->halofactor & 0x7f;
    if(mo->halofactor & 0x80)
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
    mo->halofactor &= ~0x7f;
    mo->halofactor |= i;
}

boolean PIT_ClientMobjTicker(clmobj_t *cmo, void *parm)
{
    P_MobjTicker(&cmo->mo);
    // Continue iteration.
    return true;
}

/**
 * Doomsday's own play-ticker.
 */
void P_Ticker(timespan_t time)
{
    thinker_t *th;
    static trigger_t fixed = { 1.0 / 35, 0 };

    if(!thinkercap.next)
        return;                 // Not initialized yet.

    if(!M_RunTrigger(&fixed, time))
        return;

    // New ptcgens for planes?
    P_CheckPtcPlanes();
    R_AnimateAnimGroups();
    R_SkyTicker();

    // Check all mobjs.
    for(th = thinkercap.next; th != &thinkercap; th = th->next)
    {
        if(!P_IsMobjThinker(th->function))
            continue;
        P_MobjTicker((mobj_t *) th);
    }

    Cl_MobjIterator(PIT_ClientMobjTicker, NULL);
}
