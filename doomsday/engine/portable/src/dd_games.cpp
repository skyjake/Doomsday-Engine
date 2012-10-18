/**
 * @file dd_games.cpp
 * Game collection. @ingroup core
 *
 * @author Copyright &copy; 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2012 Daniel Swanson <danij@dengine.net>
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
#include "dd_games.h"

#include "abstractresource.h"
#include "zip.h"

namespace de {

static bool validateResource(AbstractResource* rec);

struct GameCollection::Instance
{
    GameCollection& self;

    /// The actual collection.
    GameCollection::Games games;

    /// Currently active game (in this collection).
    Game* currentGame;

    /// Special "null-game" object for this collection.
    NullGame* nullGame;

    Instance(GameCollection& d)
        : self(d), games(), currentGame(0), nullGame(0)
    {}

    ~Instance()
    {
        DENG2_FOR_EACH(i, games, GameCollection::Games::const_iterator)
        {
            delete *i;
        }

        if(nullGame)
        {
            delete nullGame;
            nullGame = NULL;
        }
        currentGame = NULL;
    }
};

GameCollection::GameCollection()
{
    d = new Instance(*this);

    /**
     * One-time creation and initialization of the special "null-game"
     * object (activated once created).
     */
    ddstring_t dataPath; Str_InitStd(&dataPath);
    Str_Set(&dataPath, DD_BASEPATH_DATA);
    Str_Strip(&dataPath);
    F_FixSlashes(&dataPath, &dataPath);
    F_ExpandBasePath(&dataPath, &dataPath);
    F_AppendMissingSlash(&dataPath);

    ddstring_t defsPath; Str_InitStd(&defsPath);
    Str_Set(&defsPath, DD_BASEPATH_DEFS);
    Str_Strip(&defsPath);
    F_FixSlashes(&defsPath, &defsPath);
    F_ExpandBasePath(&defsPath, &defsPath);
    F_AppendMissingSlash(&defsPath);

    d->currentGame = d->nullGame = new NullGame(&dataPath, &defsPath);

    Str_Free(&defsPath);
    Str_Free(&dataPath);
}

GameCollection::~GameCollection()
{   
    delete d;
}

Game& GameCollection::currentGame() const
{
    return *d->currentGame;
}

Game& GameCollection::nullGame() const
{
    return *d->nullGame;
}

GameCollection& GameCollection::setCurrentGame(Game& game)
{
    // Ensure the specified game is actually in this collection (NullGame is implicitly).
    DENG_ASSERT(isNullGame(game) || id(game) > 0);
    d->currentGame = &game;
    return *this;
}

int GameCollection::numPlayable() const
{
    int count = 0;
    DENG2_FOR_EACH(i, d->games, Games::const_iterator)
    {
        Game* game = *i;
        if(!game->allStartupResourcesFound()) continue;
        ++count;
    }
    return count;
}

Game* GameCollection::firstPlayable() const
{
    DENG2_FOR_EACH(i, d->games, Games::const_iterator)
    {
        Game* game = *i;
        if(game->allStartupResourcesFound()) return game;
    }
    return NULL;
}

int GameCollection::count() const
{
    return d->games.count();
}

gameid_t GameCollection::id(Game& game) const
{
    if(&game == d->nullGame) return 0; // Invalid id.
    int idx = d->games.indexOf(&game);
    if(idx < 0) throw NotFoundError("GameCollection::id", QString("Game %p is not part of this collection").arg(de::dintptr(&game)));
    return gameid_t(idx+1);
}

Game& GameCollection::byId(gameid_t gameId) const
{
    if(gameId <= 0 || gameId > d->games.count())
        throw NotFoundError("GameCollection::byId", QString("There is no Game with id %i").arg(gameId));
    return *d->games[gameId-1];
}

