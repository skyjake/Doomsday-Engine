/** @file entitydef.cpp  World playsim data structures.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is M_Free software; you can redistribute it and/or modify
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

//#include "de_base.h"
#include "doomsday/world/entitydef.h"
#include "doomsday/world/world.h"
#include "doomsday/world/map.h"
#include "doomsday/world/propertyvalue.h"
#include "doomsday/world/entitydatabase.h"

#include <cmath>
#include <map>
#include <de/legacy/memory.h>
#include <de/error.h>
#include <de/log.h>
#include <de/string.h>
#include <de/stringpool.h>

using namespace de;
using World = world::World;

// Map entity definitions.
static StringPool *entityDefs;
typedef std::map<int, StringPool::Id> EntityDefIdMap;
static EntityDefIdMap entityDefIdMap;

static void clearEntityDefs()
{
    if (!::entityDefs) return;

    ::entityDefs->forAll([] (StringPool::Id id)
    {
        auto *def = static_cast<MapEntityDef *>( ::entityDefs->userPointer(id) );
        DE_ASSERT(def);
        for (duint i = 0; i < def->numProps; ++i)
        {
            M_Free(def->props[i].name);
        }
        M_Free(def->props);
        delete def;

        return LoopContinue;
    });
    delete ::entityDefs; ::entityDefs = nullptr;

    ::entityDefIdMap.clear();
}

MapEntityDef *P_MapEntityDef(int id)
{
    EntityDefIdMap::iterator i = entityDefIdMap.find(id);
    if (i != entityDefIdMap.end())
    {
        StringPool::Id id = i->second;
        return static_cast<MapEntityDef *>( entityDefs->userPointer(id) );
    }
    return nullptr;  // Not found.
}

MapEntityDef *P_MapEntityDefByName(const char *name)
{
    if (name && entityDefs)
    {
        StringPool::Id id = entityDefs->isInterned(String(name));
        return static_cast<MapEntityDef *>( entityDefs->userPointer(id) );
    }
    return nullptr;  // Not found.
}

AutoStr *P_NameForMapEntityDef(const MapEntityDef *def)
{
    String name;  // Not found.
    if (def)
    {
        ::entityDefs->forAll([&def, &name] (StringPool::Id id)
        {
            if (::entityDefs->userPointer(id) == def)
            {
                name = ::entityDefs->string(id);
                return LoopAbort;
            }
            return LoopContinue;
        });
    }
    return AutoStr_FromText(name);
}

int MapEntityDef_Property(MapEntityDef *def, int propertyId,
                          MapEntityPropertyDef **retDef = 0)
{
    DE_ASSERT(def);
    MapEntityPropertyDef *found = 0;
    for (uint i = 0; i < def->numProps; ++i)
    {
        MapEntityPropertyDef *prop = def->props + i;
        if (prop->id == propertyId)
        {
            found = prop;
            break;
        }
    }
    if (retDef) *retDef = found;
    return found? found - def->props : -1/* not found */;
}

int MapEntityDef_PropertyByName(MapEntityDef *def, const char *propertyName,
                                MapEntityPropertyDef **retDef)
{
    DE_ASSERT(def);
    MapEntityPropertyDef *found = 0;
    if (propertyName && propertyName[0])
    {
        for (uint i = 0; i < def->numProps; ++i)
        {
            MapEntityPropertyDef *prop = def->props + i;
            if (!iCmpStrCase(prop->name, propertyName))
            {
                found = prop;
                break;
            }
        }
    }
    if (retDef) *retDef = found;
    return found? found - def->props : -1/* not found */;
}

void MapEntityDef_AddProperty(MapEntityDef* def, int propertyId, const char* propertyName,
                              valuetype_t type)
{
    DE_ASSERT(def);

    if (propertyId == 0) // Not a valid identifier.
        throw Error("MapEntityDef_AddProperty", "0 is not a valid propertyId");

    if (!propertyName || !propertyName[0]) // Must have a name.
        throw Error("MapEntityDef_AddProperty", "Invalid propertyName (zero-length string)");

    // A supported value type?
    switch (type)
    {
    case DDVT_BYTE:
    case DDVT_SHORT:
    case DDVT_INT:
    case DDVT_FIXED:
    case DDVT_ANGLE:
    case DDVT_FLOAT:
    case DDVT_DOUBLE:
        break;

    default:
        throw Error("MapEntityDef_AddProperty", stringf("Unknown/not supported value type %i", type));
    }

    // Ensure both the identifer and the name for the new property are unique.
    if (MapEntityDef_Property(def, propertyId) >= 0)
        throw Error("MapEntityDef_AddProperty", stringf("propertyId %i not unique for %s",
                                                    propertyId, Str_Text(P_NameForMapEntityDef(def))));
    if (MapEntityDef_PropertyByName(def, propertyName) >= 0)
        throw Error("MapEntityDef_AddProperty", stringf("propertyName \"%s\" not unique for %s",
                                                    propertyName, Str_Text(P_NameForMapEntityDef(def))));

    // Looks good! Add it to the list of properties.
    def->props = (MapEntityPropertyDef*) M_Realloc(def->props, ++def->numProps * sizeof(*def->props));

    MapEntityPropertyDef* prop = &def->props[def->numProps - 1];
    prop->id = propertyId;

    int len = (int)strlen(propertyName);
    prop->name = (char *) M_Malloc(sizeof(*prop->name) * (len + 1));

    strncpy(prop->name, propertyName, len);
    prop->name[len] = '\0';
    prop->type = type;
    prop->entity = def;
}

