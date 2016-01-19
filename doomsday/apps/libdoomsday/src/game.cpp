/** @file game.cpp  Game mode configuration (metadata, resource files, etc...).
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

#include "doomsday/game.h"
#include "doomsday/games.h"
#include "doomsday/console/cmd.h"
#include "doomsday/filesys/file.h"
#include "doomsday/resource/manifest.h"
#include "doomsday/resource/resources.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/SavedSession"

#include <de/App>
#include <de/Error>
#include <de/Log>
#include <de/charsymbols.h>
#include <QtAlgorithms>

using namespace de;

static String const DEF_ID("ID");

String const Game::DEF_VARIANT_OF("variantOf");
String const Game::DEF_CONFIG_DIR("configDir");
String const Game::DEF_CONFIG_MAIN_PATH("mainConfig");
String const Game::DEF_CONFIG_BINDINGS_PATH("bindingsConfig");
String const Game::DEF_TITLE("title");
String const Game::DEF_AUTHOR("author");
String const Game::DEF_LEGACYSAVEGAME_NAME_EXP("legacySavegame.nameExp");
String const Game::DEF_LEGACYSAVEGAME_SUBFOLDER("legacySavegame.subfolder");
String const Game::DEF_MAPINFO_PATH("mapInfoPath");

DENG2_PIMPL(Game)
{
    pluginid_t pluginId = 0; ///< Unique identifier of the registering plugin.
    Record params;
    StringList requiredPackages; ///< Packages required for starting the game.

    Manifests manifests; ///< Required resource files (e.g., doomu.wad).

    //String title;        ///< Formatted default title, suitable for printing (e.g., "The Ultimate DOOM").
    //String author;       ///< Formatted default author suitable for printing (e.g., "id Software").

    //Path mainConfig;     ///< Config file name (e.g., "configs/doom/game.cfg").
    //Path bindingConfig;  ///< Control binding file name (set automatically).
    //Path mainMapInfo;    ///< Base relative path to the main MAPINFO definition data.

    //String legacySavegameNameExp;
    //String legacySavegameSubfolder;

    Instance(Public *i, Record const &parms)
        : Base(i)
        , params(parms)
    {
        // Define the optional parameters if needed.
        if(!params.has(DEF_CONFIG_MAIN_PATH))
        {
            params.set(DEF_CONFIG_MAIN_PATH, String("configs")/params.gets(DEF_CONFIG_DIR)/"game.cfg");
        }
        if(!params.has(DEF_CONFIG_BINDINGS_PATH))
        {
            params.set(DEF_CONFIG_BINDINGS_PATH, String("configs")/params.gets(DEF_CONFIG_DIR)/"player/bindings.cfg");
        }

        params.set(DEF_CONFIG_DIR, NativePath(params.gets(DEF_CONFIG_DIR)).expand().withSeparators('/'));
    }

    ~Instance()
    {
        qDeleteAll(manifests);
    }
};

Game::Game(String const &id, Record const &params)
    : AbstractGame(id)
    , d(new Instance(this, params))
{
    d->params.set(DEF_ID, id);
    setVariantOf(params.gets(DEF_VARIANT_OF, ""));
}

Game::~Game()
{}

void Game::setRequiredPackages(StringList const &packageIds)
{
    d->requiredPackages = packageIds;
}

StringList Game::requiredPackages() const
{
    return d->requiredPackages;
}

void Game::addManifest(ResourceManifest &manifest)
{
    // Ensure we don't add duplicates.
    Manifests::const_iterator found = d->manifests.find(manifest.resourceClass(), &manifest);
    if(found == d->manifests.end())
    {
        d->manifests.insert(manifest.resourceClass(), &manifest);
    }
}

bool Game::allStartupFilesFound() const
{
    for(String const &pkg : d->requiredPackages)
    {
        if(!App::packageLoader().isAvailable(pkg))
            return false;
    }

    foreach(ResourceManifest *manifest, d->manifests)
    {
        int const flags = manifest->fileFlags();

        if((flags & FF_STARTUP) && !(flags & FF_FOUND))
            return false;
    }
    return true;
}

Game::Status Game::status() const
{
    if(App_GameLoaded() && &DoomsdayApp::currentGame() == this)
    {
        return Loaded;
    }
    if(allStartupFilesFound())
    {
        return Complete;
    }
    return Incomplete;
}

String const &Game::statusAsText() const
{
    static String const statusTexts[] = {
        "Loaded",
        "Playable",
        "Not playable (incomplete resources)",
    };
    return statusTexts[int(status())];
}

String Game::description() const
{
    return String(_E(b) "%1 - %2\n" _E(.)
                  _E(l) "IdentityKey: " _E(.) "%3 "
                  _E(l) "PluginId: "    _E(.) "%4\n"
                  _E(D)_E(b) "Startup resources:\n" _E(.)_E(.) "%5\n"
                  _E(D)_E(b) "Other resources:\n" _E(.)_E(.) "%6\n"
                  _E(D)_E(b) "Status: " _E(.) "%7")
            .arg(title())
            .arg(author())
            .arg(id())
            .arg(int(pluginId()))
            .arg(filesAsText(FF_STARTUP))
            .arg(filesAsText(0, false))
            .arg(statusAsText());
}

pluginid_t Game::pluginId() const
{
    return d->pluginId;
}

void Game::setPluginId(pluginid_t newId)
{
    d->pluginId = newId;
}

String Game::logoImageId() const
{
    String idKey = id();

    /// @todo The name of the plugin should be accessible via the plugin loader.
    String plugName;
    if(idKey.contains("heretic"))
    {
        plugName = "libheretic";
    }
    else if(idKey.contains("hexen"))
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
    return d->params[DEF_LEGACYSAVEGAME_NAME_EXP];
}

String Game::legacySavegamePath() const
{
    NativePath nativeSavePath = Resources::get().nativeSavePath();
    if(nativeSavePath.isEmpty()) return "";
    if(isNull()) return "";

    if(App::commandLine().has("-savedir"))
    {
        // A custom path. The savegames are in the root of this folder.
        return nativeSavePath;
    }

    // The default save path. The savegames are in a game-specific folder.
    if(!d->params.gets(DEF_LEGACYSAVEGAME_SUBFOLDER, "").isEmpty())
    {
        return App::app().nativeHomePath() / d->params.gets(DEF_LEGACYSAVEGAME_SUBFOLDER) / id();
    }

    return "";
}

Path Game::mainConfig() const
{
    return d->params.gets(DEF_CONFIG_MAIN_PATH);
}

Path Game::bindingConfig() const
{
    return d->params.gets(DEF_CONFIG_BINDINGS_PATH);
}

Path Game::mainMapInfo() const
{
    return d->params.gets(DEF_MAPINFO_PATH);
}

String Game::title() const
{
    return d->params.gets(DEF_TITLE);
}

String Game::author() const
{
    return d->params.gets(DEF_AUTHOR);
}

Game::Manifests const &Game::manifests() const
{
    return d->manifests;
}

bool Game::isRequiredFile(File1 &file)
{
    // If this resource is from a container we must use the path of the
    // root file container instead.
    File1 &rootFile = file;
    while(rootFile.isContained())
    { rootFile = rootFile.container(); }

    String absolutePath = rootFile.composePath();
    bool isRequired = false;

    for(Manifests::const_iterator i = d->manifests.find(RC_PACKAGE);
        i != d->manifests.end() && i.key() == RC_PACKAGE; ++i)
    {
        ResourceManifest &manifest = **i;
        if(!(manifest.fileFlags() & FF_STARTUP)) continue;

        if(!manifest.resolvedPath(true/*try locate*/).compare(absolutePath, Qt::CaseInsensitive))
        {
            isRequired = true;
            break;
        }
    }

    return isRequired;
}

