/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

uint            windowIDX;   // Main window.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

static void determineGlobalPaths(application_t *app)
{
    if(!app)
        return;

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
     * The base path is always the same and depends on the build
     * configuration.  Usually this is something like
     * "/usr/share/deng/".
     */
#ifdef MACOSX
    strcpy(ddBasePath, "./");
#else
    strcpy(ddBasePath, DENG_BASE_DIR);
#endif

    if(ArgCheckWith("-basedir", 1))
    {
        strcpy(ddBasePath, ArgNext());
        Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
    }

    Dir_MakeAbsolute(ddBasePath, FILENAME_T_MAXLEN);
    Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);

    printf("determineGlobalPaths: Base path = %s\n", ddBasePath);
}

static boolean loadGamePlugin(application_t *app, const char *libPath)
{
    if(!libPath || !app)
        return false;

    // Load the library and get the API/exports.
    app->hInstGame = lt_dlopenext(libPath);
    if(!app->hInstGame)
    {
        DD_ErrorBox(true, "loadGamePlugin: Loading of %s failed (%s).\n",
                    libPath, lt_dlerror());
        return false;
    }

    app->GetGameAPI = (GETGAMEAPI) lt_dlsym(app->hInstGame, "GetGameAPI");
    if(!app->GetGameAPI)
    {
        DD_ErrorBox(true, "loadGamePlugin: Failed to get address of "
                          "GetGameAPI (%s).\n", lt_dlerror());
        return false;
    }

    // Do the API transfer.
    DD_InitAPI();

    // Everything seems to be working...
    return true;
}

static lt_dlhandle *NextPluginHandle(void)
{
    int                 i;

    for(i = 0; i < MAX_PLUGS; ++i)
    {
        if(!app.hInstPlug[i])
            return &app.hInstPlug[i];
    }

    return NULL;
}

/*
#if 0
int LoadPlugin(const char *pluginPath, lt_ptr data)
{
    filename_t name;
    lt_dlhandle plugin, *handle;
    void (*initializer)(void);

    // What is the actual file name?
    _splitpath(pluginPath, NULL, NULL, name, NULL);

    printf("LP: %s => %s\n", pluginPath, name);

    if(!strncmp(name, "libdp", 5))
    {
#if 0
        filename_t fullPath;

#ifdef DENG_LIBRARY_DIR
        sprintf(fullPath, DENG_LIBRARY_DIR "/%s", name);
#else
        strcpy(fullPath, pluginPath);
#endif
        //if(strchr(name, '.'))
            strcpy(name, pluginPath);
        else
        {
            strcpy(name, pluginPath);
            strcat(name, ".dylib");
            }
#endif
        // Try loading this one as a Doomsday plugin.
        if(NULL == (plugin = lt_dlopenext(pluginPath)))
        {
            printf("LoadPlugin: Failed to open %s!\n", pluginPath);
            return 0;
        }

        if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
           NULL == (handle = NextPluginHandle()))
        {
            printf("LoadPlugin: Failed to load %s!\n", pluginPath);
            lt_dlclose(plugin);
            return 0;
        }

        // This seems to be a Doomsday plugin.
        *handle = plugin;

        printf("LoadPlugin: %s\n", pluginPath);
        initializer();
    }

    return 0;
}
#endif
*/

int LoadPlugin(const char *pluginPath, lt_ptr data)
{
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
            return 0;

        if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
           NULL == (handle = NextPluginHandle()))
        {
            printf("LoadPlugin: Failed to load %s!\n", pluginPath);
            lt_dlclose(plugin);
            return 0;
        }

        // This seems to be a Doomsday plugin.
        *handle = plugin;

        printf("LoadPlugin: %s\n", pluginPath);
        initializer();
    }

    return 0;
}

/**
 * Loads all the plugins from the library directory.
 */
static boolean loadAllPlugins(void)
{
    // Try to load all libraries that begin with libdp.
    lt_dlforeachfile(NULL, LoadPlugin, NULL);
    return true;
}

