/** @file entitydatabase.h World map entity property value database.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_ENTITYDATABASE_H
#define LIBDOOMSDAY_WORLD_ENTITYDATABASE_H

#include <de/libcore.h>
#include <de/error.h>

#include "valuetype.h"
#include "entitydef.h"
#include "propertyvalue.h"

/**
 * An EntityDatabase is used in the process of transferring mobj spawn spot
 * information and stuff like line action specials from the wad map loader
 * plugin via the engine, through to the game plugin.
 *
 * The primary reason for its existence is that the engine does not know about
 * the game specific properties of the map data types. The engine does not care
 * at all about the values or indeed even what properties are registered; it is
 * simply a way of piping information from one part of the system to another.
 *
 * @todo C++ allows making this more generic: a set/map of polymorphic objects
 * e.g., QVariant.
 */
class LIBDOOMSDAY_PUBLIC EntityDatabase
{
public:
    EntityDatabase();

    /// @return Total number of entities by definition @a entityDef.
    uint entityCount(const MapEntityDef *entityDef) const;

    /**
     * Returns @c true iff an entity with definition @a entityDef and
     * @a elementIndex is known/present.
     */
    bool hasEntity(const MapEntityDef *entityDef, int elementIndex) const;

    /**
     * Lookup a known entity element property value in the database.
     *
     * @param def           Definition of the property to lookup an element value for.
     * @param elementIndex  Unique element index of the value to lookup.
     *
     * @return The found PropertyValue.
     */
    const PropertyValue &property(const MapEntityPropertyDef *def, int elementIndex) const;

    bool hasPropertyValue(const MapEntityPropertyDef *def, int elementIndex) const;

    /**
     * Replace/add a value for a known entity element property to the database.
     *
     * @param def           Definition of the property to add an element value for.
     * @param elementIndex  Unique element index for the value.
     * @param value         The new PropertyValue. Ownership passes to this database.
     */
    void setProperty(const MapEntityPropertyDef *def, int elementIndex,
                     PropertyValue *value);

    /// @copydoc setProperty
    inline void setProperty(const MapEntityPropertyDef *def, int elementIndex,
                            valuetype_t valueType, void *valueAdr)
    {
        setProperty(def, elementIndex, BuildPropertyValue(valueType, valueAdr));
    }

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_WORLD_ENTITYDATABASE_H
