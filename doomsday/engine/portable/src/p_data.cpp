/**
 * @file p_data.cpp
 * Playsim Data Structures. @ingroup play
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_filesys.h"

#include "game.h"
#include "lumpindex.h"
#include "m_bams.h"
#include "propertyvalue.h"
#include "rend_bias.h"

#include <map>
#include <EntityDatabase>

#include <de/Error>
#include <de/LegacyCore>
#include <de/Log>
#include <de/String>
#include <de/stringpool.h>
#include <de/memory.h>

using de::FS1;

// Map entity definitions.
static StringPool* entityDefs;
typedef std::map<int, StringPoolId> EntityDefIdMap;
static EntityDefIdMap entityDefIdMap;

extern boolean mapSetup;

Uri* mapUri; // Name by which the game referred to the current map.

/**
 * These map data arrays are internal to the engine.
 */
Vertex* vertexes;
SideDef* sideDefs;
LineDef* lineDefs;
Sector* sectors;
Polyobj** polyObjs;

HEdge** hedges;
BspLeaf** bspLeafs;
BspNode** bspNodes;

GameMap* theMap;

char const* P_GenerateUniqueMapId(char const* mapID)
{
    static char uid[255];
    try
    {
        lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(mapID);
        de::File1 const& lump = App_FileSystem()->nameIndexForLump(lumpNum).lump(lumpNum);

        AutoStr* fileName = AutoStr_NewStd();
        F_FileName(fileName, Str_Text(lump.container().name()));

        qsnprintf(uid, 255, "%s|%s|%s|%s", mapID, Str_Text(fileName), (!lump.container().hasCustom()? "iwad" : "pwad"),
                  Str_Text(&reinterpret_cast<de::Game*>(App_CurrentGame())->identityKey()));
        strlwr(uid);
    }
    catch(FS1::NotFoundError const&)
    {
        QString msg = QString("P_GenerateUniqueMapId: Failed finding lump for '%1'.").arg(mapID);
        LegacyCore_FatalError(msg.toUtf8().constData());
    }
    return uid;
}

void P_SetCurrentMap(GameMap* map)
{
    if(!map)
    {
        // @todo: Merge in explicit map unload from branch beta6-mapcache.

        // Most memory is allocated from the zone.
        Z_FreeTags(PU_MAP, PU_PURGELEVEL-1);

        if(mapUri)
        {
            Uri_Delete(mapUri);
            mapUri = NULL;
        }

        vertexes = 0;
        hedges = 0;
        sectors = 0;
        bspLeafs = 0;
        bspNodes = 0;
        lineDefs = 0;
        sideDefs = 0;
        polyObjs = 0;

        theMap = map;
        return;
    }

    mapUri = map->uri;

    vertexes = map->vertexes;
    sideDefs = map->sideDefs;
    lineDefs = map->lineDefs;
    sectors  = map->sectors;
    polyObjs = map->polyObjs;

    hedges   = map->hedges;
    bspLeafs = map->bspLeafs;
    bspNodes = map->bspNodes;

    theMap = map;
}

/// @note Part of the Doomsday public API.
boolean P_MapExists(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    return (lumpNum >= 0);
}

/// @note Part of the Doomsday public API.
boolean P_MapIsCustom(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    return (lumpNum >= 0 && W_LumpIsCustom(lumpNum));
}

/// @note Part of the Doomsday public API.
const char* P_MapSourceFile(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    if(lumpNum < 0) return NULL;
    return W_LumpSourceFile(lumpNum);
}

