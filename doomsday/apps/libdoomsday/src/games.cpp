/** @file games.cpp  Specialized collection for a set of logical Games.
 *
 * @authors Copyright © 2012-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/games.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/console/cmd.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/resource/manifest.h>
#include <doomsday/resource/bundles.h>

#include <de/App>
#include <de/ArrayValue>
#include <de/DictionaryValue>
#include <de/Log>
#include <de/Loop>
#include <de/ScriptSystem>
#include <de/TextValue>
#include <de/charsymbols.h>
#include <QtAlgorithms>

using namespace de;

DENG2_PIMPL(Games)
, DENG2_OBSERVES(res::Bundles, Identify)
{
    /// The actual collection.
    All games;

    /// Special "null-game" object for this collection.
    NullGame *nullGame;

    QHash<String, Game *> idLookup; // not owned, lower case

    LoopCallback mainCall;
    QSet<Game const *> lastCheckedPlayable; // determines when notification sent

    /**
     * Delegates game addition notifications to scripts.
     */
    class GameAdditionScriptAudience : DENG2_OBSERVES(Games, Addition)
    {
    public:
        void gameAdded(Game &game)
        {
            ArrayValue args;
            args << DictionaryValue() << TextValue(game.id());
            App::scriptSystem().nativeModule("App")["audienceForGameAddition"]
                    .array().callElements(args);
        }
    };

    GameAdditionScriptAudience scriptAudienceForGameAddition;

    Impl(Public *i) : Base(i), games(), nullGame(0)
    {
        /*
         * One-time creation and initialization of the special "null-game"
         * object (activated once created).
         */
        nullGame = new NullGame;

        // Extend the native App module with a script audience for observing game addition.
        App::scriptSystem().nativeModule("App").addArray("audienceForGameAddition");

        audienceForAddition += scriptAudienceForGameAddition;
    }

    ~Impl()
    {
        clear();
        delete nullGame;
    }

    void clear()
    {
        DENG2_ASSERT(nullGame != 0);

        qDeleteAll(games);
        games.clear();
        idLookup.clear();
    }

    void add(Game *game)
    {
        DENG2_ASSERT(game != nullptr);

        games.push_back(game);
        idLookup.insert(game->id().toLower(), game);

        DoomsdayApp::bundles().audienceForIdentify() += this;

        DENG2_FOR_PUBLIC_AUDIENCE2(Addition, i)
        {
            i->gameAdded(*game);
        }
    }

    Game *findById(String id) const
    {
        auto found = idLookup.constFind(id.toLower());
        if (found != idLookup.constEnd())
        {
            return found.value();
        }
        return nullptr;
    }

    void dataBundlesIdentified()
    {
        if (!mainCall)
        {
            mainCall.enqueue([this] () { self().checkReadiness(); });
        }
    }

    DENG2_PIMPL_AUDIENCE(Addition)
    DENG2_PIMPL_AUDIENCE(Readiness)
    DENG2_PIMPL_AUDIENCE(Progress)
};

DENG2_AUDIENCE_METHOD(Games, Addition)
DENG2_AUDIENCE_METHOD(Games, Readiness)
DENG2_AUDIENCE_METHOD(Games, Progress)

Games::Games() : d(new Impl(this))
{}

Games &Games::get() // static
{
    return DoomsdayApp::games();
}

Game &Games::nullGame() // static
{
    return *get().d->nullGame;
}

int Games::numPlayable() const
{
    int count = 0;
    foreach (Game *game, d->games)
    {
        if (game->allStartupFilesFound())
        {
            count += 1;
        }
    }
    return count;
}

GameProfile const *Games::firstPlayable() const
{
    foreach (Game *game, d->games)
    {
        if (game->profile().isPlayable()) return &game->profile();
    }
    return nullptr;
}

Game &Games::operator [] (String const &id) const
{
    if (id.isEmpty()) return *d->nullGame;

    if (auto *game = d->findById(id))
    {
        return *game;
    }

    /// @throw NotFoundError  The specified @a identityKey string is not associated with a game in the collection.
    throw NotFoundError("Games::operator []", "No game exists with ID '" + id + "'");
}

bool Games::contains(String const &id) const
{
    return d->findById(id) != nullptr;
}

Game &Games::byIndex(int idx) const
{
    if (idx < 0 || idx > d->games.count())
    {
        /// @throw NotFoundError  No game is associated with index @a idx.
        throw NotFoundError("Games::byIndex", QString("There is no Game at index %i").arg(idx));
    }
    return *d->games[idx];
}

void Games::clear()
{
    d->clear();
}

Games::All const &Games::all() const
{
    return d->games;
}

int Games::collectAll(GameList &collected)
{
    int numFoundSoFar = collected.count();
    foreach (Game *game, d->games)
    {
        collected.push_back(GameListItem(game));
    }
    return collected.count() - numFoundSoFar;
}

Game &Games::defineGame(String const &id, Record const &parameters)
{
    LOG_AS("Games");

    // Game IDs must be unique. Ensure that is the case.
    try
    {
        /// @todo Check a hash. -jk
        /*Game &game =*/ (*this)[id];
        LOGDEV_WARNING("Ignored new game \"%s\", ID'%s' already in use")
                << parameters.gets(Game::DEF_TITLE) << id;
        throw Error("Games::defineGame", String("Duplicate game ID: ") + id);
    }
    catch (Games::NotFoundError const &)
    {} // Ignore the error.

    // Add this game to our records.
    Game *game = new Game(id, parameters);
    game->setPluginId(DoomsdayApp::plugins().activePluginId());
    d->add(game);
    return *game;
}

