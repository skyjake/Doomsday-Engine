/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_pinit.c: Portable Engine Initialization
 *
 * Platform independent routines for initializing the engine.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
 * The game imports and exports.
 */
game_import_t __gi;
game_export_t __gx;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int DD_CheckArg(char* tag, const char** value)
{
    int                 i = ArgCheck(tag);
    const char*         next = ArgNext();

    if(!i)
        return 0;
    if(value && next)
        *value = next;
    return 1;
}

void DD_ErrorBox(boolean error, char* format, ...)
{
    char                buff[200];
    va_list             args;

    va_start(args, format);
    vsnprintf(buff, sizeof(buff), format, args);
    va_end(args);

#ifdef WIN32
    suspendMsgPump = true;
    MessageBox(NULL, (LPCTSTR) buff,
               (LPCTSTR) ("Doomsday " DOOMSDAY_VERSION_TEXT),
               (UINT) (MB_OK | (error ? MB_ICONERROR : MB_ICONWARNING)));
    suspendMsgPump = false;
#endif

#ifdef UNIX
    fputs(buff, stderr);
#endif
}

/**
 * Compose the title for the main window.
 */
void DD_ComposeMainWindowTitle(char* title)
{
    if(__gx.GetVariable)
    {
        char*               gameName = (char*) __gx.GetVariable(DD_GAME_ID);
        sprintf(title, "Doomsday " DOOMSDAY_VERSION_TEXT "%s : %s",
                (isDedicated? " (Dedicated)" : ""), gameName);
    }
    else
    {
        sprintf(title, "Doomsday " DOOMSDAY_VERSION_TEXT "%s",
                (isDedicated? " (Dedicated)" : ""));
    }
}

void SetGameImports(game_import_t* imp)
{
    memset(imp, 0, sizeof(*imp));
    imp->apiSize = sizeof(*imp);
    imp->version = DOOMSDAY_VERSION;

    // Data.
    imp->mobjInfo = &mobjInfo;
    imp->states = &states;
    imp->sprNames = &sprNames;
    imp->text = &texts;

    imp->validCount = &validCount;
}

void DD_InitAPI(void)
{
    GETGAMEAPI          GetGameAPI = app.GetGameAPI;

    game_export_t*      gameExPtr;

    // Put the imported stuff into the imports.
    SetGameImports(&__gi);

    memset(&__gx, 0, sizeof(__gx));
    gameExPtr = GetGameAPI(&__gi);
    memcpy(&__gx, gameExPtr,
           MIN_OF(sizeof(__gx), gameExPtr->apiSize));
}

void DD_InitCommandLine(const char* cmdLine)
{
    ArgInit(cmdLine);

    // Register some abbreviations for command line options.
    ArgAbbreviate("-game", "-g");
    ArgAbbreviate("-gl", "-r"); // As in (R)enderer...
    ArgAbbreviate("-defs", "-d");
    ArgAbbreviate("-width", "-w");
    ArgAbbreviate("-height", "-h");
    ArgAbbreviate("-winsize", "-wh");
    ArgAbbreviate("-bpp", "-b");
    ArgAbbreviate("-window", "-wnd");
    ArgAbbreviate("-nocenter", "-noc");
    ArgAbbreviate("-paltex", "-ptx");
    ArgAbbreviate("-file", "-f");
    ArgAbbreviate("-maxZone", "-mem");
    ArgAbbreviate("-config", "-cfg");
    ArgAbbreviate("-parse", "-p");
    ArgAbbreviate("-cparse", "-cp");
    ArgAbbreviate("-command", "-cmd");
    ArgAbbreviate("-fontdir", "-fd");
    ArgAbbreviate("-modeldir", "-md");
    ArgAbbreviate("-basedir", "-bd");
    ArgAbbreviate("-stdbasedir", "-sbd");
    ArgAbbreviate("-userdir", "-ud");
    ArgAbbreviate("-texdir", "-td");
    ArgAbbreviate("-texdir2", "-td2");
    ArgAbbreviate("-anifilter", "-ani");
    ArgAbbreviate("-verbose", "-v");
}

/**
 * Sets the level of verbosity that was requested using the -verbose
 * option(s).
 */
void DD_Verbosity(void)
{
    verbose = ArgExists("-verbose");
}

/**
 * Called early on during the startup process so that we can get the console
 * online ready for printing ASAP.
 */
boolean DD_EarlyInit(void)
{
    const char*         outFileName = "doomsday.out";

    // We'll redirect stdout to a log file.
    DD_CheckArg("-out", &outFileName);
    outFile = fopen(outFileName, "w");
    if(!outFile)
    {
        DD_ErrorBox(false, "Couldn't open message output file.");
    }
    else
    {
        setbuf(outFile, NULL); // Don't buffer much.

        // Determine the requested degree of verbosity.
        DD_Verbosity();

        // Get the console online ASAP.
        if(!Con_Init())
        {
            DD_ErrorBox(true, "Error initializing console.");
        }
        else
        {
            Con_Message("Executable: " DOOMSDAY_VERSIONTEXT ".\n");

            // Print the used command line.
            if(verbose)
            {
                int         p;

                Con_Message("Command line (%i strings):\n", Argc());
                for(p = 0; p < Argc(); ++p)
                    Con_Message("  %i: %s\n", p, Argv(p));
            }
        }
    }

    // Bring the window manager online.
    Sys_InitWindowManager();

    return true;
}

/**
 * This is called from DD_Shutdown().
 */
void DD_ShutdownAll(void)
{
    int                 i;

    Con_Shutdown();
    DD_ShutdownHelp();
    Zip_Shutdown();

#ifdef WIN32
    // Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
    SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);
#endif

    // Stop all demo recording.
    for(i = 0; i < DDMAXPLAYERS; ++i)
        Demo_StopRecording(i);

    P_ControlShutdown();
    Sv_Shutdown();
    R_Shutdown();
    Def_Destroy();
    F_ShutdownDirec();
    ArgShutdown();
    Z_Shutdown();
    Sys_ShutdownWindowManager();

    // Close the message output file.
    if(outFile)
    {
        fclose(outFile);
        outFile = NULL;
    }
}