/// @note Part of the Doomsday public API.
boolean P_LoadMap(const char* uriCString)
{
    if(!uriCString || !uriCString[0])
    {
        LegacyCore_FatalError("P_LoadMap: Invalid Uri argument.");
    }

    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    AutoStr* path = Uri_ToString(uri);
    LOG_MSG("Loading Map \"%s\"...") << Str_Text(path);

    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough M_Free memory of course) so that transitions are
    // (potentially) seamless :-)

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

    if(DAM_AttemptMapLoad(uri))
    {
        GameMap* map = theMap;

        // Init the thinker lists (public and private).
        GameMap_InitThinkerLists(map, 0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(GameMap_OldUniqueId(map));

        GameMap_ClMobjReset(map);

        // Clear player data, too, since we just lost all clmobjs.
        Cl_InitPlayers();

        RL_DeleteLists();
        Rend_CalcLightModRange();

        // Invalidate old cmds and init player values.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];

            /*
            if(isServer && plr->shared.inGame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);*/

            plr->extraLight = plr->targetExtraLight = 0;
            plr->extraLightCounter = 0;
        }

        // Make sure that the next frame doesn't use a filtered viewer.
        R_ResetViewer();

        // Texture animations should begin from their first step.
        Materials_ResetAnimGroups();

        R_InitObjlinkBlockmapForMap();
        LO_InitForMap(); // Lumobj management.
        R_InitShadowProjectionListsForMap(); // Projected mobj shadows.
        VL_InitForMap(); // Converted vlights (from lumobjs) management.

        // Init Particle Generator links.
        P_PtcInitForMap();

        // Initialize the lighting grid.
        LG_InitForMap();

        if(!isDedicated)
            R_InitRendVerticesPool();

        Uri_Delete(uri);
        return true;
    }

    Uri_Delete(uri);
    return false;
}

static int clearEntityDefsWorker(StringPoolId id, void* /*parameters*/)
{
    MapEntityDef* def = static_cast<MapEntityDef*>( StringPool_UserPointer(entityDefs, id) );
    DENG2_ASSERT(def);
    for(uint k = 0; k < def->numProps; ++k)
    {
        MapEntityPropertyDef* prop = def->props + k;
        M_Free(prop->name);
    }
    M_Free(def->props);
    delete def;
    return false; // Continue iteration.
}

static void clearEntityDefs(void)
{
    if(!entityDefs) return;

    StringPool_Iterate(entityDefs, clearEntityDefsWorker, 0/*no parameters*/);
    StringPool_Delete(entityDefs); entityDefs = 0;
    entityDefIdMap.clear();
}

MapEntityDef* P_MapEntityDef(int id)
{
    EntityDefIdMap::iterator i = entityDefIdMap.find(id);
    if(i != entityDefIdMap.end())
    {
        StringPoolId id = i->second;
        return static_cast<MapEntityDef*>( StringPool_UserPointer(entityDefs, id) );
    }
    return 0; // Not found.
}

MapEntityDef* P_MapEntityDefByName(char const* _name)
{
    if(entityDefs)
    {
        ddstring_t name;
        Str_InitStatic(&name, _name);
        StringPoolId id = StringPool_IsInterned(entityDefs, &name);
        return static_cast<MapEntityDef*>( StringPool_UserPointer(entityDefs, id) );
    }
    return 0; // Not found.
}

static int P_NameForMapEntityDefWorker(StringPoolId id, void* parameters)
{
    MapEntityDef* def = static_cast<MapEntityDef*>( parameters );
    if(StringPool_UserPointer(entityDefs, id) == def) return id;
    return 0; // Continue iteration.
}

Str const* P_NameForMapEntityDef(MapEntityDef* def)
{
    if(def)
    {
        StringPoolId id = StringPool_Iterate(entityDefs, P_NameForMapEntityDefWorker, def);
        return StringPool_String(entityDefs, id);
    }
    static de::Str zeroLengthString;
    return zeroLengthString;
}

int MapEntityDef_Property2(MapEntityDef* def, int propertyId, MapEntityPropertyDef** retDef)
{
    DENG2_ASSERT(def);
    MapEntityPropertyDef* found = 0;
    for(uint i = 0; i < def->numProps; ++i)
    {
        MapEntityPropertyDef* prop = def->props + i;
        if(prop->id == propertyId)
        {
            found = prop;
            break;
        }
    }
    if(retDef) *retDef = found;
    return found? found - def->props : -1/* not found */;
}

