/** @file p_data.cpp Playsim Data Structures
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

#define DENG_NO_API_MACROS_MAP

#include "de_base.h"
#include "de_defs.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_system.h"
#include "de_filesys.h"

#include "Game"
#include "map/gamemap.h"
#include "map/propertyvalue.h"
#include "render/rend_bias.h"
#include "render/vlight.h"

#include <map>
#include <EntityDatabase>

#include <de/Error>
#include <de/LegacyCore>
#include <de/Log>
#include <de/String>
#include <de/StringPool>
#include <de/binangle.h>
#include <de/memory.h>

using namespace de;

// Map entity definitions.
static StringPool *entityDefs;
typedef std::map<int, StringPool::Id> EntityDefIdMap;
static EntityDefIdMap entityDefIdMap;

extern "C" boolean mapSetup; // We are currently setting up a map.

GameMap *theMap; // Available globally inside the engine.

#undef P_MapExists
DENG_EXTERN_C boolean P_MapExists(char const *uriCString)
{
    de::Uri uri(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(uri.path().toString().toLatin1(), true/*quiet please*/);
    return (lumpNum >= 0);
}

#undef P_MapIsCustom
DENG_EXTERN_C boolean P_MapIsCustom(char const *uriCString)
{
    de::Uri uri(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(uri.path().toString().toLatin1(), true/*quiet please*/);
    return (lumpNum >= 0 && W_LumpIsCustom(lumpNum));
}

#undef P_MapSourceFile
DENG_EXTERN_C AutoStr *P_MapSourceFile(char const *uriCString)
{
    de::Uri uri(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(uri.path().toString().toLatin1(), true/*quiet please*/);
    if(lumpNum < 0) return AutoStr_NewStd();
    return W_LumpSourceFile(lumpNum);
}

#undef P_LoadMap
DENG_EXTERN_C boolean P_LoadMap(char const *uriCString)
{
    DENG_ASSERT(uriCString && uriCString[0]);

    de::Uri uri(uriCString, RC_NULL);
    LOG_MSG("Loading map \"%s\"...") << uri;

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            //player_t *plr = &ddPlayers[i];
            if(/*!(plr->shared.flags & DDPF_LOCAL) &&*/ clients[i].connected)
            {
                LOG_DEBUG("Client %i marked as 'not ready' to receive frames.") << i;
                clients[i].ready = false;
            }
        }
    }

    Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

    if((theMap = MapArchive_LoadMap(uri)))
    {
        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_VERTEX,  theMap->vertexCount());
            gx.SetupForMapData(DMU_LINEDEF, theMap->lineCount());
            gx.SetupForMapData(DMU_SIDEDEF, theMap->sideDefCount());
            gx.SetupForMapData(DMU_SECTOR,  theMap->sectorCount());
        }

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further.
        P_InitUnusedMobjList();

        // Must be called before any mobjs are spawned.
        theMap->initNodePiles();

#ifdef __CLIENT__
        // Prepare the client-side data.
        if(isClient)
        {
            theMap->initClMobjs();
        }

        Rend_DecorInitForMap();
#endif

        // See what mapinfo says about this map.
        de::Uri mapUri = theMap->uri();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
        if(!mapInfo)
        {
            de::Uri defaultMapUri("*", RC_NULL);
            mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&defaultMapUri));
        }

#ifdef __CLIENT__
        ded_sky_t *skyDef = 0;
        if(mapInfo)
        {
            skyDef = Def_GetSky(mapInfo->skyID);
            if(!skyDef)
                skyDef = &mapInfo->sky;
        }
        Sky_Configure(skyDef);
#endif

        // Setup accordingly.
        if(mapInfo)
        {
            theMap->_globalGravity = mapInfo->gravity;
            theMap->_ambientLightLevel = mapInfo->ambient * 255;
        }
        else
        {
            // No map info found, so set some basic stuff.
            theMap->_globalGravity = 1.0f;
            theMap->_ambientLightLevel = 0;
        }

        theMap->_effectiveGravity = theMap->_globalGravity;

