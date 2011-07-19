/**\file
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
 * wadmapconverter.c: Doomsday Plugin for converting DOOM-like format maps.
 *
 * The purpose of a wadmapconverter plugin is to transform a map into
 * Doomsday's native map format by use of the public map editing interface.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int ConvertMapHook(int hookType, int parm, void *data);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

map_t theMap, *map = &theMap;
boolean verbose;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_MAP_CONVERT, ConvertMapHook);
}

#ifdef WIN32
/**
 * Windows calls this when the DLL is loaded.
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Register our hooks.
        DP_Initialize();
        break;

    default:
        break;
    }

    return TRUE;
}
#endif

/**
 * This function is called when Doomsday is asked to load a map that is not
 * presently available in its native map format.
 *
 * Our job is to read in the map data structures then use the Doomsday map
 * editing interface to recreate the map in native format.
 */
int ConvertMapHook(int hookType, int param, void *data)
{
    int                *lumpList = (int*) data;

    verbose = ArgExists("-verbose");

    Con_Message("WadMapConverter::Convert: Attempting map conversion...\n");
    memset(map, 0, sizeof(*map));

    if(!IsSupportedFormat(lumpList, param))
    {
        Con_Message("WadMapConverter::Convert: Unknown map format, aborting.\n");
        return false; // Cannot convert.
    }

    // A supported format.
    Con_Message("WadMapConverter::Convert: %s map format.\n",
                (map->format == MF_DOOM64? "DOOM64" :
                 map->format == MF_HEXEN? "Hexen" : "DOOM"));

    // Load it in.
    if(!LoadMap(lumpList, param))
    {
        Con_Message("WadMapConverter::Convert: Internal error, load failed.\n");
        return false; // Something went horribly wrong...
    }

    AnalyzeMap();
    return TransferMap();
}
