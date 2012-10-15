/**\file dd_uinit.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Engine Initialization - Unix.
 *
 * Load libraries and set up APIs.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <locale.h>

#include <de/c_wrapper.h>

#ifdef UNIX
#  include "library.h"
#endif

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "displaymode.h"
#include "fs_util.h"
#include "dd_uinit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef Library* PluginHandle;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

/// @todo Assigning IDs to the libs should be handled in the plugin module.
static PluginHandle* findFirstUnusedPluginHandle(application_t* app)
{
    int i;
    assert(app);
    for(i = 0; i < MAX_PLUGS; ++i)
    {
        if(!app->hInstPlug[i])
            return &app->hInstPlug[i];
    }
    return 0; // failed
}

static int loadPlugin(const char* fileName, const char* pluginPath, void* param)
{
    application_t* app = (application_t*) param;
    Library* plugin;
    PluginHandle* handle;
    void (*initializer)(void);
    filename_t name;
    pluginid_t plugId;

    assert(app && pluginPath && pluginPath[0]);

    if(!strncmp(fileName, "audio_", 6))
    {
        // The audio plugins are loaded later on demand by AudioDriver.
        return false;
    }

    plugin = Library_New(pluginPath);
    if(!plugin)
    {
        Con_Message("  loadPlugin: Error loading \"%s\" (%s).\n", pluginPath, Library_LastError());
        return 0; // Continue iteration.
    }

    initializer = Library_Symbol(plugin, "DP_Initialize");
    if(!initializer)
    {
        // Clearly not a Doomsday plugin.
#if _DEBUG
        Con_Message("  loadPlugin: \"%s\" does not export entrypoint DP_Initialize, ignoring.\n", pluginPath);
#endif
        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // Assign a handle and ID to the plugin.
    handle = findFirstUnusedPluginHandle(app);
    plugId = handle - app->hInstPlug + 1;
    if(!handle)
    {
#if _DEBUG
        Con_Message("  loadPlugin: Failed acquiring new handle for \"%s\", ignoring.\n", pluginPath);
#endif
        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // This seems to be a Doomsday plugin.
    _splitpath(pluginPath, NULL, NULL, name, NULL);
    Con_Message("  %s (id:%i)\n", name, plugId);

    *handle = plugin;

    DD_SetActivePluginId(plugId);
    initializer();
    DD_SetActivePluginId(0);

    return 0; // Continue iteration.
}

#if 0
typedef struct {
    //boolean loadingGames;
    application_t* app;
} loadpluginparamaters_t;

static int loadPluginWorker(const char* pluginPath, void* data)
{
    loadpluginparamaters_t* params = (loadpluginparamaters_t*) data;
    filename_t name;
    filename_t ext;

    // What is the actual file name?
#ifndef MACOSX
    _splitpath(pluginPath, NULL, NULL, name, ext);
    if(((params->loadingGames  && !strncmp(name, "libj", 4)) ||
        (!params->loadingGames && !strncmp(name, "libdp", 5)))
            && !stricmp(ext, ".so")) // Only .so files
#endif
#ifdef MACOSX
    _splitpath(pluginPath, NULL, NULL, name, ext);
    if(((params->loadingGames  && !strncmp(name, "j", 1)) ||
       (!params->loadingGames && !strncmp(name, "dp", 2)))
            && (!stricmp(ext, ".dylib") || !stricmp(ext, ".bundle")))
#endif
    {
        loadPlugin(params->app, pluginPath, NULL/*no paramaters*/);
    }
    return 0; // Continue search.
}
#endif

static boolean unloadPlugin(PluginHandle* handle)
{
    int result;
    assert(handle);
    if(!*handle) return true;

    Library_Delete(*handle);
    *handle = 0;
    return result;
}

/**
 * Loads all the plugins from the library directory.
 */
static boolean loadAllPlugins(application_t* app)
{
    assert(app);

    Con_Message("Initializing plugins...\n");

    Library_IterateAvailableLibraries(loadPlugin, app);

    /*
    // Try to load all libraries that begin with libj.
    { loadpluginparamaters_t params;
    params.app = app;
    params.loadingGames = true;
    Library_IterateAvailableLibraries(loadPluginWorker, &params);
    }

    // Try to load all libraries that begin with libdp.
    { loadpluginparamaters_t params;
    params.app = app;
    params.loadingGames = false;
    Library_IterateAvailableLibraries(loadPluginWorker, &params);
    }
    */

    return true;
}

static boolean unloadAllPlugins(application_t* app)
{
    int i;
    assert(app);

    // Remove all entries; some may have been created by the plugins.
    LogBuffer_Clear();

    for(i = 0; i < MAX_PLUGS && app->hInstPlug[i]; ++i)
    {
        unloadPlugin(&app->hInstPlug[i]);
    }
    return true;
}

