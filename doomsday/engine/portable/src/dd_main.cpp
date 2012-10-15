/**
 * @file dd_main.cpp
 *
 * Engine core.
 *
 * @ingroup core
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifdef WIN32
#  define _WIN32_DCOM
#  include <objbase.h>
#endif

#include "de_platform.h"

#ifdef UNIX
#  include <ctype.h>
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

#include "abstractresource.h"
#include "displaymode.h"
#include "filedirectory.h"
#include "m_misc.h"
#include "resourcenamespace.h"
#include "texture.h"
#include "updater.h"

using namespace de;

typedef struct ddvalue_s {
    int*            readPtr;
    int*            writePtr;
} ddvalue_t;

static int DD_StartupWorker(void* parameters);
static int DD_DummyWorker(void* parameters);
static void DD_AutoLoad(void);

extern int renderTextures;
extern int monochrome;

filename_t ddBasePath = ""; // Doomsday root directory is at...?
filename_t ddRuntimePath, ddBinPath;

int isDedicated;

int verbose; // For debug messages (-verbose).

// List of file names, whitespace seperating (written to .cfg).
char* startupFiles = "";

// Id of the currently running title finale if playing, else zero.
finaleid_t titleFinale;

// List of session data files (specified via the command line or in a cfg, or
// found using the default search algorithm (e.g., /auto and DOOMWADDIR)).
static ddstring_t** sessionResourceFileList;
static size_t numSessionResourceFileList;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

// The Game collection.
static de::GameCollection* games;

D_CMD(CheckForUpdates)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Con_Message("Checking for available updates...\n");
    Updater_CheckNow(false/* don't notify */);
    return true;
}

D_CMD(CheckForUpdatesAndNotify)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    /// @todo Combine into the same command with CheckForUpdates?
    Con_Message("Checking for available updates...\n");
    Updater_CheckNow(true/* do notify */);
    return true;
}

D_CMD(LastUpdated)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Updater_PrintLastUpdated();
    return true;
}

D_CMD(ShowUpdateSettings)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    Updater_ShowSettings();
    return true;
}

/**
 * Register the engine commands and variables.
 */
void DD_Register(void)
{
    C_CMD("update",          "", CheckForUpdates);
    C_CMD("updateandnotify", "", CheckForUpdatesAndNotify);
    C_CMD("updatesettings",  "", ShowUpdateSettings);
    C_CMD("lastupdated",     "", LastUpdated);

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
    BspBuilder_Register();
    UI_Register();
    Demo_Register();
    P_ControlRegister();
    FI_Register();
}

static void addToPathList(ddstring_t*** list, size_t* listSize, const char* rawPath)
{
    ddstring_t* newPath = Str_New();
    DENG_ASSERT(list && listSize && rawPath && rawPath[0]);

    Str_Set(newPath, rawPath);
    F_FixSlashes(newPath, newPath);
    F_ExpandBasePath(newPath, newPath);

    *list = (ddstring_t**) M_Realloc(*list, sizeof(**list) * ++(*listSize));
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
        F_AddFile(token);
        token = strtok(NULL, ATWSEPS);
    }
    free(buffer);

#undef ATWSEPS
}

static void destroyPathList(ddstring_t*** list, size_t* listSize)
{
    DENG_ASSERT(list && listSize);
    if(*list)
    {
        size_t i;
        for(i = 0; i < *listSize; ++i)
            Str_Delete((*list)[i]);
        M_Free(*list); *list = 0;
    }
    *listSize = 0;
}

boolean DD_GameLoaded(void)
{
    DENG_ASSERT(games);
    return !isNullGame(games->currentGame());
}

void DD_DestroyGames(void)
{
    destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);

    delete games;
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
    fontName = R_ChooseVariableFont(FS_NORMAL, Window_Width(theWindow),
                                               Window_Height(theWindow));
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

/**
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data directory
 * are loaded to the file system.
 *
 * @return Number of new files that were loaded.
 */
static int addFilesFromAutoData(void)
{
    static const char* extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        0
    };

    int count = 0;
    AutoStr* pattern = AutoStr_NewStd();
    for(uint extIdx = 0; extensions[extIdx]; ++extIdx)
    {
        Str_Clear(pattern);
        Str_Appendf(pattern, "%sauto/*.%s", Str_Text(&games->currentGame().dataPath()), extensions[extIdx]);

        FS1::PathList found;
        if(App_FileSystem()->findAllPaths(Str_Text(pattern), 0, found))
        {
            DENG2_FOR_EACH(i, found, FS1::PathList::const_iterator)
            {
                // Ignore directories.
                if(i->attrib & A_SUBDIR) continue;

                QByteArray foundPathUtf8 = i->path.toUtf8();
                if(F_AddFile(foundPathUtf8.constData()))
                {
                    count += 1;
                }
            }
        }
    }
    return count;
}

typedef struct {
    ddstring_t*** list;
    size_t* listSize;
    int count; /// Number of files loaded successfully.
} listfilesfromautodata_params_t;

/**
 * (f_allresourcepaths_callback_t)
 */
static int listFilesWorker(char const* fileName, PathDirectoryNodeType type, void* parameters)
{
    DENG_ASSERT(fileName && parameters);
    // We are only interested in files.
    if(type == PT_LEAF)
    {
        listfilesfromautodata_params_t* data = (listfilesfromautodata_params_t*)parameters;
        addToPathList(data->list, data->listSize, fileName);
        data->count += 1;
    }
    return 0; // Continue searching.
}

