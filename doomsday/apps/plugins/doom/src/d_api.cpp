/** @file d_api.cpp
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
 * Doomsday API setup and interaction - jDoom specific
 */

#include <assert.h>
#include <string.h>

#include <de/String>
#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <gamefw/libgamefw.h>

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
    "doom2-freedoom",
    "doom1-freedoom",
    "doom1-bfg",
    "doom2-bfg",
    "doom2-nerve",
};

#define STARTUPPK3              "libdoom.pk3"
#define LEGACYSAVEGAMENAMEEXP   "^(?:DoomSav)[0-9]{1,1}(?:.dsg)"
#define LEGACYSAVEGAMESUBFOLDER "savegame"

static void setCommonParameters(Game &game)
{
    game.addRequiredPackage("net.dengine.legacy.doom_2");

    Record gameplayOptions;
    gameplayOptions.set("fast", Record::withMembers("label", "Fast Monsters/Missiles", "type", "boolean", "default", false));
    gameplayOptions.set("respawn", Record::withMembers("label", "Respawn Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("noMonsters", Record::withMembers("label", "No Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("turbo", Record::withMembers("label", "Move Speed", "type", "number", "default", 1.0, "min", 0.1, "max", 4.0, "step", 0.1));
    game.objectNamespace().set(Game::DEF_OPTIONS, gameplayOptions);
}

/**
 * Register the game modes supported by this plugin.
 */
static int G_RegisterGames(int hookType, int param, void *data)
{
    DENG2_UNUSED3(hookType, param, data);

    Games &games = DoomsdayApp::games();

    /* HacX */
    {
        Game &hacx = games.defineGame(gameIds[doom2_hacx],
            Record::withMembers(Game::DEF_CONFIG_DIR, "hacx",
                                Game::DEF_TITLE, "HACX - Twitch 'n Kill",
                                Game::DEF_AUTHOR, "Banjo Software",
                                Game::DEF_RELEASE_DATE, "1997-09-01",
                                Game::DEF_FAMILY, "",
                                Game::DEF_TAGS, "hacx",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/hacx.mapinfo"));
        hacx.addRequiredPackage("banjo.hacx");
        setCommonParameters(hacx);
        hacx.addResource(RC_DEFINITION, 0, "hacx.ded", 0);
    }

    /* Chex Quest */
    {
        Game &chex = games.defineGame(gameIds[doom_chex],
            Record::withMembers(Game::DEF_CONFIG_DIR, "chex",
                                Game::DEF_TITLE, "Chex(R) Quest",
                                Game::DEF_AUTHOR, "Digital Cafe",
                                Game::DEF_RELEASE_DATE, "1996-01-01",
                                Game::DEF_FAMILY, "",
                                Game::DEF_TAGS, "chex chexquest",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/chex.mapinfo"));
        chex.addRequiredPackage("digitalcafe.chexquest");
        setCommonParameters(chex);
        chex.addResource(RC_DEFINITION, 0, "chex.ded", 0);
    }

    /* DOOM2 (TNT) */
    {
        Game &tnt = games.defineGame(gameIds[doom2_tnt],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "Final DOOM: TNT: Evilution",
                                Game::DEF_AUTHOR, "Team TNT",
                                Game::DEF_RELEASE_DATE, "1996-06-17",
                                Game::DEF_TAGS, "finaldoom tnt evilution",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-tnt.mapinfo"));
        tnt.addRequiredPackage("com.idsoftware.finaldoom.tnt");
        setCommonParameters(tnt);
        tnt.addResource(RC_DEFINITION, 0, "doom2-tnt.ded", 0);
    }

    /* DOOM2 (Plutonia) */
    {
        Game &plut = games.defineGame(gameIds[doom2_plut],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "Final DOOM: The Plutonia Experiment",
                                Game::DEF_AUTHOR, "Dario Casali and Milo Casali",
                                Game::DEF_RELEASE_DATE, "1996-06-17",
                                Game::DEF_TAGS, "finaldoom plutonia",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-plut.mapinfo"));
        plut.addRequiredPackage("com.idsoftware.finaldoom.plutonia");
        setCommonParameters(plut);
        plut.addResource(RC_DEFINITION, 0, "doom2-plut.ded", 0);
    }

    /* DOOM2 - Freedoom Phase 2 */
    {
        Game &freedoom2 = games.defineGame(gameIds[doom2_freedoom],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "Freedoom: Phase 2",
                                Game::DEF_AUTHOR, "Freedoom Project",
                                Game::DEF_RELEASE_DATE, "2009-06-18",
                                Game::DEF_FAMILY, "",
                                Game::DEF_TAGS, "freedoom",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-freedoom.mapinfo"));
        freedoom2.addRequiredPackage("freedoom.phase2");
        setCommonParameters(freedoom2);
        freedoom2.addResource(RC_DEFINITION, 0, "doom2-freedoom.ded", 0);
    }

    /* DOOM2 - FreeDM */
    {
        Game &freedm = games.defineGame(gameIds[doom2_freedm],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "FreeDM",
                                Game::DEF_AUTHOR, "Freedoom Project",
                                Game::DEF_RELEASE_DATE, "2015-12-23",
                                Game::DEF_FAMILY, "",
                                Game::DEF_TAGS, "freedoom multiplayer",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-freedm.mapinfo"));
        freedm.addRequiredPackage("freedoom.freedm");
        setCommonParameters(freedm);
        freedm.addResource(RC_DEFINITION, 0, "doom2-freedm.ded", 0);
    }

    /* DOOM II */
    {
        Game &d2 = games.defineGame(gameIds[doom2],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "DOOM 2: Hell on Earth",
                                Game::DEF_AUTHOR, "id Software",
                                Game::DEF_RELEASE_DATE, "1994-09-30",
                                Game::DEF_TAGS, "doom2",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2.mapinfo"));
        d2.addRequiredPackage("com.idsoftware.doom2");
        setCommonParameters(d2);
        d2.addResource(RC_DEFINITION, 0, "doom2.ded", 0);
    }

    /* DOOM II (BFG Edition) */
    {
        Game &d2Bfg = games.defineGame(gameIds[doom2_bfg],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "DOOM 2: Hell on Earth (BFG Edition)",
                                Game::DEF_AUTHOR, "id Software",
                                Game::DEF_RELEASE_DATE, "2012-10-16",
                                Game::DEF_TAGS, "doom2 bfg",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-bfg.mapinfo"));
        d2Bfg.addRequiredPackage("com.idsoftware.doom2.bfg");
        setCommonParameters(d2Bfg);
        d2Bfg.addResource(RC_DEFINITION, 0, "doom2.ded", 0);
    }

    /* No Rest for the Living (BFG Edition) */
    {
        Game &nerve = games.defineGame(gameIds[doom2_nerve],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "No Rest for the Living",
                                Game::DEF_AUTHOR, "Nerve Software",
                                Game::DEF_RELEASE_DATE, "2012-10-16",
                                Game::DEF_TAGS, "doom2 bfg expansion",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom2-nerve.mapinfo"));
        nerve.addRequiredPackage("com.idsoftware.doom2.bfg");
        nerve.addRequiredPackage("com.nervesoftware.norestfortheliving");
        setCommonParameters(nerve);
        nerve.addResource(RC_DEFINITION, 0, "doom2.ded", 0);
    }

    /* Ultimate DOOM */
    {
        Game &ultimate = games.defineGame(gameIds[doom_ultimate],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "Ultimate DOOM",
                                Game::DEF_AUTHOR, "id Software",
                                Game::DEF_RELEASE_DATE, "1995-04-30",
                                Game::DEF_TAGS, "doom",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-ultimate.mapinfo"));
        ultimate.addRequiredPackage("com.idsoftware.doom.ultimate");
        setCommonParameters(ultimate);
        ultimate.addResource(RC_DEFINITION, 0, "doom1-ultimate.ded", 0);
    }

    /* Ultimate DOOM (BFG Edition) */
    {
        Game &doomBfg = games.defineGame(gameIds[doom_bfg],
             Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                 Game::DEF_TITLE, "Ultimate DOOM (BFG Edition)",
                                 Game::DEF_AUTHOR, "id Software",
                                 Game::DEF_RELEASE_DATE, "2012-10-16",
                                 Game::DEF_TAGS, "doom bfg",
                                 Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                 Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                 Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-ultimate.mapinfo"));
        doomBfg.addRequiredPackage("com.idsoftware.doom.bfg");
        setCommonParameters(doomBfg);
        doomBfg.addResource(RC_DEFINITION, 0, "doom1-ultimate.ded", 0);
    }

    /* DOOM */
    {
        Game &d1 = games.defineGame(gameIds[doom],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "DOOM Registered",
                                Game::DEF_AUTHOR, "id Software",
                                Game::DEF_RELEASE_DATE, "1993-12-10",
                                Game::DEF_TAGS, "doom",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1.mapinfo"));
        d1.addRequiredPackage("com.idsoftware.doom");
        setCommonParameters(d1);
        d1.addResource(RC_DEFINITION, 0, "doom1.ded", 0);
    }

    /* DOOM (Shareware) */
    {
        Game &shareware = games.defineGame(gameIds[doom_shareware],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "DOOM Shareware",
                                Game::DEF_AUTHOR, "id Software",
                                Game::DEF_RELEASE_DATE, "1993-12-10",
                                Game::DEF_TAGS, "doom shareware",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-share.mapinfo"));
        shareware.addRequiredPackage("com.idsoftware.doom.shareware");
        setCommonParameters(shareware);
        shareware.addResource(RC_DEFINITION, 0, "doom1-share.ded", 0);
    }

    /* DOOM - Freedoom Phase 1 */
    {
        Game &freedoom1 = games.defineGame(gameIds[doom_freedoom],
            Record::withMembers(Game::DEF_CONFIG_DIR, "doom",
                                Game::DEF_TITLE, "Freedoom: Phase 1",
                                Game::DEF_AUTHOR, "Freedoom Project",
                                Game::DEF_RELEASE_DATE, "2009-06-18",
                                Game::DEF_FAMILY, "",
                                Game::DEF_TAGS, "freedoom",
                                Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                                Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                                Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/doom1-ultimate.mapinfo"));
        freedoom1.addRequiredPackage("freedoom.phase1");
        setCommonParameters(freedoom1);
        freedoom1.addResource(RC_DEFINITION, 0, "doom1-freedoom.ded", 0);
    }
    return true;

#undef STARTUPPK3
}

/**
 * Called right after the game plugin is selected into use.
 */
DENG_ENTRYPOINT void DP_Load(void)
{
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
    gfw_SetCurrentGame(GFW_DOOM);
}

/**
 * Called when the game plugin is freed from memory.
 */
DENG_ENTRYPOINT void DP_Unload(void)
{
    Plug_RemoveHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
}

void G_PreInit(char const *gameId)
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

DENG_ENTRYPOINT void *GetGameAPI(char const *name)
{
    if (auto *ptr = Common_GetGameAPI(name))
    {
        return ptr;
    }

    #define HASH_ENTRY(Name, Func) std::make_pair(QByteArray(Name), de::function_cast<void *>(Func))
    static QHash<QByteArray, void *> const funcs(
    {
        HASH_ENTRY("DrawWindow",    D_DrawWindow),
        HASH_ENTRY("EndFrame",      D_EndFrame),
        HASH_ENTRY("GetInteger",    D_GetInteger),
        HASH_ENTRY("GetPointer",    D_GetVariable),
        HASH_ENTRY("PostInit",      D_PostInit),
        HASH_ENTRY("PreInit",       G_PreInit),
        HASH_ENTRY("Shutdown",      D_Shutdown),
        HASH_ENTRY("TryShutdown",   G_TryShutdown),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found.value();
    return nullptr;
}

/**
 * This function is called automatically when the plugin is loaded for the first time.
 * We let the engine know what we'd like to do.
 */
DENG_ENTRYPOINT void DP_Initialize()
{
    Plug_AddHook(HOOK_STARTUP, G_RegisterGames);
}

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * automatically when the plugin is loaded.
 */
DENG_ENTRYPOINT char const *deng_LibraryType(void)
{
    return "deng-plugin/game";
}

#if defined (DENG_STATIC_LINK)

DENG_EXTERN_C void *staticlib_doom_symbol(char const *name)
{
    DENG_SYMBOL_PTR(name, deng_LibraryType)
    DENG_SYMBOL_PTR(name, DP_Initialize);
    DENG_SYMBOL_PTR(name, DP_Load);
    DENG_SYMBOL_PTR(name, DP_Unload);
    DENG_SYMBOL_PTR(name, GetGameAPI);
    qWarning() << name << "not found in doom";
    return nullptr;
}

#else

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

#endif