/**
 * Look up a mapobj definition.
 *
 * @param identifer   If objName is @c NULL, compare using this unique identifier.
 * @param entityName  If not @c NULL, compare using this unique name.
 * @param canCreate   @c true= create a new definition if not found.
 */
static MapEntityDef *findMapEntityDef(int identifier, const char *entityName,
                                      bool canCreate)
{
    if (identifier == 0 && (!entityName || !entityName[0])) return 0;

    // Is this an already known entity?
    if (entityName && entityName[0])
    {
        MapEntityDef *found = P_MapEntityDefByName(entityName);
        if (found) return found;
    }
    else
    {
        MapEntityDef *found = P_MapEntityDef(identifier);
        if (found) return found;
    }

    // An unknown entity. Are we creating?
    if (!canCreate) return 0;

    // Ensure the name is unique.
    if (P_MapEntityDefByName(entityName)) return 0;

    // Ensure the identifier is unique.
    if (P_MapEntityDef(identifier)) return 0;

    // Have we yet to initialize the map entity definition dataset?
    if (!entityDefs)
    {
        entityDefs = new StringPool;
    }

    StringPool::Id id = entityDefs->intern(String(entityName));
    MapEntityDef *def = new MapEntityDef(identifier);
    entityDefs->setUserPointer(id, def);

    entityDefIdMap.insert(std::pair<int, StringPool::Id>(identifier, id));

    return def;
}

DE_EXTERN_C dd_bool P_RegisterMapObj(int identifier, const char *name)
{
    return findMapEntityDef(identifier, name, true /*do create*/) != 0;
}

DE_EXTERN_C dd_bool P_RegisterMapObjProperty(int         entityId,
                                             int         propertyId,
                                             const char *propertyName,
                                             valuetype_t type)
{
    try
    {
        MapEntityDef *def = findMapEntityDef(entityId, 0, false /*do not create*/);
        if (!def) throw Error("P_RegisterMapObjProperty", stringf("Unknown entityId %i", entityId));

        MapEntityDef_AddProperty(def, propertyId, propertyName, type);
        return true; // Success!
    }
    catch (const Error &er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

void P_InitMapEntityDefs()
{
    // Allow re-init.
    clearEntityDefs();
}

void P_ShutdownMapEntityDefs()
{
    clearEntityDefs();
}

static MapEntityPropertyDef *entityPropertyDef(int entityId, int propertyId)
{
    // Is this a known entity?
    MapEntityDef *entity = P_MapEntityDef(entityId);
    if (!entity) throw Error("entityPropertyDef", stringf("Unknown entity definition id %i", entityId));

    // Is this a known property?
    MapEntityPropertyDef *property;
    if (MapEntityDef_Property(entity, propertyId, &property) < 0)
        throw Error("entityPropertyDef", stringf("Entity definition %s has no property with id %i",
                                                 Str_Text(P_NameForMapEntityDef(entity)),
                                                 propertyId));

    return property; // Found it.
}

static void setValue(void *dst, valuetype_t dstType, const PropertyValue &pvalue)
{
    switch (dstType)
    {
    case DDVT_FIXED:  *((fixed_t *) dst) = pvalue.asFixed();  break;
    case DDVT_FLOAT:  *(  (float *) dst) = pvalue.asFloat();  break;
    case DDVT_DOUBLE: *( (double *) dst) = pvalue.asDouble(); break;
    case DDVT_BYTE:   *(   (byte *) dst) = pvalue.asByte();   break;
    case DDVT_INT:    *(    (int *) dst) = pvalue.asInt32();  break;
    case DDVT_SHORT:  *(  (short *) dst) = pvalue.asInt16();  break;
    case DDVT_ANGLE:  *((angle_t *) dst) = pvalue.asAngle();  break;
    default:
        throw Error("setValue", stringf("Unknown value type %d", dstType));
    }
}

dd_bool P_GMOPropertyIsSet(int entityId, int elementIndex, int propertyId)
{
    if (World::get().hasMap())
    {
        World::get().map().entityDatabase()
                .hasPropertyValue(entityPropertyDef(entityId, propertyId), elementIndex);
    }
    return false;
}

template <typename Type, valuetype_t returnValueType>
Type getEntityValue(int entityId, int elementIndex, int propertyId)
{
    try
    {
        Type returnVal = 0;
        if (World::get().hasMap())
        {
            const EntityDatabase &db = World::get().map().entityDatabase();
            const MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);
            setValue(&returnVal, returnValueType, db.property(propDef, elementIndex));
        }
        return returnVal;
    }
    catch (const Error &er)
    {
        LOG_WARNING("%s. Returning 0.") << er.asText();
        return 0;
    }
}

DE_EXTERN_C byte P_GetGMOByte(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<byte, DDVT_BYTE>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C short P_GetGMOShort(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<short, DDVT_SHORT>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C int P_GetGMOInt(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<int, DDVT_INT>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C fixed_t P_GetGMOFixed(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<fixed_t, DDVT_FIXED>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C angle_t P_GetGMOAngle(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<angle_t, DDVT_ANGLE>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C float P_GetGMOFloat(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<float, DDVT_FLOAT>(entityId, elementIndex, propertyId);
}

DE_EXTERN_C double P_GetGMODouble(int entityId, int elementIndex, int propertyId)
{
    return getEntityValue<double, DDVT_DOUBLE>(entityId, elementIndex, propertyId);
}
