/** @file dd_pinit.cpp Platform independent routines for initializing the engine.
 * @ingroup base
 *
 * @todo Move these to dd_init.cpp.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "def_main.h"
#include "render/r_main.h"
#include "updater.h"

#include "api_internaldata.h"

/*
 * The game imports and exports.
 */
DENG_DECLARE_API(InternalData) =
{
    { DE_API_INTERNAL_DATA },
    &mobjInfo,
    &states,
    &sprNames,
    &texts,
    &validCount
};
game_export_t __gx;

int DD_CheckArg(char const *tag, const char** value)
{
    /// @todo Add parameter for using NextAsPath().

    int                 i = CommandLine_Check(tag);
    const char*         next = CommandLine_Next();

    if(!i)
        return 0;
    if(value && next)
        *value = next;
    return 1;
}

#ifdef __CLIENT__
void DD_ComposeMainWindowTitle(char* title)
{
    if(App_GameLoaded() && gx.GetVariable)
    {
        sprintf(title, "%s - " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT,
                Str_Text(App_CurrentGame().title()));
    }
    else
    {
        strcpy(title, DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT);
    }
}
#endif

void DD_InitAPI(void)
{
    GETGAMEAPI GetGameAPI = app.GetGameAPI;
    memset(&__gx, 0, sizeof(__gx));
    if(GetGameAPI)
    {
        game_export_t* gameExPtr = GetGameAPI();
        memcpy(&__gx, gameExPtr, MIN_OF(sizeof(__gx), gameExPtr->apiSize));
    }
}

void DD_InitCommandLine(void)
{
    CommandLine_Alias("-game", "-g");
    CommandLine_Alias("-defs", "-d");
    CommandLine_Alias("-width", "-w");
    CommandLine_Alias("-height", "-h");
    CommandLine_Alias("-winsize", "-wh");
    CommandLine_Alias("-bpp", "-b");
    CommandLine_Alias("-window", "-wnd");
    CommandLine_Alias("-nocenter", "-noc");
    CommandLine_Alias("-file", "-f");
    CommandLine_Alias("-config", "-cfg");
    CommandLine_Alias("-parse", "-p");
    CommandLine_Alias("-cparse", "-cp");
    CommandLine_Alias("-command", "-cmd");
    CommandLine_Alias("-fontdir", "-fd");
    CommandLine_Alias("-modeldir", "-md");
    CommandLine_Alias("-basedir", "-bd");
    CommandLine_Alias("-stdbasedir", "-sbd");
    CommandLine_Alias("-userdir", "-ud");
    CommandLine_Alias("-texdir", "-td");
    CommandLine_Alias("-texdir2", "-td2");
    CommandLine_Alias("-anifilter", "-ani");
    CommandLine_Alias("-verbose", "-v");
}

void DD_ConsoleInit(void)
{
    // Get the console online ASAP.
    Con_Init();

    Con_Message("Executable: " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT ".");

    // Print the used command line.
    Con_Message("Command line (%i strings):", CommandLine_Count());
    for(int p = 0; p < CommandLine_Count(); ++p)
    {
        Con_Message("  %i: %s", p, CommandLine_At(p));
    }
}

void DD_ShutdownAll(void)
{
#ifdef __CLIENT__
    Updater_Shutdown();
#endif
    FI_Shutdown();
    UI_Shutdown();
    Con_Shutdown();
    DD_ShutdownHelp();

#ifdef WIN32
    // Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
    SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);
#endif

#ifdef __CLIENT__
    // Stop all demo recording.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        Demo_StopRecording(i);
    }
#endif

    P_ControlShutdown();
#ifdef __SERVER__
    Sv_Shutdown();
#endif
    R_Shutdown();

    // Ensure the global material collection is destroyed.
    App_DeleteMaterials();

    Def_Destroy();
    F_Shutdown();
    DD_ClearResourceClasses();
    Libdeng_Shutdown();
}
