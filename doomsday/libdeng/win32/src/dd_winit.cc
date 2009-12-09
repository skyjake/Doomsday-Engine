/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_winit.h: Win32 Initialization
 *
 * Create windows, load DLLs, setup APIs.
 */

// HEADER FILES ------------------------------------------------------------

#include <de/App>
#include <de/Library>
#include <de/Vector>
#include <de/Rectangle>
#include <de/Log>

using namespace de;

#define WIN32_LEAN_AND_MEAN
#define _WIN32_DCOM
#define STRICT

#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource.h"

#include "de_base.h"
#include "de_graphics.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"
#include "de_ui.h"

#include "dd_winit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

//LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint windowIDX = 0; // Main window.
application_t app;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

BOOL InitApplication(application_t *app)
{
#if 0
    WNDCLASSEX          wcex;

    if(GetClassInfoEx(app->hInstance, app->className, &wcex))
        return TRUE; // Already registered a window class.
    // Initialize a window class for our window.
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = (WNDPROC) WndProc;
    wcex.hInstance = app->hInstance;
    wcex.hIcon =
        (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY_ICON),
                          IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wcex.hIconSm =
        (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY_ICON),
                          IMAGE_ICON, 16, 16, 0);
    wcex.hCursor = LoadCursor(app->hInstance, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = app->className;
    wcex.lpszMenuName = NULL;

    // Register our window class.
    return RegisterClassEx(&wcex);
#endif
    return TRUE;
}

#if 0
/**
 * All messages go to the default window message processor.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL        forwardMsg = true;
    LRESULT     result = 0;
    static PAINTSTRUCT ps;

    switch(msg)
    {
    case WM_SIZE:
        //if(!appShutdown)
        {
            switch(wParam)
            {
            case SIZE_MAXIMIZED:
                /*
                Sys_SetWindow(windowIDX, 0, 0, 0, 0, 0, DDWF_FULLSCREEN,
                             DDSW_NOBPP|DDSW_NOSIZE|DDSW_NOMOVE|DDSW_NOCENTER);
                forwardMsg = FALSE;
                */
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
        ignoreInput = TRUE;
        forwardMsg = FALSE;
        break;

    case WM_PAINT:
        if(BeginPaint(hWnd, &ps))
            EndPaint(hWnd, &ps);
        forwardMsg = FALSE;
        break;

    case WM_KEYDOWN:
        forwardMsg = FALSE;
        break;

    case WM_KEYUP:
        forwardMsg = FALSE;
        break;

    case WM_SYSKEYDOWN:
        forwardMsg = TRUE;
        break;

    case WM_SYSKEYUP:
        forwardMsg = TRUE;
        break;

    case WM_HOTKEY: // A hot-key combination we have registered has been used.
        // Used to override alt+return and other easily misshit combinations,
        // at the user's request.
        forwardMsg = FALSE;
        break;

    case WM_SYSCOMMAND:
        switch(wParam)
        {
        case SC_SCREENSAVE: // Screensaver about to begin.
        case SC_MONITORPOWER: // Monitor trying to enter power save.
            forwardMsg = FALSE;
            break;

        default:
            break;
        }
        break;

    case WM_ACTIVATE:
        //if(!appShutdown)
        {
            if(LOWORD(wParam) == WA_ACTIVE ||
               (!HIWORD(wParam) && LOWORD(wParam) == WA_CLICKACTIVE))
            {
                SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
                DD_ClearEvents(); // For good measure.
                ignoreInput = FALSE;
            }
            else
            {
                SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
                ignoreInput = TRUE;
            }
        }
        forwardMsg = FALSE;
        break;

    default:
        break;
    }

    if(forwardMsg)
        return DefWindowProc(hWnd, msg, wParam, lParam);

    return result;
}
#endif

