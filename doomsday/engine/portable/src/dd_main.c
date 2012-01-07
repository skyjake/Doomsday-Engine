/**\file dd_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Engine Core
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif

#include "de_platform.h"

#ifdef UNIX
#  include <ctype.h>
#  include <SDL.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_bsp.h"
#include "de_edit.h"

#include "filedirectory.h"
#include "abstractresource.h"
#include "resourcenamespace.h"
#include "m_misc.h"
#include "m_args.h"
#include "texture.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct ddvalue_s {
    int*            readPtr;
    int*            writePtr;
} ddvalue_t;

typedef struct autoload_s {
    boolean         loadFiles; // Should files be loaded right away.
    int             count; // Number of files loaded successfully.
} autoload_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int DD_StartupWorker(void* paramaters);
static int DD_DummyWorker(void* paramaters);
static void DD_AutoLoad(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int renderTextures;
extern int gotFrame;
extern int monochrome;
extern int gameDataFormat;
extern int gameDrawHUD;
extern int symbolicEchoMode;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

filename_t ddBasePath = ""; // Doomsday root directory is at...?
filename_t ddRuntimePath, ddBinPath;

int isDedicated = false;

int verbose = 0; // For debug messages (-verbose).
FILE* outFile; // Output file for console messages.

// List of file names, whitespace seperating (written to .cfg).
char* gameStartupFiles = "";

// Id of the currently running title finale if playing, else zero.
finaleid_t titleFinale = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// List of game data files (specified via the command line or in a cfg, or
// found using the default search algorithm (e.g., /auto and DOOMWADDIR)).
static ddstring_t** gameResourceFileList = 0;
static size_t numGameResourceFileList = 0;

Game* theGame = NULL; // Currently active game.

// Game collection.
static Game** games = 0;
static int gamesCount = 0;
static Game* nullGame; // Special "null-game" object.

// CODE --------------------------------------------------------------------

/**
 * Register the engine commands and variables.
 */
void DD_Register(void)
{
    DD_RegisterLoop();
    DD_RegisterInput();
    F_Register();
    B_Register(); // for control bindings
    Con_Register();
    DH_Register();
    R_Register();
    S_Register();
    SBE_Register(); // for bias editor
    Rend_Register();
    GL_Register();
    Net_Register();
    I_Register();
    H_Register();
    DAM_Register();
    BSP_Register();
    UI_Register();
    Demo_Register();
    P_ControlRegister();
    FI_Register();
}

static int gameIndex(const Game* game)
{
    int i;
    if(game && !DD_IsNullGame(game))
    {
        for(i = 0; i < gamesCount; ++i)
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

static Game* findGameForIdentityKey(const char* identityKey)
{
    int i;
    assert(identityKey && identityKey[0]);
    for(i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];
        if(!stricmp(Str_Text(Game_IdentityKey(game)), identityKey))
            return game;
    }
    return NULL; // Not found.
}

static void addToPathList(ddstring_t*** list, size_t* listSize, const char* rawPath)
{
    ddstring_t* newPath = Str_New();
    assert(list && listSize && rawPath && rawPath[0]);

    Str_Set(newPath, rawPath);
    F_FixSlashes(newPath, newPath);
    F_ExpandBasePath(newPath, newPath);

    *list = realloc(*list, sizeof(**list) * ++(*listSize)); /// \fixme This is never freed!
    (*list)[(*listSize)-1] = newPath;
}

static void parseStartupFilePathsAndAddFiles(const char* pathString)
{
#define ATWSEPS                 ",; \t"

    char* token, *buffer;
    size_t len;

    if(!pathString || !pathString[0]) return;

    len = strlen(pathString);
    buffer = (char*)malloc(len + 1);
    if(!buffer) Con_Error("parseStartupFilePathsAndAddFiles: Failed on allocation of %lu bytes for parse buffer.", (unsigned long) (len+1));

    strcpy(buffer, pathString);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        F_AddFile(token, 0, false);
        token = strtok(NULL, ATWSEPS);
    }
    free(buffer);

#undef ATWSEPS
}

static void destroyPathList(ddstring_t*** list, size_t* listSize)
{
    assert(list && listSize);
    if(*list)
    {
        size_t i;
        for(i = 0; i < *listSize; ++i)
            Str_Delete((*list)[i]);
        free(*list); *list = 0;
    }
    *listSize = 0;
}

static Game* addGame(Game* game)
{
    if(game)
    {
        games = (Game**)realloc(games, sizeof(*games) * ++gamesCount);
        if(!games) Con_Error("addGame: Failed on allocation of %lu bytes enlarging Game list.", (unsigned long) (sizeof(*games) * gamesCount));
        games[gamesCount-1] = game;
    }
    return game;
}

boolean DD_GameLoaded(void)
{
    return !DD_IsNullGame(theGame);
}

int DD_GameCount(void)
{
    return gamesCount;
}

Game* DD_GameByIndex(int idx)
{
    if(idx > 0 && idx <= gamesCount)
        return games[idx-1];
    return NULL;
}

Game* DD_GameByIdentityKey(const char* identityKey)
{
    if(identityKey && identityKey[0])
        return findGameForIdentityKey(identityKey);
    return NULL;
}

gameid_t DD_GameId(Game* game)
{
    if(!game || game == nullGame) return 0; // Invalid id.
    return (gameid_t)gameIndex(game);
}

boolean DD_IsNullGame(const Game* game)
{
    if(!game) return false;
    return game == nullGame;
}

static void populateGameInfo(GameInfo* info, Game* game)
{
    info->identityKey = Str_Text(Game_IdentityKey(game));
    info->title = Str_Text(Game_Title(game));
    info->author = Str_Text(Game_Author(game));
}

