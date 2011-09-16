/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

uint windowIDX = 0; // Main window.
application_t app;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#ifdef UNICODE
static LPWSTR convBuf;
static LPSTR utf8ConvBuf;
#endif

// CODE --------------------------------------------------------------------

#ifdef UNICODE
LPCWSTR ToWideString(const char* str)
{
    // Determine the length of the output string.
    int wideChars = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);

    // Allocate the right amount of memory.
    int bufSize = wideChars * sizeof(wchar_t) + 1;
    convBuf = realloc(convBuf, bufSize);
    memset(convBuf, 0, bufSize);

    MultiByteToWideChar(CP_ACP, 0, str, -1, convBuf, wideChars);

    return convBuf;
}

LPCSTR ToAnsiString(const wchar_t* wstr)
{
    // Determine how much memory is needed for the output string.
    int utfBytes = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, 0, 0, 0, 0);

    // Allocate the right amount of memory.
    utf8ConvBuf = realloc(utf8ConvBuf, utfBytes);

    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8ConvBuf, utfBytes, 0, 0);

    return utf8ConvBuf;
}
#endif

BOOL InitApplication(application_t *app)
{
    WNDCLASSEX wcex;

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
}

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
#ifdef UNICODE
        wchar_t path[256];
        GetModuleFileName(app->hInstance, path, 255);
        Dir_FileDir(ToAnsiString(path), &ddBinDir);
#else
        char path[256];
        GetModuleFileName(app->hInstance, path, 255);
        Dir_FileDir(path, &ddBinDir);
#endif
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

static boolean loadGamePlugin(application_t *app, const char *libPath)
{
    if(!libPath || !app)
        return false;

    // Now, load the library and get the API/exports.
    app->hInstGame = LoadLibrary(WIN_STRING(libPath));
    if(!app->hInstGame)
    {
        DD_ErrorBox(true, "loadGamePlugin: Loading of %s failed (error %i).\n",
                    libPath, (int) GetLastError());
        return false;
    }

    // Get the function.
    app->GetGameAPI = (GETGAMEAPI) GetProcAddress(app->hInstGame, "GetGameAPI");
    if(!app->GetGameAPI)
    {
        DD_ErrorBox(true, "loadGamePlugin: Failed to get address of "
                          "GetGameAPI (error %i).\n", (int) GetLastError());
        return false;
    }

    // Do the API transfer.
    DD_InitAPI();

    // Everything seems to be working...
    return true;
}

/**
 * Loads the given plugin.
 *
 * @return              @c true, if the plugin was loaded succesfully.
 */
static int loadPlugin(application_t *app, const char *filename)
{
    int                 i;

    // Find the first empty plugin instance.
    for(i = 0; app->hInstPlug[i]; ++i);

    // Try to load it.
    if(!(app->hInstPlug[i] = LoadLibrary(WIN_STRING(filename))))
        return FALSE;           // Failed!

    // That was all; the plugin registered itself when it was loaded.
    return TRUE;
}

/**
 * Loads all the plugins from the startup directory.
 */
static int loadAllPlugins(application_t *app)
{
    long                hFile;
    struct _finddata_t  fd;
    char                plugfn[256];

    sprintf(plugfn, "%sdp*.dll", ddBinDir.path);
    if((hFile = _findfirst(plugfn, &fd)) == -1L)
        return TRUE;

    do
        loadPlugin(app, fd.name);
    while(!_findnext(hFile, &fd));

    return TRUE;
}

static int initTimingSystem(void)
{
    // Nothing to do.
    return TRUE;
}

static int initPluginSystem(void)
{
    // Nothing to do.
    return TRUE;
}

static int initDGL(void)
{
    return Sys_PreInitGL();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    BOOL                doShutdown = TRUE;
    int                 exitCode = 0;
    int                 lnCmdShow = nCmdShow;

    memset(&app, 0, sizeof(app));
    app.hInstance = hInstance;
    app.className = MAINWCLASS;
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

        // Prepare the command line arguments.
        DD_InitCommandLine(UTF_STRING(GetCommandLine()));

        // First order of business: are we running in dedicated mode?
        if(ArgCheck("-dedicated"))
            isDedicated = true;
        novideo = ArgCheck("-novideo") || isDedicated;
        
        DD_ComposeMainWindowTitle(buf);

        // First we need to locate the game lib name among the command line
        // arguments.
        DD_CheckArg("-game", &libName);

        // Was a game library specified?
        if(!libName)
        {
            DD_ErrorBox(true, "loadGamePlugin: No game library was specified.\n");
        }
        else
        {
            char                libPath[256];

            // Determine our basedir and other global paths.
            determineGlobalPaths(&app);

            // Compose the full path to the game library.
            _snprintf(libPath, 255, "%s%s", ddBinDir.path, libName);

            if(!DD_EarlyInit())
            {
                DD_ErrorBox(true, "Error during early init.");
            }
            else if(!initTimingSystem())
            {
                DD_ErrorBox(true, "Error initalizing timing system.");
            }
            else if(!initPluginSystem())
            {
                DD_ErrorBox(true, "Error initializing plugin system.");
            }
            else if(!initDGL())
            {
                DD_ErrorBox(true, "Error initializing DGL.");
            }
            // Load the game plugin.
            else if(!loadGamePlugin(&app, libPath))
            {
                DD_ErrorBox(true, "Error loading game library.");
            }
            // Load all other plugins that are found.
            else if(!loadAllPlugins(&app))
            {
                DD_ErrorBox(true, "Error loading plugins.");
            }
            // Initialize the memory zone.
            else if(!Z_Init())
            {
                DD_ErrorBox(true, "Error initializing memory zone.");
            }
            else
            {
                if(0 == (windowIDX =
                    Sys_CreateWindow(&app, 0, 0, 0, 640, 480, 32, 0,
                                     (isDedicated ? WT_CONSOLE : WT_NORMAL),
                                     buf, &lnCmdShow)))
                {
                    DD_ErrorBox(true, "Error creating main window.");
                }
                else if(!Sys_InitGL())
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
    DD_Shutdown();

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
    BOOL        forwardMsg = true;
    LRESULT     result = 0;
    static PAINTSTRUCT ps;

    switch(msg)
    {
    case WM_SIZE:
        if(!appShutdown)
        {
            switch(wParam)
            {
            case SIZE_MAXIMIZED:
                Sys_SetWindow(windowIDX, 0, 0, 0, 0, 0, DDWF_FULLSCREEN,
                             DDSW_NOBPP|DDSW_NOSIZE|DDSW_NOMOVE|DDSW_NOCENTER);
                forwardMsg = FALSE;
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
        if(!appShutdown)
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

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    int         i;

#ifdef UNICODE
    free(convBuf); convBuf = 0;
    free(utf8ConvBuf); utf8ConvBuf = 0;
#endif

    // Shutdown all subsystems.
    DD_ShutdownAll();

    FreeLibrary(app.hInstGame);
    for(i = 0; app.hInstPlug[i]; ++i)
        FreeLibrary(app.hInstPlug[i]);

    app.hInstGame = NULL;
    memset(app.hInstPlug, 0, sizeof(app.hInstPlug));
}
