/** @file mapobject.cpp Base class for all map objects.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "world/map.h"
#include "world/mapobject.h"

using namespace de;

DENG2_PIMPL_NOREF(MapObject)
{
    Map *map;
    int indexInMap;
    Vector3d origin;    ///< Position in map space.
    BspLeaf *bspLeaf;   ///< BSP leaf at @ref origin in the map (not owned).

    Instance(Vector3d const &origin)
        : map(0),
          indexInMap(NoIndex),
          origin(origin),
          bspLeaf(0)
    {}
};

MapObject::MapObject(Vector3d const &origin) : d(new Instance(origin))
{}

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
            d->bspLeaf = 0;
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
    return d->map != 0;
}

Map &MapObject::map() const
{
    if(d->map)
    {
        return *d->map;
    }
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapObject::map", "No map is attributed");
}

void MapObject::setMap(Map *newMap)
{
    d->map = newMap;
}

int MapObject::indexInMap() const
{
    return d->indexInMap;
}

void MapObject::setIndexInMap(int newIndex)
{
    d->indexInMap = newIndex;
}
