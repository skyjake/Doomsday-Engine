/** @file world.cpp  World base class.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/world.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/player.h"
#include "api_player.h"

#include <de/App>

using namespace de;

static World *theWorld = nullptr;

DENG2_PIMPL(World)
{
    world::Map *map = nullptr;

    Instance(Public *i) : Base(i)
    {
        theWorld = thisPublic;
    }

    ~Instance()
    {
        theWorld = nullptr;
    }

    DENG2_PIMPL_AUDIENCE(MapChange)
};

DENG2_AUDIENCE_METHOD(World, MapChange)

World::World() : d(new Instance(this))
{}

void World::reset()
{
    DoomsdayApp::players().forAll([] (Player &plr)
    {
        ddplayer_t &ddpl = plr.publicData();

        // Mobjs go down with the map.
        ddpl.mo            = nullptr;
        ddpl.extraLight    = 0;
        ddpl.fixedColorMap = 0;
        //ddpl.inGame        = false;
        ddpl.flags         &= ~DDPF_CAMERA;

        // States have changed, the state pointers are unknown.
        for(ddpsprite_t &pspr : ddpl.pSprites)
        {
            pspr.statePtr = nullptr;
        }

        return LoopContinue;
    });
}

void World::timeChanged(Clock const &)
{
    // Nothing to do.
}

void World::setMap(world::Map *map)
{
    d->map = map;
}

bool World::hasMap() const
{
    return d->map != nullptr;
}

world::Map &World::map() const
{
    DENG2_ASSERT(hasMap());
    return *d->map;
}

World &World::get()
{
    DENG2_ASSERT(theWorld);
    return *theWorld;
}

void World::notifyMapChange()
{
    DENG2_FOR_AUDIENCE2(MapChange, i) i->worldMapChanged();
}