Game& GameCollection::byIdentityKey(char const* identityKey) const
{
    if(identityKey && identityKey[0])
    {
        DENG2_FOR_EACH(i, d->games, GameCollection::Games::const_iterator)
        {
            Game* game = *i;
            if(!Str_CompareIgnoreCase(&game->identityKey(), identityKey))
                return *game;
        }
    }
    throw NotFoundError("GameCollection::byIdentityKey",
                        QString("There is no Game with identity key \"%1\"").arg(identityKey));
}

Game& GameCollection::byIndex(int idx) const
{
    if(idx < 0 || idx > d->games.count())
        throw NotFoundError("GameCollection::byIndex", QString("There is no Game at index %i").arg(idx));
    return *d->games[idx];
}

GameCollection::Games const& GameCollection::games() const
{
    return d->games;
}

int GameCollection::findAll(GameList& found)
{
    int numFoundSoFar = found.count();
    DENG2_FOR_EACH(i, d->games, Games::const_iterator)
    {
        found.push_back(GameListItem(*i));
    }
    return found.count() - numFoundSoFar;
}

GameCollection& GameCollection::add(Game& game)
{
    if(d->games.indexOf(&game) < 0)
    {
        d->games.push_back(&game);
    }
    return *this;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeWAD(char const* filePath, void* parameters)
{
    lumpnum_t auxLumpBase = App_FileSystem()->openAuxiliary(filePath);
    bool result = false;

    if(auxLumpBase >= 0)
    {
        // Ensure all identity lumps are present.
        if(parameters)
        {
            ddstring_t const* const* lumpNames = (ddstring_t const* const*) parameters;
            result = true;
            for(; result && *lumpNames; lumpNames++)
            {
                lumpnum_t lumpNum = App_FileSystem()->lumpNumForName(Str_Text(*lumpNames));
                if(lumpNum < 0)
                {
                    result = false;
                }
            }
        }
        else
        {
            // Matched.
            result = true;
        }

        F_CloseAuxiliary();
    }
    return result;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeZIP(char const* filePath, void* parameters)
{
    DENG_UNUSED(parameters);
    try
    {
        DFile& hndl = App_FileSystem()->openFile(filePath, "rbf");
        bool result = Zip::recognise(hndl);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem()->closeFile(hndl);
        return result;
    }
    catch(FS1::NotFoundError const&)
    {} // Ignore error.
    return false;
}

/// @todo This logic should be encapsulated by AbstractResource.
static bool validateResource(AbstractResource* rec)
{
    DENG_ASSERT(rec);
    bool validated = false;

    if(AbstractResource_ResourceClass(rec) == RC_PACKAGE)
    {
        Uri* const* uriList = AbstractResource_SearchPaths(rec);
        Uri* const* ptr;
        int idx = 0;
        for(ptr = uriList; *ptr; ptr++, idx++)
        {
            ddstring_t const* path = AbstractResource_ResolvedPathWithIndex(rec, idx, true/*locate resources*/);
            if(!path) continue;

            if(recognizeWAD(Str_Text(path), (void*)AbstractResource_IdentityKeys(rec)))
            {
                validated = true;
                break;
            }
            if(recognizeZIP(Str_Text(path), (void*)AbstractResource_IdentityKeys(rec)))
            {
                validated = true;
                break;
            }
        }
    }
    else
    {
        // Other resource types are not validated.
        validated = true;
    }

    AbstractResource_MarkAsFound(rec, validated);
    return validated;
}

GameCollection& GameCollection::locateStartupResources(Game& game)
{
    Game* oldGame = d->currentGame;
    if(d->currentGame != &game)
    {
        /// @attention Kludge: Temporarily switch Game.
        d->currentGame = &game;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }

    for(uint rclass = RESOURCECLASS_FIRST; rclass < RESOURCECLASS_COUNT; ++rclass)
    {
        AbstractResource* const* records = game.resources(resourceclass_t(rclass), 0);
        if(!records) continue;

        for(AbstractResource* const* i = records; *i; i++)
        {
            AbstractResource* rec = *i;

            // We are only interested in startup resources at this time.
            if(!(AbstractResource_ResourceFlags(rec) & RF_STARTUP)) continue;

            validateResource(rec);
        }
    }

    if(d->currentGame != oldGame)
    {
        // Kludge end - Restore the old Game.
        d->currentGame = oldGame;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }
    return *this;
}

static int locateAllResourcesWorker(void* parameters)
{
    GameCollection* gameCollection = (GameCollection*) parameters;
    int n = 0;
    DENG2_FOR_EACH(i, gameCollection->games(), GameCollection::Games::const_iterator)
    {
        Game* game = *i;

        Con_Message("Locating resources for \"%s\"...\n", Str_Text(&game->title()));

        gameCollection->locateStartupResources(*game);
        Con_SetProgress((n + 1) * 200 / gameCollection->count() - 1);

        VERBOSE( Game::print(*game, PGF_LIST_STARTUP_RESOURCES|PGF_STATUS) )
        ++n;
    }
    BusyMode_WorkerEnd();
    return 0;
}

GameCollection& GameCollection::locateAllResources()
{
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                locateAllResourcesWorker, (void*)this, "Locating game resources...");
    return *this;
}

} // namespace de