static int initTimingSystem(void)
{
    // For timing, we use SDL under *nix, so get it initialized.
    // SDL_Init() returns zero on success.
    return !SDL_Init(SDL_INIT_TIMER);
}

static int initPluginSystem(void)
{
    // Initialize libtool's dynamic library routines.
    lt_dlinit();

#ifdef DENG_LIBRARY_DIR
    // The default directory is defined in the Makefile.  For
    // instance, "/usr/local/lib".
    lt_dladdsearchdir(DENG_LIBRARY_DIR);
#endif

    return true;
}

static int initDGL(void)
{
    return Sys_PreInitGL();
}

int main(int argc, char** argv)
{
    char*               cmdLine;
    int                 i, length;
    int                 exitCode = 0;
    boolean             doShutdown = true;
    char                buf[256];
    const char*         libName = NULL;

    app.userDirOk = true;

    // Assemble a command line string.
    for(i = 0, length = 0; i < argc; ++i)
        length += strlen(argv[i]) + 1;

    // Allocate a large enough string.
    cmdLine = M_Malloc(length);

    for(i = 0, length = 0; i < argc; ++i)
    {
        strcpy(cmdLine + length, argv[i]);
        if(i < argc - 1)
            strcat(cmdLine, " ");
        length += strlen(argv[i]) + 1;
    }

    // Prepare the command line arguments.
    DD_InitCommandLine(cmdLine);
    M_Free(cmdLine);
    cmdLine = NULL;

    // First order of business: are we running in dedicated mode?
    if(ArgCheck("-dedicated"))
        isDedicated = true;

    DD_ComposeMainWindowTitle(buf);

    // First we need to locate the game lib name among the command line
    // arguments.
    DD_CheckArg("-game", &libName);

    // Was a game library specified?
    if(!libName)
    {
        DD_ErrorBox(true, "loadGamePlugin: No game library was specified.\n");
    }
    else
    {
        char                libPath[PATH_MAX];

        if(!initPluginSystem())
        {
            DD_ErrorBox(true, "Error initializing plugin system.");
        }

        // Determine our basedir, and other global paths.
        determineGlobalPaths(&app);

        // Compose the full path to the game library.
        // Under *nix this is handled a bit differently.
#ifdef MACOSX
        strcpy(libPath, libName);
#else
        sprintf(libPath, "lib%s.so", libName);
#endif

        if(!DD_EarlyInit())
        {
            DD_ErrorBox(true, "Error during early init.");
        }
        else if(!initTimingSystem())
        {
            DD_ErrorBox(true, "Error initalizing timing system.");
        }
        // Load the rendering DLL.
        else if(!initDGL())
        {
            DD_ErrorBox(true, "Error initializing DGL.");
        }
        // Load the game plugin.
        else if(!loadGamePlugin(&app, libPath))
        {
            DD_ErrorBox(true, "Error loading game library.");
        }
        // Load all other plugins that are found.
        else if(!loadAllPlugins())
        {
            DD_ErrorBox(true, "Error loading plugins.");
        }
        // Init memory zone.
        else if(!Z_Init())
        {
            DD_ErrorBox(true, "Error initializing memory zone.");
        }
        else
        {
            if(0 == (windowIDX =
                Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, isDedicated,
                                 buf, NULL)))
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

                // Append the main window title with the game name and ensure it
                // is the at the foreground, with focus.
                DD_ComposeMainWindowTitle(buf);
                Sys_SetWindowTitle(windowIDX, buf);

               // \todo Set foreground window and focus.
            }
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
    int                 i;

    // Shutdown all subsystems.
    DD_ShutdownAll();

    SDL_Quit();

    // Close the dynamic libraries.
    lt_dlclose(app.hInstGame);
    app.hInstGame = NULL;
    for(i = 0; app.hInstPlug[i]; ++i)
        lt_dlclose(app.hInstPlug[i]);
    memset(app.hInstPlug, 0, sizeof(app.hInstPlug));

    lt_dlexit();
}
