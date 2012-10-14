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
#include "zipfile.h"

namespace de {

static bool validateResource(AbstractResource* rec);

struct Games::Instance
{
    Games& self;

    /// Game collection.
    Game** games;
    int gamesCount;

    /// Currently active game (in this collection).
    Game* theGame;

    /// Special "null-game" object for this collection.
    Game* nullGame;

    Instance(Games& d)
        : self(d), games(0), gamesCount(0), theGame(0), nullGame(0)
    {}

    ~Instance()
    {
        if(games)
        {
            for(int i = 0; i < gamesCount; ++i)
            {
                delete games[i];
            }
            M_Free(games); games = 0;
            gamesCount = 0;
        }

        if(nullGame)
        {
            delete nullGame;
            nullGame = NULL;
        }
        theGame = NULL;
    }

    int index(Game const& game)
    {
        if(&game != nullGame)
        {
            for(int i = 0; i < gamesCount; ++i)
            {
                if(&game == games[i])
                    return i+1;
            }
        }
        return 0;
    }

    Game* findByIdentityKey(char const* identityKey)
    {
        DENG_ASSERT(identityKey && identityKey[0]);
        for(int i = 0; i < gamesCount; ++i)
        {
            Game* game = games[i];
            if(!stricmp(Str_Text(&game->identityKey()), identityKey))
                return game;
        }
        return NULL; // Not found.
    }
};

Games::Games()
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

    d->theGame = d->nullGame = new Game("null-game", &dataPath, &defsPath, "doomsday", 0, 0);

    Str_Free(&defsPath);
    Str_Free(&dataPath);
}

Games::~Games()
{   
    delete d;
}

Game& Games::currentGame() const
{
    return *d->theGame;
}

Game& Games::nullGame() const
{
    return *d->nullGame;
}

Games& Games::setCurrentGame(Game& game)
{
    d->theGame = &game;
    return *this;
}

int Games::numPlayable() const
{
    int count = 0;
    for(int i = 0; i < d->gamesCount; ++i)
    {
        de::Game* game = d->games[i];
        if(!game->allStartupResourcesFound()) continue;
        ++count;
    }
    return count;
}

Game* Games::firstPlayable() const
{
    for(int i = 0; i < d->gamesCount; ++i)
    {
        Game* game = d->games[i];
        if(game->allStartupResourcesFound()) return game;
    }
    return NULL;
}

int Games::count() const
{
    return d->gamesCount;
}

Game* Games::byIndex(int idx) const
{
    if(idx > 0 && idx <= d->gamesCount)
        return d->games[idx-1];
    return NULL;
}

Game* Games::byIdentityKey(char const* identityKey) const
{
    if(identityKey && identityKey[0])
        return d->findByIdentityKey(identityKey);
    return NULL;
}

Game* Games::byId(gameid_t gameId) const
{
    if(gameId > 0 && gameId <= d->gamesCount)
        return d->games[gameId-1];
    return NULL; // Not found.
}

gameid_t Games::id(Game& game) const
{
    if(&game == d->nullGame) return 0; // Invalid id.
    return (gameid_t) d->index(game);
}

int Games::findAll(GameList& found)
{
    int numFoundSoFar = found.count();
    for(int i = 0; i < d->gamesCount; ++i)
    {
        found.push_back(GameListItem(d->games[i]));
    }
    return found.count() - numFoundSoFar;
}