boolean DD_GameInfo(GameInfo* info)
{
    if(!info)
    {
#if _DEBUG
        Con_Message("Warning:DD_GameInfo: Received invalid info (=NULL), ignoring.");
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

void DD_AddGameResource(gameid_t gameId, resourceclass_t rclass, int rflags,
    const char* _names, void* params)
{
    Game* game = findGameForId(gameId);
    AbstractResource* rec;
    ddstring_t name;
    ddstring_t str;
    const char* p;

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

gameid_t DD_DefineGame(const GameDef* def)
{
    Game* game;

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
    game = addGame(Game_FromDef(def));
    if(game)
    {
        Game_SetPluginId(game, DD_PluginIdForActiveHook());
        return DD_GameId(game);
    }
    return 0; // Invalid id.
}

gameid_t DD_GameIdForKey(const char* identityKey)
{
    Game* game = findGameForIdentityKey(identityKey);
    if(game)
    {
        return DD_GameId(game);
    }
#ifdef _DEBUG
    Con_Message("Warning:DD_GameIdForKey: Game \"%s\" not defined.\n", identityKey);
#endif
    return 0; // Invalid id.
}

void DD_DestroyGames(void)
{
    destroyPathList(&gameResourceFileList, &numGameResourceFileList);

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

/**
 * Begin the Doomsday title animation sequence.
 */
void DD_StartTitle(void)
{
    ddstring_t setupCmds;
    const char* fontName;
    ddfinale_t fin;
    int i;

    if(isDedicated || !Def_Get(DD_DEF_FINALE, "background", &fin)) return;

    Str_Init(&setupCmds);

    // Configure the predefined fonts (all normal, variable width).
    fontName = R_ChooseVariableFont(FS_NORMAL, theWindow->geometry.size.width,
                                               theWindow->geometry.size.height);
    for(i = 1; i <= FIPAGE_NUM_PREDEFINED_FONTS; ++i)
    {
        Str_Appendf(&setupCmds, "prefont %i "FN_SYSTEM_NAME":%s\n", i, fontName);
    }

    // Configure the predefined colors.
    for(i = 1; i <= MIN_OF(NUM_UI_COLORS, FIPAGE_NUM_PREDEFINED_FONTS); ++i)
    {
        ui_color_t* color = UI_Color(i-1);
        Str_Appendf(&setupCmds, "precolor %i %f %f %f\n", i, color->red, color->green, color->blue);
    }

    titleFinale = FI_Execute2(fin.script, FF_LOCAL, Str_Text(&setupCmds));
    Str_Free(&setupCmds);
}

/// @return  @c true, iff the resource appears to be what we think it is.
static boolean recognizeWAD(const char* filePath, void* data)
{
    lumpnum_t auxLumpBase = F_OpenAuxiliary3(filePath, 0, true);
    boolean result;

    if(auxLumpBase == -1) return false;

    // Ensure all identity lumps are present.
    result = true;
    if(data)
    {
        const ddstring_t* const* lumpNames = (const ddstring_t* const*) data;
        for(; result && *lumpNames; lumpNames++)
        {
            lumpnum_t lumpNum = F_CheckLumpNumForName2(Str_Text(*lumpNames), true);
            if(lumpNum == -1)
            {
                result = false;
            }
        }
    }

    F_CloseAuxiliary();
    return result;
}

/// @return  @c true, iff the resource appears to be what we think it is.
static boolean recognizeZIP(const char* filePath, void* data)
{
    /// \todo dj: write me.
    return F_FileExists(filePath);
}

static int validateResource(AbstractResource* rec, void* paramaters)
{
    int validated = false;
    const ddstring_t* path = AbstractResource_ResolvedPath(rec, true/*can locate resources*/);

    if(path)
    {
        switch(AbstractResource_ResourceClass(rec))
        {
        case RC_PACKAGE:
            if(recognizeWAD(Str_Text(path), (void*)AbstractResource_IdentityKeys(rec)))
            {
                validated = true;
            }
            else if(recognizeZIP(Str_Text(path), (void*)AbstractResource_IdentityKeys(rec)))
            {
                validated = true;
            }
            break;
        default: break;
        }
    }

    return validated;
}

static boolean isRequiredResource(Game* game, const char* absolutePath)
{
    AbstractResource* const* records = Game_Resources(game, RC_PACKAGE, 0);
    if(records)
    {
        AbstractResource* const* recordIt;
        // Is this resource from a container?
        abstractfile_t* file = F_FindLumpFile(absolutePath, NULL);
        if(file)
        {
            // Yes; use the container's path instead.
            absolutePath = Str_Text(AbstractFile_Path(file));
        }

        for(recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;
            if(AbstractResource_ResourceFlags(rec) & RF_STARTUP)
            {
                const ddstring_t* resolvedPath = AbstractResource_ResolvedPath(rec, true);
                if(resolvedPath && !Str_CompareIgnoreCase(resolvedPath, absolutePath))
                {
                    return true;
                }
            }
        }
    }
    // Not found, so no.
    return false;
}

static void locateGameResources(Game* game)
{
    Game* oldGame = theGame;
    uint i;

    if(!game) return;

    if(theGame != game)
    {
        /// \kludge Temporarily switch Game.
        theGame = game;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }

    for(i = RESOURCECLASS_FIRST; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = Game_Resources(game, (resourceclass_t)i, 0);
        AbstractResource* const* recordIt;

        if(records)
        for(recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;

            // We are only interested in startup resources at this time.
            if(!(AbstractResource_ResourceFlags(rec) & RF_STARTUP)) continue;

            validateResource(rec, 0);
        }
    }

    if(theGame != oldGame)
    {
        /// \kludge Restore the old Game.
        theGame = oldGame;
        // Re-init the resource locator using the search paths of this Game.
        F_ResetAllResourceNamespaces();
    }
}

static boolean allGameResourcesFound(Game* game)
{
    if(!DD_IsNullGame(game))
    {
        uint i;
        for(i = 0; i < RESOURCECLASS_COUNT; ++i)
        {
            AbstractResource* const* records = Game_Resources(game, (resourceclass_t)i, 0);
            AbstractResource* const* recordIt;

            if(records)
            for(recordIt = records; *recordIt; recordIt++)
            {
                AbstractResource* rec = *recordIt;

                if((AbstractResource_ResourceFlags(rec) & RF_STARTUP) &&
                   !AbstractResource_ResolvedPath(rec, false))
                    return false;
            }
        }
    }
    return true;
}

static void loadGameResources(Game* game, resourceclass_t rclass)
{
    AbstractResource* const* records;
    AbstractResource* const* recordIt;

    if(!game || !VALID_RESOURCE_CLASS(rclass)) return;

    records = Game_Resources(game, rclass, 0);
    if(!records) return;

    Con_Message("Loading game resources%s\n", verbose >= 1? ":" : "...");

    for(recordIt = records; *recordIt; recordIt++)
    {
        AbstractResource* rec = *recordIt;

        switch(AbstractResource_ResourceClass(rec))
        {
        case RC_PACKAGE: {
            const ddstring_t* path = AbstractResource_ResolvedPath(rec, false/*do not locate resource*/);
            if(path)
            {
                F_AddFile(Str_Text(path), 0, false);
            }
            break;
          }
        default:
            Con_Error("loadGameResources: No resource loader found for %s.",
                      F_ResourceClassStr(AbstractResource_ResourceClass(rec)));
        }
    }
}

/**
 * Print a game mode banner with rulers.
 * \todo dj: This has been moved here so that strings like the game
 * title and author can be overridden (e.g., via DEHACKED). Make it so!
 */
static void printGameBanner(Game* game)
{
    assert(game);
    Con_PrintRuler();
    Con_FPrintf(CPF_WHITE | CPF_CENTER, "%s\n", Str_Text(Game_Title(game)));
    Con_PrintRuler();
}

static void printGameResources(Game* game, boolean printStatus, int rflags)
{
    size_t count = 0;
    uint i;

    if(!game) return;

    for(i = 0; i < RESOURCECLASS_COUNT; ++i)
    {
        AbstractResource* const* records = Game_Resources(game, (resourceclass_t)i, 0);
        AbstractResource* const* recordIt;

        if(records)
        for(recordIt = records; *recordIt; recordIt++)
        {
            AbstractResource* rec = *recordIt;

            if(AbstractResource_ResourceFlags(rec) == rflags)
            {
                AbstractResource_Print(rec, printStatus);
                count += 1;
            }
        }
    }

    if(count == 0)
        Con_Printf(" None\n");
}

void DD_PrintGame(Game* game, int flags)
{
    if(DD_IsNullGame(game))
        flags &= ~PGF_BANNER;

#if _DEBUG
    Con_Printf("pluginid:%i data:\"%s\" defs:\"%s\"\n", (int)Game_PluginId(game),
               F_PrettyPath(Str_Text(Game_DataPath(game))),
               F_PrettyPath(Str_Text(Game_DefsPath(game))));
#endif

    if(flags & PGF_BANNER)
        printGameBanner(game);

    if(!(flags & PGF_BANNER))
        Con_Printf("Game: %s - ", Str_Text(Game_Title(game)));
    else
        Con_Printf("Author: ");
    Con_Printf("%s\n", Str_Text(Game_Author(game)));
    Con_Printf("IdentityKey: %s\n", Str_Text(Game_IdentityKey(game)));

    if(flags & PGF_LIST_STARTUP_RESOURCES)
    {
        Con_Printf("Startup resources:\n");
        printGameResources(game, (flags & PGF_STATUS) != 0, RF_STARTUP);
    }

    if(flags & PGF_LIST_OTHER_RESOURCES)
    {
        Con_Printf("Other resources:\n");
        /*@todo dj: we need a resource flag for "located"*/
        Con_Printf("   ");
        printGameResources(game, /*(flags & PGF_STATUS) != 0*/false, 0);
    }

    if(flags & PGF_STATUS)
        Con_Printf("Status: %s\n",       theGame == game? "Loaded" :
                                   allGameResourcesFound(game)? "Complete/Playable" :
                                                                "Incomplete/Not playable");
}

/**
 * (f_allresourcepaths_callback_t)
 */
static int autoDataAdder(const ddstring_t* fileName, pathdirectorynode_type_t type, void* paramaters)
{
    assert(fileName && paramaters);
    // We are only interested in files.
    if(type == PT_LEAF)
    {
        autoload_t* data = (autoload_t*)paramaters;
        if(data->loadFiles)
        {
            if(F_AddFile(Str_Text(fileName), 0, false))
                ++data->count;
        }
        else
        {
            addToPathList(&gameResourceFileList, &numGameResourceFileList, Str_Text(fileName));
        }
    }
    return 0; // Continue searching.
}

/**
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data
 * directory are added to gameResourceFileList.
 *
 * @return              Number of new files that were loaded.
 */
static int addFilesFromAutoData(boolean loadFiles)
{
    static const char* extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        0
    };

    ddstring_t pattern;
    autoload_t data;
    uint i;

    data.loadFiles = loadFiles;
    data.count = 0;

    Str_Init(&pattern);
    for(i = 0; extensions[i]; ++i)
    {
        Str_Clear(&pattern);
        Str_Appendf(&pattern, "%sauto/*.%s", Str_Text(Game_DataPath(theGame)), extensions[i]);
        F_AllResourcePaths2(Str_Text(&pattern), autoDataAdder, (void*)&data);
    }
    Str_Free(&pattern);
    return data.count;
}

static boolean exchangeEntryPoints(pluginid_t pluginId)
{
    if(pluginId != 0)
    {
        // Do the API transfer.
        GETGAMEAPI fptAdr;
        if(!(fptAdr = (GETGAMEAPI) DD_FindEntryPoint(pluginId, "GetGameAPI")))
            return false;
        app.GetGameAPI = fptAdr;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    else
    {
        app.GetGameAPI = 0;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    return true;
}

typedef struct {
    Game* game;
    /// @c true iff caller (i.e., DD_ChangeGame) initiated busy mode.
    boolean initiatedBusyMode;
} ddchangegameworker_paramaters_t;

static int DD_ChangeGameWorker(void* paramaters)
{
    ddchangegameworker_paramaters_t* p = (ddchangegameworker_paramaters_t*)paramaters;
    uint i;
    assert(p);

    // Reset file Ids so previously seen files can be processed again.
    F_ResetFileIds();
    F_InitVirtualDirectoryMappings();
    F_ResetAllResourceNamespaces();

    if(!DD_IsNullGame(p->game))
    {
        ddstring_t temp;

        // Create default Auto mappings in the runtime directory.
        // Data class resources.
        Str_Init(&temp);
        Str_Appendf(&temp, "%sauto", Str_Text(Game_DataPath(p->game)));
        F_AddVirtualDirectoryMapping("auto", Str_Text(&temp));

        // Definition class resources.
        Str_Clear(&temp);
        Str_Appendf(&temp, "%sauto", Str_Text(Game_DefsPath(p->game)));
        F_AddVirtualDirectoryMapping("auto", Str_Text(&temp));
        Str_Free(&temp);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(10);

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * \note duplicate processing of the same file is automatically guarded against by
     * the virtual file system layer.
     *
     * Phase 1: Add game-resource files.
     * \fixme dj: First ZIPs then WADs (they may contain WAD files).
     */
#pragma message("!!!WARNING: Phase 1 of game resource loading does not presently prioritize ZIP!!!")
    loadGameResources(p->game, RC_PACKAGE);

    /**
     * Phase 2: Add additional game-startup files.
     * \note These must take precedence over Auto but not game-resource files.
     */
    if(gameStartupFiles && gameStartupFiles[0])
        parseStartupFilePathsAndAddFiles(gameStartupFiles);

    if(!DD_IsNullGame(p->game))
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         * First ZIPs then WADs (they may contain WAD files).
         */
        addFilesFromAutoData(false);
        if(numGameResourceFileList > 0)
        {
            size_t i, pass;
            for(pass = 0; pass < 2; ++pass)
            for(i = 0; i < numGameResourceFileList; ++i)
            {
                resourcetype_t resType = F_GuessResourceTypeByName(Str_Text(gameResourceFileList[i]));
                if((pass == 0 && resType == RT_ZIP) ||
                   (pass == 1 && resType == RT_WAD))
                    F_AddFile(Str_Text(gameResourceFileList[i]), 0, false);
            }
        }

        // Final autoload round.
        DD_AutoLoad();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(60);

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    F_InitLumpDirectoryMappings();
    F_ResetAllResourceNamespaces();
    Cl_InitTranslations();

    // Texture resources are located now, prior to initializing the game.
    R_InitPatchComposites();
    R_InitFlatTextures();
    R_InitSpriteTextures();

    Con_SetProgress(100);

    // Now that resources have been located we can begin to initialize the game.
    if(!DD_IsNullGame(p->game) && gx.PreInit)
        gx.PreInit(DD_GameId(p->game));

    /**
     * Parse the game's main config file.
     * If a custom top-level config is specified; let it override.
     */
    { const ddstring_t* configFileName = 0;
    ddstring_t tmp;
    if(ArgCheckWith("-config", 1))
    {
        Str_Init(&tmp); Str_Set(&tmp, ArgNext());
        F_FixSlashes(&tmp, &tmp);
        configFileName = &tmp;
    }
    else
    {
        configFileName = Game_MainConfig(p->game);
    }

    Con_Message("Parsing primary config \"%s\"...\n", F_PrettyPath(Str_Text(configFileName)));
    Con_ParseCommands(Str_Text(configFileName), true);
    if(configFileName == &tmp)
        Str_Free(&tmp);
    }

    if(!isDedicated && !DD_IsNullGame(p->game))
    {
        // Apply default control bindings for this game.
        B_BindGameDefaults();

        // Read bindings for this game and merge with the working set.
        Con_ParseCommands(Str_Text(Game_BindingConfig(p->game)), false);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(120);

    Def_Read();

    if(p->initiatedBusyMode)
        Con_SetProgress(130);

    R_InitSprites(); // Fully initialize sprites.
    R_InitModels();

    Def_PostInit();

    if(p->initiatedBusyMode)
        Con_SetProgress(150);

    DD_ReadGameHelp();

    // Re-init to update the title, background etc.
    Rend_ConsoleInit();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();

    if(p->initiatedBusyMode)
        Con_SetProgress(160);

    // Invalidate old cmds and init player values.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t* plr = &ddPlayers[i];

        plr->extraLight = plr->targetExtraLight = 0;
        plr->extraLightCounter = 0;

        /*
        if(isServer && plr->shared.inGame)
            clients[i].runTime = SECONDS_TO_TICKS(gameTime);
         */
    }

    if(gx.PostInit)
    {
        gx.PostInit();
        if(p->initiatedBusyMode)
            Con_SetProgress(190);
    }

    if(!DD_IsNullGame(p->game))
    {
        printGameBanner(p->game);
    }
    else
    {   // Lets play a nice title animation.
        DD_StartTitle();
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        Con_BusyWorkerEnd();
    }
    return 0;
}

/**
 * Switch to/activate the specified game.
 */
boolean DD_ChangeGame2(Game* game, boolean allowReload)
{
    boolean isReload = false;
    char buf[256];
    assert(game);

    // Ignore attempts to re-load the current game?
    if(theGame == game)
    {
        if(!allowReload)
        {
            if(DD_GameLoaded())
                Con_Message("%s (%s) - already loaded.\n", Str_Text(Game_Title(game)), Str_Text(Game_IdentityKey(game)));
            return true;
        }
        // We are re-loading.
        isReload = true;
    }

    // Quit netGame if one is in progress.
    if(netGame)
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect", true, false);

    S_Reset();
    Demo_StopPlayback();

    GL_PurgeDeferredTasks();
    GL_ResetTextureManager();
    GL_SetFilter(false);

    // If a game is presently loaded; unload it.
    if(DD_GameLoaded())
    {
        uint i;

        if(gx.Shutdown)
            gx.Shutdown();
        Con_SaveDefaults();

        LO_Clear();
        //R_DestroyObjlinks();
        R_ClearAnimGroups();

        P_PtcShutdown();
        P_ControlShutdown();
        Con_Execute(CMDS_DDAY, "clearbindings", true, false);

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            // Mobjs go down with the map.
            ddpl->mo = 0;
            // States have changed, the states are unknown.
            ddpl->pSprites[0].statePtr = ddpl->pSprites[1].statePtr = 0;

            //ddpl->inGame = false;
            ddpl->flags &= ~DDPF_CAMERA;

            ddpl->fixedColorMap = 0;
            ddpl->extraLight = 0;
        }

        Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);
        // If a map was loaded; unload it.
        P_SetCurrentMap(0);
        P_ShutdownGameMapObjDefs();
        Cl_Reset();

        R_ShutdownSvgs();
        R_DestroyColorPalettes();

        Fonts_ClearRuntime();
        Textures_ClearRuntime();

        Sfx_InitLogical();
        P_InitThinkerLists(0x1|0x2);

        Con_ClearDatabases();

        { // Tell the plugin it is being unloaded.
            void* unloader = DD_FindEntryPoint(Game_PluginId(theGame), "DP_Unload");
#ifdef _DEBUG
            Con_Message("DD_ChangeGame2: Calling DP_Unload (%p)\n", unloader);
#endif
            if(unloader) ((pluginfunc_t)unloader)();
        }

        // The current game is now the special "null-game".
        theGame = nullGame;

        Con_InitDatabases();
        DD_Register();
        I_InitVirtualInputDevices();

        R_InitSvgs();
        R_InitViewWindow();

        /// \fixme Assumes we only cache lumps from non-startup wads.
        Z_FreeTags(PU_CACHE, PU_CACHE);

        F_Reset();
        F_ResetAllResourceNamespaces();
    }

    FI_Shutdown();
    titleFinale = 0; // If the title finale was in progress it isn't now.

    /// \fixme Materials database should not be shutdown during a reload.
    Materials_Shutdown();

    VERBOSE(
        if(!DD_IsNullGame(game))
        {
            Con_Message("Selecting game '%s'...\n", Str_Text(Game_IdentityKey(game)));
        }
        else if(!isReload)
        {
            Con_Message("Unloaded game.\n");
        }
    )

    Library_ReleaseGames();

    if(!exchangeEntryPoints(Game_PluginId(game)))
    {
        DD_ComposeMainWindowTitle(buf);
        Sys_SetWindowTitle(windowIDX, buf);

        Materials_Init();
        FI_Init();
        P_PtcInit();

        Con_Message("Warning:DD_ChangeGame: Failed exchanging entrypoints with plugin %i, aborting.\n", (int)Game_PluginId(game));
        return false;
    }

    // This is now the current game.
    theGame = game;

    if(!DD_IsNullGame(theGame))
    {
        // Tell the plugin it is being loaded.
        void* loader = DD_FindEntryPoint(Game_PluginId(theGame), "DP_Load");
#ifdef _DEBUG
        Con_Message("DD_ChangeGame2: Calling DP_Load (%p)\n", loader);
#endif
        if(loader) ((pluginfunc_t)loader)();
    }

    DD_ComposeMainWindowTitle(buf);
    Sys_SetWindowTitle(windowIDX, buf);

    Materials_Init();
    FI_Init();
    P_PtcInit();

    P_InitMapUpdate();
    P_InitGameMapObjDefs();
    DAM_Init();

    /**
     * The bulk of this we can do in busy mode unless we are already busy
     * (which can happen if a fatal error occurs during game load and we must
     * shutdown immediately; Sys_Shutdown will call back to load the special
     * "null-game" game).
     */
    { ddchangegameworker_paramaters_t p;
    p.game = game;
    p.initiatedBusyMode = !Con_IsBusy();
    if(p.initiatedBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                 (DD_IsNullGame(game)?"Unloading game...":"Changing game..."), DD_ChangeGameWorker, &p);
    }
    else
    {   /// \todo Update the current task name and push progress.
        DD_ChangeGameWorker(&p);
    }
    }

    // Process any GL-related tasks we couldn't while Busy.
    Rend_ParticleLoadExtraTextures();

    /**
     * Clear any input events we may have accumulated during this process.
     * \note Only necessary here because we might not have been able to use
     * busy mode (which would normally do this for us on end).
     */
    DD_ClearEvents();
    return true;
}

boolean DD_ChangeGame(Game* game)
{
    return DD_ChangeGame2(game, false);
}

/**
 * Looks for new files to autoload from the auto-load data directory.
 */
static void DD_AutoLoad(void)
{
    /**
     * Keep loading files if any are found because virtual files may now
     * exist in the auto-load directory.
     */
    int numNewFiles;
    while((numNewFiles = addFilesFromAutoData(true)) > 0)
    {
        VERBOSE( Con_Message("Autoload round completed with %i new files.\n", numNewFiles) );
    }
}

static int countPlayableGames(void)
{
    int i, count = 0;
    for(i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];
        if(!allGameResourcesFound(game)) continue;
        ++count;
    }
    return count;
}

/**
 * Attempt automatic game selection.
 */
void DD_AutoselectGame(void)
{
    int numPlayableGames = countPlayableGames();

    if(0 >= numPlayableGames) return;

    if(1 == numPlayableGames)
    {
        // Find this game and select it.
        Game* game;
        int i;
        for(i = 0; i < gamesCount; ++i)
        {
            Game* cand = games[i];
            if(!allGameResourcesFound(cand)) continue;

            game = cand;
            break;
        }

        if(game)
        {
            DD_ChangeGame(game);
        }
        return;
    }

    if(ArgCheckWith("-game", 1))
    {
        const char* identityKey = ArgNext();
        Game* game = findGameForIdentityKey(identityKey);
        if(game && allGameResourcesFound(game))
        {
            DD_ChangeGame(game);
        }
    }
}

int DD_EarlyInit(void)
{
    ddstring_t dataPath, defsPath;

    // Determine the requested degree of verbosity.
    verbose = ArgExists("-verbose");

    // The memory zone must be online before the console module.
    if(!Z_Init())
    {
        DD_ErrorBox(true, "Error initializing memory zone.");
    }

    // Bring the console online as soon as we can.
    DD_ConsoleInit();

    Con_InitDatabases();

    // Register the engine's console commands and variables.
    DD_Register();

    // Bring the window manager online.
    Sys_InitWindowManager();

    /**
     * One-time creation and initialization of the special "null-game"
     * object (activated once created).
     *
     * \note Ideally this would call DD_ChangeGame but not all required
     * subsystems are online at this time.
     */
    Str_Init(&dataPath);
    Str_Set(&dataPath, DD_BASEPATH_DATA);
    Str_Strip(&dataPath);
    F_FixSlashes(&dataPath, &dataPath);
    F_ExpandBasePath(&dataPath, &dataPath);
    F_AppendMissingSlash(&dataPath);

    Str_Init(&defsPath);
    Str_Set(&defsPath, DD_BASEPATH_DEFS);
    Str_Strip(&defsPath);
    F_FixSlashes(&defsPath, &defsPath);
    F_ExpandBasePath(&defsPath, &defsPath);
    F_AppendMissingSlash(&defsPath);

    theGame = nullGame = Game_New("null-game", &dataPath, &defsPath, "doomsday", 0, 0);

    Str_Free(&defsPath);
    Str_Free(&dataPath);
    return true;
}

static int DD_LocateAllGameResourcesWorker(void* paramaters)
{
    int i;
    for(i = 0; i < gamesCount; ++i)
    {
        Game* game = games[i];

        VERBOSE( Con_Printf("Locating resources for \"%s\"...\n", Str_Text(Game_Title(game))) )

        locateGameResources(game);
        Con_SetProgress((i+1) * 200/gamesCount -1);

        VERBOSE( DD_PrintGame(game, PGF_LIST_STARTUP_RESOURCES|PGF_STATUS) )
    }
    Con_BusyWorkerEnd();
    return 0;
}

/**
 * Engine initialization. When complete, starts the "game loop".
 */
int DD_Main(void)
{
    // By default, use the resolution defined in (default).cfg.
    int winWidth = defResX, winHeight = defResY, winBPP = defBPP, winX = 0, winY = 0;
    uint winFlags = DDWF_VISIBLE | DDWF_CENTER | (defFullscreen? DDWF_FULLSCREEN : 0);
    boolean noCenter = false;
    int i, exitCode = 0;

#ifdef _DEBUG
    // Type size check.
    {
        void* ptr = 0;
        int32_t int32 = 0;
        int16_t int16 = 0;
        float float32 = 0;
        ASSERT_32BIT(int32);
        ASSERT_16BIT(int16);
        ASSERT_32BIT(float32);
#ifdef __64BIT__
        ASSERT_64BIT(ptr);
#else
        ASSERT_NOT_64BIT(ptr);
#endif
    }
#endif

    // Check for command line options modifying the defaults.
    if(ArgCheckWith("-width", 1))
        winWidth = atoi(ArgNext());
    if(ArgCheckWith("-height", 1))
        winHeight = atoi(ArgNext());
    if(ArgCheckWith("-winsize", 2))
    {
        winWidth = atoi(ArgNext());
        winHeight = atoi(ArgNext());
    }
    if(ArgCheckWith("-bpp", 1))
        winBPP = atoi(ArgNext());
    if(winBPP != 16 && winBPP != 32)
        winBPP = 32;
    if(ArgCheck("-nocenter"))
        noCenter = true;
    if(ArgCheckWith("-xpos", 1))
    {
        winX = atoi(ArgNext());
        noCenter = true;
    }
    if(ArgCheckWith("-ypos", 1))
    {
        winY = atoi(ArgNext());
        noCenter = true;
    }
    if(noCenter)
        winFlags &= ~DDWF_CENTER;

    if(ArgExists("-nofullscreen") || ArgExists("-window"))
        winFlags &= ~DDWF_FULLSCREEN;

    if(!Sys_SetWindow(windowIDX, winX, winY, winWidth, winHeight, winBPP, winFlags, 0))
        return -1;

    if(!GL_EarlyInit())
    {
        Sys_CriticalMessage("GL_EarlyInit() failed.");
        return -1;
    }

    if(!novideo)
    {
        // Render a few black frames before we continue. This will help to
        // stabilize things before we begin drawing for real and to avoid any
        // unwanted video artefacts.
        i = 0;
        while(i++ < 3)
        {
            glClear(GL_COLOR_BUFFER_BIT);
            GL_DoUpdate();
        }
    }

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    F_Init();

    Fonts_Init();
    FR_Init();

    // Enter busy mode until startup complete.
    Con_InitProgress(200);
    Con_Busy(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
            "Starting up...", DD_StartupWorker, 0);

    // Engine initialization is complete. Now finish up with the GL.
    GL_Init();
    GL_InitRefresh();

    // Do deferred uploads.
    Con_InitProgress(200);
    Con_Busy(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
             "Buffering...", DD_DummyWorker, 0);

    // Clean up.
    if(!novideo)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        GL_DoUpdate();
    }

    // Add paths to resources specified using -iwad options on the command line.
    { resourcenamespaceid_t rnId = F_DefaultResourceNamespaceForClass(RC_PACKAGE);
    int p;

    for(p = 0; p < Argc(); ++p)
    {
        if(!ArgRecognize("-iwad", Argv(p)))
            continue;

        while(++p != Argc() && !ArgIsOption(p))
        {
            const char* filePath = Argv(p);
            directory_t* dir;
            Uri* searchPath;

            /// \todo Do not add these as search paths, publish them directly
            /// to the FileDirectory owned by the "packages" ResourceNamespace.
            dir = Dir_ConstructFromPathDir(filePath);
            searchPath = Uri_NewWithPath2(Dir_Path(dir), RC_PACKAGE);

            F_AddSearchPathToResourceNamespace(rnId, 0, searchPath, SPG_DEFAULT);

            Uri_Delete(searchPath);
            Dir_Delete(dir);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }}

    // Try to locate all required data files for all registered games.
    Con_InitProgress(200);
    Con_Busy(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
             "Locating game resources...", DD_LocateAllGameResourcesWorker, 0);

    // Unless we reenter busy-mode due to automatic game selection, we won't be
    // drawing anything further until DD_GameLoop; so lets clean up.
    if(!novideo)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        GL_DoUpdate();
    }

    // Attempt automatic game selection.
    if(!ArgExists("-noautoselect"))
    {
        DD_AutoselectGame();
    }

    // Load resources specified using -file options on the command line.
    {int p;
    for(p = 0; p < Argc(); ++p)
    {
        if(!ArgRecognize("-file", Argv(p)))
            continue;

        while(++p != Argc() && !ArgIsOption(p))
            F_AddFile(Argv(p), 0, false);

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }}

    /// Re-initialize the resource locator as there are now new resources to be found
    /// on existing search paths (probably that is).
    F_InitLumpDirectoryMappings();
    F_ResetAllResourceNamespaces();

    // One-time execution of various command line features available during startup.
    if(ArgCheckWith("-dumplump", 1))
    {
        const char* name = ArgNext();
        lumpnum_t absoluteLumpNum = F_CheckLumpNumForName(name);
        if(absoluteLumpNum >= 0)
        {
            int lumpIdx;
            abstractfile_t* fsObject = F_FindFileForLumpNum2(absoluteLumpNum, &lumpIdx);
            F_DumpLump(fsObject, lumpIdx, NULL);
        }
    }

    if(ArgCheck("-dumpwaddir"))
    {
        F_PrintLumpDirectory();
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    Con_ParseCommands("autoexec.cfg", false);

    // Read additional config files that should be processed post engine init.
    if(ArgCheckWith("-parse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Sys_GetRealTime();
        for(;;)
        {
            const char* arg = ArgNext();
            if(!arg || arg[0] == '-') break;

            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg, false);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    }

    // A console command on the command line?
    { int p;
    for(p = 1; p < Argc() - 1; p++)
    {
        if(stricmp(Argv(p), "-command") && stricmp(Argv(p), "-cmd"))
            continue;

        for(++p; p < Argc(); p++)
        {
            const char* arg = Argv(p);

            if(arg[0] == '-')
            {
                p--;
                break;
            }
            Con_Execute(CMDS_CMDLINE, arg, false, false);
        }
    }}

    /**
     * One-time execution of network commands on the command line.
     * Commands are only executed if we have loaded a game during startup.
     */
    if(DD_GameLoaded())
    {
        // Client connection command.
        if(ArgCheckWith("-connect", 1))
            Con_Executef(CMDS_CMDLINE, false, "connect %s", ArgNext());

        // Incoming TCP port.
        if(ArgCheckWith("-port", 1))
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", ArgNext());

        // Server start command.
        // (shortcut for -command "net init tcpip; net server start").
        if(ArgExists("-server"))
        {
            if(!N_InitService(true))
                Con_Message("Can't start server: network init failed.\n");
            else
                Con_Executef(CMDS_CMDLINE, false, "net server start");
        }
    }
    else
    {
        // No game loaded.
        // Ok, lets get most of everything else initialized.
        // Reset file IDs so previously seen files can be processed again.
        F_ResetFileIds();
        F_InitLumpDirectoryMappings();
        F_InitVirtualDirectoryMappings();
        F_ResetAllResourceNamespaces();

        R_InitPatchComposites();
        R_InitFlatTextures();
        R_InitSpriteTextures();

        Def_Read();

        R_InitSprites();
        R_InitModels();
        Cl_InitTranslations();

        Def_PostInit();

        // Lets play a nice title animation.
        DD_StartTitle();

        // We'll open the console and print a list of the known games too.
        Con_Execute(CMDS_DDAY, "conopen", true, false);
        if(!ArgExists("-noautoselect"))
            Con_Printf("Automatic game selection failed.\n");
        Con_Execute(CMDS_DDAY, "listgames", false, false);
    }

    // Start the game loop.
    exitCode = DD_GameLoop();

    // Time to shutdown.
    Sys_Shutdown();

    // Bye!
    return exitCode;
}

static void DD_InitResourceSystem(void)
{
    Con_Message("Initializing Resource subsystem...\n");

    F_InitResourceLocator();
    F_CreateNamespacesForFileResourcePaths();
    F_InitVirtualDirectoryMappings();
    F_ResetAllResourceNamespaces();

    // Initialize the definition databases.
    Def_Init();
}

static int DD_StartupWorker(void* parm)
{
#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif

    // Initialize the key mappings.
    DD_InitInput();

    Con_SetProgress(10);

    // Any startup hooks?
    DD_CallHooks(HOOK_STARTUP, 0, 0);

    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(ArgCheckWith("-userdir", 1) && !app.usingUserDir)
        Con_Message("--(!)-- User directory not found (check -userdir).\n");

    bamsInit(); // Binary angle calculations.

    DD_InitResourceSystem();

    Con_SetProgress(40);

    Net_Init();
    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Read config files that should be read BEFORE engine init.
    if(ArgCheckWith("-cparse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Sys_GetRealTime();
        for(;;)
        {
            const char* arg = ArgNext();
            if(!arg || arg[0] == '-')
                break;
            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg, false);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    }

    // Add required engine resource files.
    { ddstring_t foundPath; Str_Init(&foundPath);
    if(0 == F_FindResource2(RC_PACKAGE, "doomsday.pk3", &foundPath) ||
       !F_AddFile(Str_Text(&foundPath), 0, false))
    {
        Con_Error("DD_StartupWorker: Failed to locate required resource \"doomsday.pk3\".");
    }
    Str_Free(&foundPath);
    }

    // No more lumps/packages will be loaded in startup mode after this point.
    F_EndStartup();

    // Load engine help resources.
    DD_InitHelp();

    Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg", false);

    // Get the material manager up and running.
    Con_SetProgress(90);
    GL_EarlyInitTextureManager();
    Materials_Init();

    Con_SetProgress(140);
    Con_Message("Initializing Binding subsystem...\n");
    B_Init();

    Con_SetProgress(150);
    R_Init();

    Con_SetProgress(165);
    Net_InitGame();
    Demo_Init();

    Con_Message("Initializing InFine subsystem...\n");
    FI_Init();

    Con_Message("Initializing UI subsystem...\n");
    UI_Init();

    Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
        Con_Open(true);

    Con_SetProgress(199);

    DD_CallHooks(HOOK_INIT, 0, 0); // Any initialization hooks?

    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    Con_BusyWorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static int DD_DummyWorker(void *parm)
{
    Con_SetProgress(200);
    Con_BusyWorkerEnd();
    return 0;
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(ArgCheckWith("-timedemo", 1) || // Timedemo mode.
           ArgCheckWith("-playdemo", 1)) // Play-once mode.
        {
            char            buf[200];

            sprintf(buf, "playdemo %s", ArgNext());
            Con_Execute(CMDS_CMDLINE, buf, false, false);
        }
    }
}

typedef struct {
    /// @c true iff caller (i.e., DD_UpdateEngineState) initiated busy mode.
    boolean initiatedBusyMode;
} ddupdateenginestateworker_paramaters_t;

static int DD_UpdateEngineStateWorker(void* paramaters)
{
    assert(paramaters);
    {
    ddupdateenginestateworker_paramaters_t* p = (ddupdateenginestateworker_paramaters_t*) paramaters;

    if(!novideo)
    {
        GL_InitRefresh();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    R_Update();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        Con_BusyWorkerEnd();
    }
    return 0;
    }
}

void DD_UpdateEngineState(void)
{
    boolean hadFog;

    Con_Message("Updating engine state...\n");

    // Stop playing sounds and music.
    GL_SetFilter(false);
    Demo_StopPlayback();
    S_Reset();

    //F_ResetFileIds();

    // Update the dir/WAD translations.
    F_InitLumpDirectoryMappings();
    F_InitVirtualDirectoryMappings();
    /// Re-initialize the resource locator as there may now be new resources to be found.
    F_ResetAllResourceNamespaces();

    R_InitPatchComposites();
    R_InitFlatTextures();
    R_InitSpriteTextures();

    if(DD_GameLoaded() && gx.UpdateState)
        gx.UpdateState(DD_PRE);

    hadFog = usingFog;
    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
        GL_UseFog(true);

    /**
     * The bulk of this we can do in busy mode unless we are already busy
     * (which can happen during a runtime game change).
     */
    { ddupdateenginestateworker_paramaters_t p;
    p.initiatedBusyMode = !Con_IsBusy();
    if(p.initiatedBusyMode)
    {
        Con_InitProgress(200);
        Con_Busy(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                 "Updating engine state...", DD_UpdateEngineStateWorker, &p);
    }
    else
    {   /// \todo Update the current task name and push progress.
        DD_UpdateEngineStateWorker(&p);
    }
    }

    if(DD_GameLoaded() && gx.UpdateState)
        gx.UpdateState(DD_POST);

    // Reset the anim groups (if in-game)
    Materials_ResetAnimGroups();
}

/* *INDENT-OFF* */
ddvalue_t ddValues[DD_LAST_VALUE - DD_FIRST_VALUE - 1] = {
    {&netGame, 0},
    {&isServer, 0},                         // An *open* server?
    {&isClient, 0},
    {&allowFrames, &allowFrames},
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, 0 /*&displayPlayer*/}, // use R_SetViewPortPlayer() instead
    {&mipmapping, 0},
    {&filterUI, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {0, 0},
    {0, 0}, //{&mouseInverseY, &mouseInverseY},
    {&levelFullBright, &levelFullBright},
    {&CmdReturnValue, 0},
    {&gameReady, &gameReady},
    {&isDedicated, 0},
    {&novideo, 0},
    {&defs.count.mobjs.num, 0},
    {&gotFrame, 0},
    {&playback, 0},
    {&defs.count.sounds.num, 0},
    {&defs.count.music.num, 0},
    {0, 0},
    {&clientPaused, &clientPaused},
    {&weaponOffsetScaleY, &weaponOffsetScaleY},
    {&monochrome, &monochrome},
    {&gameDataFormat, &gameDataFormat},
    {&gameDrawHUD, 0},
    {&upscaleAndSharpenPatches, &upscaleAndSharpenPatches},
    {&symbolicEchoMode, &symbolicEchoMode},
    {&numTexUnits, 0}
};
/* *INDENT-ON* */

/**
 * Get a 32-bit signed integer value.
 */
int DD_GetInteger(int ddvalue)
{
    switch(ddvalue)
    {
    case DD_WINDOW_WIDTH:
        return theWindow->geometry.size.width;

    case DD_WINDOW_HEIGHT:
        return theWindow->geometry.size.height;

    case DD_DYNLIGHT_TEXTURE:
        return (int) GL_PrepareLSTexture(LST_DYNAMIC);

    case DD_NUMLUMPS:
        return F_LumpCount();

    case DD_MAP_MUSIC: {
        gamemap_t* map = P_GetCurrentMap();
        ded_mapinfo_t* mapInfo = Def_GetMapInfo(P_MapUri(map));
        if(!mapInfo) return -1;
        return Def_GetMusicNum(mapInfo->music);
      }
    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;
    if(ddValues[ddvalue].readPtr == 0)
        return 0;
    return *ddValues[ddvalue].readPtr;
}

/**
 * Set a 32-bit signed integer value.
 */
void DD_SetInteger(int ddvalue, int parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
        return;
    if(ddValues[ddvalue].writePtr)
        *ddValues[ddvalue].writePtr = parm;
}

/**
 * Get a pointer to the value of a variable. Not all variables support
 * this. Added for 64-bit support.
 */
void* DD_GetVariable(int ddvalue)
{
    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_SECTOR_COUNT:
        return &numSectors;

    case DD_LINE_COUNT:
        return &numLineDefs;

    case DD_SIDE_COUNT:
        return &numSideDefs;

    case DD_VERTEX_COUNT:
        return &numVertexes;

    case DD_POLYOBJ_COUNT:
        return &numPolyObjs;

    case DD_SEG_COUNT:
        return &numSegs;

    case DD_SUBSECTOR_COUNT:
        return &numSSectors;

    case DD_NODE_COUNT:
        return &numNodes;

    case DD_MATERIAL_COUNT: {
        static uint value;
        value = Materials_Size();
        return &value;
      }
    case DD_TRACE_ADDRESS:
        return &traceLOS;

    case DD_TRANSLATIONTABLES_ADDRESS:
        return translationTables;

    case DD_MAP_NAME: {
        gamemap_t* map = P_GetCurrentMap();
        ded_mapinfo_t* mapInfo = Def_GetMapInfo(P_MapUri(map));
        if(mapInfo && mapInfo->name[0])
        {
            int id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL);
            if(id != -1)
            {
                return defs.text[id].text;
            }
            return mapInfo->name;
        }
        break;
      }
    case DD_MAP_AUTHOR: {
        gamemap_t* map = P_GetCurrentMap();
        ded_mapinfo_t* mapInfo = Def_GetMapInfo(P_MapUri(map));

        if(mapInfo && mapInfo->author[0])
            return mapInfo->author;
        break;
      }
    case DD_MAP_MIN_X: {
        gamemap_t* map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXLEFT];
        else
            return NULL;
      }
    case DD_MAP_MIN_Y: {
        gamemap_t* map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXBOTTOM];
        else
            return NULL;
      }
    case DD_MAP_MAX_X: {
        gamemap_t* map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXRIGHT];
        else
            return NULL;
      }
    case DD_MAP_MAX_Y: {
        gamemap_t* map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXTOP];
        else
            return NULL;
      }
    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[VX];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[VY];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_GRAVITY:
        return &mapGravity;

    case DD_TORCH_RED:
        return &torchColor[CR];

    case DD_TORCH_GREEN:
        return &torchColor[CG];

    case DD_TORCH_BLUE:
        return &torchColor[CB];

    case DD_TORCH_ADDITIVE:
        return &torchAdditive;
#ifdef WIN32
    case DD_WINDOW_HANDLE:
        return Sys_GetWindowHandle(windowIDX);
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC: {
        static timespan_t       fracTic;
        fracTic = gameTime * TICSPERSEC;
        return &fracTic;
      }
    case DD_OPENRANGE:
        return &openrange;

    case DD_OPENTOP:
        return &opentop;

    case DD_OPENBOTTOM:
        return &openbottom;

    case DD_LOWFLOOR:
        return &lowfloor;

    case DD_NUMLUMPS: {
        static int count;
        count = F_LumpCount();
        return &count;
      }
    default: break;
    }

    if(ddvalue >= DD_LAST_VALUE || ddvalue <= DD_FIRST_VALUE)
        return 0;

    // Other values not supported.
    return ddValues[ddvalue].writePtr;
}

