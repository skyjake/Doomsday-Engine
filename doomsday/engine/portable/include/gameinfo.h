/**\file gameinfo.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
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

struct resourcerecord_s;
struct GameDef;

typedef struct {
    struct resourcerecord_s** records;
    size_t numRecords;
} resourcerecordset_t;

/**
 * Game Info.  Used to record top-level game configurations registered by
 * the loaded game logic module(s).
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

    /// Vector of records for required game resources (e.g., doomu.wad).
    resourcerecordset_t _requiredResources[RESOURCECLASS_COUNT];
} gameinfo_t;

/**
 * Construct a new GameInfo instance.
 *
 * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
 * @param dataPath      The base directory for all data-class resources.
 * @param defsPath      The base directory for all defs-class resources.
 * @param mainConfig    The main config file. Can be @c NULL.
 * @param title         Default game title.
 * @param author        Default game author.
 */
gameinfo_t* GameInfo_New(const char* identityKey, const ddstring_t* dataPath,
    const ddstring_t* defsPath, const char* mainConfig, const char* title,
    const char* author);

void GameInfo_Delete(gameinfo_t* info);

/**
 * Add a new resource to the list of resources.
 *
 * \note Resource registration order defines the order in which resources of
 * each type are loaded.
 *
 * @param rclass  Class of resource being added.
 * @param record  Resource record being added.
 */
struct resourcerecord_s* GameInfo_AddResource(gameinfo_t* info,
    resourceclass_t rclass, struct resourcerecord_s* record);

/**
 * Change the identfier of the plugin associated with this.
 * @param pluginId  New identifier.
 * @return  Same as @a pluginId for convenience.
 */
pluginid_t GameInfo_SetPluginId(gameinfo_t* info, pluginid_t pluginId);

/**
 * Accessor methods.
 */
/// @return  Unique plugin identifier attributed to that which registered this.
pluginid_t GameInfo_PluginId(gameinfo_t* info);

/// @return  String containing the identity key.
const ddstring_t* GameInfo_IdentityKey(gameinfo_t* info);

/// @return  String containing the default title.
const ddstring_t* GameInfo_Title(gameinfo_t* info);

/// @return  String containing the default author.
const ddstring_t* GameInfo_Author(gameinfo_t* info);

/// @return  String containing the name of the main config file.
const ddstring_t* GameInfo_MainConfig(gameinfo_t* info);

/// @return  String containing the name of the binding config file.
const ddstring_t* GameInfo_BindingConfig(gameinfo_t* info);

/**
 * Retrieve a subset of the resource collection associated with this.
 *
 * @param rclass  Class of resource to collect.
 * @return  Vector of selected resource records.
 */
struct resourcerecord_s* const* GameInfo_Resources(gameinfo_t* info,
    resourceclass_t rclass, size_t* count);

/**
 * \note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the absolute data path and shouldn't be used directly for resource location.
 *
 * @return  String containing the base data-class resource directory.
 */
const ddstring_t* GameInfo_DataPath(gameinfo_t* info);

/**
 * \note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the absolute defs path and shouldn't be used directly for resource location.
 *
 * @return  String containing the base defs-class resource directory.
 */
const ddstring_t* GameInfo_DefsPath(gameinfo_t* info);

/**
 * Static non-members:
 */
gameinfo_t* GameInfo_FromDef(const GameDef* def);

#endif /* LIBDENG_GAMEINFO_H */
