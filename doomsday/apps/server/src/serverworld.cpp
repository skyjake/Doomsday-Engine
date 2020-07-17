/** @file serverworld.cpp  World subsystem for the Server app.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "serverworld.h"

#include "dd_main.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "def_main.h"  // ::defs
#include "api_player.h"
#include "network/net_main.h"
#include "api_mapedit.h"
#include "world/p_players.h"
#include "world/p_ticker.h"
#include "world/sky.h"
#include "edit_map.h"

#include "server/sv_pool.h"
#include <doomsday/world/convexsubspace.h>
#include <doomsday/world/mobjthinkerdata.h>

#include <doomsday/world/sector.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/defs/mapinfo.h>
#include <doomsday/res/mapmanifests.h>
#include <doomsday/world/mapbuilder.h>
#include <doomsday/world/materialmanifest.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/plane.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/subsector.h>
#include <doomsday/world/surface.h>
#include <doomsday/world/thinkers.h>

#include <de/keymap.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/timer.h>
#include <de/dscript.h>
#include <de/error.h>
#include <de/log.h>
#include <de/scripting/scheduler.h>
#include <de/time.h>

#include <map>
#include <utility>

using namespace de;
using namespace res;

ServerWorld::ServerWorld()
{
    useDefaultConstructors();

    audienceForMapChange() += [this]() {
        if (hasMap())
        {
            map().thinkers().audienceForRemoval() += this;
        }
        // Now that the setup is done, let's reset the timer so that it will
        // appear that no time has passed during the setup.
        DD_ResetTimer();
    };
}

void ServerWorld::aboutToChangeMap()
{
    // Initialize the logical sound manager.
    App_AudioSystem().aboutToUnloadMap();

    // Whenever the map changes, remote players must tell us when they're
    // ready to begin receiving frames.
    for (uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if (DD_Player(i)->isConnected())
        {
            LOG_DEBUG("Client %i marked as 'not ready' to receive frames.") << i;
            DD_Player(i)->ready = false;
        }
    }

    if (hasMap())
    {
        map().thinkers().audienceForRemoval() -= this;
    }
}

void ServerWorld::thinkerRemoved(thinker_t &th)
{
    auto *mob = reinterpret_cast<mobj_t *>(&th);

    // If the state of the mobj is the NULL state, this is a
    // predictable mobj removal (result of animation reaching its
    // end) and shouldn't be included in netGame deltas.
    if (!mob->state || !runtimeDefs.states.indexOf(mob->state))
    {
        Sv_MobjRemoved(th.id);
    }
}

void ServerWorld::mapFinalized()
{
    world::World::mapFinalized();
    
    if (gameTime > 20000000 / TICSPERSEC)
    {
        // In very long-running games, gameTime will become so large that
        // it cannot be accurately converted to 35 Hz integer tics. Thus it
        // needs to be reset back to zero.
        gameTime = 0;
    }

    if (netState.isServer)
    {
        // Init server data.
        Sv_InitPools();
    }
}

void ServerWorld::reset()
{
    World::reset();
    unloadMap();
}
