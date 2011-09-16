/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Kernen <jaakko.keranen@iki.fi>
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
 * dd_winit.h: Win32 Initialization.
 */

#ifndef __DOOMSDAY_WINIT_H__
#define __DOOMSDAY_WINIT_H__

#include "dd_pinit.h"
#include <windows.h>

#define MAINWCLASS          TEXT("DoomsdayMainWClass")

typedef struct {
    HINSTANCE       hInstance;
#ifdef UNICODE
    LPCWSTR         className;
#else
    LPCSTR          className;
#endif
    BOOL            suspendMsgPump; // Set to true to disable checking windows msgs.
    BOOL            userDirOk;

    HINSTANCE       hInstGame; // Instance handle to the game DLL.
    HINSTANCE       hInstPlug[MAX_PLUGS]; // Instances to plugin DLLs.
    GETGAMEAPI      GetGameAPI;
} application_t;

extern uint windowIDX; // Main window.
extern application_t app;

#ifdef UNICODE
LPCWSTR         ToWideString(const char* str);
LPCSTR          ToAnsiString(const wchar_t* wstr);
#  define WIN_STRING(s)     (ToWideString(s))
#  define UTF_STRING(ws)    (ToAnsiString(ws))
#else
#  define WIN_STRING(s)     (s)
#  define UTF_STRING(ws)    (ws)
#endif

void            DD_Shutdown(void);

#endif
