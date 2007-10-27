/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
 * dam_main.c: Doomsday Archived Map (DAM), map management.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dam.h"

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

// Should we generate new blockmap data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
byte createBMap = 1;

// Should we generate new reject data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
byte createReject = 1;

// \todo: This should come from the maploader plugin.
const int requiredMapDataClasses[] =
{
    ML_THINGS,
    ML_LINEDEFS,
    ML_SIDEDEFS,
    ML_VERTEXES,
    ML_SECTORS,
    -1
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static archivedmap_t **archivedMaps;
static uint numArchivedMaps;

// CODE --------------------------------------------------------------------

/**
 * Called during init to register the cvars and ccmds for DAM.
 */
void DAM_Register(void)
{
    C_VAR_BYTE("blockmap-build", &createBMap, 0, 0, 2);
    C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
    C_VAR_BYTE("reject-build", &createReject, 0, 0, 2);
}

/**
 * Initializes DAM.
 */
void DAM_Init(void)
{
    DAM_InitReader();

    archivedMaps = NULL;
    numArchivedMaps = 0;
}

/**
 * Shutdown DAM.
 */
void DAM_Shutdown(void)
{
    archivedmap_t *dam;

    // Free all the archived map records.
    dam = *archivedMaps;
    while(dam)
    {
        freeArchivedMap(dam);
    }

    Z_Free(archivedMaps);
    archivedMaps = NULL;
    numArchivedMaps = 0;
}

static int mapLumpTypeForName(const char *name)
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
    listnode_t *node = Z_Calloc(sizeof(listnode_t), PU_STATIC, 0);
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
    maplumpinfo_t *info = Z_Calloc(sizeof(maplumpinfo_t), PU_STATIC, 0);
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
 * Create a new map lump info record.
 */
static maplumpinfo_t* createMapLumpInfo(int lumpNum, int lumpClass)
{
    maplumpinfo_t *info = allocMapLumpInfo();

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
static void addLumpInfoToList(listnode_t **headPtr, maplumpinfo_t *info)
{
    listnode_t *node = allocListNode();

    node->data = info;

    node->next = *headPtr;
    *headPtr = node;
}

/**
 * Allocate memory for a new archived map record.
 */
static archivedmap_t* allocArchivedMap(void)
{
    archivedmap_t *dam = Z_Calloc(sizeof(archivedmap_t), PU_STATIC, 0);
    return dam;
}

/**
 * Free a list of maplumpinfo records.
 */
static void freeMapLumpInfoList(listnode_t *headPtr)
{
    listnode_t *node, *np;

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
 * Free all memory acquired for an archived map record.
 */
static void freeArchivedMap(archivedmap_t *dam)
{
    if(!dam)
        return;

    Z_Free(dam->identifier);
    freeMapLumpInfoList(dam->lumpNodes);
}

/**
 * Create a new archivedmap record.
 */
static archivedmap_t* createArchivedMap(const char *mapID,
                                        listnode_t *lumpNodes)
{
    size_t     len = strlen(mapID);
    archivedmap_t *dam = allocArchivedMap();

    VERBOSE(
    Con_Message("createArchivedMap: Add record for map \"%s\".\n", mapID));

    dam->identifier = Z_Malloc(len + 1, PU_STATIC, 0);
    strncpy(dam->identifier, mapID, len);
    dam->identifier[len] = '\0'; // Terminate.

    dam->lastLoadAttemptFailed = false;

    // \todo Harden the lumpNode linkedlist into an array.
    dam->lumpNodes = lumpNodes;
    if(verbose >= 2)
    {   // Announce.
        listnode_t     *node;

        node = dam->lumpNodes;
        while(node)
        {
            maplumpinfo_t *info = node->data;

            Con_Message("  %s is %lu bytes.\n",
                        (char*) W_CacheLumpNum(info->lumpNum, PU_GETNAME),
                        (unsigned long) info->length);

            node = node->next;
        }
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
    if(archivedMaps)
    {
        archivedmap_t **p = archivedMaps;

        while(*p)
        {
            archivedmap_t *dam = *p;

            if(!stricmp(dam->identifier, mapID))
                return dam;

            *p++;
        }
    }

    return NULL;
}

/**
 * Add an archived map to the list of maps.
 */
static void addArchivedMap(archivedmap_t *dam)
{
    uint        i = 0;
    archivedmap_t **newList =
        Z_Calloc(sizeof(archivedmap_t*) * (++numArchivedMaps + 1),
                 PU_STATIC, 0);

    // Add the new map to the start of the list.
    newList[i++] = dam;

    // Copy the old map ptrs to the new list.
    if(archivedMaps)
    {
        archivedmap_t **p = archivedMaps;

        while(*p)
        {
            archivedmap_t *other = *p;

            newList[i++] = other;

            *p++;
        }

        Z_Free(archivedMaps);
    }

    newList[i] = NULL; // Terminate.
    archivedMaps = newList;
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
static uint collectMapLumps(listnode_t **headPtr, int startLump)
{
    int         i;
    uint        numCollectedLumps = 0;

    VERBOSE(Con_Message("collectMapLumps: Locating lumps...\n"));

    if(startLump > 0 && startLump < numlumps)
    {
        // Keep checking lumps to see if its a map data lump.
        for(i = startLump; i < numlumps; ++i)
        {
            int         lumpType;
            char       *lumpName;

            // Lookup the lump name in our list of known map lump names.
            lumpName = W_CacheLumpNum(i, PU_GETNAME);
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
 * Make sure we have (at least) one lump of each lump class that we require.
 *
 * During the process we will record various info about the lumps that will
 * be of use further down the line.
 *
 * @return  boolean     @c true, if the map dataset provides us with all we
 *                      need proceed with attempting to load the map.
 */
static boolean verifyMapLumps(listnode_t *nodes,
                              const int *requiredMapDataClasses)
{
    uint        i;
    boolean     found;
    listnode_t *node;

    // Ensure we have at least one lump of each required lump class.
    i = 0;
    do
    {
        int         requiredClass = requiredMapDataClasses[i];

        found = false;
        node = nodes;
        while(node && !found)
        {
            maplumpinfo_t *info = node->data;

            // Is this a lump of the class we are looking for?
            if(info->lumpClass == requiredClass)
            {
                // We've found (at least) one lump of this class.
                found = true;
            }

            node = node->next;
        }

        // We didn't find any lumps of this class?
        if(!found)
        {   // Darn, the map data is incomplete.
            return false;
        }
    } while(requiredMapDataClasses[++i] != -1);

    // All is well, we can attempt to determine the map format.
    return true;
}

static archivedmap_t *getArchivedMap(const char *mapID)
{
    archivedmap_t *dam = findArchivedMap(mapID);

    if(!dam)
    {   // We've not yet attempted to load this map.
        int         startLump = W_CheckNumForName(mapID);
        listnode_t *headPtr = NULL;

        if(startLump == -1)
            return NULL;

        // Add the marker lump to the list of lumps for this map.
        addLumpInfoToList(&headPtr, createMapLumpInfo(startLump, ML_LABEL));

        // Find the rest of the map data lumps associated with this map.
        collectMapLumps(&headPtr, startLump + 1);

        // Make sure we have all the data we need to load this map.
        if(verifyMapLumps(headPtr, requiredMapDataClasses))
        {   // The map dataset appears to be formed OK...
            // Create an archivedmap record and link it to the list of maps.
            dam = createArchivedMap(mapID, headPtr);
            addArchivedMap(dam);
        }
        else
        {   // Darn, the level data is incomplete.
            // Free any lumps we may have already precached in the process.
            freeMapLumpInfoList(headPtr);

            // Sorry, but we can't continue...
            return NULL;
        }
    }

    return dam;
}

/**
 * Attempt to load a map associated with the specified identifier.
 */
boolean DAM_AttemptMapLoad(const char *mapID)
{
    archivedmap_t *dam = getArchivedMap(mapID);

    // Load it in.
    if(!dam->lastLoadAttemptFailed)
    {
        // Attempt to load the map.
        if(DAM_LoadMap(dam))
        {
            return true;
        }

        dam->lastLoadAttemptFailed = true;
    }

    return false;
}
