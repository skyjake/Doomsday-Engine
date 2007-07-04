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
 * dd_winit.h: Win32 Initialization
 *
 * Create windows, load DLLs, setup APIs.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#define STRICT

#include <stdlib.h>
#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <string.h>
#include <stdio.h>
#include "resource.h"

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"
#include "de_ui.h"

#include "dd_winit.h"

// MACROS ------------------------------------------------------------------

#define MAINWCLASS _T("DoomsdayMainWClass")

#define WINDOWEDSTYLE (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | \
                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
#define FULLSCREENSTYLE (WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)

// TYPES -------------------------------------------------------------------

typedef struct {
    HWND            hWnd;       // Window handle.
    boolean         inited;     // True if the window has been initialized.
                                // i.e. DD_SetWindow() has been called at
                                // least once for this window.
    int             flags;
    int             x, y, width, height;
    int             bpp;
} ddwindow_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void destroyDDWindow(ddwindow_t *win);
static boolean setDDWindow(ddwindow_t *win, int newX, int newY, int newWidth,
                           int newHeight, int newBPP, uint wFlags,
                           uint uFlags);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint windowIDX = 0; // Main window.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;
static uint numWindows = 0;
static ddwindow_t **windows = NULL;

// CODE --------------------------------------------------------------------

static ddwindow_t *getWindow(uint idx)
{
    if(idx >= numWindows)
        return NULL;

    return windows[idx];
}

BOOL InitApplication(application_t *app)
{
    WNDCLASSEX  wcex;

    if(GetClassInfoEx(app->hInstance, app->className, &wcex))
        return TRUE; // Already registered a window class.

    // Initialize a window class for our window.
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = (WNDPROC) WndProc;
    wcex.hInstance = app->hInstance;
    wcex.hIcon =
        (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY),
                          IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wcex.hIconSm =
        (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY),
                          IMAGE_ICON, 16, 16, 0);
    wcex.hCursor = LoadCursor(app->hInstance, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = app->className;

    // Register our window class.
    return RegisterClassEx(&wcex);
}

static ddwindow_t *createDDWindow(application_t *app, uint parentIDX,
                                  int x, int y, int w, int h, int bpp,
                                  int flags, const char *title, int cmdShow)
{
    ddwindow_t *win, *pWin = NULL;
    HWND        phWnd = NULL;
    BOOL        ok = TRUE;

    if(!(bpp == 32 || bpp == 16))
    {
        Con_Message("DD_CreateWindow: Unsupported BPP %i.", bpp);
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
    if((win = (ddwindow_t*) calloc(1, sizeof(ddwindow_t))) == NULL)
        return 0;

    // Create the window.
    win->hWnd =
        CreateWindowEx(WS_EX_APPWINDOW, MAINWCLASS, _T(title),
                       WINDOWEDSTYLE,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       phWnd, NULL,
                       app->hInstance, NULL);
    if(!win->hWnd)
    {
        win->hWnd = NULL;
        ok = FALSE;
    }
    else // Initialize.
    {
        PIXELFORMATDESCRIPTOR pfd;
        int         pixForm;
        HDC         hDC;

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
                ok = FALSE;
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
                ok = FALSE;
            }
        }

        if(ok)
        {   // Make sure that the driver is hardware-accelerated.
            DescribePixelFormat(hDC, pixForm, sizeof(pfd), &pfd);
            if((pfd.dwFlags & PFD_GENERIC_FORMAT) && !ArgCheck("-allowsoftware"))
            {
                Sys_CriticalMessage("DD_CreateWindow: GL driver not accelerated!\n"
                                    "Use the -allowsoftware option to bypass this.");
                ok = DGL_FALSE;
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

uint DD_CreateWindow(application_t *app, uint parentIDX,
                     int x, int y, int w, int h, int bpp, int flags,
                     const char *title, int cmdShow)
{
    ddwindow_t *win =
        createDDWindow(app, parentIDX, x, y, w, h, bpp, flags, title, cmdShow);

    if(win)
    {   // Success, link it in.
        uint        i = 0;
        ddwindow_t **newList = malloc(sizeof(ddwindow_t*) * ++numWindows);

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
            free(windows); // Free windows? har, har.
        windows = newList;
    }

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
    free(window);
}

boolean DD_DestroyWindow(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    destroyDDWindow(window);
    windows[idx-1] = NULL;
    return true;
}

static boolean setDDWindow(ddwindow_t *window, int newX, int newY,
                           int newWidth, int newHeight, int newBPP,
                           uint wFlags, uint uFlags)
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
            Con_Error("DD_SetWindow: Unsupported BPP %i.", newBPP);

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
                Sys_CriticalMessage("DD_SetWindow: Resolution change failed.");
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
        AdjustWindowRect(&rect, WINDOWEDSTYLE, FALSE);
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

boolean DD_SetWindow(uint idx, int newX, int newY, int newWidth, int newHeight,
                     int newBPP, uint wFlags, uint uFlags)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(window)
        return setDDWindow(window, newX, newY, newWidth, newHeight, newBPP,
                           wFlags, uFlags);
    return false;
}

boolean DD_GetWindowDimensions(uint idx, int *x, int *y, int *width, int *height)
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

boolean DD_GetWindowBPP(uint idx, int *bpp)
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

boolean DD_GetWindowFullscreen(uint idx, boolean *fullscreen)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window || !fullscreen)
        return false;

    *fullscreen = ((window->flags & DDWF_FULLSCREEN)? true : false);

    return true;
}