D_CMD(ListGames)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    de::GameCollection* games = reinterpret_cast<de::GameCollection*>(App_GameCollection());
    if(!games || !games->count())
    {
        Con_Printf("No Registered Games.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Registered Games:\n");
    Con_Printf("Key: '!'= Incomplete/Not playable '*'= Loaded\n");
    Con_PrintRuler();

    de::GameCollection::GameList found;
    games->findAll(found);
    // Sort so we get a nice alphabetical list.
    qSort(found.begin(), found.end());

    int numCompleteGames = 0;
    DENG2_FOR_EACH(i, found, de::GameCollection::GameList::const_iterator)
    {
        de::Game* game = i->game;

        Con_Printf(" %s %-16s %s (%s)\n", games->isCurrentGame(*game)? "*" :
                                   !game->allStartupResourcesFound()? "!" : " ",
                   Str_Text(&game->identityKey()), Str_Text(&game->title()),
                   Str_Text(&game->author()));

        if(game->allStartupResourcesFound())
            numCompleteGames++;
    }

    Con_PrintRuler();
    Con_Printf("%i of %i games playable.\n", numCompleteGames, games->count());
    Con_Printf("Use the 'load' command to load a game. For example: \"load gamename\".\n");

    return true;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::GameCollection*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::GameCollection const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::GameCollection* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::GameCollection const* self = TOINTERNAL_CONST(inst)

Game* GameCollection_CurrentGame(GameCollection* games)
{
    SELF(games);
    return reinterpret_cast<Game*>(&self->currentGame());
}

int GameCollection_NumPlayable(GameCollection* games)
{
    SELF(games);
    return self->numPlayable();
}

Game* GameCollection_FirstPlayable(GameCollection* games)
{
    SELF(games);
    return reinterpret_cast<Game*>(self->firstPlayable());
}

int GameCollection_Count(GameCollection* games)
{
    SELF(games);
    return self->count();
}

Game* GameCollection_ByIndex(GameCollection* games, int idx)
{
    SELF(games);
    try
    {
        return reinterpret_cast<Game*>(&self->byIndex(idx));
    }
    catch(const de::GameCollection::NotFoundError&)
    {} // Ignore error.
    return 0; // Not found.
}

Game* GameCollection_ByIdentityKey(GameCollection* games, char const* identityKey)
{
    SELF(games);
    try
    {
        return reinterpret_cast<Game*>(&self->byIdentityKey(identityKey));
    }
    catch(const de::GameCollection::NotFoundError&)
    {} // Ignore error.
    return 0; // Not found.
}

gameid_t GameCollection_Id(GameCollection* games, Game* game)
{
    SELF(games);
    try
    {
        return self->id(*reinterpret_cast<de::Game*>(game));
    }
    catch(const de::GameCollection::NotFoundError&)
    {} // Ignore error.
    return 0; // Invalid id.
}

void GameCollection_LocateAllResources(GameCollection* games)
{
    SELF(games);
    self->locateAllResources();
}
