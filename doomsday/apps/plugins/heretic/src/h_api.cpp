/**\file h_api.c
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
 * Doomsday API exchange - jHeretic specific.
 */

#include <assert.h>
#include <string.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <gamefw/libgamefw.h>
#include <de/ScriptSystem>

#include "doomsday.h"

#include "jheretic.h"

#include "d_netsv.h"
#include "d_net.h"
#include "fi_lib.h"
#include "g_common.h"
#include "g_defs.h"
#include "g_update.h"
#include "hu_menu.h"
#include "p_mapsetup.h"
#include "r_common.h"
#include "p_map.h"
#include "p_enemy.h"
#include "polyobjs.h"

using namespace de;

// Identifiers given to the games we register during startup.
static char const *gameIds[NUM_GAME_MODES] =
{
    "heretic-share",
    "heretic",
    "heretic-ext",
};

static void setCommonParameters(Game &game)
{
    Record gameplayOptions;
    gameplayOptions.set("fast", Record::withMembers("label", "Fast Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("respawn", Record::withMembers("label", "Respawn Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("noMonsters", Record::withMembers("label", "No Monsters", "type", "boolean", "default", false));
    gameplayOptions.set("turbo", Record::withMembers("label", "Move Speed", "type", "number", "default", 1.0, "min", 0.1, "max", 4.0, "step", 0.1));
    game.objectNamespace().set(Game::DEF_OPTIONS, gameplayOptions);
}

/**
 * Register the game modes supported by this plugin.
 */
static int G_RegisterGames(int hookType, int param, void* data)
{
    Games &games = DoomsdayApp::games();

#define CONFIGDIR               "heretic"
#define STARTUPPK3              "libheretic.pk3"
#define LEGACYSAVEGAMENAMEEXP   "^(?:HticSav)[0-9]{1,1}(?:.hsg)"
#define LEGACYSAVEGAMESUBFOLDER "savegame"

    DENG_UNUSED(hookType); DENG_UNUSED(param); DENG_UNUSED(data);

    /* Heretic (Extended) */
    Game &extended = games.defineGame(gameIds[heretic_extended],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Heretic: Shadow of the Serpent Riders",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1996-03-31",
                            Game::DEF_TAGS, "heretic",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/heretic-ext.mapinfo"));
    //extended.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    //extended.addResource(RC_PACKAGE, FF_STARTUP, "heretic.wad", "EXTENDED;E5M2;E5M7;E6M2;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    extended.addResource(RC_DEFINITION, 0, "heretic-ext.ded", 0);
    extended.setRequiredPackages(StringList() << "com.ravensoftware.heretic.extended"
                                              << "net.dengine.legacy.heretic_2");
    setCommonParameters(extended);

    /* Heretic */
    Game &htc = games.defineGame(gameIds[heretic],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Heretic Registered",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1994-12-23",
                            Game::DEF_TAGS, "heretic",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/heretic.mapinfo"));
    //htc.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    //htc.addResource(RC_PACKAGE, FF_STARTUP, "heretic.wad", "E2M2;E3M6;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    htc.addResource(RC_DEFINITION, 0, "heretic.ded", 0);
    htc.setRequiredPackages(StringList() << "com.ravensoftware.heretic"
                                         << "net.dengine.legacy.heretic_2");
    setCommonParameters(htc);

    /* Heretic (Shareware) */
    Game &shareware = games.defineGame(gameIds[heretic_shareware],
        Record::withMembers(Game::DEF_CONFIG_DIR, CONFIGDIR,
                            Game::DEF_TITLE, "Heretic Shareware",
                            Game::DEF_AUTHOR, "Raven Software",
                            Game::DEF_RELEASE_DATE, "1994-12-23",
                            Game::DEF_TAGS, "heretic shareware",
                            Game::DEF_LEGACYSAVEGAME_NAME_EXP, LEGACYSAVEGAMENAMEEXP,
                            Game::DEF_LEGACYSAVEGAME_SUBFOLDER, LEGACYSAVEGAMESUBFOLDER,
                            Game::DEF_MAPINFO_PATH, "$(App.DataPath)/$(GamePlugin.Name)/heretic-share.mapinfo"));
    //shareware.addResource(RC_PACKAGE, FF_STARTUP, STARTUPPK3, 0);
    //shareware.addResource(RC_PACKAGE, FF_STARTUP, "heretic1.wad", "E1M1;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    shareware.addResource(RC_DEFINITION, 0, "heretic-share.ded", 0);
    shareware.setRequiredPackages(StringList() << "com.ravensoftware.heretic.shareware"
                                               << "net.dengine.legacy.heretic_2");
    setCommonParameters(shareware);
    return true;

#undef STARTUPPK3
#undef CONFIGDIR
}

static de::Value *Function_Player_SetFlameCount(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    P_ContextPlayer(ctx).flameCount = args.at(0)->asInt();
    return nullptr;
}

static de::Value *Function_Thing_Attack(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    using namespace de;

    mobj_t *src = &P_ContextMobj(ctx);

    const int meleeDamage = args.at(0)->asInt();
    const mobjtype_t missileId = mobjtype_t(Defs().getMobjNum(args.at(1)->asText()));

    return new NumberValue(P_Attack(src, meleeDamage, missileId));
}

/**
 * Called right after the game plugin is selected into use.
 */
DENG_ENTRYPOINT void DP_Load(void)
{
    Plug_AddHook(HOOK_VIEWPORT_RESHAPE, R_UpdateViewport);
    gfw_SetCurrentGame(GFW_HERETIC);
    Common_Load();

    // Scripting setup.
    {
        auto &scr = ScriptSystem::get();
        auto &playerClass = scr.builtInClass("App", "Player");

        Common_GameBindings().init(playerClass)
            << DE_FUNC(Player_SetFlameCount, "setFlameCount", "tics");

        // Powerup constants.
        playerClass.set("PT_ALLMAP", PT_ALLMAP);
        playerClass.set("PT_FLIGHT", PT_FLIGHT);
        playerClass.set("PT_HEALTH2", PT_HEALTH2);
        playerClass.set("PT_INFRARED", PT_INFRARED);
        playerClass.set("PT_INVISIBILITY", PT_INVISIBILITY);
        playerClass.set("PT_INVULNERABILITY", PT_INVULNERABILITY);
        playerClass.set("PT_SHIELD", PT_SHIELD);
        playerClass.set("PT_WEAPONLEVEL2", PT_WEAPONLEVEL2);

        Function::Defaults attackArgs;
        attackArgs["damage"]  = new NumberValue(0.0);
        attackArgs["missile"] = new NoneValue;

        Common_GameBindings().init(scr.builtInClass("World", "Thing"))
                << DENG2_FUNC_DEFS(Thing_Attack, "attack", "damage" << "missile", attackArgs);
    }
}

/**
 * Called when the game plugin is freed from memory.
 */
DENG_ENTRYPOINT void DP_Unload(void)
{
    // Scripting cleanup.
    {
        ScriptSystem::get().builtInClass("App", "Player").removeMembersWithPrefix("PT_");
    }

    Common_Unload();

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

    H_PreInit();
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
        HASH_ENTRY("DrawWindow",    H_DrawWindow),
        HASH_ENTRY("EndFrame",      H_EndFrame),
        HASH_ENTRY("GetInteger",    H_GetInteger),
        HASH_ENTRY("GetPointer",    H_GetVariable),
        HASH_ENTRY("PostInit",      H_PostInit),
        HASH_ENTRY("PreInit",       G_PreInit),
        HASH_ENTRY("Shutdown",      H_Shutdown),
        HASH_ENTRY("TryShutdown",   G_TryShutdown),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found.value();
    return nullptr;
}

/**
 * This function is called automatically when the plugin is loaded.
 * We let the engine know what we'd like to do.
 */
DENG_ENTRYPOINT void DP_Initialize(void)
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

DENG_EXTERN_C void *staticlib_heretic_symbol(char const *name)
{
    DENG_SYMBOL_PTR(name, deng_LibraryType)
    DENG_SYMBOL_PTR(name, DP_Initialize);
    DENG_SYMBOL_PTR(name, DP_Load);
    DENG_SYMBOL_PTR(name, DP_Unload);
    DENG_SYMBOL_PTR(name, GetGameAPI);
    qWarning() << name << "not found in heretic";
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
