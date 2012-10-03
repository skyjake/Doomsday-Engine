/**\file game.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_GAME_H
#define LIBDENG_GAME_H

#include "dd_plugin.h"
#include <de/ddstring.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AbstractResource_s;
struct gamedef_s;

/**
 * Game.  Used to record top-level game configurations registered by
 * the loaded game plugin(s).
 *
 * @ingroup core
 */
struct Game_s; // The game instance (opaque).
typedef struct Game_s Game;

/**
 * Construct a new Game instance.
 *
 * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
 * @param dataPath      The base directory for all data-class resources.
 * @param defsPath      The base directory for all defs-class resources.
 * @param configDir     Name of the config directory.
 * @param title         Default game title.
 * @param author        Default game author.
 */
Game* Game_New(const char* identityKey, const ddstring_t* dataPath,
    const ddstring_t* defsPath, const char* configDir, const char* title,
    const char* author);

void Game_Delete(Game* game);

/**
 * Add a new resource to the list of resources.
 *
 * @note Resource registration order defines the order in which resources of each
 *       type are loaded.
 *
 * @param rclass  Class of resource being added.
 * @param record  Resource record being added.
 */
struct AbstractResource_s* Game_AddResource(Game* game, resourceclass_t rclass,
    struct AbstractResource_s* record);

/**
 * @return  @c true iff @a absolutePath points to a required resource.
 */
boolean Game_IsRequiredResource(Game* game, const char* absolutePath);

boolean Game_AllStartupResourcesFound(Game* game);

/**
 * Change the identfier of the plugin associated with this.
 * @param pluginId  New identifier.
 * @return  Same as @a pluginId for convenience.
 */
pluginid_t Game_SetPluginId(Game* game, pluginid_t pluginId);

/**
 * Accessor methods.
 */
/// @return  Unique plugin identifier attributed to that which registered this.
pluginid_t Game_PluginId(Game* game);

/// @return  String containing the identity key.
const ddstring_t* Game_IdentityKey(Game* game);

/// @return  String containing the default title.
const ddstring_t* Game_Title(Game* game);

/// @return  String containing the default author.
const ddstring_t* Game_Author(Game* game);

/// @return  String containing the name of the main config file.
const ddstring_t* Game_MainConfig(Game* game);

/// @return  String containing the name of the binding config file.
const ddstring_t* Game_BindingConfig(Game* game);

/**
 * Retrieve a subset of the resource collection associated with this.
 *
 * @param rclass  Class of resource to collect.
 * @return  Vector of selected resource records.
 */
struct AbstractResource_s* const* Game_Resources(Game* game, resourceclass_t rclass, int* count);

/**
 * @note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the absolute data path and shouldn't be used directly for resource location.
 *
 * @return  String containing the base data-class resource directory.
 */
const ddstring_t* Game_DataPath(Game* game);

/**
 * @note Unless caller is the resource locator then you probably shouldn't be calling.
 * This is the absolute defs path and shouldn't be used directly for resource location.
 *
 * @return  String containing the base defs-class resource directory.
 */
const ddstring_t* Game_DefsPath(Game* game);

/**
 * Static non-members:
 */
Game* Game_FromDef(const GameDef* def);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAME_H */
