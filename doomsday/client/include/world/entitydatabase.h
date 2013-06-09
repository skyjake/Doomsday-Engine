/** @file world/entitydatabase.h World entity property database.
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

#ifndef DENG_WORLD_ENTITYDATABASE_H
#define DENG_WORLD_ENTITYDATABASE_H

#include <de/libdeng2.h>
#include <de/Error>

#include "api_mapedit.h" // valuetype_t
#include "world/propertyvalue.h"
#include "world/p_mapdata.h"

namespace de {

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
class EntityDatabase
{
public:
    EntityDatabase();

    /// @return Total number of entities by definition @a entityDef.
    uint entityCount(MapEntityDef const *entityDef);

    /**
     * Returns @c true iff an entity with definition @a entityDef and
     * @a elementIndex is known/present.
     */
    bool hasEntity(MapEntityDef const *entityDef, int elementIndex);

    /**
     * Lookup a known entity element property value in the database.
     *
     * @param def           Definition of the property to lookup an element value for.
     * @param elementIndex  Unique element index of the value to lookup.
     *
     * @return The found PropertyValue.
     */
    PropertyValue const &property(MapEntityPropertyDef const *def, int elementIndex);

    /**
     * Replace/add a value for a known entity element property to the database.
     *
     * @param def           Definition of the property to add an element value for.
     * @param elementIndex  Unique element index for the value.
     * @param value         The new PropertyValue. Ownership passes to this database.
     */
    void setProperty(MapEntityPropertyDef const *def, int elementIndex,
                     PropertyValue *value);

    /// @copydoc setProperty
    inline void setProperty(MapEntityPropertyDef const *def, int elementIndex,
                            valuetype_t valueType, void *valueAdr)
    {
        setProperty(def, elementIndex, BuildPropertyValue(valueType, valueAdr));
    }

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_ENTITYDATABASE_H
