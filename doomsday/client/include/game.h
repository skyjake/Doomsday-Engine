/** @file game.h  Game mode configuration (metadata, resource files, etc...).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_GAME_H
#define DENG_GAME_H

#include "api_plugin.h"
#include <de/Error>
#include <de/Path>
#include <de/String>
#include <de/game/Game>
#include <QMultiMap>

struct manifest_s;
struct gamedef_s;

namespace de {

class File1;
class ResourceManifest;

/**
 * Records top-level game configurations registered by the loaded game plugin(s).
 *
 * @ingroup core
 */
class Game : public de::game::Game
{
public:
    /// Logical game status:
    enum Status {
        Loaded,
        Complete,
        Incomplete
    };

    typedef QMultiMap<resourceclassid_t, ResourceManifest *> Manifests;

public:
    /**
     * @param identityKey   Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
     * @param configDir     Name of the config directory.
     */
    Game(String const &identityKey, Path const &configDir,
         String const &title = "Unnamed", String const &author = "Unknown");

    virtual ~Game();

    /**
     * Determines the status of the game.
     *
     * @see statusAsText()
     */
    Status status() const;

    /**
     * Returns a textual representation of the current game status.
     *
     * @see status()
     */
    String const &statusAsText() const;

    /**
     * Returns the unique identifier of the plugin which registered the game.
     */
    pluginid_t pluginId() const;

    /**
     * Change the identfier of the plugin associated with this.
     * @param newId  New identifier.
     */
    void setPluginId(pluginid_t newId);

    /**
     * Returns the unique identity key of the game.
     */
    de::String const &identityKey() const;

    /**
     * Returns the title of the game, as text.
     */
    de::String const &title() const;

    /**
     * Returns the author of the game, as text.
     */
    de::String const &author() const;

    /**
     * Returns the name of the main config file for the game.
     */
    de::Path const &mainConfig() const;

    /**
     * Returns the name of the binding config file for the game.
     */
    de::Path const &bindingConfig() const;

    /**
     * Returns the identifier of the Style logo image to represent this game.
     */
    de::String logoImageId() const;

    /**
     * Add a new manifest to the list of manifests.
     *
     * @note Registration order defines load order (among files of the same class).
     *
     * @param manifest  Manifest to add.
     */
    void addManifest(ResourceManifest &manifest);

    bool allStartupFilesFound() const;

    /**
     * Provides access to the manifests for efficent traversals.
     */
    Manifests const &manifests() const;

    /**
     * Is @a file required by this game? This decision is made by comparing the
     * absolute path of the specified file to those in the list of located, startup
     * resources for the game. If the file's path matches one of these it is therefore
     * "required" by this game.
     *
     * @param file      File to be tested for required-status. Can be a contained file
     *                  (such as a lump from a Wad file), in which case the path of the
     *                  root (i.e., outermost file) file is used for testing this status.
     *
     * @return  @c true iff @a file is required by this game.
     */
    bool isRequiredFile(File1 &file);

public:
    /**
     * Construct a new Game instance from the specified definition @a def.
     *
     * @note May fail if the definition is incomplete or invalid (@c NULL is returned).
     */
    static Game *fromDef(GameDef const &def);

    /**
     * Print a game mode banner with rulers.
     *
     * @todo This has been moved here so that strings like the game title and author
     *       can be overridden (e.g., via DEHACKED). Make it so!
     */
    static void printBanner(Game const &game);

    /**
     * Print the list of resource files for @a Game.
     *
     * @param game          Game to list the files of.
     * @param rflags        Only consider files whose @ref fileFlags match
     *                      this value. If @c <0 the flags are ignored.
     * @param printStatus   @c true to  include the current availability/load status
     *                      of each file.
     */
    static void printFiles(Game const &game, int rflags, bool printStatus = true);

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

typedef Game::Manifests GameManifests;

/**
 * The special "null" Game object.
 * @todo Should employ the Singleton pattern.
 */
class NullGame : public Game
{
public:
    /// General exception for invalid action on a NULL object. @ingroup errors
    DENG2_ERROR(NullObjectError);

public:
    NullGame();

    void addManifest(struct manifest_s & /*record*/) {
        throw NullObjectError("NullGame::addResource", "Invalid action on null-object");
    }

    bool isRequiredResource(char const * /*absolutePath*/) {
        return false; // Never.
    }

    bool allStartupFilesFound() const {
        return true; // Always.
    }

    struct manifest_s *const *manifests(resourceclassid_t /*classId*/, int * /*count*/) const {
        return 0;
    }

    static Game *fromDef(GameDef const & /*def*/) {
        throw NullObjectError("NullGame::fromDef", "Not valid for null-object");
    }
};

} // namespace de

#endif /* DENG_GAME_H */
