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

/**
 * sys_window.c: Win32-specific window management.
 *
 * This code wraps system-specific window management routines in order to
 * provide a cross-platform interface and common behavior.
 * The availabilty of features and behavioral traits can be queried for.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

#define WINDOWEDSTYLE (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | \
                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
#define FULLSCREENSTYLE (WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void destroyDDWindow(ddwindow_t *win);
static boolean setDDWindow(ddwindow_t *win, int newX, int newY, int newWidth,
                           int newHeight, int newBPP, int wFlags,
                           int uFlags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently active window where all drawing operations are directed at.
const ddwindow_t* theWindow;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean winManagerInited = false;

static uint numWindows = 0;
static ddwindow_t **windows = NULL;

// CODE --------------------------------------------------------------------

static __inline ddwindow_t *getWindow(uint idx)
{
    if(!winManagerInited)
        return NULL; // Window manager is not initialized.

    if(idx >= numWindows)
        return NULL;

    return windows[idx];
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

    Con_Message("Sys_InitWindowManager: Using Win32 window management.\n"); 

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

    if(windows)
    {
        uint        i;

        for(i = 0; i < numWindows; ++i)
        {
            Sys_DestroyWindow(i+1);
        }

        M_Free(windows);
        windows = NULL;
    }

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
    info->canMoveWindow = true;
    info->maxWindows = 0;

    return true;
}

static ddwindow_t *createDDWindow(application_t *app, uint parentIDX,
                                  int x, int y, int w, int h, int bpp,
                                  int flags, const char *title)
{
    ddwindow_t *win, *pWin = NULL;
    HWND        phWnd = NULL;
    boolean     ok = true;

    if(!(bpp == 32 || bpp == 16))
    {
        Con_Message("createWindow: Unsupported BPP %i.", bpp);
        return 0;
    }

    if(parentIDX)
    {
        pWin = getWindow(parentIDX - 1);
        if(pWin)
            phWnd = pWin->hWnd;
    }

    // Allocate a structure to wrap the various handles and state variables
    // used with this window.
    if((win = (ddwindow_t*) M_Calloc(sizeof(ddwindow_t))) == NULL)
        return 0;

    // Create the window.
    win->hWnd =
        CreateWindowEx(WS_EX_APPWINDOW, MAINWCLASS, title,
                       WINDOWEDSTYLE,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       phWnd, NULL,
                       app->hInstance, NULL);
    if(!win->hWnd)
    {
        win->hWnd = NULL;
        ok = false;
    }
    else // Initialize.
    {
        PIXELFORMATDESCRIPTOR pfd;
        int         pixForm = 0;
        HDC         hDC = NULL;

        // Setup the pixel format descriptor.
        ZeroMemory(&pfd, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.iLayerType = PFD_MAIN_PLANE;
#ifndef DRMESA
        pfd.dwFlags =
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 32;
#else /* Double Buffer, no alpha */
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
            PFD_GENERIC_FORMAT | PFD_DOUBLEBUFFER | PFD_SWAP_COPY;
        pfd.cColorBits = 24;
        pfd.cRedBits = 8;
        pfd.cGreenBits = 8;
        pfd.cGreenShift = 8;
        pfd.cBlueBits = 8;
        pfd.cBlueShift = 16;
        pfd.cDepthBits = 16;
        pfd.cStencilBits = 8;
