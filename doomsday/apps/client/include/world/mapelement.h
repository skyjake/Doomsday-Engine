/** @file mapelement.h  Base class for all map elements.
 * @ingroup world
 *
 * @authors Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAPELEMENT_H
#define DENG_WORLD_MAPELEMENT_H

#include "dmuobject.h"
#include <de/Error>
#include <de/Observers>

namespace world {

class Map;

/**
 * Base class for map elements.
 */
class MapElement : public de::DmuObject
{
public:
    /// Notified whenever the attributed Map changes.
    DENG2_DEFINE_AUDIENCE(MapChange, void mapElementMapChanged(MapElement &));

    /// No map is attributed. @ingroup errors
    DENG2_ERROR(MissingMapError);

public:
    explicit MapElement(de::dint dmuType = DMU_NONE, Map *map = nullptr);
    virtual ~MapElement();

    /**
     * Returns @c true iff a Map is attributed to the map-element.
     *
     * @see map(), setMap(), mapPtr()
     */
    bool hasMap() const;

    /**
     * Returns the Map instance attributed to the map-element.
     *
     * @see hasMap(), setMap(), mapPtr()
     */
    Map &map() const;

    /**
     * Returns a pointer to the Map instance attributed to the map-element (may return
     * @c nullptr if not attributed).
     *
     * @see hasMap(), map()
     */
    Map *mapPtr() const;

    /**
     * Change the map attributed to the map element. Note that if the map element has a
     * @em parent that attempting to change the map property of "this" map element is
     * considered an error (delegation). The MapChange audience is notified if changed.
     *
     * @param newMap
     *
     * @see hasMap(), map()
     */
    void setMap(Map *newMap);

private:
    Map *_map = nullptr;
};

}  // namespace world

#endif  // DENG_WORLD_MAPELEMENT_H
