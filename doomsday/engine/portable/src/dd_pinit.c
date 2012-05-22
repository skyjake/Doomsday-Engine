/**\file dd_pinit.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Portable Engine Initialization
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
#include "de_network.h"
#include "de_ui.h"
#include "de_filesys.h"

#include "m_args.h"
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

/**
 * Compose the title for the main window.
 */
void DD_ComposeMainWindowTitle(char* title)
{
    if(DD_GameLoaded() && gx.GetVariable)
    {
        sprintf(title, DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "%s - %s (%s %s)",
            (isDedicated? " (Dedicated)" : ""), Str_Text(Game_Title(theGame)),
            (char*) gx.GetVariable(DD_PLUGIN_NAME), (char*) gx.GetVariable(DD_PLUGIN_VERSION_SHORT));
    }
    else
    {
        sprintf(title, DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "%s", (isDedicated? " (Dedicated)" : ""));
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
    GETGAMEAPI GetGameAPI = app.GetGameAPI;

    // Put the imported stuff into the imports.
    SetGameImports(&__gi);

    memset(&__gx, 0, sizeof(__gx));
    if(GetGameAPI)
    {
        game_export_t* gameExPtr = GetGameAPI(&__gi);
        memcpy(&__gx, gameExPtr, MIN_OF(sizeof(__gx), gameExPtr->apiSize));
    }
}

void DD_InitCommandLine(void)
{
#if 0
    ArgInit(cmdLine);
#endif

    // Register some abbreviations for command line options.
    ArgAbbreviate("-game", "-g");
    ArgAbbreviate("-defs", "-d");
    ArgAbbreviate("-width", "-w");
    ArgAbbreviate("-height", "-h");
    ArgAbbreviate("-winsize", "-wh");
    ArgAbbreviate("-bpp", "-b");
    ArgAbbreviate("-window", "-wnd");
    ArgAbbreviate("-nocenter", "-noc");
    ArgAbbreviate("-file", "-f");
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
 * Called early on during the startup process so that we can get the console
 * online ready for printing ASAP.
 */
void DD_ConsoleInit(void)
{
    const char* outFileName = "doomsday.out";
    ddstring_t nativePath;
    boolean outFileOk;

    DD_CheckArg("-out", &outFileName);
    Str_Init(&nativePath); Str_Set(&nativePath, outFileName);
    F_ToNativeSlashes(&nativePath, &nativePath);

    // We'll redirect stdout to a log file.
    outFileOk = LegacyCore_SetLogFile(de2LegacyCore, Str_Text(&nativePath));
    //outFile = fopen(Str_Text(&nativePath), "w");
    Str_Free(&nativePath);

    if(!outFileOk)
    {
        Sys_MessageBoxf(MBT_WARNING, "Console", "Couldn't open message output file: %s",
                        LegacyCore_LogFile(de2LegacyCore));
    }

    // Get the console online ASAP.
    Con_Init();

    Con_Message("Executable: " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT ".\n");

    // Print the used command line.
    if(verbose)
    {
        int p;
        Con_Message("Command line (%i strings):\n", Argc());
        for(p = 0; p < Argc(); ++p)
            Con_Message("  %i: %s\n", p, Argv(p));
    }
}

/**
 * This is called from DD_Shutdown().
 */
void DD_ShutdownAll(void)
{
    int i;

    FI_Shutdown();
    UI_Shutdown();
    Con_Shutdown();
    DD_ShutdownHelp();

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
    Materials_Shutdown();
    Def_Destroy();
    F_ShutdownResourceLocator();
    F_Shutdown();
    Garbage_Shutdown(); // before memzone
    Z_Shutdown();
    Sys_ShutdownWindowManager();

    /*
    // Close the message output file.
    if(outFile)
    {
        fclose(outFile);
        outFile = NULL;
    }
    */
}
