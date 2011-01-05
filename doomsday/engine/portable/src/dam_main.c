/**\file dam_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * Doomsday Archived Map (DAM), map management.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_dam.h"
#include "de_misc.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_defs.h"
#include "de_edit.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void freeArchivedMap(archivedmap_t *dam);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Should we be caching successfully loaded maps?
byte mapCache = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *mapCacheDir = "mapcache/";

static archivedmap_t **archivedMaps;
static uint numArchivedMaps;

// CODE --------------------------------------------------------------------

/**
 * Called during init to register the cvars and ccmds for DAM.
 */
void DAM_Register(void)
{
    C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
}

/**
 * Initializes DAM.
 */
void DAM_Init(void)
{
    archivedMaps = NULL;
    numArchivedMaps = 0;

    P_InitGameMapObjDefs();
}

/**
 * Shutdown DAM.
 */
void DAM_Shutdown(void)
{
    archivedmap_t      *dam;

    // Free all the archived map records.
    dam = *archivedMaps;
    while(dam)
    {
        freeArchivedMap(dam);
    }

    Z_Free(archivedMaps);
    archivedMaps = NULL;
    numArchivedMaps = 0;

    P_ShutdownGameMapObjDefs();
}

static int mapLumpTypeForName(const char* name)
{
    struct maplumpinfo_s {
        int         type;
        const char *name;
    } mapLumpInfos[] =
    {
        {ML_THINGS,     "THINGS"},
        {ML_LINEDEFS,   "LINEDEFS"},
        {ML_SIDEDEFS,   "SIDEDEFS"},
        {ML_VERTEXES,   "VERTEXES"},
        {ML_SEGS,       "SEGS"},
        {ML_SSECTORS,   "SSECTORS"},
        {ML_NODES,      "NODES"},
        {ML_SECTORS,    "SECTORS"},
        {ML_REJECT,     "REJECT"},
        {ML_BLOCKMAP,   "BLOCKMAP"},
        {ML_BEHAVIOR,   "BEHAVIOR"},
        {ML_SCRIPTS,    "SCRIPTS"},
        {ML_LIGHTS,     "LIGHTS"},
        {ML_MACROS,     "MACROS"},
        {ML_LEAFS,      "LEAFS"},
        {ML_INVALID,    NULL}
    };

    int         i;

    if(!name)
        return ML_INVALID;

    for(i = 0; mapLumpInfos[i].type > ML_INVALID; ++i)
    {
        if(!strcmp(name, mapLumpInfos[i].name))
            return mapLumpInfos[i].type;
    }

    return ML_INVALID;
}

/**
 * Allocate a new list node.
 */
static listnode_t* allocListNode(void)
{
    listnode_t         *node = Z_Calloc(sizeof(listnode_t), PU_APPSTATIC, 0);
    return node;
}

/**
 * Free all memory acquired for the given list node.
 */
static void freeListNode(listnode_t *node)
{
    if(node)
        Z_Free(node);
}

/**
 * Allocate memory for a new map lump info record.
 */
static maplumpinfo_t* allocMapLumpInfo(void)
{
    maplumpinfo_t *info = Z_Calloc(sizeof(maplumpinfo_t), PU_APPSTATIC, 0);
    return info;
}

/**
 * Free all memory acquired for the given map lump info record.
 */
static void freeMapLumpInfo(maplumpinfo_t *info)
{
    if(info)
        Z_Free(info);
}

/**
 * Free a list of maplumpinfo records.
 */
static void freeMapLumpInfoList(listnode_t* headPtr)
{
    listnode_t*         node, *np;

    node = headPtr;
    while(node)
    {
        np = node->next;

        if(node->data)
            freeMapLumpInfo(node->data);
        freeListNode(node);

        node = np;
    }
}

/**
 * Create a new map lump info record.
 */
static maplumpinfo_t* createMapLumpInfo(int lumpNum, int lumpClass)
{
    maplumpinfo_t*      info = allocMapLumpInfo();

    info->lumpNum = lumpNum;
    info->lumpClass = lumpClass;
    info->length = W_LumpLength(lumpNum);
    info->format = NULL;
    info->startOffset = 0;

    return info;
}

/**
 * Link a maplumpinfo record to an archivedmap record.
 */
static void addLumpInfoToList(listnode_t** headPtr, maplumpinfo_t* info)
{
    listnode_t*         node = allocListNode();

    node->data = info;

    node->next = *headPtr;
    *headPtr = node;
}

/**
 * Allocate memory for a new archived map record.
 */
static archivedmap_t* allocArchivedMap(void)
{
    archivedmap_t *dam = Z_Calloc(sizeof(archivedmap_t), PU_APPSTATIC, 0);
    return dam;
}

/**
 * Free all memory acquired for an archived map record.
 */
static void freeArchivedMap(archivedmap_t *dam)
{
    if(!dam)
        return;
    Z_Free(dam->lumpList);
    Z_Free(dam);
}