#endif

        if(ok)
        {
            // Acquire a device context handle.
            hDC = GetDC(win->hWnd);
            if(!hDC)
            {
                Sys_CriticalMessage("DD_CreateWindow: Failed acquiring device context handle.");
                hDC = NULL;
                ok = false;
            }
            else // Initialize.
            {
                // Nothing to do.
            }
        }

        if(ok)
        {   // Request a matching (or similar) pixel format.
            pixForm = ChoosePixelFormat(hDC, &pfd);
            if(!pixForm)
            {
                Sys_CriticalMessage("DD_CreateWindow: Choosing of pixel format failed.");
                pixForm = -1;
                ok = false;
            }
        }

        if(ok)
        {   // Make sure that the driver is hardware-accelerated.
            DescribePixelFormat(hDC, pixForm, sizeof(pfd), &pfd);
            if((pfd.dwFlags & PFD_GENERIC_FORMAT) && !ArgCheck("-allowsoftware"))
            {
                Sys_CriticalMessage("DD_CreateWindow: GL driver not accelerated!\n"
                                    "Use the -allowsoftware option to bypass this.");
                ok = false;
            }
        }

        if(ok)
        {   // Set the pixel format for the device context. Can only be done once
            // (unless we release the context and acquire another).
            if(!SetPixelFormat(hDC, pixForm, &pfd))
            {
                Sys_CriticalMessage("DD_CreateWindow: Warning, setting of pixel "
                                    "format failed.");
            }
        }

        // We've now finished with the device context.
        if(hDC)
            ReleaseDC(win->hWnd, hDC);

        setDDWindow(win, x, y, w, h, bpp, flags,
                    DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

        // Ensure new windows are hidden on creation.
        ShowWindow(win->hWnd, SW_HIDE);
    }

    if(!ok)
    {   // Damn, something went wrong... clean up.
        destroyDDWindow(win);
        return NULL;
    }

    return win;
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
    //int         nCmdShow = (data? *((int*) data) : 0); // Currently ignored.

    if(!winManagerInited)
        return 0; // Window manager not initialized yet.

    win = createDDWindow(app, parentIDX, x, y, w, h, bpp, flags, title);

    if(win)
    {   // Success, link it in.
        uint        i = 0;
        ddwindow_t **newList = M_Malloc(sizeof(ddwindow_t*) * ++numWindows);

        // Copy the existing list?
        if(windows)
        {
            for(; i < numWindows - 1; ++i)
                newList[i] = windows[i];
        }

        // Add the new one to the end.
        newList[i] = win;

        // Replace the list.
        if(windows)
            M_Free(windows); // Free windows? har, har.
        windows = newList;
    }
    else
    {   // Un-successful.
        return 0;
    }

    // Make this the new active window.
    theWindow = win;

    return numWindows; // index + 1.
}

