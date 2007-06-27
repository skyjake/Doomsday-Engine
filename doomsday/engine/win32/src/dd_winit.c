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

#include "dd_winit.h"

// MACROS ------------------------------------------------------------------

#define MAINWCLASS _T("DoomsdayMainWClass")

// TYPES -------------------------------------------------------------------

// Doomsday window flags.
#define DDWF_VISIBLE            0x01
#define DDWF_FULLSCREEN         0x02

typedef struct {
    HWND            hWnd;       // Window handle.

    int             flags;
    int             x, y, width, height;
    int             bpp;
} ddwindow_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static destroyDDWindow(ddwindow_t *win);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint            windowIDX;   // Main window.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;
static ddwindow_t *mainWindow;

// CODE --------------------------------------------------------------------

static ddwindow_t *getWindow(uint idx)
{
    if(idx != 0)
        return NULL;

    return mainWindow;
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

uint DD_CreateWindow(application_t *app, uint parentIDX,
                     int x, int y, int w, int h, int bpp, int flags,
                     const char *title, int cmdShow)
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
    if((win = (ddwindow_t*) malloc(sizeof(ddwindow_t))) == NULL)
        return 0;

    // Create the main window.
    win->hWnd =
        CreateWindowEx(WS_EX_APPWINDOW, MAINWCLASS, _T(title),
                       WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                       WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
        win->flags = flags;
        win->x = x;
        win->y = y;
        win->width = w;
        win->height = h;
        win->bpp = bpp;

        // Ensure new windows are hidden on creation.
        ShowWindow(win->hWnd, SW_HIDE);
    }

    if(!ok)
    {   // Damn, something went wrong... clean up.
        destroyDDWindow(win);
        return 0;
    }

    mainWindow = win;
    return 1;
}

static destroyDDWindow(ddwindow_t *window)
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

    if(idx == windowIDX)
        windowIDX = 0;

    return true;
}

boolean DD_SetWindowDimensions(uint idx, int x, int y, int width, int height)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    // Moving does not work in dedicated mode.
    if(isDedicated)
        return false;
/*
    if(window->flags & DDWF_FULLSCREEN)
        Con_Error("DD_ChangeWindowDimensions: Window is fullsreen; "
                  "position not changeable.");
*/
    if(x != window->x || y != window->y ||
       width != window->width || height != window->height)
    {
        window->x = x;
        window->y = y;
        window->width = width;
        window->height = height;

        SetWindowPos(window->hWnd, HWND_TOP, x, y, width, height,
                     SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
    }

    return true;
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

boolean DD_SetWindowBPP(uint idx, int bpp)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!(bpp == 32 || bpp == 16))
        Con_Error("DD_SetWindowBPP: Unsupported BPP %i.", bpp);

    if(!window)
        return false;

    if(window->bpp != bpp)
    {
        window->bpp = bpp;
    }

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

boolean DD_SetWindowFullscreen(uint idx, boolean fullscreen)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    if((window->flags & DDWF_FULLSCREEN) != fullscreen)
    {
        window->flags ^= DDWF_FULLSCREEN;
    }

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

boolean DD_SetWindowVisibility(uint idx, boolean show)
{
    ddwindow_t *window = getWindow(idx - 1);

    if(!window)
        return false;

    // Showing does not work in dedicated mode.
    if(isDedicated && show)
        return false;

    if((window->flags & DDWF_VISIBLE) != show)
    {
        window->flags ^= DDWF_VISIBLE;

        SetWindowPos(window->hWnd, HWND_TOP, 0, 0, 0, 0,
                     ((window->flags & DDWF_VISIBLE) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) |
                     SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER);

        if(window->flags & DDWF_VISIBLE)
            SetActiveWindow(window->hWnd);
    }

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
    else if(0 == (windowIDX = 
            DD_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0, buf, nCmdShow)))
    {
        DD_ErrorBox(true, "Error creating main window.");
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
        else
        {   // All initialization complete.
            ddwindow_t *win = getWindow(windowIDX-1);

            doShutdown = FALSE;

            // Append the main window title with the game name.
            DD_MainWindowTitle(buf);

            // Tell DGL of our main window.
            if(gl.SetInteger)
                gl.SetInteger(DGL_WINDOW_HANDLE, (int) win->hWnd);

            // We can now show our main window.
            DD_SetWindowVisibility(1, TRUE);
        }
    }

    if(!doShutdown)
    {
        // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }

#if _DEBUG
    {
    ddwindow_t *win = getWindow(windowIDX-1);
    if(!ReleaseDC(win->hWnd, GetDC(win->hWnd)))
    {
        DD_ErrorBox(true, "Error someone destroyed the main window before us!");
        exitCode = -1;
    }
    }
#endif  

    DD_DestroyWindow(windowIDX);

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
                GL_ChangeResolution(-1, -1, 0, true);
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
