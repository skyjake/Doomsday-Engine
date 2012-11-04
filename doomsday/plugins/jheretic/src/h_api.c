/**\file h_api.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Doomsday API exchange - jHeretic specific.
 */

#include <assert.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "jheretic.h"

#include "d_netsv.h"
#include "d_net.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_menu.h"
#include "p_mapsetup.h"
#include "r_common.h"
#include "p_map.h"

#define GID(v)          (toGameId(v))

// The interface to the Doomsday engine.
game_import_t gi;
game_export_t gx;

// Identifiers given to the games we register during startup.
static gameid_t gameIds[NUM_GAME_MODES];

static __inline gameid_t toGameId(int gamemode)
{
    assert(gamemode >= 0 && gamemode < NUM_GAME_MODES);
    return gameIds[(gamemode_t) gamemode];
}

/**
 * Register the game modes supported by this plugin.
 */
int G_RegisterGames(int hookType, int param, void* data)
{
#define CONFIGDIR       "heretic"
#define STARTUPPK3      PLUGIN_NAMETEXT2 ".pk3"

    GameDef const hereticExtDef = {
        "heretic-ext", CONFIGDIR,
        "Heretic: Shadow of the Serpent Riders", "Raven Software"
    };
    GameDef const hereticDef = {
        "heretic", CONFIGDIR,
        "Heretic Registered", "Raven Software"
    };
    GameDef const hereticShareDef = {
        "heretic-share", CONFIGDIR,
        "Heretic Shareware", "Raven Software"
    };

    DENG_UNUSED(hookType); DENG_UNUSED(param); DENG_UNUSED(data);

    /* Heretic (Extended) */
    gameIds[heretic_extended] = DD_DefineGame(&hereticExtDef);
    DD_AddGameResource(GID(heretic_extended), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic_extended), RC_PACKAGE, RF_STARTUP, "heretic.wad", "EXTENDED;E5M2;E5M7;E6M2;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic_extended), RC_DEFINITION, 0, "heretic-ext.ded", 0);

    /* Heretic */
    gameIds[heretic] = DD_DefineGame(&hereticDef);
    DD_AddGameResource(GID(heretic), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic), RC_PACKAGE, RF_STARTUP, "heretic.wad", "E2M2;E3M6;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic), RC_DEFINITION, 0, "heretic.ded", 0);

    /* Heretic (Shareware) */
    gameIds[heretic_shareware] = DD_DefineGame(&hereticShareDef);
    DD_AddGameResource(GID(heretic_shareware), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic_shareware), RC_PACKAGE, RF_STARTUP, "heretic1.wad", "E1M1;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic_shareware), RC_DEFINITION, 0, "heretic-share.ded", 0);
    return true;

#undef STARTUPPK3
#undef CONFIGDIR
}

/**
 * Called right after the game plugin is selected into use.
 */
void DP_Load(void)
{
    // We might've been freed from memory, so refresh the game ids.
    gameIds[heretic_shareware] = DD_GameIdForKey("heretic-share");
    gameIds[heretic]           = DD_GameIdForKey("heretic");
    gameIds[heretic_extended]  = DD_GameIdForKey("heretic-ext");

    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

/**
 * Called when the game plugin is freed from memory.
 */
void DP_Unload(void)
{
    Plug_RemoveHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

void G_PreInit(gameid_t gameId)
{
    /// \todo Refactor me away.
    { size_t i;
    for(i = 0; i < NUM_GAME_MODES; ++i)
        if(gameIds[i] == gameId)
        {
            gameMode = (gamemode_t) i;
            gameModeBits = 1 << gameMode;
            break;
        }
    if(i == NUM_GAME_MODES)
        Con_Error("Failed gamemode lookup for id %i.", gameId);
    }

    H_PreInit();
}

/**
 * Called by the engine to initiate a soft-shutdown request.
 */
boolean G_TryShutdown(void)
{
    G_QuitGame();
    return true;
}

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t* GetGameAPI(game_import_t* imports)
{
    // Make sure this plugin isn't newer than Doomsday...
    if(imports->version < DOOMSDAY_VERSION)
        Con_Error(PLUGIN_NICENAME " requires at least " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "!\n");

    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = G_PreInit;
    gx.PostInit = H_PostInit;
    gx.TryShutdown = G_TryShutdown;
    gx.Shutdown = H_Shutdown;
    gx.Ticker = G_Ticker;
    gx.DrawViewPort = H_DrawViewPort;
    gx.DrawWindow = H_DrawWindow;
    gx.FinaleResponder = FI_PrivilegedResponder;
    gx.PrivilegedResponder = G_PrivilegedResponder;
    gx.Responder = G_Responder;
    gx.EndFrame = H_EndFrame;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (coord_t (*)(void *)) P_MobjGetFriction;
    gx.MobjCheckPositionXYZ = P_CheckPositionXYZ;
    gx.MobjTryMoveXYZ = P_TryMoveXYZ;
    gx.SectorHeightChangeNotification = P_HandleSectorHeightChange;
    gx.UpdateState = G_UpdateState;

    gx.GetInteger = H_GetInteger;
    gx.GetVariable = H_GetVariable;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // Data structure sizes.
    gx.mobjSize = sizeof(mobj_t);
    gx.polyobjSize = sizeof(Polyobj);

    gx.SetupForMapData = P_SetupForMapData;

    // These really need better names. Ideas?
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;

    return &gx;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize(void)
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
const char* deng_LibraryType(void)
{
    return "deng-plugin/game";
}
