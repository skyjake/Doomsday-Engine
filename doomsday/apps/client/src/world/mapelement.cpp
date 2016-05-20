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

MapElement::MapElement(dint dmuType, Map *map)
    : DmuObject(dmuType)
{
    setMap(map);
}

MapElement::~MapElement()
{}

bool MapElement::hasMap() const
{
    return _map != nullptr;
}

Map &MapElement::map() const
{
    // If a parent is configured this property is delegated to the parent.
    if(hasParent()) return parent().as<MapElement>().map();

    if(hasMap()) return *_map;
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapElement::map", "No map is attributed");
}

Map *MapElement::mapPtr() const
{
    return hasMap() ? &map() : nullptr;
}

void MapElement::setMap(Map *newMap)
{
    if(_map == newMap) return;
    _map = newMap;
    DENG2_FOR_AUDIENCE(MapChange, i) i->mapElementMapChanged(*this);
}

}  // namespace world
