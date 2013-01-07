/**\file d_api.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#define STARTUPPK3      PLUGIN_NAMETEXT2 ".pk3"

    GameDef const hacxDef = {
        "hacx", "hacx", "HACX - Twitch 'n Kill", "Banjo Software"
    };
    GameDef const chexDef = {
        "chex", "chex", "Chex(R) Quest", "Digital Cafe"
    };
    GameDef const doom2TntDef = {
        "doom2-tnt", "doom", "Final DOOM: TNT: Evilution", "Team TNT"
    };
    GameDef const doom2PlutDef = {
        "doom2-plut", "doom", "Final DOOM: The Plutonia Experiment", "Dario Casali and Milo Casali"
    };
    GameDef const doom2Def = {
        "doom2", "doom", "DOOM 2: Hell on Earth", "id Software"
    };
    GameDef const doomUltimateDef = {
        "doom1-ultimate", "doom", "Ultimate DOOM", "id Software"
    };
    GameDef const doomDef = {
        "doom1", "doom", "DOOM Registered", "id Software"
    };
    GameDef const doomShareDef = {
        "doom1-share", "doom", "DOOM Shareware", "id Software"
    };

    DENG_UNUSED(hookType); DENG_UNUSED(param); DENG_UNUSED(data);

    /* HacX */
    gameIds[doom2_hacx] = DD_DefineGame(&hacxDef);
    DD_AddGameResource(GID(doom2_hacx), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_hacx), RC_PACKAGE, FF_STARTUP, "hacx.wad", "HACX-R;PLAYPAL");
    DD_AddGameResource(GID(doom2_hacx), RC_DEFINITION, 0, "hacx.ded", 0);

    /* Chex Quest */
    gameIds[doom_chex] = DD_DefineGame(&chexDef);
    DD_AddGameResource(GID(doom_chex), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_chex), RC_PACKAGE, FF_STARTUP, "chex.wad", "E1M1;E4M1;_DEUTEX_;POSSH0M0");
    DD_AddGameResource(GID(doom_chex), RC_DEFINITION, 0, "chex.ded", 0);

    /* DOOM2 (TNT) */
    gameIds[doom2_tnt] = DD_DefineGame(&doom2TntDef);
    DD_AddGameResource(GID(doom2_tnt), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_tnt), RC_PACKAGE, FF_STARTUP, "tnt.wad", "CAVERN5;CAVERN7;STONEW1");
    DD_AddGameResource(GID(doom2_tnt), RC_DEFINITION, 0, "doom2-tnt.ded", 0);

    /* DOOM2 (Plutonia) */
    gameIds[doom2_plut] = DD_DefineGame(&doom2PlutDef);
    DD_AddGameResource(GID(doom2_plut), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2_plut), RC_PACKAGE, FF_STARTUP, "plutonia.wad", "_DEUTEX_;MAP01;MAP25;MC5;MC11;MC16;MC20");
    DD_AddGameResource(GID(doom2_plut), RC_DEFINITION, 0, "doom2-plut.ded", 0);

    /* DOOM2 */
    gameIds[doom2] = DD_DefineGame(&doom2Def);
    DD_AddGameResource(GID(doom2), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom2), RC_PACKAGE, FF_STARTUP, "doom2f.wad;doom2.wad", "MAP01;MAP02;MAP03;MAP04;MAP10;MAP20;MAP25;MAP30;VILEN1;VILEO1;VILEQ1;GRNROCK");
    DD_AddGameResource(GID(doom2), RC_DEFINITION, 0, "doom2.ded", 0);

    /* DOOM (Ultimate) */
    gameIds[doom_ultimate] = DD_DefineGame(&doomUltimateDef);
    DD_AddGameResource(GID(doom_ultimate), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_ultimate), RC_PACKAGE, FF_STARTUP, "doomu.wad;doom.wad", "E4M1;E4M2;E4M3;E4M4;E4M5;E4M6;E4M7;E4M8;E4M9;M_EPI4");
    DD_AddGameResource(GID(doom_ultimate), RC_DEFINITION, 0, "doom1-ultimate.ded", 0);

    /* DOOM */
    gameIds[doom] = DD_DefineGame(&doomDef);
    DD_AddGameResource(GID(doom), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom), RC_PACKAGE, FF_STARTUP, "doom.wad", "E2M1;E2M2;E2M3;E2M4;E2M5;E2M6;E2M7;E2M8;E2M9;E3M1;E3M2;E3M3;E3M4;E3M5;E3M6;E3M7;E3M8;E3M9;CYBRE1;CYBRD8;FLOOR7_2");
    DD_AddGameResource(GID(doom), RC_DEFINITION, 0, "doom1.ded", 0);

    /* DOOM (Shareware) */
    gameIds[doom_shareware] = DD_DefineGame(&doomShareDef);
    DD_AddGameResource(GID(doom_shareware), RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(doom_shareware), RC_PACKAGE, FF_STARTUP, "doom1.wad", "E1M1;E1M2;E1M3;E1M4;E1M5;E1M6;E1M7;E1M8;E1M9;D_E1M1;FLOOR4_8;FLOOR7_2");
    DD_AddGameResource(GID(doom_shareware), RC_DEFINITION, 0, "doom1-share.ded", 0);
    return true;

#undef STARTUPPK3
}

/**
 * Called right after the game plugin is selected into use.
 */
void DP_Load(void)
{
    // We might've been freed from memory, so refresh the game ids.
    gameIds[doom_shareware] = DD_GameIdForKey("doom1-share");
    gameIds[doom]           = DD_GameIdForKey("doom1");
    gameIds[doom_ultimate]  = DD_GameIdForKey("doom1-ultimate");
    gameIds[doom_chex]      = DD_GameIdForKey("chex");
    gameIds[doom2]          = DD_GameIdForKey("doom2");
    gameIds[doom2_tnt]      = DD_GameIdForKey("doom2-tnt");
    gameIds[doom2_plut]     = DD_GameIdForKey("doom2-plut");
    gameIds[doom2_hacx]     = DD_GameIdForKey("hacx");

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
game_export_t* GetGameAPI(void)
{
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
    gx.EndFrame = D_EndFrame;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (coord_t (*)(void *)) P_MobjGetFriction;
    gx.MobjCheckPositionXYZ = P_CheckPositionXYZ;
    gx.MobjTryMoveXYZ = P_TryMoveXYZ;
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
    gx.polyobjSize = sizeof(Polyobj);

    gx.SetupForMapData = P_SetupForMapData;

    // These really need better names. Ideas?
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;

    return &gx;
}

/**
 * This function is called automatically when the plugin is loaded for the first time.
 * We let the engine know what we'd like to do.
 */
void DP_Initialize()
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
DENG_DECLARE_API(InternalData);
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
    DENG_GET_API(DE_API_INTERNAL_DATA, InternalData);
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
