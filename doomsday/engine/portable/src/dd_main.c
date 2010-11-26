/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * dd_main.c: Engine Core
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
#include "de_network.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_ui.h"

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

static int DD_StartupWorker(void* parm);
static int DD_DummyWorker(void* parm);
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
directory_t ddRuntimeDir, ddBinDir;

/// The default main configuration file (will invariably be overridden by the game).
ddstring_t configFileName = { "doomsday.cfg" };
ddstring_t bindingsConfigFileName;

int isDedicated = false;

int verbose = 0; // For debug messages (-verbose).
FILE* outFile; // Output file for console messages.

/// List of file names, whitespace seperating (written to .cfg).
char* gameStartupFiles = "";

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/// List of game data files (specified via the command line or in a cfg, or
/// found using the default search algorithm (e.g., /auto and DOOMWADDIR)).
static ddstring_t** gameResourceFileList = 0;
static size_t numGameResourceFileList = 0;

/// GameInfo records and associated found-file lists.
static gameinfo_t** gameInfo = 0;
static uint numGameInfo = 0, currentGameInfoIndex = 0;

// CODE --------------------------------------------------------------------

static __inline size_t countElements(const ddstring_t* const* list)
{
    size_t n = 0;
    if(list)
        while(list[n++]);
    return n;
}

static __inline uint gameInfoIndex(const gameinfo_t* info)
{
    assert(info);
    { uint i;
    for(i = 0; i < numGameInfo; ++i)
    {
        if(info == gameInfo[i])
            return i+1;
    }}
    return 0;
}

static gameinfo_t* findGameInfoForId(gameid_t gameId)
{
    if(gameId != 0 && gameId <= numGameInfo)
        return gameInfo[gameId-1];
    return 0; // Not found.
}

static gameinfo_t* findGameInfoForIdentityKey(const char* identityKey)
{
    assert(identityKey && identityKey[0]);
    { uint i;
    for(i = 0; i < numGameInfo; ++i)
    {
        gameinfo_t* info = gameInfo[i];
        if(!stricmp(Str_Text(GameInfo_IdentityKey(info)), identityKey))
            return info;
    }}
    return 0; // Not found.
}

static gameinfo_t* findGameInfoForCmdlineFlag(const char* cmdlineFlag)
{
    assert(cmdlineFlag && cmdlineFlag[0]);
    { uint i;
    for(i = 0; i < numGameInfo; ++i)
    {
        gameinfo_t* info = gameInfo[i];
        if((GameInfo_CmdlineFlag (info) && !stricmp(Str_Text(GameInfo_CmdlineFlag (info)), cmdlineFlag)) ||
           (GameInfo_CmdlineFlag2(info) && !stricmp(Str_Text(GameInfo_CmdlineFlag2(info)), cmdlineFlag)))
            return info;
    }}
    return 0; // Not found.
}

static void addToPathList(ddstring_t*** list, size_t* listSize, const char* rawPath)
{
    assert(list && listSize && rawPath && rawPath[0]);
    {
    ddstring_t* newPath = Str_New();

    { filename_t temp;
    M_TranslatePath(temp, rawPath, FILENAME_T_MAXLEN);
    Str_Set(newPath, temp); }

    *list = M_Realloc(*list, sizeof(**list) * ++(*listSize)); /// \fixme This is never freed!
    (*list)[(*listSize)-1] = newPath;
    }
}

static void parseStartupFilePathsAndAddFiles(const char* pathString)
{
#define ATWSEPS                 ",; \t"

    assert(pathString && pathString[0]);
    {
    size_t len = strlen(pathString);
    char* buffer = M_Malloc(len + 1), *token;

    strcpy(buffer, pathString);
    token = strtok(buffer, ATWSEPS);
    while(token)
    {
        W_AddFile(token, false);
        token = strtok(NULL, ATWSEPS);
    }
    M_Free(buffer);
    }

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
        M_Free(*list); *list = 0;
    }
    *listSize = 0;
}

static gameinfo_t* addGameInfoRecord(pluginid_t pluginId, const char* identityKey, const char* dataPath,
    const char* defsPath, const ddstring_t* mainDef, const char* title, const char* author, const ddstring_t* cmdlineFlag,
    const ddstring_t* cmdlineFlag2)
{
    gameInfo = M_Realloc(gameInfo, sizeof(*gameInfo) * (numGameInfo + 1));
    gameInfo[numGameInfo] = P_CreateGameInfo(pluginId, identityKey, dataPath, defsPath, mainDef, title, author, cmdlineFlag, cmdlineFlag2);
    return gameInfo[numGameInfo++];
}

resourcenamespace_t* DD_ResourceNamespace(resourcenamespaceid_t rni)
{
    return F_ToResourceNamespace(rni);
}

void DD_ShutdownResourceSearchPaths(void)
{
    GameInfo_ClearResourceSearchPaths(DD_GameInfo());
}

void DD_ClearResourceSearchPathList(resourcenamespaceid_t rni)
{
    GameInfo_ClearResourceSearchPaths2(DD_GameInfo(), rni);
}

const ddstring_t* DD_ResourceSearchPaths(resourcenamespaceid_t rni)
{
    return GameInfo_ResourceSearchPaths(DD_GameInfo(), rni);
}

gameinfo_t* DD_GameInfo(void)
{
    assert(currentGameInfoIndex != 0);
    return gameInfo[currentGameInfoIndex-1];
}

boolean DD_IsNullGameInfo(gameinfo_t* info)
{
    assert(info);
    return GameInfo_PluginId(info) == 0;
}

