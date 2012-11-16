/**\file dd_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Engine Core
 */

#ifndef LIBDENG_MAIN_H
#define LIBDENG_MAIN_H

#include "dd_types.h"
#include "dd_plugin.h"
#ifndef __cplusplus // Kludge: this isn't yet C++ compatible
#  include "textures.h"
#endif
#include "filesys/sys_direc.h"
#include <de/c_wrapper.h>

#ifdef __cplusplus
extern "C" {
#endif

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

#ifdef _DEBUG
#  define DEBUG_Message(code)           {Con_Message code;}
#  define DEBUG_VERBOSE_Message(code)   {if(verbose >= 1) {Con_Message code;}}
#  define DEBUG_VERBOSE2_Message(code)  {if(verbose >= 2) {Con_Message code;}}
#else
#  define DEBUG_Message(code)
#  define DEBUG_VERBOSE_Message(code)
#  define DEBUG_VERBOSE2_Message(code)
#endif

struct gamecollection_s;
struct game_s;

extern int verbose;
//extern FILE* outFile; // Output file for console messages.

extern filename_t ddBasePath;
extern filename_t ddRuntimePath, ddBinPath;

extern char* startupFiles; // A list of names of files to be autoloaded during startup, whitespace in between (in .cfg).

extern int isDedicated;

extern finaleid_t titleFinale;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

/// @return  The Game collection.
struct gamecollection_s* App_GameCollection();

/// @return  The current Game in the collection.
struct game_s* App_CurrentGame();

int DD_EarlyInit(void);
void DD_FinishInitializationAfterWindowReady(void);
boolean DD_Init(void);

/// @return  @c true if shutdown is in progress.
boolean DD_IsShuttingDown(void);

/// @return  @c true iff there is presently a game loaded.
boolean DD_GameLoaded(void);

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

/**
 * Executes all the hooks of the given type. Bit zero of the return value
 * is set if a hook was executed successfully (returned true). Bit one is
 * set if all the hooks that were executed returned true.
 */
int DD_CallHooks(int hook_type, int parm, void* data);

/**
 * Sets the ID of the currently active plugin. Note that plugin hooks are
 * executed in a single-threaded manner; only one can be active at a time.
 *
 * @param id  Plugin id.
 */
void DD_SetActivePluginId(pluginid_t id);

/**
 * @return Unique identifier of the currently active plugin. Note that plugin
 * hooks are executed in a single-threaded manner; only one is active at a
 * time.
 */
pluginid_t DD_ActivePluginId(void);

boolean DD_ExchangeGamePluginEntryPoints(pluginid_t pluginId);

/**
 * Locate the address of the named, exported procedure in the plugin.
 */
void* DD_FindEntryPoint(pluginid_t pluginId, const char* fn);

int DD_GetInteger(int ddvalue);
void DD_SetInteger(int ddvalue, int parm);
void DD_SetVariable(int ddvalue, void* ptr);
void* DD_GetVariable(int ddvalue);

ddplayer_t* DD_GetPlayer(int number);

texturenamespaceid_t DD_ParseTextureNamespace(const char* str);

materialnamespaceid_t DD_ParseMaterialNamespace(const char* str);

fontnamespaceid_t DD_ParseFontNamespace(const char* str);

/// @return  Symbolic name of the material namespace associated with @a namespaceId.
const ddstring_t* DD_MaterialNamespaceNameForTextureNamespace(texturenamespaceid_t texNamespaceId);

/// @return  Unique identifier of the material associated with the identified @a uniqueId texture.
materialid_t DD_MaterialForTextureUniqueId(texturenamespaceid_t texNamespaceId, int uniqueId);

const char* value_Str(int val);

/**
 * Frees the info structures for all registered games.
 */
void DD_DestroyGames(void);

boolean DD_GameInfo(struct gameinfo_s* info);

void DD_AddGameResource(gameid_t game, resourceclassid_t classId, int rflags, char const* names, void* params);

gameid_t DD_DefineGame(struct gamedef_s const* def);

gameid_t DD_GameIdForKey(char const* identityKey);

D_CMD(Load);
D_CMD(Unload);
D_CMD(Reset);
D_CMD(ReloadGame);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_MAIN_H */