/*
void Games::locateStartupResources(Game &game)
{
    Game *oldCurrentGame = &DoomsdayApp::currentGame();

    if (oldCurrentGame != &game)
    {
        /// @attention Kludge: Temporarily switch Game.
        DoomsdayApp::setGame(game);
        DoomsdayApp::plugins().exchangeGameEntryPoints(game.pluginId());

        // Re-init the filesystem subspace schemes using the search paths of this Game.
        App_FileSystem().resetAllSchemes();
    }

    foreach (ResourceManifest *manifest, game.manifests())
    {
        // We are only interested in startup resources at this time.
        if (manifest->fileFlags() & FF_STARTUP)
        {
            manifest->locateFile();
        }
    }

    if (oldCurrentGame != &game)
    {
        // Kludge end - Restore the old Game.
        DoomsdayApp::setGame(*oldCurrentGame);
        DoomsdayApp::plugins().exchangeGameEntryPoints(oldCurrentGame->pluginId());

        // Re-init the filesystem subspace schemes using the search paths of this Game.
        App_FileSystem().resetAllSchemes();
    }
}
*/

LoopResult Games::forAll(std::function<LoopResult (Game &)> callback) const
{
    foreach (Game *game, all())
    {
        if (auto result = callback(*game))
        {
            return result;
        }
    }
    return LoopContinue;
}

void Games::checkReadiness()
{
    /*
    int n = 1;
    DoomsdayApp::busyMode().runNewTaskWithName(
                BUSYF_STARTUP | BUSYF_PROGRESS_BAR,
                "Locating game resources...", [this, &n] (void *)
    {
        forAll([this, &n] (Game &game)
        {
            LOG_RES_MSG("Locating " _E(b) "\"%s\"" _E(.) "...") << game.title();

            locateStartupResources(game);

            Games &self = *this; // MSVC 2013 cannot figure it out inside the lambda...
            DENG2_FOR_EACH_OBSERVER(ProgressAudience, i, self().audienceForProgress())
            {
                i->gameWorkerProgress(n * 200 / count() - 1);
            }

            LOG_RES_VERBOSE(_E(l) "  Game: " _E(.)_E(>) "%s - %s") << game.title() << game.author();
            LOG_RES_VERBOSE(_E(l) "  IdentityKey: " _E(.)_E(>)) << game.id();
            Game::printFiles(game, FF_STARTUP);

            LOG_RES_MSG(" " DENG2_CHAR_RIGHT_DOUBLEARROW " ") << game.statusAsText();
            ++n;

            return 0;
        });
        return LoopContinue;
    });
*/

    QSet<Game const *> playable;
    forAll([this, &playable] (Game &game)
    {
        if (game.isPlayable()) playable.insert(&game);
        return LoopContinue;
    });

    // Only notify when the set of playable games changes.
    if (playable != d->lastCheckedPlayable)
    {
        DENG2_FOR_AUDIENCE2(Readiness, i)
        {
            i->gameReadinessUpdated();
        }
    }
    d->lastCheckedPlayable = playable;
}

/*void Games::forgetAllResources()
{
    foreach (Game *game, d->games)
    {
        foreach (ResourceManifest *manifest, game->manifests())
        {
            if (manifest->fileFlags() & FF_STARTUP)
            {
                manifest->forgetFile();
            }
        }
    }
}*/

D_CMD(ListGames)
{
    DENG2_UNUSED3(src, argc, argv);

    Games &games = DoomsdayApp::games();
    if (!games.count())
    {
        LOG_MSG("No games are currently registered.");
        return true;
    }

    LOG_MSG(_E(b) "Registered Games:");
    LOG_VERBOSE("Key: %s'!'=Incomplete/Not playable %s'*'=Loaded")
            << _E(>) _E(D) << _E(B);

    LOG_MSG(_E(R) "\n");

    Games::GameList found;
    games.collectAll(found);
    // Sort so we get a nice alphabetical list.
    qSort(found.begin(), found.end());

    String list;

    int numCompleteGames = 0;
    DENG2_FOR_EACH_CONST(Games::GameList, i, found)
    {
        Game *game = i->game;
        bool isCurrent = (&DoomsdayApp::game() == game);

        if (!list.isEmpty()) list += "\n";

        list += String(_E(0)
                       _E(Ta) "%1%2 "
                       _E(Tb) "%3 "
                       _E(Tc) _E(2) "%4 " _E(i) "(%5)")
                .arg(isCurrent? _E(B) _E(b) :
                     !game->allStartupFilesFound()? _E(D) : "")
                .arg(isCurrent? "*" : !game->allStartupFilesFound()? "!" : " ")
                .arg(game->id())
                .arg(game->title())
                .arg(game->author());

        if (game->allStartupFilesFound())
        {
            numCompleteGames++;
        }
    }
    LOG_MSG("%s") << list;

    LOG_MSG(_E(R) "\n");
    LOG_MSG("%i of %i games are playable") << numCompleteGames << games.count();
    LOG_SCR_MSG("Use the " _E(b) "load" _E(.) " command to load a game, for example: \"load gamename\"");

    return true;
}

void Games::consoleRegister() //static
{
    C_CMD("listgames", "", ListGames);

    Game::consoleRegister();
}