static void populateExtendedInfo(gameinfo_t* info, ddgameinfo_t* ex)
{
    ex->identityKey = Str_Text(GameInfo_IdentityKey(info));
    ex->title = Str_Text(GameInfo_Title(info));
    ex->author = Str_Text(GameInfo_Author(info));
}

void DD_GetGameInfo2(gameid_t gameId, ddgameinfo_t* ex)
{
    if(!ex)
        Con_Error("DD_GetGameInfo2: Invalid info argument.");
    { gameinfo_t* info;
    if((info = findGameInfoForId(gameId)))
        populateExtendedInfo(info, ex);
    }
    Con_Error("DD_GetGameInfo2: Unknown gameid %i.", (int)gameId);
}

boolean DD_GetGameInfo(ddgameinfo_t* ex)
{
    if(!ex)
        Con_Error("DD_GetGameInfo: Invalid info argument.");

    if(!DD_IsNullGameInfo(DD_GameInfo()))
    {
        populateExtendedInfo(DD_GameInfo(), ex);
        return true;
    }

#if _DEBUG
Con_Message("DD_GetGameInfo: Warning, no game currently loaded - returning false.\n");
#endif
    return false;
}

static void addIdentityKeyToResourceNamespaceRecord(gameresource_record_t* rec, const ddstring_t* identityKey)
{
    assert(rec && identityKey);
    {
    size_t num = countElements(rec->identityKeys);
    rec->identityKeys = M_Realloc(rec->identityKeys, sizeof(*rec->identityKeys) * MAX_OF(num+1, 2));
    if(num) num -= 1;
    rec->identityKeys[num] = Str_New();
    Str_Copy(rec->identityKeys[num], identityKey);
    rec->identityKeys[num+1] = 0; // Terminate.
    }
}

void DD_AddGameResource(gameid_t gameId, resourcetype_t type, const char* _names, void* params)
{
    gameinfo_t* info = findGameInfoForId(gameId);
    resourcenamespaceid_t rni;
    ddstring_t names, name;

    if(!info || DD_IsNullGameInfo(info))
        Con_Error("DD_AddGameResource: Error, unknown game id %u.", gameId);
    if(!VALID_RESOURCE_TYPE(type))
        Con_Error("DD_AddGameResource: Error, unknown resource type %i.", (int)type);
    if(!_names || !_names[0] || !strcmp(_names, ";"))
        Con_Error("DD_AddGameResource: Error, invalid name argument.");

    Str_Init(&names);
    Str_Set(&names, _names);
    // Ensure the name list has the required terminating semicolon.
    if(Str_RAt(&names, 0) != ';')
        Str_Append(&names, ";");

    if((rni = F_ParseResourceNamespace(Str_Text(&names))) == 0)
        rni = F_DefaultResourceNamespaceForType(type);

    Str_Init(&name);
    { gameresource_record_t* rec;
    if((rec = GameInfo_AddResource(info, type, rni, &names)))
    {
        if(params)
        switch(rec->type)
        {
        case RT_PACKAGE:
            // Add an auto-identification file name list to the info record.
            { ddstring_t fileNames, fileName;

            // Ensure the name list has the required terminating semicolon.
            Str_Init(&fileNames); Str_Set(&fileNames, (const char*) params);
            if(Str_RAt(&fileNames, 0) != ';')
                Str_Append(&fileNames, ";");

            Str_Init(&fileName);
            { const char* p = Str_Text(&fileNames);
            while((p = Str_CopyDelim(&fileName, p, ';')))
                  addIdentityKeyToResourceNamespaceRecord(rec, &fileName);
            }

            Str_Free(&fileName);
            Str_Free(&fileNames);
            break;
            }
        default:
            break;
        }
    }}

    Str_Free(&name);
    Str_Free(&names);
}

