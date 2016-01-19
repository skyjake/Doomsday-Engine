/** @file d_api.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/String>
#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>

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
#include "polyobjs.h"

using namespace de;

// The interface to the Doomsday engine.
game_export_t gx;

// Identifiers given to the games we register during startup.
static char const *gameIds[NUM_GAME_MODES] =
{
    "doom1-share",
    "doom1",
    "doom1-ultimate",
    "chex",
    "doom2",
    "doom2-plut",
    "doom2-tnt",
    "hacx",
    "doom2-freedm",
};

/**
 * Register the game modes supported by this plugin.
 */
int G_RegisterGames(int hookType, int param, void *data)
{
    Games &games = DoomsdayApp::games();

#define STARTUPPK3              "libdoom.pk3"
#define LEGACYSAVEGAMENAMEEXP   "^(?:DoomSav)[0-9]{1,1}(?:.dsg)"
#define LEGACYSAVEGAMESUBFOLDER "savegame"

    DENG_UNUSED(hookType); DENG_UNUSED(param); DENG_UNUSED(data);

    /* HacX */
    Game &hacx = games.defineGame(gameIds[doom2_hacx],
        Record::withMembers(Game::DEF_CONFIG_DIR, "hacx",
                            Game::DEF_TITLE, "HACX - Twitch 'n Kill",
                            Game::DEF_AUTHOR, "Banjo Software",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hacx.mapinfo"));
    hacx.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    hacx.addResource(RC_PACKAGE, FF_STARTUP, "hacx.wad", "HACX-R;PLAYPAL");
    hacx.addResource(RC_DEFINITION, 0, "hacx.ded", 0);

    /* Chex Quest */
    Game &chex = games.defineGame(gameIds[doom_chex],
        Record::withMembers(Game::DEF_CONFIG_DIR, "chex",
                            Game::DEF_TITLE, "Chex(R) Quest",
                            Game::DEF_AUTHOR, "Digital Cafe",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/chex.mapinfo"));
    chex.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    chex.addResource(RC_PACKAGE, FF_STARTUP, "chex.wad", "E1M1;E4M1;_DEUTEX_;POSSH0M0");
    chex.addResource(RC_DEFINITION, 0, "chex.ded", 0);

    /* DOOM2 (TNT) */
    Game &tnt = games.defineGame(gameIds[doom2_tnt],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "Final DOOM: TNT: Evilution",
                            Game::DEF_AUTHOR, "Team TNT",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-tnt.mapinfo"));
    tnt.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    tnt.addResource(RC_PACKAGE, FF_STARTUP, "tnt.wad", "CAVERN5;CAVERN7;STONEW1");
    tnt.addResource(RC_DEFINITION, 0, "doom2-tnt.ded", 0);

    /* DOOM2 (Plutonia) */
    Game &plut = games.defineGame(gameIds[doom2_plut],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "Final DOOM: The Plutonia Experiment",
                            Game::DEF_AUTHOR, "Dario Casali and Milo Casali",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-plut.mapinfo"));
    plut.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    plut.addResource(RC_PACKAGE, FF_STARTUP, "plutonia.wad", "_DEUTEX_;MAP01;MAP25;MC5;MC11;MC16;MC20");
    plut.addResource(RC_DEFINITION, 0, "doom2-plut.ded", 0);

    /* DOOM2 - FreeDM */
    Game &freedm = games.defineGame(gameIds[doom2_freedm],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "FreeDM",
                            Game::DEF_AUTHOR, "Freedoom Project",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-freedm.mapinfo"));
    freedm.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    freedm.addResource(RC_PACKAGE, FF_STARTUP, "freedm.wad", "MAP01");
    freedm.addResource(RC_DEFINITION, 0, "doom2-freedm.ded", 0);

    /* DOOM2 */
    Game &d2 = games.defineGame(gameIds[doom2],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "DOOM 2: Hell on Earth",
                            Game::DEF_AUTHOR, "id Software",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2.mapinfo"));
    d2.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    d2.addResource(RC_PACKAGE, FF_STARTUP, "doom2f.wad;doom2.wad", "MAP01;MAP02;MAP03;MAP04;MAP10;MAP20;MAP25;MAP30;VILEN1;VILEO1;VILEQ1;GRNROCK");
    d2.addResource(RC_DEFINITION, 0, "doom2.ded", 0);

    /* DOOM (Ultimate) */
    Game &ultimate = games.defineGame(gameIds[doom_ultimate],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "Ultimate DOOM",
                            Game::DEF_AUTHOR, "id Software",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-ultimate.mapinfo"));
    ultimate.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    ultimate.addResource(RC_PACKAGE, FF_STARTUP, "doomu.wad;doom.wad", "E4M1;E4M2;E4M3;E4M4;E4M5;E4M6;E4M7;E4M8;E4M9;M_EPI4");
    ultimate.addResource(RC_DEFINITION, 0, "doom1-ultimate.ded", 0);

    /* DOOM */
    Game &d1 = games.defineGame(gameIds[doom],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "DOOM Registered",
                            Game::DEF_AUTHOR, "id Software",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1.mapinfo"));
    d1.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    d1.addResource(RC_PACKAGE, FF_STARTUP, "doom.wad", "E2M1;E2M2;E2M3;E2M4;E2M5;E2M6;E2M7;E2M8;E2M9;E3M1;E3M2;E3M3;E3M4;E3M5;E3M6;E3M7;E3M8;E3M9;CYBRE1;CYBRD8;FLOOR7_2");
    d1.addResource(RC_DEFINITION, 0, "doom1.ded", 0);

    /* DOOM (Shareware) */
    Game &shareware = games.defineGame(gameIds[doom_shareware],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                            Game::DEF_TITLE, "DOOM Shareware",
                            Game::DEF_AUTHOR, "id Software",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-share.mapinfo"));
    shareware.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    shareware.addResource(RC_PACKAGE, FF_STARTUP, "doom1.wad", "E1M1;E1M2;E1M3;E1M4;E1M5;E1M6;E1M7;E1M8;E1M9;D_E1M1;FLOOR4_8;FLOOR7_2");
    shareware.addResource(RC_DEFINITION, 0, "doom1-share.ded", 0);
    return true;

#undef STARTUPPK3
}

/**
 * Called right after the game plugin is selected into use.
 */
DENG_EXTERN_C void DP_Load(void)
{
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

/**
 * Called when the game plugin is freed from memory.
 */
DENG_EXTERN_C void DP_Unload(void)
{
    Plug_RemoveHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

DENG_EXTERN_C void G_PreInit(char const *gameId)
{
    /// \todo Refactor me away.
    { size_t i;
    for(i = 0; i < NUM_GAME_MODES; ++i)
        if(!strcmp(gameIds[i], gameId))
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
dd_bool G_TryShutdown(void)
{
    G_QuitGame();
    return true;
}

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
DENG_EXTERN_C game_export_t *GetGameAPI(void)
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
    gx.DrawViewPort = G_DrawViewPort;
    gx.DrawWindow = D_DrawWindow;
    gx.FinaleResponder = FI_PrivilegedResponder;
    gx.PrivilegedResponder = G_PrivilegedResponder;
    gx.Responder = G_Responder;
    gx.EndFrame = D_EndFrame;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = Mobj_Friction;
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

    gx.FinalizeMapChange = (void (*)(void const *)) P_FinalizeMapChange;

    // These really need better names. Ideas?
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;

    return &gx;
}

/**
 * This function is called automatically when the plugin is loaded for the first time.
 * We let the engine know what we'd like to do.
 */
DENG_EXTERN_C void DP_Initialize()
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_EXTERN_C char const *deng_LibraryType(void)
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
DENG_DECLARE_API(R);
DENG_DECLARE_API(Rend);
DENG_DECLARE_API(S);
DENG_DECLARE_API(Server);
DENG_DECLARE_API(Svg);
DENG_DECLARE_API(Thinker);
DENG_DECLARE_API(Uri);

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
    DENG_GET_API(DE_API_RESOURCE, R);
    DENG_GET_API(DE_API_RENDER, Rend);
    DENG_GET_API(DE_API_SOUND, S);
    DENG_GET_API(DE_API_SERVER, Server);
    DENG_GET_API(DE_API_SVG, Svg);
    DENG_GET_API(DE_API_THINKER, Thinker);
    DENG_GET_API(DE_API_URI, Uri);
)
