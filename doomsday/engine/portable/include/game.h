/**
 * @file game.h
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
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

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace de {

/**
 * Game.  Used to record top-level game configurations registered by
 * the loaded game plugin(s).
 *
 * @ingroup core
 */
class Game
{
public:
    /**
     * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
     * @param dataPath      The base directory for all data-class resources.
     * @param defsPath      The base directory for all defs-class resources.
     * @param configDir     Name of the config directory.
     * @param title         Default game title.
     * @param author        Default game author.
     */
    Game(char const* identityKey, ddstring_t const* dataPath, ddstring_t const* defsPath,
         char const* configDir, char const* title = "Unnamed", char const* author = "Unknown");
    ~Game();

    /// @return  Unique plugin identifier attributed to that which registered this.
    pluginid_t pluginId();

    /// @return  String containing the identity key.
    ddstring_t const& identityKey();

    /// @return  String containing the default title.
    ddstring_t const& title();

    /// @return  String containing the default author.
    ddstring_t const& author();

    /// @return  String containing the name of the main config file.
    ddstring_t const& mainConfig();

    /// @return  String containing the name of the binding config file.
    ddstring_t const& bindingConfig();

    /**
     * @note Unless caller is the resource locator then you probably shouldn't be calling.
     * This is the absolute data path and shouldn't be used directly for resource location.
     *
     * @return  String containing the base data-class resource directory.
     */
    ddstring_t const& dataPath();

    /**
     * @note Unless caller is the resource locator then you probably shouldn't be calling.
     * This is the absolute defs path and shouldn't be used directly for resource location.
     *
     * @return  String containing the base defs-class resource directory.
     */
    ddstring_t const& defsPath();

    /**
     * Change the identfier of the plugin associated with this.
     * @param newId  New identifier.
     */
    Game& setPluginId(pluginid_t newId);

    /**
     * Add a new resource to the list of resources.
     *
     * @note Resource registration order defines the order in which resources of each
     *       type are loaded.
     *
     * @param rclass  Class of resource being added.
     */
    Game& addResource(resourceclass_t rclass, struct AbstractResource_s& record);

    /**
     * @return  @c true iff @a absolutePath points to a required resource.
     */
    bool isRequiredResource(char const* absolutePath);

    bool allStartupResourcesFound();

    /**
     * Retrieve a subset of the resource collection associated with this.
     *
     * @param rclass  Class of resource to collect.
     * @return  Vector of selected resource records.
     */
    struct AbstractResource_s* const* resources(resourceclass_t rclass, int* count);

// Static members ------------------------------------------------------------------

    /**
     * Construct a new Game instance from the specified definition @a def.
     *
     * @note May fail if the definition is incomplete or invalid (@c NULL is returned).
     */
    static Game* fromDef(GameDef const& def);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct game_s; // The game instance (opaque).
typedef struct game_s Game;

Game* Game_New(char const* identityKey, ddstring_t const* dataPath, ddstring_t const* defsPath, char const* configDir, char const* title, char const* author);

void Game_Delete(Game* game);

struct game_s* Game_AddResource(Game* game, resourceclass_t rclass, struct AbstractResource_s* record);

boolean Game_IsRequiredResource(Game* game, char const* absolutePath);

boolean Game_AllStartupResourcesFound(Game* game);

Game* Game_SetPluginId(Game* game, pluginid_t pluginId);

pluginid_t Game_PluginId(Game* game);

ddstring_t const* Game_IdentityKey(Game* game);

ddstring_t const* Game_Title(Game* game);

ddstring_t const* Game_Author(Game* game);

ddstring_t const* Game_MainConfig(Game* game);

ddstring_t const* Game_BindingConfig(Game* game);

struct AbstractResource_s* const* Game_Resources(Game* game, resourceclass_t rclass, int* count);

ddstring_t const* Game_DataPath(Game* game);

ddstring_t const* Game_DefsPath(Game* game);

struct game_s* Game_FromDef(GameDef const* def);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAME_H */