static void destroyDDWindow(ddwindow_t *window)
{
    if(window->flags & DDWF_FULLSCREEN)
    {   // Change back to the desktop before doing anything further to try
        // and circumvent crusty old drivers from corrupting the desktop.
        ChangeDisplaySettings(NULL, 0);
    }

    // Destroy the window and release the handle.
    if(window->hWnd && !DestroyWindow(window->hWnd))
    {
        DD_ErrorBox(true, "Error destroying window.");
        window->hWnd = NULL;
    }

    // Free any local memory we acquired for managing the window's state, then
    // finally the ddwindow.
    M_Free(window);
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

    destroyDDWindow(window);
    windows[idx-1] = NULL;
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
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    theWindow = window;
    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newX, int newY,
                           int newWidth, int newHeight, int newBPP,
                           int wFlags, int uFlags)
{
    int             x, y, width, height, bpp, flags;
    HWND            hWnd;
    boolean         newGLContext = false;
    boolean         updateStyle = false;
    boolean         changeVideoMode = false;
    boolean         changeWindowDimensions = false;
    boolean         noMove = (uFlags & DDSW_NOMOVE);
    boolean         noSize = (uFlags & DDSW_NOSIZE);
    boolean         inControlPanel;

    // Window paramaters are not changeable in dedicated mode.
    if(isDedicated)
        return false;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    // Grab the current values.
    hWnd = window->hWnd;
    x = window->x;
    y = window->y;
    width = window->width;
    height = window->height;
    bpp = window->bpp;
    flags = window->flags;
    // Force update on init?
    if(!window->inited)
    {
        newGLContext = updateStyle = true;
    }

    inControlPanel = UI_IsActive();

    // Change auto window centering?
    if(!(uFlags & DDSW_NOCENTER) &&
       (flags & DDWF_CENTER) != (wFlags & DDWF_CENTER))
    {
        flags ^= DDWF_CENTER;
    }

    // Change to fullscreen?
    if(!(uFlags & DDSW_NOFULLSCREEN) &&
       (flags & DDWF_FULLSCREEN) != (wFlags & DDWF_FULLSCREEN))
    {
        flags ^= DDWF_FULLSCREEN;

        newGLContext = true;
        updateStyle = true;
        changeVideoMode = true;
    }

    // Change window size?
    if(!(uFlags & DDSW_NOSIZE) && (width != newWidth || height != newHeight))
    {
        width = newWidth;
        height = newHeight;

        if(flags & DDWF_FULLSCREEN)
            changeVideoMode = true;
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
        changeVideoMode = true;
    }

    if(changeWindowDimensions)
    {
        // Can't change the resolution while the UI is active.
        // (controls need to be repositioned).
        if(inControlPanel)
            UI_End();
    }

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

    // Change window position?
    if(flags & DDWF_FULLSCREEN)
    {
        if(x != 0 || y != 0)
        {   // Force move to [0,0]
            x = y = 0;
            noMove = false;
        }
    }
    else if(!(uFlags & DDSW_NOMOVE))
    {
        if(flags & DDWF_CENTER)
        {   // Auto centering mode.
            x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
            y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        }
        else if(x != newX || y != newY)
        {
            x = newX;
            y = newY;
        }

        // Are we in range here?
        if(width > GetSystemMetrics(SM_CXSCREEN))
            width = GetSystemMetrics(SM_CXSCREEN);

        if(height > GetSystemMetrics(SM_CYSCREEN))
            height = GetSystemMetrics(SM_CYSCREEN);
    }

    // Change visibility?
    if(!(uFlags & DDSW_NOVISIBLE) &&
       (flags & DDWF_VISIBLE) != (wFlags & DDWF_VISIBLE))
    {
        flags ^= DDWF_VISIBLE;
    }

    // Hide the window?
    if(!(flags & DDWF_VISIBLE))
        ShowWindow(hWnd, SW_HIDE);

    // Update the current values.
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify ddwindow_t properties after this point.

    if(updateStyle)
    {   // We need to request changes to the window style.
        LONG    style;

        if(flags & DDWF_FULLSCREEN)
            style = FULLSCREENSTYLE;
        else
            style = WINDOWEDSTYLE;

        SetWindowLong(hWnd, GWL_STYLE, style);
    }

    if(!(flags & DDWF_FULLSCREEN))
    {   // We need to have a large enough client area.
        RECT        rect;

        rect.left = x;
        rect.top = y;
        rect.right = x + width;
        rect.bottom = y + height;
        AdjustWindowRect(&rect, WINDOWEDSTYLE, false);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
        noSize = false;
    }

    // Make it so.
    SetWindowPos(hWnd, 0,
                 x, y, width, height,
                 (noSize? SWP_NOSIZE : 0) |
                 (noMove? SWP_NOMOVE : 0) |
                 (updateStyle? SWP_FRAMECHANGED : 0) |
                 SWP_NOZORDER | SWP_NOCOPYBITS | SWP_NOACTIVATE);

    // Do we need a new GL context due to changes to the window?
    if(!novideo && newGLContext)
    {   // Maybe requires a renderer restart.
        boolean         glIsInited = GL_IsInited();

        if(glIsInited)
        {
            // Shut everything down, but remember our settings.
            GL_TotalReset(true, 0, 0);
            gx.UpdateState(DD_RENDER_RESTART_PRE);

            gl.DestroyContext();
        }

        gl.CreateContext(window->width, window->height, window->bpp,
                         (window->flags & DDWF_FULLSCREEN)? DGL_MODE_FULLSCREEN : DGL_MODE_WINDOW,
                         window->hWnd);

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

    // Show the hidden window?
    if(flags & DDWF_VISIBLE)
    {
        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);
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
        return setDDWindow(window, newX, newY, newWidth, newHeight, newBPP,
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
        return (SetWindowText(window->hWnd, (title))? true : false);

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
HWND Sys_GetWindowHandle(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return NULL;

    return window->hWnd;
}
