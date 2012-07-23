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

map_t DENG_PLUGIN_GLOBAL(theMap);
map_t* DENG_PLUGIN_GLOBAL(map) = &DENG_PLUGIN_GLOBAL(theMap);
int DENG_PLUGIN_GLOBAL(verbose);

static void configure(void)
{
    DENG_PLUGIN_GLOBAL(verbose) = CommandLine_Exists("-verbose");
}

static lumptype_t mapLumpTypeForName(const char* name)
{
    static const struct maplumpinfo_s {
        const char* name;
        lumptype_t type;
    } lumptypeForNameDict[] =
    {
        { "THINGS",     ML_THINGS },
        { "LINEDEFS",   ML_LINEDEFS },
        { "SIDEDEFS",   ML_SIDEDEFS },
        { "VERTEXES",   ML_VERTEXES },
        { "SEGS",       ML_SEGS },
        { "SSECTORS",   ML_SSECTORS },
        { "NODES",      ML_NODES },
        { "SECTORS",    ML_SECTORS },
        { "REJECT",     ML_REJECT },
        { "BLOCKMAP",   ML_BLOCKMAP },
        { "BEHAVIOR",   ML_BEHAVIOR },
        { "SCRIPTS",    ML_SCRIPTS },
        { "LIGHTS",     ML_LIGHTS },
        { "MACROS",     ML_MACROS },
        { "LEAFS",      ML_LEAFS },
        { "GL_VERT",    ML_GLVERT },
        { "GL_SEGS",    ML_GLSEGS },
        { "GL_SSECT",   ML_GLSSECT },
        { "GL_NODES",   ML_GLNODES },
        { "GL_PVS",     ML_GLPVS},
        { NULL }
    };

    DENG_ASSERT(name);

    if(name[0])
    for(int i = 0; lumptypeForNameDict[i].name; ++i)
    {
        if(!strnicmp(lumptypeForNameDict[i].name, name, strlen(lumptypeForNameDict[i].name)))
            return lumptypeForNameDict[i].type;
    }

    return ML_INVALID;
}

/**
 * Create a new map lump info record.
 */
static maplumpinfo_t* createMapLumpInfo(lumpnum_t lumpNum, lumptype_t lumpType)
{
    maplumpinfo_t* info = static_cast<maplumpinfo_t*>(calloc(1, sizeof(*info)));

    info->lumpNum = lumpNum;
    info->lumpType = lumpType;
    info->length = W_LumpLength(lumpNum);

    return info;
}

/**
 * Free all memory acquired for the given map lump info record.
 */
static void freeMapLumpInfo(maplumpinfo_t* info)
{
    if(!info) return;
    free(info);
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
 */
static void collectMapLumps(maplumpinfo_t* lumpInfos[NUM_LUMP_TYPES], lumpnum_t startLump)
{
    DENG_ASSERT(lumpInfos);

    WADMAPCONVERTER_TRACE("Locating data lumps...");

    const int numLumps = *reinterpret_cast<int*>(DD_GetVariable(DD_NUMLUMPS));
    if(startLump < 0 || startLump >= numLumps) return;

    // Keep checking lumps to see if its a map data lump.
    for(lumpnum_t i = startLump; i < numLumps; ++i)
    {
        // Lookup the lump name in our list of known map lump names.
        const char* lumpName = W_LumpName(i);
        lumptype_t lumpType = mapLumpTypeForName(lumpName);

        if(lumpType == ML_INVALID)
        {
            // Stop looking, we *should* have found them all.
            break;
        }

        // A recognised map data lump; record it in the collection.
        if(lumpInfos[lumpType])
        {
            freeMapLumpInfo(lumpInfos[lumpType]);
        }
        lumpInfos[lumpType] = createMapLumpInfo(i, lumpType);
    }
}

static lumpnum_t locateMapDataMarkerLumpForUri(const Uri* uri)
{
    DENG_ASSERT(uri);
    const char* mapId = Str_Text(Uri_Path(uri));
    return W_CheckLumpNumForName2(mapId, true /*quiet please*/);
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
    DENG_UNUSED(hookType);
    DENG_UNUSED(parm);

    // Setup the processing parameters.
    configure();

    // Begin the conversion attempt.
    int ret_val = true; // Assume success.

    // Attempt to locate the identified map data marker lump.
    const Uri* uri = reinterpret_cast<const Uri*>(context);
    lumpnum_t markerLump = locateMapDataMarkerLumpForUri(uri);
    if(0 > markerLump)
    {
        ret_val = false;
        goto FAIL_LOCATE_MAP;
    }

    // Collect all of the map data lumps associated with this map.
    maplumpinfo_t* lumpInfos[NUM_LUMP_TYPES];
    memset(lumpInfos, 0, sizeof(lumpInfos));
    collectMapLumps(lumpInfos, markerLump + 1 /*begin after the marker*/);

    memset(DENG_PLUGIN_GLOBAL(map), 0, sizeof(*DENG_PLUGIN_GLOBAL(map)));
    if(!IsSupportedFormat(lumpInfos))
    {
        Con_Message("WadMapConverter: Unknown map format, aborting.\n");
        ret_val = false;
        goto FAIL_UNKNOWN_FORMAT;
    }

    // Read the archived map.
    int loadError = !LoadMap(lumpInfos);
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
    for(uint i = 0; i < (uint)NUM_LUMP_TYPES; ++i)
    {
        maplumpinfo_t* info = lumpInfos[i];
        if(!info) continue;
        freeMapLumpInfo(info);
    }

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
