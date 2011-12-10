/**\file d_api.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Doomsday API setup and interaction - jDoom specific
 */

#include <assert.h>
#include <string.h>

#include "doomsday.h"
#include "dd_api.h"

#include "jdoom.h"

#include "d_netsv.h"
#include "d_net.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_menu.h"
#include "p_map.h"
#include "p_mapsetup.h"
#include "r_common.h"
#include "p_map.h"
#include "p_tick.h"

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
#define DATAPATH        DD_BASEPATH_DATA PLUGIN_NAMETEXT "/"
#define DEFSPATH        DD_BASEPATH_DEFS PLUGIN_NAMETEXT "/"
#define MAINCONFIG      PLUGIN_NAMETEXT ".cfg"
#define STARTUPPK3      PLUGIN_NAMETEXT ".pk3"

    /* HacX */
    gameIds[doom2_hacx] = DD_AddGame("hacx", DATAPATH, DEFSPATH, MAINCONFIG, "HACX - Twitch 'n Kill", "Banjo Software", "hacx", 0);
    DD_AddGameResource(GID(doom2_hacx), RC_PACKAGE, RF_STARTUP, "hacx.wad", "HACX-R");
    DD_AddGameResource(GID(doom2_hacx), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_hacx), RC_DEFINITION, 0, "hacx.ded", 0);

    /* Chex Quest */
    gameIds[doom_chex] = DD_AddGame("chex", DATAPATH, DEFSPATH, MAINCONFIG, "Chex(R) Quest", "Digital Cafe", "chex", 0);
    DD_AddGameResource(GID(doom_chex), RC_PACKAGE, RF_STARTUP, "chex.wad", "E1M1;E4M1;_DEUTEX_;POSSH0M0");
    DD_AddGameResource(GID(doom_chex), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_chex), RC_DEFINITION, 0, "chex.ded", 0);

    /* DOOM2 (TNT) */
    gameIds[doom2_tnt] = DD_AddGame("doom2-tnt", DATAPATH, DEFSPATH, MAINCONFIG, "Final DOOM: TNT: Evilution", "Team TNT", "tnt", 0);
    DD_AddGameResource(GID(doom2_tnt), RC_PACKAGE, RF_STARTUP, "tnt.wad", "CAVERN5;CAVERN7;STONEW1");
    DD_AddGameResource(GID(doom2_tnt), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_tnt), RC_DEFINITION, 0, "doom2-tnt.ded", 0);

    /* DOOM2 (Plutonia) */
    gameIds[doom2_plut] = DD_AddGame("doom2-plut", DATAPATH, DEFSPATH, MAINCONFIG, "Final DOOM: The Plutonia Experiment", "Dario Casali and Milo Casali", "plutonia", "plut");
    DD_AddGameResource(GID(doom2_plut), RC_PACKAGE, RF_STARTUP, "plutonia.wad", "_DEUTEX_;MAP01;MAP25;MC5;MC11;MC16;MC20");
    DD_AddGameResource(GID(doom2_plut), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_plut), RC_DEFINITION, 0, "doom2-plut.ded", 0);

    /* DOOM2 */
    gameIds[doom2] = DD_AddGame("doom2", DATAPATH, DEFSPATH, MAINCONFIG, "DOOM 2: Hell on Earth", "id Software", "doom2", 0);
    DD_AddGameResource(GID(doom2), RC_PACKAGE, RF_STARTUP, "doom2f.wad;doom2.wad", "MAP01;MAP02;MAP03;MAP04;MAP10;MAP20;MAP25;MAP30;VILEN1;VILEO1;VILEQ1;GRNROCK");
    DD_AddGameResource(GID(doom2), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2), RC_DEFINITION, 0, "doom2.ded", 0);

    /* DOOM (Ultimate) */
    gameIds[doom_ultimate] = DD_AddGame("doom1-ultimate", DATAPATH, DEFSPATH, MAINCONFIG, "Ultimate DOOM", "id Software", "ultimatedoom", "udoom");
    DD_AddGameResource(GID(doom_ultimate), RC_PACKAGE, RF_STARTUP, "doomu.wad;doom.wad", "E4M1;E4M2;E4M3;E4M4;E4M5;E4M6;E4M7;E4M8;E4M9;M_EPI4");
    DD_AddGameResource(GID(doom_ultimate), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_ultimate), RC_DEFINITION, 0, "doom1-ultimate.ded", 0);

    /* DOOM */
    gameIds[doom] = DD_AddGame("doom1", DATAPATH, DEFSPATH, MAINCONFIG, "DOOM Registered", "id Software", "doom", 0);
    DD_AddGameResource(GID(doom), RC_PACKAGE, RF_STARTUP, "doom.wad", "E2M1;E2M2;E2M3;E2M4;E2M5;E2M6;E2M7;E2M8;E2M9;E3M1;E3M2;E3M3;E3M4;E3M5;E3M6;E3M7;E3M8;E3M9;CYBRE1;CYBRD8;FLOOR7_2");
    DD_AddGameResource(GID(doom), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom), RC_DEFINITION, 0, "doom1.ded", 0);

    /* DOOM (Shareware) */
    gameIds[doom_shareware] = DD_AddGame("doom1-share", DATAPATH, DEFSPATH, MAINCONFIG, "DOOM Shareware", "id Software", "sdoom", 0);
    DD_AddGameResource(GID(doom_shareware), RC_PACKAGE, RF_STARTUP, "doom1.wad", "E1M1;E1M2;E1M3;E1M4;E1M5;E1M6;E1M7;E1M8;E1M9;D_E1M1;FLOOR4_8;FLOOR7_2");
    DD_AddGameResource(GID(doom_shareware), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_shareware), RC_DEFINITION, 0, "doom1-share.ded", 0);
    return true;

#undef STARTUPPK3
#undef MAINCONFIG
#undef DEFSPATH
#undef DATAPATH
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

    D_PreInit();
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
    gx.PostInit = D_PostInit;
    gx.Shutdown = D_Shutdown;
    gx.TryShutdown = G_TryShutdown;
    gx.Ticker = G_Ticker;
    gx.DrawViewPort = D_DrawViewPort;
    gx.DrawWindow = D_DrawWindow;
    gx.FinaleResponder = FI_PrivilegedResponder;
    gx.PrivilegedResponder = G_PrivilegedResponder;
    gx.Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (float (*)(void *)) P_MobjGetFriction;
    gx.MobjCheckPosition3f = P_CheckPosition3f;
    gx.MobjTryMove3f = P_TryMove3f;
    gx.SectorHeightChangeNotification = P_HandleSectorHeightChange;
    gx.UpdateState = G_UpdateState;
    gx.GetInteger = D_GetInteger;
    gx.GetVariable = D_GetVariable;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;

    // Data structure sizes.
    gx.mobjSize = sizeof(mobj_t);
    gx.polyobjSize = sizeof(polyobj_t);

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
void DP_Initialize()
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}
