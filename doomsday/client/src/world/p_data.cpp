/** @file p_data.cpp World playsim data structures.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>
#include <map>

#include <de/memory.h>

#include <de/Error>
#include <de/Log>
#include <de/String>
#include <de/StringPool>

#include "de_base.h"
#include "de_play.h"

#include "EntityDatabase"

#include "world/map.h"
#include "world/propertyvalue.h"

using namespace de;

// Map entity definitions.
static StringPool *entityDefs;
typedef std::map<int, StringPool::Id> EntityDefIdMap;
static EntityDefIdMap entityDefIdMap;

static int clearEntityDefsWorker(StringPool::Id id, void * /*parameters*/)
{
    MapEntityDef *def = static_cast<MapEntityDef *>( entityDefs->userPointer(id) );
    DENG2_ASSERT(def);
    for(uint i = 0; i < def->numProps; ++i)
    {
        MapEntityPropertyDef *prop = def->props + i;
        M_Free(prop->name);
    }
    M_Free(def->props);
    delete def;
    return false; // Continue iteration.
}

static void clearEntityDefs(void)
{
    if(!entityDefs) return;

    entityDefs->iterate(clearEntityDefsWorker, 0/*no parameters*/);
    delete entityDefs; entityDefs = 0;
    entityDefIdMap.clear();
}

MapEntityDef *P_MapEntityDef(int id)
{
    EntityDefIdMap::iterator i = entityDefIdMap.find(id);
    if(i != entityDefIdMap.end())
    {
        StringPool::Id id = i->second;
        return static_cast<MapEntityDef *>( entityDefs->userPointer(id) );
    }
    return 0; // Not found.
}

MapEntityDef *P_MapEntityDefByName(char const *name)
{
    if(name && entityDefs)
    {
        StringPool::Id id = entityDefs->isInterned(String(name));
        return static_cast<MapEntityDef *>( entityDefs->userPointer(id) );
    }
    return 0; // Not found.
}

static int P_NameForMapEntityDefWorker(StringPool::Id id, void *parameters)
{
    MapEntityDef *def = static_cast<MapEntityDef *>( parameters );
    if(entityDefs->userPointer(id) == def) return id;
    return 0; // Continue iteration.
}

AutoStr *P_NameForMapEntityDef(MapEntityDef *def)
{
    if(def)
    {
        StringPool::Id id = entityDefs->iterate(P_NameForMapEntityDefWorker, def);
        String const& name = entityDefs->string(id);
        QByteArray nameUtf8 = name.toUtf8();
        return AutoStr_FromText(nameUtf8.constData());
    }
    return AutoStr_NewStd();
}

int MapEntityDef_Property(MapEntityDef *def, int propertyId,
                          MapEntityPropertyDef **retDef = 0)
{
    DENG2_ASSERT(def);
    MapEntityPropertyDef *found = 0;
    for(uint i = 0; i < def->numProps; ++i)
    {
        MapEntityPropertyDef *prop = def->props + i;
        if(prop->id == propertyId)
        {
            found = prop;
            break;
        }
    }
    if(retDef) *retDef = found;
    return found? found - def->props : -1/* not found */;
}

int MapEntityDef_PropertyByName(MapEntityDef *def, char const *propertyName,
                                MapEntityPropertyDef **retDef)
{
    DENG2_ASSERT(def);
    MapEntityPropertyDef *found = 0;
    if(propertyName && propertyName[0])
    {
        for(uint i = 0; i < def->numProps; ++i)
        {
            MapEntityPropertyDef *prop = def->props + i;
            if(!stricmp(prop->name, propertyName))
            {
                found = prop;
                break;
            }
        }
    }
    if(retDef) *retDef = found;
    return found? found - def->props : -1/* not found */;
}