gameid_t DD_AddGame(const char* identityKey, const char* _dataPath, const char* _defsPath, const char* _mainDef,
    const char* defaultTitle, const char* defaultAuthor, const char* _cmdlineFlag, const char* _cmdlineFlag2)
{
    assert(identityKey && identityKey[0] && _dataPath && _dataPath[0] && _defsPath && _defsPath[0] && defaultTitle && defaultTitle[0] && defaultAuthor && defaultAuthor[0]);
    {
    gameinfo_t* info;
    ddstring_t mainDef, cmdlineFlag, cmdlineFlag2;
    filename_t dataPath, defsPath;
    pluginid_t pluginId = Plug_PluginIdForActiveHook();

    // Game mode identity keys must be unique. Ensure that is the case.
    if((info = findGameInfoForIdentityKey(identityKey)))
        Con_Error("DD_AddGame: Failed adding game \"%s\", identity key '%s' already in use.", defaultTitle, identityKey);

    M_TranslatePath(dataPath, _dataPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(dataPath, FILENAME_T_MAXLEN);

    M_TranslatePath(defsPath, _defsPath, FILENAME_T_MAXLEN);
    Dir_ValidDir(defsPath, FILENAME_T_MAXLEN);

    Str_Init(&mainDef);
    Str_Init(&cmdlineFlag);
    Str_Init(&cmdlineFlag2);

    if(_mainDef)
        Str_Set(&mainDef, _mainDef);

    // Command-line game selection override arguments must be unique. Ensure that is the case.
    if(_cmdlineFlag)
    {
        Str_Appendf(&cmdlineFlag, "-%s", _cmdlineFlag);
        if((info = findGameInfoForCmdlineFlag(Str_Text(&cmdlineFlag))))
            Con_Error("DD_AddGame: Failed adding game \"%s\", cmdlineFlag '%s' already in use.", defaultTitle, Str_Text(&cmdlineFlag));
    }
    if(_cmdlineFlag2)
    {
        Str_Appendf(&cmdlineFlag2, "-%s", _cmdlineFlag2);
        if((info = findGameInfoForCmdlineFlag(Str_Text(&cmdlineFlag2))))
            Con_Error("DD_AddGame: Failed adding game \"%s\", cmdlineFlag '%s' already in use.", defaultTitle, Str_Text(&cmdlineFlag2));
    }

    /**
     * Looking good. Add this game to our records.
     */
    info = addGameInfoRecord(pluginId, identityKey, dataPath, defsPath, &mainDef, defaultTitle, defaultAuthor, _cmdlineFlag? &cmdlineFlag : 0, _cmdlineFlag2? &cmdlineFlag2 : 0);

    Str_Free(&mainDef);
    Str_Free(&cmdlineFlag);
    Str_Free(&cmdlineFlag2);

    return (gameid_t)gameInfoIndex(info);
    }
}

void DD_DestroyGameInfo(void)
{
    Str_Free(&configFileName);
    Str_Free(&bindingsConfigFileName);

    destroyPathList(&gameResourceFileList, &numGameResourceFileList);

    if(gameInfo)
    {
        { uint i;
        for(i = 0; i < numGameInfo; ++i)
            P_DestroyGameInfo(gameInfo[i]);
        }
        M_Free(gameInfo); gameInfo = 0;
    }
    numGameInfo = 0;
    currentGameInfoIndex = 0;
}

/// @return              @c true, iff the resource appears to be what we think it is.
static boolean recognizeWAD(const char* filePath, void* data)
{
    if(!M_FileExists(filePath))
        return false;

    { FILE* file;
    if((file = fopen(filePath, "rb")))
    {
        char id[4];
        if(fread(id, 4, 1, file) != 1)
        {
            fclose(file);
            return false;
        }
        if(!(!strncmp(id, "IWAD", 4) || !strncmp(id, "PWAD", 4)))
        {
            fclose(file);
            return false;
        }
        /*if(data)
        {
            /// \todo dj: Open this using the auxillary features of the WAD loader
            /// and check all the specified lumps are present.
            const ddstring_t* const* lumpNames = (const ddstring_t* const*) data;
            for(; *lumpNames; lumpNames++)
            {
                if(W_CheckNumForName2(Str_Text(*lumpNames), true) == -1)
                    return false;
            }
        }*/
    }}
    return true;
}

/// @return              @c true, iff the resource appears to be what we think it is.
static boolean recognizeZIP(const char* filePath, void* data)
{
    /// \todo dj: write me.
    return M_FileExists(filePath);
}

static boolean validateGameResource(gameresource_record_t* rec, const char* name)
{
    filename_t foundPath;
    if(!F_FindResource(rec->type, foundPath, name, 0, FILENAME_T_MAXLEN))
        return false;
    switch(rec->type)
    {
    case RT_PACKAGE:
        if(recognizeWAD(foundPath, (void*)rec->identityKeys)) break;
        if(recognizeZIP(foundPath, (void*)rec->identityKeys)) break;
        return false;
    default: break;
    }
    // Passed.
    Str_Set(&rec->path, foundPath);
    return true;
}

static void locateGameResources(gameinfo_t* info)
{
    assert(info);
    {
    uint oldGameInfoIndex = currentGameInfoIndex;
    ddstring_t name;

    if(DD_GameInfo() != info)
    {
        /// \kludge Temporarily switch GameInfo.
        currentGameInfoIndex = gameInfoIndex(info);
        // Re-init the resource locator using the search paths of this GameInfo.
        F_InitResourceLocator();
    }

    Str_Init(&name);
    { uint i, numResourceNamespaces = F_NumResourceNamespaces();
    for(i = 1; i < numResourceNamespaces+1; ++i)
    {
        gameresource_record_t* const* records;
        if((records = GameInfo_Resources(info, (resourcenamespaceid_t)i, 0)))
            do
            {
                const char* p = Str_Text(&(*records)->names);
                while((p = Str_CopyDelim(&name, p, ';')) &&
                      !validateGameResource(*records, Str_Text(&name)));
            } while(*(++records));
    }}
    Str_Free(&name);

    if(currentGameInfoIndex != oldGameInfoIndex)
    {
        /// \kludge Restore the old GameInfo.
        currentGameInfoIndex = oldGameInfoIndex;
        // Re-init the resource locator using the search paths of this GameInfo.
        F_InitResourceLocator();
    }
    }
}

static boolean allGameResourcesFound(gameinfo_t* info)
{
    assert(info);
    { uint i, numResourceNamespaces = F_NumResourceNamespaces();
    for(i = 1; i < numResourceNamespaces+1; ++i)
    {
        gameresource_record_t* const* records;
        if((records = GameInfo_Resources(info, (resourcenamespaceid_t)i, 0)))
            do
            {
                if(Str_Length(&(*records)->path) == 0)
                    return false;
            } while(*(++records));
    }}
    return true;
}

static void loadGameResources(gameinfo_t* info, resourcetype_t type, const char* searchPath)
{
    assert(info && VALID_RESOURCE_TYPE(type) && searchPath && searchPath[0]);
    {
    resourcenamespaceid_t rni = F_ParseResourceNamespace(searchPath);
    gameresource_record_t* const* records = GameInfo_Resources(info, rni, 0);
    if(!records)
        return;
    do
    {
        switch((*records)->type)
        {
        case RT_PACKAGE:
            if(Str_Length(&(*records)->path) != 0)
                W_AddFile(Str_Text(&(*records)->path), false);
            break;
        default:
            Con_Error("loadGameResources: Error, no resource loader found for %s.", F_ResourceTypeStr((*records)->type));
        };
    } while(*(++records));
    }
}

static void printGameInfo(gameinfo_t* info)
{
    assert(info);

    if(DD_IsNullGameInfo(info))
        return;

    Con_Printf("Game: %s - %s\n", Str_Text(GameInfo_Title(info)), Str_Text(GameInfo_Author(info)));
#if _DEBUG
    Con_Printf("  Meta: pluginid:%i identitykey:\"%s\" data:\"%s\" defs:\"%s\"\n", (int)GameInfo_PluginId(info), Str_Text(GameInfo_IdentityKey(info)), M_PrettyPath(Str_Text(GameInfo_DataPath(info))), M_PrettyPath(Str_Text(GameInfo_DefsPath(info))));
#endif

    { uint i, numResourceNamespaces = F_NumResourceNamespaces();
    for(i = 1; i < numResourceNamespaces+1; ++i)
    {
        gameresource_record_t* const* records;
        if((records = GameInfo_Resources(info, (resourcenamespaceid_t)i, 0)))
        {
            int n = 0;
            Con_Printf("  Namespace: \"%s\"\n", Str_Text(&F_ToResourceNamespace((resourcenamespaceid_t)i)->_name));
            do
            {
                Con_Printf("    %i:%s - \"%s\" > %s\n", n++, F_ResourceTypeStr((*records)->type), Str_Text(&(*records)->names), Str_Length(&(*records)->path) == 0? "--(!)missing" : M_PrettyPath(Str_Text(&(*records)->path)));
            } while(*(++records));
        }
    }}
    Con_Printf("  Status: %s\n", DD_GameInfo() == info? "Loaded" : allGameResourcesFound(info)? "Complete/Playable" : "Incomplete/Not playable");
}

D_CMD(ListGames)
{
    uint i;
    for(i = 0; i < numGameInfo; ++i)
        printGameInfo(gameInfo[i]);
    return true;
}

/**
 * (f_forall_func_t)
 */
static int autoDataAdder(const char* fileName, filetype_t type, void* ptr)
{
    autoload_t* data = ptr;

    // Skip directories.
    if(type == FT_DIRECTORY)
        return true;

    if(data->loadFiles)
    {
        if(W_AddFile(fileName, false))
            ++data->count;
    }
    else
    {
        addToPathList(&gameResourceFileList, &numGameResourceFileList, fileName);
    }

    // Continue searching.
    return true;
}

/**
 * Files with the extensions wad, lmp, pk3, zip and deh in the automatical data
 * directory are added to gameResourceFileList.
 *
 * @return              Number of new files that were loaded.
 */
static int addFilesFromAutoData(boolean loadFiles)
{
    autoload_t data;
    const char* extensions[] = {
        "wad", "lmp", "pk3", "zip", "deh",
#ifdef UNIX
        "WAD", "LMP", "PK3", "ZIP", "DEH", // upper case alternatives
#endif
        0
    };

    data.loadFiles = loadFiles;
    data.count = 0;

    { uint i;
    for(i = 0; extensions[i]; ++i)
    {
        filename_t pattern;
        dd_snprintf(pattern, FILENAME_T_MAXLEN, "%sauto\\*.%s", Str_Text(GameInfo_DataPath(DD_GameInfo())), extensions[i]);

        Dir_FixSlashes(pattern, FILENAME_T_MAXLEN);
        F_ForAll(pattern, &data, autoDataAdder);
    }}

    return data.count;
}

/**
 * \todo dj: This is clearly a platform service and therefore does not belong here.
 */
#ifdef WIN32
static void* getEntryPoint(HINSTANCE* handle, const char* fn)
{
    void* adr = (void*)GetProcAddress(*handle, fn);
    if(!adr)
    {
        LPVOID lpMsgBuf;
        DWORD dw = GetLastError(); 
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      0, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, 0);
        if(lpMsgBuf)
        {
            Con_Printf("getEntryPoint: Error locating \"%s\" #%d: %s", fn, dw, (char*)lpMsgBuf);
            LocalFree(lpMsgBuf); lpMsgBuf = 0;
        }
    }
    return adr;
}
#elif UNIX
static void* getEntryPoint(lt_dlhandle* handle, const char* fn)
{
    void* adr = (void*)lt_dlsym(*handle, fn);
    if(!adr)
    {
        Con_Message("getEntryPoint: Error locating address of \"%s\".\n", fn);
    }
    return adr;
}
#endif

