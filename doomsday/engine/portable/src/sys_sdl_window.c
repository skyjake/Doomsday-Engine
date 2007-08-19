/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <SDL.h>
#include <SDL_syswm.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void destroyDDWindow(ddwindow_t *win);
static boolean setDDWindow(ddwindow_t *win, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently active window where all drawing operations are directed at.
const ddwindow_t* theWindow;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean winManagerInited = false;

static ddwindow_t mainWindow;
static boolean mainWindowInited = false;

// CODE --------------------------------------------------------------------

static __inline ddwindow_t *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx != 0)
        return NULL;

    return &mainWindow;
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

	// Initialize the SDL video subsystem, unless we're going to run in
    // dedicated mode.
	if(!ArgExists("-dedicated"))
	{
/**
 * @attention Solaris has no Joystick support according to 
 * https://sourceforge.net/tracker/?func=detail&atid=542099&aid=1732554&group_id=74815
 */
#ifdef SOLARIS
		if(SDL_InitSubSystem(SDL_INIT_VIDEO))
#else
		if(SDL_InitSubSystem(SDL_INIT_VIDEO | (!ArgExists("-nojoy")?SDL_INIT_JOYSTICK : 0)))
#endif
		{
			Con_Message("SDL Init Failed: %s\n", SDL_GetError());
			return false;
		}
	}

    memset(&mainWindow, 0, sizeof(mainWindow));
    winManagerInited = true;
    theWindow = &mainWindow;
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

/**
 * Complete the given wminfo_t, detailing what features are supported by
 * this window manager implementation.
 *
 * @param info          Ptr to the wminfo_t structure to complete.
 *
 * @return              <code>true</code> if successful.
 */
boolean Sys_GetWindowManagerInfo(wminfo_t *info)
{
    if(!winManagerInited)
        return false; // Window manager is not initialized.

    if(!info)
        return false; // Wha?

    // Complete the structure detailing what features are available.
    info->canMoveWindow = false;
    info->maxWindows = 1;

    return true;
}

