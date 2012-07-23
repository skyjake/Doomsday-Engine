/**
 * @file wadmapconverter.cpp
 * Doomsday Plugin for converting DOOM-like format maps.
 *
 * The purpose of a wadmapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#include "wadmapconverter.h"
#include "doomsday.h"
#include <de/c_wrapper.h>
#include "map.h"

typedef struct maplumpformat_s {
    int hversion;
    char* formatName;
    int lumpClass;
} maplumpformat_t;

typedef struct maplumpinfo_s {
    lumpnum_t lumpNum;
    maplumpformat_t* format;
    int lumpClass;
    size_t startOffset;
    uint elements;
    size_t length;
} maplumpinfo_t;

typedef struct listnode_s {
    void* data;
    struct listnode_s* next;
} listnode_t;

map_t DENG_PLUGIN_GLOBAL(theMap);
map_t* DENG_PLUGIN_GLOBAL(map) = &DENG_PLUGIN_GLOBAL(theMap);
int DENG_PLUGIN_GLOBAL(verbose);

static int mapLumpTypeForName(const char* name)
{
    static const struct maplumpinfo_s {
        int type;
        const char* name;
    } mapLumpInfos[] =
    {
        { ML_THINGS,     "THINGS" },
        { ML_LINEDEFS,   "LINEDEFS" },
        { ML_SIDEDEFS,   "SIDEDEFS" },
        { ML_VERTEXES,   "VERTEXES" },
        { ML_SEGS,       "SEGS" },
        { ML_SSECTORS,   "SSECTORS" },
        { ML_NODES,      "NODES" },
        { ML_SECTORS,    "SECTORS" },
        { ML_REJECT,     "REJECT" },
        { ML_BLOCKMAP,   "BLOCKMAP" },
        { ML_BEHAVIOR,   "BEHAVIOR" },
        { ML_SCRIPTS,    "SCRIPTS" },
        { ML_LIGHTS,     "LIGHTS" },
        { ML_MACROS,     "MACROS" },
        { ML_LEAFS,      "LEAFS" },
        { ML_INVALID,    NULL }
    };

    if(!name) return ML_INVALID;

    for(int i = 0; mapLumpInfos[i].type > ML_INVALID; ++i)
    {
        if(!strnicmp(mapLumpInfos[i].name, name, strlen(mapLumpInfos[i].name)))
            return mapLumpInfos[i].type;
    }

    return ML_INVALID;
}

/**
 * Allocate a new list node.
 */
static listnode_t* allocListNode(void)
{
    listnode_t* node = static_cast<listnode_t*>(Z_Calloc(sizeof(listnode_t), PU_APPSTATIC, 0));
    return node;
}

/**
 * Free all memory acquired for the given list node.
 */
static void freeListNode(listnode_t* node)
{
    if(!node) return;
    Z_Free(node);
}

/**
 * Allocate memory for a new map lump info record.
 */
static maplumpinfo_t* allocMapLumpInfo(void)
{
    maplumpinfo_t* info = static_cast<maplumpinfo_t*>(Z_Calloc(sizeof(maplumpinfo_t), PU_APPSTATIC, 0));
    return info;
}

/**
 * Free all memory acquired for the given map lump info record.
 */
static void freeMapLumpInfo(maplumpinfo_t* info)
{
    if(!info) return;
    Z_Free(info);
}

/**
 * Free a list of maplumpinfo records.
 */
static void freeMapLumpInfoList(listnode_t* headPtr)
{
    listnode_t* node = headPtr;
    while(node)
    {
        listnode_t* np = node->next;

        if(node->data)
            freeMapLumpInfo(reinterpret_cast<maplumpinfo_t*>(node->data));
        freeListNode(node);

        node = np;
    }
}

/**
 * Create a new map lump info record.
 */
static maplumpinfo_t* createMapLumpInfo(lumpnum_t lumpNum, int lumpClass)
{
    maplumpinfo_t* info = allocMapLumpInfo();

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
    listnode_t* node = allocListNode();
    node->data = info;
    node->next = *headPtr;
    *headPtr = node;
}