static boolean exchangeEntryPoints(pluginid_t pluginId)
{
    if(!app.GetGameAPI)
    {
        // Do the API transfer.
        if(!(app.GetGameAPI = (GETGAMEAPI) getEntryPoint(&app.hInstPlug[pluginId-1], "GetGameAPI")))
            return false;
        DD_InitAPI();
        Def_GetGameClasses();
    }
    return true;
}

static __inline boolean isZip(const char* fn)
{
    size_t len = strlen(fn);
    return (len > 4 && (!strnicmp(fn + len - 4, ".pk3", 4) || !strnicmp(fn + len - 4, ".zip", 4)));
}

static __inline boolean isWad(const char* fn)
{
    size_t len = strlen(fn);
    return (len > 4 && !strnicmp(fn + len - 4, ".wad", 4));
}

static int DD_ChangeGameWorker(void* parm)
{
    assert(parm);
    {
    gameinfo_t* info = (gameinfo_t*)parm;
    uint startTime;

    /**
     * Parse the game's main config file.
     * If a custom top-level config is specified; let it override.
     */
    if(ArgCheckWith("-config", 1))
    {
        Str_Set(&configFileName, ArgNext());
        Dir_FixSlashes(Str_Text(&configFileName), Str_Length(&configFileName));
    }
    Con_Message("Parsing game config: \"%s\" ...\n", M_PrettyPath(Str_Text(&configFileName)));
    Con_ParseCommands(Str_Text(&configFileName), true);

    Con_SetProgress(10);

    F_InitResourceLocator();

    /**
     * Create default Auto mappings in the runtime directory.
     */
    { ddstring_t temp;

    Str_Init(&temp);
    // Data class resources.
    Str_Appendf(&temp, "%sauto", Str_Text(GameInfo_DataPath(info)));
    F_AddMapping("auto", Str_Text(&temp));

    Str_Clear(&temp);
    // Definition class resources.
    Str_Appendf(&temp, "%sauto", Str_Text(GameInfo_DefsPath(info)));
    F_AddMapping("auto", Str_Text(&temp));

    Str_Free(&temp);
    }

    /**
     * Open all the files, load headers, count lumps, etc, etc...
     * \note duplicate processing of the same file is automatically guarded against by
     * the virtual file system layer.
     */
    Con_Message("Loading game resources:\n");
    startTime = Sys_GetRealTime();

    /**
     * Phase 1: Add game-resource files.
     * First ZIPs then WADs (they may contain virtual WAD files).
     */
    loadGameResources(info, RT_PACKAGE, "packages:");
    loadGameResources(info, RT_PACKAGE, "packages:");

    /**
     * Phase 2: Add additional game-startup files.
     * \note These must take precedence over Auto but not game-resource files.
     */
    if(gameStartupFiles && gameStartupFiles[0])
        parseStartupFilePathsAndAddFiles(gameStartupFiles);

    /**
     * Phase 3: Add real files from the Auto directory.
     * First ZIPs then WADs (they may contain virtual WAD files).
     */
    addFilesFromAutoData(false);
    if(numGameResourceFileList > 0)
    {
        size_t i;
        for(i = 0; i < numGameResourceFileList; ++i)
            if(isZip(Str_Text(gameResourceFileList[i])))
                W_AddFile(Str_Text(gameResourceFileList[i]), false);

        for(i = 0; i < numGameResourceFileList; ++i)
            if(isWad(Str_Text(gameResourceFileList[i])))
                W_AddFile(Str_Text(gameResourceFileList[i]), false);
    }

    // Final autoload round.
    DD_AutoLoad();

    F_InitDirec();

    Con_SetProgress(60);
    VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );

    // Read bindings for this game and merge with the working set.
    Con_ParseCommands(Str_Text(&bindingsConfigFileName), false);

    R_InitTextures();
    R_InitFlats();
    R_PreInitSprites();

    Con_SetProgress(120);

    // Now that we've generated the auto-materials we can initialize definitions.
    Def_Read();

    Con_SetProgress(160);

    R_InitSprites(); // Fully initialize sprites.
    R_InitModels();
    Rend_ParticleLoadExtraTextures();
    Cl_InitTranslations();

    Def_PostInit();

    DD_ReadGameHelp();
    Con_InitUI(); // Update the console title display(s).

    if(gx.PostInit)
    {
        Con_SetProgress(180);
        gx.PostInit((gameid_t)gameInfoIndex(info));
    }

    Con_SetProgress(200);
    Con_BusyWorkerEnd();
    return 0;
    }
}

