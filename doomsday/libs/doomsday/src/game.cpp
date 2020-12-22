/** @file game.cpp  Game mode configuration (metadata, resource files, etc...).
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

#include "doomsday/game.h"
#include "doomsday/games.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/gameprofiles.h"
#include "doomsday/gamestatefolder.h"
#include "doomsday/console/cmd.h"
#include "doomsday/filesys/file.h"
#include "doomsday/manifest.h"
#include "doomsday/res/resources.h"

#include <de/app.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/dictionaryvalue.h>
#include <de/error.h>
#include <de/log.h>
#include <de/packageloader.h>
#include <de/textvalue.h>
#include <de/charsymbols.h>
#include <algorithm>

using namespace de;
using namespace res;

const static String VAR_RESOURCE_LOCAL_PACKAGES("resource.localPackages");
const static String VAR_RESOURCE_LOCAL_PACKAGES_FOR_GAME("resource.localPackagesForGame");

const static String DEF_ID("ID");

const String Game::DEF_VARIANT_OF("variantOf");
const String Game::DEF_FAMILY("family");
const String Game::DEF_CONFIG_DIR("configDir");
const String Game::DEF_CONFIG_MAIN_PATH("mainConfig");
const String Game::DEF_CONFIG_BINDINGS_PATH("bindingsConfig");
const String Game::DEF_TITLE("title");
const String Game::DEF_AUTHOR("author");
const String Game::DEF_RELEASE_DATE("releaseDate");
const String Game::DEF_TAGS("tags");
const String Game::DEF_LEGACYSAVEGAME_NAME_EXP("legacySavegame.nameExp");
const String Game::DEF_LEGACYSAVEGAME_SUBFOLDER("legacySavegame.subfolder");
const String Game::DEF_MAPINFO_PATH("mapInfoPath");
String const Game::DEF_OPTIONS("options");

DE_PIMPL(Game), public Lockable
{
    pluginid_t pluginId = 0; ///< Unique identifier of the registering plugin.
    Record params;
    StringList requiredPackages; ///< Packages required for starting the game.

    Manifests manifests; ///< Required resource files (e.g., doomu.wad).

    Impl(Public *i, const Record &parms)
        : Base(i)
        , params(parms)
    {
        // Define the optional parameters if needed.
        if (!params.has(DEF_CONFIG_MAIN_PATH))
        {
            params.set(DEF_CONFIG_MAIN_PATH, "/home/configs"/params.gets(DEF_CONFIG_DIR)/"game.cfg");
        }
        if (!params.has(DEF_CONFIG_BINDINGS_PATH))
        {
            params.set(DEF_CONFIG_BINDINGS_PATH, "/home/configs"/params.gets(DEF_CONFIG_DIR)/"player/bindings.cfg");
        }
        if (!params.has(DEF_OPTIONS))
        {
            params.set(DEF_OPTIONS, RecordValue::takeRecord(Record()));
        }

        params.set(DEF_CONFIG_DIR, NativePath(params.gets(DEF_CONFIG_DIR)).expand().withSeparators('/'));
    }

    ~Impl()
    {
        DE_GUARD(this);
        for (auto &i : manifests) delete i.second;
    }

    GameProfile *profile() const
    {
        return maybeAs<GameProfile>(DoomsdayApp::gameProfiles().tryFind(self().title()));
    }

    StringList packagesFromProfile() const
    {
        if (const auto *prof = profile())
        {
            return prof->packages();
        }
        return {};
    }
};

Game::Game(const String &id, const Record &params)
    : d(new Impl(this, params))
{
    d->params.set(DEF_ID, id);
    d->params.set(DEF_VARIANT_OF, params.gets(DEF_VARIANT_OF, ""));
}

Game::~Game()
{}

bool Game::isNull() const
{
    DE_GUARD(d);
    return id().isEmpty();
}

String Game::id() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_ID);
}

String Game::variantOf() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_VARIANT_OF);
}

String Game::family() const
{
    DE_GUARD(d);
    if (d->params.has(DEF_FAMILY))
    {
        return d->params.gets(DEF_FAMILY);
    }
    // We can make a guess...
    if (id().contains("doom"))    return "doom";
    if (id().contains("heretic")) return "heretic";
    if (id().contains("hexen"))   return "hexen";
    return "";
}

void Game::setRequiredPackages(const StringList &packageIds)
{
    DE_GUARD(d);
    d->requiredPackages = packageIds;
}

void Game::addRequiredPackage(const String &packageId)
{
    DE_GUARD(d);
    d->requiredPackages.append(packageId);
}

StringList Game::requiredPackages() const
{
    DE_GUARD(d);
    return d->requiredPackages;
}

StringList Game::localMultiplayerPackages() const
{
    DE_GUARD(d);
    return localMultiplayerPackages(id());
}

bool Game::isLocalPackagesEnabled() // static
{
    return Config::get().getb(VAR_RESOURCE_LOCAL_PACKAGES, false);
}

StringList Game::localMultiplayerPackages(const String &gameId) // static
{
    try
    {
        if (isLocalPackagesEnabled())
        {
            const auto &pkgDict = Config::get().getdt(VAR_RESOURCE_LOCAL_PACKAGES_FOR_GAME);
            TextValue const key(gameId);
            if (pkgDict.contains(key))
            {
                return pkgDict.element(key).as<ArrayValue>().toStringList();
            }
        }
        return StringList();
    }
    catch (const Error &)
    {
        return StringList();
    }
}

void Game::setLocalMultiplayerPackages(const String &gameId, StringList packages) // static
{
    std::unique_ptr<ArrayValue> ids(new ArrayValue);
    for (const String &pkg : packages)
    {
        ids->add(pkg);
    }
    Config::get(VAR_RESOURCE_LOCAL_PACKAGES_FOR_GAME)
            .value().as<DictionaryValue>()
            .setElement(TextValue(gameId), ids.release());
}

void Game::addManifest(ResourceManifest &manifest)
{
    DE_GUARD(d);
    // Ensure we don't add duplicates.
//    auto found = d->manifests.find(manifest.resourceClass(), &manifest);
//    if (found == d->manifests.end())
//    {
//        d->manifests.insert(manifest.resourceClass(), &manifest);
//    }
    d->manifests.insert(std::make_pair(manifest.resourceClass(), &manifest));
}

bool Game::allStartupFilesFound() const
{
    DE_GUARD(d);

    for (const String &pkg : d->requiredPackages + d->packagesFromProfile())
    {
        if (!App::packageLoader().isAvailable(pkg))
            return false;
    }

    for (const auto &i : d->manifests)
    {
        const int flags = i.second->fileFlags();

        if ((flags & FF_STARTUP) && !(flags & FF_FOUND))
            return false;
    }
    return true;
}

bool Game::isPlayable() const
{
    return allStartupFilesFound();
}

bool Game::isPlayableWithDefaultPackages() const
{
    DE_GUARD(d);
    for (const String &pkg : d->requiredPackages)
    {
        if (!App::packageLoader().isAvailable(pkg))
            return false;
    }
    return true;
}

Game::Status Game::status() const
{
    DE_GUARD(d);
    if (App_GameLoaded() && &DoomsdayApp::game() == this)
    {
        return Loaded;
    }
    if (allStartupFilesFound())
    {
        return Complete;
    }
    return Incomplete;
}

const String &Game::statusAsText() const
{
    DE_GUARD(d);
    static String const statusTexts[] = {
        "Loaded",
        "Playable",
        "Not playable (incomplete resources)",
    };
    return statusTexts[int(status())];
}

String Game::description() const
{
    DE_GUARD(d);
    return Stringf(_E(b) "%s - %s\n" _E(.)
                  _E(l) "ID: " _E(.) "%s "
                  _E(l) "PluginId: "    _E(.) "%i\n"
                  _E(D)_E(b) "Packages:\n" _E(.)_E(.) "%s\n"
                  //_E(D)_E(b) "Startup resources:\n" _E(.)_E(.) "%6\n"
                  _E(D)_E(b) "Custom resources:\n" _E(.)_E(.) "%s\n"
                  _E(D)_E(b) "Status: " _E(.) "%s",
            title().c_str(),
            author().c_str(),
            id().c_str(),
            int(pluginId()),
            (" - " _E(>) + String::join(d->requiredPackages, _E(<) "\n - " _E(>)) + _E(<)).c_str(),
            //.arg(filesAsText(FF_STARTUP))
            filesAsText(0, false).c_str(),
            statusAsText().c_str());
}

pluginid_t Game::pluginId() const
{
    DE_GUARD(d);
    return d->pluginId;
}

void Game::setPluginId(pluginid_t newId)
{
    DE_GUARD(d);
    d->pluginId = newId;
}

String Game::logoImageId() const
{
    DE_GUARD(d);
    return logoImageForId(id());
}

String Game::logoImageForId(const String &id)
{
    /// @todo The name of the plugin should be accessible via the plugin loader.
    String plugName;
    if (id.contains("heretic"))
    {
        plugName = "libheretic";
    }
    else if (id.contains("hexen"))
    {
        plugName = "libhexen";
    }
    else
    {
        plugName = "libdoom";
    }

    return "logo.game." + plugName;
}

String Game::legacySavegameNameExp() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_LEGACYSAVEGAME_NAME_EXP, "");
}

String Game::legacySavegamePath() const
{
    DE_GUARD(d);
    NativePath nativeSavePath = Resources::get().nativeSavePath();
    if (nativeSavePath.isEmpty()) return "";
    if (isNull()) return "";

    if (App::commandLine().has("-savedir"))
    {
        // A custom path. The savegames are in the root of this folder.
        return nativeSavePath;
    }

    // The default save path. The savegames are in a game-specific folder.
    if (d->params.gets(DEF_LEGACYSAVEGAME_SUBFOLDER, ""))
    {
        return App::app().nativeHomePath() / d->params.gets(DEF_LEGACYSAVEGAME_SUBFOLDER) / id();
    }

    return "";
}

Path Game::mainConfig() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_CONFIG_MAIN_PATH);
}

Path Game::bindingConfig() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_CONFIG_BINDINGS_PATH);
}

Path Game::mainMapInfo() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_MAPINFO_PATH);
}

String Game::title() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_TITLE);
}

String Game::author() const
{
    DE_GUARD(d);
    return d->params.gets(DEF_AUTHOR);
}

Date Game::releaseDate() const
{
    DE_GUARD(d);
    return Date::fromText(d->params.gets(DEF_RELEASE_DATE, ""));
}

const Game::Manifests &Game::manifests() const
{
    DE_GUARD(d);
    return d->manifests;
}

bool Game::isRequiredFile(File1 &file) const
{
    DE_GUARD(d);

    // If this resource is from a container we must use the path of the
    // root file container instead.
    File1 &rootFile = file;
    while (rootFile.isContained())
    { rootFile = rootFile.container(); }

    String absolutePath = rootFile.composePath();
    bool isRequired = false;

    const auto packages = d->manifests.equal_range(RC_PACKAGE);
    for (auto i = packages.first; i != packages.second; ++i)
    {
        ResourceManifest &manifest = *i->second;
        if (!(manifest.fileFlags() & FF_STARTUP)) continue;

        if (!manifest.resolvedPath(true/*try locate*/).compare(absolutePath, CaseInsensitive))
        {
            isRequired = true;
            break;
        }
    }

    return isRequired;
}

