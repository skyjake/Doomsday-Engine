/** @file games.cpp Specialized collection for a set of logical Games.
 *
 * @authors Copyright &copy; 2012-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de_console.h"
#include "de_filesys.h"
#include "filesys/manifest.h"
#include "resource/zip.h"
#include <QtAlgorithms>

#include "games.h"

namespace de {

DENG2_PIMPL(Games)
{
    /// The actual collection.
    Games::All games;

    /// Currently active game (in this collection).
    Game *currentGame;

    /// Special "null-game" object for this collection.
    NullGame *nullGame;

    Instance(Public &a)
        : Base(a), games(), currentGame(0), nullGame(0)
    {}

    ~Instance()
    {
        qDeleteAll(games);

        if(nullGame)
        {
            delete nullGame;
            nullGame = 0;
        }
        currentGame = 0;
    }
};

Games::Games() : d(new Instance(*this))
{
    /*
     * One-time creation and initialization of the special "null-game"
     * object (activated once created).
     */
    d->currentGame = d->nullGame = new NullGame();
}

Games::~Games()
{
    delete d;
}

Game &Games::current() const
{
    return *d->currentGame;
}

Game &Games::nullGame() const
{
    return *d->nullGame;
}

void Games::setCurrent(Game &game)
{
    // Ensure the specified game is actually in this collection (NullGame is implicitly).
    DENG_ASSERT(isNullGame(game) || id(game) > 0);
    d->currentGame = &game;
}

int Games::numPlayable() const
{
    int count = 0;
    foreach(Game *game, d->games)
    {
        if(!game->allStartupFilesFound()) continue;
        ++count;
    }
    return count;
}

Game *Games::firstPlayable() const
{
    foreach(Game *game, d->games)
    {
        if(game->allStartupFilesFound()) return game;
    }
    return NULL;
}

gameid_t Games::id(Game &game) const
{
    if(&game == d->nullGame) return 0; // Invalid id.
    int idx = d->games.indexOf(&game);
    if(idx < 0)
    {
        /// @throw NotFoundError  The specified @a game is not a member of the collection.
        throw NotFoundError("Games::id", QString("Game %p is not a member of the collection").arg(de::dintptr(&game)));
    }
    return gameid_t(idx+1);
}

Game &Games::byId(gameid_t gameId) const
{
    if(gameId <= 0 || gameId > d->games.count())
    {
        /// @throw NotFoundError  The specified @a gameId is out of range.
        throw NotFoundError("Games::byId", QString("There is no Game with id %i").arg(gameId));
    }
    return *d->games[gameId-1];
}

Game &Games::byIdentityKey(char const *identityKey) const
{
    if(identityKey && identityKey[0])
    {
        foreach(Game *game, d->games)
        {
            if(!Str_CompareIgnoreCase(game->identityKey(), identityKey))
                return *game;
        }
    }
    /// @throw NotFoundError  The specified @a identityKey string is not associated with a game in the collection.
    throw NotFoundError("Games::byIdentityKey",
                        QString("There is no Game with identity key \"%1\"").arg(identityKey));
}

Game &Games::byIndex(int idx) const
{
    if(idx < 0 || idx > d->games.count())
    {
        /// @throw NotFoundError  No game is associated with index @a idx.
        throw NotFoundError("Games::byIndex", QString("There is no Game at index %i").arg(idx));
    }
    return *d->games[idx];
}

Games::All const &Games::all() const
{
    return d->games;
}

int Games::collectAll(GameList &collected)
{
    int numFoundSoFar = collected.count();
    foreach(Game *game, d->games)
    {
        collected.push_back(GameListItem(game));
    }
    return collected.count() - numFoundSoFar;
}

void Games::add(Game &game)
{
    // Already a member of the collection?
    if(d->games.indexOf(&game) >= 0) return;

    d->games.push_back(&game);
}

void Games::locateStartupResources(Game &game)
{
    Game *oldCurrrentGame = d->currentGame;
    if(d->currentGame != &game)
    {
        /// @attention Kludge: Temporarily switch Game.
        d->currentGame = &game;
        DD_ExchangeGamePluginEntryPoints(game.pluginId());

        // Re-init the filesystem subspace schemes using the search paths of this Game.
        App_FileSystem()->resetAllSchemes();
    }

    foreach(Manifest *manifest, game.manifests())
    {
        // We are only interested in startup resources at this time.
        if(!(manifest->fileFlags() & FF_STARTUP)) continue;

        manifest->locateFile();
    }

    if(d->currentGame != oldCurrrentGame)
    {
        // Kludge end - Restore the old Game.
        d->currentGame = oldCurrrentGame;
        DD_ExchangeGamePluginEntryPoints(oldCurrrentGame->pluginId());

        // Re-init the filesystem subspace schemes using the search paths of this Game.
        App_FileSystem()->resetAllSchemes();
    }
}

static int locateAllResourcesWorker(void *parameters)
{
    Games *games = (Games *) parameters;
    int n = 0;
    foreach(Game *game, games->all())
    {
        Con_Message("Locating \"%s\"...\n", Str_Text(game->title()));

        games->locateStartupResources(*game);
        Con_SetProgress((n + 1) * 200 / games->count() - 1);

        VERBOSE( Game::print(*game, PGF_LIST_STARTUP_RESOURCES|PGF_STATUS) )
        ++n;
    }
    BusyMode_WorkerEnd();
    return 0;
}

void Games::locateAllResources()
{
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                locateAllResourcesWorker, (void *)this, "Locating game resources...");
}

} // namespace de

D_CMD(ListGames)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    de::Games &games = App_Games();
    if(!games.count())
    {
        Con_Printf("No Registered Games.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Registered Games:\n");
    Con_Printf("Key: '!'= Incomplete/Not playable '*'= Loaded\n");
    Con_PrintRuler();

    de::Games::GameList found;
    games.collectAll(found);
    // Sort so we get a nice alphabetical list.
    qSort(found.begin(), found.end());

    int numCompleteGames = 0;
    DENG2_FOR_EACH_CONST(de::Games::GameList, i, found)
    {
        de::Game *game = i->game;

        Con_Printf(" %s %-16s %s (%s)\n",      game->isCurrent()? "*" :
                                   !game->allStartupFilesFound()? "!" : " ",
                   Str_Text(game->identityKey()), Str_Text(game->title()),
                   Str_Text(game->author()));

        if(game->allStartupFilesFound())
            numCompleteGames++;
    }

    Con_PrintRuler();
    Con_Printf("%i of %i games playable.\n", numCompleteGames, games.count());
    Con_Printf("Use the 'load' command to load a game. For example: \"load gamename\".\n");

    return true;
}
