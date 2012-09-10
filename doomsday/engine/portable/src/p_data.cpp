/**
 * @file p_data.c
 * Playsim Data Structures. @ingroup play
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>
#include <stddef.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_misc.h"

#include "rend_bias.h"
#include "m_bams.h"

extern boolean mapSetup;

Uri* mapUri; // Name by which the game referred to the current map.

/**
 * These map data arrays are internal to the engine.
 */
Vertex* vertexes = NULL;
SideDef* sideDefs = NULL;
LineDef* lineDefs = NULL;
Sector* sectors = NULL;
Polyobj** polyObjs = NULL; // List of all poly-objects in the map.

HEdge** hedges = NULL;
BspLeaf** bspLeafs = NULL;
BspNode** bspNodes = NULL;

GameMap* theMap = NULL;

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
    dd_snprintf(uid, 255, "%s|%s|%s|%s", mapID, Str_Text(&fileName),
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
    sectors = map->sectors;
    polyObjs = map->polyObjs;

    hedges = map->hedges;
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
    AutoStr* path;
    Uri* uri;
    uint i;

    if(!uriCString || !uriCString[0])
    {
        DEBUG_Message(("Warning: P_LoadMap: Passed invalid Uri reference, ignoring.\n"));
        return false; // Yeah, ok... :P
    }

    uri = Uri_NewWithPath2(uriCString, RC_NULL);

    path = Uri_ToString(uri);
    Con_Message("Loading Map \"%s\"...\n", Str_Text(path));

    // It would be very cool if map loading happened in another
    // thread. That way we could be keeping ourselves busy while
    // the intermission is played...

    // We could even try to divide a HUB up into zones, so that
    // when a player enters a zone we could begin loading the map(s)
    // reachable through exits in that zone (providing they have
    // enough free memory of course) so that transitions are
    // (potentially) seamless :-)

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            //player_t *plr = &ddPlayers[i];
            if(/*!(plr->shared.flags & DDPF_LOCAL) &&*/ clients[i].connected)
            {
                DEBUG_Message(("Client %i marked as 'not ready' to receive frames.\n", i));
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
        for(i = 0; i < DDMAXPLAYERS; ++i)
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
    size_t len = (objName? strlen(objName) : 0);
    gamemapobjdef_t* def;
    uint i;

    // Is this a known game object?
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];

        if(objName && objName[0])
        {
            if(!strnicmp(objName, def->name, len))
            {   // Found it!
                return def;
            }
        }
        else
        {
            if(identifier == def->identifier)
            {   // Found it!
                return def;
            }
        }
    }

    if(!canCreate)
        return NULL; // Not a known game map object.

    if(identifier == 0)
        return NULL; // Not a valid indentifier.

    if(!objName || !objName[0])
        return NULL; // Must have a name.

    // Ensure the name is unique.
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        def = &gameMapObjDefs[i];
        if(!strnicmp(objName, def->name, len))
        {   // Oh dear, a duplicate.
            return NULL;
        }
    }

    gameMapObjDefs = M_Realloc(gameMapObjDefs, ++numGameMapObjDefs * sizeof(*gameMapObjDefs));

    def = &gameMapObjDefs[numGameMapObjDefs - 1];
    def->identifier = identifier;
    def->name = M_Malloc(len+1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProps = 0;
    def->props = NULL;

    return def;
}

boolean P_RegisterMapObj(int identifier, const char* name)
{
    return NULL != P_GetGameMapObjDef(identifier, name, true);
}

boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
    const char* propName, valuetype_t type)
{
    gamemapobjdef_t* def = P_GetGameMapObjDef(identifier, NULL, false);
    mapobjprop_t* prop;
    size_t len;
    uint i;

    if(!def) // Not a valid identifier.
    {
        Con_Error("P_RegisterMapObjProperty: Unknown mapobj identifier %i.", identifier);
    }

    if(propIdentifier == 0) // Not a valid identifier.
        Con_Error("P_RegisterMapObjProperty: 0 not valid for propIdentifier.");

    if(!propName || !propName[0]) // Must have a name.
        Con_Error("P_RegisterMapObjProperty: Cannot register without name.");

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
        Con_Error("P_RegisterMapObjProperty: Unknown/not supported value type %i.", type);
    }

    // Next, make sure propIdentifer and propName are unique.
    len = strlen(propName);
    for(i = 0; i < def->numProps; ++i)
    {
        prop = &def->props[i];

        if(prop->identifier == propIdentifier)
            Con_Error("P_RegisterMapObjProperty: propIdentifier %i not unique for %s.",
                      propIdentifier, def->name);

        if(!strnicmp(propName, prop->name, len))
            Con_Error("P_RegisterMapObjProperty: propName \"%s\" not unique for %s.",
                      propName, def->name);
    }

    // Looks good! Add it to the list of properties.
    def->props = (mapobjprop_t*) realloc(def->props, ++def->numProps * sizeof(*def->props));
    if(!def->props)
        Con_Error("P_RegisterMapObjProperty: Failed on (re)allocation of %lu bytes for "
                  "new MapObjProperty.", (unsigned long) sizeof(*def->props));

    prop = &def->props[def->numProps - 1];
    prop->identifier = propIdentifier;

    prop->name = (char*) malloc(sizeof(*prop->name) * (len + 1));
    if(!prop->name)
        Con_Error("P_RegisterMapObjProperty: Failed on allocation of %lu bytes for "
                  "MapObjProperty::name.", (unsigned long) (sizeof(*prop->name) * (len + 1)));

    strncpy(prop->name, propName, len);
    prop->name[len] = '\0';
    prop->type = type;

    return true; // Success!
}

static void clearGameMapObjDefs(void)
{
    if(gameMapObjDefs)
    {
        uint i, j;
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjdef_t* def = &gameMapObjDefs[i];
            for(j = 0; j < def->numProps; ++j)
            {
                mapobjprop_t* prop = &def->props[j];
                free(prop->name);
            }
            free(def->props);
            free(def->name);
        }
        free(gameMapObjDefs);
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
    db->tables = M_Realloc(db->tables, ++db->numTables * sizeof(*db->tables));
    tbl = db->tables[db->numTables - 1] = M_Malloc(sizeof(valuetable_t));

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
        Con_Error("insetIntoDB: Unknown value type %d.", type);
    }

    return tbl->numElms - 1;
}

static void* getPtrToDBElm(valuedb_t* db, valuetype_t type, uint elmIdx)
{
    valuetable_t* tbl = getDBTable(db, type, false);

    if(!tbl)
        Con_Error("getPtrToDBElm: Table for type %i not found.", (int) type);

    // Sanity check: ensure the elmIdx is in bounds.
    if(elmIdx >= tbl->numElms)
        Con_Error("P_GetGMOByte: valueIdx out of bounds.");

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
        Con_Error("P_GetGMOByte: Invalid table type %i.", tbl->type);
    }

    // Should never reach here.
    return NULL;
}

void P_DestroyGameMapObjDB(gameobjdata_t *moData)
{
    uint i, j;

    if(moData->objLists)
    {
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t* objList = &moData->objLists[i];

            for(j = 0; j < objList->num; ++j)
            {
                gamemapobj_t* gmo = objList->objs[j];

                if(gmo->props)
                    M_Free(gmo->props);

                M_Free(gmo);
            }
        }

        M_Free(moData->objLists);
    }
    moData->objLists = NULL;

    if(moData->db.tables)
    {
        for(i = 0; i < moData->db.numTables; ++i)
        {
            valuetable_t* tbl = moData->db.tables[i];

            if(tbl->data)
                M_Free(tbl->data);

            M_Free(tbl);
        }

        M_Free(moData->db.tables);
    }
    moData->db.tables = NULL;
    moData->db.numTables = 0;
}