void Game::addResource(resourceclassid_t classId, dint rflags,
                       const char *names, const void *params)
{
    DE_GUARD(d);

    if (!VALID_RESOURCECLASSID(classId))
    {
        throw Error("Game::addResource",
                    "Unknown resource class " + String::asText(classId));
    }

    if (!names || !names[0])
    {
        throw Error("Game::addResource", "Invalid name argument");
    }

    // Construct and attach the new resource record.
    ResourceManifest *manifest = new ResourceManifest(classId, rflags);
    addManifest(*manifest);

    // Add the name list to the resource record.
    StringList nameList = String(names).split(";");
    for (const String &nameRef : nameList)
    {
        manifest->addName(nameRef);
    }

    if (params && classId == RC_PACKAGE)
    {
        // Add the identityKey list to the resource record.
        StringList idKeys = String((const char *) params).split(";");
        for (const String &idKeyRef : idKeys)
        {
            manifest->addIdentityKey(idKeyRef);
        }
    }
}

GameProfile &Game::profile() const
{
    DE_GUARD(d);
    DE_ASSERT(d->profile()); // all games have a matching built-in profile
    return *d->profile();
}

const Record &Game::objectNamespace() const
{
    return d->params;
}

Record &Game::objectNamespace()
{
    return d->params;
}

