/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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
 * example.c: Example of Doomsday plugin which is called at startup.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "version.h"

// MACROS ------------------------------------------------------------------

#define DLLEXPORT __declspec( dllexport )

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DLLEXPORT game_export_t* GetGameAPI(game_import_t* imports);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The interface to the Doomsday engine.
game_import_t gi;
game_export_t gx;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void ExampleTicker(timespan_t ticLength)
{

}

void ExampleDrawer(int layer)
{

}

boolean ExampleResponder(event_t* ev)
{
    return false;
}

/**
 * Get a 32-bit integer value.
 */
int G_GetInteger(int id)
{
    switch(id)
    {
    case DD_GAME_DMUAPI_VER:
        return DMUAPI_VER;

    default:
        break;
    }
    return 0;
}

/**
 * Get a pointer to the value of a named variable/constant.
 */
void* G_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_GAME_NAME:
        return PLUGIN_NAMETEXT;

    case DD_GAME_NICENAME:
        return PLUGIN_NICENAME;

    case DD_GAME_ID:
        return PLUGIN_NAMETEXT " " PLUGIN_VERSION_TEXT;

    case DD_GAME_VERSION_SHORT:
        return PLUGIN_VERSION_TEXT;

    case DD_GAME_VERSION_LONG:
        return PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS;

    default:
        break;
    }
    return 0;
}

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t* GetGameAPI(game_import_t* imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.Ticker = ExampleTicker;
    gx.G_Drawer = ExampleDrawer;
    //gx.G_Drawer2 = D_Display2;
    //gx.PrivilegedResponder = (boolean (*)(event_t*)) G_PrivilegedResponder;
    gx.G_Responder = ExampleResponder;
    gx.GetInteger = G_GetInteger;
    gx.GetVariable = G_GetVariable;

#if 0
    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;
    gx.NetWriteCommands = D_NetWriteCommands;
    gx.NetReadCommands = D_NetReadCommands;
#endif

    // Data structure sizes.
    /*gx.ticcmdSize = sizeof(ticcmd_t);
    gx.mobjSize = sizeof(mobj_t);
    gx.polyobjSize = sizeof(polyobj_t);

    gx.SetupForMapData = P_SetupForMapData;

    // These really need better names. Ideas?
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;*/

    return &gx;
}

/**
 * This function will be called ASAP after Doomsday has completed startup.
 *
 * @param parm
 * @param data
 *
 * @return              Non-zero if successful.
 */
int ExampleHook(int hookType, int parm, void *data)
{
    DD_AddGame("examplegame", DD_BASEPATH_DATA PLUGIN_NAMETEXT "\\", DD_BASEPATH_DEFS PLUGIN_NAMETEXT "\\", 0, "Example Game", "deng team", "examplegame", 0);
    return true;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_STARTUP, ExampleHook);
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
