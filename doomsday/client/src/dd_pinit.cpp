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

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_network.h"
#include "de_ui.h"
#include "de_filesys.h"

#include "def_main.h"
#include "gl/svg.h"
#ifdef __CLIENT__
#  include "render/r_draw.h"
#  include "render/r_main.h"
#  include "render/rend_main.h"
#  include "updater.h"
#endif

#include "api_internaldata.h"

#include <de/String>
#include <cstdarg>

using namespace de;

/*
 * The game imports and exports.
 */
DENG_DECLARE_API(InternalData) =
{
    { DE_API_INTERNAL_DATA },
    runtimeDefs.mobjInfo.elementsPtr(),
    runtimeDefs.states  .elementsPtr(),
    runtimeDefs.sprNames.elementsPtr(),
    runtimeDefs.texts   .elementsPtr(),
    &validCount
};
game_export_t __gx;

#ifdef __CLIENT__
de::String DD_ComposeMainWindowTitle()
{
    de::String title = DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT;

    if(App_GameLoaded() && gx.GetVariable)
    {
        title = App_CurrentGame().title() + " - " + title;
    }

    return title;
}
#endif

void DD_InitAPI()
{
    GETGAMEAPI GetGameAPI = app.GetGameAPI;
    zap(__gx);
    if(GetGameAPI)
    {
        game_export_t *gameExPtr = GetGameAPI();
        std::memcpy(&__gx, gameExPtr, MIN_OF(sizeof(__gx), gameExPtr->apiSize));
    }
}

void DD_InitCommandLine()
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

static void App_AddKnownWords()
{
    // Add games as known words.
    foreach(Game *game, App_Games().all())
    {
        Con_AddKnownWord(WT_GAME, game);
    }
}

void DD_ConsoleInit()
{
    // Get the console online ASAP.
    Con_Init();
    Con_SetApplicationKnownWordCallback(App_AddKnownWords);

    LOG_NOTE("Executable: " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_FULLTEXT);

    // Print the used command line.
    LOG_MSG("Command line options:");
    for(int p = 0; p < CommandLine_Count(); ++p)
    {
        LOG_MSG("  %i: " _E(>) "%s") << p << CommandLine_At(p);
    }
}

void DD_ShutdownAll()
{
    App_InFineSystem().reset();
#ifdef __CLIENT__
    App_InFineSystem().deinitBindingContext();
#endif
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
    R_ShutdownSvgs();
#ifdef __CLIENT__
    R_ShutdownViewWindow();
    Rend_Shutdown();
#endif
    Def_Destroy();
    F_Shutdown();
    Libdeng_Shutdown();
}