/**
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data
 * directory are added to the specified file list.
 *
 * @return Number of new files that were added to the list.
 */
static int listFilesFromAutoData(ddstring_t*** list, size_t* listSize)
{
    static const char* extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        0
    };

    listfilesfromautodata_params_t data;
    ddstring_t pattern;
    uint i;

    if(!list || !listSize) return 0;

    data.list = list;
    data.listSize = listSize;
    data.count = 0;

    Str_Init(&pattern);
    for(i = 0; extensions[i]; ++i)
    {
        Str_Clear(&pattern);
        Str_Appendf(&pattern, "%sauto/*.%s", Str_Text(&games->currentGame().dataPath()), extensions[i]);
        F_AllResourcePaths2(Str_Text(&pattern), 0, listFilesWorker, (void*)&data);
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

static void loadResource(AbstractResource* res)
{
    if(!res) return;

    switch(AbstractResource_ResourceClass(res))
    {
    case RC_PACKAGE: {
        ddstring_t const* path = AbstractResource_ResolvedPath(res, false/*do not locate resource*/);
        if(path)
        {
            struct abstractfile_s* file = F_AddFile(Str_Text(path));
            if(file)
            {
                // Mark this as an original game resource.
                F_SetCustom(file, false);

                // Print the 'CRC' number of IWADs, so they can be identified.
                /// @todo fixme
                //if(FT_WADFILE == AbstractFile_Type(file))
                //    Con_Message("  IWAD identification: %08x\n", WadFile_CalculateCRC((WadFile*)file));

            }
        }
        break; }

    default: Con_Error("loadGameResource: No resource loader found for %s.",
                       F_ResourceClassStr(AbstractResource_ResourceClass(res)));
    }
}

typedef struct {
    /// @c true iff caller (i.e., DD_ChangeGame) initiated busy mode.
    boolean initiatedBusyMode;
} ddgamechange_paramaters_t;

static int DD_BeginGameChangeWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    P_InitMapUpdate();
    P_InitMapEntityDefs();

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    DAM_Init();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_LoadGameStartupResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    // Reset file Ids so previously seen files can be processed again.
    F_ResetFileIds();
    F_ResetAllResourceNamespaces();

    F_InitVirtualDirectoryMappings();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(DD_GameLoaded())
    {
        ddstring_t temp;

        // Create default Auto mappings in the runtime directory.
        // Data class resources.
        Str_Init(&temp);
        Str_Appendf(&temp, "%sauto", Str_Text(&games->currentGame().dataPath()));
        F_AddVirtualDirectoryMapping("auto", Str_Text(&temp));

        // Definition class resources.
        Str_Clear(&temp);
        Str_Appendf(&temp, "%sauto", Str_Text(&games->currentGame().defsPath()));
        F_AddVirtualDirectoryMapping("auto", Str_Text(&temp));
        Str_Free(&temp);
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * @note  Duplicate processing of the same file is automatically guarded
     *        against by the virtual file system layer.
     */
    Con_Message("Loading game resources%s\n", verbose >= 1? ":" : "...");

    { int numResources;
    AbstractResource* const* resources;
    if((resources = games->currentGame().resources(RC_PACKAGE, &numResources)))
    {
        AbstractResource* const* resIt;
        for(resIt = resources; *resIt; resIt++)
        {
            loadResource(*resIt);

            // Update our progress.
            if(p->initiatedBusyMode)
            {
                Con_SetProgress(((resIt - resources)+1) * (200-50)/numResources -1);
            }
        }
    }}

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int addListFiles(ddstring_t*** list, size_t* listSize, resourcetype_t resType)
{
    size_t i;
    int count = 0;
    if(!list || !listSize) return 0;
    for(i = 0; i < *listSize; ++i)
    {
        if(resType != F_GuessResourceTypeByName(Str_Text((*list)[i]))) continue;
        if(F_AddFile(Str_Text((*list)[i])))
        {
            count += 1;
        }
    }
    return count;
}

static int DD_LoadAddonResourcesWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    DENG_ASSERT(p);

    /**
     * Add additional game-startup files.
     * @note These must take precedence over Auto but not game-resource files.
     */
    if(startupFiles && startupFiles[0])
        parseStartupFilePathsAndAddFiles(startupFiles);

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    if(DD_GameLoaded())
    {
        /**
         * Phase 3: Add real files from the Auto directory.
         * First ZIPs then WADs (they may contain WAD files).
         */
        listFilesFromAutoData(&sessionResourceFileList, &numSessionResourceFileList);
        if(numSessionResourceFileList > 0)
        {
            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, RT_ZIP);

            addListFiles(&sessionResourceFileList, &numSessionResourceFileList, RT_WAD);
        }

        // Final autoload round.
        DD_AutoLoad();
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(180);

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    F_InitLumpDirectoryMappings();
    F_ResetAllResourceNamespaces();

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

static int DD_ActivateGameWorker(void* parameters)
{
    ddgamechange_paramaters_t* p = (ddgamechange_paramaters_t*)parameters;
    uint i;
    DENG_ASSERT(p);

    // Texture resources are located now, prior to initializing the game.
    R_InitPatchComposites();
    R_InitFlatTextures();
    R_InitSpriteTextures();

    if(p->initiatedBusyMode)
        Con_SetProgress(50);

    // Now that resources have been located we can begin to initialize the game.
    if(!DD_GameLoaded() && gx.PreInit)
        gx.PreInit(games->id(games->currentGame()));

    if(p->initiatedBusyMode)
        Con_SetProgress(100);

    /**
     * Parse the game's main config file.
     * If a custom top-level config is specified; let it override.
     */
    { ddstring_t const* configFileName = 0;
    ddstring_t tmp;
    if(CommandLine_CheckWith("-config", 1))
    {
        Str_Init(&tmp); Str_Set(&tmp, CommandLine_Next());
        F_FixSlashes(&tmp, &tmp);
        configFileName = &tmp;
    }
    else
    {
        configFileName = &games->currentGame().mainConfig();
    }

    Con_Message("Parsing primary config \"%s\"...\n", F_PrettyPath(Str_Text(configFileName)));
    Con_ParseCommands2(Str_Text(configFileName), CPCF_SET_DEFAULT | CPCF_ALLOW_SAVE_STATE);
    if(configFileName == &tmp)
        Str_Free(&tmp);
    }

    if(!isDedicated && DD_GameLoaded())
    {
        // Apply default control bindings for this game.
        B_BindGameDefaults();

        // Read bindings for this game and merge with the working set.
        Con_ParseCommands2(Str_Text(&games->currentGame().bindingConfig()), CPCF_ALLOW_SAVE_BINDINGS);
    }

    if(p->initiatedBusyMode)
        Con_SetProgress(120);

    Def_Read();

    if(p->initiatedBusyMode)
        Con_SetProgress(130);

    R_InitSprites(); // Fully initialize sprites.
    R_InitModels();

    Def_PostInit();

    DD_ReadGameHelp();

    // Re-init to update the title, background etc.
    Rend_ConsoleInit();

    // Reset the tictimer so than any fractional accumulation is not added to
    // the tic/game timer of the newly-loaded game.
    gameTime = 0;
    DD_ResetTimer();

    // Make sure that the next frame does not use a filtered viewer.
    R_ResetViewer();

    // Invalidate old cmds and init player values.
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t* plr = &ddPlayers[i];

        plr->extraLight = plr->targetExtraLight = 0;
        plr->extraLightCounter = 0;
    }

    if(gx.PostInit)
    {
        gx.PostInit();
    }

    if(p->initiatedBusyMode)
    {
        Con_SetProgress(200);
        BusyMode_WorkerEnd();
    }
    return 0;
}

struct gamecollection_s* App_GameCollection()
{
    return reinterpret_cast<struct gamecollection_s*>(games);
}

struct game_s* App_CurrentGame()
{
    return reinterpret_cast<struct game_s*>(&games->currentGame());
}

static void populateGameInfo(GameInfo& info, de::Game& game)
{
    info.identityKey = Str_Text(&game.identityKey());
    info.title       = Str_Text(&game.title());
    info.author      = Str_Text(&game.author());
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
        populateGameInfo(*info, games->currentGame());
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
    DENG_ASSERT(games);

    de::Game& game = games->byId(gameId);

    if(!VALID_RESOURCE_CLASS(rclass))
        Con_Error("DD_AddGameResource: Unknown resource class %i.", (int)rclass);
    if(!_names || !_names[0] || !strcmp(_names, ";"))
        Con_Error("DD_AddGameResource: Invalid name argument.");

    AbstractResource* rec;
    if(0 == (rec = AbstractResource_New(rclass, rflags)))
        Con_Error("DD_AddGameResource: Unknown error occured during AbstractResource::Construct.");

    // Add a name list to the game record.
    ddstring_t str; Str_InitStd(&str);
    Str_Set(&str, _names);
    // Ensure the name list has the required terminating semicolon.
    if(Str_RAt(&str, 0) != ';')
        Str_Append(&str, ";");

    char const* p = Str_Text(&str);
    ddstring_t name; Str_Init(&name);
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

    game.addResource(rclass, *rec);

    Str_Free(&str);
}

/// @note Part of the Doomsday public API.
gameid_t DD_DefineGame(GameDef const* def)
{
    if(!def)
    {
#if _DEBUG
        Con_Message("Warning: DD_DefineGame: Received invalid GameDef (=NULL), ignoring.");
#endif
        return 0; // Invalid id.
    }

    // Game mode identity keys must be unique. Ensure that is the case.
    DENG_ASSERT(games);
    try
    {
        /*de::Game& game =*/ games->byIdentityKey(def->identityKey);
#if _DEBUG
        Con_Message("Warning: DD_DefineGame: Failed adding game \"%s\", identity key '%s' already in use, ignoring.\n", def->defaultTitle, def->identityKey);
#endif
        return 0; // Invalid id.
    }
    catch(de::GameCollection::NotFoundError)
    {} // Ignore the error.

    de::Game* game = de::Game::fromDef(*def);
    if(!game) return 0; // Invalid def.

    // Add this game to our records.
    game->setPluginId(DD_PluginIdForActiveHook());
    games->add(*game);
    return games->id(*game);
}

/// @note Part of the Doomsday public API.
gameid_t DD_GameIdForKey(const char* identityKey)
{
    DENG_ASSERT(games);
    try
    {
        return games->id(games->byIdentityKey(identityKey));
    }
    catch(de::GameCollection::NotFoundError)
    {
        DEBUG_Message(("Warning: DD_GameIdForKey: Game \"%s\" not defined.\n", identityKey));
    }
    return 0; // Invalid id.
}

/**
 * Switch to/activate the specified game.
 */
bool DD_ChangeGame(de::Game& game, bool allowReload = false)
{
    bool isReload = false;

    // Ignore attempts to re-load the current game?
    if(games->isCurrentGame(game))
    {
        if(!allowReload)
        {
            if(DD_GameLoaded())
                Con_Message("%s (%s) - already loaded.\n", Str_Text(&game.title()), Str_Text(&game.identityKey()));
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
        if(gx.Shutdown)
            gx.Shutdown();
        Con_SaveDefaults();

        LO_Clear();
        R_DestroyObjlinkBlockmap();
        R_ClearAnimGroups();

        P_ControlShutdown();
        Con_Execute(CMDS_DDAY, "clearbindings", true, false);
        B_BindDefaults();
        B_InitialContextActivations();

        for(uint i = 0; i < DDMAXPLAYERS; ++i)
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

        // If a map was loaded; unload it.
        if(theMap)
        {
            GameMap_ClMobjReset(theMap);
        }
        // Clear player data, too, since we just lost all clmobjs.
        Cl_InitPlayers();

        P_SetCurrentMap(0);
        Z_FreeTags(PU_GAMESTATIC, PU_PURGELEVEL - 1);

        P_ShutdownMapEntityDefs();

        R_ShutdownSvgs();
        R_DestroyColorPalettes();

        Fonts_ClearRuntime();
        Textures_ClearRuntime();

        Sfx_InitLogical();

        /// @todo Why is this being done here?
        if(theMap)
        {
            GameMap_InitThinkerLists(theMap, 0x1|0x2);
        }

        Con_ClearDatabases();

        { // Tell the plugin it is being unloaded.
            void* unloader = DD_FindEntryPoint(games->currentGame().pluginId(), "DP_Unload");
            DEBUG_Message(("DD_ChangeGame: Calling DP_Unload (%p)\n", unloader));
            if(unloader) ((pluginfunc_t)unloader)();
        }

        // The current game is now the special "null-game".
        games->setCurrentGame(games->nullGame());

        Con_InitDatabases();
        DD_Register();
        I_InitVirtualInputDevices();

        R_InitSvgs();
        R_InitViewWindow();

        F_Reset();
        F_ResetAllResourceNamespaces();
    }

    FI_Shutdown();
    titleFinale = 0; // If the title finale was in progress it isn't now.

    /// @todo Materials database should not be shutdown during a reload.
    Materials_Shutdown();

    VERBOSE(
        if(!isNullGame(game))
        {
            Con_Message("Selecting game '%s'...\n", Str_Text(&game.identityKey()));
        }
        else if(!isReload)
        {
            Con_Message("Unloaded game.\n");
        }
    )

    // Remove all entries; some may have been created by the game plugin (if it used libdeng2 C++ API).
    LogBuffer_Clear();

    Library_ReleaseGames();

    char buf[256];
    DD_ComposeMainWindowTitle(buf);
    Window_SetTitle(theWindow, buf);

    if(!DD_IsShuttingDown())
    {
        // Re-initialize subsystems needed even when in ringzero.
        if(!exchangeEntryPoints(game.pluginId()))
        {
            Con_Message("Warning: DD_ChangeGame: Failed exchanging entrypoints with plugin %i, aborting.\n", (int)game.pluginId());
            return false;
        }

        Materials_Init();
        FI_Init();
    }

    // This is now the current game.
    games->setCurrentGame(game);

    DD_ComposeMainWindowTitle(buf);
    Window_SetTitle(theWindow, buf);

    /**
     * If we aren't shutting down then we are either loading a game or switching
     * to ringzero (the current game will have already been unloaded).
     */
    if(!DD_IsShuttingDown())
    {
        /**
         * The bulk of this we can do in busy mode unless we are already busy
         * (which can happen if a fatal error occurs during game load and we must
         * shutdown immediately; Sys_Shutdown will call back to load the special
         * "null-game" game).
         */
        const int busyMode = BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0);
        ddgamechange_paramaters_t p;
        BusyTask gameChangeTasks[] = {
            // Phase 1: Initialization.
            { DD_BeginGameChangeWorker,          &p, busyMode, "Loading game...",   200, 0.0f, 0.1f },

            // Phase 2: Loading "startup" resources.
            { DD_LoadGameStartupResourcesWorker, &p, busyMode, NULL,                200, 0.1f, 0.3f },

            // Phase 3: Loading "addon" resources.
            { DD_LoadAddonResourcesWorker,       &p, busyMode, "Loading addons...", 200, 0.3f, 0.7f },

            // Phase 4: Game activation.
            { DD_ActivateGameWorker,             &p, busyMode, "Starting game...",  200, 0.7f, 1.0f }
        };

        p.initiatedBusyMode = !BusyMode_Active();

        if(DD_GameLoaded())
        {
            // Tell the plugin it is being loaded.
            /// @todo Must this be done in the main thread?
            void* loader = DD_FindEntryPoint(games->currentGame().pluginId(), "DP_Load");
            DEBUG_Message(("DD_ChangeGame: Calling DP_Load (%p)\n", loader));
            if(loader) ((pluginfunc_t)loader)();
        }

        /// @kludge Use more appropriate task names when unloading a game.
        if(isNullGame(game))
        {
            gameChangeTasks[0].name = "Unloading game...";
            gameChangeTasks[3].name = "Switching to ringzero...";
        }
        // kludge end

        BusyMode_RunTasks(gameChangeTasks, sizeof(gameChangeTasks)/sizeof(gameChangeTasks[0]));

        // Process any GL-related tasks we couldn't while Busy.
        Rend_ParticleLoadExtraTextures();

        if(DD_GameLoaded())
        {
            de::Game::printBanner(games->currentGame());
        }
        else
        {
            // Lets play a nice title animation.
            DD_StartTitle();
        }
    }

    /**
     * Clear any input events we may have accumulated during this process.
     * @note Only necessary here because we might not have been able to use
     *       busy mode (which would normally do this for us on end).
     */
    DD_ClearEvents();
    return true;
}

boolean DD_IsShuttingDown(void)
{
    return Sys_IsShuttingDown();
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
    while((numNewFiles = addFilesFromAutoData()) > 0)
    {
        VERBOSE( Con_Message("Autoload round completed with %i new files.\n", numNewFiles) );
    }
}

/**
 * Attempt to determine which game is to be played.
 *
 * @todo Logic here could be much more elaborate but is it necessary?
 */
de::Game* DD_AutoselectGame(void)
{
    if(CommandLine_CheckWith("-game", 1))
    {
        char const* identityKey = CommandLine_Next();
        try
        {
            de::Game& game = games->byIdentityKey(identityKey);
            if(game.allStartupResourcesFound())
            {
                return &game;
            }
        }
        catch(de::GameCollection::NotFoundError)
        {} // Ignore the error.
    }

    // If but one lonely game; select it.
    if(games->numPlayable() == 1)
    {
        return games->firstPlayable();
    }

    // We don't know what to do.
    return NULL;
}

int DD_EarlyInit(void)
{
    // Determine the requested degree of verbosity.
    verbose = CommandLine_Exists("-verbose");

    // Bring the console online as soon as we can.
    DD_ConsoleInit();

    Con_InitDatabases();

    // Register the engine's console commands and variables.
    DD_Register();

    // Bring the window manager online.
    Sys_InitWindowManager();

    // Instantiate the Games collection.
    games = new de::GameCollection();

    return true;
}

/**
 * This gets called when the main window is ready for GL init. The application
 * event loop is already running.
 */
void DD_FinishInitializationAfterWindowReady(void)
{    
#ifdef WIN32
    // Now we can get the color transfer table as the window is available.
    DisplayMode_SaveOriginalColorTransfer();
#endif

    if(!Sys_GLInitialize())
    {
        Con_Error("Error initializing OpenGL.\n");
    }
    else
    {
        char buf[256];
        DD_ComposeMainWindowTitle(buf);
        Window_SetTitle(theWindow, buf);
    }

    // Now we can start executing the engine's main loop.
    LegacyCore_SetLoopFunc(DD_GameLoopCallback);

    // Initialize engine subsystems and initial state.
    if(!DD_Init())
    {
        exit(2); // Cannot continue...
        return;
    }

    // Start drawing with the game loop drawer.
    Window_SetDrawFunc(Window_Main(), DD_GameLoopDrawer);
}

/**
 * Engine initialization. After completed, the game loop is ready to be started.
 * Called from the app entrypoint function.
 *
 * @return  @c true on success, @c false if an error occurred.
 */
boolean DD_Init(void)
{
    // By default, use the resolution defined in (default).cfg.
    //int winWidth = defResX, winHeight = defResY, winBPP = defBPP, winX = 0, winY = 0;
    //uint winFlags = DDWF_VISIBLE | DDWF_CENTER | (defFullscreen? DDWF_FULLSCREEN : 0);
    //boolean noCenter = false;
    //int i; //, exitCode = 0;

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
        ASSERT_64BIT(int64_t);
#else
        ASSERT_NOT_64BIT(ptr);
#endif
    }
#endif

    if(!GL_EarlyInit())
    {
        Sys_CriticalMessage("GL_EarlyInit() failed.");
        return false;
    }

    /*
    DENG_ASSERT(!Sys_GLCheckError());
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
    DENG_ASSERT(!Sys_GLCheckError());
*/

    // Initialize the subsystems needed prior to entering busy mode for the first time.
    Sys_Init();
    F_Init();

    Fonts_Init();
    FR_Init();
    DD_InitInput();

    // Enter busy mode until startup complete.
    Con_InitProgress2(200, 0, .25f); // First half.
    BusyMode_RunNewTaskWithName(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_StartupWorker, 0, "Starting up...");

    // Engine initialization is complete. Now finish up with the GL.
    GL_Init();
    GL_InitRefresh();

    // Do deferred uploads.
    Con_InitProgress2(200, .25f, .25f); // Stop here for a while.
    BusyMode_RunNewTaskWithName(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                DD_DummyWorker, 0, "Buffering...");

    // Add resource paths specified using -iwad on the command line.
    { resourcenamespaceid_t rnId = F_DefaultResourceNamespaceForClass(RC_PACKAGE);
    int p;

    for(p = 0; p < CommandLine_Count(); ++p)
    {
        if(!CommandLine_IsMatchingAlias("-iwad", CommandLine_At(p)))
            continue;

        while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
        {
            const char* filePath = CommandLine_PathAt(p);
            directory_t* dir;
            Uri* searchPath;

            /// @todo Do not add these as search paths, publish them directly to
            ///       the FileDirectory owned by the "packages" ResourceNamespace.
            dir = Dir_ConstructFromPathDir(filePath);
            searchPath = Uri_NewWithPath2(Dir_Path(dir), RC_PACKAGE);

            F_AddSearchPathToResourceNamespace(rnId, SPF_NO_DESCEND, searchPath, SPG_DEFAULT);

            Uri_Delete(searchPath);
            Dir_Delete(dir);
        }

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }}

    // Try to locate all required data files for all registered games.
    Con_InitProgress2(200, .25f, 1); // Second half.
    games->locateAllResources();

    /*
    // Unless we reenter busy-mode due to automatic game selection, we won't be
    // drawing anything further until DD_GameLoop; so lets clean up.
    if(!novideo)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        GL_DoUpdate();
    }
    */

    // Attempt automatic game selection.
    if(!CommandLine_Exists("-noautoselect"))
    {
        de::Game* game = DD_AutoselectGame();

        if(game)
        {
            // An implicit game session has been defined.
            int p;

            // Add all resources specified using -file options on the command line
            // to the list for this session.
            for(p = 0; p < CommandLine_Count(); ++p)
            {
                if(!CommandLine_IsMatchingAlias("-file", CommandLine_At(p))) continue;

                while(++p != CommandLine_Count() && !CommandLine_IsOption(p))
                {
                    addToPathList(&sessionResourceFileList, &numSessionResourceFileList, CommandLine_PathAt(p));
                }

                p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
            }

            // Begin the game session.
            DD_ChangeGame(*game);

            // We do not want to load these resources again on next game change.
            destroyPathList(&sessionResourceFileList, &numSessionResourceFileList);
        }
    }

    // Re-initialize the resource locator as there are now new resources to be found
    // on existing search paths (probably that is).
    F_InitLumpDirectoryMappings();
    F_ResetAllResourceNamespaces();

    // One-time execution of various command line features available during startup.
    if(CommandLine_CheckWith("-dumplump", 1))
    {
        const char* name = CommandLine_Next();
        lumpnum_t absoluteLumpNum = F_LumpNumForName(name);
        if(absoluteLumpNum >= 0)
        {
            F_DumpLump(absoluteLumpNum);
        }
        else
        {
            Con_Message("Warning: Cannot dump unknown lump \"%s\", ignoring.\n", name);
        }
    }

    if(CommandLine_Check("-dumpwaddir"))
    {
        Con_Executef(CMDS_CMDLINE, false, "listlumps");
    }

    // Try to load the autoexec file. This is done here to make sure everything is
    // initialized: the user can do here anything that s/he'd be able to do in-game
    // provided a game was loaded during startup.
    Con_ParseCommands("autoexec.cfg");

    // Read additional config files that should be processed post engine init.
    if(CommandLine_CheckWith("-parse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Sys_GetRealTime();
        for(;;)
        {
            const char* arg = CommandLine_Next();
            if(!arg || arg[0] == '-') break;

            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    }

    // A console command on the command line?
    { int p;
    for(p = 1; p < CommandLine_Count() - 1; p++)
    {
        if(stricmp(CommandLine_At(p), "-command") && stricmp(CommandLine_At(p), "-cmd"))
            continue;

        for(++p; p < CommandLine_Count(); p++)
        {
            const char* arg = CommandLine_At(p);

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
        if(CommandLine_CheckWith("-connect", 1))
            Con_Executef(CMDS_CMDLINE, false, "connect %s", CommandLine_Next());

        // Incoming TCP port.
        if(CommandLine_CheckWith("-port", 1))
            Con_Executef(CMDS_CMDLINE, false, "net-ip-port %s", CommandLine_Next());

        // Server start command.
        // (shortcut for -command "net init tcpip; net server start").
        if(CommandLine_Exists("-server"))
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
        // Lets get most of everything else initialized.
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

        Def_PostInit();

        // Lets play a nice title animation.
        DD_StartTitle();

        // We'll open the console and print a list of the known games too.
        Con_Execute(CMDS_DDAY, "conopen", true, false);
        if(!CommandLine_Exists("-noautoselect"))
            Con_Message("Automatic game selection failed.\n");
        Con_Execute(CMDS_DDAY, "listgames", false, false);
    }

    return true;
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
    DENG_UNUSED(parm);

#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif

    Con_SetProgress(10);

    // Any startup hooks?
    DD_CallHooks(HOOK_STARTUP, 0, 0);

    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(CommandLine_CheckWith("-userdir", 1) && !app.usingUserDir)
        Con_Message("--(!)-- User directory not found (check -userdir).\n");

    bamsInit(); // Binary angle calculations.

    DD_InitResourceSystem();

    Con_SetProgress(40);

    Net_Init();
    // Now we can hide the mouse cursor for good.
    Sys_HideMouse();

    // Read config files that should be read BEFORE engine init.
    if(CommandLine_CheckWith("-cparse", 1))
    {
        uint startTime;
        Con_Message("Parsing additional (pre-init) config files:\n");
        startTime = Sys_GetRealTime();
        for(;;)
        {
            const char* arg = CommandLine_NextAsPath();
            if(!arg || arg[0] == '-')
                break;
            Con_Message("  Processing \"%s\"...\n", F_PrettyPath(arg));
            Con_ParseCommands(arg);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    }

    // Add required engine resource files.
    { ddstring_t foundPath; Str_Init(&foundPath);
    if(0 == F_FindResource2(RC_PACKAGE, "doomsday.pk3", &foundPath) ||
       !F_AddFile(Str_Text(&foundPath)))
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
    Con_ParseCommands("startup.cfg");

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
    {
        Con_Open(true);

        // Also make sure the game loop isn't running needlessly often.
        LegacyCore_SetLoopRate(35);
    }

    Con_SetProgress(199);

    DD_CallHooks(HOOK_INIT, 0, 0); // Any initialization hooks?

    Con_SetProgress(200);

#ifdef WIN32
    // This thread has finished using COM.
    CoUninitialize();
#endif

    BusyMode_WorkerEnd();
    return 0;
}

/**
 * This only exists so we have something to call while the deferred uploads of the
 * startup are processed.
 */
static int DD_DummyWorker(void *parm)
{
    DENG_UNUSED(parm);
    Con_SetProgress(200);
    BusyMode_WorkerEnd();
    return 0;
}

void DD_CheckTimeDemo(void)
{
    static boolean checked = false;

    if(!checked)
    {
        checked = true;
        if(CommandLine_CheckWith("-timedemo", 1) || // Timedemo mode.
           CommandLine_CheckWith("-playdemo", 1)) // Play-once mode.
        {
            char            buf[200];

            sprintf(buf, "playdemo %s", CommandLine_Next());
            Con_Execute(CMDS_CMDLINE, buf, false, false);
        }
    }
}

typedef struct {
    /// @c true iff caller (i.e., DD_UpdateEngineState) initiated busy mode.
    boolean initiatedBusyMode;
} ddupdateenginestateworker_paramaters_t;

static int DD_UpdateEngineStateWorker(void* parameters)
{
    DENG_ASSERT(parameters);
    {
    ddupdateenginestateworker_paramaters_t* p = (ddupdateenginestateworker_paramaters_t*) parameters;

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
        BusyMode_WorkerEnd();
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
    p.initiatedBusyMode = !BusyMode_Active();
    if(p.initiatedBusyMode)
    {
        Con_InitProgress(200);
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    DD_UpdateEngineStateWorker, &p, "Updating engine state...");
    }
    else
    {
        /// @todo Update the current task name and push progress.
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
    case DD_SHIFT_DOWN:
        return I_ShiftDown();

    case DD_WINDOW_WIDTH:
        return Window_Width(theWindow);

    case DD_WINDOW_HEIGHT:
        return Window_Height(theWindow);

    case DD_DYNLIGHT_TEXTURE:
        return (int) GL_PrepareLSTexture(LST_DYNAMIC);

    case DD_NUMLUMPS:
        return F_LumpCount();

    case DD_CURRENT_CLIENT_FINALE_ID:
        return Cl_CurrentFinale();

    case DD_MAP_MUSIC: {
        GameMap* map = theMap;
        if(map)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(map));
            if(mapInfo)
            {
                return Def_GetMusicNum(mapInfo->music);
            }
        }
        return -1;
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
    static uint valueU;
    static float valueF;
    static double valueD;

    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_SECTOR_COUNT:
        valueU = theMap? GameMap_SectorCount(theMap) : 0;
        return &valueU;

    case DD_LINE_COUNT:
        valueU = theMap? GameMap_LineDefCount(theMap) : 0;
        return &valueU;

    case DD_SIDE_COUNT:
        valueU = theMap? GameMap_SideDefCount(theMap) : 0;
        return &valueU;

    case DD_VERTEX_COUNT:
        valueU = theMap? GameMap_VertexCount(theMap) : 0;
        return &valueU;

    case DD_POLYOBJ_COUNT:
        valueU = theMap? GameMap_PolyobjCount(theMap) : 0;
        return &valueU;

    case DD_HEDGE_COUNT:
        valueU = theMap? GameMap_HEdgeCount(theMap) : 0;
        return &valueU;

    case DD_BSPLEAF_COUNT:
        valueU = theMap? GameMap_BspLeafCount(theMap) : 0;
        return &valueU;

    case DD_BSPNODE_COUNT:
        valueU = theMap? GameMap_BspNodeCount(theMap) : 0;
        return &valueU;

    case DD_TRACE_ADDRESS:
        /// @todo Do not cast away const.
        return (void*)P_TraceLOS();

    case DD_TRANSLATIONTABLES_ADDRESS:
        return translationTables;

    case DD_MAP_NAME:
        if(theMap)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
            if(mapInfo && mapInfo->name[0])
            {
                int id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL);
                if(id != -1)
                {
                    return defs.text[id].text;
                }
                return mapInfo->name;
            }
        }
        return NULL;

    case DD_MAP_AUTHOR:
        if(theMap)
        {
            ded_mapinfo_t* mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
            if(mapInfo && mapInfo->author[0])
            {
                return mapInfo->author;
            }
        }
        return NULL;

    case DD_MAP_MIN_X:
        if(theMap)
        {
            return &theMap->aaBox.minX;
        }
        return NULL;

    case DD_MAP_MIN_Y:
        if(theMap)
        {
            return &theMap->aaBox.minY;
        }
        return NULL;

    case DD_MAP_MAX_X:
        if(theMap)
        {
            return &theMap->aaBox.maxX;
        }
        return NULL;

    case DD_MAP_MAX_Y:
        if(theMap)
        {
            return &theMap->aaBox.maxY;
        }
        return NULL;

    case DD_PSPRITE_OFFSET_X:
        return &pspOffset[VX];

    case DD_PSPRITE_OFFSET_Y:
        return &pspOffset[VY];

    case DD_PSPRITE_LIGHTLEVEL_MULTIPLIER:
        return &pspLightLevelMultiplier;

    /*case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;*/

    case DD_GRAVITY:
        valueD = theMap? GameMap_Gravity(theMap) : 0;
        return &valueD;

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
        return Window_NativeHandle(Window_Main());
#endif

    // We have to separately calculate the 35 Hz ticks.
    case DD_GAMETIC: {
        static timespan_t       fracTic;
        fracTic = gameTime * TICSPERSEC;
        return &fracTic; }

    case DD_OPENRANGE: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->range;
        return &valueF; }

    case DD_OPENTOP: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->top;
        return &valueF; }

    case DD_OPENBOTTOM: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->bottom;
        return &valueF; }

    case DD_LOWFLOOR: {
        const TraceOpening* open = P_TraceOpening();
        valueF = open->lowFloor;
        return &valueF; }

    case DD_NUMLUMPS: {
        static int count;
        count = F_LumpCount();
        return &count; }

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
            if(theMap) GameMap_SetGravity(theMap, *(coord_t*) parm);
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

/// @note Part of the Doomsday public API.
materialnamespaceid_t DD_ParseMaterialNamespace(const char* str)
{
    return Materials_ParseNamespace(str);
}

/// @note Part of the Doomsday public API.
texturenamespaceid_t DD_ParseTextureNamespace(const char* str)
{
    return Textures_ParseNamespace(str);
}

/// @note Part of the Doomsday public API.
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
    materialid_t matId;
    Uri* uri;

    if(texId == NOTEXTUREID) return NOMATERIALID;

    uri = Textures_ComposeUri(texId);
    Uri_SetScheme(uri, Str_Text(DD_MaterialNamespaceNameForTextureNamespace(texNamespaceId)));
    matId = Materials_ResolveUri2(uri, true/*quiet please*/);
    Uri_Delete(uri);
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
        { DDVT_DOUBLE, "DDVT_DOUBLE" },
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
    DENG_UNUSED(src);

    boolean didLoadGame = false, didLoadResource = false;
    ddstring_t foundPath, searchPath;
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
    try
    {
        de::Game& game = games->byIdentityKey(Str_Text(&searchPath));
        if(!game.allStartupResourcesFound())
        {
            Con_Message("Failed to locate all required startup resources:\n");
            de::Game::printResources(game, true, RF_STARTUP);
            Con_Message("%s (%s) cannot be loaded.\n", Str_Text(&game.title()), Str_Text(&game.identityKey()));
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
    catch(de::GameCollection::NotFoundError)
    {} // Ignore the error.

    // Try the resource locator.
    Str_Init(&foundPath);
    for(; arg < argc; ++arg)
    {
        Str_Set(&searchPath, argv[arg]);
        Str_Strip(&searchPath);

        if(F_FindResource3(RC_PACKAGE, Str_Text(&searchPath), &foundPath, RLF_MATCH_EXTENSION) != 0 &&
           F_AddFile(Str_Text(&foundPath)))
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
    DENG_UNUSED(src);

    boolean didUnloadFiles = false;
    ddstring_t searchPath;
    int i;

    // No arguments; unload the current game if loaded.
    if(argc == 1)
    {
        if(!DD_GameLoaded())
        {
            Con_Message("There is no game currently loaded.\n");
            return true;
        }
        return DD_ChangeGame(games->nullGame());
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
    if(argc == 2)
    {
        try
        {
            de::Game& game = games->byIdentityKey(Str_Text(&searchPath));
            Str_Free(&searchPath);
            if(DD_GameLoaded())
            {
                return DD_ChangeGame(games->nullGame());
            }

            Con_Message("%s is not currently loaded.\n", Str_Text(&game.identityKey()));
            return true;
        }
        catch(de::GameCollection::NotFoundError)
        {} // Ignore the error.
    }

    // Try the resource locator.
    for(i = 1; i < argc; ++i)
    {
        if(!F_FindResource2(RC_PACKAGE, argv[i], &searchPath) ||
           !F_RemoveFile2(Str_Text(&searchPath), false/*not required game resources*/))
        {
            continue;
        }

        // Success!
        didUnloadFiles = true;
    }

    Str_Free(&searchPath);
    return didUnloadFiles;
}

D_CMD(Reset)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    DD_UpdateEngineState();
    return true;
}

D_CMD(ReloadGame)
{
    DENG_UNUSED(src); DENG_UNUSED(argc); DENG_UNUSED(argv);

    if(!DD_GameLoaded())
    {
        Con_Message("No game is presently loaded.\n");
        return true;
    }
    DD_ChangeGame(games->currentGame(), true/* allow reload */);
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