static void determineGlobalPaths(application_t *app)
{
    if(!app)
        return;

    // Where are we?
#if defined(DENG_LIBRARY_DIR)
# if !defined(_DEBUG)
#pragma message("!!!WARNING: DENG_LIBRARY_DIR defined in non-debug build!!!")
# endif
#endif

#if defined(DENG_LIBRARY_DIR)
    _snprintf(ddBinDir.path, 254, "%s", DENG_LIBRARY_DIR);
    if(ddBinDir.path[strlen(ddBinDir.path)] != '\\')
        sprintf(ddBinDir.path, "%s\\", ddBinDir.path);
    Dir_MakeAbsolute(ddBinDir.path);
    ddBinDir.drive = toupper(ddBinDir.path[0]) - 'A' + 1;
#else
    {
    char                path[256];
    GetModuleFileName(app->hInstance, path, 255);
    Dir_FileDir(path, &ddBinDir);
    }
#endif

    // The -userdir option sets the working directory.
    if(ArgCheckWith("-userdir", 1))
    {
        Dir_MakeDir(ArgNext(), &ddRuntimeDir);
        app->userDirOk = Dir_ChDir(&ddRuntimeDir);
    }

    // The current working directory is the runtime dir.
    Dir_GetDir(&ddRuntimeDir);

    // The standard base directory is two levels upwards.
    if(ArgCheck("-stdbasedir"))
    {
        strncpy(ddBasePath, "..\\..\\", FILENAME_T_MAXLEN);
    }

    if(ArgCheckWith("-basedir", 1))
    {
        strncpy(ddBasePath, ArgNext(), FILENAME_T_MAXLEN);
        Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
    }

    Dir_MakeAbsolute(ddBasePath, FILENAME_T_MAXLEN);
    Dir_ValidDir(ddBasePath, FILENAME_T_MAXLEN);
}

static boolean loadGamePlugin(application_t *app)
{
    // Get the function.
    app->GetGameAPI = reinterpret_cast<GETGAMEAPI>(App::game().address("GetGameAPI"));

    // Do the API transfer.
    DD_InitAPI();

    // Everything seems to be working...
    return true;
}

static int initTimingSystem(void)
{
    // Nothing to do.
    return TRUE;
}

static int initDGL(void)
{
    return Sys_PreInitGL();
}

int DD_Entry(int argc, char* argv[])
{
    BOOL                doShutdown = TRUE;
    int                 exitCode = 0;
    int                 lnCmdShow = SW_SHOW;

    DD_InitCommandLineAliases();

    memset(&app, 0, sizeof(app));
    app.hInstance = GetModuleHandle(NULL);
    //app.className = TEXT(MAINWCLASS);
    app.userDirOk = true;

    if(!InitApplication(&app))
    {
        DD_ErrorBox(true, "Couldn't initialize application.");
    }
    else
    {
        char                buf[256];
        const char*         libName = NULL;

        // Initialize COM.
        CoInitialize(NULL);

        // First order of business: are we running in dedicated mode?
        if(ArgCheck("-dedicated"))
            isDedicated = true;
        novideo = ArgCheck("-novideo") || isDedicated;

        DD_ComposeMainWindowTitle(buf);

        // Was a game library specified?
        if(!App::app().hasGame())
        {
            DD_ErrorBox(true, "loadGamePlugin: No game library was specified.\n");
        }
        else
        {
            // Determine our basedir and other global paths.
            determineGlobalPaths(&app);

            if(!DD_EarlyInit())
            {
                DD_ErrorBox(true, "Error during early init.");
            }
            else if(!initTimingSystem())
            {
                DD_ErrorBox(true, "Error initalizing timing system.");
            }
            else if(!initDGL())
            {
                DD_ErrorBox(true, "Error initializing DGL.");
            }
            // Load the game plugin.
            else if(!loadGamePlugin(&app))
            {
                DD_ErrorBox(true, "Error loading game library.");
            }
            else if(isDedicated)
            {
                // We're done.
                doShutdown = FALSE;
            }
            else 
            {
                /*
                if(0 == (windowIDX =
                    Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0,
                                     (isDedicated ? WT_CONSOLE : WT_NORMAL),
                                     buf, &lnCmdShow)))
                {
                    DD_ErrorBox(true, "Error creating main window.");
                }
                else */
                if(!Sys_InitGL())
                {
                    DD_ErrorBox(true, "Error initializing OpenGL.");
                }
                else
                {   // All initialization complete.
                    doShutdown = FALSE;

                    // Append the main window title with the game name and ensure it
                    // is the at the foreground, with focus.
                    DD_ComposeMainWindowTitle(buf);
                    Sys_SetWindowTitle(windowIDX, buf);

                   // SetForegroundWindow(win->hWnd);
                   // SetFocus(win->hWnd);
                }
            }
        }
    }

    if(!doShutdown)
    {   // Fire up the engine. The game loop will also act as the message pump.
        exitCode = DD_Main();
    }
    return exitCode;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    Demo_StopPlayback();
    Con_SaveDefaults();
    Sys_Shutdown();
    B_Shutdown();

    // Shutdown all subsystems.
    DD_ShutdownAll();

    // No more use of COM beyond this point.
    CoUninitialize();

    // Unregister our window class.
#if 0
    UnregisterClass(app.className, app.hInstance);
#endif

    // Bye!
}
