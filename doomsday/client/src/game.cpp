/** @file game.cpp  Game mode configuration (metadata, resource files, etc...).
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

#include "de_base.h"
#include "game.h"

#include "con_main.h"
#include "filesys/manifest.h"

#include <de/Error>
#include <de/Log>
#include <QtAlgorithms>

namespace de {

DENG2_PIMPL(Game)
{
    pluginid_t pluginId; ///< Unique identifier of the registering plugin.
    Manifests manifests; ///< Required resource files (e.g., doomu.wad).
    String identityKey;  ///< Unique game mode identifier (e.g., "doom1-ultimate").

    String title;        ///< Formatted default title, suitable for printing (e.g., "The Ultimate DOOM").
    String author;       ///< Formatted default author suitable for printing (e.g., "id Software").

    Path mainConfig;     ///< Config file name (e.g., "configs/doom/game.cfg").
    Path bindingConfig;  ///< Control binding file name (set automatically).

    Instance(Public &a, String const &identityKey, Path const &configDir,
             String const &title, String const &author)
        : Base(a)
        , pluginId     (0) // Not yet assigned.
        , identityKey  (identityKey)
        , title        (title)
        , author       (author)
        , mainConfig   (Path("configs") / configDir / "game.cfg")
        , bindingConfig(Path("configs") / configDir / "player/bindings.cfg")
    {}

    ~Instance()
    {
        qDeleteAll(manifests);
    }
};

Game::Game(String const &identityKey, Path const &configDir, String const &title,
    String const &author)
    : game::Game(identityKey)
    , d(new Instance(*this, identityKey, configDir, title, author))
{}

Game::~Game()
{}

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
    if(App_GameLoaded() && &App_CurrentGame() == this)
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
        "Complete/Playable",
        "Incomplete/Not playable",
    };
    return statusTexts[int(status())];
}

pluginid_t Game::pluginId() const
{
    return d->pluginId;
}

void Game::setPluginId(pluginid_t newId)
{
    d->pluginId = newId;
}

String const &Game::identityKey() const
{
    return d->identityKey;
}

Path const &Game::mainConfig() const
{
    return d->mainConfig;
}

Path const &Game::bindingConfig() const
{
    return d->bindingConfig;
}

String const &Game::title() const
{
    return d->title;
}

String const &Game::author() const
{
    return d->author;
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

Game *Game::fromDef(GameDef const &def)
{
    return new Game(def.identityKey, NativePath(def.configDir).expand().withSeparators('/'),
                    def.defaultTitle, def.defaultAuthor);
}

void Game::printBanner(Game const &game)
{
    LOG_MSG(_E(R) "\n");
    LOG_MSG(_E(1)) << game.title();
    LOG_MSG(_E(R) "\n");
}

void Game::printFiles(Game const &game, int rflags, bool printStatus)
{
    int numPrinted = 0;

    // Group output by resource class.
    Manifests const &manifests = game.manifests();
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        resourceclassid_t const classId = resourceclassid_t(i);
        for(Manifests::const_iterator i = manifests.find(classId);
            i != manifests.end() && i.key() == classId; ++i)
        {
            ResourceManifest &manifest = **i;
            if(rflags >= 0 && (rflags & manifest.fileFlags()))
            {
                bool const resourceFound = (manifest.fileFlags() & FF_FOUND) != 0;

                String text;
                if(printStatus)
                {
                    text += (resourceFound? "   " : " ! ");
                }

                // Format the resource name list.
                text += manifest.names().join(" or ");

                if(printStatus)
                {
                    text += String(" - ") + (resourceFound? "found" : "missing");
                    if(resourceFound)
                    {
                        text += String(" ") + NativePath(manifest.resolvedPath(false/*don't try to locate*/)).expand().pretty();
                    }
                }

                LOG_MSG("") << text;
                numPrinted += 1;
            }
        }
    }

    if(numPrinted == 0)
    {
        LOG_MSG(" None");
    }
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
            LOG_WARNING("No game is currently loaded.\nPlease specify the identity-key of the game to inspect.");
            return false;
        }
        game = &App_CurrentGame();
    }
    else
    {
        String idKey = argv[1];
        try
        {
            game = &App_Games().byIdentityKey(idKey);
        }
        catch(Games::NotFoundError const &)
        {
            LOG_WARNING("Unknown game '%s'.") << idKey;
            return false;
        }
    }

    DENG2_ASSERT(!game->isNull());

    LOG_MSG(_E(1) "%s - %s") << game->title() << game->author();
    LOG_MSG(_E(l) "IdentityKey: " _E(.) _E(i) "%s " _E(.)
            _E(l) "PluginId: "    _E(.) _E(i) "%s")
        << game->identityKey() << int(game->pluginId());

    LOG_MSG(_E(D) "Startup resources:");
    Game::printFiles(*game, FF_STARTUP);

    LOG_MSG(_E(D) "Other resources:");
    Game::printFiles(*game, 0, false);

    LOG_MSG(_E(D) "Status: " _E(.) _E(1)) << game->statusAsText();

    return true;
}

void Game::consoleRegister() //static
{
    C_CMD("inspectgame", "", InspectGame);
    C_CMD("inspectgame", "s", InspectGame);
}

NullGame::NullGame() : Game("" /*null*/, "doomsday", "null-game", "null-game")
{}

} // namespace de
