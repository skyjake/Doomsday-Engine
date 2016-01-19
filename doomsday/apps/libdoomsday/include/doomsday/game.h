/** @file game.h  Game mode configuration (metadata, resource files, etc...).
 *
 * @authors Copyright © 2003-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <doomsday/resource/resourceclass.h>
#include <de/Error>
#include <de/Path>
#include <de/String>
#include <QMultiMap>

class ResourceManifest;
namespace de { class File1; }

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

    typedef QMultiMap<resourceclassid_t, ResourceManifest *> Manifests;

    /**
     * Specifies the game that this game is a variant of. For instance, "Final
     * Doom: Plutonia Experiment" (doom2-plut) is a variant of "Doom II"
     * (doom2). The base game can be used as a fallback for resources,
     * configurations, and other data.
     */
    static de::String const DEF_VARIANT_OF;

    static de::String const DEF_CONFIG_DIR;   ///< Name of the config directory.
    static de::String const DEF_CONFIG_MAIN_PATH; ///< Optional: Path of the main config file.
    static de::String const DEF_CONFIG_BINDINGS_PATH; ///< Optional: Path of the bindings config file.
    static de::String const DEF_TITLE;        ///< Title for the game (intended for humans).
    static de::String const DEF_AUTHOR;       ///< Author of the game (intended for humans).
    static de::String const DEF_LEGACYSAVEGAME_NAME_EXP;  ///< Regular expression used for matching legacy savegame names.
    static de::String const DEF_LEGACYSAVEGAME_SUBFOLDER; ///< Game-specific subdirectory of /home for legacy savegames.
    static de::String const DEF_MAPINFO_PATH; ///< Base relative path to the main MAPINFO definition data.

public:
    /**
     * Constructs a new game.
     *
     * @param id      Identifier. Unique game mode key/identifier, 16 chars max (e.g., "doom1-ultimate").
     * @param params  Parameters.
     */
    Game(de::String const &id, de::Record const &params);

    virtual ~Game();

    bool isNull() const;
    de::String id() const;
    de::String variantOf() const;

    /**
     * Sets the packages required for loading the game.
     *
     * All these packages are loaded when the game is loaded.
     *
     * @param packageIds  List of package IDs.
     */
    void setRequiredPackages(de::StringList const &packageIds);

    /**
     * Returns the list of required package IDs for loading the game.
     */
    de::StringList requiredPackages() const;

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
    de::String const &statusAsText() const;

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
    bool isRequiredFile(de::File1 &file);

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
    void addResource(resourceclassid_t classId, de::dint rflags,
                     char const *names, void const *params);

    // IObject.
    de::Record const &objectNamespace() const;
    de::Record &objectNamespace();

public:
    /**
     * Print a game mode banner with rulers.
     *
     * @todo This has been moved here so that strings like the game title and author
     *       can be overridden (e.g., via DEHACKED). Make it so!
     */
    static void printBanner(Game const &game);

    /**
     * Composes a list of the resource files of the game.
     *
     * @param rflags      Only consider files whose @ref fileFlags match
     *                    this value. If @c <0 the flags are ignored.
     * @param withStatus  @c true to  include the current availability/load status
     *                    of each file.
     */
    de::String filesAsText(int rflags, bool withStatus = true) const;

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
class LIBDOOMSDAY_PUBLIC NullGame : public Game
{
public:
    /// General exception for invalid action on a NULL object. @ingroup errors
    DENG2_ERROR(NullObjectError);

public:
    NullGame();

    void addManifest(ResourceManifest &) override {
        throw NullObjectError("NullGame::addResource", "Invalid action on null-object");
    }
};

#endif // __cplusplus

#endif /* LIBDOOMSDAY_GAME_H */
