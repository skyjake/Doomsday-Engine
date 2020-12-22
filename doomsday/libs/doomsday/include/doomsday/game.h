/** @file game.h  Game mode configuration (metadata, resource files, etc...).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_GAME_H
#define LIBDOOMSDAY_GAME_H

#ifdef __cplusplus

#include <doomsday/plugins.h>
#include <doomsday/resourceclass.h>
#include <doomsday/gameprofiles.h>
#include <de/error.h>
#include <de/path.h>
#include <de/date.h>
#include <de/string.h>
#include <map>

class ResourceManifest;
namespace res { class File1; }

/**
 * Represents a specific playable game that runs on top of Doomsday. There can
 * be only one game loaded at a time. Examples of games are "Doom II" and
 * "Ultimate Doom".
 *
 * The 'load' command can be used to load a game based on its identifier:
 * <pre>load doom2</pre>
 */
class LIBDOOMSDAY_PUBLIC Game : public de::IObject
{
public:
    /// Logical game status:
    enum Status {
        Loaded,
        Complete,
        Incomplete
    };

    typedef std::multimap<resourceclassid_t, ResourceManifest *> Manifests;

    /**
     * Specifies the game that this game is a variant of. For instance, "Final
     * Doom: Plutonia Experiment" (doom2-plut) is a variant of "Doom II"
     * (doom2). The base game can be used as a fallback for resources,
     * configurations, and other data.
     */
    static de::String const DEF_VARIANT_OF;

    static de::String const DEF_FAMILY;       ///< Game family.
    static de::String const DEF_CONFIG_DIR;   ///< Name of the config directory.
    static de::String const DEF_CONFIG_MAIN_PATH; ///< Optional: Path of the main config file.
    static de::String const DEF_CONFIG_BINDINGS_PATH; ///< Optional: Path of the bindings config file.
    static de::String const DEF_TITLE;        ///< Title for the game (intended for humans).
    static de::String const DEF_AUTHOR;       ///< Author of the game (intended for humans).
    static de::String const DEF_RELEASE_DATE;
    static de::String const DEF_TAGS;
    static de::String const DEF_LEGACYSAVEGAME_NAME_EXP;  ///< Regular expression used for matching legacy savegame names.
    static de::String const DEF_LEGACYSAVEGAME_SUBFOLDER; ///< Game-specific subdirectory of /home for legacy savegames.
    static de::String const DEF_MAPINFO_PATH; ///< Base relative path to the main MAPINFO definition data.
    static de::String const DEF_OPTIONS;

public:
    /**
     * Constructs a new game.
     *
     * @param id      Identifier. Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
     * @param params  Parameters.
     */
    Game(const de::String &id, const de::Record &params);

    virtual ~Game();

    bool isNull() const;
    de::String id() const;
    de::String variantOf() const;
    de::String family() const;

    /**
     * Sets the packages required for loading the game.
     *
     * All these packages are loaded when the game is loaded.
     *
     * @param packageIds  List of package IDs.
     */
    void setRequiredPackages(const de::StringList &packageIds);

    void addRequiredPackage(const de::String &packageId);

    /**
     * Returns the list of required package IDs for loading the game.
     */
    de::StringList requiredPackages() const;

    /**
     * Returns the list of packages that the user has chosen to be loaded when
     * joining multiplayer using this game. The list of packages is read from Config.
     */
    de::StringList localMultiplayerPackages() const;

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
    const de::String &statusAsText() const;

    /**
     * Returns information about the game as styled text. Printed by "inspectgame",
     * for instance.
     */
    de::String description() const;

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
     * Returns the title of the game, as text.
     */
    de::String title() const;

    /**
     * Returns the author of the game, as text.
     */
    de::String author() const;

    de::Date releaseDate() const;

    /**
     * Returns the name of the main config file for the game.
     */
    de::Path mainConfig() const;

    /**
     * Returns the name of the binding config file for the game.
     */
    de::Path bindingConfig() const;

    /**
     * Returns the base relative path of the main MAPINFO definition data for the game (if any).
     */
    de::Path mainMapInfo() const;