/**
 * Switch to/activate the specified game.
 */
void DD_ChangeGame(gameinfo_t* info)
{
    assert(info);

    Con_Message("DD_ChangeGame: Selecting \"%s\".\n", Str_Text(GameInfo_IdentityKey(info)));
    if(!exchangeEntryPoints(GameInfo_PluginId(info)))
    {
        Con_Message("DD_ChangeGame: Warning, error exchanging entrypoints with plugin %i - aborting.\n", (int)GameInfo_PluginId(info));
        return;
    }

    P_InitMapUpdate();
    DAM_Init();

    // This is now the current game.
    currentGameInfoIndex = gameInfoIndex(info);

    if(gx.PreInit)
        gx.PreInit();

    // The bulk of this we can do in busy mode.
    Con_InitProgress(200);
    Con_Busy(BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Changing game...", DD_ChangeGameWorker, info);

    /**
     * Print a game mode banner with rulers.
     * \todo dj: This has been deferred here so that strings like the game
     * title and author can be overridden (e.g., via DEHACKED). Make it so!
     */
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER, Str_Text(GameInfo_Title(info))); Con_FPrintf(CBLF_WHITE | CBLF_CENTER, "\n");
    Con_FPrintf(CBLF_RULER, "");
}

void R_PrependDataPath(char* newPath, const char* origPath, size_t len)
{
    assert(newPath && origPath && origPath[0] && len > 0);

    if(Dir_IsAbsolute(origPath))
    {   // origPath is already absolute; use as-is.
        strncpy(newPath, origPath, len);
        return;
    }
    dd_snprintf(newPath, len, "%s%s", Str_Text(GameInfo_DataPath(DD_GameInfo())), origPath);
}

