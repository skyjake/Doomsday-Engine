/** @file dd_winit.cpp  Engine Initialization (Windows).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#if defined (_MSC_VER) || defined (__MINGW32__)

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
//#define STRICT
#include <windows.h>
#include <windowsx.h>

#include "de_base.h"
#include "init_windows.h"

#include <objbase.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <tchar.h>

#include <de/app.h>
#ifdef __CLIENT__
#  include "gl/sys_opengl.h"
#endif
#include "sys_system.h"

using namespace de;

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

dd_bool DD_Win32_Init()
{
    SetProcessDPIAware();
    CoInitialize(NULL);

    DoomsdayApp::app().determineGlobalPaths();

    // Perform early initialization of subsystems that require it.
    BOOL failed = TRUE;
    if(!DD_EarlyInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
    }
#ifdef __CLIENT__
    else if(!Sys_GLPreInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing GL.", 0);
    }
#endif
    else
    {
        // All initialization complete.
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

    // No more use of COM beyond, this point.
    CoUninitialize();
}

#endif // defined (_MSC_VER) || defined (__MINGW32__)