Games& Games::add(Game& game)
{
    d->games = (Game**) M_Realloc(d->games, sizeof(*d->games) * ++d->gamesCount);
    if(!d->games) Con_Error("Games::add: Failed on allocation of %lu bytes enlarging Game list.", (unsigned long) (sizeof(*d->games) * d->gamesCount));
    d->games[d->gamesCount-1] = &game;
    return *this;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeWAD(char const* filePath, void* parameters)
{
    lumpnum_t auxLumpBase = F_OpenAuxiliary(filePath);
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

    DFile* dfile = App_FileSystem()->openFile(filePath, "bf");
    bool result = false;
    if(dfile)
    {
        result = ZipFile::recognise(*dfile);
        /// @todo Check files. We should implement an auxiliary zip lump index...
        App_FileSystem()->closeFile(*dfile);
    }
    return result;
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
            else if(recognizeZIP(Str_Text(path), (void*)AbstractResource_IdentityKeys(rec)))
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

Games& Games::locateStartupResources(Game& game)
{
    Game* oldGame = d->theGame;
    if(d->theGame != &game)
    {
        /// @attention Kludge: Temporarily switch Game.
        d->theGame = &game;
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

    if(d->theGame != oldGame)
    {
        // Kludge end - Restore the old Game.
        d->theGame = oldGame;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }
    return *this;
}

static int locateAllResourcesWorker(void* parameters)
{
    Games* games = (Games*) parameters;
    for(int i = 0; i < games->count(); ++i)
    {
        Game* game = games->byIndex(i+1);

        VERBOSE( Con_Printf("Locating resources for \"%s\"...\n", Str_Text(&game->title())) )

        games->locateStartupResources(*game);
        Con_SetProgress((i + 1) * 200 / games->count() - 1);

        VERBOSE( games->print(*game, PGF_LIST_STARTUP_RESOURCES|PGF_STATUS) )
    }
    BusyMode_WorkerEnd();
    return 0;
}

Games& Games::locateAllResources()
{
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                locateAllResourcesWorker, (void*)this, "Locating game resources...");
    return *this;
}

/**
 * @todo This has been moved here so that strings like the game title and author can
 *       be overridden (e.g., via DEHACKED). Make it so!
 */
void Games::printBanner(Game& game)
{
    Con_PrintRuler();
    Con_FPrintf(CPF_WHITE | CPF_CENTER, "%s\n", Str_Text(&game.title()));
    Con_PrintRuler();
}

void Games::printResources(Game& game, bool printStatus, int rflags)
{
    size_t count = 0;
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = game.resources((resourceclass_t)i, 0);
        if(!records) continue;

        for(AbstractResource* const* recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;

            if(rflags >= 0 && (rflags & AbstractResource_ResourceFlags(rec)))
            {
                AbstractResource_Print(rec, printStatus);
                count += 1;
            }
        }
    }

    if(count == 0)
        Con_Printf(" None\n");
}

void Games::print(Game& game, int flags) const
{
    if(isNullGame(game))
        flags &= ~PGF_BANNER;

#if _DEBUG
    Con_Printf("pluginid:%i data:\"%s\" defs:\"%s\"\n", int(game.pluginId()),
               F_PrettyPath(Str_Text(&game.dataPath())),
               F_PrettyPath(Str_Text(&game.defsPath())));
#endif

    if(flags & PGF_BANNER)
        printBanner(game);

    if(!(flags & PGF_BANNER))
        Con_Printf("Game: %s - ", Str_Text(&game.title()));
    else
        Con_Printf("Author: ");
    Con_Printf("%s\n", Str_Text(&game.author()));
    Con_Printf("IdentityKey: %s\n", Str_Text(&game.identityKey()));

    if(flags & PGF_LIST_STARTUP_RESOURCES)
    {
        Con_Printf("Startup resources:\n");
        printResources(game, (flags & PGF_STATUS) != 0, RF_STARTUP);
    }

    if(flags & PGF_LIST_OTHER_RESOURCES)
    {
        Con_Printf("Other resources:\n");
        Con_Printf("   ");
        printResources(game, /*(flags & PGF_STATUS) != 0*/false, 0);
    }

    if(flags & PGF_STATUS)
        Con_Printf("Status: %s\n",         isCurrentGame(game)? "Loaded" :
                               game.allStartupResourcesFound()? "Complete/Playable" :
                                                                "Incomplete/Not playable");
}

} // namespace de

D_CMD(ListGames)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    de::Games* games = reinterpret_cast<de::Games*>(App_Games());
    if(!games || !games->count())
    {
        Con_Printf("No Registered Games.\n");
        return true;
    }

    Con_FPrintf(CPF_YELLOW, "Registered Games:\n");
    Con_Printf("Key: '!'= Incomplete/Not playable '*'= Loaded\n");
    Con_PrintRuler();

    de::Games::GameList found;
    games->findAll(found);
    // Sort so we get a nice alphabetical list.
    qSort(found.begin(), found.end());

    int numCompleteGames = 0;
    DENG2_FOR_EACH(i, found, de::Games::GameList::const_iterator)
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
    (inst) != 0? reinterpret_cast<de::Games*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::Games const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Games* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::Games const* self = TOINTERNAL_CONST(inst)

Game* Games_CurrentGame(Games* games)
{
    SELF(games);
    return reinterpret_cast<Game*>(&self->currentGame());
}

Game* Games_NullGame(Games* games)
{
    SELF(games);
    return reinterpret_cast<Game*>(&self->nullGame());
}

int Games_NumPlayable(Games* games)
{
    SELF(games);
    return self->numPlayable();
}

Game* Games_FirstPlayable(Games* games)
{
    SELF(games);
    return reinterpret_cast<Game*>(self->firstPlayable());
}

int Games_Count(Games* games)
{
    SELF(games);
    return self->count();
}

Game* Games_ByIndex(Games* games, int idx)
{
    SELF(games);
    return reinterpret_cast<Game*>(self->byIndex(idx));
}

Game* Games_ByIdentityKey(Games* games, char const* identityKey)
{
    SELF(games);
    return reinterpret_cast<Game*>(self->byIdentityKey(identityKey));
}

gameid_t Games_Id(Games* games, Game* game)
{
    SELF(games);
    if(!game) return 0; // Invalid id.
    return self->id(*reinterpret_cast<de::Game*>(game));
}

boolean Games_IsNullObject(Games* games, Game const* game)
{
    SELF(games);
    if(!game) return false;
    return self->isNullGame(*reinterpret_cast<de::Game const*>(game));
}

void Games_LocateAllResources(Games* games)
{
    SELF(games);
    self->locateAllResources();
}

void Games_Print(Games* games, Game* game, int flags)
{
    if(!game) return;
    SELF(games);
    self->print(*reinterpret_cast<de::Game*>(game), flags);
}

void Games_PrintBanner(Game* game)
{
    if(!game) return;
    de::Games::printBanner(*reinterpret_cast<de::Game*>(game));
}

void Games_PrintResources(Game* game, boolean printStatus, int rflags)
{
    if(!game) return;
    de::Games::printResources(*reinterpret_cast<de::Game*>(game), CPP_BOOL(printStatus), rflags);
}