void Game::addResource(resourceclassid_t classId, dint rflags,
                       char const *names, void const *params)
{
    if(!VALID_RESOURCECLASSID(classId))
    {
        throw Error("Game::addResource",
                    "Unknown resource class " + QString::number(classId));
    }

    if(!names || !names[0])
    {
        throw Error("Game::addResource", "Invalid name argument");
    }

    // Construct and attach the new resource record.
    ResourceManifest *manifest = new ResourceManifest(classId, rflags);
    addManifest(*manifest);

    // Add the name list to the resource record.
    QStringList nameList = String(names).split(";", QString::SkipEmptyParts);
    foreach(QString const &nameRef, nameList)
    {
        manifest->addName(nameRef);
    }

    if(params && classId == RC_PACKAGE)
    {
        // Add the identityKey list to the resource record.
        QStringList idKeys = String((char const *) params).split(";", QString::SkipEmptyParts);
        foreach(QString const &idKeyRef, idKeys)
        {
            manifest->addIdentityKey(idKeyRef);
        }
    }
}

Record const &Game::objectNamespace() const
{
    return d->params;
}

Record &Game::objectNamespace()
{
    return d->params;
}

void Game::printBanner(Game const &game)
{
    LOG_MSG(_E(R) "\n");
    LOG_MSG(_E(1)) << game.title();
    LOG_MSG(_E(R) "\n");
}

