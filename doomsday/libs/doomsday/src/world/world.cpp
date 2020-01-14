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
#include "doomsday/world/materials.h"
#include "doomsday/world/map.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/line.h"
#include "doomsday/world/thinkers.h"
#include "doomsday/defs/ded.h"
#include "doomsday/defs/mapinfo.h"
#include "doomsday/DoomsdayApp"
#include "doomsday/players.h"
#include "api_player.h"

#include <de/Context>

namespace world {

using namespace de;

static World *theWorld = nullptr;

int World::ddMapSetup = false;
int World::validCount = 1;

DE_PIMPL(World)
{
    Record           fallbackMapInfo; // Used when no effective MapInfo definition.
    world::Map *     map = nullptr;
    world::Materials materials;

    Impl(Public *i) : Base(i)
    {
        theWorld = thisPublic;

        // One time init of the fallback MapInfo definition.
        defn::MapInfo(fallbackMapInfo).resetToDefaults();
    }

    ~Impl()
    {
        theWorld = nullptr;
    }

    DE_PIMPL_AUDIENCE(MapChange)
};

DE_AUDIENCE_METHOD(World, MapChange)

World::World() : d(new Impl(this))
{
    // Let players know that a world exists.
    DoomsdayApp::players().forAll([this] (Player &plr)
    {
        plr.setWorld(this);
        return LoopContinue;
    });
}

const Record &World::mapInfoForMapUri(const res::Uri &mapUri) const
{
    // Is there a MapInfo definition for the given URI?
    if (const Record *def = DED_Definitions()->mapInfos.tryFind("id", mapUri.compose()))
    {
        return *def;
    }
    // Is there is a default definition (for all maps)?
    if (const Record *def = DED_Definitions()->mapInfos.tryFind("id", res::Uri("Maps", Path("*")).compose()))
    {
        return *def;
    }
    // Use the fallback.
    return d->fallbackMapInfo;
}

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
        for (ddpsprite_t &pspr : ddpl.pSprites)
        {
            pspr.statePtr = nullptr;
        }

        return LoopContinue;
    });
}

//void World::timeChanged(const Clock &)
//{
//    // Nothing to do.
//}

bool World::allowAdvanceTime() const
{
    return true;
}

void World::tick(timespan_t)
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
    DE_ASSERT(hasMap());
    return *d->map;
}

Materials &World::materials()
{
    return d->materials;
}

const Materials &World::materials() const
{
    return d->materials;
}

World &World::get()
{
    DE_ASSERT(theWorld);
    return *theWorld;
}

mobj_t &World::contextMobj(const Context &ctx) // static
{
    const int id = ctx.selfInstance().geti(DE_STR("__id__"), 0);
    mobj_t *mo = get().map().thinkers().mobjById(id);
    if (!mo)
    {
        throw Map::MissingObjectError("World::contextMobj",
                                      Stringf("Mobj %d does not exist", id));
    }
    return *mo;
}

} // namespace world

