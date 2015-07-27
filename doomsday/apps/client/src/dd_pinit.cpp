/** @file dd_pinit.cpp  Platform independent routines for initializing the engine.
 *
 * @todo Move these to dd_init.cpp.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "de_base.h"
#include "dd_pinit.h"

#include <cstdarg>
#include <de/String>
#include <de/Library>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/knownword.h>
#include <doomsday/filesys/fs_main.h>
#ifdef __CLIENT__
#  include "clientapp.h"
#endif
#ifdef __SERVER__
#  include "server/sv_def.h"
#endif
#include "def_main.h"

#include "api_internaldata.h"
#include "api_client.h"
#include "api_console.h"
#include "api_def.h"
#include "api_filesys.h"
#include "api_fontrender.h"
#include "api_material.h"
#include "api_materialarchive.h"
#include "api_render.h"
#include "api_resource.h"
#include "api_sound.h"
#include "api_server.h"

#include "gl/svg.h"

#include "world/p_players.h"

#ifdef __CLIENT__
#  include "network/net_demo.h"

#  include "render/r_draw.h"
#  include "render/r_main.h"
#  include "render/rend_main.h"

#  include "updater.h"
#endif

using namespace de;

/*
 * The game imports and exports.
 */
DENG_DECLARE_API(InternalData) =
{
    { DE_API_INTERNAL_DATA },
    runtimeDefs.mobjInfo.elementsPtr(),
    runtimeDefs.states  .elementsPtr(),
    runtimeDefs.texts   .elementsPtr(),
    &validCount
};

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

void DD_PublishAPIs(::Library *lib)
{
    de::Library &library = Library_File(lib).library();

    if(library.hasSymbol("deng_API"))
    {
        de::Library::deng_API setAPI = library.DENG2_SYMBOL(deng_API);

#define PUBLISH(X) setAPI(X.api.id, &X)

        PUBLISH(_api_Base);
        PUBLISH(_api_Busy);
        PUBLISH(_api_Con);
        PUBLISH(_api_Def);
        PUBLISH(_api_F);
        PUBLISH(_api_Infine);
        PUBLISH(_api_InternalData);
        PUBLISH(_api_Map);
        PUBLISH(_api_MPE);
        PUBLISH(_api_Material);
        PUBLISH(_api_MaterialArchive);
        PUBLISH(_api_Player);
        PUBLISH(_api_R);
        PUBLISH(_api_S);
        PUBLISH(_api_Thinker);
        PUBLISH(_api_Uri);

#ifdef __CLIENT__
        // Client-only APIs.
        PUBLISH(_api_B);
        PUBLISH(_api_Client);
        PUBLISH(_api_FR);
        PUBLISH(_api_GL);
        PUBLISH(_api_Rend);
        PUBLISH(_api_Svg);
#endif

#ifdef __SERVER__
        // Server-only APIs.
        PUBLISH(_api_Server);
#endif
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
    Con_Shutdown();
    DD_ShutdownHelp();

#ifdef WIN32
    // Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
    SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);
#endif

#ifdef __CLIENT__
    // Stop all demo recording.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        Demo_StopRecording(i);
    }
#endif

    P_ClearPlayerImpulses();
#ifdef __SERVER__
    Sv_Shutdown();
#endif
    R_ShutdownSvgs();
#ifdef __CLIENT__
    R_ShutdownViewWindow();
    if(ClientApp::hasRenderSystem())
    {
        ClientApp::renderSystem().clearDrawLists();
    }
#endif
    Def_Destroy();
    F_Shutdown();
    Libdeng_Shutdown();
}