void MapEntityDef_AddProperty(MapEntityDef* def, int propertyId, const char* propertyName,
                              valuetype_t type)
{
    DENG2_ASSERT(def);

    if(propertyId == 0) // Not a valid identifier.
        throw Error("MapEntityDef_AddProperty", "0 is not a valid propertyId");

    if(!propertyName || !propertyName[0]) // Must have a name.
        throw Error("MapEntityDef_AddProperty", "Invalid propertyName (zero-length string)");

    // A supported value type?
    switch(type)
    {
    case DDVT_BYTE:
    case DDVT_SHORT:
    case DDVT_INT:
    case DDVT_FIXED:
    case DDVT_ANGLE:
    case DDVT_FLOAT:
        break;

    default:
        throw Error("MapEntityDef_AddProperty", QString("Unknown/not supported value type %1").arg(type));
    }

    // Ensure both the identifer and the name for the new property are unique.
    if(MapEntityDef_Property(def, propertyId) >= 0)
        throw Error("MapEntityDef_AddProperty", QString("propertyId %1 not unique for %2")
                                                    .arg(propertyId).arg(Str_Text(P_NameForMapEntityDef(def))));
    if(MapEntityDef_PropertyByName(def, propertyName) >= 0)
        throw Error("MapEntityDef_AddProperty", QString("propertyName \"%1\" not unique for %2")
                                                    .arg(propertyName).arg(Str_Text(P_NameForMapEntityDef(def))));

    // Looks good! Add it to the list of properties.
    def->props = (MapEntityPropertyDef*) M_Realloc(def->props, ++def->numProps * sizeof(*def->props));
    if(!def->props)
        throw Error("MapEntityDef_AddProperty",
                        QString("Failed on (re)allocation of %1 bytes for new MapEntityPropertyDef array")
                            .arg((unsigned long) sizeof(*def->props)));

    MapEntityPropertyDef* prop = &def->props[def->numProps - 1];
    prop->id = propertyId;

    int len = (int)strlen(propertyName);
    prop->name = (char *) M_Malloc(sizeof(*prop->name) * (len + 1));
    if(!prop->name)
        throw Error("MapEntityDef_AddProperty",
                        QString("Failed on allocation of %1 bytes for property name")
                            .arg((unsigned long) (sizeof(*prop->name) * (len + 1))));

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
static MapEntityDef *findMapEntityDef(int identifier, char const *entityName,
                                      bool canCreate)
{
    if(identifier == 0 && (!entityName || !entityName[0])) return 0;

    // Is this an already known entity?
    if(entityName && entityName[0])
    {
        MapEntityDef *found = P_MapEntityDefByName(entityName);
        if(found) return found;
    }
    else
    {
        MapEntityDef *found = P_MapEntityDef(identifier);
        if(found) return found;
    }

    // An unknown entity. Are we creating?
    if(!canCreate) return 0;

    // Ensure the name is unique.
    if(P_MapEntityDefByName(entityName)) return 0;

    // Ensure the identifier is unique.
    if(P_MapEntityDef(identifier)) return 0;

    // Have we yet to initialize the map entity definition dataset?
    if(!entityDefs)
    {
        entityDefs = new StringPool;
    }

    StringPool::Id id = entityDefs->intern(String(entityName));
    MapEntityDef *def = new MapEntityDef(identifier);
    entityDefs->setUserPointer(id, def);

    entityDefIdMap.insert(std::pair<int, StringPool::Id>(identifier, id));

    return def;
}

#undef P_RegisterMapObj
DENG_EXTERN_C boolean P_RegisterMapObj(int identifier, char const *name)
{
    return findMapEntityDef(identifier, name, true /*do create*/) != 0;
}

#undef P_RegisterMapObjProperty
DENG_EXTERN_C boolean P_RegisterMapObjProperty(int entityId, int propertyId,
                                               char const *propertyName, valuetype_t type)
{
    try
    {
        MapEntityDef *def = findMapEntityDef(entityId, 0, false /*do not create*/);
        if(!def) throw Error("P_RegisterMapObjProperty", QString("Unknown entityId %1").arg(entityId));

        MapEntityDef_AddProperty(def, propertyId, propertyName, type);
        return true; // Success!
    }
    catch(Error const &er)
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
    if(!entity) throw Error("entityPropertyDef", QString("Unknown entity definition id %1").arg(entityId));

    // Is this a known property?
    MapEntityPropertyDef *property;
    if(MapEntityDef_Property(entity, propertyId, &property) < 0)
        throw Error("entityPropertyDef", QString("Entity definition %1 has no property with id %2")
                                                 .arg(Str_Text(P_NameForMapEntityDef(entity)))
                                                 .arg(propertyId));

    return property; // Found it.
}

static void setValue(void *dst, valuetype_t dstType, PropertyValue const &pvalue)
{
    switch(dstType)
    {
    case DDVT_FIXED: *((fixed_t *) dst) = pvalue.asFixed(); break;
    case DDVT_FLOAT: *(  (float *) dst) = pvalue.asFloat(); break;
    case DDVT_BYTE:  *(   (byte *) dst) = pvalue.asByte();  break;
    case DDVT_INT:   *(    (int *) dst) = pvalue.asInt32(); break;
    case DDVT_SHORT: *(  (short *) dst) = pvalue.asInt16(); break;
    case DDVT_ANGLE: *((angle_t *) dst) = pvalue.asAngle(); break;
    default:
        throw Error("setValue", QString("Unknown value type %d").arg(dstType));
    }
}

#undef P_GetGMOByte
DENG_EXTERN_C byte P_GetGMOByte(int entityId, int elementIndex, int propertyId)
{
    byte returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_BYTE, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOShort
DENG_EXTERN_C short P_GetGMOShort(int entityId, int elementIndex, int propertyId)
{
    short returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_SHORT, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOInt
DENG_EXTERN_C int P_GetGMOInt(int entityId, int elementIndex, int propertyId)
{
    int returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_INT, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOFixed
DENG_EXTERN_C fixed_t P_GetGMOFixed(int entityId, int elementIndex, int propertyId)
{
    fixed_t returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FIXED, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOAngle
DENG_EXTERN_C angle_t P_GetGMOAngle(int entityId, int elementIndex, int propertyId)
{
    angle_t returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_ANGLE, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOFloat
DENG_EXTERN_C float P_GetGMOFloat(int entityId, int elementIndex, int propertyId)
{
    float returnVal = 0;
    if(App_World().hasMap())
    {
        try
        {
            EntityDatabase &db = App_World().map().entityDatabase();
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FLOAT, db.property(propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}
