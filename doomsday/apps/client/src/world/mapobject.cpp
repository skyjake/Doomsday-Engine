/** @file mapobject.cpp  Base class for all map objects.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "world/mapobject.h"

#include "world/map.h"

using namespace de;

DENG2_PIMPL_NOREF(MapObject)
{
    Map *map = nullptr;
    dint indexInMap = NoIndex;
    Vector3d origin;             ///< Position in map space.
    BspLeaf *bspLeaf = nullptr;  ///< BSP leaf at @ref origin in the map (not owned).
};

MapObject::MapObject(Vector3d const &origin) : d(new Instance)
{
    d->origin = origin;
}

MapObject::~MapObject()
{}

Vector3d const &MapObject::origin() const
{
    return d->origin;
}

void MapObject::setOrigin(Vector3d const &newOrigin)
{
    if(d->origin != newOrigin)
    {
        // When moving on the XY plane; invalidate the BSP leaf.
        if(!de::fequal(d->origin.x, newOrigin.x) ||
           !de::fequal(d->origin.y, newOrigin.y))
        {
            d->bspLeaf = nullptr;
        }

        d->origin = newOrigin;
    }
}

void MapObject::move(Vector3d const &delta)
{
    setOrigin(d->origin + delta);
}

BspLeaf &MapObject::bspLeafAtOrigin() const
{
    if(!d->bspLeaf)
    {
        // Determine this now.
        d->bspLeaf = &map().bspLeafAt(origin());
    }
    return *d->bspLeaf;
}

bool MapObject::hasMap() const
{
    return d->map != nullptr;
}

Map &MapObject::map() const
{
    if(d->map) return *d->map;
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapObject::map", "No map is attributed");
}

void MapObject::setMap(Map *newMap)
{
    d->map = newMap;
}

dint MapObject::indexInMap() const
{
    return d->indexInMap;
}

void MapObject::setIndexInMap(dint newIndex)
{
    d->indexInMap = newIndex;
}
