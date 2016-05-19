/** @file mapelement.cpp  Base class for all map elements.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
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

#include "world/mapelement.h"

#include "world/map.h"

using namespace de;

namespace world {

MapLink::MapLink(Map *map)
{
    setMap(map);
}

MapLink::~MapLink()
{}

bool MapLink::hasMap() const
{
    return _map != nullptr;
}

Map &MapLink::map() const
{
    if(hasMap()) return *_map;
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapLink::map", "No map is attributed");
}

Map *MapLink::mapPtr() const
{
    return hasMap() ? &map() : nullptr;
}

void MapLink::setMap(Map *newMap)
{
    _map = newMap;
}

//---------------------------------------------------------------------------------------

MapElement::MapElement(dint dmuType, Map *map)
    : DmuObject(dmuType)
    , MapLink(map)
{}

MapElement::~MapElement()
{}

}  // namespace world