/**
 * Set the value of a variable. The pointer can point to any data, its
 * interpretation depends on the variable. Added for 64-bit support.
 */
void DD_SetVariable(int ddvalue, void *parm)
{
    if(ddvalue <= DD_FIRST_VALUE || ddvalue >= DD_LAST_VALUE)
    {
        switch(ddvalue)
        {
        /*case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(float*) parm;
            return;*/

        case DD_GRAVITY:
            mapGravity = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_X:
            pspOffset[VX] = *(float*) parm;
            return;

        case DD_PSPRITE_OFFSET_Y:
            pspOffset[VY] = *(float*) parm;
            return;

        case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
            pspLightLevelMultiplier = *(float*) parm;
            return;

        case DD_TORCH_RED:
            torchColor[CR] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_GREEN:
            torchColor[CG] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_BLUE:
            torchColor[CB] = MINMAX_OF(0, *((float*) parm), 1);
            return;

        case DD_TORCH_ADDITIVE:
            torchAdditive = (*(int*) parm)? true : false;
            break;

        default:
            break;
        }
    }
}

/// \note Part of the Doomsday public API.
materialnamespaceid_t DD_ParseMaterialNamespace(const char* str)
{
    return Materials_ParseNamespace(str);
}

/// \note Part of the Doomsday public API.
texturenamespaceid_t DD_ParseTextureNamespace(const char* str)
{
    return Textures_ParseNamespace(str);
}

