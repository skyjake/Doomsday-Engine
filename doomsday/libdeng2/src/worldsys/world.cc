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

using namespace de;

World::World() : map_(0)
{
    // Create a blank map.
    map_ = App::game().SYMBOL(deng_NewMap)();
}

World::~World()
{
    App::setCurrentMap(0);
    delete map_;
}

void World::loadMap(const String& name)
{
    assert(map_ != NULL);
    App::setCurrentMap(0);
    delete map_;

    // The map will do its own loading.
    map_ = App::game().SYMBOL(deng_NewMap)();
    App::setCurrentMap(map_);
    
    map_->load(name);
}

void World::operator >> (Writer& to) const
{
    to << info_ << *map_;
}

void World::operator << (Reader& from)
{
    from >> info_ >> *map_;
}
