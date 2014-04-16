/** @file mapobject.h  Base class for all world map objects.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_MAPOBJECT_H
#define DENG_WORLD_MAPOBJECT_H

#include <de/Error>
#include <de/Vector>

class BspLeaf;

namespace de {

class Map;

/**
 * Base class for all map objects.
 *
 * While logically relatted to MapElement, a map object is considered a dynamic
 * and volatile entity (whereas a map element can be largely considered static).
 *
 * The lifetime of a map object may varry massively between instances and range
 * from only a few milliseconds to a few hours or longer.
 *
 * @ingroup world
 */
class MapObject
{
    DENG2_NO_COPY  (MapObject)
    DENG2_NO_ASSIGN(MapObject)

public:
    /// No map is attributed. @ingroup errors
    DENG2_ERROR(MissingMapError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

public:
    MapObject(de::Vector3d const &origin = de::Vector3d());
    virtual ~MapObject();

    DENG2_AS_IS_METHODS()

    /**
     * Returns the map BSP leaf at the origin of the object (result cached).
     * Naturally a map must be attributed.
     *
     * @see setMap(), hasMap()
     */
    BspLeaf &bspLeafAtOrigin() const;

    /**
     * Returns the origin of the object in map space.
     *
     * @see move(), setOrigin(), bspLeafAtOrigin()
     */
    de::Vector3d const &origin() const;

    /**
     * Change the origin of the object in map space.
     *
     * @param newOrigin  New absolute origin to apply, in map units.
     *
     * @see move(), origin()
     */
    virtual void setOrigin(de::Vector3d const &newOrigin);

    /**
     * Translate the origin of the object in map space.
     *
     * @param delta  Movement delta.
     *
     * @see setOrigin(), origin()
     */
    virtual void move(de::Vector3d const &delta);

    /**
     * Returns @c true iff a map is attributed to the object.
     *
     * @see map(), setMap()
     */
    bool hasMap() const;

    /**
     * Returns the map attributed to the object.
     *
     * @see hasMap(), setMap()
     */
    Map &map() const;

    /**
     * Change the map attributed to the map object.
     *
     * @param newMap
     *
     * @see hasMap(), map()
     */
    void setMap(Map *newMap);

    /**
     * Returns the "in-map" index attributed to the map object.
     *
     * @see setIndexInMap()
     */
    int indexInMap() const;

    /**
     * Change the "in-map" index attributed to the map object.
     *
     * @param newIndex  New index to attribute. Use @c NoIndex to clear the
     *                  attribution (not a valid index).
     *
     * @see indexInMap()
     */
    void setIndexInMap(int newIndex = NoIndex);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAPOBJECT_H
