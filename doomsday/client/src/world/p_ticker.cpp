/** @file p_ticker.cpp Timed Playsim Events.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "de_network.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

#include "world/map.h"
#include "render/sky.h"

using namespace de;

int P_MobjTicker(thinker_t* th, void* context)
{
    DENG_UNUSED(context);

#ifdef __CLIENT__

    uint                i;
    mobj_t*             mo = (mobj_t*) th;

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int                 f;
        byte*               haloFactor = &mo->haloFactors[i];

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
    }

#else
    DENG_UNUSED(th);
#endif

    return false; // Continue iteration.
}

#ifdef __CLIENT__

/**
 * Process a tic of @a elapsed length, animating all materials.
 * @param elapsed  Length of tic to be processed.
 */
static void materialsTicker(timespan_t elapsed)
{
    foreach(Material *material, App_Materials().all())
    foreach(MaterialAnimation *animation, material->animations())
    {
        animation->animate(elapsed);
    }
}

#endif // __CLIENT__

void P_Ticker(timespan_t elapsed)
{
    P_ControlTicker(elapsed);

#ifdef __CLIENT__
    materialsTicker(elapsed);
#endif

    if(!App_World().hasMap()) return;

    Map &map = App_World().map();
    if(!map.thinkerListInited()) return; // Not initialized yet.

    if(DD_IsSharpTick())
    {
        Sky_Ticker();

        // Check all mobjs (always public).
        map.iterateThinkers(reinterpret_cast<thinkfunc_t>(gx.MobjThinker),
                            0x1, P_MobjTicker, NULL);
    }
}
