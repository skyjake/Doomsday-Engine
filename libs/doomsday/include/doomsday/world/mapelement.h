/** @file mapelement.h  Base class for all map elements.
 * @ingroup world
 *
 * @authors Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "dd_share.h"
#include "dmuargs.h"
#include <de/error.h>

namespace world {

class Map;

/**
 * Base class for all elements of a map. Provides runtime type information and safe dynamic
 * casting to various derived types.
 *
 * Maps are composed out of vertices, lines, sectors, etc.
 *
 * Abstract handling of map elements is particularly helpful in the public Map Update (DMU)
 * API, where objects can be referenced either by type and index or by an opaque pointer.
 */
class LIBDOOMSDAY_PUBLIC MapElement
{
    DE_NO_COPY  (MapElement)
    DE_NO_ASSIGN(MapElement)

public:
    /// Attempted to configure an invalid parent element. @ingroup errors
    DE_ERROR(InvalidParentError);

    /// No map is attributed. @ingroup errors
    DE_ERROR(MissingMapError);

    /// The referenced property does not exist. @ingroup errors
    DE_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DE_ERROR(WritePropertyError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

public:
    /**
     * Construct a new MapElement
     *
     * @param type    DMU type identifier.
     * @param parent  Parent map element (if any).
     */
    explicit MapElement(int t = DMU_NONE, MapElement *parent = nullptr);

    virtual ~MapElement();

    /**
     * Returns the DMU_* type of the object.
     */
    int type() const;

    DE_CAST_METHODS()

    /**
     * Returns @c true iff a parent is attributed to the map element.
     *
     * @see parent(), setParent()
     */
    inline bool hasParent() const { return _parent != nullptr; }

    /**
     * Returns the parent of the map element.
     *
     * @see hasParent(), setParent()
     */
    inline MapElement &parent() {
        DE_ASSERT(_parent);
        return *_parent;
    }

    inline const MapElement &parent() const {
        DE_ASSERT(_parent);
        return *_parent;
    }

    /**
     * Change the parent of the map element.
     *
     * @param newParent  MapElement to attribute as the new parent (use @c nullptr to clear).
     * Ownership is unaffected..
     *
     * @see hasParent(), parent()
     */
    void setParent(MapElement *newParent);

    /**
     * Returns @c true iff a map is attributed to the map element. Note that if the map
     * element has a @em parent that this state is delegated to the parent map element.
     *
     * @see map(), setMap(), mapPtr()
     */
    bool hasMap() const;

    /**
     * Returns the map attributed to the map element. Note that if the map element has a
     * @em parent that this property comes from the parent map element (delegation).
     *
     * @see hasMap(), setMap(), mapPtr()
     */
    Map &map() const;

    /**
     * Returns a pointer to the attributed world::Map or @c nullptr if not attributed.
     */
    inline Map *mapPtr() const { return hasMap() ? &map() : nullptr; }

    /**
     * Change the map attributed to the map element. Note that if the map element has a
     * @em parent that attempting to change the map property of "this" map element is an
     * error (delegation).
     *
     * @param newMap
     *
     * @see hasMap(), map()
     */
    void setMap(Map *newMap);

    /**
     * Returns the "in-map" index attributed to the map element.
     */
    int indexInMap() const;

    /**
     * Change the "in-map" index attributed to the map element.
     *
     * @param newIndex  New index to attribute to the map element. @c NoIndex clears the
     * attribution (not a valid index).
     */
    void setIndexInMap(int newIndex = NoIndex);

    /**
     * Returns the archive index for the map element. The archive index is the position of
     * the relevant data or definition in the archived map. For example, in the case of a
     * DMU_SIDE element that is produced from an id Tech 1 format map, this should be the
     * index of the definition in the SIDEDEFS data lump.
     *
     * @see setIndexInArchive()
     */
    int indexInArchive() const;

    /**
     * Change the "archive index" of the map element to @a newIndex.
     *
     * @see indexInArchive()
     */
    void setIndexInArchive(int newIndex = NoIndex);

    /**
     * Get a property value, selected by DMU_* name.
     *
     * Derived classes can override this to implement read access for additional DMU
     * properties. @ref property() must be called from an overridding method if the named
     * property is unknown/not handled, returning the result. If the property is known
     * and the read access is handled the overriding method should return @c false.
     *
     * @param args  Property arguments.
     *
     * @return  Always @c 0 (can be used as an iterator).
     */
    virtual int property(world::DmuArgs &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * Derived classes can override this to implement write access for additional DMU
     * properties. @ref setProperty() must be called from an overridding method if the
     * named property is unknown/not handled, returning the result. If the property is
     * known and the write access is handled the overriding method should return @c false.
     *
     * @param args  Property arguments.

     * @return  Always @c 0 (can be used as an iterator).
     */
    virtual int setProperty(const world::DmuArgs &args);

private:
    DE_PRIVATE(d)

    MapElement *_parent = nullptr;
};

} // namespace world

#undef DMU_Str
LIBDOOMSDAY_PUBLIC const char *DMU_Str(de::duint prop);
