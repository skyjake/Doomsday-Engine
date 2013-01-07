// OBSOLETE: see window.cpp (GUI) and sys_console.c (text-mode)

/**\file sys_window.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Win32-specific window management.
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
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_ui.h"

#include "rend_particle.h" // \todo Should not be necessary at this level.

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void destroyWindow(ddwindow_t *win);
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

static int screenWidth, screenHeight, screenBPP;

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
 * Attempt to get the BPP (bits-per-pixel) of the local desktop.
 *
 * @param bpp           Address to write the BPP back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetDesktopBPP(int *bpp)
{
    if(!winManagerInited)
        return false;

    if(bpp)
    {
        HWND                hDesktop = GetDesktopWindow();
        HDC                 desktop_hdc = GetDC(hDesktop);
        int                 deskbpp = GetDeviceCaps(desktop_hdc, PLANES) *
            GetDeviceCaps(desktop_hdc, BITSPIXEL);

        ReleaseDC(hDesktop, desktop_hdc);
        *bpp = deskbpp;
        return true;
    }

    return false;
}

/**
 * Change the display mode using the Win32 API, the closest available
 * refresh rate is selected.
 *
 * @param width         Requested horizontal resolution.
 * @param height        Requested vertical resolution.
 * @param bpp           Requested bits per pixel.
 *
 * @return              Non-zero= success.
 */
int Sys_ChangeVideoMode(int width, int height, int bpp)
{
    int                 res, i;
    DEVMODE             current, testMode, newMode;

    if(!winManagerInited || width < 0 || height < 0)
        return 0;

    LIBDENG_ASSERT_IN_MAIN_THREAD();

    Sys_GetDesktopBPP(&screenBPP);

    // First get the current settings.
    memset(&current, 0, sizeof(current));
    current.dmSize = sizeof(DEVMODE);
    if(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current))
    {
        if(!bpp)
            bpp = current.dmBitsPerPel;
    }
    else if(!bpp)
    {   // A safe fallback.
        bpp = 16;
    }

    if(bpp < 0) return 0;

    if((unsigned)width == current.dmPelsWidth && (unsigned)height == current.dmPelsHeight &&
       (unsigned)bpp == current.dmBitsPerPel)
       return 1; // No need to change, so success!

    // Override refresh rate?
    if(ArgCheckWith("-refresh", 1))
        current.dmDisplayFrequency = strtol(ArgNext(), 0, 0);

    // Clear the structure.
    memset(&newMode, 0, sizeof(newMode));
    newMode.dmSize = sizeof(newMode);

    // Let's enumerate all possible modes to find the most suitable one.
    for(i = 0;; i++)
    {
        memset(&testMode, 0, sizeof(testMode));
        testMode.dmSize = sizeof(DEVMODE);
        if(!EnumDisplaySettings(NULL, i, &testMode))
            break;

        if(testMode.dmPelsWidth == (unsigned) width &&
           testMode.dmPelsHeight == (unsigned) height &&
           testMode.dmBitsPerPel == (unsigned) bpp)
        {
            // This looks promising. We'll take the one that best matches
            // the current refresh rate.
            if(abs(current.dmDisplayFrequency - testMode.dmDisplayFrequency) <
               abs(current.dmDisplayFrequency - newMode.dmDisplayFrequency))
            {
                memcpy(&newMode, &testMode, sizeof(DEVMODE));
            }
        }
    }

    if(!newMode.dmPelsWidth)
    {
        // A perfect match was not found. Let's try something.
        newMode.dmPelsWidth = width;
        newMode.dmPelsHeight = height;
        newMode.dmBitsPerPel = bpp;
        newMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    }

    if((res = ChangeDisplaySettings(&newMode, 0)) != DISP_CHANGE_SUCCESSFUL)
    {
        Con_Message("Sys_ChangeVideoMode: Error %x.\n", res);
        return 0; // Failed, damn you.
    }

    // Update the screen size variables.
    screenWidth = width;
    screenHeight = height;
    if(bpp)
        screenBPP = bpp;

    return 1; // Success.
}

