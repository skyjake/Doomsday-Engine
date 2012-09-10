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
#include "rend_bias.h"
#include "m_bams.h"

#include <de/Error>
#include <de/LegacyCore>
#include <de/Log>
#include <de/String>
#include <de/memory.h>

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

// Game-specific, map object type definitions.
static uint numGameMapObjDefs;
static gamemapobjdef_t* gameMapObjDefs;

const char* P_GenerateUniqueMapId(const char* mapID)
{
    static char uid[255];
    lumpnum_t lumpNum = F_CheckLumpNumForName2(mapID, true);
    ddstring_t fileName;

    Str_Init(&fileName);
    F_FileName(&fileName, F_LumpSourceFile(lumpNum));
    qsnprintf(uid, 255, "%s|%s|%s|%s", mapID, Str_Text(&fileName),
              (!F_LumpIsCustom(lumpNum) ? "iwad" : "pwad"), Str_Text(Game_IdentityKey(theGame)));
    strlwr(uid);

    Str_Free(&fileName);
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

gamemapobjdef_t* P_GetGameMapObjDef(int identifier, const char* objName, boolean canCreate)
{
    const int len = (objName? strlen(objName) : 0);
    gamemapobjdef_t* def;

    // Is this a known game object?
    for(uint i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];

        if(objName && objName[0])
        {
            if(!strnicmp(objName, def->name, len)) return def; // Found it!
        }
        else
        {
            if(identifier == def->identifier) return def; // Found it!
        }
    }

    if(!canCreate) return NULL; // Not a known game map object.

    if(identifier == 0) return NULL; // Not a valid indentifier.
    if(!objName || !objName[0]) return NULL; // Must have a name.

    // Ensure the name is unique.
    for(uint i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];
        if(!strnicmp(objName, def->name, len)) return NULL; // Oh dear, a duplicate.
    }

    gameMapObjDefs = (gamemapobjdef_t*)M_Realloc(gameMapObjDefs, ++numGameMapObjDefs * sizeof(*gameMapObjDefs));

    def = &gameMapObjDefs[numGameMapObjDefs - 1];
    def->identifier = identifier;
    def->name = (char*) M_Malloc(len + 1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProps = 0;
    def->props = NULL;

    return def;
}

boolean P_RegisterMapObj(int identifier, const char* name)
{
    return !!P_GetGameMapObjDef(identifier, name, true);
}

static void registerMapObjProperty(int identifier, int propIdentifier,
    const char* propName, valuetype_t type)
{
    gamemapobjdef_t* def = P_GetGameMapObjDef(identifier, NULL, false);

    if(!def) // Not a valid identifier.
        throw de::Error("registerMapObjProperty", QString("Unknown map object identifier %1").arg(identifier));

    if(propIdentifier == 0) // Not a valid identifier.
        throw de::Error("registerMapObjProperty", "0 is not valid argument for propIdentifier");

    if(!propName || !propName[0]) // Must have a name.
        throw de::Error("registerMapObjProperty", "Cannot register a property with a zero-length name");

    // Screen out value types we don't currently support for gmos.
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
        throw de::Error("registerMapObjProperty", QString("Unknown/not supported value type %1").arg(type));
    }

    // Next, make sure propIdentifer and propName are unique.
    int len = (int)strlen(propName);
    for(uint i = 0; i < def->numProps; ++i)
    {
        mapobjprop_t* prop = &def->props[i];

        if(prop->identifier == propIdentifier)
            throw de::Error("registerMapObjProperty",
                            QString("propIdentifier %1 not unique for %2")
                                .arg(propIdentifier).arg(def->name));

        if(!strnicmp(propName, prop->name, len))
            throw de::Error("registerMapObjProperty",
                            QString("propName \"1\" not unique for %2")
                                .arg(propName).arg(def->name));
    }

    // Looks good! Add it to the list of properties.
    def->props = (mapobjprop_t*) M_Realloc(def->props, ++def->numProps * sizeof(*def->props));
    if(!def->props)
        throw de::Error("registerMapObjProperty",
                        QString("Failed on (re)allocation of %1 bytes for new MapObjProperty")
                            .arg((unsigned long) sizeof(*def->props)));

    mapobjprop_t* prop = &def->props[def->numProps - 1];
    prop->identifier = propIdentifier;

    prop->name = (char*) M_Malloc(sizeof(*prop->name) * (len + 1));
    if(!prop->name)
        throw de::Error("registerMapObjProperty",
                        QString("Failed on allocation of %1 bytes for MapObjProperty::name")
                            .arg((unsigned long) (sizeof(*prop->name) * (len + 1))));

    strncpy(prop->name, propName, len);
    prop->name[len] = '\0';
    prop->type = type;
}

boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
    const char* propName, valuetype_t type)
{
    try
    {
        registerMapObjProperty(identifier, propIdentifier, propName, type);
        return true; // Success!
    }
    catch(de::Error& er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
    return false;
}

static void clearGameMapObjDefs(void)
{
    if(gameMapObjDefs)
    {
        for(uint i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjdef_t* def = &gameMapObjDefs[i];
            for(uint k = 0; k < def->numProps; ++k)
            {
                mapobjprop_t* prop = &def->props[k];
                M_Free(prop->name);
            }
            M_Free(def->props);
            M_Free(def->name);
        }
        M_Free(gameMapObjDefs);
        gameMapObjDefs = NULL;
    }
    numGameMapObjDefs = 0;
}

void P_InitGameMapObjDefs(void)
{
    // Allow re-init.
    clearGameMapObjDefs();
}

void P_ShutdownGameMapObjDefs(void)
{
    clearGameMapObjDefs();
}

static valuetable_t* getDBTable(valuedb_t* db, valuetype_t type, boolean canCreate)
{
    valuetable_t* tbl;
    uint i;

    if(!db) return NULL;

    for(i = 0; i < db->numTables; ++i)
    {
        tbl = db->tables[i];
        if(tbl->type == type)
        {   // Found it!
            return tbl;
        }
    }

    if(!canCreate) return NULL;

    // We need to add a new value table to the db.
    db->tables = (valuetable_t**) M_Realloc(db->tables, ++db->numTables * sizeof(*db->tables));
    tbl = db->tables[db->numTables - 1] = (valuetable_t*) M_Malloc(sizeof(valuetable_t));

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElms = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t* db, valuetype_t type, void *data)
{
    valuetable_t* tbl = getDBTable(db, type, true);

    // Insert the new value.
    switch(type)
    {
    case DDVT_BYTE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms);
        ((byte*) tbl->data)[tbl->numElms - 1] = *((byte*) data);
        break;

    case DDVT_SHORT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(short));
        ((short*) tbl->data)[tbl->numElms - 1] = *((short*) data);
        break;

    case DDVT_INT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(int));
        ((int*) tbl->data)[tbl->numElms - 1] = *((int*) data);
        break;

    case DDVT_FIXED:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(fixed_t));
        ((fixed_t*) tbl->data)[tbl->numElms - 1] = *((fixed_t*) data);
        break;

    case DDVT_ANGLE:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(angle_t));
        ((angle_t*) tbl->data)[tbl->numElms - 1] = *((angle_t*) data);
        break;

    case DDVT_FLOAT:
        tbl->data = M_Realloc(tbl->data, ++tbl->numElms * sizeof(float));
        ((float*) tbl->data)[tbl->numElms - 1] = *((float*) data);
        break;

    default:
        throw de::Error("insetIntoDB", QString("Unknown value type %1").arg(type));
    }

    return tbl->numElms - 1;
}

static void* getPtrToDBElm(valuedb_t* db, valuetype_t type, uint elmIdx)
{
    valuetable_t* tbl = getDBTable(db, type, false);

    if(!tbl)
        throw de::Error("getPtrToDBElm", QString("Table for type %1 not found").arg(int(type)));

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx >= tbl->numElms)
        throw de::Error("getPtrToDBElm", "valueIdx out of range");

    switch(tbl->type)
    {
    case DDVT_BYTE:
        return &(((byte*) tbl->data)[elmIdx]);

    case DDVT_SHORT:
        return &(((short*)tbl->data)[elmIdx]);

    case DDVT_INT:
        return &(((int*) tbl->data)[elmIdx]);

    case DDVT_FIXED:
        return &(((fixed_t*) tbl->data)[elmIdx]);

    case DDVT_ANGLE:
        return &(((angle_t*) tbl->data)[elmIdx]);

    case DDVT_FLOAT:
        return &(((float*) tbl->data)[elmIdx]);

    default:
        throw de::Error("P_GetGMOByte", QString("Invalid table type %1").arg(tbl->type));
    }

    // Should never reach here.
    return NULL;
}

