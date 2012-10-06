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

using de::FS;
using de::DFile;
using de::ZipFile;

extern "C" {

Game* theGame; // Currently active game.
Game* nullGame; // Special "null-game" object.

}

// Game collection.
static Game** games;
static int gamesCount;

static int gameIndex(Game const* game)
{
    if(game && !Games_IsNullObject(game))
    {
        for(int i = 0; i < gamesCount; ++i)
        {
            if(game == games[i])
                return i+1;
        }
    }
    return 0;
}

static Game* findGameForId(gameid_t gameId)
{
    if(gameId > 0 && gameId <= gamesCount)
        return games[gameId-1];
    return NULL; // Not found.
}

static Game* findGameForIdentityKey(char const* identityKey)
{
    DENG_ASSERT(identityKey && identityKey[0]);
    for(int i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];
        if(!stricmp(Str_Text(Game_IdentityKey(game)), identityKey))
            return game;
    }
    return NULL; // Not found.
}

void Games_Shutdown(void)
{
    if(games)
    {
        int i;
        for(i = 0; i < gamesCount; ++i)
        {
            Game_Delete(games[i]);
        }
        free(games); games = 0;
        gamesCount = 0;
    }

    if(nullGame)
    {
        Game_Delete(nullGame);
        nullGame = NULL;
    }
    theGame = NULL;
}

int Games_NumPlayable(void)
{
    int count = 0;
    for(int i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];
        if(!Game_AllStartupResourcesFound(game)) continue;
        ++count;
    }
    return count;
}

Game* Games_FirstPlayable(void)
{
    for(int i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];
        if(Game_AllStartupResourcesFound(game)) return game;
    }
    return NULL;
}

static Game* addGame(Game* game)
{
    if(game)
    {
        games = (Game**) M_Realloc(games, sizeof(*games) * ++gamesCount);
        if(!games) Con_Error("addGame: Failed on allocation of %lu bytes enlarging Game list.", (unsigned long) (sizeof(*games) * gamesCount));
        games[gamesCount-1] = game;
    }
    return game;
}

int Games_Count(void)
{
    return gamesCount;
}

Game* Games_ByIndex(int idx)
{
    if(idx > 0 && idx <= gamesCount)
        return games[idx-1];
    return NULL;
}

Game* Games_ByIdentityKey(char const* identityKey)
{
    if(identityKey && identityKey[0])
        return findGameForIdentityKey(identityKey);
    return NULL;
}

gameid_t Games_Id(Game* game)
{
    if(!game || game == nullGame) return 0; // Invalid id.
    return (gameid_t)gameIndex(game);
}

boolean Games_IsNullObject(Game const* game)
{
    if(!game) return false;
    return game == nullGame;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static bool recognizeWAD(char const* filePath, void* parameters)
{
    lumpnum_t auxLumpBase = F_OpenAuxiliary3(filePath, 0, true);
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
                lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(*lumpNames), true);
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

    DFile* dfile = FS::openFile(filePath, "bf");
    bool result = false;
    if(dfile)
    {
        result = ZipFile::recognise(*dfile);
        /// @todo Check files. We should implement an auxiliary zip lumpdirectory...
        FS::closeFile(dfile);
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
            Str const* path = AbstractResource_ResolvedPathWithIndex(rec, idx, true/*locate resources*/);
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

static void locateGameStartupResources(Game* game)
{
    if(!game) return;

    Game* oldGame = theGame;
    if(theGame != game)
    {
        /// @attention Kludge: Temporarily switch Game.
        theGame = game;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }

    for(uint rclass = RESOURCECLASS_FIRST; rclass < RESOURCECLASS_COUNT; ++rclass)
    {
        AbstractResource* const* records = Game_Resources(game, resourceclass_t(rclass), 0);
        if(!records) continue;

        for(AbstractResource* const* i = records; *i; i++)
        {
            AbstractResource* rec = *i;

            // We are only interested in startup resources at this time.
            if(!(AbstractResource_ResourceFlags(rec) & RF_STARTUP)) continue;

            validateResource(rec);
        }
    }

    if(theGame != oldGame)
    {
        // Kludge end - Restore the old Game.
        theGame = oldGame;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }
}

static int locateAllResourcesWorker(void* parameters)
{
    DENG_UNUSED(parameters);
    for(int i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];

        VERBOSE( Con_Printf("Locating resources for \"%s\"...\n", Str_Text(Game_Title(game))) )

        locateGameStartupResources(game);
        Con_SetProgress((i+1) * 200/Games_Count() -1);

        VERBOSE( Games_Print(game, PGF_LIST_STARTUP_RESOURCES|PGF_STATUS) )
    }
    BusyMode_WorkerEnd();
    return 0;
}

void Games_LocateAllResources(void)
{
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                locateAllResourcesWorker, 0, "Locating game resources...");
}