#ifdef __CLIENT__
        Rend_RadioInitForMap();
#endif

        theMap->initSkyFix();

        // Init the thinker lists (public and private).
        GameMap_InitThinkerLists(theMap, 0x1 | 0x2);

#ifdef __CLIENT__
        if(isClient)
        {
            theMap->clMobjReset();
        }

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(theMap->oldUniqueId());

        // Clear player data, too, since we just lost all clmobjs.
        Cl_InitPlayers();

        RL_DeleteLists();
        Rend_CalcLightModRange();
#endif

        // Invalidate old cmds and init player values.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];

            /*
            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);*/

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

#ifdef __CLIENT__
        // Material animations should begin from their first step.
        App_Materials().restartAllAnimations();

        R_InitObjlinkBlockmapForMap();

        LO_InitForMap(); // Lumobj management.
        R_InitShadowProjectionListsForMap(); // Projected mobj shadows.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        // Initialize the lighting grid.
        LG_InitForMap();

        R_InitRendPolyPools();
#endif

        // Init Particle Generator links.
        P_PtcInitForMap();

        return true;
    }

    return false;
}

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
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
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
    return !!findMapEntityDef(identifier, name, true /*do create*/);
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

#undef P_CountGameMapObjs
DENG_EXTERN_C uint P_CountGameMapObjs(int entityId)
{
    if(!theMap || !theMap->entityDatabase) return 0;
    EntityDatabase *db = theMap->entityDatabase;
    return EntityDatabase_EntityCount(db, P_MapEntityDef(entityId));
}

boolean P_SetMapEntityProperty(EntityDatabase *db, MapEntityPropertyDef *propertyDef,
    uint elementIndex, valuetype_t valueType, void *valueAdr)
{
    try
    {
        return EntityDatabase_SetProperty(db, propertyDef, elementIndex, valueType, valueAdr);
    }
    catch(Error const &er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
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

static void setValue(void *dst, valuetype_t dstType, PropertyValue const *pvalue)
{
    switch(dstType)
    {
    case DDVT_FIXED: *((fixed_t *) dst) = pvalue->asFixed(); break;
    case DDVT_FLOAT: *(  (float *) dst) = pvalue->asFloat(); break;
    case DDVT_BYTE:  *(   (byte *) dst) = pvalue->asByte();  break;
    case DDVT_INT:   *(    (int *) dst) = pvalue->asInt32(); break;
    case DDVT_SHORT: *(  (short *) dst) = pvalue->asInt16(); break;
    case DDVT_ANGLE: *((angle_t *) dst) = pvalue->asAngle(); break;
    default:
        throw Error("setValue", QString("Unknown value type %d").arg(dstType));
    }
}

#undef P_GetGMOByte
DENG_EXTERN_C byte P_GetGMOByte(int entityId, uint elementIndex, int propertyId)
{
    byte returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_BYTE, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOShort
DENG_EXTERN_C short P_GetGMOShort(int entityId, uint elementIndex, int propertyId)
{
    short returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_SHORT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOInt
DENG_EXTERN_C int P_GetGMOInt(int entityId, uint elementIndex, int propertyId)
{
    int returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_INT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOFixed
DENG_EXTERN_C fixed_t P_GetGMOFixed(int entityId, uint elementIndex, int propertyId)
{
    fixed_t returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FIXED, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOAngle
DENG_EXTERN_C angle_t P_GetGMOAngle(int entityId, uint elementIndex, int propertyId)
{
    angle_t returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_ANGLE, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

#undef P_GetGMOFloat
DENG_EXTERN_C float P_GetGMOFloat(int entityId, uint elementIndex, int propertyId)
{
    float returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase *db = theMap->entityDatabase;
            MapEntityPropertyDef *propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FLOAT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(Error const &er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}