void P_DestroyGameMapObjDB(gameobjdata_t* moData)
{
    if(!moData) return;

    if(moData->objLists)
    {
        for(uint i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t* objList = &moData->objLists[i];
            for(uint k = 0; k < objList->num; ++k)
            {
                gamemapobj_t* gmo = objList->objs[k];
                if(gmo->props) M_Free(gmo->props);
                M_Free(gmo);
            }
        }
        M_Free(moData->objLists);
    }
    moData->objLists = 0;

    if(moData->db.tables)
    {
        for(uint i = 0; i < moData->db.numTables; ++i)
        {
            valuetable_t* tbl = moData->db.tables[i];
            if(tbl->data) M_Free(tbl->data);
            M_Free(tbl);
        }
        M_Free(moData->db.tables);
    }
    moData->db.tables = 0;
    moData->db.numTables = 0;
}

static uint countGameMapObjs(gameobjdata_t* moData, int identifier)
{
    if(moData)
    {
        for(uint i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t* objList = &moData->objLists[i];
            if(objList->def->identifier == identifier)
                return objList->num;
        }
    }
    return 0;
}

uint P_CountGameMapObjs(int identifier)
{
    if(!theMap) return 0;
    return countGameMapObjs(&theMap->gameObjData, identifier);
}

static gamemapobjlist_t* getMapObjList(gameobjdata_t* moData, gamemapobjdef_t* def)
{
    if(moData && def)
    {
        for(uint i = 0; i < numGameMapObjDefs; ++i)
        {
            if(moData->objLists[i].def == def)
                return &moData->objLists[i];
        }
    }
    return NULL;
}

