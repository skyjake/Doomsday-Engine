/** @file dd_main.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Engine Core
 */

#ifndef LIBDENG_MAIN_H
#define LIBDENG_MAIN_H

#ifndef __cplusplus
#  error "C++ required"
#endif

#include "dd_types.h"
#include "resource/resourcesystem.h"
#include "Games"
#include "world/worldsystem.h"
#include "ui/infine/infinesystem.h"
#include "api_plugin.h"
#include "api_gameexport.h"
#include <doomsday/filesys/sys_direc.h>
#include <de/c_wrapper.h>
#include <de/LibraryFile>

#include <QList>
#include <QMap>
#include <de/String>
#include <doomsday/resource/resourceclass.h>

struct game_s;

extern int verbose;
//extern FILE* outFile; // Output file for console messages.

extern char *startupFiles; // A list of names of files to be autoloaded during startup, whitespace in between (in .cfg).

extern int isDedicated;

extern finaleid_t titleFinale;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

extern int gameDataFormat;

extern int symbolicEchoMode;

int DD_EarlyInit(void);
void DD_FinishInitializationAfterWindowReady(void);
dd_bool DD_Init(void);

/**
 * Print an error message and quit.
 */
void App_Error(char const *error, ...);

void App_AbnormalShutdown(char const *error);

#undef Con_Open

/**
 * Attempt to change the 'open' state of the console.
 * @note In dedicated mode the console cannot be closed.
 */
void Con_Open(int yes);

/// @return  @c true if shutdown is in progress.
dd_bool DD_IsShuttingDown(void);

void DD_CheckTimeDemo(void);
void DD_UpdateEngineState(void);

/**
 * Loads all the plugins from the library directory. Note that audio plugins
 * are not loaded here, they are managed by AudioDriver.
 */
void Plug_LoadAll(void);

/**
 * Unloads all plugins.
 */
void Plug_UnloadAll(void);

de::LibraryFile const &Plug_FileForPlugin(pluginid_t id);

/**
 * Executes all the hooks of the given type. Bit zero of the return value
 * is set if a hook was executed successfully (returned true). Bit one is
 * set if all the hooks that were executed returned true.
 */
int DD_CallHooks(int hook_type, int parm, void* data);

/**
 * Sets the ID of the currently active plugin in the current thread.
 *
 * @param id  Plugin id.
 */
void DD_SetActivePluginId(pluginid_t id);

/**
 * @return Unique identifier of the currently active plugin. The currently
 * active plugin is tracked separately for each thread.
 */
pluginid_t DD_ActivePluginId(void);

dd_bool DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId);

/**
 * Locate the address of the named, exported procedure in the plugin.
 */
void* DD_FindEntryPoint(pluginid_t pluginId, const char* fn);

/// @return  Symbolic name of the material scheme associated with @a textureSchemeName.
de::String DD_MaterialSchemeNameForTextureScheme(de::String textureSchemeName);

/// @return  The application's global InFineSystem.
InFineSystem &App_InFineSystem();

/// @return  The application's global ResourceSystem.
ResourceSystem &App_ResourceSystem();

/// @return  The application's global WorldSystem.
de::WorldSystem &App_WorldSystem();

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 */
ResourceClass &App_ResourceClass(de::String className);

/**
 * Convenient method of returning a resource class from the application's global
 * resource system.
 */
ResourceClass &App_ResourceClass(resourceclassid_t classId);

/// @return  @c true iff there is presently a game loaded.
dd_bool App_GameLoaded();

/// @return  The current game from the application's global collection.
de::Game &App_CurrentGame();

/**
 * Switch to/activate the specified game.
 */
bool App_ChangeGame(de::Game &game, bool allowReload = false);

/// @return  The application's global Games (collection).
de::Games &App_Games();

fontschemeid_t DD_ParseFontSchemeName(char const *str);

/// @return  Symbolic name of the material scheme associated with @a textureSchemeName.
AutoStr *DD_MaterialSchemeNameForTextureScheme(Str const *textureSchemeName);

const char* value_Str(int val);

/**
 * Frees the info structures for all registered games.
 */
void DD_DestroyGames(void);

/**
 * Attempts to read help strings from the game-specific help file.
 */
void DD_ReadGameHelp(void);

D_CMD(Load);
D_CMD(Unload);
D_CMD(Reset);
D_CMD(ReloadGame);

#endif /* LIBDENG_MAIN_H */
