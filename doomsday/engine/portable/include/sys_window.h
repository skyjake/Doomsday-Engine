/**\file sys_window.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * sys_window.h: Window management.
 */

#ifndef LIBDENG_SYS_WINDOW_H
#define LIBDENG_SYS_WINDOW_H

#include "de_base.h"

#if defined(UNIX)
#  include <curses.h>
#endif

#if defined(WIN32)
#  include <windows.h>
#endif

// Structure used to describe what features are available in a window
// manager implementation.
typedef struct wminfo_s {
    uint        maxWindows; /* Max number of simultaneous windows of all
                               supported types
                               0 = Unlimited. */
    uint        maxConsoles;
    boolean     canMoveWindow; // Windows can be moved.
} wminfo_t;

typedef enum {
    WT_NORMAL,
    WT_CONSOLE
} ddwindowtype_t;

// Console commandline flags:
#define CLF_CURSOR_LARGE        0x1 // Use the large command line cursor.

typedef struct {
    ddwindowtype_t  type;
    boolean         inited;
    int             x, y; // SDL cannot move windows; these are ignored.
    int             width, height;
    int             flags;
#if defined(WIN32)
    HWND            hWnd; // Needed for input (among other things).
#endif
    union {
        struct {
#if defined(WIN32)
            HGLRC           glContext;
#endif
            int             bpp;
        } normal;
        struct {
#if defined(WIN32)
            HANDLE      hcScreen;
            CONSOLE_SCREEN_BUFFER_INFO cbInfo;
            WORD        attrib;
#elif defined(UNIX)
            WINDOW     *winTitle, *winText, *winCommand;
#endif
            int         cx, cy;
            int         needNewLine;
            struct {
                int         flags;
            } cmdline;
        } console;
    };
} ddwindow_t;

// Doomsday window flags.
#define DDWF_VISIBLE            0x01
#define DDWF_FULLSCREEN         0x02
#define DDWF_CENTER             0x04

// Flags for Sys_SetWindow()
#define DDSW_NOSIZE             0x01
#define DDSW_NOMOVE             0x02
#define DDSW_NOBPP              0x04
#define DDSW_NOVISIBLE          0x08
#define DDSW_NOFULLSCREEN       0x10
#define DDSW_NOCENTER           0x20
#define DDSW_NOCHANGES          (DDSW_NOSIZE & DDSW_NOMOVE & DDSW_NOBPP & \
                                 DDSW_NOFULLSCREEN & DDSW_NOVISIBLE & \
                                 DDSW_NOCENTER)

/**
 * Currently active window. There is always one active window, so no need
 * to worry about NULLs. The easiest way to get information about the
 * window where drawing is being is done.
 */
extern const ddwindow_t* theWindow;

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y) (theWindow->height - (y+1))

boolean         Sys_InitWindowManager(void);
boolean         Sys_ShutdownWindowManager(void);
boolean         Sys_GetWindowManagerInfo(wminfo_t *info);

boolean         Sys_ChangeVideoMode(int width, int height, int bpp);
boolean         Sys_GetDesktopBPP(int *bpp);

/**
 * Window management.
 */
uint            Sys_CreateWindow(application_t *app, uint parentIDX,
                                 int x, int y, int w, int h, int bpp, int flags,
                                 ddwindowtype_t type, const char *title, void *data);
boolean         Sys_DestroyWindow(uint idx);

void            Sys_UpdateWindow(uint idx);

boolean         Sys_GetWindowDimensions(uint idx, int *x, int *y, int *w, int *h);
boolean         Sys_GetWindowBPP(uint idx, int *bpp);
boolean         Sys_GetWindowFullscreen(uint idx, boolean *fullscreen);
boolean         Sys_GetWindowVisibility(uint idx, boolean *show);

boolean         Sys_SetActiveWindow(uint idx);
boolean         Sys_SetWindow(uint idx, int x, int y, int w, int h, int bpp,
                              uint wflags, uint uflags);
boolean         Sys_SetWindowTitle(uint idx, const char *title);

// Console window routines.
void            Sys_ConPrint(uint idx, const char *text, int flags);
void            Sys_SetConWindowCmdLine(uint idx, const char *text,
                                        unsigned int cursorPos, int clflags);

/**
 *\todo This is a compromise to prevent having to refactor half the
 * engine. In general, system specific patterns should not be carried
 * into the engine.
 */
#if defined(WIN32)
HWND            Sys_GetWindowHandle(uint idx);
#endif

#endif /* LIBDENG_SYS_WINDOW_H */