void Game::printBanner(const Game &game)
{
    LOG_MSG(_E(R) "\n");
    LOG_MSG(_E(1)) << game.title();
    LOG_MSG(_E(R) "\n");
}

String Game::filesAsText(int rflags, bool withStatus) const
{
    String text;

    // Group output by resource class.
    const Manifests &manifs = manifests();
    for (uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        const resourceclassid_t classId = resourceclassid_t(i);
        const auto range = manifs.equal_range(classId);
        for (auto i = range.first; i != range.second; ++i)
        {
            ResourceManifest &manifest = *i->second;
            if (rflags >= 0 && (rflags & manifest.fileFlags()))
            {
                const bool resourceFound = (manifest.fileFlags() & FF_FOUND) != 0;

                if (!text.isEmpty()) text += "\n" _E(0);

                if (withStatus)
                {
                    text += (resourceFound? " - " : _E(1) " ! " _E(.));
                }

                // Format the resource name list.
                text += Stringf(_E(>) "%s%s",
                        !resourceFound? _E(D) : "",
                        String::join(manifest.names(), _E(l) " or " _E(.)).c_str());

                if (withStatus)
                {
                    text += String(": ") + _E(>) + (!resourceFound? _E(b) "missing " _E(.) : "");
                    if (resourceFound)
                    {
                        text += Stringf(
                            _E(C) "\"%s\"" _E(.),
                            NativePath(manifest.resolvedPath(false /*don't try to locate*/))
                                .expand()
                                .pretty()
                                .c_str());
                    }
                    text += _E(<);
                }

                text += _E(<);
            }
        }
    }

    if (text.isEmpty()) return " none";

    return text;
}