/**
 * Create a new archivedmap record.
 */
static archivedmap_t* createArchivedMap(const char *mapID,
                                        listnode_t *headPtr,
                                        filename_t cachedMapDataFile)
{
    uint                i;
    listnode_t         *ln;
    archivedmap_t      *dam = allocArchivedMap();

    VERBOSE(
    Con_Message("createArchivedMap: Add record for map \"%s\".\n", mapID));

    strncpy(dam->identifier, mapID, 8);
    dam->identifier[8] = '\0'; // Terminate.
    dam->lastLoadAttemptFailed = false;
    memcpy(dam->cachedMapDataFile, cachedMapDataFile,
           sizeof(dam->cachedMapDataFile));
    if(DAM_MapIsValid(dam->cachedMapDataFile,
                      W_GetNumForName(dam->identifier)))
        dam->cachedMapFound = true;

    // Count the lumps for this map
    dam->numLumps = 0;
    ln = headPtr;
    while(ln)
    {
        dam->numLumps++;
        ln = ln->next;
    }

    // Allocate an array of the lump indices.
    dam->lumpList = Z_Malloc(sizeof(int) * dam->numLumps, PU_APPSTATIC, 0);
    ln = headPtr;
    i = 0;
    while(ln)
    {
        maplumpinfo_t      *info = (maplumpinfo_t*) ln->data;

        dam->lumpList[i++] = info->lumpNum;
        ln = ln->next;
    }

    return dam;
}

/**
 * Search the list of maps for one matching the specified identifier.
 *
 * @param mapID         Identifier of the map to be searched for.
 * @return              Ptr to the found map, else @c NULL.
 */
static archivedmap_t* findArchivedMap(const char *mapID)
{
    if(numArchivedMaps)
    {
        archivedmap_t** p = archivedMaps;
        while(*p)
        {
            archivedmap_t *dam = *p;
            if(!strnicmp(dam->identifier, mapID, 8))
                return dam;
            p++;
        }
    }
    return 0;
}

/**
 * Add an archived map to the list of maps.
 */
static void addArchivedMap(archivedmap_t* dam)
{
    archivedMaps =
        Z_Realloc(archivedMaps,
                  sizeof(archivedmap_t*) * (++numArchivedMaps + 1),
                  PU_APPSTATIC);
    archivedMaps[numArchivedMaps - 1] = dam;
    archivedMaps[numArchivedMaps] = NULL; // Terminate.
}

/**
 * Find the lumps associated with this map dataset and link them to the
 * archivedmap record.
 *
 * \note Some obscure PWADs have these lumps in a non-standard order,
 * so we need to go resort to finding them automatically.
 *
 * @param headPtr       The list to link the created maplump records to.
 * @param startLump     The lump number to begin our search with.
 *
 * @return              The number of collected lumps.
 */
static uint collectMapLumps(listnode_t** headPtr, int startLump)
{
    uint numCollectedLumps = 0;

    VERBOSE(Con_Message("collectMapLumps: Locating lumps...\n"));

    if(startLump > 0 && startLump < W_NumLumps())
    {
        // Keep checking lumps to see if its a map data lump.
        int i;
        for(i = startLump; i < W_NumLumps(); ++i)
        {
            const char* lumpName;
            int lumpType;

            // Lookup the lump name in our list of known map lump names.
            lumpName = W_LumpName(i);
            lumpType = mapLumpTypeForName(lumpName);

            if(lumpType != ML_INVALID)
            {   // Its a known map lump.
                maplumpinfo_t *info = createMapLumpInfo(i, lumpType);

                addLumpInfoToList(headPtr, info);
                numCollectedLumps++;
                continue;
            }

            // Stop looking, we *should* have found them all.
            break;
        }
    }

    return numCollectedLumps;
}

/**
 * Compose the path where to put the cached map data.
 */
void DAM_GetCachedMapDir(char* dir,  int mainLump, size_t len)
{
    const char* sourceFile = W_LumpSourceFile(mainLump);
    ushort identifier = 0;
    filename_t base;

    M_ExtractFileBase(base, sourceFile, FILENAME_T_MAXLEN);

    { int i;
    for(i = 0; sourceFile[i]; ++i)
        identifier ^= sourceFile[i] << ((i * 3) % 11);
    }

    // The cached map directory is relative to the runtime directory.
    dd_snprintf(dir, len, "%s%s/%s-%04X/", mapCacheDir, Str_Text(GameInfo_IdentityKey(DD_GameInfo())), base, identifier);

    M_TranslatePath(dir, dir, len);
}

static boolean loadMap(gamemap_t **map, archivedmap_t *dam)
{
    *map = Z_Calloc(sizeof(gamemap_t), PU_MAPSTATIC, 0);

    if(DAM_MapRead(*map, dam->cachedMapDataFile))
        return true;

    return false;
}

