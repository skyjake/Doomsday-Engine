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

/**
 * @defgroup printGameFlags  Print Game Flags.
 */
///@{
#define PGF_BANNER                 0x1
#define PGF_STATUS                 0x2
#define PGF_LIST_STARTUP_RESOURCES 0x4
#define PGF_LIST_OTHER_RESOURCES   0x8

#define PGF_EVERYTHING             (PGF_BANNER|PGF_STATUS|PGF_LIST_STARTUP_RESOURCES|PGF_LIST_OTHER_RESOURCES)
///@}

struct AbstractResource_s;
struct gamedef_s;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace de {

class GameCollection;

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
    virtual ~Game();

    /// @return  Collection in which this game exists.
    GameCollection& collection() const;

    /// @return  Unique plugin identifier attributed to that which registered this.
    pluginid_t pluginId() const;

    /// @return  String containing the identity key.
    ddstring_t const& identityKey() const;

    /// @return  String containing the default title.
    ddstring_t const& title() const;

    /// @return  String containing the default author.
    ddstring_t const& author() const;

    /// @return  String containing the name of the main config file.
    ddstring_t const& mainConfig() const;

    /// @return  String containing the name of the binding config file.
    ddstring_t const& bindingConfig() const;

    /**
     * @note Unless caller is the resource locator then you probably shouldn't be calling.
     * This is the absolute data path and shouldn't be used directly for resource location.
     *
     * @return  String containing the base data-class resource directory.
     */
    ddstring_t const& dataPath() const;

    /**
     * @note Unless caller is the resource locator then you probably shouldn't be calling.
     * This is the absolute defs path and shouldn't be used directly for resource location.
     *
     * @return  String containing the base defs-class resource directory.
     */
    ddstring_t const& defsPath() const;

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

    bool allStartupResourcesFound() const;

    /**
     * Retrieve a subset of the resource collection associated with this.
     *
     * @param rclass  Class of resource to collect.
     * @return  Vector of selected resource records.
     */
    struct AbstractResource_s* const* resources(resourceclass_t rclass, int* count) const;

// Static members ------------------------------------------------------------------

    /**
     * Construct a new Game instance from the specified definition @a def.
     *
     * @note May fail if the definition is incomplete or invalid (@c NULL is returned).
     */
    static Game* fromDef(GameDef const& def);

    /**
     * Print a game mode banner with rulers.
     *
     * @todo This has been moved here so that strings like the game title and author
     *       can be overridden (e.g., via DEHACKED). Make it so!
     */
    static void printBanner(Game const& game);

    /**
     * Print the list of resources for @a Game.
     *
     * @param game          Game to list resources of.
     * @param printStatus   @c true= Include the current availability/load status
     *                      of each resource.
     * @param rflags        Only consider resources whose @ref resourceFlags match
     *                      this value. If @c <0 the flags are ignored.
     */
    static void printResources(Game const& game, bool printStatus, int rflags);

    /**
     * Print extended information about game @a info.
     *
     * @param info  Game record to be printed.
     * @param flags  &ref printGameFlags
     */
    static void print(Game const& game, int flags);

private:
    struct Instance;
    Instance* d;
};

/**
 * The special "null" Game object.
 */
class NullGame : public Game
{
public:
    /// General exception for invalid action on a NULL object. @ingroup errors
    DENG2_ERROR(NullObjectError);

public:
    NullGame(ddstring_t const* dataPath, ddstring_t const* defsPath);

    Game& addResource(resourceclass_t /*rclass*/, struct AbstractResource_s& /*record*/) {
        throw NullObjectError("NullGame::addResource", "Invalid action on null-object");
    }

    bool isRequiredResource(char const* /*absolutePath*/) {
        return false; // Never.
    }

    bool allStartupResourcesFound() const {
        return true; // Always.
    }

    struct AbstractResource_s* const* resources(resourceclass_t /*rclass*/, int* /*count*/) const {
        return 0;
    }

    static Game* fromDef(GameDef const& /*def*/) {
        throw NullObjectError("NullGame::fromDef", "Not valid for null-object");
    }
};

/// @return  @c true= @a game is a "null-game" object (not a real playable game).
inline bool isNullGame(Game const& game) {
    return !!dynamic_cast<NullGame const*>(&game);
}

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

boolean Game_IsNullObject(Game const* game);

struct game_s* Game_AddResource(Game* game, resourceclass_t rclass, struct AbstractResource_s* record);

boolean Game_AllStartupResourcesFound(Game const* game);

Game* Game_SetPluginId(Game* game, pluginid_t pluginId);

pluginid_t Game_PluginId(Game const* game);

ddstring_t const* Game_IdentityKey(Game const* game);

ddstring_t const* Game_Title(Game const* game);

ddstring_t const* Game_Author(Game const* game);

ddstring_t const* Game_MainConfig(Game const* game);

ddstring_t const* Game_BindingConfig(Game const* game);

struct AbstractResource_s* const* Game_Resources(Game const* game, resourceclass_t rclass, int* count);

ddstring_t const* Game_DataPath(Game const* game);

ddstring_t const* Game_DefsPath(Game const* game);

Game* Game_FromDef(GameDef const* def);

void Game_PrintBanner(Game const* game);

void Game_PrintResources(Game const* game, boolean printStatus, int rflags);

void Game_Print(Game const* game, int flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_GAME_H */
