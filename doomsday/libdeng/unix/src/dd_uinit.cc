/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/App>
#include <de/Library>

using namespace de;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL.h>

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "dd_uinit.h"

#include <de/core.h>

using namespace de;

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint windowIDX;   // Main window.
application_t app;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void determineGlobalPaths(application_t *app)
{
    if(!app)
        return;

    LOG_AS("determineGlobalPaths");

    /**
     * The base path is always the same and depends on the build
     * configuration.  Usually this is something like
     * "/usr/share/deng/".
     */
    strcpy(ddBasePath, DENG_BASE_DIR);

    if(ArgCheckWith("-basedir", 1))
    {
        strcpy(ddBasePath, ArgNext());
        Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
    }

    Dir_MakeAbsolute(ddBasePath, FILENAME_T_MAXLEN);
    Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);

    LOG_VERBOSE("Base path: %s") << ddBasePath;

    // The -userdir option sets the working directory.
    if(ArgCheckWith("-userdir", 1))
    {
        Dir_MakeDir(ArgNext(), &ddRuntimeDir);
        app->userDirOk = Dir_ChDir(&ddRuntimeDir);
    }
	else
	{
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
	}

    // The current working directory is the runtime dir.
    Dir_GetDir(&ddRuntimeDir);

    LOG_VERBOSE("Runtime directory: %s") << ddRuntimeDir.path;
}

static boolean loadGamePlugin(application_t *app)
{
    app->GetGameAPI = reinterpret_cast<GETGAMEAPI>(App::game().address("GetGameAPI"));

    // Do the API transfer.
    DD_InitAPI();

    // Everything seems to be working...
    return true;
}

static int initTimingSystem(void)
{
    // For timing, we use SDL under *nix, so get it initialized.
    // SDL_Init() returns zero on success.
    if(SDL_InitSubSystem(SDL_INIT_TIMER) == -1)
        return false;
        
    if(isDedicated && SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
        return false;
        
    return true;
}

static int initDGL(void)
{
    return Sys_PreInitGL();
}

int DD_Entry(int argc, char* argv[])
{
    //int                 exitCode = 0;
    char                buf[256];

    DD_InitCommandLineAliases();

    app.userDirOk = true;

    // First order of business: are we runing in dedicated mode?
    if(ArgCheck("-dedicated"))
        isDedicated = true;
    novideo = ArgCheck("-novideo") || isDedicated;

    DD_ComposeMainWindowTitle(buf);

    // Determine our basedir, and other global paths.
    determineGlobalPaths(&app);

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
    else if(!loadGamePlugin(&app))
    {
        DD_ErrorBox(true, "Error loading game library.");
    }
    else
    {
        /*
        if(0 == (windowIDX =
            Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, 
                isDedicated? WT_CONSOLE : WT_NORMAL, buf, NULL)))
        {
            DD_ErrorBox(true, "Error creating main window.");
        }
        else*/
        if(!Sys_InitGL())
        {
            DD_ErrorBox(true, "Error initializing OpenGL.");
        }
        else
        {   // All initialization complete.
            //doShutdown = false;

            // Append the main window title with the game name and ensure it
            // is the at the foreground, with focus.
            DD_ComposeMainWindowTitle(buf);
            Sys_SetWindowTitle(windowIDX, buf);

           // \todo Set foreground window and focus.
        }
    }

/*
    //if(!doShutdown)
    {   // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }
    //DD_Shutdown();

    // Bye!
    return exitCode;
    */
    
    DD_Main();
    return 0;
}

/**
 * Shuts down the engine. Called after main loop finishes.
 */
void DD_Shutdown(void)
{
#if 0
    if(netGame)
    {   // Quit netGame if one is in progress.
        Con_Execute(CMDS_DDAY, isServer ? "net server close" : "net disconnect",
                    true, false);
    }
#endif

    Demo_StopPlayback();
    Con_SaveDefaults();
    Sys_Shutdown();
    B_Shutdown();

    // Shutdown all subsystems.
    DD_ShutdownAll();
}