static int initDGL(void)
{
    return (int) Sys_GLPreInit();
}

static void determineGlobalPaths(application_t* app)
{
    assert(app);

#ifndef MACOSX
    if(getenv("HOME"))
    {
        filename_t homePath;
        directory_t* temp;
        dd_snprintf(homePath, FILENAME_T_MAXLEN, "%s/.doomsday/runtime/", getenv("HOME"));
        temp = Dir_New(homePath);
        Dir_mkpath(Dir_Path(temp));
        app->usingHomeDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingHomeDir)
        {
            strncpy(ddRuntimePath, Dir_Path(temp), FILENAME_T_MAXLEN);
        }
        Dir_Delete(temp);
    }
#endif

    // The -userdir option sets the working directory.
    if(CommandLine_CheckWith("-userdir", 1))
    {
        filename_t runtimePath;
        directory_t* temp;

        strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
        Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
        // Ensure the path is closed with a directory separator.
        F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

        temp = Dir_New(runtimePath);
        app->usingUserDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingUserDir)
        {
            strncpy(ddRuntimePath, Dir_Path(temp), FILENAME_T_MAXLEN);
#ifndef MACOSX
            app->usingHomeDir = false;
#endif
        }
        Dir_Delete(temp);
    }

#ifndef MACOSX
    if(!app->usingHomeDir && !app->usingUserDir)
#else
    if(!app->usingUserDir)
#endif
    {
        // The current working directory is the runtime dir.
        directory_t* temp = Dir_NewFromCWD();
        strncpy(ddRuntimePath, Dir_Path(temp), FILENAME_T_MAXLEN);
        Dir_Delete(temp);
    }

    /**
     * Determine the base path. Unless overridden on the command line this is
     * determined according to the the build configuration.
     * Usually this is something like "/usr/share/deng/".
     */
    if(CommandLine_CheckWith("-basedir", 1))
    {
        strncpy(ddBasePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
    }
    else
    {
#ifdef MACOSX
        strncpy(ddBasePath, "./", FILENAME_T_MAXLEN);
#else
        strncpy(ddBasePath, DENG_BASE_DIR, FILENAME_T_MAXLEN);
#endif
        // Also check the system config files.
        UnixInfo_GetConfigValue("paths", "basedir", ddBasePath, FILENAME_T_MAXLEN);
    }
    Dir_CleanPath(ddBasePath, FILENAME_T_MAXLEN);
    Dir_MakeAbsolutePath(ddBasePath, FILENAME_T_MAXLEN);
    // Ensure it ends with a directory separator.
    F_AppendMissingSlashCString(ddBasePath, FILENAME_T_MAXLEN);
}

#if 0
static char* buildCommandLineString(int argc, char** argv)
{
    char* cmdLine;
    int i, length = 0;

    // Assemble a command line string.
    for(i = 0; i < argc; ++i)
    {
        length += strlen(argv[i]) + 1;
    }

    // Allocate a large enough string.
    cmdLine = M_Malloc(length);

    length = 0;
    for(i = 0; i < argc; ++i)
    {
        strcpy(cmdLine + length, argv[i]);
        if(i < argc - 1)
            strcat(cmdLine, " ");
        length += strlen(argv[i]) + 1;
    }
    return cmdLine;
}
#endif

/*
static int createMainWindow(void)
{
    Point2Raw origin = { 0, 0 };
    Size2Raw size = { 640, 480 };
    char buf[256];
    DD_ComposeMainWindowTitle(buf);
    mainWindowIdx = Window_Create(&app, &origin, &size, 32, 0,
                                  isDedicated? WT_CONSOLE : WT_NORMAL, buf, 0);
    return mainWindowIdx != 0;
}
*/

boolean DD_Unix_Init(void)
{
    boolean failed = true;

    memset(&app, 0, sizeof(app));

    // We wish to use U.S. English formatting for time and numbers.
    setlocale(LC_ALL, "en_US.UTF-8");

    DD_InitCommandLine();

    // First order of business: are we running in dedicated mode?
    isDedicated = CommandLine_Check("-dedicated");
    novideo = CommandLine_Check("-novideo") || isDedicated;

    Library_Init();

    // Determine our basedir and other global paths.
    determineGlobalPaths(&app);

    if(!DD_EarlyInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
    }
    else if(!initDGL())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing DGL.", 0);
    }
    else if(!loadAllPlugins(&app))
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error loading plugins.", 0);
    }
    else
    {
        // Everything okay so far.
        failed = false;
    }

    return !failed;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    // Shutdown all subsystems.
    DD_ShutdownAll();
    
    unloadAllPlugins(&app);
    Library_Shutdown();
    DisplayMode_Shutdown();
}
