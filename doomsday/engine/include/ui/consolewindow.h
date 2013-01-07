/**
 * @file consolewindow.h
 * Private header for console (text mode) windows. @ingroup base
 *
 * @todo This needs further refactoring to fit more nicely into the rest
 *       of the window management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_CONSOLE_WINDOW_H
#define LIBDENG_CONSOLE_WINDOW_H

#include "sys_console.h"

#if defined(UNIX)
#  include <curses.h>
#  include "dd_uinit.h"
#endif

#if defined(WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include "dd_winit.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Console window state.
typedef struct consolewindow_s {
#if defined(WIN32)
    HWND hWnd;
    HANDLE hcScreen;
    CONSOLE_SCREEN_BUFFER_INFO cbInfo;
    WORD attrib;
#elif defined(UNIX)
    WINDOW *winTitle, *winText, *winCommand;
#endif
    int cx, cy;
    int needNewLine;
    struct {
        int flags;
    } cmdline;
} consolewindow_t;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
