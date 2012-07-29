/**
 * @file wadmapconverter.cpp
 * Map converter plugin for id tech 1 format maps. @ingroup wadmapconverter
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
#include "id1map_load.h"
#include "id1map_util.h"
#include <de/Log>

#define map                     DENG_PLUGIN_GLOBAL(map)
Id1Map* map;

/**
 * Allocate and initialize a new MapLumpInfo record.
 */
static MapLumpInfo* newMapLumpInfo(lumpnum_t lumpNum, MapLumpType lumpType)
{
    MapLumpInfo* info = new MapLumpInfo();
    return info->init(lumpNum, lumpType, W_LumpLength(lumpNum));
}

/**
 * Given a map @a uri, attempt to locate the associated marker lump for the
 * map data using the Doomsday file system.
 *
 * @param uri  Map identifier, e.g., "Map:E1M1"
 *
 * @return Lump number for the found data else @c -1
 */
static lumpnum_t locateMapMarkerLumpForUri(const Uri* uri)
{
    DENG_ASSERT(uri);
    const char* mapId = Str_Text(Uri_Path(uri));
    return W_CheckLumpNumForName2(mapId, true /*quiet please*/);
}

/**
 * Given a @a lumpNumber, attempt to recognize the data as a known map format
 * data lump.
 *
 * @note Presently this recognition logic is rather primitive and relies soley
 *       on matching the lump name to a known name set. This should be expanded
 *       to actually qualify the data itself (expected size checks, etc...).
 *
 * @param lumpNumber    Lump number to recognize the format of.
 * @return MapLumpType identifier for the recognized lump, else @c ML_INVALID.
 */
static MapLumpType recognizeMapLump(lumpnum_t lumpNumber)
{
    /// @todo Relocate recognition logic from recognizeMapFormat() here.
    return MapLumpTypeForName(W_LumpName(lumpNumber));
}

/**
 * Find all data lumps associated with this map and populate their metadata
 * in the @a lumpInfos record set.
 *
 * @param lumpInfos     MapLumpInfo record set to populate.
 * @param startLump     Lump number at which to begin searching.
 */
static void collectMapLumps(MapLumpInfo* lumpInfos[NUM_MAPLUMP_TYPES], lumpnum_t startLump)
{
    DENG_ASSERT(lumpInfos);

    LOG_AS("WadMapConverter");
    LOG_TRACE("Locating data lumps...");

    if(startLump < 0) return;

    // Keep checking lumps to see if its a map data lump.
    const int numLumps = *reinterpret_cast<int*>(DD_GetVariable(DD_NUMLUMPS));
    for(lumpnum_t i = startLump; i < numLumps; ++i)
    {
        MapLumpType lumpType = recognizeMapLump(i);

        // If this lump is of an invalid type then we *should* have found all
        // the required map data lumps.
        if(lumpType == ML_INVALID)
        {
            break; // Stop looking.
        }

        // A recognized map data lump; record it in the collection.
        if(lumpInfos[lumpType])
        {
            delete lumpInfos[lumpType];
        }
        lumpInfos[lumpType] = newMapLumpInfo(i, lumpType);
    }
}