HWND DD_GetWindowHandle(uint idx)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return NULL;

    return window->hWnd;
}

BOOL InitGameDLL(application_t *app)
{
    char       *dllName = NULL; // Pointer to the filename of the game DLL.

    // First we need to locate the dll name among the command line arguments.
    DD_CheckArg("-game", &dllName);

    // Was a game dll specified?
    if(!dllName)
    {
        DD_ErrorBox(true, "InitGameDLL: No game DLL was specified.\n");
        return FALSE;
    }

    // Now, load the DLL and get the API/exports.
    app->hInstGame = LoadLibrary(dllName);
    if(!app->hInstGame)
    {
        DD_ErrorBox(true, "InitGameDLL: Loading of %s failed (error %d).\n",
                    dllName, GetLastError());
        return FALSE;
    }

    // Get the function.
    app->GetGameAPI = (GETGAMEAPI) GetProcAddress(app->hInstGame, "GetGameAPI");
    if(!app->GetGameAPI)
    {
        DD_ErrorBox(true,
                    "InitGameDLL: Failed to get proc address of "
                    "GetGameAPI (error %d).\n", GetLastError());
        return FALSE;
    }

    // Do the API transfer.
    DD_InitAPI();

    // Everything seems to be working...
    return TRUE;
}

/**
 * Loads the given plugin.
 *
 * @return              <code>true</code> if the plugin was loaded succesfully.
 */
int LoadPlugin(application_t *app, const char *filename)
{
    int         i;

    // Find the first empty plugin instance.
    for(i = 0; app->hInstPlug[i]; ++i);

    // Try to load it.
    if(!(app->hInstPlug[i] = LoadLibrary(filename)))
        return FALSE;           // Failed!

    // That was all; the plugin registered itself when it was loaded.
    return TRUE;
}

/**
 * Loads all the plugins from the startup directory.
 */