/// \note Part of the Doomsday public API.
fontnamespaceid_t DD_ParseFontNamespace(const char* str)
{
    return Fonts_ParseNamespace(str);
}

const ddstring_t* DD_MaterialNamespaceNameForTextureNamespace(texturenamespaceid_t texNamespace)
{
    static const materialnamespaceid_t namespaceIds[TEXTURENAMESPACE_COUNT] = {
        /* TN_SYSTEM */    MN_SYSTEM,
        /* TN_FLATS */     MN_FLATS,
        /* TN_TEXTURES */  MN_TEXTURES,
        /* TN_SPRITES */   MN_SPRITES,
        /* TN_PATCHES */   MN_ANY, // No materials for these yet.

        // -- No Materials for these --
        /* TN_DETAILS */   MN_INVALID,
        /* TN_REFLECTIONS */ MN_INVALID,
        /* TN_MASKS */      MN_INVALID,
        /* TN_MODELSKINS */ MN_INVALID,
        /* TN_MODELREFLECTIONSKINS */ MN_INVALID,
        /* TN_LIGHTMAPS */ MN_INVALID,
        /* TN_FLAREMAPS */ MN_INVALID
    };
    materialnamespaceid_t namespaceId = MN_INVALID; // Unknown.
    if(VALID_TEXTURENAMESPACEID(texNamespace))
        namespaceId = namespaceIds[texNamespace-TEXTURENAMESPACE_FIRST];
    return Materials_NamespaceName(namespaceId);
}

