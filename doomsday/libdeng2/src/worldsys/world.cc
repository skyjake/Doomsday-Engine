/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/World"
#include "de/Writer"
#include "de/Reader"
#include "de/Map"
#include "de/App"
#include "de/Library"
#include "de/Log"

using namespace de;

World::World() : _map(0)
{
    // Create a blank map.
    _map = GAME_SYMBOL(deng_NewMap)();
}

World::~World()
{
    App::setCurrentMap(0);
    delete _map;
}

void World::loadMap(const String& name)
{
    LOG_AS("World::loadMap");
    LOG_VERBOSE("%s") << name;
    
    assert(_map != NULL);
    App::setCurrentMap(0);
    delete _map;

    LOG_TRACE("Creating an empty map.");

    // The map will do its own loading.
    _map = GAME_SYMBOL(deng_NewMap)();
    App::setCurrentMap(_map);
    
    _map->load(name);

    LOG_TRACE("Finished.");
}

void World::operator >> (Writer& to) const
{
    to << _info << *_map;
}

void World::operator << (Reader& from)
{
    App::setCurrentMap(_map);
    from >> _info >> *_map;
}