/**
 * @todo This has been moved here so that strings like the game title and author can
 *       be overridden (e.g., via DEHACKED). Make it so!
 */
void Games_PrintBanner(Game* game)
{
    DENG_ASSERT(game);
    Con_PrintRuler();
    Con_FPrintf(CPF_WHITE | CPF_CENTER, "%s\n", Str_Text(Game_Title(game)));
    Con_PrintRuler();
}

void Games_PrintResources(Game* game, boolean printStatus, int rflags)
{
    if(!game) return;

    size_t count = 0;
    for(uint i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = Game_Resources(game, (resourceclass_t)i, 0);
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

void Games_Print(Game* game, int flags)
{
    if(!game) return;

    if(Games_IsNullObject(game))
        flags &= ~PGF_BANNER;

#if _DEBUG
    Con_Printf("pluginid:%i data:\"%s\" defs:\"%s\"\n", (int)Game_PluginId(game),
               F_PrettyPath(Str_Text(Game_DataPath(game))),
               F_PrettyPath(Str_Text(Game_DefsPath(game))));
#endif

    if(flags & PGF_BANNER)
        Games_PrintBanner(game);

    if(!(flags & PGF_BANNER))
        Con_Printf("Game: %s - ", Str_Text(Game_Title(game)));
    else
        Con_Printf("Author: ");
    Con_Printf("%s\n", Str_Text(Game_Author(game)));
    Con_Printf("IdentityKey: %s\n", Str_Text(Game_IdentityKey(game)));

    if(flags & PGF_LIST_STARTUP_RESOURCES)
    {
        Con_Printf("Startup resources:\n");
        Games_PrintResources(game, (flags & PGF_STATUS) != 0, RF_STARTUP);
    }

    if(flags & PGF_LIST_OTHER_RESOURCES)
    {
        Con_Printf("Other resources:\n");
        Con_Printf("   ");
        Games_PrintResources(game, /*(flags & PGF_STATUS) != 0*/false, 0);
    }

    if(flags & PGF_STATUS)
        Con_Printf("Status: %s\n",       theGame == game? "Loaded" :
                                   Game_AllStartupResourcesFound(game)? "Complete/Playable" :
                                                                        "Incomplete/Not playable");
}

static void populateGameInfo(GameInfo* info, Game* game)
{
    info->identityKey = Str_Text(Game_IdentityKey(game));
    info->title = Str_Text(Game_Title(game));
    info->author = Str_Text(Game_Author(game));
}

/// @note Part of the Doomsday public API.
boolean DD_GameInfo(GameInfo* info)
{
    if(!info)
    {
#if _DEBUG
        Con_Message("Warning: DD_GameInfo: Received invalid info (=NULL), ignoring.");
#endif
        return false;
    }

    memset(info, 0, sizeof(*info));

    if(DD_GameLoaded())
    {
        populateGameInfo(info, theGame);
        return true;
    }

#if _DEBUG
    Con_Message("DD_GameInfo: Warning, no game currently loaded - returning false.\n");
#endif
    return false;
}

/// @note Part of the Doomsday public API.
void DD_AddGameResource(gameid_t gameId, resourceclass_t rclass, int rflags,
    char const* _names, void* params)
{
    Game* game = findGameForId(gameId);
    AbstractResource* rec;
    ddstring_t name;
    ddstring_t str;
    char const* p;

    if(!game)
        Con_Error("DD_AddGameResource: Error, unknown game id %i.", gameId);
    if(!VALID_RESOURCE_CLASS(rclass))
        Con_Error("DD_AddGameResource: Unknown resource class %i.", (int)rclass);
    if(!_names || !_names[0] || !strcmp(_names, ";"))
        Con_Error("DD_AddGameResource: Invalid name argument.");
    if(0 == (rec = AbstractResource_New(rclass, rflags)))
        Con_Error("DD_AddGameResource: Unknown error occured during AbstractResource::Construct.");

    // Add a name list to the game record.
    Str_Init(&str);
    Str_Set(&str, _names);
    // Ensure the name list has the required terminating semicolon.
    if(Str_RAt(&str, 0) != ';')
        Str_Append(&str, ";");

    p = Str_Text(&str);
    Str_Init(&name);
    while((p = Str_CopyDelim2(&name, p, ';', CDF_OMIT_DELIMITER)))
    {
        AbstractResource_AddName(rec, &name);
    }
    Str_Free(&name);

    if(params)
    switch(rclass)
    {
    case RC_PACKAGE: {
        // Add an auto-identification file identityKey list to the game record.
        ddstring_t identityKey;
        const char* p;

        // Ensure the identityKey list has the required terminating semicolon.
        Str_Set(&str, (const char*) params);
        if(Str_RAt(&str, 0) != ';')
            Str_Append(&str, ";");

        Str_Init(&identityKey);
        p = Str_Text(&str);
        while((p = Str_CopyDelim2(&identityKey, p, ';', CDF_OMIT_DELIMITER)))
        {
            AbstractResource_AddIdentityKey(rec, &identityKey);
        }

        Str_Free(&identityKey);
        break;
      }
    default: break;
    }

    Game_AddResource(game, rclass, rec);

    Str_Free(&str);
}

/// @note Part of the Doomsday public API.
gameid_t DD_DefineGame(GameDef const* def)
{
    if(!def)
    {
#if _DEBUG
        Con_Message("Warning:DD_DefineGame: Received invalid GameDef (=NULL), ignoring.");
#endif
        return 0; // Invalid id.
    }

    // Game mode identity keys must be unique. Ensure that is the case.
    if(findGameForIdentityKey(def->identityKey))
    {
#if _DEBUG
        Con_Message("Warning:DD_DefineGame: Failed adding game \"%s\", identity key '%s' already in use, ignoring.\n", def->defaultTitle, def->identityKey);
#endif
        return 0; // Invalid id.
    }

    // Add this game to our records.
    Game* game = addGame(Game_FromDef(def));
    if(game)
    {
        Game_SetPluginId(game, DD_PluginIdForActiveHook());
        return Games_Id(game);
    }
    return 0; // Invalid id.
}

/// @note Part of the Doomsday public API.
gameid_t DD_GameIdForKey(const char* identityKey)
{
    Game* game = findGameForIdentityKey(identityKey);
    if(game) return Games_Id(game);

    DEBUG_Message(("Warning:DD_GameIdForKey: Game \"%s\" not defined.\n", identityKey));
    return 0; // Invalid id.
}

static int C_DECL compareGameByName(const void* a, const void* b)
{
    return stricmp(Str_Text(Game_Title(*(Game**)a)), Str_Text(Game_Title(*(Game**)b)));
}

D_CMD(ListGames)
{
    DENG_UNUSED(src);
    DENG_UNUSED(argc);
    DENG_UNUSED(argv);

    const int numAvailableGames = gamesCount;
    if(numAvailableGames)
    {
        int i, numCompleteGames = 0;
        Game** gamePtrs;

        Con_FPrintf(CPF_YELLOW, "Registered Games:\n");
        Con_Printf("Key: '!'= Incomplete/Not playable '*'= Loaded\n");
        Con_PrintRuler();

        // Sort a copy of games so we get a nice alphabetical list.
        gamePtrs = (Game**) M_Malloc(gamesCount * sizeof *gamePtrs);
        if(!gamePtrs) Con_Error("CCmdListGames: Failed on allocation of %lu bytes for sorted Game list.", (unsigned long) (gamesCount * sizeof *gamePtrs));

        memcpy(gamePtrs, games, gamesCount * sizeof *gamePtrs);
        qsort(gamePtrs, gamesCount, sizeof *gamePtrs, compareGameByName);

        for(i = 0; i < gamesCount; ++i)
        {
            Game* game = gamePtrs[i];

            Con_Printf(" %s %-16s %s (%s)\n", theGame == game? "*" :
                                          !Game_AllStartupResourcesFound(game)? "!" : " ",
                       Str_Text(Game_IdentityKey(game)), Str_Text(Game_Title(game)),
                       Str_Text(Game_Author(game)));

            if(Game_AllStartupResourcesFound(game))
                numCompleteGames++;
        }

        Con_PrintRuler();
        Con_Printf("%i of %i games playable.\n", numCompleteGames, numAvailableGames);
        Con_Printf("Use the 'load' command to load a game. For example: \"load gamename\".\n");

        M_Free(gamePtrs);
    }
    else
    {
        Con_Printf("No Registered Games.\n");
    }

    return true;
}
