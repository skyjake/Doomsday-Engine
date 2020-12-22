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

#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/filesys/fs_main.h>
#include <doomsday/manifest.h>
#include <doomsday/res/bundles.h>

#include <de/app.h>
#include <de/log.h>
#include <de/loop.h>
#include <de/dscript.h>
#include <de/charsymbols.h>
#include <algorithm>

using namespace de;

DE_PIMPL(Games)
, DE_OBSERVES(res::Bundles, Identify)
, public Lockable
{
    /// The actual collection.
    All games;

    /// Special "null-game" object for this collection.
    NullGame *nullGame;

    Hash<String, Game *> idLookup; // not owned, lower case

    Dispatch dispatch;
    Set<const Game *> lastCheckedPlayable; // determines when notification sent

    /**
     * Delegates game addition notifications to scripts.
     */
    class GameAdditionScriptAudience : DE_OBSERVES(Games, Addition)
    {
    public:
        void gameAdded(Game &game)
        {
            ArrayValue args;
            args << DictionaryValue() << TextValue(game.id());
            App::scriptSystem()["App"]["audienceForGameAddition"]
                    .array().callElements(args);
        }
    };

    GameAdditionScriptAudience scriptAudienceForGameAddition;

    Impl(Public *i) : Base(i), games(), nullGame(nullptr)
    {
        /*
         * One-time creation and initialization of the special "null-game"
         * object (activated once created).
         */
        nullGame = new NullGame;

        // Extend the native App module with a script audience for observing game addition.
        App::scriptSystem()["App"].addArray("audienceForGameAddition");

        audienceForAddition += scriptAudienceForGameAddition;
    }

    ~Impl()
    {
        clear();
        delete nullGame;
    }

    void clear()
    {
        DE_ASSERT(nullGame);

        deleteAll(games);
        games.clear();
        idLookup.clear();
    }

    void add(Game *game)
    {
        DE_ASSERT(game != nullptr);

        games.push_back(game);
        idLookup.insert(game->id().lower(), game);

        DoomsdayApp::bundles().audienceForIdentify() += this;

        DE_NOTIFY_PUBLIC(Addition, i)
        {
            i->gameAdded(*game);
        }
    }

    Game *findById(String id) const
    {
       if (id.beginsWith("doom-"))
        {
            // Originally, Freedoom and BFG variants used an inconsistently named ID.
            id = "doom1-" + id.substr(BytePos(5));
        }
        auto found = idLookup.find(id.lower());
        if (found != idLookup.end())
        {
            return found->second;
        }        
        return nullptr;
    }

    void dataBundlesIdentified()
    {
        if (!dispatch)
        {
            dispatch += [this]() { self().checkReadiness(); };
        }
    }

    DE_PIMPL_AUDIENCES(Addition, Readiness, Progress)
};

DE_AUDIENCE_METHODS(Games, Addition, Readiness, Progress)

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
    DE_GUARD(d);
    int count = 0;
    for (Game *game : d->games)
    {
        if (game->allStartupFilesFound())
        {
            count += 1;
        }
    }
    return count;
}

int Games::numPlayable(const String &family) const
{
    DE_GUARD(d);
    int count = 0;
    for (Game *game : d->games)
    {
        if (game->isPlayableWithDefaultPackages() && game->family() == family)
        {
            count++;
        }
    }
    return count;
}

const GameProfile *Games::firstPlayable() const
{
    DE_GUARD(d);
    for (Game *game : d->games)
    {
        if (game->profile().isPlayable()) return &game->profile();
    }
    return nullptr;
}

Game &Games::operator[](const String &id) const
{
    DE_GUARD(d);
    if (id.isEmpty())
    {
        return *d->nullGame;
    }
    if (auto *game = d->findById(id))
    {
        return *game;
    }
    /// @throw NotFoundError  The specified @a identityKey string is not associated with
    ///                       a game in the collection.
    throw NotFoundError("Games::operator[]", "No game exists with ID '" + id + "'");
}

bool Games::contains(const String &id) const
{
    DE_GUARD(d);
    return d->findById(id) != nullptr;
}

Game &Games::byIndex(int idx) const
{
    DE_GUARD(d);
    if (idx < 0 || idx > d->games.sizei())
    {
        /// @throw NotFoundError  No game is associated with index @a idx.
        throw NotFoundError("Games::byIndex", stringf("There is no Game at index %i", idx));
    }
    return *d->games[idx];
}

void Games::clear()
{
    DE_GUARD(d);
    d->clear();
}

Games::All Games::all() const
{
    DE_GUARD(d);
    return d->games;
}

int Games::collectAll(GameList &collected)
{
    DE_GUARD(d);
    int numFoundSoFar = collected.sizei();
    for (Game *game : d->games)
    {
        collected.push_back(GameListItem(game));
    }
    return collected.sizei() - numFoundSoFar;
}

Game &Games::defineGame(const String &id, const Record &parameters)
{
    DE_GUARD(d);
    LOG_AS("Games");

    // Game IDs must be unique. Ensure that is the case.
    if (d->idLookup.contains(id))
    {
        LOGDEV_WARNING("Ignored new game \"%s\", ID'%s' already in use")
                << parameters.gets(Game::DEF_TITLE) << id;
        throw Error("Games::defineGame", String("Duplicate game ID: ") + id);
    }

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

LoopResult Games::forAll(const std::function<LoopResult (Game &)>& callback) const
{
    for (Game *game : all())
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
            DE_FOR_OBSERVERS(ProgressAudience, i, self().audienceForProgress())
            {
                i->gameWorkerProgress(n * 200 / count() - 1);
            }

            LOG_RES_VERBOSE(_E(l) "  Game: " _E(.)_E(>) "%s - %s") << game.title() << game.author();
            LOG_RES_VERBOSE(_E(l) "  IdentityKey: " _E(.)_E(>)) << game.id();
            Game::printFiles(game, FF_STARTUP);

            LOG_RES_MSG(" " DE_CHAR_RIGHT_DOUBLEARROW " ") << game.statusAsText();
            ++n;

            return 0;
        });
        return LoopContinue;
    });
*/
    Set<const Game *> playable;
    bool changed = false;
    {
        DE_GUARD(d);
        forAll([&playable] (Game &game) {
            if (game.isPlayable()) playable.insert(&game);
            return LoopContinue;
        });
        changed = (playable != d->lastCheckedPlayable);
    }

    // Only notify when the set of playable games changes.
    if (changed)
    {
        DE_NOTIFY(Readiness, i)
        {
            i->gameReadinessUpdated();
        }

        DE_GUARD(d);
        d->lastCheckedPlayable = std::move(playable);
    }
}

/*void Games::forgetAllResources()
{
    for (Game *game : d->games)
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
    DE_UNUSED(src, argc, argv);

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
    found.sort();

    String list;

    int numCompleteGames = 0;
    DE_FOR_EACH_CONST(Games::GameList, i, found)
    {
        Game *game = i->game;
        bool isCurrent = (&DoomsdayApp::game() == game);

        if (!list.isEmpty()) list += "\n";

        list += Stringf(_E(0)
                       _E(Ta) "%s%s "
                       _E(Tb) "%s "
                       _E(Tc) _E(2) "%s " _E(i) "(%s)",
                isCurrent? _E(B) _E(b) :
                     !game->allStartupFilesFound()? _E(D) : "",
                isCurrent? "*" : !game->allStartupFilesFound()? "!" : " ",
                game->id().c_str(),
                game->title().c_str(),
                game->author().c_str());

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