    /**
     * Returns the identifier of the Style logo image to represent this game.
     */
    de::String logoImageId() const;

    /**
     * Returns the regular expression used for locating legacy savegame files.
     */
    de::String legacySavegameNameExp() const;

    /**
     * Determine the absolute path to the legacy savegame folder for the game. If there is
     * no possibility of a legacy savegame existing (e.g., because the game is newer than
     * the introduction of the modern, package-based .save format) then a zero length string
     * is returned.
     */
    de::String legacySavegamePath() const;

    /**
     * Add a new manifest to the list of manifests.
     *
     * @note Registration order defines load order (among files of the same class).
     *
     * @param manifest  Manifest to add.
     */
    virtual void addManifest(ResourceManifest &manifest);

    bool allStartupFilesFound() const;

    bool isPlayable() const;

    bool isPlayableWithDefaultPackages() const;

    /**
     * Provides access to the manifests for efficent traversals.
     */
    const Manifests &manifests() const;

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
    bool isRequiredFile(res::File1 &file) const;

    /**
     * Adds a new resource to the list for the identified @a game.
     *
     * @note Resource order defines the load order of resources (among those of
     * the same type). Resources are loaded from most recently added to least
     * recent.
     *
     * @param game      Unique identifier/name of the game.
     * @param classId   Class of resource being defined.
     * @param fFlags    File flags (see @ref fileFlags).
     * @param names     One or more known potential names, seperated by semicolon
     *                  (e.g., <pre> "name1;name2" </pre>). Valid names include
     *                  absolute or relative file paths, possibly with encoded
     *                  symbolic tokens, or predefined symbols into the virtual
     *                  file system.
     * @param params    Additional parameters. Usage depends on resource type.
     *                  For package resources this may be C-String containing a
     *                  semicolon delimited list of identity keys.
     */
    void addResource(resourceclassid_t classId, int rflags,
                     const char *names, const void *params);

    /**
     * Returns the built-in profile of the game.
     */
    GameProfile &profile() const;

    // IObject.
    const de::Record &objectNamespace() const;
    de::Record &objectNamespace();

public:
    /**
     * Print a game mode banner with rulers.
     *
     * @todo This has been moved here so that strings like the game title and author
     *       can be overridden (e.g., via DEHACKED). Make it so!
     */
    static void printBanner(const Game &game);

    /**
     * Composes a list of the resource files of the game.
     *
     * @param rflags      Only consider files whose @ref fileFlags match
     *                    this value. If @c <0 the flags are ignored.
     * @param withStatus  @c true to  include the current availability/load status
     *                    of each file.
     */
    de::String filesAsText(int rflags, bool withStatus = true) const;

    static void printFiles(const Game &game, int rflags, bool printStatus = true);

    /// Register the console commands, variables, etc..., of this module.
    static void consoleRegister();

    static de::String logoImageForId(const de::String &id);

    /**
     * Checks the Config if using local packages is enabled in multiplayer games. The
     * user has to manually enable local packages because there may be compatibility
     * problems if the client is using custom resources.
     */
    static bool isLocalPackagesEnabled();

    static de::StringList localMultiplayerPackages(const de::String &gameId);

    /**
     * Sets the packages that will be loaded locally in addition to the server's
     * packages. This is saved to Config.
     *
     * @param packages  List of local packages.
     */
    static void setLocalMultiplayerPackages(const de::String &gameId, de::StringList packages);

private:
    DE_PRIVATE(d)
};

typedef Game::Manifests GameManifests;

/**
 * The special "null" Game object.
 * @todo Should employ the Singleton pattern.
 */
class LIBDOOMSDAY_PUBLIC NullGame : public Game
{
public:
    /// General exception for invalid action on a NULL object. @ingroup errors
    DE_ERROR(NullObjectError);

public:
    NullGame();

    void addManifest(ResourceManifest &) override {
        throw NullObjectError("NullGame::addResource", "Invalid action on null-object");
    }
};

#endif // __cplusplus

#endif /* LIBDOOMSDAY_GAME_H */