/**
 * Find the lumps associated with this map dataset and link them to the
 * archivedmap record.
 *
 * @note Some obscure PWADs have these lumps in a non-standard order,
 * so we need to go resort to finding them automatically.
 *
 * @param headPtr       The list to link the created maplump records to.
 * @param startLump     The lump number to begin our search with.
 *
 * @return              The number of collected lumps.
 */
static uint collectMapLumps(listnode_t** headPtr, lumpnum_t startLump)
{
    WADMAPCONVERTER_TRACE("collectMapLumps: Locating lumps...");

    const int numLumps = *reinterpret_cast<int*>(DD_GetVariable(DD_NUMLUMPS));
    uint numCollectedLumps = 0;

    if(startLump > 0 && startLump < numLumps)
    {
        // Keep checking lumps to see if its a map data lump.
        for(lumpnum_t i = startLump; i < numLumps; ++i)
        {
            // Lookup the lump name in our list of known map lump names.
            const char* lumpName = W_LumpName(i);
            int lumpType = mapLumpTypeForName(lumpName);

            if(lumpType != ML_INVALID)
            {
                // Its a known map lump.
                maplumpinfo_t* info = createMapLumpInfo(i, lumpType);

                addLumpInfoToList(headPtr, info);
                numCollectedLumps += 1;
                continue;
            }

            // Stop looking, we *should* have found them all.
            break;
        }
    }

    return numCollectedLumps;
}

/**
 * This function will be called when Doomsday is asked to load a map that is
 * not available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int parm, void* context)
{
    int ret_val = true; // Assume success.

    DENG_UNUSED(hookType);
    DENG_UNUSED(parm);

    DENG_PLUGIN_GLOBAL(verbose) = CommandLine_Exists("-verbose");

    const Uri* uri = reinterpret_cast<const Uri*>(context);
    const char* mapId = Str_Text(Uri_Path(uri));
    lumpnum_t markerLump;

    markerLump = W_CheckLumpNumForName2(mapId, true /*quiet please*/);
    if(0 > markerLump)
    {
        ret_val = false;
        goto FAIL_LOCATE_MAP;
    }

    // Add the marker lump to the list of lumps for this map.
    listnode_t* sourceLumpListHead = NULL;
    addLumpInfoToList(&sourceLumpListHead, createMapLumpInfo(markerLump, ML_LABEL));

    // Find the rest of the map data lumps associated with this map.
    collectMapLumps(&sourceLumpListHead, markerLump + 1);

    // Count the number of source data lumps.
    uint lumpListSize = 0;
    listnode_t* node = sourceLumpListHead;
    while(node)
    {
        ++lumpListSize;
        node = node->next;
    }

    // Allocate and populate the source data lump list.
    lumpnum_t* lumpList = static_cast<lumpnum_t*>(Z_Malloc(sizeof(*lumpList) * lumpListSize, PU_APPSTATIC, 0));
    if(!lumpList)
        Con_Error("WadMapConverter: Failed on allocation of %lu bytes for lump list.",
                  (unsigned long) (sizeof(*lumpList) * lumpListSize));

    node = sourceLumpListHead;
    uint idx = 0;
    while(node)
    {
        maplumpinfo_t* info = reinterpret_cast<maplumpinfo_t*>(node->data);
        lumpList[idx++] = info->lumpNum;
        node = node->next;
    }

    memset(DENG_PLUGIN_GLOBAL(map), 0, sizeof(*DENG_PLUGIN_GLOBAL(map)));
    if(!IsSupportedFormat(lumpList, lumpListSize))
    {
        Con_Message("WadMapConverter: Unknown map format, aborting.\n");
        ret_val = false;
        goto FAIL_UNKNOWN_FORMAT;
    }

    // Read the archived map.
    int loadError = !LoadMap(lumpList, lumpListSize);
    if(loadError)
    {
        Con_Message("WadMapConverter: Internal error, load failed.\n");
        ret_val = false;
        goto FAIL_LOAD_ERROR;
    }

    // Perform post read analses.
    AnalyzeMap();

    // Rebuild the map in Doomsday's native format.
    ret_val = TransferMap();

    // Cleanup.
FAIL_LOAD_ERROR:
FAIL_UNKNOWN_FORMAT:
    sourceLumpListHead;
    freeMapLumpInfoList(sourceLumpListHead);
    Z_Free(lumpList);

FAIL_LOCATE_MAP:

    return ret_val;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}