materialid_t DD_MaterialForTextureUniqueId(texturenamespaceid_t texNamespaceId, int uniqueId)
{
    textureid_t texId = Textures_TextureForUniqueId(texNamespaceId, uniqueId);
    ddstring_t* texPath;
    materialid_t matId;
    Uri* uri;

    if(texId == NOTEXTUREID) return NOMATERIALID;

    texPath = Textures_ComposePath(texId);
    uri = Uri_NewWithPath2(Str_Text(texPath), RC_NULL);
    Uri_SetScheme(uri, Str_Text(DD_MaterialNamespaceNameForTextureNamespace(texNamespaceId)));
    matId = Materials_ResolveUri2(uri, true/*quiet please*/);
    Uri_Delete(uri);
    Str_Delete(texPath);
    return matId;
}

/**
 * Gets the data of a player.
 */
ddplayer_t* DD_GetPlayer(int number)
{
    return (ddplayer_t *) &ddPlayers[number].shared;
}

/**
 * Convert propertyType enum constant into a string for error/debug messages.
 */
const char* value_Str(int val)
{
    static char valStr[40];
    struct val_s {
        int                 val;
        const char*         str;
    } valuetypes[] = {
        { DDVT_BOOL, "DDVT_BOOL" },
        { DDVT_BYTE, "DDVT_BYTE" },
        { DDVT_SHORT, "DDVT_SHORT" },
        { DDVT_INT, "DDVT_INT" },
        { DDVT_UINT, "DDVT_UINT" },
        { DDVT_FIXED, "DDVT_FIXED" },
        { DDVT_ANGLE, "DDVT_ANGLE" },
        { DDVT_FLOAT, "DDVT_FLOAT" },
        { DDVT_LONG, "DDVT_LONG" },
        { DDVT_ULONG, "DDVT_ULONG" },
        { DDVT_PTR, "DDVT_PTR" },
        { DDVT_BLENDMODE, "DDVT_BLENDMODE" },
        { 0, NULL }
    };
    uint i;

    for(i = 0; valuetypes[i].str; ++i)
        if(valuetypes[i].val == val)
            return valuetypes[i].str;

    sprintf(valStr, "(unnamed %i)", val);
    return valStr;
}