static MapFormatId recognizeMapFormat(MapLumpInfo* lumpInfos[NUM_MAPLUMP_TYPES])
{
    DENG_ASSERT(lumpInfos);

    LOG_AS("WadMapConverter");

    // Assume DOOM format by default.
    MapFormatId mapFormat = MF_DOOM;

    // Some data lumps are specific to a particular map format and thus
    // their presence unambiguously signifies which format we have.
    for(uint i = 0; i < (uint)NUM_MAPLUMP_TYPES; ++i)
    {
        const MapLumpInfo* info = lumpInfos[i];
        if(!info) continue;

        switch(info->type)
        {
        default: break;

        case ML_BEHAVIOR:   mapFormat = MF_HEXEN; break;

        case ML_MACROS:
        case ML_LIGHTS:
        case ML_LEAFS:      mapFormat = MF_DOOM64; break;
        }
    }

    // Determine whether each data lump is of the expected size.
    uint numVertexes = 0, numThings = 0, numLines = 0, numSides = 0, numSectors = 0, numLights = 0;
    for(uint i = 0; i < (uint)NUM_MAPLUMP_TYPES; ++i)
    {
        const MapLumpInfo* info = lumpInfos[i];
        if(!info) continue;

        // Determine the number of map data objects of each data type.
        uint* elmCountAddr = NULL;
        size_t elementSize = ElementSizeForMapLumpType(mapFormat, info->type);
        switch(info->type)
        {
        default: break;

        case ML_VERTEXES:   elmCountAddr = &numVertexes;    break;
        case ML_THINGS:     elmCountAddr = &numThings;      break;
        case ML_LINEDEFS:   elmCountAddr = &numLines;       break;
        case ML_SIDEDEFS:   elmCountAddr = &numSides;       break;
        case ML_SECTORS:    elmCountAddr = &numSectors;     break;
        case ML_LIGHTS:     elmCountAddr = &numLights;      break;
        }

        if(elmCountAddr)
        {
            if(0 != info->length % elementSize)
            {
                // What is this??
                return MF_UNKNOWN;
            }

            *elmCountAddr += info->length / elementSize;
        }
    }

    // A valid map has at least one of each of these elements.
    if(!numVertexes || !numLines || !numSides || !numSectors)
    {
        return MF_UNKNOWN;
    }

    LOG_INFO("Recognized a %s format map.") << Str_Text(MapFormatNameForId(mapFormat));
    return mapFormat;
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
    int loadError;

    DENG_UNUSED(hookType);
    DENG_UNUSED(parm);
    DENG_ASSERT(context);

    // Begin the conversion attempt.
    int ret_val = true; // Assume success.

    // Attempt to locate the identified map data marker lump.
    const Uri* uri = reinterpret_cast<const Uri*>(context);
    lumpnum_t markerLump = locateMapMarkerLumpForUri(uri);
    if(0 > markerLump)
    {
        ret_val = false;
        goto FAIL_LOCATE_MAP;
    }

    // Collect all of the map data lumps associated with this map.
    MapLumpInfo* lumpInfos[NUM_MAPLUMP_TYPES];
    memset(lumpInfos, 0, sizeof(lumpInfos));
    collectMapLumps(lumpInfos, markerLump + 1 /*begin after the marker*/);

    // Do we recognize this format?
    MapFormatId mapFormat = recognizeMapFormat(lumpInfos);
    if(mapFormat == MF_UNKNOWN)
    {
        ret_val = false;
        goto FAIL_UNKNOWN_FORMAT;
    }

    // Read the archived map.
    map = new Id1Map(mapFormat);
    loadError = map->load(lumpInfos);
    if(loadError)
    {
        LOG_AS("WadMapConverter");
        LOG_INFO("Internal load error %i, aborting conversion...") << loadError;
        ret_val = false;
        goto FAIL_LOAD_ERROR;
    }

    // Perform post read analyses.
    {
        LOG_AS("WadMapConverter");
        map->analyze();
    }

    // Rebuild the map in Doomsday's native format.
    MPE_Begin("");
    {
        LOG_AS("WadMapConverter");
        map->transfer();

        // We have now finished with our local for-conversion map representation.
        delete map;
        map = 0;
    }
    MPE_End();

    // Cleanup.
FAIL_LOAD_ERROR:
FAIL_UNKNOWN_FORMAT:
    for(uint i = 0; i < (uint)NUM_MAPLUMP_TYPES; ++i)
    {
        MapLumpInfo* info = lumpInfos[i];
        if(!info) continue;
        delete info;
    }

FAIL_LOCATE_MAP:

    return ret_val;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
extern "C" void DP_Initialize(void)
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}