String Game::filesAsText(int rflags, bool withStatus) const
{
    String text;

    // Group output by resource class.
    Manifests const &manifs = manifests();
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourceclassid_t const classId = resourceclassid_t(i);
        for(Manifests::const_iterator i = manifs.find(classId);
            i != manifs.end() && i.key() == classId; ++i)
        {
            ResourceManifest &manifest = **i;
            if(rflags >= 0 && (rflags & manifest.fileFlags()))
            {
                bool const resourceFound = (manifest.fileFlags() & FF_FOUND) != 0;

                if(!text.isEmpty()) text += "\n" _E(0);

                if(withStatus)
                {
                    text += (resourceFound? " - " : _E(1) " ! " _E(.));
                }

                // Format the resource name list.
                text += String(_E(>) "%1%2")
                        .arg(!resourceFound? _E(D) : "")
                        .arg(manifest.names().join(_E(l) " or " _E(.)));

                if(withStatus)
                {
                    text += String(": ") + _E(>) + (!resourceFound? _E(b) "missing " _E(.) : "");
                    if(resourceFound)
                    {
                        text += String(_E(C) "\"%1\"" _E(.)).arg(NativePath(manifest.resolvedPath(false/*don't try to locate*/)).expand().pretty());
                    }
                    text += _E(<);
                }

                text += _E(<);
            }
        }
    }

    if(text.isEmpty()) return " none";

    return text;
}

void Game::printFiles(Game const &game, int rflags, bool printStatus)
{
    LOG_RES_MSG("") << game.filesAsText(rflags, printStatus);
}

D_CMD(InspectGame)
{
    DENG2_UNUSED(src);

    Game *game = 0;
    if(argc < 2)
    {
        // No game identity key was specified - assume the current game.
        if(!App_GameLoaded())
        {
            LOG_WARNING("No game is currently loaded.\nPlease specify the identifier of the game to inspect.");
            return false;
        }
        game = &DoomsdayApp::currentGame();
    }
    else
    {
        String idKey = argv[1];
        try
        {
            game = &DoomsdayApp::games()[idKey];
        }
        catch(Games::NotFoundError const &)
        {
            LOG_WARNING("Unknown game '%s'") << idKey;
            return false;
        }
    }

    DENG2_ASSERT(!game->isNull());

    LOG_MSG("") << game->description();

/*
    LOG_MSG(_E(b) "%s - %s") << game->title() << game->author();
    LOG_MSG(_E(l) "IdentityKey: " _E(.) _E(i) "%s " _E(.)
            _E(l) "PluginId: "    _E(.) _E(i) "%s")
        << game->identityKey() << int(game->pluginId());

    LOG_MSG(_E(D) "Startup resources:");
    Game::printFiles(*game, FF_STARTUP);

    LOG_MSG(_E(D) "Other resources:");
    Game::printFiles(*game, 0, false);

    LOG_MSG(_E(D) "Status: " _E(.) _E(b)) << game->statusAsText();
    */

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
