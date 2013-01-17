/**\file dd_uinit.cpp
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "de_network.h"
#include "de_misc.h"

#include "ui/displaymode.h"
#include "filesys/fs_util.h"
#include "dd_uinit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

#ifdef __CLIENT__
static int initDGL(void)
{
    return (int) Sys_GLPreInit();
}
#endif

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

boolean DD_Unix_Init(void)
{
    boolean failed = true;

    memset(&app, 0, sizeof(app));

    // We wish to use U.S. English formatting for time and numbers.
    setlocale(LC_ALL, "en_US.UTF-8");

    DD_InitCommandLine();

    Library_Init();

    // Determine our basedir and other global paths.
    determineGlobalPaths(&app);

    if(!DD_EarlyInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
    }
#ifdef __CLIENT__
    else if(!initDGL())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing DGL.", 0);
    }
#endif
    else
    {
        // Everything okay so far.
        failed = false;
    }

    Plug_LoadAll();

    return !failed;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    // Shutdown all subsystems.
    DD_ShutdownAll();

    Plug_UnloadAll();
    Library_Shutdown();
    DisplayMode_Shutdown();
}
