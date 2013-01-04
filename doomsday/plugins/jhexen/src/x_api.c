/**\file x_api.c
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
 * Doomsday API exchange - jHexen specific
 */

#include <assert.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "jhexen.h"

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
game_export_t gx;
game_import_t gi;

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
#define CONFIGDIR       "hexen"
#define STARTUPPK3      PLUGIN_NAMETEXT2 ".pk3"

    GameDef const deathkingsDef = {
        "hexen-dk", CONFIGDIR,
        "Hexen: Deathkings of the Dark Citadel", "Raven Software"
    };
    GameDef const hexenDef = {
        "hexen", CONFIGDIR, "Hexen", "Raven Software"
    };
    GameDef const hexenDemoDef = {
        "hexen-demo", CONFIGDIR, "Hexen 4-map Demo", "Raven Software"
    };
    GameDef const hexenBetaDemoDef = {
        "hexen-betademo", CONFIGDIR, "Hexen 4-map Beta Demo", "Raven Software"
    };
    GameDef const hexenV10Def = {
        "hexen-v10", CONFIGDIR, "Hexen v1.0", "Raven Software"
    };

    DENG_UNUSED(hookType); DENG_UNUSED(param); DENG_UNUSED(data);

    /* Hexen (Death Kings) */
    gameIds[hexen_deathkings] = DD_DefineGame(&deathkingsDef);
    DD_AddGameResource(GID(hexen_deathkings), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(hexen_deathkings), RC_PACKAGE, FF_STARTUP, "hexdd.wad", "MAP59;MAP60");
    DD_AddGameResource(GID(hexen_deathkings), RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    DD_AddGameResource(GID(hexen_deathkings), RC_DEFINITION, 0, "hexen-dk.ded", 0);

    /* Hexen */
    gameIds[hexen] = DD_DefineGame(&hexenDef);
    DD_AddGameResource(GID(hexen), RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    DD_AddGameResource(GID(hexen), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(hexen), RC_DEFINITION, 0, "hexen.ded", 0);

    /* Hexen (v1.0) */
    gameIds[hexen_v10] = DD_DefineGame(&hexenV10Def);
    DD_AddGameResource(GID(hexen_v10), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(hexen_v10), RC_PACKAGE, FF_STARTUP, "hexen.wad", "MAP08;MAP22;MAP41;TINTTAB;FOGMAP;DARTA1;ARTIPORK;SKYFOG;GROVER");
    DD_AddGameResource(GID(hexen_v10), RC_DEFINITION, 0, "hexen-v10.ded", 0);

    /* Hexen (Demo) */
    gameIds[hexen_demo] = DD_DefineGame(&hexenDemoDef);
    DD_AddGameResource(GID(hexen_demo), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(hexen_demo), RC_PACKAGE, FF_STARTUP, "hexendemo.wad;machexendemo.wad;hexen.wad", "MAP01;MAP04;TINTTAB;FOGMAP;DARTA1;ARTIPORK;DEMO3==18150");
    DD_AddGameResource(GID(hexen_demo), RC_DEFINITION, 0, "hexen-demo.ded", 0);

    /* Hexen (Beta Demo) */
    gameIds[hexen_betademo] = DD_DefineGame(&hexenBetaDemoDef);
    DD_AddGameResource(GID(hexen_betademo), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(hexen_betademo), RC_PACKAGE, FF_STARTUP, "hexendemo.wad;machexendemo.wad;hexenbeta.wad;hexen.wad", "MAP01;MAP04;TINTTAB;FOGMAP;DARTA1;ARTIPORK;AFLYA0;DEMO3==13866");
    DD_AddGameResource(GID(hexen_betademo), RC_DEFINITION, 0, "hexen-demo.ded", 0);

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
    gameIds[hexen_deathkings] = DD_GameIdForKey("hexen-dk");
    gameIds[hexen]            = DD_GameIdForKey("hexen");
    gameIds[hexen_v10]        = DD_GameIdForKey("hexen-v10");
    gameIds[hexen_demo]       = DD_GameIdForKey("hexen-demo");
    gameIds[hexen_betademo]   = DD_GameIdForKey("hexen-betademo");

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

    X_PreInit();
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
    gx.PostInit = X_PostInit;
    gx.TryShutdown = G_TryShutdown;
    gx.Shutdown = X_Shutdown;
    gx.Ticker = G_Ticker;
    gx.DrawViewPort = X_DrawViewPort;
    gx.DrawWindow = X_DrawWindow;
    gx.FinaleResponder = FI_PrivilegedResponder;
    gx.PrivilegedResponder = G_PrivilegedResponder;
    gx.Responder = G_Responder;
    gx.EndFrame = X_EndFrame;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (coord_t (*)(void *)) P_MobjGetFriction;
    gx.MobjCheckPositionXYZ = P_CheckPositionXYZ;
    gx.MobjTryMoveXYZ = P_TryMoveXYZ;
    gx.SectorHeightChangeNotification = P_HandleSectorHeightChange;
    gx.UpdateState = G_UpdateState;
    gx.GetInteger = X_GetInteger;
    gx.GetVariable = X_GetVariable;

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

DENG_DECLARE_API(Base);
DENG_DECLARE_API(B);
DENG_DECLARE_API(Busy);
DENG_DECLARE_API(Client);
DENG_DECLARE_API(Con);
DENG_DECLARE_API(Def);
DENG_DECLARE_API(F);
DENG_DECLARE_API(FR);
DENG_DECLARE_API(GL);
DENG_DECLARE_API(Infine);
DENG_DECLARE_API(Material);
DENG_DECLARE_API(MaterialArchive);
DENG_DECLARE_API(Map);
DENG_DECLARE_API(MPE);
DENG_DECLARE_API(Player);
DENG_DECLARE_API(Plug);
DENG_DECLARE_API(R);
DENG_DECLARE_API(Rend);
DENG_DECLARE_API(S);
DENG_DECLARE_API(Server);
DENG_DECLARE_API(Svg);
DENG_DECLARE_API(Thinker);
DENG_DECLARE_API(Uri);
DENG_DECLARE_API(W);

DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_BASE, Base);
    DENG_GET_API(DE_API_BINDING, B);
    DENG_GET_API(DE_API_BUSY, Busy);
    DENG_GET_API(DE_API_CLIENT, Client);
    DENG_GET_API(DE_API_CONSOLE, Con);
    DENG_GET_API(DE_API_DEFINITIONS, Def);
    DENG_GET_API(DE_API_FILE_SYSTEM, F);
    DENG_GET_API(DE_API_FONT_RENDER, FR);
    DENG_GET_API(DE_API_GL, GL);
    DENG_GET_API(DE_API_INFINE, Infine);
    DENG_GET_API(DE_API_MATERIALS, Material);
    DENG_GET_API(DE_API_MATERIAL_ARCHIVE, MaterialArchive);
    DENG_GET_API(DE_API_MAP, Map);
    DENG_GET_API(DE_API_MAP_EDIT, MPE);
    DENG_GET_API(DE_API_PLAYER, Player);
    DENG_GET_API(DE_API_PLUGIN, Plug);
    DENG_GET_API(DE_API_RESOURCE, R);
    DENG_GET_API(DE_API_RENDER, Rend);
    DENG_GET_API(DE_API_SOUND, S);
    DENG_GET_API(DE_API_SERVER, Server);
    DENG_GET_API(DE_API_SVG, Svg);
    DENG_GET_API(DE_API_THINKER, Thinker);
    DENG_GET_API(DE_API_URI, Uri);
    DENG_GET_API(DE_API_WAD, W);
)