void Game::printFiles(const Game &game, int rflags, bool printStatus)
{
    LOG_RES_MSG("") << game.filesAsText(rflags, printStatus);
}

D_CMD(InspectGame)
{
    DE_UNUSED(src);

    const Game *game = nullptr;
    if (argc < 2)
    {
        // No game identity key was specified - assume the current game.
        if (!App_GameLoaded())
        {
            LOG_WARNING("No game is currently loaded.\nPlease specify the identifier of the game to inspect.");
            return false;
        }
        game = &DoomsdayApp::game();
    }
    else
    {
        String idKey = argv[1];
        try
        {
            game = &DoomsdayApp::games()[idKey];
        }
        catch (const Games::NotFoundError &)
        {
            LOG_WARNING("Unknown game '%s'") << idKey;
            return false;
        }
    }

    DE_ASSERT(!game->isNull());

    LOG_MSG("") << game->description();

    return true;
}

void Game::consoleRegister() //static
{
    C_CMD("inspectgame", "", InspectGame);
    C_CMD("inspectgame", "s", InspectGame);
}

NullGame::NullGame()
    : Game("" /*null*/, Record::withMembers(DEF_CONFIG_DIR, "doomsday",
                                            DEF_TITLE,      "null-game",
                                            DEF_AUTHOR,     "null-game"))
{}
