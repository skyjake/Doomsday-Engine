/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GAMEINFO_H
#define LIBDENG_GAMEINFO_H

#include "de_base.h"
#include "m_string.h"

/**
 * Game Info.
 * Used to record top-level game configurations registered by the loaded game logic module(s).
 *
 * @ingroup core
 */
typedef struct {
//private:
    /// Unique identifier of the plugin which registered this game.
    pluginid_t _pluginId;

    /// Mode id supplied by the plugin which registered this game.
    int _mode;

    /// Unique mode identifier string (e.g., doom1-ultimate).
    ddstring_t _modeIdentifier;

    /// Formatted default title suitable for printing (e.g., "The Ultimate DOOM").
    ddstring_t _title;

    /// Formatted default author suitable for printing (e.g., "id Software").
    ddstring_t _author;

    /// The base directory for all data-class resources.
    ddstring_t _dataPath;

    /// Command-line selection flags.
    ddstring_t* _cmdlineFlag, *_cmdlineFlag2;

    /// Lists of relative search paths to use when locating resource files. Determined automatically at creation time.
    ddstring_t _searchPathLists[NUM_RESOURCE_CLASSES];

    /// Vector of file names for required resource files (e.g., "doomu.wad").
    ddstring_t** _requiredFileNames;

    /// Vector of lump names used for automatic identification and/or selection.
    ddstring_t** _modeLumpNames;
} gameinfo_t;

gameinfo_t* P_CreateGameInfo(pluginid_t pluginId, int mode, const char* modeString, const char* dataPath,
    const char* title, const char* author, const ddstring_t* cmdlineFlag, const ddstring_t* cmdlineFlag2);

void P_DestroyGameInfo(gameinfo_t* info);

/**
 * Add a new file name to the list of required files.
 */
void GameInfo_AddRequiredFileName(gameinfo_t* info, const ddstring_t* fileName);

/**
 * Add a new lump name to the list of identification lumps.
 */
void GameInfo_AddModeLumpName(gameinfo_t* info, const ddstring_t* lumpName);

/**
 * Add a new file path to the list of resource-locator search paths.
 */
boolean GameInfo_AddResourceSearchPath(gameinfo_t* info, ddresourceclass_t resClass,
    const char* newPath, boolean append);

/**
 * Clear resource-locator search paths for all resource classes.
 */
void GameInfo_ClearResourceSearchPaths(gameinfo_t* info);

/**
 * Clear resource-locator search paths for a specific resource class.
 */
void GameInfo_ClearResourceSearchPaths2(gameinfo_t* info, ddresourceclass_t resClass);

/**
 * @return              Ptr to a string containing the resource class search path list.
 */
const ddstring_t* GameInfo_ResourceSearchPaths(gameinfo_t* info, ddresourceclass_t resClass);

/**
 * Accessor methods.
 */
/// @return             Unique plugin identifier attributed to that which registered the game.
pluginid_t GameInfo_PluginId(gameinfo_t* info);

/// @return             Integer id of the game-mode provided by the plugin which registered it.
int GameInfo_Mode(gameinfo_t* info);

/// @return             Ptr to a string containing the game-mode-identifier.
const ddstring_t* GameInfo_ModeIdentifier(gameinfo_t* info);

/// @return             Ptr to a string containing the default game title.
const ddstring_t* GameInfo_Title(gameinfo_t* info);

/// @return             Ptr to a string containing the default game author.
const ddstring_t* GameInfo_Author(gameinfo_t* info);

/// @return             Ptr to a string containing the base data-class resource directory.
const ddstring_t* GameInfo_DataPath(gameinfo_t* info);

/// @return             Ptr to a string containing command line (name) flag.
const ddstring_t* GameInfo_CmdlineFlag(gameinfo_t* info);

/// @return             Ptr to a string containing command line (name) flag2.
const ddstring_t* GameInfo_CmdlineFlag2(gameinfo_t* info);

/// @return             Ptr to an array of strings containing the names of required files.
const ddstring_t* const* GameInfo_RequiredFileNames(gameinfo_t* info);

/// @return             Ptr to an array of strings containing the names of lumps for game identification purposes.
const ddstring_t* const* GameInfo_ModeLumpNames(gameinfo_t* info);

#endif /* LIBDENG_GAMEINFO_H */