gamemapobj_t* P_GetGameMapObj(gameobjdata_t* moData, gamemapobjdef_t* def, uint elmIdx,
    boolean canCreate)
{
    if(!moData->objLists)
    {
        // We haven't yet created the lists.
        moData->objLists = (gamemapobjlist_t*) M_Malloc(sizeof(*moData->objLists) * numGameMapObjDefs);
        for(uint i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t* objList = &moData->objLists[i];

            objList->def = &gameMapObjDefs[i];
            objList->objs = NULL;
            objList->num = 0;
        }
    }

    gamemapobjlist_t* objList = getMapObjList(moData, def);
    DENG2_ASSERT(objList);

    // Have we already created this gmo?
    for(uint i = 0; i < objList->num; ++i)
    {
        gamemapobj_t* gmo = objList->objs[i];
        if(gmo->elmIdx == elmIdx)
            return gmo; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new gamemapobj.
    objList->objs = (gamemapobj_t**) M_Realloc(objList->objs, ++objList->num * sizeof(*objList->objs));

    gamemapobj_t* gmo = objList->objs[objList->num - 1] = (gamemapobj_t*) M_Malloc(sizeof(*gmo));
    gmo->elmIdx = elmIdx;
    gmo->numProps = 0;
    gmo->props = NULL;

    return gmo;
}

static void addGameMapObjValue(gameobjdata_t* moData, gamemapobjdef_t* gmoDef,
    uint propIdx, uint elmIdx, valuetype_t type, void* data)
{
    gamemapobj_t* gmo = P_GetGameMapObj(moData, gmoDef, elmIdx, true);

    if(!gmo)
        throw de::Error("addGameMapObj", "Failed creating new game object def");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(uint i = 0; i < gmo->numProps; ++i)
    {
        if(gmo->props[i].idx == propIdx)
        {
            // We are updating.
            //if(gmo->props[i].type == type)
            //    updateInDB(map->values, type, gmo->props[i].valueIdx, data);
            //else
                throw de::Error("addGameMapObj", "Value type changes not currently supported");
            return;
        }
    }

    // Its a new value.
    gmo->props = (customproperty_t*) M_Realloc(gmo->props, ++gmo->numProps * sizeof(*gmo->props));

    customproperty_t* prop = &gmo->props[gmo->numProps - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&moData->db, type, data);
}

void P_AddGameMapObjValue(gameobjdata_t* moData, gamemapobjdef_t* gmoDef, uint propIdx,
    uint elmIdx, valuetype_t type, void* data)
{
    try
    {
        addGameMapObjValue(moData, gmoDef, propIdx, elmIdx, type, data);
    }
    catch(de::Error& er)
    {
        LOG_WARNING("%s. Ignoring.") << er.asText();
    }
}

static void* getGMOPropValue(gameobjdata_t* data, int identifier, uint elmIdx,
    int propIdentifier, valuetype_t *type)
{
    gamemapobjdef_t* def = P_GetGameMapObjDef(identifier, NULL, false);
    if(!def)
        throw de::Error("getGMOPropValue", QString("Invalid identifier %1").arg(identifier));

    gamemapobj_t* gmo = P_GetGameMapObj(data, def, elmIdx, false);
    if(!gmo)
        throw de::Error("getGMOPropValue", QString("There is no element %1 of type %2").arg(elmIdx).arg(def->name));

    // Find the requested property.
    for(uint i = 0; i < gmo->numProps; ++i)
    {
        customproperty_t* prop = &gmo->props[i];

        if(def->props[prop->idx].identifier == propIdentifier)
        {
            void* ptr = getPtrToDBElm(&data->db, prop->type, prop->valueIdx);
            if(!ptr)
                throw de::Error("getGMOPropValue", "Failed value look up");

            if(type) *type = prop->type;

            return ptr;
        }
    }

    return NULL;
}

/**
 * Takes care of some basic type conversions.
 */
static void setValue(void* dst, valuetype_t dstType, void* src, valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t* d = reinterpret_cast<fixed_t*>(dst);

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = (*((byte*) src) << FRACBITS);
            break;
        case DDVT_INT:
            *d = (*((int*) src) << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = *((fixed_t*) src);
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(*((float*) src));
            break;
        default:
            throw de::Error("setValue", QString("DDVT_FIXED incompatible with value type %1").arg(value_Str(srcType)));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float* d = reinterpret_cast<float*>(dst);

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(*((fixed_t*) src));
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        default:
            throw de::Error("setValue", QString("DDVT_FLOAT incompatible with value type %1").arg(value_Str(srcType)));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte* d = reinterpret_cast<byte*>(dst);

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_FLOAT:
            *d = (byte) *((float*) src);
            break;
        default:
            throw de::Error("setValue", QString("DDVT_BYTE incompatible with value type %1").arg(value_Str(srcType)));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int* d = reinterpret_cast<int*>(dst);

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            throw de::Error("setValue", QString("DDVT_INT incompatible with value type %1.").arg(value_Str(srcType)));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short* d = reinterpret_cast<short*>(dst);

        switch(srcType)
        {
        case DDVT_BYTE:
            *d = *((byte*) src);
            break;
        case DDVT_INT:
            *d = *((int*) src);
            break;
        case DDVT_SHORT:
            *d = *((short*) src);
            break;
        case DDVT_FLOAT:
            *d = *((float*) src);
            break;
        case DDVT_FIXED:
            *d = (*((fixed_t*) src) >> FRACBITS);
            break;
        default:
            throw de::Error("setValue", QString("DDVT_SHORT incompatible with value type %1").arg(value_Str(srcType)));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t* d = reinterpret_cast<angle_t*>(dst);

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            throw de::Error("setValue", QString("DDVT_ANGLE incompatible with value type %1").arg(value_Str(srcType)));
        }
    }
    else
    {
        throw de::Error("setValue", QString("Unknown value type %d").arg(dstType));
    }
}

byte P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier)
{
    byte returnVal = 0;
    if(theMap)
    {
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_BYTE, ptr, type);
        }
    }
    return returnVal;
}

short P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier)
{
    short returnVal = 0;
    if(theMap)
    {
        try
        {
            valuetype_t type;
            void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
            if(ptr) setValue(&returnVal, DDVT_SHORT, ptr, type);
        }
        catch(de::Error& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier)
{
    int returnVal = 0;
    if(theMap)
    {
        try
        {
            valuetype_t type;
            void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
            if(ptr) setValue(&returnVal, DDVT_INT, ptr, type);
        }
        catch(de::Error& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier)
{
    fixed_t returnVal = 0;
    if(theMap)
    {
        try
        {
            valuetype_t type;
            void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
            if(ptr) setValue(&returnVal, DDVT_FIXED, ptr, type);
        }
        catch(de::Error& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier)
{
    angle_t returnVal = 0;
    if(theMap)
    {
        try
        {
            valuetype_t type;
            void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
            if(ptr) setValue(&returnVal, DDVT_ANGLE, ptr, type);
        }
        catch(de::Error& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}

float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier)
{
    float returnVal = 0;
    if(theMap)
    {
        try
        {
            valuetype_t type;
            void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
            if(ptr) setValue(&returnVal, DDVT_FLOAT, ptr, type);
        }
        catch(de::Error& er)
        {
            LOG_WARNING("%s. Returning 0.") << er.asText();
        }
    }
    return returnVal;
}