D_CMD(Load)
{
    boolean didLoadGame = false, didLoadResource = false;
    ddstring_t foundPath, searchPath;
    Game* game;
    int arg = 1;

    Str_Init(&searchPath);
    Str_Set(&searchPath, argv[arg]);
    Str_Strip(&searchPath);
    if(Str_IsEmpty(&searchPath))
    {
        Str_Free(&searchPath);
        return false;
    }
    F_FixSlashes(&searchPath, &searchPath);

    // Ignore attempts to load directories.
    if(Str_RAt(&searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"loaded\" (only files and/or known games).\n");
        Str_Free(&searchPath);
        return true;
    }

    // Are we loading a game?
    game = findGameForIdentityKey(Str_Text(&searchPath));
    if(game)
    {
        if(!allGameResourcesFound(game))
        {
            Con_Message("Failed to locate all required startup resources:\n");
            printGameResources(game, true, RF_STARTUP);
            Con_Message("%s (%s) cannot be loaded.\n", Str_Text(Game_Title(game)), Str_Text(Game_IdentityKey(game)));
            Str_Free(&searchPath);
            return true;
        }
        if(!DD_ChangeGame(game))
        {
            Str_Free(&searchPath);
            return false;
        }
        didLoadGame = true;
        ++arg;
    }

    // Try the resource locator.
    Str_Init(&foundPath);
    for(; arg < argc; ++arg)
    {
        Str_Set(&searchPath, argv[arg]);
        Str_Strip(&searchPath);

        if(F_FindResource3(RC_PACKAGE, Str_Text(&searchPath), &foundPath, RLF_MATCH_EXTENSION) != 0 &&
           F_AddFile(Str_Text(&foundPath), 0, false))
            didLoadResource = true;
    }
    Str_Free(&foundPath);

    if(didLoadResource)
        DD_UpdateEngineState();

    Str_Free(&searchPath);
    return (didLoadGame || didLoadResource);
}

