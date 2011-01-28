/**\file wadmapconverter.c
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
 * Doomsday Plugin for converting DOOM-like format maps.
 *
 * The purpose of a wadmapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

map_t theMap, *map = &theMap;
boolean verbose;

/**
 * This function will be called when Doomsday is asked to load a map that is
 * not available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int param, void *data)
{
    int* lumpList = (int*) data;

    verbose = ArgExists("-verbose");

    VERBOSE2( Con_Message("Attempting map conversion using WadMapConverter ...\n") );
    memset(map, 0, sizeof(*map));

    if(!IsSupportedFormat(lumpList, param))
    {
        Con_Message("WadMapConverter: Unknown map format, aborting.\n");
        return false; // Cannot convert.
    }

    // A supported format.
    VERBOSE( Con_Message("WadMapConverter: Recognised a %s format map.\n",
                         (map->format == MF_DOOM64? "DOOM64" :
                          map->format == MF_HEXEN?  "Hexen"  : "DOOM") ));

    // Load it in.
    if(!LoadMap(lumpList, param))
    {
        Con_Message("WadMapConverter: Internal error, load failed.\n");
        return false; // Something went horribly wrong...
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
