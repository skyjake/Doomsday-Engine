/** @file mapobject.h  Base class for all map objects.
 * @ingroup world
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_WORLD_MAPOBJECT_H
#define DE_WORLD_MAPOBJECT_H

#include <de/error.h>
#include <de/vector.h>

namespace world {

class BspLeaf;
class Map;

/**
 * Base class for all map objects.
 *
 * While logically related to MapElement, a map object is considered a dynamic
 * and volatile entity (whereas a map element can be largely considered static).
 *
 * The lifetime of a map object may vary massively between instances and range
 * from only a few milliseconds to a few hours or longer.
 */
class MapObject
{
    DE_NO_COPY  (MapObject)
    DE_NO_ASSIGN(MapObject)

public:
    /// No map is attributed. @ingroup errors
    DE_ERROR(MissingMapError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

public:
    MapObject(const de::Vec3d &origin = de::Vec3d());
    virtual ~MapObject();

    DE_CAST_METHODS()

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
    const de::Vec3d &origin() const;

    inline double x() const { return origin().x; }
    inline double y() const { return origin().y; }
    inline double z() const { return origin().z; }

    /**
     * Change the origin of the object in map space.
     *
     * @param newOrigin  New absolute origin to apply, in map units.
     *
     * @see move(), origin()
     */
    virtual void setOrigin(const de::Vec3d &newOrigin);

    /**
     * Translate the origin of the object in map space.
     *
     * @param delta  Movement delta.
     *
     * @see setOrigin(), origin()
     */
    virtual void move(const de::Vec3d &delta);

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
    DE_PRIVATE(d)
};

}  // namespace world

#endif  // DE_WORLD_MAPOBJECT_H