static boolean convertMap(gamemap_t **map, archivedmap_t *dam)
{
    boolean converted = false;

    VERBOSE( Con_Message("convertMap: Attempting conversion of \"%s\".\n", dam->identifier) );

    // Nope. See if there is a converter available.
    if(Plug_CheckForHook(HOOK_MAP_CONVERT))
    {
        // Pass the lump list around the map converters, hopefully
        // one of them will recognise the format and convert it.
        if(Plug_DoHook(HOOK_MAP_CONVERT, dam->numLumps, (void*) dam->lumpList))
        {
            converted = true;
            *map = MPE_GetLastBuiltMap();
        }
    }

    if(!converted || verbose >= 2)
        Con_Message("convertMap: %s.\n", (converted? "Successful" : "Failed"));

    return converted;
}

/**
 * Attempt to load the map associated with the specified identifier.
 */
boolean DAM_AttemptMapLoad(const char* mapID)
{
    archivedmap_t*      dam;
    boolean             loadedOK = false;

    VERBOSE2(
    Con_Message("DAM_AttemptMapLoad: Loading \"%s\"...\n", mapID));

    dam = findArchivedMap(mapID);

    if(!dam)
    {   // We've not yet attempted to load this map.
        int                 startLump = W_CheckNumForName(mapID);
        listnode_t*         headPtr = NULL;
        filename_t          cachedMapDir;
        filename_t          cachedMapDataFile;

        if(startLump == -1)
            return false;

        // Add the marker lump to the list of lumps for this map.
        addLumpInfoToList(&headPtr, createMapLumpInfo(startLump, ML_LABEL));

        // Find the rest of the map data lumps associated with this map.
        collectMapLumps(&headPtr, startLump + 1);

        DAM_GetCachedMapDir(cachedMapDir, startLump, FILENAME_T_MAXLEN);

        // Make sure the directory exists.
        M_CheckPath(cachedMapDir);

        // First test if we already have valid cached map data.
        sprintf(cachedMapDataFile, "%s%s", cachedMapDir,
                                   W_LumpName(startLump));
        M_TranslatePath(cachedMapDataFile, cachedMapDataFile,
                        FILENAME_T_MAXLEN);
        strncat(cachedMapDataFile, ".dcm", FILENAME_T_MAXLEN);

        // Create an archivedmap record and link it to our list
        // of available maps.
        dam = createArchivedMap(mapID, headPtr, cachedMapDataFile);
        addArchivedMap(dam);

        freeMapLumpInfoList(headPtr);
    }

    // Load it in.
    if(!dam->lastLoadAttemptFailed)
    {
        gamemap_t* map = NULL;
        ded_mapinfo_t* mapInfo;

        Z_FreeTags(PU_MAP, PU_PURGELEVEL - 1);

        if(mapCache && dam->cachedMapFound)
        {   // Attempt to load the cached map data.
            if(loadMap(&map, dam))
                loadedOK = true;
        }
        else
        {   // Try a JIT conversion with the help of a plugin.
            if(convertMap(&map, dam))
                loadedOK = true;
        }

        if(loadedOK)
        {
            ded_sky_t*          skyDef = NULL;

            // Do any initialization/error checking work we need to do.
            // Must be called before we go any further.
            P_InitUnusedMobjList();

            // Must be called before any mobjs are spawned.
            R_InitLinks(map);

            Rend_DecorInit();

            // Init blockmap for searching subsectors.
            P_BuildSubsectorBlockMap(map);

            strncpy(map->mapID, dam->identifier, 8);
            strncpy(map->uniqueID, P_GenerateUniqueMapID(dam->identifier),
                    sizeof(map->uniqueID));

            // See what mapinfo says about this map.
            mapInfo = Def_GetMapInfo(map->mapID);
            if(!mapInfo)
                mapInfo = Def_GetMapInfo("*");

            if(mapInfo)
            {
                skyDef = Def_GetSky(mapInfo->skyID);
                if(!skyDef)
                    skyDef = &mapInfo->sky;
            }

            R_SetupSky(skyDef);

            // Setup accordingly.
            if(mapInfo)
            {
                map->globalGravity = mapInfo->gravity;
                map->ambientLightLevel = mapInfo->ambient * 255;
            }
            else
            {
                // No map info found, so set some basic stuff.
                map->globalGravity = 1.0f;
                map->ambientLightLevel = 0;
            }

            // \todo should be called from P_LoadMap() but R_InitMap requires the
            // currentMap to be set first.
            P_SetCurrentMap(map);

            R_InitSectorShadows();

            { uint startTime = Sys_GetRealTime();
            R_InitSkyFix();
            VERBOSE( Con_Message("R_InitSkyFix: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
            }
        }
    }

    if(!loadedOK)
        dam->lastLoadAttemptFailed = true;

    return loadedOK;
}
