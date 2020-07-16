/** @file p_ticker.cpp  Timed world events.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#  include "resource/materialanimator.h"
#  include <doomsday/world/materials.h>
#endif

using namespace de;

void P_Ticker(timespan_t elapsed)
{
#ifdef __CLIENT__
    // Animate materials.
    /// @todo Each context animator should be driven by a more relevant ticker, rather
    /// than using the playsim's ticker for all contexts. (e.g., animators for the UI
    /// context should be driven separately).
    world::Materials::get().forAnimatedMaterials([&elapsed] (world::Material &material)
    {
        auto &mat = material.as<ClientMaterial>();
        for (int i = mat.animatorCount() - 1; i >= 0; --i)
        {
            mat.animator(i).animate(elapsed);
        }
        return LoopContinue;
    });
#endif

    world::World::get().tick(elapsed);

    // Internal ticking for all players.
    DoomsdayApp::players().forAll([&elapsed] (Player &plr) {
        plr.tick(elapsed);
        return LoopContinue;
    });
}
