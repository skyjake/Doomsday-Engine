/** @file d64_api.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Doomsday API setup and interaction - Doom64 specific.
 */

#include <assert.h>
#include <string.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/gamefw/defs.h>
#include <de/extension.h>

#include "doomsday.h"

#include "jdoom64.h"

#include "d_netsv.h"
#include "d_net.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_update.h"
#include "hu_menu.h"
#include "p_mapsetup.h"
#include "r_common.h"
#include "p_map.h"
#include "polyobjs.h"

using namespace de;

// Identifiers given to the games we register during startup.
static const char *gameIds[NUM_GAME_MODES] =
{
    "doom64"
};

/**
 * Register the game modes supported by this plugin.
 */
static int G_RegisterGames(int hookType, int param, void* data)
{
    DE_UNUSED(hookType); DE_UNUSED(param); DE_UNUSED(data);

    Game &game = DoomsdayApp::games().defineGame(gameIds[doom64],
        Record::withMembers(Game::DEF_CONFIG_DIR, "doom64",
                            Game::DEF_TITLE, "Doom 64: Absolution",
                            Game::DEF_AUTHOR, "Kaiser et al.",
                            Game::DEF_FAMILY, "",
                            Game::DEF_TAGS, "doom64",
                            Game::DEF_RELEASE_DATE, "2003-12-31",
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom64.mapinfo"));
    //game.addResource(RC_PACKAGE, FF_STARTUP, "libdoom64.pk3", 0);
    //game.addResource(RC_PACKAGE, FF_STARTUP, "doom64.wad", "MAP01;MAP20;MAP33;F_SUCK");
    game.addResource(RC_DEFINITION, 0, PLUGIN_NAMETEXT ".ded", 0);
    game.setRequiredPackages(StringList()
                             << "kaiser.doom64"
                             << "net.dengine.legacy.doom64_2");
    // Options.
    {
        Record gameplayOptions;
        gameplayOptions.set("fast", Record::withMembers("label", "Fast Monsters/Missiles", "type", "boolean", "default", false));
        gameplayOptions.set("respawn", Record::withMembers("label", "Respawn Monsters", "type", "boolean", "default", false));
        gameplayOptions.set("noMonsters", Record::withMembers("label", "No Monsters", "type", "boolean", "default", false));
        gameplayOptions.set("turbo", Record::withMembers("label", "Move Speed", "type", "number", "default", 1.0, "min", 0.1, "max", 4.0, "step", 0.1));
        game.objectNamespace().set(Game::DEF_OPTIONS, gameplayOptions);
    }
    return true;
}

/**
 * Called right after the game plugin is selected into use.
 */
static void DP_Load(void)
{
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
    gfw_SetCurrentGame(GFW_DOOM64);
    Common_Load();
}

/**
 * Called when the game plugin is freed from memory.
 */
static void DP_Unload(void)
{
    Common_Unload();
    Plug_RemoveHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

static void G_PreInit(const char *gameId)
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
        Con_Error("Failed gamemode lookup for ID %s", gameId);
    }

    D64_PreInit();
}

/**
 * Called by the engine to initiate a soft-shutdown request.
 */
dd_bool G_TryShutdown(void)
{
    G_QuitGame();
    return true;
}

static void *GetGameAPI(const char *name)
{
    if (auto *ptr = Common_GetGameAPI(name))
    {
        return ptr;
    }

    #define HASH_ENTRY(Name, Func) std::make_pair(Name, de::function_cast<void *>(Func))
    static const Hash<String, void *> funcs(
    {
        HASH_ENTRY("DrawWindow",    D64_DrawWindow),
        HASH_ENTRY("EndFrame",      D64_EndFrame),
        HASH_ENTRY("GetInteger",    D64_GetInteger),
        HASH_ENTRY("GetPointer",    D64_GetVariable),
        HASH_ENTRY("PostInit",      D64_PostInit),
        HASH_ENTRY("PreInit",       G_PreInit),
        HASH_ENTRY("Shutdown",      D64_Shutdown),
        HASH_ENTRY("TryShutdown",   G_TryShutdown),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found->second;
    return nullptr;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
static void DP_Initialize()
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
static const char* deng_LibraryType(void)
{
    return "deng-plugin/game";
}

DE_DECLARE_API(Base);
DE_DECLARE_API(B);
DE_DECLARE_API(Busy);
DE_DECLARE_API(Client);
DE_DECLARE_API(Con);
DE_DECLARE_API(Def);
DE_DECLARE_API(F);
DE_DECLARE_API(FR);
DE_DECLARE_API(GL);
DE_DECLARE_API(Infine);
DE_DECLARE_API(InternalData);
DE_DECLARE_API(Material);
DE_DECLARE_API(MPE);
DE_DECLARE_API(Player);
DE_DECLARE_API(R);
DE_DECLARE_API(Rend);
DE_DECLARE_API(S);
DE_DECLARE_API(Server);
DE_DECLARE_API(Svg);
DE_DECLARE_API(Thinker);
DE_DECLARE_API(Uri);

DE_API_EXCHANGE(
    DE_GET_API(DE_API_BASE, Base);
    DE_GET_API(DE_API_BINDING, B);
    DE_GET_API(DE_API_BUSY, Busy);
    DE_GET_API(DE_API_CLIENT, Client);
    DE_GET_API(DE_API_CONSOLE, Con);
    DE_GET_API(DE_API_DEFINITIONS, Def);
    DE_GET_API(DE_API_FILE_SYSTEM, F);
    DE_GET_API(DE_API_FONT_RENDER, FR);
    DE_GET_API(DE_API_GL, GL);
    DE_GET_API(DE_API_INFINE, Infine);
    DE_GET_API(DE_API_INTERNAL_DATA, InternalData);
    DE_GET_API(DE_API_MATERIALS, Material);
    DE_GET_API(DE_API_MAP_EDIT, MPE);
    DE_GET_API(DE_API_PLAYER, Player);
    DE_GET_API(DE_API_RESOURCE, R);
    DE_GET_API(DE_API_RENDER, Rend);
    DE_GET_API(DE_API_SOUND, S);
    DE_GET_API(DE_API_SERVER, Server);
    DE_GET_API(DE_API_SVG, Svg);
    DE_GET_API(DE_API_THINKER, Thinker);
    DE_GET_API(DE_API_URI, Uri);
)

DE_ENTRYPOINT void *extension_doom64_symbol(const char *name)
{
    DE_SYMBOL_PTR(name, deng_LibraryType);
    DE_SYMBOL_PTR(name, deng_API);
    DE_SYMBOL_PTR(name, DP_Initialize);
    DE_SYMBOL_PTR(name, DP_Load);
    DE_SYMBOL_PTR(name, DP_Unload);
    DE_SYMBOL_PTR(name, GetGameAPI);
    warning("\"%s\" not found in doom64", name);
    return nullptr;
}