D_CMD(Unload)
{
    ddstring_t foundPath, searchPath;
    int i, result = 0;
    Game* game;

    // No arguments; unload the current game if loaded.
    if(argc == 1)
    {
        if(!DD_GameLoaded())
        {
            Con_Message("There is no game currently loaded.\n");
            return true;
        }
        return DD_ChangeGame(nullGame);
    }

    Str_Init(&searchPath);
    Str_Set(&searchPath, argv[1]);
    Str_Strip(&searchPath);
    if(Str_IsEmpty(&searchPath))
    {
        Str_Free(&searchPath);
        return false;
    }
    F_FixSlashes(&searchPath, &searchPath);

    // Ignore attempts to unload directories.
    if(Str_RAt(&searchPath, 0) == '/')
    {
        Con_Message("Directories cannot be \"unloaded\" (only files and/or known games).\n");
        Str_Free(&searchPath);
        return true;
    }

    // Unload the current game if specified.
    if(argc == 2 && (game = findGameForIdentityKey(Str_Text(&searchPath))) != 0)
    {
        Str_Free(&searchPath);
        if(DD_GameLoaded())
        {
            return DD_ChangeGame(nullGame);
        }

        Con_Message("%s is not currently loaded.\n", Str_Text(Game_IdentityKey(game)));
        return true;
    }

    /// Try the resource locator.
    Str_Init(&foundPath);
    for(i = 1; i < argc; ++i)
    {
        Str_Set(&searchPath, argv[i]);
        Str_Strip(&searchPath);

        if(!F_FindResource2(RC_PACKAGE, Str_Text(&searchPath), &foundPath))
            continue;

        // Do not attempt to unload a resource required by the current game.
        if(isRequiredResource(theGame, Str_Text(&foundPath)))
        {
            Con_Message("\"%s\" is required by the current game and cannot be unloaded in isolation.\n",
                F_PrettyPath(Str_Text(&foundPath)));
            continue;
        }

        // We can safely remove this file.
        if(F_RemoveFile(Str_Text(&foundPath)))
        {
            result = 1;
        }
    }

    Str_Free(&foundPath);
    Str_Free(&searchPath);
    return result != 0;
}

