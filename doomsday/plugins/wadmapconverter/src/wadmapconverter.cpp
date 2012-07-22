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
#include <stdio.h>
#include <string.h>

#include "doomsday.h"
#include <de/c_wrapper.h>
#include "map.h"

map_t DENG_PLUGIN_GLOBAL(theMap);
map_t* DENG_PLUGIN_GLOBAL(map) = &DENG_PLUGIN_GLOBAL(theMap);
int DENG_PLUGIN_GLOBAL(verbose);

/**
 * This function will be called when Doomsday is asked to load a map that is
 * not available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int param, void* context)
{
    DENG_UNUSED(hookType);

    DENG_PLUGIN_GLOBAL(verbose) = CommandLine_Exists("-verbose");

    memset(DENG_PLUGIN_GLOBAL(map), 0, sizeof(*DENG_PLUGIN_GLOBAL(map)));

    lumpnum_t* lumpList = reinterpret_cast<int*>(context);
    if(!IsSupportedFormat(lumpList, param))
    {
        Con_Message("WadMapConverter: Unknown map format, aborting.\n");
        return false;
    }

    if(!LoadMap(lumpList, param))
    {
        Con_Message("WadMapConverter: Internal error, load failed.\n");
        return false;
    }

    AnalyzeMap();
    return TransferMap();
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}
