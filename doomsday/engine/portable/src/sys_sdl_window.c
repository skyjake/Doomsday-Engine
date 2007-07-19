/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * sys_sdl_window.c: Cross-platform, SDL-based window management.
 *
 * This code wraps SDL window management routines in order to provide
 * common behavior. The availabilty of features and behavioral traits can
 * be queried for.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    boolean         inited;     // true if the window has been initialized.
                                // i.e. Sys_SetWindow() has been called at
                                // least once for this window.
    int             flags;
    int             x, y, width, height;
    int             bpp;
} ddwindow_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void destroyDDWindow(ddwindow_t *win);
static boolean setDDWindow(ddwindow_t *win, int newX, int newY, int newWidth,
                           int newHeight, int newBPP, uint wFlags,
                           uint uFlags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean winManagerInited = false;

// CODE --------------------------------------------------------------------

static __inline ddwindow_t *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.
/*
    if(idx >= numWindows)
        return NULL;

    return windows[idx];
*/
    return 0;
}

/**
 * Initialize the window manager.
 * Tasks include; checking the system environment for feature enumeration.
 *
 * @return              <code>true</code> if initialization was successful.
 */
boolean Sys_InitWindowManager(void)
{
    if(winManagerInited)
        return true; // Already been here.

    Con_Message("Sys_InitWindowManager: Using SDL window management.\n"); 

    winManagerInited = true;
    return true;
}

/**
 * Shutdown the window manager.
 *
 * @return              <code>true</code> if shutdown was successful.
 */
boolean Sys_ShutdownWindowManager(void)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.


    // Now off-line, no more window management will be possible.
    winManagerInited = false;

    return true;
}

static ddwindow_t *createDDWindow(application_t *app, uint parentIDX,
                                  int x, int y, int w, int h, int bpp,
                                  int flags, const char *title, int nCmdShow)
{
    return NULL;
}

/**
 * Create a new (OpenGL-ready) system window.
 *
 * @param app           Ptr to the application structure holding our globals.
 * @param parentIDX     Index number of the window that is to be the parent
 *                      of the new window. If <code>0</code> window has no
 *                      parent.
 * @param x             X position (in desktop-space).
 * @param y             Y position (in desktop-space).
 * @param w             Width (client area).
 * @param h             Height (client area).
 * @param bpp           BPP (bits-per-pixel)
 * @param flags         DDWF_* flags, control appearance/behavior.
 * @param title         Window title string, ELSE <code>NULL</code>.
 * @param data          Platform specific data.
 *
 * @return              If <code>0</code> window creation was unsuccessful,
 *                      ELSE 1-based index identifier of the new window.
 */
uint Sys_CreateWindow(application_t *app, uint parentIDX,
                      int x, int y, int w, int h, int bpp, int flags,
                      const char *title, void *data)
{
    ddwindow_t *win;

    if(!winManagerInited)
        return 0; // Window manager not initialized yet.

    win = createDDWindow(app, parentIDX, x, y, w, h, bpp, flags, title,
                         NULL);

    if(!win)
    {   // Un-successful.
        return 0;
    }

    return 0 /*numWindows*/; // index + 1.
}

static void destroyDDWindow(ddwindow_t *window)
{

}

/**
 * Destroy the specified window.
 *
 * Side-effects: If the window is fullscreen and the current video mode is
 * not that set as the desktop default: an attempt will be made to change
 * back to the desktop default video mode.
 *
 * @param idx           Index of the window to destroy (1-based).
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_DestroyWindow(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newX, int newY,
                           int newWidth, int newHeight, int newBPP,
                           uint wFlags, uint uFlags)
{
    // Window paramaters are not changeable in dedicated mode.
    if(isDedicated)
        return false;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    return true;
}

/**
 * Attempt to set the appearance/behavioral properties of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param newX          New x position of the left side of the window.
 * @param newY          New y position of the top of the window.
 * @param newWidth      New width to set the window to.
 * @param newHeight     New height to set the window to.
 * @param newBPP        New BPP (bits-per-pixel) to change to.
 * @param wFlags        DDWF_* flags to change other appearance/behavior.
 * @param uFlags        DDSW_* flags govern how the other paramaters should be
 *                      interpreted.
 *
 *                      DDSW_NOSIZE:
 *                      If set, params 'newWidth' and 'newHeight' are ignored
 *                      and no change will be made to the window dimensions.
 *
 *                      DDSW_NOMOVE:
 *                      If set, params 'newX' and 'newY' are ignored and no
 *                      change will be made to the window position.
 *
 *                      DDSW_NOBPP:
 *                      If set, param 'newBPP' is ignored and the no change
 *                      will be made to the window color depth.
 *
 *                      DDSW_NOFULLSCREEN:
 *                      If set, the value of the DDWF_FULLSCREEN bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      fullscreen state of the window.
 *
 *                      DDSW_NOVISIBLE:
 *                      If set, the value of the DDWF_VISIBLE bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      window's visibility.
 *
 *                      DDSW_NOCENTER:
 *                      If set, the value of the DDWF_CENTER bit in param
 *                      'wFlags' is ignored and no change will be made to the
 *                      auto-center state of the window.
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_SetWindow(uint idx, int newX, int newY, int newWidth, int newHeight,
                      int newBPP, uint wFlags, uint uFlags)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
    {

    }

    return false;
}

/**
 * Attempt to set the title of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param title         New title for the window.
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_SetWindowTitle(uint idx, const char *title)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
    {

    }

    return false;
}

/**
 * Attempt to get the dimensions (and position) of the given window (client
 * area) in screen-space.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param x             Address to write the x position back to (if any).
 * @param y             Address to write the y position back to (if any).
 * @param width         Address to write the width back to (if any).
 * @param height        Address to write the height back to (if any).
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_GetWindowDimensions(uint idx, int *x, int *y, int *width,
                                int *height)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || (!x && !y && !width && !height))
        return false;

    // Moving does not work in dedicated mode.
    if(isDedicated)
        return false;

    if(x)
        *x = window->x;
    if(y)
        *y = window->y;
    if(width)
        *width = window->width;
    if(height)
        *height = window->height;

    return true;
}

/**
 * Attempt to get the BPP (bits-per-pixel) of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param bpp           Address to write the BPP back to (if any).
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_GetWindowBPP(uint idx, int *bpp)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || !bpp)
        return false;

    // Not in dedicated mode.
    if(isDedicated)
        return false;

    *bpp = window->bpp;

    return true;
}

/**
 * Attempt to get the fullscreen-state of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param fullscreen    Address to write the fullscreen state back to (if any).
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_GetWindowFullscreen(uint idx, boolean *fullscreen)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || !fullscreen)
        return false;

    *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);

    return true;
}
