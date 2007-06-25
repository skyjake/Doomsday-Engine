/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

/*
 * dd_winit.h: Win32 Initialization
 */

#ifndef __DOOMSDAY_WINIT_H__
#define __DOOMSDAY_WINIT_H__

#include "dd_pinit.h"
#include <windows.h>

typedef struct {
    HWND            hWnd;       // Window handle.
    int             flags;
} ddwindow_t;

typedef struct {
    HINSTANCE       hInstance;
    LPCSTR          className;
    ddwindow_t*     window;     // Main window.
    BOOL            suspendMsgPump; // Set to true to disable checking windows msgs.

    HINSTANCE       hInstGame;  // Instance handle to the game DLL.
    HINSTANCE       hInstPlug[MAX_PLUGS]; // Instances to plugin DLLs.
    GETGAMEAPI      GetGameAPI;
} application_t;

extern application_t app;

ddwindow_t     *DD_GetWindow(uint idx);
void            DD_WindowShow(ddwindow_t *window, boolean show);
void            DD_Shutdown(void);

#endif