/**
 * Initialize the window manager.
 * Tasks include; checking the system environment for feature enumeration.
 *
 * @return              @c true, if initialization was successful.
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
 * @return              @c true, if shutdown was successful.
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

    // Go back to normal display settings.
    ChangeDisplaySettings(0, 0);

    // Now off-line, no more window management will be possible.
    winManagerInited = false;

    return true;
}

/**
 * Attempt to acquire a device context for OGL rendering and then init.
 *
 * @param window        Ptr to the window to attach the context to.
 *
 * @return              @c true iff successful.
 */
static boolean createContext(ddwindow_t *window)
{
    HDC                 hdc;
    boolean             ok = true;

    if(window->type != WT_NORMAL)
        Sys_CriticalMessage("createContext: Window type does not support "
                            "rendering contexts.");

    hdc = GetDC(window->hWnd);
    if(!hdc)
    {
        Sys_CriticalMessage("createContext: Failed acquiring device.");
        ok = false;
    }

    // Create the OpenGL rendering context.
    if(!(window->normal.glContext = wglCreateContext(hdc)))
    {
        Sys_CriticalMessage("createContext: Creation of rendering context "
                            "failed.");
        ok = false;
    }
    // Make the context current.
    else if(!wglMakeCurrent(hdc, window->normal.glContext))
    {
        Sys_CriticalMessage("createContext: Couldn't make the rendering "
                            "context current.");
        ok = false;
    }

    if(hdc)
        ReleaseDC(window->hWnd, hdc);

    return ok;
}

/**
 * Complete the given wminfo_t, detailing what features are supported by
 * this window manager implementation.
 *
 * @param info          Ptr to the wminfo_t structure to complete.
 *
 * @return              @c true, if successful.
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
    info->maxConsoles = 1;

    return true;
}

static ddwindow_t* createGLWindow(application_t* app, uint parentIdx,
    const Point2Raw* origin, const Size2Raw* size, int bpp, int flags,
    const char* title)
{
    ddwindow_t* win, *pWin = NULL;
    HWND phWnd = NULL;
    boolean ok = true;

    if(!(bpp == 32 || bpp == 16))
    {
        Con_Message("createWindow: Unsupported BPP %i.\n", bpp);
        return NULL;
    }

    if(parentIdx)
    {
        pWin = getWindow(parentIdx - 1);
        if(pWin) phWnd = pWin->hWnd;
    }

    win = (ddwindow_t*) calloc(1, sizeof *win);
    if(!win) return NULL;

    win->type = WT_NORMAL;

    // Create the window.
    win->hWnd =
        CreateWindowEx(WS_EX_APPWINDOW, TEXT(MAINWCLASS), WIN_STRING(title),
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
        int pixForm = 0;
        HDC hDC = NULL;

        // Setup the pixel format descriptor.
        ZeroMemory(&pfd, sizeof pfd);
        pfd.nSize = sizeof pfd;
        pfd.nVersion = 1;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.iLayerType = PFD_MAIN_PLANE;
#ifndef DRMESA
        pfd.dwFlags =
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = (bpp == 32? 24 : 16);
        pfd.cDepthBits = 16;
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
        {
            // Choose a suitable pixel format.
            // If multisampling is available, make use of it.
            if(GL_state.features.multisample)
            {
                pixForm = GL_state.multisampleFormat;
            }
            else
            {   // Request a matching (or similar) pixel format.
                pixForm = ChoosePixelFormat(hDC, &pfd);
                if(!pixForm)
                {
                    Sys_CriticalMessage("DD_CreateWindow: Choosing of pixel format failed.");
                    pixForm = -1;
                    ok = false;
                }
            }
        }

        if(ok)
        {   // Make sure that the driver is hardware-accelerated.
            DescribePixelFormat(hDC, pixForm, sizeof pfd, &pfd);
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
                Sys_CriticalMessage("DD_CreateWindow: Failed setting pixel format.");
                ok = false;
            }
        }

        // We've now finished with the device context.
        if(hDC)
            ReleaseDC(win->hWnd, hDC);
    }

    setDDWindow(win, origin->x, origin->y, size->width, size->height, bpp, flags,
                DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

    if(win->hWnd)
    {
        // Ensure new windows are hidden on creation.
        ShowWindow(win->hWnd, SW_HIDE);
    }

    if(!ok)
    {   // Damn, something went wrong... clean up.
        destroyWindow(win);
        return NULL;
    }

    return win;
}

uint Sys_CreateWindow(application_t* app, uint parentIdx, const Point2Raw* origin,
    const Size2Raw* size, int bpp, int flags, ddwindowtype_t type, const char* title,
    void* userData)
{
    ddwindow_t* win = NULL;
    /* Currently ignored.
    int nCmdShow = (userData? *((int*) userData) : 0); */

    if(!winManagerInited) return 0;

    switch(type)
    {
    case WT_NORMAL:
        win = createGLWindow(app, parentIdx, origin, size, bpp, flags, title);
        break;

    case WT_CONSOLE:
        win = createConsoleWindow(app, parentIdx, origin, size, bpp, flags, title);
        break;

    default:
        Con_Error("Sys_CreateWindow: Unknown window type (%i).", (int) type);
        break;
    }

    if(win)
    {   // Success, link it in.
        uint i = 0;
        ddwindow_t** newList = malloc(sizeof *newList * ++numWindows);

        // Copy the existing list?
        if(windows)
        {
            for(; i < numWindows - 1; ++i)
                newList[i] = windows[i];
        }

        // Add the new one to the end.
        newList[i] = win;

        // Replace the list.
        if(windows) free(windows); // Free windows? har, har.
        windows = newList;
    }
    else
    {   // Un-successful.
        return 0;
    }

    // Make this the new active window.
    Sys_SetActiveWindow(numWindows); // index + 1;

    return numWindows; // index + 1.
}

