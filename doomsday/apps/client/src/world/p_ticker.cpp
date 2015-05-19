/** @file p_ticker.cpp  Timed world events.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifdef __CLIENT__
#  include "MaterialAnimator"
#endif

using namespace de;

void P_Ticker(timespan_t elapsed)
{
#ifdef __CLIENT__
    // Animate materials.
    /// @todo Each context animator should be driven by a more relevant ticker, rather
    /// than using the playsim's ticker for all contexts. (e.g., animators for the UI
    /// context should be driven separately).
    App_ResourceSystem().forAllMaterials([&elapsed] (Material &material)
    {
        return material.forAllAnimators([&elapsed] (MaterialAnimator &animator)
        {
            animator.animate(elapsed);
            return LoopContinue;
        });
    });
#endif

    App_WorldSystem().tick(elapsed);
}