D_CMD(Reset)
{
    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    if(!DD_GameLoaded())
    {
        Con_Message("No game is presently loaded.\n");
        return true;
    }
    DD_ChangeGame2(theGame, true);
    return true;
}

static int C_DECL compareGameByName(const void* a, const void* b)
{
    return stricmp(Str_Text(Game_Title(*(Game**)a)), Str_Text(Game_Title(*(Game**)b)));
}

D_CMD(ListGames)
{
    const int numAvailableGames = gamesCount;
    if(numAvailableGames)
    {
        int i, numCompleteGames = 0;
        Game** gamePtrs;

        Con_FPrintf(CPF_YELLOW, "Registered Games:\n");
        Con_Printf("Key: '!'= Incomplete/Not playable '*'= Loaded\n");
        Con_PrintRuler();

        // Sort a copy of games so we get a nice alphabetical list.
        gamePtrs = (Game**)malloc(gamesCount * sizeof *gamePtrs);
        if(!gamePtrs) Con_Error("CCmdListGames: Failed on allocation of %lu bytes for sorted Game list.", (unsigned long) (gamesCount * sizeof *gamePtrs));

        memcpy(gamePtrs, games, gamesCount * sizeof *gamePtrs);
        qsort(gamePtrs, gamesCount, sizeof *gamePtrs, compareGameByName);

        for(i = 0; i < gamesCount; ++i)
        {
            Game* game = gamePtrs[i];

            Con_Printf(" %s %-16s %s (%s)\n", theGame == game? "*" :
                                          !allGameResourcesFound(game)? "!" : " ",
                       Str_Text(Game_IdentityKey(game)), Str_Text(Game_Title(game)),
                       Str_Text(Game_Author(game)));

            if(allGameResourcesFound(game))
                numCompleteGames++;
        }

        Con_PrintRuler();
        Con_Printf("%i of %i games playable.\n", numCompleteGames, numAvailableGames);
        Con_Printf("Use the 'load' command to load a game. For example: \"load gamename\".\n");

        free(gamePtrs);
    }
    else
    {
        Con_Printf("No Registered Games.\n");
    }

    return true;
}

#ifdef UNIX
/**
 * Some routines not available on the *nix platform.
 */
char* strupr(char* string)
{
    char* ch = string;
    for(; *ch; ch++)
        *ch = toupper(*ch);
    return string;
}

char* strlwr(char* string)
{
    char* ch = string;
    for(; *ch; ch++)
        *ch = tolower(*ch);
    return string;
}
#endif

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be
 * written to the output buffer @c str. The output will always contain a
 * terminating null character.
 *
 * @param str           Output buffer.
 * @param size          Size of the output buffer.
 * @param format        Format of the output.
 * @param ap            Variable-size argument list.
 *
 * @return              Number of characters written to the output buffer
 *                      if lower than or equal to @c size, else @c -1.
 */
int dd_vsnprintf(char* str, size_t size, const char* format, va_list ap)
{
    int result = vsnprintf(str, size, format, ap);

#ifdef WIN32
    // Always terminate.
    str[size - 1] = 0;
    return result;
#else
    return result >= (int)size? -1 : (int)size;
#endif
}

/**
 * Prints a formatted string into a fixed-size buffer. At most @c size
 * characters will be written to the output buffer @c str. The output will
 * always contain a terminating null character.
 *
 * @param str           Output buffer.
 * @param size          Size of the output buffer.
 * @param format        Format of the output.
 *
 * @return              Number of characters written to the output buffer
 *                      if lower than or equal to @c size, else @c -1.
 */
int dd_snprintf(char* str, size_t size, const char* format, ...)
{
    int result = 0;

    va_list args;
    va_start(args, format);
    result = dd_vsnprintf(str, size, format, args);
    va_end(args);

    return result;
}