static uint countGameMapObjs(gameobjdata_t* moData, int identifier)
{
    if(moData)
    {
        uint i;
        for(i = 0; i < numGameMapObjDefs; ++i)
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
    uint i;
    for(i = 0; i < numGameMapObjDefs; ++i)
    {
        if(moData->objLists[i].def == def)
            return &moData->objLists[i];
    }
    return NULL;
}

gamemapobj_t* P_GetGameMapObj(gameobjdata_t* moData, gamemapobjdef_t* def, uint elmIdx,
    boolean canCreate)
{
    gamemapobjlist_t* objList;
    gamemapobj_t* gmo;
    uint i;

    if(!moData->objLists)
    {
        // We haven't yet created the lists.
        moData->objLists = M_Malloc(sizeof(*objList) * numGameMapObjDefs);
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            objList = &moData->objLists[i];

            objList->def = &gameMapObjDefs[i];
            objList->objs = NULL;
            objList->num = 0;
        }
    }

    objList = getMapObjList(moData, def);
    assert(objList);

    // Have we already created this gmo?
    for(i = 0; i < objList->num; ++i)
    {
        gmo = objList->objs[i];
        if(gmo->elmIdx == elmIdx)
            return gmo; // Yep, return it.
    }

    if(!canCreate)
        return NULL;

    // It is a new gamemapobj.
    objList->objs = M_Realloc(objList->objs, ++objList->num * sizeof(gamemapobj_t*));

    gmo = objList->objs[objList->num - 1] = M_Malloc(sizeof(*gmo));
    gmo->elmIdx = elmIdx;
    gmo->numProps = 0;
    gmo->props = NULL;

    return gmo;
}

void P_AddGameMapObjValue(gameobjdata_t* moData, gamemapobjdef_t* gmoDef, uint propIdx,
    uint elmIdx, valuetype_t type, void *data)
{
    gamemapobj_t* gmo = P_GetGameMapObj(moData, gmoDef, elmIdx, true);
    customproperty_t* prop;
    uint i;

    if(!gmo) Con_Error("addGameMapObj: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < gmo->numProps; ++i)
    {
        if(gmo->props[i].idx == propIdx)
        {
            // We are updating.
            //if(gmo->props[i].type == type)
            //    updateInDB(map->values, type, gmo->props[i].valueIdx, data);
            //else
                Con_Error("addGameMapObj: Value type change not currently supported.");
            return;
        }
    }

    // Its a new value.
    gmo->props = M_Realloc(gmo->props, ++gmo->numProps * sizeof(*gmo->props));

    prop = &gmo->props[gmo->numProps - 1];
    prop->idx = propIdx;
    prop->type = type;
    prop->valueIdx = insertIntoDB(&moData->db, type, data);
}

static void* getGMOPropValue(gameobjdata_t* data, int identifier, uint elmIdx,
    int propIdentifier, valuetype_t *type)
{
    gamemapobjdef_t* def;
    gamemapobj_t* gmo;
    uint i;

    def = P_GetGameMapObjDef(identifier, NULL, false);
    if(!def) Con_Error("P_GetGMOByte: Invalid identifier %i.", identifier);

    gmo = P_GetGameMapObj(data, def, elmIdx, false);
    if(!gmo) Con_Error("P_GetGMOByte: There is no element %i of type %s.", elmIdx, def->name);

    // Find the requested property.
    for(i = 0; i < gmo->numProps; ++i)
    {
        customproperty_t* prop = &gmo->props[i];

        if(def->props[prop->idx].identifier == propIdentifier)
        {
            void* ptr = getPtrToDBElm(&data->db, prop->type, prop->valueIdx);
            if(!ptr) Con_Error("P_GetGMOByte: Failed db look up.");

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
        fixed_t* d = dst;

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
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float* d = dst;

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
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte* d = dst;

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
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int* d = dst;

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
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short* d = dst;

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
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n", value_Str(srcType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", dstType);
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
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_SHORT, ptr, type);
        }
    }
    return returnVal;
}

int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier)
{
    int returnVal = 0;
    if(theMap)
    {
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_INT, ptr, type);
        }
    }
    return returnVal;
}

fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier)
{
    fixed_t returnVal = 0;
    if(theMap)
    {
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_FIXED, ptr, type);
        }
    }
    return returnVal;
}

angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier)
{
    angle_t returnVal = 0;
    if(theMap)
    {
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_ANGLE, ptr, type);
        }
    }
    return returnVal;
}

float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier)
{
    float returnVal = 0;
    if(theMap)
    {
        valuetype_t type;
        void* ptr = getGMOPropValue(&theMap->gameObjData, identifier, elmIdx, propIdentifier, &type);
        if(ptr)
        {
            setValue(&returnVal, DDVT_FLOAT, ptr, type);
        }
    }
    return returnVal;
}
