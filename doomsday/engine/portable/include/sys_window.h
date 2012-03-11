/**\file sys_window.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "dd_types.h"
#include "rect.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(UNIX)
#  include <curses.h>
#  include "dd_uinit.h"
#endif

#if defined(WIN32)
#  include <windows.h>
#  include "dd_winit.h"
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

/**
 * @defgroup consoleCommandlineFlags Console Commandline flags
 * @{
 */
#define CLF_CURSOR_LARGE        0x1 // Use the large command line cursor.
/**@}*/

typedef struct {
    ddwindowtype_t  type;
    boolean         inited;
    /// \note SDL cannot move windows; thus origin is ignored.
    RectRaw        geometry;
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
#define FLIP(y) (theWindow->geometry.size.height - (y+1))

boolean Sys_InitWindowManager(void);
boolean Sys_ShutdownWindowManager(void);
boolean Sys_GetWindowManagerInfo(wminfo_t* info);

boolean Sys_ChangeVideoMode(int width, int height, int bpp);
boolean Sys_GetDesktopBPP(int* bpp);

/**
 * Window management.
 */

/**
 * Create a new window.
 *
 * @param app           Ptr to the application structure holding our globals.
 * @param parentIdx     Index number of the window that is to be the parent
 *                      of the new window. If @c 0, window has no parent.
 * @param origin        Origin of the window in desktop-space.
 * @param size          Size of the window (client area) in desktop-space.
 * @param bpp           BPP (bits-per-pixel)
 * @param flags         DDWF_* flags, control appearance/behavior.
 * @param type          Type of window to be created (WT_*)
 * @param title         Window title string, ELSE @c NULL,.
 * @param data          Platform specific data.
 *
 * @return              If @c 0, window creation was unsuccessful,
 *                      ELSE 1-based index identifier of the new window.
 */
uint Sys_CreateWindow(application_t* app, uint parentIDX, const Point2Raw* origin,
    const Size2Raw* size, int bpp, int flags, ddwindowtype_t type, const char* title, void* data);
boolean Sys_DestroyWindow(uint idx);

void Sys_UpdateWindow(uint idx);

/**
 * Attempt to get the dimensions (and position) of the given window
 * (client area) in screen-space.
 *
 * @param idx  Unique identifier (1-based) to the window.
 * @return  Geometry of the window else @c NULL if @a idx is not valid.
 */
const RectRaw* Sys_GetWindowGeometry(uint idx);
const Point2Raw* Sys_GetWindowOrigin(uint idx);
const Size2Raw* Sys_GetWindowSize(uint idx);

boolean Sys_GetWindowBPP(uint idx, int* bpp);
boolean Sys_GetWindowFullscreen(uint idx, boolean* fullscreen);
boolean Sys_GetWindowVisibility(uint idx, boolean* show);

boolean Sys_SetActiveWindow(uint idx);
boolean Sys_SetWindow(uint idx, int x, int y, int w, int h, int bpp, uint wflags, uint uflags);
boolean Sys_SetWindowTitle(uint idx, const char* title);

ddwindow_t* Sys_Window(uint idx);
ddwindow_t* Sys_MainWindow(void);

/**
 *\todo This is a compromise to prevent having to refactor half the
 * engine. In general, system specific patterns should not be carried
 * into the engine.
 */
#if defined(WIN32)
HWND Sys_GetWindowHandle(uint idx);
#endif

#ifdef __cplusplus
}
#endif

#endif /* LIBDENG_SYS_WINDOW_H */
