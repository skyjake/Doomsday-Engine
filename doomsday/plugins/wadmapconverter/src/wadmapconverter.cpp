/** @file wadmapconverter.cpp  Map converter plugin for id tech 1 format maps.
 *
 * @ingroup wadmapconverter
 *
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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
#include "id1map_util.h"
#include <de/Error>
#include <de/Log>

using namespace de;
using namespace wadimp;

typedef QMap<MapLumpType, lumpnum_t> MapDataLumps;

/**
 * Given a map @a uri, attempt to locate the associated marker lump for the
 * map data using the Doomsday file system.
 *
 * @param uri  Map identifier, e.g., "Map:E1M1"
 *
 * @return Lump number for the found data else @c -1
 */
static lumpnum_t locateMapMarkerLumpForUri(Uri const &uri)
{
    char const *mapId = Str_Text(Uri_Path(&uri));
    return W_CheckLumpNumForName(mapId);
}

/**
 * Find all data lumps associated with this map and populate their metadata
 * in the @a lumpInfos record set.
 *
 * @param lumps      Record set to populate.
 * @param startLump  Lump number at which to begin searching.
 */
static void collectMapLumps(MapDataLumps &lumps, lumpnum_t startLump)
{
    LOG_AS("WadMapConverter");
    LOG_RES_XVERBOSE("Locating data lumps...");

    if(startLump < 0) return;

    // Keep checking lumps to see if its a map data lump.
    int const numLumps = *reinterpret_cast<int *>(DD_GetVariable(DD_NUMLUMPS));
    for(lumpnum_t i = startLump; i < numLumps; ++i)
    {
        // Lump name determines whether this lump should be included.
        MapLumpType lumpType = MapLumpTypeForName(Str_Text(W_LumpName(i)));

        // If this lump is of an invalid type then we *should* have found all
        // the required map data lumps.
        if(lumpType == ML_INVALID) break; // Stop looking.

        // A recognized map data lump; record it in the collection (replacing any
        // existing record of the same type).
        lumps.insert(lumpType, i);
    }
}

static Id1Map::Format recognizeMapFormat(MapDataLumps &lumps)
{
    LOG_AS("WadMapConverter");

    Id1Map::Format mapFormat = Id1Map::UnknownFormat;

    // Some data lumps are specific to a particular map format and thus their
    // presence unambiguously signifies which format we have.
    if(lumps.contains(ML_BEHAVIOR))
    {
        mapFormat = Id1Map::HexenFormat;
    }
    else if(lumps.contains(ML_MACROS) || lumps.contains(ML_LIGHTS) ||
            lumps.contains(ML_LEAFS))
    {
        mapFormat = Id1Map::Doom64Format;
    }
    else
    {
        mapFormat = Id1Map::DoomFormat;
    }

    // Determine whether each data lump is of the expected size.
    uint numVertexes = 0, numThings = 0, numLines = 0, numSides = 0, numSectors = 0, numLights = 0;
    DENG2_FOR_EACH_CONST(MapDataLumps, i, lumps)
    {
        MapLumpType type  = i.key();
        lumpnum_t lumpNum = i.value();

        // Determine the number of map data objects of each data type.
        uint *elemCountAddr = 0;
        size_t const elemSize = Id1Map::ElementSizeForMapLumpType(mapFormat, type);

        switch(type)
        {
        default: break;

        case ML_VERTEXES:   elemCountAddr = &numVertexes;    break;
        case ML_THINGS:     elemCountAddr = &numThings;      break;
        case ML_LINEDEFS:   elemCountAddr = &numLines;       break;
        case ML_SIDEDEFS:   elemCountAddr = &numSides;       break;
        case ML_SECTORS:    elemCountAddr = &numSectors;     break;
        case ML_LIGHTS:     elemCountAddr = &numLights;      break;
        }

        if(elemCountAddr)
        {
            size_t const lumpLength = W_LumpLength(lumpNum);

            if(lumpLength % elemSize != 0)
            {
                // What *is* this??
                return Id1Map::UnknownFormat;
            }

            *elemCountAddr += lumpLength / elemSize;
        }
    }

    // A valid map has at least one of each of these element types.
    if(!numVertexes || !numLines || !numSides || !numSectors)
    {
        return Id1Map::UnknownFormat;
    }

    LOG_RES_VERBOSE("Recognized %s format map") << Id1Map::formatName(mapFormat);
    return mapFormat;
}

static void loadAndTransferMap(Uri const &uri, Id1Map::Format mapFormat,
    MapDataLumps &lumps)
{
    QScopedPointer<Id1Map> map(new Id1Map(mapFormat));

    // Load the archived map.
    map->load(lumps);

    // Rebuild the map in Doomsday's native format.
    LOG_AS("WadMapConverter");
    map->transfer(uri);
}

/**
 * This function will be called when Doomsday is asked to load a map that is
 * not available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int /*hookType*/, int /*parm*/, void *context)
{
    DENG2_ASSERT(context != 0);

    // Attempt to locate the identified map data marker lump.
    Uri const &uri = *reinterpret_cast<Uri const *>(context);
    lumpnum_t markerLump = locateMapMarkerLumpForUri(uri);
    if(markerLump < 0)
    {
        return false;
    }

    // Collect all of the map data lumps associated with this map.
    MapDataLumps lumps;
    collectMapLumps(lumps, markerLump + 1 /*begin after the marker*/);

    // Is this a recognized format?
    Id1Map::Format mapFormat = recognizeMapFormat(lumps);
    if(mapFormat != Id1Map::UnknownFormat)
    {
        // Convert this map.
        try
        {
            loadAndTransferMap(uri, mapFormat, lumps);
            return true; // success
        }
        catch(Id1Map::LoadError const &er)
        {
            LOG_AS("WadMapConverter");
            LOG_MAP_ERROR("Load error: %s") << er.asText();
        }
    }

    return false; // failure :(
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
extern "C" void DP_Initialize()
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
extern "C" char const *deng_LibraryType()
{
    return "deng-plugin/generic";
}

DENG_DECLARE_API(Base);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(Map);
DENG_DECLARE_API(MPE);
DENG_DECLARE_API(Plug);
DENG_DECLARE_API(Uri);
DENG_DECLARE_API(W);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
    DENG_GET_API(DE_API_PLUGIN, Plug);
    DENG_GET_API(DE_API_URI, Uri);
    DENG_GET_API(DE_API_WAD, W);
)
