/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_SYS_WINDOW_H__
#define __DOOMSDAY_SYS_WINDOW_H__

#include "de_base.h"

#if defined(WIN32)
#  include <windows.h>
#endif

// Structure used to describe what features are available in a window
// manager implementation.
typedef struct wminfo_s {
    uint        maxWindows; /* Max number of simultaneous windows.
                               0 = Unlimited. */
    boolean     canMoveWindow; // Windows can be moved.
} wminfo_t;

typedef struct {
#if defined(WIN32)
    HWND            hWnd; // Needed for input (among other things).
    HGLRC           glContext;
#endif
    boolean         inited;
    int             flags;
    int             x, y; // SDL cannot move windows; these are ignored.
    int             width, height;
    int             bpp;
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

boolean         Sys_InitWindowManager(void);
boolean         Sys_ShutdownWindowManager(void);
boolean         Sys_GetWindowManagerInfo(wminfo_t *info);

boolean         Sys_ChangeVideoMode(int width, int height, int bpp);
int             Sys_GetDesktopBPP(void);

/**
 * Window management.
 */
uint            Sys_CreateWindow(application_t *app, uint parent,
                                int x, int y, int w, int h, int bpp, int flags,
                                const char *title, void *data);
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

/**
 *\todo This is a compromise to prevent having to refactor half the
 * engine. In general, system specific patterns should not be carried
 * into the engine.
 */
#if defined(WIN32)
HWND            Sys_GetWindowHandle(uint idx);
#endif

#endif
