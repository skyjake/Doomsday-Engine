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

#include "dd_share.h"
#include "api_map.h"
#include "world/bspleaf.h"
#include <de/Error>
#include <de/Observers>
#include <de/String>

namespace de {

/**
 *
 */
class IAnchor
{
public:
    virtual ~IAnchor() {}

    /**
     * Returns the coordinate space origin of the object.
     *
     * @see move(), setOrigin()
     */
    virtual de::Vector3d const &origin() const = 0;

    inline de::ddouble x() const { return origin().x; }
    inline de::ddouble y() const { return origin().y; }
    inline de::ddouble z() const { return origin().z; }

    /**
     * Change the coordinate space origin of the object in map space.
     *
     * @param newOrigin  New absolute origin to apply, in map units.
     *
     * @see move(), origin()
     */
    virtual void setOrigin(de::Vector3d const &newOrigin) = 0;

    /**
     * Change the coordinate space origin of the object in map space.
     *
     * @param delta  Movement delta.
     *
     * @see setOrigin(), origin()
     */
    inline void move(de::Vector3d const &delta) { setOrigin(origin() + delta); }
};

}  // namespace de

namespace world {

class Map;

class MapLink
{
public:
    /// No map is attributed. @ingroup errors
    DENG2_ERROR(MissingMapError);

    /// Notified whenever the attributed Map changes.
    DENG2_DEFINE_AUDIENCE2(MapChanged, void mapLinkMapChanged(MapLink &));

public:
    MapLink(Map *map = nullptr);
    virtual ~MapLink() {}

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
     * @em parent that attempting to change the map property of "this" map element is an
     * error (delegation).
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

namespace de {

/**
 * Abstract base class for objects addressable via the public Map Update (DMU) API.
 * Provides runtime type information and safe dynamic casting to various derived types.
 */
class DmuObject
{
public:
    /// Attempted to configure an invalid parent element. @ingroup errors
    DENG2_ERROR(InvalidParentError);

    /// No parent map element is configured. @ingroup errors
    DENG2_ERROR(MissingParentError);

    /// No map is attributed. @ingroup errors
    DENG2_ERROR(MissingAnchorError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

public:
    /**
     * @param type  DMU type identifier.
     */
    explicit DmuObject(de::dint type = DMU_NONE, de::dint index = NoIndex);
    virtual ~DmuObject() {}

    DENG2_AS_IS_METHODS()

    /// Returns the DMU_* type of the object.
    de::dint type() const;

    /**
     * Returns a textual description of the map element, intended only for humans.
     *
     * @param verbosity  Level of verbosity:
     *                    - Level 1: include instance identifier
     *                   Use @c -1 for autodetection (depends on current log entry level).
     *
     * @return  Full human-friendly description of the object.
     */
    de::String description(dint verbosty = -1) const;

    /**
     * Returns a textual description of this map element only. Subclasses must override
     * this method to provide a description relevant to the subclass.
     *
     * @return  Human-friendly description of this map element only.
     */
    virtual de::String describe() const;

    /**
     * Returns the archive index for the map element. The archive index is the position of
     * the relevant data or definition in the archived map. For example, in the case of a
     * DMU_SIDE element that is produced from an id Tech 1 format map, this should be the
     * index of the definition in the SIDEDEFS data lump.
     *
     * @see setIndexInArchive()
     */
    de::dint indexInArchive() const;

    /**
     * Change the "archive index" of the map element to @a newIndex.
     *
     * @see indexInArchive()
     */
    void setIndexInArchive(de::dint newIndex = NoIndex);

    /**
     * Returns the DMU instance identifier attributed to the map element.
     */
    de::dint indexInMap() const;

    /**
     * Change the DMU instance identifier attributed to the map element to @a newIndex.
     */
    void setIndexInMap(de::dint newIndex = NoIndex);

    /**
     * Returns @c true iff a parent is attributed to the map element.
     *
     * @see parent(), setParent()
     */
    bool hasParent() const;

    /**
     * Returns the parent of the map element.
     *
     * @see hasParent(), setParent()
     */
    DmuObject       &parent();
    DmuObject const &parent() const;

    /**
     * Change the parent of the map element.
     *
     * @param newParent  DmuObject to attribute as the new parent (use @c nullptr to clear).
     * Ownership is unaffected..
     *
     * @see hasParent(), parent()
     */
    void setParent(DmuObject *newParent);

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
    virtual de::dint property(de::DmuArgs &args) const;

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
    virtual de::dint setProperty(de::DmuArgs const &args);

private:
    DENG2_PRIVATE(d)
};

}  // namespace de

namespace world {

/**
 * Base class for map elements.
 */
class MapElement : public de::DmuObject, public MapLink
{
public:
    explicit MapElement(de::dint dmuType = DMU_NONE, Map *map = nullptr);
    virtual ~MapElement() {}
};

}

#endif  // DENG_WORLD_MAPELEMENT_H
