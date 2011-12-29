/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_data.c: Playsim Data Structures, Macros and Constants
 */

// HEADER FILES ------------------------------------------------------------

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

#include <math.h>
#include <stddef.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// Bad texture record.
typedef struct {
    char       *name;
    boolean     planeTex;
    uint    count;
} badtex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean mapSetup;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/**
 * These map data arrays are internal to the engine.
 */
Uri* mapUri; // Name by which the game referred to the current map.
uint numVertexes = 0;
vertex_t* vertexes = NULL;

uint numSegs = 0;
seg_t* segs = NULL;

uint numSectors = 0;
sector_t* sectors = NULL;

uint numSSectors = 0;
subsector_t* ssectors = NULL;

uint numNodes = 0;
node_t* nodes = NULL;

uint numLineDefs = 0;
linedef_t* lineDefs = NULL;

uint numSideDefs = 0;
sidedef_t* sideDefs = NULL;

watchedplanelist_t* watchedPlaneList = NULL;
surfacelist_t* movingSurfaceList = NULL;
surfacelist_t* decoratedSurfaceList = NULL;
surfacelist_t* glowingSurfaceList = NULL;

nodepile_t* mobjNodes = NULL, *lineNodes = NULL; // All kinds of wacky links.

float mapGravity;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static gamemap_t* currentMap = NULL;

// Bad texture list
static uint numBadTexNames = 0;
static uint maxBadTexNames = 0;
static badtex_t* badTexNames = NULL;

// Game-specific, map object type definitions.
static uint numGameMapObjDefs;
static gamemapobjdef_t* gameMapObjDefs;

// CODE --------------------------------------------------------------------

void P_PolyobjChanged(polyobj_t* po)
{
    uint                i;
    seg_t**             segPtr = po->segs;

    for(i = 0; i < po->numSegs; ++i, segPtr++)
    {
        int                 j;
        seg_t*              seg = *segPtr;

        // Shadow bias must be told.
        for(j = 0; j < 3; ++j)
        {
            SB_SurfaceMoved(seg->bsuf[j]);
        }
    }
}

/**
 * Generate a 'unique' identifier for the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char* P_GenerateUniqueMapId(const char* mapID)
{
    static char uid[255];
    lumpnum_t lumpNum = F_CheckLumpNumForName2(mapID, true);
    ddstring_t fileName;

    Str_Init(&fileName);
    F_FileName(&fileName, F_LumpSourceFile(lumpNum));
    dd_snprintf(uid, 255, "%s|%s|%s|%s", mapID, Str_Text(&fileName),
        (!F_LumpIsCustom(lumpNum) ? "iwad" : "pwad"), Str_Text(GameInfo_IdentityKey(DD_CurrentGameInfo())));
    strlwr(uid);

    Str_Free(&fileName);
    return uid;
}

/**
 * @return              Ptr to the current map.
 */
gamemap_t *P_GetCurrentMap(void)
{
    return currentMap;
}

void P_SetCurrentMap(gamemap_t* map)
{
    if(!map)
    {
        // \todo dj: Merge in explicit map unload from branch beta6-mapcache.

        // Most memory is allocated from the zone.
        Z_FreeTags(PU_MAP, PU_PURGELEVEL-1);

        if(mapUri)
        {
            Uri_Delete(mapUri), mapUri = NULL;
        }
        numVertexes = 0;
        vertexes = 0;

        numSegs = 0;
        segs = 0;

        numSectors = 0;
        sectors = 0;

        numSSectors = 0;
        ssectors = 0;

        numNodes = 0;
        nodes = 0;

        numLineDefs = 0;
        lineDefs = 0;

        numSideDefs = 0;
        sideDefs = 0;

        watchedPlaneList = 0;
        movingSurfaceList = 0;
        decoratedSurfaceList = 0;
        glowingSurfaceList = 0;

        numPolyObjs = 0;
        polyObjs = 0;

        mobjNodes = 0;
        lineNodes = 0;
        linelinks = 0;

        mapGravity = 0;

        currentMap = map;
        return;
    }

    mapUri = map->uri;

    numVertexes = map->numVertexes;
    vertexes = map->vertexes;

    numSegs = map->numSegs;
    segs = map->segs;

    numSectors = map->numSectors;
    sectors = map->sectors;

    numSSectors = map->numSSectors;
    ssectors = map->ssectors;

    numNodes = map->numNodes;
    nodes = map->nodes;

    numLineDefs = map->numLineDefs;
    lineDefs = map->lineDefs;

    numSideDefs = map->numSideDefs;
    sideDefs = map->sideDefs;

    watchedPlaneList = &map->watchedPlaneList;
    movingSurfaceList = &map->movingSurfaceList;
    decoratedSurfaceList = &map->decoratedSurfaceList;
    glowingSurfaceList = &map->glowingSurfaceList;

    numPolyObjs = map->numPolyObjs;
    polyObjs = map->polyObjs;

    mobjNodes = &map->mobjNodes;
    lineNodes = &map->lineNodes;
    linelinks = map->lineLinks;

    mapGravity = map->globalGravity;

    currentMap = map;
}

