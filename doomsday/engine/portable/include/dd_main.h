/**\file dd_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Engine Core
 */

#ifndef LIBDENG_MAIN_H
#define LIBDENG_MAIN_H

#include "dd_types.h"
#include "gameinfo.h"
#include "sys_direc.h"

// Forward declarations.
struct pathdirectory_s;
struct pathdirectory_node_s;

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern int verbose;
extern FILE* outFile; // Output file for console messages.

extern filename_t ddBasePath;
extern filename_t ddRuntimePath, ddBinPath;

extern char* gameStartupFiles; // A list of names of files to be autoloaded during startup, whitespace in between (in .cfg).

extern int isDedicated;

extern finaleid_t titleFinale;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

int DD_EarlyInit(void);
int DD_Main(void);
void DD_CheckTimeDemo(void);
void DD_UpdateEngineState(void);

/**
 * Executes all the hooks of the given type. Bit zero of the return value
 * is set if a hook was executed successfully (returned true). Bit one is
 * set if all the hooks that were executed returned true.
 */
int DD_CallHooks(int hook_type, int parm, void* data);

/// @return  Unique identified of the plugin responding to active hook callback.
pluginid_t DD_PluginIdForActiveHook(void);

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
const ddstring_t* DD_TextureNamespaceNameForId(texturenamespaceid_t id);

materialnamespaceid_t DD_ParseMaterialNamespace(const char* str);

fontnamespaceid_t DD_ParseFontNamespace(const char* str);

struct material_s* DD_MaterialForTextureIndex(uint index, texturenamespaceid_t texNamespace);

int DD_SearchPathDirectoryCompare(struct pathdirectory_node_s* node, void* paramaters);

/**
 * Search the directory for @a searchPath.
 *
 * @param flags  @see pathComparisonFlags
 * @param searchPath  Relative or absolute path.
 * @param delimiter  Fragments of the path are delimited by this character.
 *
 * @return  Pointer to the associated node iff found else @c 0
 */
struct pathdirectory_node_s* DD_SearchPathDirectory(struct pathdirectory_s* pd, int flags, const char* searchPath, char delimiter);

const char* value_Str(int val);

/**
 * @return  Ptr to the currently active GameInfo structure (always succeeds).
 */
gameinfo_t* DD_GameInfo(void);

/**
 * @return  Current number of GameInfo structures.
 */
int DD_GameInfoCount(void);

/**
 * @return  GameInfo for index @a idx else @c NULL.
 */
gameinfo_t* DD_GameInfoByIndex(int idx);

/**
 * @return  GameInfo associated with @a identityKey else @c NULL.
 */
gameinfo_t* DD_GameInfoByIdentityKey(const char* identityKey);

/**
 * Is this the special "null-game" object (not a real playable game).
 * \todo Implement a proper null-gameinfo object for this.
 */
boolean DD_IsNullGameInfo(gameinfo_t* info);

/**
 * @defgroup printGameInfoFlags  Print Game Info Flags.
 * @{
 */
#define PGIF_BANNER                 0x1
#define PGIF_STATUS                 0x2
#define PGIF_LIST_STARTUP_RESOURCES 0x4
#define PGIF_LIST_OTHER_RESOURCES   0x8

#define PGIF_EVERYTHING             (PGIF_BANNER|PGIF_STATUS|PGIF_LIST_STARTUP_RESOURCES|PGIF_LIST_OTHER_RESOURCES)
/**@}*/

/**
 * Print extended information about game @a info.
 * @param info  GameInfo record to be printed.
 * @param flags  &see printGameInfoFlags
 */
void DD_PrintGameInfo(gameinfo_t* info, int flags);

/**
 * Frees the info structures for all registered games.
 */
void DD_DestroyGameInfo(void);

D_CMD(Load);
D_CMD(Unload);
D_CMD(Reset);
D_CMD(ReloadGame);
D_CMD(ListGames);

#endif /* LIBDENG_MAIN_H */