void R_PrependDefsPath(char* newPath, const char* origPath, size_t len)
{
    assert(newPath && origPath && origPath[0] && len > 0);

    if(Dir_IsAbsolute(origPath))
    {   // origPath is already absolute; use as-is.
        strncpy(newPath, origPath, len);
        return;
    }
    dd_snprintf(newPath, len, "%s%s", Str_Text(GameInfo_DefsPath(DD_GameInfo())), origPath);
}

void DD_SetConfigFile(const char* file)
{
    if(!file || !file[0])
        Con_Error("DD_SetConfigFile: Invalid file argument.");

    Str_Set(&configFileName, file);
    Dir_FixSlashes(Str_Text(&configFileName), Str_Length(&configFileName));

    Str_Clear(&bindingsConfigFileName);
    Str_PartAppend(&bindingsConfigFileName, Str_Text(&configFileName), 0, Str_Length(&configFileName)-4);
    Str_Append(&bindingsConfigFileName, "-bindings.cfg");
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
    { int numNewFiles;
    while((numNewFiles = addFilesFromAutoData(true)) > 0)
    {
        VERBOSE(Con_Message("Autoload round completed with %i new files.\n", numNewFiles));
    }}
}

static uint countAvailableGames(void)
{
    uint i, numAvailableGames = 0;
    for(i = 0; i < numGameInfo; ++i)
    {
        gameinfo_t* info = gameInfo[i];
        if(DD_IsNullGameInfo(info) || !allGameResourcesFound(info))
            continue;
        numAvailableGames++;
    }
    return numAvailableGames;
}

/**
 * Attempt automatic game selection.
 */