const Uri* P_MapUri(gamemap_t* map)
{
    if(!map) return NULL;
    return map->uri;
}

const char* P_GetUniqueMapId(gamemap_t* map)
{
    if(!map) return NULL;
    return map->uniqueId;
}

void P_GetMapBounds(gamemap_t* map, float* min, float* max)
{
    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

/**
 * Get the ambient light level of the specified map.
 */
int P_GetMapAmbientLightLevel(gamemap_t* map)
{
    if(!map)
        return 0;

    return map->ambientLightLevel;
}

/// \note Part of the Doomsday public API.
boolean P_MapExists(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    return (lumpNum >= 0);
}

/// \note Part of the Doomsday public API.
boolean P_MapIsCustom(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    return (lumpNum >= 0 && W_LumpIsCustom(lumpNum));
}

/// \note Part of the Doomsday public API.
const char* P_MapSourceFile(const char* uriCString)
{
    Uri* uri = Uri_NewWithPath2(uriCString, RC_NULL);
    lumpnum_t lumpNum = W_CheckLumpNumForName2(Str_Text(Uri_Path(uri)), true/*quiet please*/);
    Uri_Delete(uri);
    if(lumpNum < 0) return NULL;
    return W_LumpSourceFile(lumpNum);
}

/// \note Part of the Doomsday public API.
boolean P_LoadMap(const char* uriCString)
{
    ddstring_t* path;
    Uri* uri;
    uint i;

    if(!uriCString || !uriCString[0])
    {
#if _DEBUG
        Con_Message("Warning:P_LoadMap: Passed invalid Uri reference, ignoring.\n");
#endif
        return false; // Yeah, ok... :P
    }

    uri = Uri_NewWithPath2(uriCString, RC_NULL);

    path = Uri_ToString(uri);
    Con_Message("Loading Map \"%s\"...\n", Str_Text(path));
    Str_Delete(path);

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
#ifdef _DEBUG
                Con_Message("Client %i marked as 'not ready' to receive frames.\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    if(DAM_AttemptMapLoad(uri))
    {
        gamemap_t* map = P_GetCurrentMap();

        // Init the thinker lists (public and private).
        P_InitThinkerLists(0x1 | 0x2);

        // Tell shadow bias to initialize the bias light sources.
        SB_InitForMap(P_GetUniqueMapId(map));

        Cl_Reset();
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

void P_RegisterUnknownTexture(const char *name, boolean planeTex)
{
    uint                i;
    char                namet[9];
    boolean             known = false;

    namet[8] = 0;
    memcpy(namet, name, 8);

    // Do we already know about it?
    if(numBadTexNames > 0)
    {
        for(i = 0; i < numBadTexNames && !known; ++i)
        {
            if(!strcmp(badTexNames[i].name, namet) &&
                badTexNames[i].planeTex == planeTex)
            {
                // Yep we already know about it.
                known = true;
                badTexNames[i].count++;
            }
        }
    }

    if(!known)
    {   // A new unknown texture. Add it to the list
        if(++numBadTexNames > maxBadTexNames)
        {
            // Allocate more memory
            maxBadTexNames *= 2;
            if(maxBadTexNames < numBadTexNames)
                maxBadTexNames = numBadTexNames;

            badTexNames = M_Realloc(badTexNames, sizeof(badtex_t)
                                                * maxBadTexNames);
        }

        badTexNames[numBadTexNames -1].name = M_Malloc(strlen(namet) +1);
        strcpy(badTexNames[numBadTexNames -1].name, namet);

        badTexNames[numBadTexNames -1].planeTex = planeTex;
        badTexNames[numBadTexNames -1].count = 1;
    }
}

void P_PrintMissingTextureList(void)
{
    // Announce any bad texture names we came across when loading the map.
    // Non-critical as a "MISSING" texture will be drawn in place of them.
    if(numBadTexNames)
    {
        uint        i;

        Con_Message("  [110] Warning: Found %u bad texture name(s):\n",
                    numBadTexNames);

        for(i = 0; i < numBadTexNames; ++i)
            Con_Message(" %4u x \"%s\"\n", badTexNames[i].count,
                        badTexNames[i].name);
    }
}

/**
 * Frees memory we allocated for bad texture name collection.
 */
void P_FreeBadTexList(void)
{
    uint        i;

    if(badTexNames != NULL)
    {
        for(i = 0; i < numBadTexNames; ++i)
        {
            M_Free(badTexNames[i].name);
            badTexNames[i].name = NULL;
        }

        M_Free(badTexNames);
        badTexNames = NULL;

        numBadTexNames = maxBadTexNames = 0;
    }
}

/**
 * Look up a mapobj definition.
 *
 * @param identifer     If objName is @c NULL, compare using this unique identifier.
 * @param objName       If not @c NULL, compare using this unique name.
 */
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

    gameMapObjDefs =
        M_Realloc(gameMapObjDefs, ++numGameMapObjDefs * sizeof(*gameMapObjDefs));

    def = &gameMapObjDefs[numGameMapObjDefs - 1];
    def->identifier = identifier;
    def->name = M_Malloc(len+1);
    strncpy(def->name, objName, len);
    def->name[len] = '\0';
    def->numProps = 0;
    def->props = NULL;

    return def;
}

/**
 * Called by the game to register the map object types it wishes us to make
 * public via the MPE interface.
 */
boolean P_RegisterMapObj(int identifier, const char *name)
{
    if(P_GetGameMapObjDef(identifier, name, true))
        return true; // Success.

    return false;
}

/**
 * Called by the game to add a new property to a previously registered
 * map object type definition.
 */
boolean P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                 const char *propName, valuetype_t type)
{
    uint                    i;
    size_t                  len;
    mapobjprop_t           *prop;
    gamemapobjdef_t        *def = P_GetGameMapObjDef(identifier, NULL, false);

    if(!def) // Not a valid identifier.
    {
        Con_Error("P_RegisterMapObjProperty: Unknown mapobj identifier %i.",
                  identifier);
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
        Con_Error("P_RegisterMapObjProperty: Unknown/not supported value type %i.",
                  type);
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
    if(NULL == (def->props = (mapobjprop_t*)
       realloc(def->props, ++def->numProps * sizeof(*def->props))))
        Con_Error("P_RegisterMapObjProperty: Failed on (re)allocation of %lu bytes for "
            "new MapObjProperty.", (unsigned long) sizeof(*def->props));

    prop = &def->props[def->numProps - 1];
    prop->identifier = propIdentifier;
    if(NULL == (prop->name = (char*) malloc(sizeof(*prop->name) * (len + 1))))
        Con_Error("P_RegisterMapObjProperty: Failed on allocation of %lu bytes for "
            "MapObjProperty::name.", (unsigned long) (sizeof(*prop->name) * (len + 1)));
    strncpy(prop->name, propName, len);
    prop->name[len] = '\0';
    prop->type = type;

    return true; // Success!
}

static void clearGameMapObjDefs(void)
{
    if(NULL != gameMapObjDefs)
    {
        uint i;
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjdef_t* def = &gameMapObjDefs[i];
            { uint j;
            for(j = 0; j < def->numProps; ++j)
            {
                mapobjprop_t* prop = &def->props[j];
                free(prop->name);
            }}
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

static valuetable_t *getDBTable(valuedb_t *db, valuetype_t type,
                                boolean canCreate)
{
    uint                i;
    valuetable_t       *tbl;

    if(!db)
        return NULL;

    for(i = 0; i < db->numTables; ++i)
    {
        tbl = db->tables[i];
        if(tbl->type == type)
        {   // Found it!
            return tbl;
        }
    }

    if(!canCreate)
        return NULL;

    // We need to add a new value table to the db.
    db->tables = M_Realloc(db->tables, ++db->numTables * sizeof(*db->tables));
    tbl = db->tables[db->numTables - 1] = M_Malloc(sizeof(valuetable_t));

    tbl->data = NULL;
    tbl->type = type;
    tbl->numElms = 0;

    return tbl;
}

static uint insertIntoDB(valuedb_t *db, valuetype_t type, void *data)
{
    valuetable_t       *tbl = getDBTable(db, type, true);

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

/**
 * Destroy the given game map obj database.
 */
void P_DestroyGameMapObjDB(gameobjdata_t *moData)
{
    uint                i, j;

    if(moData->objLists)
    {
        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t*       objList = &moData->objLists[i];

            for(j = 0; j < objList->num; ++j)
            {
                gamemapobj_t       *gmo = objList->objs[j];

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
            valuetable_t       *tbl = moData->db.tables[i];

            if(tbl->data)
                M_Free(tbl->data);

            M_Free(tbl);
        }

        M_Free(moData->db.tables);
    }
    moData->db.tables = NULL;
    moData->db.numTables = 0;
}

static uint countGameMapObjs(gameobjdata_t *moData, int identifier)
{
    if(moData)
    {
        uint                i;

        for(i = 0; i < numGameMapObjDefs; ++i)
        {
            gamemapobjlist_t*   objList = &moData->objLists[i];

            if(objList->def->identifier == identifier)
                return objList->num;
        }
    }

    return 0;
}

uint P_CountGameMapObjs(int identifier)
{
    gamemap_t*          map = P_GetCurrentMap();

    return countGameMapObjs(&map->gameObjData, identifier);
}

static gamemapobjlist_t* getMapObjList(gameobjdata_t* moData,
                                       gamemapobjdef_t* def)
{
    uint                i;

    for(i = 0; i < numGameMapObjDefs; ++i)
        if(moData->objLists[i].def == def)
            return &moData->objLists[i];

    return NULL;
}

gamemapobj_t* P_GetGameMapObj(gameobjdata_t *moData, gamemapobjdef_t *def,
                              uint elmIdx, boolean canCreate)
{
    uint                i;
    gamemapobj_t*       gmo;
    gamemapobjlist_t*   objList;

    if(!moData->objLists)
    {   // We haven't yet created the lists.
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
    objList->objs =
        M_Realloc(objList->objs, ++objList->num * sizeof(gamemapobj_t*));

    gmo = objList->objs[objList->num - 1] = M_Malloc(sizeof(*gmo));
    gmo->elmIdx = elmIdx;
    gmo->numProps = 0;
    gmo->props = NULL;

    return gmo;
}

void P_AddGameMapObjValue(gameobjdata_t *moData, gamemapobjdef_t *gmoDef,
                          uint propIdx, uint elmIdx, valuetype_t type,
                          void *data)
{
    uint                i;
    customproperty_t   *prop;
    gamemapobj_t       *gmo = P_GetGameMapObj(moData, gmoDef, elmIdx, true);

    if(!gmo)
        Con_Error("addGameMapObj: Failed creation.");

    // Check whether this is a new value or whether we are updating an
    // existing one.
    for(i = 0; i < gmo->numProps; ++i)
    {
        if(gmo->props[i].idx == propIdx)
        {   // We are updating.
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

static void* getGMOPropValue(gameobjdata_t *data, int identifier,
                             uint elmIdx, int propIdentifier,
                             valuetype_t *type)
{
    uint                i;
    gamemapobjdef_t    *def;
    gamemapobj_t       *gmo;

    if((def = P_GetGameMapObjDef(identifier, NULL, false)) == NULL)
        Con_Error("P_GetGMOByte: Invalid identifier %i.", identifier);

    if((gmo = P_GetGameMapObj(data, def, elmIdx, false)) == NULL)
        Con_Error("P_GetGMOByte: There is no element %i of type %s.", elmIdx,
                  def->name);

    // Find the requested property.
    for(i = 0; i < gmo->numProps; ++i)
    {
        customproperty_t   *prop = &gmo->props[i];

        if(def->props[prop->idx].identifier == propIdentifier)
        {
            void               *ptr;

            if(NULL ==
               (ptr = getPtrToDBElm(&data->db, prop->type, prop->valueIdx)))
                Con_Error("P_GetGMOByte: Failed db look up.");

            if(type)
                *type = prop->type;

            return ptr;
        }
    }

    return NULL;
}

/**
 * Handle some basic type conversions.
 */
static void setValue(void *dst, valuetype_t dstType, void *src,
                     valuetype_t srcType)
{
    if(dstType == DDVT_FIXED)
    {
        fixed_t        *d = dst;

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
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_FLOAT)
    {
        float          *d = dst;

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
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_BYTE)
    {
        byte           *d = dst;

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
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_INT)
    {
        int            *d = dst;

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
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_SHORT)
    {
        short          *d = dst;

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
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else if(dstType == DDVT_ANGLE)
    {
        angle_t        *d = dst;

        switch(srcType)
        {
        case DDVT_ANGLE:
            *d = *((angle_t*) src);
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(srcType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", dstType);
    }
}

byte P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    byte                returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_BYTE, ptr, type);

    return returnVal;
}

short P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    short               returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_SHORT, ptr, type);

    return returnVal;
}

int P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    int                 returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                          propIdentifier, &type)))
        setValue(&returnVal, DDVT_INT, ptr, type);

    return returnVal;
}

fixed_t P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    fixed_t             returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_FIXED, ptr, type);

    return returnVal;
}

angle_t P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    angle_t             returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_ANGLE, ptr, type);

    return returnVal;
}

float P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier)
{
    valuetype_t         type;
    gamemap_t*          map = P_GetCurrentMap();
    void*               ptr;
    float               returnVal = 0;

    if((ptr = getGMOPropValue(&map->gameObjData, identifier, elmIdx,
                              propIdentifier, &type)))
        setValue(&returnVal, DDVT_FLOAT, ptr, type);

    return returnVal;
}