static boolean destroyGLWindow(ddwindow_t *window)
{
    // Delete the window's rendering context if one has been acquired.
    if(window->normal.glContext)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(window->normal.glContext);
    }

    // Destroy the window and release the handle.
    if(window->hWnd && !DestroyWindow(window->hWnd))
        return false;

    return true;
}


static void destroyWindow(ddwindow_t *window)
{
    boolean         ok = true;

    if(window->flags & DDWF_FULLSCREEN)
    {   // Change back to the desktop before doing anything further to try
        // and circumvent crusty old drivers from corrupting the desktop.
        ChangeDisplaySettings(NULL, 0);
    }

    switch(window->type)
    {
    case WT_NORMAL:
        ok = destroyGLWindow(window);
        break;

    case WT_CONSOLE:
        ok = destroyConsoleWindow(window);
        break;

    default:
        Con_Error("destroyWindow: Invalid window type %i.", window->type);
        break;
    };

    // Free any local memory we acquired for managing the window's state, then
    // finally the ddwindow.
    M_Free(window);

    if(!ok)
        DD_ErrorBox(true, "Error destroying window.");
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
 * @return              @c true, if successful.
 */
boolean Sys_DestroyWindow(uint idx)
{
    if(winManagerInited)
    {
        ddwindow_t         *window = getWindow(idx - 1);

        if(window)
        {
            destroyWindow(window);
            windows[idx-1] = NULL;
            return true;
        }
    }

    return false;
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
    if(winManagerInited)
    {
        ddwindow_t         *window = getWindow(idx - 1);

        if(window)
        {
            theWindow = window;
            return true;
        }
    }

    return false;
}

static boolean setDDWindow(ddwindow_t *window, int newX, int newY,
                           int newWidth, int newHeight, int newBPP,
                           int wFlags, int uFlags)
{
    RectRaw geometry;
    int bpp, flags;
    HWND hWnd;
    boolean newGLContext = false;
    boolean updateStyle = false;
    boolean changeVideoMode = false;
    boolean changeWindowDimensions = false;
    boolean noMove = (uFlags & DDSW_NOMOVE);
    boolean noSize = (uFlags & DDSW_NOSIZE);
    boolean inControlPanel = false;
    boolean noFrame;

    if(uFlags & DDSW_NOCHANGES)
        return true; // Nothing to do.

    // Grab the current values.
    hWnd = window->hWnd;
    memcpy(&geometry, &window->geometry, sizeof geometry);
    bpp = window->normal.bpp;
    flags = window->flags;
    noFrame = ArgCheck("-noframe");
    // Force update on init?
    if(!window->inited && window->type == WT_NORMAL)
    {
        newGLContext = updateStyle = true;
    }

    if(window->type == WT_NORMAL)
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

        if(window->type == WT_NORMAL)
        {
            newGLContext = true;
            updateStyle = true;
            changeVideoMode = true;
        }
    }

    // Change window size?
    if(!(uFlags & DDSW_NOSIZE) &&
       (geometry.size.width != newWidth || geometry.size.height != newHeight))
    {
        geometry.size.width  = newWidth;
        geometry.size.height = newHeight;

        if(window->type == WT_NORMAL)
        {
            if(flags & DDWF_FULLSCREEN)
                changeVideoMode = true;
            newGLContext = true;
        }

        changeWindowDimensions = true;
    }

    if(window->type == WT_NORMAL)
    {
        // Change BPP (bits per pixel)?
        if(!(uFlags & DDSW_NOBPP) && bpp != newBPP)
        {
            if(!(newBPP == 32 || newBPP == 16))
                Con_Error("Sys_SetWindow: Unsupported BPP %i.", newBPP);

            bpp = newBPP;

            newGLContext = true;
            changeVideoMode = true;
        }
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
            if(!Sys_ChangeVideoMode(geometry.size.width, geometry.size.height, bpp))
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
        if(geometry.origin.x != 0 || geometry.origin.y != 0)
        {   // Force move to [0,0]
            geometry.origin.x = geometry.origin.y = 0;
            noMove = false;
        }
    }
    else if(!(uFlags & DDSW_NOMOVE))
    {
        if(flags & DDWF_CENTER)
        {   // Auto centering mode.
            geometry.origin.x = (GetSystemMetrics(SM_CXVIRTUALSCREEN) - geometry.size.width) / 2;
            geometry.origin.y = (GetSystemMetrics(SM_CYVIRTUALSCREEN) - geometry.size.height) / 2;
        }
        else if(geometry.origin.x != newX || geometry.origin.y != newY)
        {
            geometry.origin.x = newX;
            geometry.origin.y = newY;
        }

        // Are we in range here?
        if(geometry.size.width > GetSystemMetrics(SM_CXVIRTUALSCREEN))
            geometry.size.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);

        if(geometry.size.height > GetSystemMetrics(SM_CYVIRTUALSCREEN))
            geometry.size.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
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
    memcpy(&window->geometry, &geometry, sizeof window->geometry);
    if(window->type == WT_NORMAL)
        window->normal.bpp = bpp;
    window->flags = flags;
    if(!window->inited)
        window->inited = true;

    // Do NOT modify ddwindow_t properties after this point.

    if(updateStyle)
    {   // We need to request changes to the window style.
        LONG    style;

        if((flags & DDWF_FULLSCREEN) || noFrame)
            style = FULLSCREENSTYLE;
        else
            style = WINDOWEDSTYLE;

        SetWindowLong(hWnd, GWL_STYLE, style);
    }

    if(!(flags & DDWF_FULLSCREEN) && !noFrame)
    {   // We need to have a large enough client area.
        RECT        rect;

        rect.left  = geometry.origin.x;
        rect.right = geometry.origin.x + geometry.size.width;
        rect.top    = geometry.origin.y;
        rect.bottom = geometry.origin.y + geometry.size.height;
        AdjustWindowRect(&rect, WINDOWEDSTYLE, false);
        geometry.size.width  = rect.right  - rect.left;
        geometry.size.height = rect.bottom - rect.top;
        noSize = false;
    }

    // Make it so.
    SetWindowPos(hWnd, 0,
                 geometry.origin.x, geometry.origin.y,
                 geometry.size.width, geometry.size.height,
                 (noSize? SWP_NOSIZE : 0) |
                 (noMove? SWP_NOMOVE : 0) |
                 (updateStyle? SWP_FRAMECHANGED : 0) |
                 SWP_NOZORDER | SWP_NOCOPYBITS | SWP_NOACTIVATE);

    // Do we need a new GL context due to changes to the window?
    if(!novideo && newGLContext)
    {
        // Maybe requires a renderer restart.
extern boolean usingFog;

        boolean glIsInited = GL_IsInited();
        boolean hadFog;

        if(glIsInited)
        {
            // Shut everything down, but remember our settings.
            hadFog = usingFog;
            GL_TotalReset();

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_PRE);

            R_UnloadSvgs();
            GL_ReleaseTextures();

            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(window->normal.glContext);
        }

        if(createContext(window))
        {
            // We can get on with initializing the OGL state.
            Sys_GLConfigureDefaultState();
        }

        if(glIsInited)
        {
            // Re-initialize.
            GL_TotalRestore();
            GL_InitRefresh();

            if(hadFog)
                GL_UseFog(true);

            if(DD_GameLoaded() && gx.UpdateState)
                gx.UpdateState(DD_RENDER_RESTART_POST);
        }
    }

    // If the window dimensions have changed, update any sub-systems
    // which need to respond.
    if(changeWindowDimensions && window->type == WT_NORMAL)
    {
        // Update viewport coordinates.
        R_SetViewGrid(0, 0);

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
 * @return              @c true, if successful.
 */
boolean Sys_SetWindow(uint idx, int newX, int newY, int newWidth, int newHeight,
                      int newBPP, uint wFlags, uint uFlags)
{
    if(winManagerInited)
    {
        ddwindow_t* window = getWindow(idx - 1);
        if(window)
        {
            return setDDWindow(window, newX, newY, newWidth, newHeight, newBPP, wFlags, uFlags);
        }
    }
    return false;
}

/**
 * Update the contents of the window.
 */
void Sys_UpdateWindow(uint idx)
{
    if(winManagerInited)
    {
        ddwindow_t* window = getWindow(idx - 1);

        if(window->type == WT_NORMAL)
        {
            if(window->normal.glContext)
            {   // Window has a glContext attached, so make the content of the
                // framebuffer visible.
                HDC hdc = GetDC(window->hWnd);

                LIBDENG_ASSERT_IN_MAIN_THREAD();

                if(GL_state.forceFinishBeforeSwap)
                {
                    glFinish();
                }

                // Swap buffers.
                glFlush();
                SwapBuffers(hdc);
                ReleaseDC(window->hWnd, hdc);
            }
        }
    }
}

/**
 * Attempt to set the title of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param title         New title for the window.
 *
 * @return              @c true, if successful.
 */
boolean Sys_SetWindowTitle(uint idx, const char *title)
{
    if(winManagerInited)
    {
        ddwindow_t* window = getWindow(idx - 1);
        LIBDENG_ASSERT_IN_MAIN_THREAD();
        if(window)
        {
            return (SetWindowText(window->hWnd, WIN_STRING(title))? true : false);
        }
    }
    return false;
}

const RectRaw* Sys_GetWindowGeometry(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    return &window->geometry;
}

const Point2Raw* Sys_GetWindowOrigin(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    return &window->geometry.origin;
}

const Size2Raw* Sys_GetWindowSize(uint idx)
{
    ddwindow_t* window = getWindow(idx - 1);
    if(!window) return NULL;
    return &window->geometry.size;
}

/**
 * Attempt to get the BPP (bits-per-pixel) of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param bpp           Address to write the BPP back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowBPP(uint idx, int* bpp)
{
    if(winManagerInited)
    {
        ddwindow_t* window = getWindow(idx - 1);

        if(window)
        {
            if(window->type == WT_NORMAL)
            {
                if(bpp)
                {
                    *bpp = window->normal.bpp;
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Attempt to get the fullscreen-state of the given window.
 *
 * @param idx           Index identifier (1-based) to the window.
 * @param fullscreen    Address to write the fullscreen state back to (if any).
 *
 * @return              @c true, if successful.
 */
boolean Sys_GetWindowFullscreen(uint idx, boolean *fullscreen)
{
    if(winManagerInited)
    {
        ddwindow_t         *window = getWindow(idx - 1);

        if(window && fullscreen)
        {
            *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);
        }

        return true;
    }

    return false;
}

/**
 * Attempt to get a HWND handle to the given window.
 *
 * \todo: Factor platform specific design patterns out of Doomsday.
 * We should not be passing around HWND handles...
 *
 * @param idx           Index identifier (1-based) to the window.
 *
 * @return              HWND handle if successful, ELSE @c NULL,.
 */
HWND Sys_GetWindowHandle(uint idx)
{
    if(winManagerInited)
    {
        ddwindow_t         *window = getWindow(idx - 1);

        if(window)
        {
            return window->hWnd;
        }
    }

    return NULL;
}