int InitPlugins(application_t *app)
{
    long        hFile;
    struct _finddata_t fd;
    char        plugfn[256];

    sprintf(plugfn, "%sdp*.dll", ddBinDir.path);
    if((hFile = _findfirst(plugfn, &fd)) == -1L)
        return TRUE;

    do
        LoadPlugin(app, fd.name);
    while(!_findnext(hFile, &fd));

    return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    char        path[256];
    char        buf[256];
    BOOL        doShutdown = TRUE;
    int         exitCode = 0;

    memset(&app, 0, sizeof(app));
    app.hInstance = hInstance;
    app.className = MAINWCLASS;
    DD_MainWindowTitle(buf);

    if(!InitApplication(&app))
    {
        DD_ErrorBox(true, "Couldn't initialize application.");
    }
    else
    {
        // Initialize COM.
        CoInitialize(NULL);

        // Where are we?
        GetModuleFileName(hInstance, path, 255);
        Dir_FileDir(path, &ddBinDir);

        // Prepare the command line arguments.
        DD_InitCommandLine(GetCommandLine());

        // Load the rendering DLL.
        if(!DD_InitDGL())
        {
            DD_ErrorBox(true, "Error loading rendering library.");
        }
        // Load the game DLL.
        else if(!InitGameDLL(&app))
        {
            DD_ErrorBox(true, "Error loading game library.");
        }
        // Load all plugins that are found.
        else if(!InitPlugins(&app))
        {
            DD_ErrorBox(true, "Error loading plugins.");
        }
        // Initialize the memory zone.
        else if(!Z_Init())
        {
            DD_ErrorBox(true, "Error initializing memory zone.");
        }
        else if(0 == (windowIDX =
                DD_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, buf, nCmdShow)))
        {
            DD_ErrorBox(true, "Error creating main window.");
        }
        else
        {   // All initialization complete.
            ddwindow_t *win = getWindow(windowIDX-1);

            doShutdown = FALSE;

            // Append the main window title with the game name and ensure it
            // is the at the foreground, with focus.
            DD_MainWindowTitle(buf);
            SetWindowText(win->hWnd, _T(buf));
            SetForegroundWindow(win->hWnd);
            SetFocus(win->hWnd);
        }
    }

    if(!doShutdown)
    {
        // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }

    // Destroy all created windows.
    if(windows)
    {
        uint        i;

        for(i = 0; i < numWindows; ++i)
        {
            DD_DestroyWindow(i+1);
        }

        free(windows);
        windows = NULL;
    }

    // No more use of COM beyond this point.
    CoUninitialize();

    // Unregister our window class.
    UnregisterClass(app.className, app.hInstance);

    // Bye!
    return exitCode;
}

/**
 * All messages go to the default window message processor.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    boolean forwardMsg = true;
    LRESULT result = 0;
    static PAINTSTRUCT ps;

    switch(msg)
    {
    case WM_SIZE:
		if(!appShutdown)
        {
            switch(wParam)
            {
            case SIZE_MAXIMIZED:
                DD_SetWindow(windowIDX, 0, 0, 0, 0, 0, DDWF_FULLSCREEN,
                             DDSW_NOBPP|DDSW_NOSIZE|DDSW_NOMOVE|DDSW_NOCENTER);
                forwardMsg = false;
                break;

            default:
                break;
            }
        }
        break;

	case WM_NCLBUTTONDOWN:
		switch(wParam)
		{
		case HTMINBUTTON:
			ShowWindow(hWnd, SW_MINIMIZE);
			break;

		case HTCLOSE:
            PostQuitMessage(0);
			return 0;

        default:
            break;
		}
        // Un-acquire device when entering menu or re-sizing the mouse
        // cursor again.
        //g_bActive = FALSE;
        //SetAcquire();
        break;

    case WM_CLOSE:
        PostQuitMessage(0);
        ignoreInput = true;
        forwardMsg = false;
        break;

    case WM_PAINT:
        if(BeginPaint(hWnd, &ps))
            EndPaint(hWnd, &ps);
        forwardMsg = false;
        result = FALSE;
        break;

    case WM_HOTKEY: // A hot-key combination we have registered has been used.
        // Used to override alt+enter and other easily misshit combinations,
        // at the user's request.
        forwardMsg = false;
        break;

    case WM_SYSCOMMAND:
        switch(wParam)
        {
        case SC_SCREENSAVE: // Screensaver about to begin.
        case SC_MONITORPOWER: // Monitor trying to enter power save.
            forwardMsg = false;
            break;

        default:
            break;
        }
        break;

    case WM_ACTIVATE:
        if(!appShutdown)
		{
			if(LOWORD(wParam) == WA_ACTIVE ||
               (!HIWORD(wParam) && LOWORD(wParam) == WA_CLICKACTIVE))
			{
		        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
                DD_ClearEvents(); // For good measure.
                ignoreInput = false;
            }
			else
			{
		        SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
                ignoreInput = true;
            }
		}
        forwardMsg = false;
        break;

    default:
        break;
    }

    return (forwardMsg? DefWindowProc(hWnd, msg, wParam, lParam) : result);
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    int         i;

    // Shutdown all subsystems.
    DD_ShutdownAll();

    FreeLibrary(app.hInstGame);
    for(i = 0; app.hInstPlug[i]; ++i)
        FreeLibrary(app.hInstPlug[i]);

    app.hInstGame = NULL;
    memset(app.hInstPlug, 0, sizeof(app.hInstPlug));
}
