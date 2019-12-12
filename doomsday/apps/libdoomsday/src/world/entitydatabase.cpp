/** @file entitydatabase.cpp World map entity property value database.
 * @ingroup world
 *
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/entitydatabase.h"

#include <de/Log>
#include <map>

using namespace de;

/// @todo Use Records instead of these.

// An entity is a set of one or more properties.
// Key is the unique identifier of said property in the
// MapEntityPropertyDef it is derived from.
typedef std::map<int, PropertyValue *> Entity;

// Entities are stored in a set, each associated with a unique map element index.
typedef std::map<int, Entity> Entities;

// Entities are grouped in sets by their unique identifier.
typedef std::map<int, Entities> EntitySet;

DENG2_PIMPL(EntityDatabase)
{
    EntitySet entitySets;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        DENG2_FOR_EACH(EntitySet, setIt, entitySets)
        DENG2_FOR_EACH(Entities, entityIt, setIt->second)
        DENG2_FOR_EACH(Entity, propIt, entityIt->second)
        {
            delete propIt->second;
        }
    }

    /**
     * Lookup the set in which entities with the unique identifier
     * @a entityId are stored.
     */
    Entities *entities(int entityId)
    {
        std::pair<EntitySet::iterator, bool> result;
        result = entitySets.insert(std::pair<int, Entities>(entityId, Entities()));
        return &result.first->second;
    }

    /**
     * Lookup an entity in @a set by its unique @a elementIndex.
     */
    Entity *entityByElementIndex(Entities& set, int elementIndex, bool canCreate)
    {
        // Do we already have a record for this entity?
        Entities::iterator found = set.find(elementIndex);
        if (found != set.end()) return &found->second;

        if (!canCreate) return 0;

        // A new entity.
        std::pair<Entities::iterator, bool> result;
        result = set.insert(std::pair<int, Entity>(elementIndex, Entity()));
        return &result.first->second;
    }

    PropertyValue const *tryFindPropertyValue(MapEntityPropertyDef const &def,
                                              int elementIndex)
    {
         Entities *set = entities(def.entity->id);
         Entity const *entity = entityByElementIndex(*set, elementIndex, false /*do not create*/);
         if (!entity)
         {
             throw Error("EntityDatabase::property", QString("There is no element %1 of type %2")
                                                         .arg(elementIndex)
                                                         .arg(Str_Text(P_NameForMapEntityDef(def.entity))));
         }
         Entity::const_iterator found = entity->find(def.id);
         if (found == entity->end())
         {
             return nullptr;
         }
         return found->second;
    }
};

EntityDatabase::EntityDatabase() : d(new Impl(this))
{}

uint EntityDatabase::entityCount(MapEntityDef const *entityDef) const
{
    DENG2_ASSERT(entityDef);
    Entities *set = d->entities(entityDef->id);
    return set->size();
}

bool EntityDatabase::hasEntity(MapEntityDef const *entityDef, int elementIndex) const
{
    DENG2_ASSERT(entityDef);
    Entities *set = d->entities(entityDef->id);
    return d->entityByElementIndex(*set, elementIndex, false /*do not create*/) != 0;
}

PropertyValue const &EntityDatabase::property(MapEntityPropertyDef const *def,
                                              int elementIndex) const
{
    DENG2_ASSERT(def);
    if (auto const *propValue = d->tryFindPropertyValue(*def, elementIndex))
    {
        return *propValue;
    }
    throw Error("EntityDatabase::property",
                QString("Element %1 of type %2 has no value for property %3")
                .arg(elementIndex)
                .arg(Str_Text(P_NameForMapEntityDef(def->entity)))
                .arg(def->id));
}

bool EntityDatabase::hasPropertyValue(MapEntityPropertyDef const *def, int elementIndex) const
{
    DENG2_ASSERT(def);
    return d->tryFindPropertyValue(*def, elementIndex) != nullptr;
}

void EntityDatabase::setProperty(MapEntityPropertyDef const *def, int elementIndex,
    PropertyValue *value)
{
    DENG2_ASSERT(def);
    Entities *set = d->entities(def->entity->id);
    Entity *entity = d->entityByElementIndex(*set, elementIndex, true);
    if (!entity)
        throw Error("EntityDatabase::setProperty", "Failed adding new entity element record");

    // Do we already have a record for this?
    Entity::iterator found = entity->find(def->id);
    if (found != entity->end())
    {
        PropertyValue **adr = &found->second;
        delete *adr;
        *adr = value;
        return;
    }

    // Add a new record.
    entity->insert(std::pair<int, PropertyValue *>(def->id, value));
}