void DD_AutoselectGame(void)
{
    uint numAvailableGames = countAvailableGames();

    if(numAvailableGames == 0)
        return;

    if(numAvailableGames == 1)
    {   // Find this game and select it.
        uint i;
        for(i = 0; i < numGameInfo; ++i)
        {
            gameinfo_t* info = gameInfo[i];
            if(DD_IsNullGameInfo(info))
                continue;
            if(!allGameResourcesFound(info))
                continue;
            DD_ChangeGame(info);
            break;
        }
        return;
    }

    {
    const char* expGame = ArgCheckWith("-game", 1)? ArgNext() : 0;
    uint pass = expGame? 0 : 1;
    do
    {
        uint infoIndex = 0;
        do
        {
            gameinfo_t* info = gameInfo[infoIndex];

            if(DD_IsNullGameInfo(info))
                continue;
            if(!allGameResourcesFound(info))
                continue;

            switch(pass)
            {
            case 0: // Command line modestring match for-development/debug (e.g., "-game doom1-ultimate").
                if(!Str_CompareIgnoreCase(GameInfo_IdentityKey(info), expGame))
                    DD_ChangeGame(info);
                break;

            case 1: // Command line name flag match (e.g., "-doom2").
                if((GameInfo_CmdlineFlag (info) && ArgCheck(Str_Text(GameInfo_CmdlineFlag (info)))) ||
                   (GameInfo_CmdlineFlag2(info) && ArgCheck(Str_Text(GameInfo_CmdlineFlag2(info)))))
                    DD_ChangeGame(info);
                break;
            }
        } while(++infoIndex < numGameInfo && DD_IsNullGameInfo(DD_GameInfo()));
    } while(++pass < 2 && DD_IsNullGameInfo(DD_GameInfo()));
    }
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
    int exitCode = 0;

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
    // Ensure a valid value.
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

    if(!isDedicated)
    {
        if(!GL_EarlyInit())
        {
            Sys_CriticalMessage("GL_EarlyInit() failed.");
            return -1;
        }
    }

    /**
     * \note This must be called from the main thread due to issues with
     * the devices we use via the WINAPI, MCI (cdaudio, mixer etc).
     */
    Sys_Init();

    // Enter busy mode until startup complete.
    Con_InitProgress(200);
    Con_Busy(BUSYF_NO_UPLOADS | BUSYF_STARTUP | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
            "Starting up...", DD_StartupWorker, 0);

    // Engine initialization is complete. Now finish up with the GL.
    if(!isDedicated)
    {
        GL_Init();
        GL_InitRefresh();
        GL_LoadSystemTextures();
    }

    // Do deferred uploads.
    Con_Busy(BUSYF_STARTUP | BUSYF_PROGRESS_BAR | BUSYF_ACTIVITY | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
             "Buffering...", DD_DummyWorker, 0);

    // Unless we reenter busy-mode due to automatic game selection, we won't be
    // drawing anything further until DD_GameLoop; so lets clean up.
    glClear(GL_COLOR_BUFFER_BIT);
    GL_DoUpdate();

    // Add resources specified using -iwad options on the command line.
#pragma message("!!!WARNING: Re-implement support for the -iwad option!!!")
#if 0
    {int p;
    for(p = 0; p < Argc(); ++p)
    {
        if(!ArgRecognize("-iwad", Argv(p)))
            continue;

        while(++p != Argc() && !ArgIsOption(p))
            addToPathList(&gameResourceFileList, &numGameResourceFileList, Argv(p));

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }}
#endif

    // Try to locate all required data files for all registered games.
    { uint i;
    for(i = 0; i < numGameInfo; ++i)
    {
        gameinfo_t* info = gameInfo[i];
        if(DD_IsNullGameInfo(info))
            continue;
        locateGameResources(info);
    }}
    VERBOSE( Con_Execute(CMDS_DDAY, "listgames", false, false) );

    // Attempt automatic game selection.
    if(!ArgExists("-noautoselect"))
    {
        DD_AutoselectGame();
        if(DD_IsNullGameInfo(DD_GameInfo()))
            Con_Message("Automatic game selection failed.\n");
    }

    // Load resources specified using -file options on the command line.
    {int p;
    for(p = 0; p < Argc(); ++p)
    {
        if(!ArgRecognize("-file", Argv(p)))
            continue;

        while(++p != Argc() && !ArgIsOption(p))
            W_AddFile(Argv(p), false);

        p--;/* For ArgIsOption(p) necessary, for p==Argc() harmless */
    }}

    // One-time execution of various command line features available during startup.
    if(ArgCheckWith("-dumplump", 1))
    {
        lumpnum_t lumpNum;
        if((lumpNum = W_CheckNumForName(ArgNext())) != -1)
            W_DumpLump(lumpNum, 0);
    }
    if(ArgCheck("-dumpwaddir"))
        W_DumpLumpDir();

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
            if(!arg || arg[0] == '-')
                break;
            Con_Message("  Processing \"%s\" ...\n", M_PrettyPath(arg));
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
    if(!DD_IsNullGameInfo(DD_GameInfo()))
    {
        // Client connection command.
        if(ArgCheckWith("-connect", 1))
            Con_Executef(CMDS_CMDLINE, false, "connect %s", ArgNext());

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
    {   // For now we'll just open the console automatically.
        Con_Execute(CMDS_DDAY, "conopen", true, false);
    }

    // Start the game loop.
    exitCode = DD_GameLoop();

    // Time to shutdown.

    if(netGame)
    {   // Quit netGame if one is in progress.
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect", true, false);
    }

    Demo_StopPlayback();

    if(!DD_IsNullGameInfo(DD_GameInfo()))
    {   // A game is still loaded; save state, user configs etc, etc...
        Con_SaveDefaults();
    }

    B_Shutdown();
    DD_DestroyGameInfo();
    Sys_Shutdown();

    return exitCode;
}

static int DD_StartupWorker(void* parm)
{
#ifdef WIN32
    // Initialize COM for this thread (needed for DirectInput).
    CoInitialize(NULL);
#endif

    F_InitMapping();

    /**
     * One-time creation and initialization of the special "null-game" object (activated once created).
     */
    { filename_t dataPath, defsPath;
    M_TranslatePath(dataPath, DD_BASEPATH_DATA, FILENAME_T_MAXLEN);
    Dir_ValidDir(dataPath, FILENAME_T_MAXLEN);
    M_TranslatePath(defsPath, DD_BASEPATH_DEFS, FILENAME_T_MAXLEN);
    Dir_ValidDir(defsPath, FILENAME_T_MAXLEN);
    currentGameInfoIndex = gameInfoIndex(addGameInfoRecord(0, 0, dataPath, defsPath, 0, 0, 0, 0, 0));
    }

    // Initialize the key mappings.
    DD_InitInput();

    Con_SetProgress(10);

    // Any startup hooks?
    Plug_DoHook(HOOK_STARTUP, 0, 0);

    Con_SetProgress(20);

    // Was the change to userdir OK?
    if(!app.userDirOk)
        Con_Message("--(!)-- User directory not found (check -userdir).\n");

    bamsInit(); // Binary angle calculations.

    // Initialize the file system databases.
    Zip_Init();
    W_Init();
    F_InitResourceLocator();

    // Initialize the definition databases.
    Def_Init();

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
            Con_Message("  Processing \"%s\" ...\n", M_PrettyPath(arg));
            Con_ParseCommands(arg, false);
        }
        VERBOSE( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) );
    }

    // Add required engine resource files.
    { filename_t foundPath;
    if(F_FindResource(RT_PACKAGE, foundPath, "doomsday.pk3", 0, FILENAME_T_MAXLEN))
        W_AddFile(foundPath, false);
    else
        Con_Error("DD_StartupWorker: Failed to locate required resource \"doomsday.pk3\".");
    }

    // No more WADs will be loaded in startup mode after this point.
    W_EndStartup();

    F_InitDirec();

    // Load engine help resources.
    DD_InitHelp();

    Con_SetProgress(60);

    // Execute the startup script (Startup.cfg).
    Con_ParseCommands("startup.cfg", false);

    // Get the material manager up and running.
    Con_SetProgress(90);
    GL_EarlyInitTextureManager();
    Materials_Initialize();

    Con_SetProgress(140);
    Con_Message("B_Init: Init binding system.\n");
    B_Init();

    Con_SetProgress(150);
    Con_Message("R_Init: Init the refresh daemon.\n");
    R_Init();

    Con_SetProgress(165);
    Con_Message("Net_InitGame: Initializing game data.\n");
    Net_InitGame();
    Demo_Init();

    Con_Message("FI_Init: Initializing InFine.\n");
    FI_Init();

    Con_Message("UI_PageInit: Initializing user interface.\n");
    UI_Init();

    Con_SetProgress(190);

    // In dedicated mode the console must be opened, so all input events
    // will be handled by it.
    if(isDedicated)
        Con_Open(true);

    Con_SetProgress(199);

    Plug_DoHook(HOOK_INIT, 0, 0); // Any initialization hooks?
    Con_UpdateKnownWords(); // For word completion (with Tab).

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

static int DD_UpdateWorker(void* parm)
{
    GL_InitRefresh();
    R_Update();

    Con_BusyWorkerEnd();
    return 0;
}

