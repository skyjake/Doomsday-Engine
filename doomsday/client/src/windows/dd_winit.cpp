/** @file dd_winit.cpp  Engine Initialization (Windows).
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
//#define STRICT

#ifdef __CLIENT__
#  include <de/DisplayMode>
#endif

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include "resource.h"

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_network.h"
#include "de_misc.h"
#include "de_ui.h"
#include "dd_winit.h"

#include <doomsday/filesys/fs_util.h>
#include <doomsday/paths.h>
#include <de/App>

application_t app;

/**
 * @note GetLastError() should only be called when we *know* an error was thrown.
 * The result of calling this any other time is undefined.
 *
 * @return Ptr to a string containing a textual representation of the last
 * error thrown in the current thread else @c NULL.
 */
char const *DD_Win32_GetLastErrorMessage()
{
    static char *buffer = 0; /// @todo Never free'd!
    static size_t currentBufferSize = 0;

    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(), lpMsgBufLen;
    lpMsgBufLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 0, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, 0);
    if(!lpMsgBuf || lpMsgBufLen == 0)
        return "";

    if(!buffer || (size_t)(lpMsgBufLen+1+8) > currentBufferSize)
    {
        currentBufferSize = (size_t)(lpMsgBufLen+1+8);
        buffer = (char *) M_Realloc(buffer, currentBufferSize);
    }

    dd_snprintf(buffer, currentBufferSize, "#%-5d: ", (int)dw);

    // Continue splitting as long as there are parts.
    char *part, *cursor = (char *)lpMsgBuf;
    while(*(part = M_StrTok(&cursor, "\n")))
    {
        strcat(buffer, part);
    }

    // We're done with the system-allocated message.
    LocalFree(lpMsgBuf);
    return buffer;
}

static BOOL initDGL()
{
#ifdef __CLIENT__
    return (BOOL) Sys_GLPreInit();
#else
    return TRUE;
#endif
}

static void determineGlobalPaths(application_t *app)
{
    DENG2_ASSERT(app != 0);

    de::String binDir = de::App::executablePath().fileNamePath().withSeparators('/');

    // The -userdir option sets the working directory.
    if(CommandLine_CheckWith("-userdir", 1))
    {
        filename_t runtimePath;
        directory_t *temp;

        strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
        Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
        // Ensure the path is closed with a directory separator.
        F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

        temp = Dir_New(runtimePath);
        app->usingUserDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingUserDir)
        {
            DD_SetRuntimePath(Dir_Path(temp));
        }
        Dir_Delete(temp);
    }

    if(!app->usingUserDir)
    {
        // The current working directory is the runtime dir.
        directory_t *temp = Dir_NewFromCWD();
        Dir_SetCurrent(Dir_Path(temp));
        DD_SetRuntimePath(Dir_Path(temp));
        Dir_Delete(temp);
    }

    if(CommandLine_CheckWith("-basedir", 1))
    {
        DD_SetBasePath(CommandLine_Next());
    }
    else
    {
        // The standard base directory is one level up from the bin dir.
        de::String baseDir = de::String(QDir::cleanPath(binDir / de::String(".."))) + '/';
        DD_SetBasePath(baseDir.toUtf8().constData());
    }
}

dd_bool DD_Win32_Init()
{
    BOOL failed = TRUE;

    std::memset(&app, 0, sizeof(app));
    app.hInstance = GetModuleHandle(NULL);

    // Initialize COM.
    CoInitialize(NULL);

    // Prepare the command line arguments.
    DD_InitCommandLine();

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
    else
    {   // All initialization complete.
        failed = FALSE;
    }

#ifdef __CLIENT__
    // No Windows system keys?
    if(CommandLine_Check("-nowsk"))
    {
        // Disable Alt-Tab, Alt-Esc, Ctrl-Alt-Del.  A bit of a hack...
        SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, 0, 0);
        LOG_INPUT_NOTE("Windows system keys disabled");
    }
#endif

    return !failed;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown()
{
    DD_ShutdownAll(); // Stop all engine subsystems.
    Plug_UnloadAll();
    Library_Shutdown();

    // No more use of COM beyond, this point.
    CoUninitialize();

#ifdef __CLIENT__
    DisplayMode_Shutdown();
#endif
}
