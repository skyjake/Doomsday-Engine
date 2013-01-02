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
#include <de/Error>
#include <de/Log>

#define map                     DENG_PLUGIN_GLOBAL(map)
Id1Map* map;

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
    DENG2_ASSERT(uri);
    const char* mapId = Str_Text(Uri_Path(uri));
    return W_CheckLumpNumForName2(mapId, true /*quiet please*/);
}

/**
 * Add a @a newLumpInfo record to the @a lumpInfos record collection. If an
 * existing MapLumpInfo record of the same type is present in the collection
 * it will be replaced.
 *
 * @param lumpInfos     MapLumpInfo record set to be updated.
 * @param newLumpInfo   New MapLumpInfo to be added to @a lumpInfos.
 * @return  Same as @a newLumpInfo for caller convenience.
 */
static MapLumpInfo& addMapLumpInfoToCollection(MapLumpInfos& lumpInfos,
    MapLumpInfo& newLumpInfo)
{
    MapLumpType lumpType = newLumpInfo.type;
    MapLumpInfos::iterator i = lumpInfos.find(lumpType);
    if(i != lumpInfos.end())
    {
        delete i->second;
    }
    lumpInfos[lumpType] = &newLumpInfo;
    return newLumpInfo;
}

/**
 * Find all data lumps associated with this map and populate their metadata
 * in the @a lumpInfos record set.
 *
 * @param lumpInfos     MapLumpInfo record set to populate.
 * @param startLump     Lump number at which to begin searching.
 */
static void collectMapLumps(MapLumpInfos& lumpInfos, lumpnum_t startLump)
{
    LOG_AS("WadMapConverter");
    LOG_TRACE("Locating data lumps...");

    if(startLump < 0) return;

    // Keep checking lumps to see if its a map data lump.
    const int numLumps = *reinterpret_cast<int*>(DD_GetVariable(DD_NUMLUMPS));
    for(lumpnum_t i = startLump; i < numLumps; ++i)
    {
        // Lump name determines whether this lump should be included.
        MapLumpType lumpType = MapLumpTypeForName(Str_Text(W_LumpName(i)));

        // If this lump is of an invalid type then we *should* have found all
        // the required map data lumps.
        if(lumpType == ML_INVALID) break; // Stop looking.

        // A recognized map data lump; record it in the collection.
        MapLumpInfo* info = new MapLumpInfo();
        info->init(i, lumpType, W_LumpLength(i));
        addMapLumpInfoToCollection(lumpInfos, *info);
    }
}

static MapFormatId recognizeMapFormat(MapLumpInfos& lumpInfos)
{
    LOG_AS("WadMapConverter");

    // Assume DOOM format by default.
    MapFormatId mapFormat = MF_DOOM;

    // Some data lumps are specific to a particular map format and thus
    // their presence unambiguously signifies which format we have.
    DENG2_FOR_EACH_CONST(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo* info = i->second;
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
    DENG2_FOR_EACH_CONST(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo* info = i->second;
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

static void loadAndTransferMap(MapFormatId mapFormat, MapLumpInfos& lumpInfos)
{
    DENG2_ASSERT(VALID_MAPFORMATID(mapFormat));

    // Load the archived map.
    map = new Id1Map(mapFormat);
    map->load(lumpInfos);

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
        delete map; map = 0;
    }
    MPE_End();
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
    MapLumpInfos lumpInfos;
    MapFormatId mapFormat;

    DENG2_UNUSED(hookType);
    DENG2_UNUSED(parm);
    DENG2_ASSERT(context);

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
    collectMapLumps(lumpInfos, markerLump + 1 /*begin after the marker*/);

    // Do we recognize this format?
    mapFormat = recognizeMapFormat(lumpInfos);
    if(!VALID_MAPFORMATID(mapFormat))
    {
        ret_val = false;
        goto FAIL_UNKNOWN_FORMAT;
    }

    // Convert this map.
    try
    {
        loadAndTransferMap(mapFormat, lumpInfos);
    }
    catch(Id1Map::LumpBufferError const& er)
    {
        LOG_AS("WadMapConverter");
        LOG_WARNING("Load error: %s\nAborting conversion...") << er.asText();
        ret_val = false;
    }

FAIL_UNKNOWN_FORMAT:

    // Cleanup.
    if(map)
    {
        // If the Id1Map instance still exists it can only mean an error occured
        // during map data load.
        delete map; map = 0;
    }

    DENG2_FOR_EACH(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo* info = i->second;
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

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" const char* deng_LibraryType(void)
{
    return "deng-plugin/generic";
}

DENG_DECLARE_API(Plug);
DENG_DECLARE_API(Uri);
DENG_DECLARE_API(W);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_PLUGIN_v1, Plug);
    DENG_GET_API(DE_API_URI_v1, Uri);
    DENG_GET_API(DE_API_WAD_v1, W);
)
