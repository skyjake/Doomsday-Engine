/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * dd_uinit.c: Unix Initialization
 *
 * Load libraries and set up APIs.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "dd_uinit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint windowIDX;   // Main window.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

static lt_dlhandle* findFirstUnusedPluginHandle(application_t* app)
{
    assert(app);
    { size_t i;
    for(i = 0; i < MAX_PLUGS; ++i)
    {
        if(!app->hInstPlug[i])
            return &app->hInstPlug[i];
    }}
    return 0;
}

/**
 * Atempts to load the specified plugin.
 *
 * @return              @c true, if the plugin was loaded succesfully.
 */
static int loadPlugin(const char* pluginPath, lt_ptr data)
{
    application_t* app = (application_t*) data;
#ifndef MACOSX
    filename_t  name;
#endif
    lt_dlhandle plugin, *handle;
    void (*initializer)(void);

#ifndef MACOSX
    // What is the actual file name?
    _splitpath(pluginPath, NULL, NULL, name, NULL);
    if(!strncmp(name, "libdp", 5))
#endif
    {
        // Try loading this one as a Doomsday plugin.
        if(NULL == (plugin = lt_dlopenext(pluginPath)))
            return false;

        if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
           NULL == (handle = findFirstUnusedPluginHandle(app)))
        {
            Con_Printf("loadPlugin: Error loading \"%s\" (%s)!\n", pluginPath, lt_dlerror());
            lt_dlclose(plugin);
            return false;
        }

        // This seems to be a Doomsday plugin.
        *handle = plugin;
        initializer();
        return true;
    }
    return false;
}

static boolean unloadPlugin(lt_dlhandle* handle)
{
    assert(handle);
    {
    int result = lt_dlclose(*handle);
    *handle = 0;
    if(result != 0)
        Con_Printf("unloadPlugin: Error unloading plugin (%s)\n", lt_dlerror());
    return result;
    }
}

/**
 * Loads all the plugins from the library directory.
 */
static boolean loadAllPlugins(application_t* app)
{
    assert(app);
    // Try to load all libraries that begin with libdp.
    lt_dlforeachfile(NULL, loadPlugin, (lt_ptr) app);
    return true;
}

static boolean unloadAllPlugins(application_t* app)
{
    assert(app);
    { size_t i;
    for(i = 0; i < MAX_PLUGS && app->hInstPlug[i]; ++i)
    {
        unloadPlugin(&app->hInstPlug[i]);
    }}
    return true;
}

static int initTimingSystem(void)
{
    // For timing, we use SDL under *nix, so get it initialized. SDL_Init() returns zero on success.
    return !SDL_Init(SDL_INIT_TIMER);
}

static int initPluginSystem(void)
{
    // Initialize libtool's dynamic library routines.
    lt_dlinit();
#ifdef DENG_LIBRARY_DIR
    // The default directory is defined in the Makefile. For instance, "/usr/local/lib".
    lt_dladdsearchdir(DENG_LIBRARY_DIR);
#endif
    return true;
}

static int initDGL(void)
{
    return (int) Sys_PreInitGL();
}

static void determineGlobalPaths(application_t* app)
{
    assert(app);

#ifndef MACOSX
    if(getenv("HOME"))
    {
        filename_t homeDir;
        sprintf(homeDir, "%s/.deng", getenv("HOME"));
        M_CheckPath(homeDir);
        Dir_MakeDir(homeDir, &ddRuntimeDir);
        app->userDirOk = Dir_ChDir(&ddRuntimeDir);
    }
#endif

    // The -userdir option sets the working directory.
    if(ArgCheckWith("-userdir", 1))
    {
        Dir_MakeDir(ArgNext(), &ddRuntimeDir);
        app->userDirOk = Dir_ChDir(&ddRuntimeDir);
    }

    // The current working directory is the runtime dir.
    Dir_GetDir(&ddRuntimeDir);

    /**
     * The base path is always the same and depends on the build configuration. 
     * Usually this is something like "/usr/share/deng/".
     */
#ifdef MACOSX
    strcpy(ddBasePath, "./");
#else
    strcpy(ddBasePath, DENG_BASE_DIR);
#endif

    if(ArgCheckWith("-basedir", 1))
    {
        strncpy(ddBasePath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
    }

    Dir_MakeAbsolute(ddBasePath, FILENAME_T_MAXLEN);
    Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
}

static char* buildCommandLineString(int argc, char** argv)
{
    char* cmdLine;
    int length = 0;

    // Assemble a command line string.
    { int i;
    for(i = 0; i < argc; ++i)
        length += strlen(argv[i]) + 1;
    }

    // Allocate a large enough string.
    cmdLine = M_Malloc(length);

    length = 0;
    { int i;
    for(i = 0; i < argc; ++i)
    {
        strcpy(cmdLine + length, argv[i]);
        if(i < argc - 1)
            strcat(cmdLine, " ");
        length += strlen(argv[i]) + 1;
    }}
    return cmdLine;
}

static int createMainWindow(void)
{
    char buf[256];
    DD_ComposeMainWindowTitle(buf);
    windowIDX = Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, isDedicated, buf, 0);
    return windowIDX != 0;
}

int main(int argc, char** argv)
{
    boolean doShutdown = true;
    int exitCode = 0;

    memset(&app, 0, sizeof(app));
    app.userDirOk = true;

    /*if(!initApplication(&app))
    {
        DD_ErrorBox(true, "Failed to initialize application.\n");
    }
    else*/
    {
        // Prepare the command line arguments.
        { char* cmdLine = buildCommandLineString(argc, argv);
        DD_InitCommandLine(cmdLine);
        M_Free(cmdLine);
        }

        // First order of business: are we running in dedicated mode?
        if(ArgCheck("-dedicated"))
            isDedicated = true;

        // Determine our basedir and other global paths.
        determineGlobalPaths(&app);

        if(!DD_EarlyInit())
        {
            DD_ErrorBox(true, "Error during early init.");
        }
        else if(!initTimingSystem())
        {
            DD_ErrorBox(true, "Error initalizing timing system.");
        }
        else if(!Z_Init())
        {
            DD_ErrorBox(true, "Error initializing memory zone.");
        }
        else if(!initPluginSystem())
        {
            DD_ErrorBox(true, "Error initializing plugin system.");
        }
        else if(!initDGL())
        {
            DD_ErrorBox(true, "Error initializing DGL.");
        }
        else if(!loadAllPlugins(&app))
        {
            DD_ErrorBox(true, "Error loading plugins.");
        }
        else if(!createMainWindow())
        {
            DD_ErrorBox(true, "Error creating main window.");
        }
        else if(!Sys_InitGL())
        {
            DD_ErrorBox(true, "Error initializing OpenGL.");
        }
        else
        {   // All initialization complete.
            doShutdown = false;

            { char buf[256];
            DD_ComposeMainWindowTitle(buf);
            Sys_SetWindowTitle(windowIDX, buf);
            }

           // \todo Set foreground window and focus.
        }
    }

    if(!doShutdown)
    {   // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }
    DD_Shutdown();

    // Bye!
    return exitCode;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    // Shutdown all subsystems.
    DD_ShutdownAll();

    SDL_Quit();
    unloadAllPlugins(&app);
	lt_dlexit();
}
