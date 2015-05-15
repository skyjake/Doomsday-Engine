/** @file dd_main.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_MAIN_H
#define DENG_MAIN_H

#include <QList>
#include <QMap>
#include <de/LibraryFile>
#include <de/String>
#include <doomsday/resource/resourceclass.h>

#include "api_plugin.h"
#include "api_gameexport.h"
#include "Games"

#include "resource/resourcesystem.h"
#include "world/worldsystem.h"
#include "ui/infine/infinesystem.h"

namespace de {
class File1;
}

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

extern de::dint verbose;
extern de::dint isDedicated;
extern de::dint gameDataFormat;
#ifdef __CLIENT__
extern de::dint symbolicEchoMode;
#endif

de::dint DD_EarlyInit();
void DD_FinishInitializationAfterWindowReady();

/**
 * Returns @c true if shutdown is in progress.
 */
bool DD_IsShuttingDown();

/**
 * Print an error message and quit.
 */
void App_Error(char const *error, ...);

void App_AbnormalShutdown(char const *error);

/// Returns the application's global InFineSystem.
InFineSystem &App_InFineSystem();

/// Returns the application's global WorldSystem.
de::WorldSystem &App_WorldSystem();

#undef Con_Open

/**
 * Attempt to change the 'open' state of the console.
 * @note In dedicated mode the console cannot be closed.
 */
void Con_Open(de::dint yes);

void DD_CheckTimeDemo();
void DD_UpdateEngineState();

//
// Resources (logical) ------------------------------------------------------------
//

/// Returns the application's global ResourceSystem.
ResourceSystem &App_ResourceSystem();

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 */
ResourceClass &App_ResourceClass(de::String className);

/// @overload
ResourceClass &App_ResourceClass(resourceclassid_t classId);

//
// Game modules -------------------------------------------------------------------
//

/**
 * Switch to/activate the specified game.
 */
bool App_ChangeGame(de::Game &game, bool allowReload = false);

/**
 * Returns @c true if a game module is presently loaded.
 */
dd_bool App_GameLoaded();

/**
 * Returns the application's global Games (collection).
 */
de::Games &App_Games();

/**
 * Returns the current game from the application's global collection.
 */
de::Game &App_CurrentGame();

/**
 * Frees the info structures for all registered games.
 */
void App_ClearGames();

//
// Plugins ------------------------------------------------------------------------
//

/**
 * Loads all the plugins from the library directory. Note that audio plugins
 * are not loaded here, they are managed by AudioDriver.
 */
void Plug_LoadAll();

/**
 * Unloads all plugins.
 */
void Plug_UnloadAll();


/**
 * @return Unique identifier of the currently active plugin. The currently
 * active plugin is tracked separately for each thread.
 */
pluginid_t DD_ActivePluginId();

/**
 * Sets the ID of the currently active plugin in the current thread.
 *
 * @param id  Plugin id.
 */
void DD_SetActivePluginId(pluginid_t id);

/**
 * Executes all the hooks of the given type. Bit zero of the return value
 * is set if a hook was executed successfully (returned true). Bit one is
 * set if all the hooks that were executed returned true.
 */
de::dint DD_CallHooks(de::dint hook_type, de::dint parm, void *context);

de::LibraryFile const &Plug_FileForPlugin(pluginid_t id);

bool DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId);

/**
 * Locate the address of the named, exported procedure in the plugin.
 */
void *DD_FindEntryPoint(pluginid_t pluginId, char const *fn);

//
// Misc/utils ---------------------------------------------------------------------
//

/**
 * Attempts to read help strings from the game-specific help file.
 */
void DD_ReadGameHelp();

/// @return  Symbolic name of the material scheme associated with @a textureSchemeName.
AutoStr *DD_MaterialSchemeNameForTextureScheme(Str const *textureSchemeName);

/// @overload
de::String DD_MaterialSchemeNameForTextureScheme(de::String textureSchemeName);

fontschemeid_t DD_ParseFontSchemeName(char const *str);

#endif  // DENG_MAIN_H
