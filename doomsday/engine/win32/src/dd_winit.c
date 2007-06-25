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

// Doomsday window flags.
#define DDWF_VISIBLE            0x01
#define DDWF_FULLSCREEN         0x02

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ddwindow_t *DD_CreateWindow(application_t *app, ddwindow_t *parent,
                           int x, int y, int w, int h, int flags,
                           const char *title, int cmdShow);
void    DD_DestroyWindow(ddwindow_t *window);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

application_t app;

// CODE --------------------------------------------------------------------

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

ddwindow_t *DD_CreateWindow(application_t *app, ddwindow_t *parent,
                            int x, int y, int w, int h, int flags,
                            const char *title, int cmdShow)
{
    ddwindow_t *win;
    BOOL        ok = TRUE;

    // Allocate a structure to wrap the various handles and state variables
    // used with this window.
    if((win = (ddwindow_t*) malloc(sizeof(ddwindow_t))) == NULL)
        return FALSE;

    // Initialize.
    win->hWnd = NULL;
    win->flags = 0;

    // Create the main window.
    win->hWnd =
        CreateWindowEx(WS_EX_APPWINDOW, MAINWCLASS, _T(title),
                       WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                       WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       (parent? parent->hWnd : NULL), NULL,
                       app->hInstance, NULL);
    if(!win->hWnd)
    {
        win->hWnd = NULL;
        ok = FALSE;
    }
    else // Initialize.
    {
        // Ensure new windows are hidden on creation.
        ShowWindow(win->hWnd, SW_HIDE);
    }

    if(!ok)
    {   // Damn, something went wrong... clean up.
        DD_DestroyWindow(win);
        return NULL;
    }
    return win;
}

void DD_DestroyWindow(ddwindow_t *window)
{
    if(!window)
        return;

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

ddwindow_t *DD_GetWindow(uint idx)
{
    if(idx == 0)
        return app.window;

    return NULL;
}

void DD_WindowShow(ddwindow_t *window, boolean show)
{
    if(!window)
        return;

    // Showing does not work in dedicated mode.
    if(isDedicated && show)
        return;

    if(show)
        window->flags |= DDWF_VISIBLE;
    else
        window->flags &= ~DDWF_VISIBLE;

#ifdef WIN32
    SetWindowPos(window->hWnd, HWND_TOP, 0, 0, 0, 0,
                 ((window->flags & DDWF_VISIBLE) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW) |
                 SWP_NOSIZE | SWP_NOMOVE);
    if(window->flags & DDWF_VISIBLE)
        SetActiveWindow(window->hWnd);
#endif
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
    else if(NULL == (app.window = 
            DD_CreateWindow(&app, NULL, 0, 0, 640, 480, 0, buf, nCmdShow)))
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
            doShutdown = FALSE;

            // Append the main window title with the game name.
            DD_MainWindowTitle(buf);

            // Tell DGL of our main window.
            if(gl.SetInteger)
                gl.SetInteger(DGL_WINDOW_HANDLE, (int) app.window->hWnd);

            // We can now show our main window.
            DD_WindowShow(app.window, TRUE);
        }
    }

    if(!doShutdown)
    {
        // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }

#if _DEBUG
    if(!ReleaseDC(app.window->hWnd, GetDC(app.window->hWnd)))
    {
        DD_ErrorBox(true, "Error someone destroyed the main window before us!");
        exitCode = -1;
    }
#endif  

    DD_DestroyWindow(app.window);

    // No more use of COM beyond this point.
    CoUninitialize();

    // Unregister our window class.
    if(!UnregisterClass(app.className, app.hInstance))
    {
        DD_ErrorBox(true, "Failed unregistering window class.");
        exitCode = -1;
    }

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