static ddwindow_t *createDDWindow(application_t *app, int w, int h, int bpp,
                                  int flags, const char *title)
{
    // SDL only supports one window.
    if(mainWindowInited)
        return NULL;

    if(!(bpp == 32 || bpp == 16))
    {
        Con_Message("createWindow: Unsupported BPP %i.", bpp);
        return 0;
    }

#if defined(WIN32)
    // We need to grab a handle from SDL so we can link other subsystems
    // (e.g. DX-based input).
    {
    struct SDL_SysWMinfo wmInfo;
    if(!SDL_GetWMInfo(&wmInfo))
        return NULL;

    mainWindow.hWnd = wmInfo.window;
    }
#endif

    setDDWindow(&mainWindow, w, h, bpp, flags,
                DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

    mainWindowInited = true;
    return &mainWindow;
}

/**
 * Create a new (OpenGL-ready) system window.
 *
 * @param app           Ptr to the application structure holding our globals.
 * @param parentIDX     Ignored: SDL does not support parent/child windows.
 * @param x             Ignored: SDL does not support changing X position.
 * @param y             Ignored: SDL does not support changing Y position..
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

    if(isDedicated)
        return 1; // No use.
    
    if(!winManagerInited)
        return 0; // Window manager not initialized yet.

    win = createDDWindow(app, w, h, bpp, flags, title);
    

    if(win)
        return 1; // Success.

    return 0;
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

/**
 * Change the currently active window.
 *
 * @param idx           Index of the window to make active (1-based).
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetActiveWindow(uint idx)
{
    // We only support one window, so yes its active.
    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newWidth, int newHeight,
                           int newBPP, uint wFlags, uint uFlags)
{
    int             width, height, bpp, flags;
    boolean         newGLContext = false;
    boolean         changeWindowDimensions = false;
    boolean         inControlPanel;

    // Window paramaters are not changeable in dedicated mode.
    if(isDedicated)
        return false;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    // Grab the current values.
    width = window->width;
    height = window->height;
    bpp = window->bpp;
    flags = window->flags;
    // Force update on init?
    if(!window->inited)
    {
        newGLContext = true;
    }

    inControlPanel = UI_IsActive();

    // Change to fullscreen?
    if(!(uFlags & DDSW_NOFULLSCREEN) &&
       (flags & DDWF_FULLSCREEN) != (wFlags & DDWF_FULLSCREEN))
    {
        flags ^= DDWF_FULLSCREEN;

        newGLContext = true;
      //  changeVideoMode = true;
    }

    // Change window size?
    if(!(uFlags & DDSW_NOSIZE) && (width != newWidth || height != newHeight))
    {
        width = newWidth;
        height = newHeight;

        //if(flags & DDWF_FULLSCREEN)
        //    changeVideoMode = true;
        newGLContext = true;
        changeWindowDimensions = true;
    }

    // Change BPP (bits per pixel)?
    if(!(uFlags & DDSW_NOBPP) && bpp != newBPP)
    {
        if(!(newBPP == 32 || newBPP == 16))
            Con_Error("Sys_SetWindow: Unsupported BPP %i.", newBPP);

        bpp = newBPP;

        newGLContext = true;
//        changeVideoMode = true;
    }

    if(changeWindowDimensions)
    {
        // Can't change the resolution while the UI is active.
        // (controls need to be repositioned).
        if(inControlPanel)
            UI_End();
    }
/*
    if(changeVideoMode)
    {
        if(flags & DDWF_FULLSCREEN)
        {
            if(!gl.ChangeVideoMode(width, height, bpp))
            {
                Sys_CriticalMessage("Sys_SetWindow: Resolution change failed.");
                return false;
            }
        }
        else
        {
            // Go back to normal display settings.
            ChangeDisplaySettings(0, 0);
        }
    }
*/
    // Update the current values.
    window->width = width;
    window->height = height;
    window->bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify ddwindow_t properties after this point.

    // Do we need a new GL context due to changes to the window?
    if(!novideo && newGLContext)
    {   // Maybe requires a renderer restart.
        boolean         glIsInited = GL_IsInited();
#if defined(WIN32)
        void           *data = window->hWnd;
#else
        void           *data = NULL;
#endif

        if(glIsInited)
        {
            // Shut everything down, but remember our settings.
            GL_TotalReset(true, 0, 0);
            gx.UpdateState(DD_RENDER_RESTART_PRE);

            gl.DestroyContext();
        }

        gl.CreateContext(window->width, window->height, window->bpp,
                         (window->flags & DDWF_FULLSCREEN)? DGL_MODE_FULLSCREEN : DGL_MODE_WINDOW,
                         data);

        if(glIsInited)
        {
            // Re-initialize.
            GL_TotalReset(false, true, true);
            gx.UpdateState(DD_RENDER_RESTART_POST);
        }
    }

    // If the window dimensions have changed, update any sub-systems
    // which need to respond.
    if(changeWindowDimensions)
    {
        if(inControlPanel) // Reactivate the panel?
            Con_Execute(CMDS_DDAY, "panel", true, false);
    }

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
        return setDDWindow(window, newWidth, newHeight, newBPP,
                           wFlags, uFlags);

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
        SDL_WM_SetCaption(title, NULL);
        return true;
    }

    return false;
}

/**
 * Attempt to get the dimensions (and position) of the given window (client
 * area) in screen-space.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param x             Address to write the x position back to,
 *                      unsupported by SDL so always 0.
 * @param y             Address to write the y position back to,
 *                      unsupported by SDL so always 0.
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
        *x = 0;
    if(y)
        *y = 0;
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

/**
 * Attempt to get a HWND handle to the given window.
 *
 * \todo: Factor platform specific design patterns out of Doomsday. We should
 * not be passing around HWND handles...
 *
 * @param idx           Index identifier (1-based) to the window.
 *
 * @return              HWND handle if successful, ELSE <code>NULL</code>.
 */
#if defined(WIN32)
HWND Sys_GetWindowHandle(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return NULL;

    return window->hWnd;
}
#endif
