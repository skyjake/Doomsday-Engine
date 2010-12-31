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

#include "dd_string.h"

/**
 * Game Resource Record.
 * Used to record information about a resource (e.g., a file name).
 *
 * @ingroup core
 */
typedef struct {
    /// Class of resource.
    resourceclass_t rclass;

    /// @see resourceFlags.
    int rflags;

    /// List of known potential names. Seperated with a semicolon.
    ddstring_t names;

    /// Vector of resource identify keys (e.g., file or lump names), used for identification purposes.
    ddstring_t** identityKeys;

    /// Path to this resource if found. Set during resource location.
    ddstring_t path;
} gameresource_record_t;

typedef struct {
    gameresource_record_t** records;
    size_t numRecords;
} gameresource_recordset_t;

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

    /// Unique identifier string (e.g., "doom1-ultimate").
    ddstring_t _identityKey;

    /// Formatted default title suitable for printing (e.g., "The Ultimate DOOM").
    ddstring_t _title;

    /// Formatted default author suitable for printing (e.g., "id Software").
    ddstring_t _author;

    /// The base directory for all data-class resources.
    ddstring_t _dataPath;

    /// The base directory for all defs-class resources.
    ddstring_t _defsPath;

    /// Name of the main config file (e.g., "jdoom.cfg").
    ddstring_t _mainConfig;

    /// Name of the file used for control bindings, set automatically at creation time.
    ddstring_t _bindingConfig;

    /// Command-line selection flags.
    ddstring_t* _cmdlineFlag, *_cmdlineFlag2;

    /// Vector of records for required game resources (e.g., doomu.wad).
    gameresource_recordset_t _requiredResources[NUM_RESOURCE_CLASSES];
} gameinfo_t;

/**
 * Create a new GameInfo.
 *
 * @param pluginId      Unique identifier of the plugin to associate with this game.
 * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
 * @param dataPath      The base directory for all data-class resources.
 * @param defsPath      The base directory for all defs-class resources.
 * @param mainConfig    The main config file. Can be @c NULL.
 * @param title         Default game title.
 * @param author        Default game author.
 * @param cmdlineFlag   Command-line game selection override argument (e.g., "ultimate"). Can be @c NULL.
 * @param cmdlineFlag2  Alternative override. Can be @c NULL.
 */
gameinfo_t* P_CreateGameInfo(pluginid_t pluginId, const char* identityKey, const char* dataPath,
    const char* defsPath, const char* mainConfig, const char* title, const char* author,
    const ddstring_t* cmdlineFlag, const ddstring_t* cmdlineFlag2);

void P_DestroyGameInfo(gameinfo_t* info);

/**
 * Add a new resource to the list of resources.
 *
 * \note Resource registration order defines the order in which resources of each type are loaded.
 *
 * @param rclass        Class of resource being added.
 * @param rflags        @see resourceFlags.
 * @param name          Potential resource name.
 */
gameresource_record_t* GameInfo_AddResource(gameinfo_t* info, resourceclass_t rclass, int rflags,
    const ddstring_t* name);

/**
 * Accessor methods.
 */
/// @return             Unique plugin identifier attributed to that which registered this.
pluginid_t GameInfo_PluginId(gameinfo_t* info);

/// @return             Ptr to a string containing the identity key.
const ddstring_t* GameInfo_IdentityKey(gameinfo_t* info);

/// @return             Ptr to a string containing the default title.
const ddstring_t* GameInfo_Title(gameinfo_t* info);

/// @return             Ptr to a string containing the default author.
const ddstring_t* GameInfo_Author(gameinfo_t* info);

/// @return             Ptr to a string containing the name of the main config file.
const ddstring_t* GameInfo_MainConfig(gameinfo_t* info);

/// @return             Ptr to a string containing the name of the binding config file.
const ddstring_t* GameInfo_BindingConfig(gameinfo_t* info);

/// @return             Ptr to a string containing command line (name) flag.
const ddstring_t* GameInfo_CmdlineFlag(gameinfo_t* info);

/// @return             Ptr to a string containing command line (name) flag2.
const ddstring_t* GameInfo_CmdlineFlag2(gameinfo_t* info);

/// @return             Ptr to a vector of required resource records.
gameresource_record_t* const* GameInfo_Resources(gameinfo_t* info, resourceclass_t rclass, size_t* count);

/**
 * \note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the base data path and shouldn't be used directly for resource location.
 * @return              Ptr to a string containing the base data-class resource directory.
 */
const ddstring_t* GameInfo_DataPath(gameinfo_t* info);

/**
 * \note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the base defs path and shouldn't be used directly for resource location.
 * @return              Ptr to a string containing the base defs-class resource directory.
 */
const ddstring_t* GameInfo_DefsPath(gameinfo_t* info);

#endif /* LIBDENG_GAMEINFO_H */
