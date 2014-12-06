/** @file p_ticker.cpp Timed world events.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
#include "world/p_ticker.h"

#include "de_network.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"
#ifdef __CLIENT__
#  include "clientapp.h"
#  include "render/rendersystem.h"
#  include "render/skydrawable.h"
#endif
#include "world/map.h"
#include "world/sky.h"

using namespace de;

int P_MobjTicker(thinker_t *th, void *context)
{
    DENG_UNUSED(context);

#ifdef __CLIENT__

    mobj_t *mo = (mobj_t*) th;

    for(uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        int f;
        byte *haloFactor = &mo->haloFactors[i];

        // Set the high bit of halofactor if the light is clipped. This will
        // make P_Ticker diminish the factor to zero. Take the first step here
        // and now, though.
        if(mo->lumIdx == Lumobj::NoIndex || R_ViewerLumobjIsClipped(mo->lumIdx))
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
    /// @todo Each context animator should be driven by a more relevant ticker, rather
    /// than using the playsim's ticker for all contexts. (e.g., animators for the UI
    /// context should be driven separately).
    for(Material *material : App_ResourceSystem().allMaterials())
    {
        material->forAllAnimators([&elapsed] (MaterialAnimator &animator)
        {
            animator.animate(elapsed);
            return LoopContinue;
        });
    }
}

#endif // __CLIENT__

void P_Ticker(timespan_t elapsed)
{
#ifdef __CLIENT__
    materialsTicker(elapsed);
#else
    DENG2_UNUSED(elapsed);
#endif
    App_WorldSystem().tick(elapsed);
}