void DD_UpdateEngineState(void)
{
    boolean hadFog;

    // Update refresh.
    Con_Message("Updating state...\n");

    // Update the dir/WAD translations.
    F_InitDirec();

    gx.UpdateState(DD_PRE);

    // Stop playing sounds and music.
    Demo_StopPlayback();
    S_Reset();

    hadFog = usingFog;
    GL_TotalReset();
    GL_TotalRestore(); // Bring GL back online.

    // Make sure the fog is enabled, if necessary.
    if(hadFog)
        GL_UseFog(true);

    // Now that GL is back online, we can continue the update in busy mode.
    Con_InitProgress(200);
    Con_Busy(BUSYF_ACTIVITY | BUSYF_PROGRESS_BAR | (verbose? BUSYF_CONSOLE_OUTPUT : 0), "Updating engine state...", DD_UpdateWorker, 0);

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
    {&viewwindowx, &viewwindowx},
    {&viewwindowy, &viewwindowy},
    {&viewwidth, &viewwidth},
    {&viewheight, &viewheight},
    {&consolePlayer, &consolePlayer},
    {&displayPlayer, &displayPlayer},
    {&mipmapping, 0},
    {&filterUI, 0},
    {&defResX, &defResX},
    {&defResY, &defResY},
    {&skyDetail, 0},
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
    {&GL_state.maxTexUnits, 0}
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
        return theWindow->width;

    case DD_WINDOW_HEIGHT:
        return theWindow->height;

    case DD_DYNLIGHT_TEXTURE:
        return (int) GL_PrepareLSTexture(LST_DYNAMIC);

    case DD_NUMLUMPS:
        return W_NumLumps();

    case DD_MAP_MUSIC:
        { gamemap_t *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo)
            return Def_GetMusicNum(mapInfo->music);
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
    switch(ddvalue)
    {
    case DD_GAME_EXPORTS:
        return &gx;

    case DD_VIEW_X:
        return &viewX;

    case DD_VIEW_Y:
        return &viewY;

    case DD_VIEW_Z:
        return &viewZ;

    case DD_VIEW_ANGLE:
        return &viewAngle;

    case DD_VIEW_PITCH:
        return &viewPitch;

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

    case DD_MATERIAL_COUNT:
        {
        static uint value;
        value = Materials_Count();
        return &value;
        }
    case DD_TRACE_ADDRESS:
        return &traceLOS;

    case DD_TRANSLATIONTABLES_ADDRESS:
        return translationTables;

    case DD_MAP_NAME:
    {
        gamemap_t *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo && mapInfo->name[0])
        {
            int id;

            if((id = Def_Get(DD_DEF_TEXT, mapInfo->name, NULL)) != -1)
            {
                return defs.text[id].text;
            }

            return mapInfo->name;
        }
        break;
    }
    case DD_MAP_AUTHOR:
    {
        gamemap_t *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo && mapInfo->author[0])
            return mapInfo->author;
        break;
    }
    case DD_MAP_MIN_X:
    {
        gamemap_t  *map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXLEFT];
        else
            return NULL;
    }
    case DD_MAP_MIN_Y:
    {
        gamemap_t  *map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXBOTTOM];
        else
            return NULL;
    }
    case DD_MAP_MAX_X:
    {
        gamemap_t  *map = P_GetCurrentMap();
        if(map)
            return &map->bBox[BOXRIGHT];
        else
            return NULL;
    }
    case DD_MAP_MAX_Y:
    {
        gamemap_t  *map = P_GetCurrentMap();
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

    case DD_SHARED_FIXED_TRIGGER:
        return &sharedFixedTrigger;

    case DD_CPLAYER_THRUST_MUL:
        return &cplrThrustMul;

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
    case DD_GAMETIC:
        { static timespan_t       fracTic;
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

    case DD_NUMLUMPS:
        { static int count;
        count = W_NumLumps();
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
        case DD_VIEW_X:
            viewX = *(float*) parm;
            return;

        case DD_VIEW_Y:
            viewY = *(float*) parm;
            return;

        case DD_VIEW_Z:
            viewZ = *(float*) parm;
            return;

        case DD_VIEW_ANGLE:
            viewAngle = *(angle_t*) parm;
            return;

        case DD_VIEW_PITCH:
            viewPitch = *(float*) parm;
            return;

        case DD_CPLAYER_THRUST_MUL:
            cplrThrustMul = *(float*) parm;
            return;

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

material_namespace_t DD_MaterialNamespaceForTextureType(gltexture_type_t t)
{
    if(t < GLT_ANY || t >= NUM_GLTEXTURE_TYPES)
        Con_Error("DD_MaterialNamespaceForTextureType: Internal error, invalid type %i.", (int) t);
    switch(t)
    {
    case GLT_ANY:           return MN_ANY;
    case GLT_DOOMTEXTURE:   return MN_TEXTURES;
    case GLT_FLAT:          return MN_FLATS;
    case GLT_SPRITE:        return MN_SPRITES;
    case GLT_SYSTEM:        return MN_SYSTEM;
    default:
#if _DEBUG
        Con_Message("DD_MaterialNamespaceForTextureType: No namespace for type %i:%s.", (int)t, GLTEXTURE_TYPE_STRING(t));
#endif
        return 0;
    }
}

materialnum_t DD_MaterialForTexture(uint ofTypeId, gltexture_type_t type)
{
    const gltexture_t* tex;
    if(ofTypeId != 0 && (tex = GL_GetGLTextureByTypeId(ofTypeId-1, type)))
        return Materials_CheckNumForName(GLTexture_Name(tex), DD_MaterialNamespaceForTextureType(type));
    return 0;
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
    } valuetypes[] =
    {
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
    return result >= size? -1 : size;
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