int MapEntityDef_Property(MapEntityDef* def, int propertyId)
{
    return MapEntityDef_Property2(def, propertyId, NULL/*do not retrieve the property def*/);
}

int MapEntityDef_PropertyByName2(MapEntityDef* def, const char* propertyName, MapEntityPropertyDef** retDef)
{
    DENG2_ASSERT(def);
    MapEntityPropertyDef* found = 0;
    if(propertyName && propertyName[0])
    {
        for(uint i = 0; i < def->numProps; ++i)
        {
            MapEntityPropertyDef* prop = def->props + i;
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

int MapEntityDef_PropertyByName(MapEntityDef* def, const char* propertyName)
{
    return MapEntityDef_PropertyByName2(def, propertyName, NULL/*do not retrieve the property def*/);
}

void MapEntityDef_AddProperty(MapEntityDef* def, int propertyId, const char* propertyName,
                              valuetype_t type)
{
    DENG2_ASSERT(def);

    if(propertyId == 0) // Not a valid identifier.
        throw de::Error("MapEntityDef_AddProperty", "0 is not a valid propertyId");

    if(!propertyName || !propertyName[0]) // Must have a name.
        throw de::Error("MapEntityDef_AddProperty", "Invalid propertyName (zero-length string)");

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
        throw de::Error("MapEntityDef_AddProperty", QString("Unknown/not supported value type %1").arg(type));
    }

    // Ensure both the identifer and the name for the new property are unique.
    if(MapEntityDef_Property(def, propertyId) >= 0)
        throw de::Error("MapEntityDef_AddProperty", QString("propertyId %1 not unique for %2")
                                                    .arg(propertyId).arg(Str_Text(P_NameForMapEntityDef(def))));
    if(MapEntityDef_PropertyByName(def, propertyName) >= 0)
        throw de::Error("MapEntityDef_AddProperty", QString("propertyName \"%1\" not unique for %2")
                                                    .arg(propertyName).arg(Str_Text(P_NameForMapEntityDef(def))));

    // Looks good! Add it to the list of properties.
    def->props = (MapEntityPropertyDef*) M_Realloc(def->props, ++def->numProps * sizeof(*def->props));
    if(!def->props)
        throw de::Error("MapEntityDef_AddProperty",
                        QString("Failed on (re)allocation of %1 bytes for new MapEntityPropertyDef array")
                            .arg((unsigned long) sizeof(*def->props)));

    MapEntityPropertyDef* prop = &def->props[def->numProps - 1];
    prop->id = propertyId;

    int len = (int)strlen(propertyName);
    prop->name = (char*) M_Malloc(sizeof(*prop->name) * (len + 1));
    if(!prop->name)
        throw de::Error("MapEntityDef_AddProperty",
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
static MapEntityDef* findMapEntityDef(int identifier, const char* entityName, boolean canCreate)
{
    if(identifier == 0 && (!entityName || !entityName[0])) return 0;

    // Is this an already known entity?
    if(entityName && entityName[0])
    {
        MapEntityDef* found = P_MapEntityDefByName(entityName);
        if(found) return found;
    }
    else
    {
        MapEntityDef* found = P_MapEntityDef(identifier);
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
        entityDefs = StringPool_New();
    }

    Str name; Str_InitStatic(&name, entityName);
    StringPoolId id = StringPool_Intern(entityDefs, &name);
    MapEntityDef* def = new MapEntityDef(identifier);
    StringPool_SetUserPointer(entityDefs, id, def);

    entityDefIdMap.insert(std::pair<int, StringPoolId>(identifier, id));

    return def;
}

/// @note Part of the Doomsday public API.
boolean P_RegisterMapObj(int identifier, const char* name)
{
    return !!findMapEntityDef(identifier, name, true /*do create*/);
}

/// @note Part of the Doomsday public API.
boolean P_RegisterMapObjProperty(int entityId, int propertyId,
    const char* propertyName, valuetype_t type)
{
    try
    {
        MapEntityDef* def = findMapEntityDef(entityId, 0, false /*do not create*/);
        if(!def) throw de::Error("P_RegisterMapObjProperty", QString("Unknown entityId %1").arg(entityId));

        MapEntityDef_AddProperty(def, propertyId, propertyName, type);
        return true; // Success!
    }
    catch(de::Error const& er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

void P_InitMapEntityDefs(void)
{
    // Allow re-init.
    clearEntityDefs();
}

void P_ShutdownMapEntityDefs(void)
{
    clearEntityDefs();
}

/// @note Part of the Doomsday public API.
extern "C" uint P_CountGameMapObjs(int entityId)
{
    if(!theMap || !theMap->entityDatabase) return 0;
    EntityDatabase* db = theMap->entityDatabase;
    return EntityDatabase_EntityCount(db, P_MapEntityDef(entityId));
}

boolean P_SetMapEntityProperty(EntityDatabase* db, MapEntityPropertyDef* propertyDef,
    uint elementIndex, valuetype_t valueType, void* valueAdr)
{
    try
    {
        return EntityDatabase_SetProperty(db, propertyDef, elementIndex, valueType, valueAdr);
    }
    catch(de::Error const& er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

static MapEntityPropertyDef* entityPropertyDef(int entityId, int propertyId)
{
    // Is this a known entity?
    MapEntityDef* entity = P_MapEntityDef(entityId);
    if(!entity) throw de::Error("entityPropertyDef", QString("Unknown entity definition id %1").arg(entityId));

    // Is this a known property?
    MapEntityPropertyDef* property;
    if(MapEntityDef_Property2(entity, propertyId, &property) < 0)
        throw de::Error("entityPropertyDef", QString("Entity definition %1 has no property with id %2")
                                                 .arg(Str_Text(P_NameForMapEntityDef(entity)))
                                                 .arg(propertyId));

    return property; // Found it.
}

static void setValue(void* dst, valuetype_t dstType, PropertyValue const* pvalue)
{
    switch(dstType)
    {
    case DDVT_FIXED: *((fixed_t*) dst) = pvalue->asFixed(); break;
    case DDVT_FLOAT: *(  (float*) dst) = pvalue->asFloat(); break;
    case DDVT_BYTE:  *(   (byte*) dst) = pvalue->asByte();  break;
    case DDVT_INT:   *(    (int*) dst) = pvalue->asInt32(); break;
    case DDVT_SHORT: *(  (short*) dst) = pvalue->asInt16(); break;
    case DDVT_ANGLE: *((angle_t*) dst) = pvalue->asAngle(); break;
    default:
        throw de::Error("setValue", QString("Unknown value type %d").arg(dstType));
    }
}

/// @note Part of the Doomsday public API.
byte P_GetGMOByte(int entityId, uint elementIndex, int propertyId)
{
    byte returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_BYTE, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

/// @note Part of the Doomsday public API.
short P_GetGMOShort(int entityId, uint elementIndex, int propertyId)
{
    short returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_SHORT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

/// @note Part of the Doomsday public API.
int P_GetGMOInt(int entityId, uint elementIndex, int propertyId)
{
    int returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_INT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

/// @note Part of the Doomsday public API.
fixed_t P_GetGMOFixed(int entityId, uint elementIndex, int propertyId)
{
    fixed_t returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FIXED, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

/// @note Part of the Doomsday public API.
angle_t P_GetGMOAngle(int entityId, uint elementIndex, int propertyId)
{
    angle_t returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_ANGLE, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

/// @note Part of the Doomsday public API.
float P_GetGMOFloat(int entityId, uint elementIndex, int propertyId)
{
    float returnVal = 0;
    if(theMap && theMap->entityDatabase)
    {
        try
        {
            EntityDatabase* db = theMap->entityDatabase;
            MapEntityPropertyDef* propDef = entityPropertyDef(entityId, propertyId);

            setValue(&returnVal, DDVT_FLOAT, EntityDatabase_Property(db, propDef, elementIndex));
        }
        catch(de::Error const& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}
