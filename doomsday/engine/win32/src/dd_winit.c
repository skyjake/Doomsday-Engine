/**\file dd_winit.c
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
 * Engine Initialization - Windows.
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
#include <tchar.h>

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

#include "fs_util.h"
#include "dd_winit.h"
#include "displaymode.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

//LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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

/**
 * \note GetLastError() should only be called when we *know* an error was thrown.
 * The result of calling this any other time is undefined.
 *
 * @return              Ptr to a string containing a textual representation of
 *                      the last error thrown in the current thread else @c NULL.
 */
const char* DD_Win32_GetLastErrorMessage(void)
{
    static char* buffer = 0; /// @todo Never free'd!
    static size_t currentBufferSize = 0;

    LPVOID lpMsgBuf;
    DWORD dw = GetLastError(), lpMsgBufLen;
    lpMsgBufLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 0, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&lpMsgBuf, 0, 0);
    if(!lpMsgBuf || lpMsgBufLen == 0)
        return "";

    if(!buffer || (size_t)(lpMsgBufLen+1+8) > currentBufferSize)
    {
        currentBufferSize = (size_t)(lpMsgBufLen+1+8);
        buffer = M_Realloc(buffer, currentBufferSize);
    }

    dd_snprintf(buffer, currentBufferSize, "#%-5d: ", (int)dw);

    // Continue splitting as long as there are parts.
    { char* part, *cursor = (char*)lpMsgBuf;
    while(*(part = M_StrTok(&cursor, "\n")))
        strcat(buffer, part);
    }

    // We're done with the system-allocated message.
    LocalFree(lpMsgBuf);
    return buffer;
}

static HINSTANCE* findFirstUnusedPluginHandle(application_t* app)
{
    int i;
    assert(app);

    for(i = 0; i < MAX_PLUGS; ++i)
    {
        if(!app->hInstPlug[i])
            return &app->hInstPlug[i];
    }
    return 0;
}

static int loadPlugin(application_t* app, const char* pluginPath, void* paramaters)
{
    HINSTANCE plugin, *handle;
    void (*initializer)(void);
    filename_t name;
    assert(app && pluginPath && pluginPath[0]);

/*#if _DEBUG
    Con_Printf("Attempting to load \"%s\" as a plugin...\n", pluginPath);
#endif*/

    plugin = LoadLibrary(WIN_STRING(pluginPath));
    if(!plugin)
    {
        Con_Printf("loadPlugin: Error loading \"%s\" (%s).\n", pluginPath, DD_Win32_GetLastErrorMessage());
        return 0; // Continue iteration.
    }

    initializer = (void*)GetProcAddress(plugin, _T("DP_Initialize"));
    if(!initializer)
    {
        // Clearly not a Doomsday plugin.
#if _DEBUG
        Con_Printf("loadPlugin: \"%s\" does not export entrypoint DP_Initialize, ignoring.\n", pluginPath);
#endif
        FreeLibrary(plugin);
        return 0; // Continue iteration.
    }

    handle = findFirstUnusedPluginHandle(app);
    if(!handle)
    {
#if _DEBUG
        Con_Printf("loadPlugin: Failed acquiring new handle for \"%s\", ignoring.\n", pluginPath);
#endif
        FreeLibrary(plugin);
        return 0; // Continue iteration.
    }

    // This seems to be a Doomsday plugin.
    _splitpath(pluginPath, NULL, NULL, name, NULL);
    Con_Printf("  %s\n", name);

    *handle = plugin;
    initializer();

    return 0; // Continue iteration.
}

static BOOL unloadPlugin(HINSTANCE* handle)
{
    BOOL result;
    assert(handle);

    result = FreeLibrary(*handle);
    *handle = 0;
    if(!result)
        Con_Printf("unloadPlugin: Error unloading plugin (%s).\n", DD_Win32_GetLastErrorMessage());
    return result;
}

/**
 * Loads all the plugins from the library directory.
 */
static BOOL loadAllPlugins(application_t* app)
{
    ddstring_t searchPattern, absolutePath;
    struct _finddata_t fd;
    long hFile;
    assert(app);

    Con_Printf("Initializing plugins...\n");

    Str_Init(&absolutePath);
    Str_Init(&searchPattern);
    Str_Appendf(&searchPattern, "%sj*.dll", ddBinPath);
    if(-1L != (hFile = _findfirst(Str_Text(&searchPattern), &fd)))
    {
        do
        {
            Str_Clear(&absolutePath);
            Str_Appendf(&absolutePath, "%s%s", ddBinPath, fd.name);
            loadPlugin(app, Str_Text(&absolutePath), NULL/*no paramaters*/);
        } while(!_findnext(hFile, &fd));
    }

    Str_Clear(&searchPattern);
    Str_Appendf(&searchPattern, "%sdp*.dll", ddBinPath);
    if(-1L != (hFile = _findfirst(Str_Text(&searchPattern), &fd)))
    {
        do
        {
            Str_Clear(&absolutePath);
            Str_Appendf(&absolutePath, "%s%s", ddBinPath, fd.name);
            loadPlugin(app, Str_Text(&absolutePath), NULL/*no paramaters*/);
        } while(!_findnext(hFile, &fd));
    }

    Str_Free(&searchPattern);
    Str_Free(&absolutePath);
    return TRUE;
}

static BOOL unloadAllPlugins(application_t* app)
{
    int i;
    assert(app);

    // Remove all entries; some may have been created by the plugins.
    LogBuffer_Clear();

    for(i = 0; i < MAX_PLUGS && app->hInstPlug[i]; ++i)
    {
        unloadPlugin(&app->hInstPlug[i]);
    }
    return TRUE;
}

static BOOL initDGL(void)
{
    return (BOOL) Sys_GLPreInit();
}

static BOOL initApplication(application_t* app)
{
#if 0
    WNDCLASSEX wcex;
    assert(app);

    if(GetClassInfoEx(app->hInstance, app->className, &wcex))
        return TRUE; // Already registered a window class.

    // Initialize a window class for our window.
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = (WNDPROC) WndProc;
    wcex.hInstance = app->hInstance;
    wcex.hIcon   = (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    wcex.hIconSm = (HICON) LoadImage(app->hInstance, MAKEINTRESOURCE(IDI_DOOMSDAY_ICON), IMAGE_ICON, 16, 16, 0);
    wcex.hCursor = LoadCursor(app->hInstance, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wcex.lpszClassName = app->className;
    wcex.lpszMenuName = 0;

    // Register our window class.
    return RegisterClassEx(&wcex);
#endif
    return TRUE;
}

static void determineGlobalPaths(application_t* app)
{
    assert(app);

    // Where are we?
#if defined(DENG_LIBRARY_DIR)
#  if !defined(_DEBUG)
#pragma message("!!!WARNING: DENG_LIBRARY_DIR defined in non-debug build!!!")
#  endif
    {
    filename_t path;
    directory_t* temp;

    dd_snprintf(path, FILENAME_T_MAXLEN, "%s", DENG_LIBRARY_DIR);
    // Ensure it ends with a directory separator.
    F_AppendMissingSlashCString(path, FILENAME_T_MAXLEN);
    Dir_MakeAbsolutePath(path);
    temp = Dir_ConstructFromPathDir(path);
    strncpy(ddBinPath, Str_Text(temp), FILENAME_T_MAXLEN);
    Dir_Delete(temp);
    }
#else
    {
        directory_t* temp;
#ifdef UNICODE
        wchar_t path[FILENAME_T_MAXLEN];
        GetModuleFileName(app->hInstance, path, FILENAME_T_MAXLEN);
        temp = Dir_ConstructFromPathDir(ToAnsiString(path));
#else
        filename_t path;
        GetModuleFileName(app->hInstance, path, FILENAME_T_MAXLEN);
        temp = Dir_ConstructFromPathDir(path);
#endif
        strncpy(ddBinPath, Dir_Path(temp), FILENAME_T_MAXLEN);
        Dir_Delete(temp);
    }
#endif

    // The -userdir option sets the working directory.
    if(CommandLine_CheckWith("-userdir", 1))
    {
        filename_t runtimePath;
        directory_t* temp;

        strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
        Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
        // Ensure the path is closed with a directory separator.
        F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

        temp = Dir_New(runtimePath);
        app->usingUserDir = Dir_SetCurrent(Dir_Path(temp));
        if(app->usingUserDir)
        {
            strncpy(ddRuntimePath, Dir_Path(temp), FILENAME_T_MAXLEN);
        }
        Dir_Delete(temp);
    }

    if(!app->usingUserDir)
    {
        // The current working directory is the runtime dir.
        directory_t* temp = Dir_NewFromCWD();
        Dir_SetCurrent(Dir_Path(temp));
        strncpy(ddRuntimePath, Dir_Path(temp), FILENAME_T_MAXLEN);
        Dir_Delete(temp);
    }

    if(CommandLine_CheckWith("-basedir", 1))
    {
        strncpy(ddBasePath, CommandLine_Next(), FILENAME_T_MAXLEN);
    }
    else
    {
        // The standard base directory is one level up from the bin dir.
        dd_snprintf(ddBasePath, FILENAME_T_MAXLEN, "%s../", ddBinPath);
    }
    Dir_CleanPath(ddBasePath, FILENAME_T_MAXLEN);
    Dir_MakeAbsolutePath(ddBasePath, FILENAME_T_MAXLEN);
    // Ensure it ends with a directory separator.
    F_AppendMissingSlashCString(ddBasePath, FILENAME_T_MAXLEN);
}

/*
static BOOL createMainWindow(int lnCmdShow)
{
    char buf[256];
    Point2Raw origin = { 0, 0 };
    Size2Raw size = { 640, 480 };
    DD_ComposeMainWindowTitle(buf);
    mainWindowIdx = Window_Create(&app, &origin, &size, 32, 0,
                                  (isDedicated ? WT_CONSOLE : WT_NORMAL), buf, &lnCmdShow);
    return mainWindowIdx != 0;
}
*/

boolean DD_Win32_Init(void)
{
    BOOL failed = TRUE;

    memset(&app, 0, sizeof(app));
    app.hInstance = GetModuleHandle(NULL);
#if 0
    app.className = TEXT(MAINWCLASS);
#endif

    {
        // Initialize COM.
        CoInitialize(NULL);

        // Prepare the command line arguments.
        DD_InitCommandLine();

        // First order of business: are we running in dedicated mode?
        isDedicated = CommandLine_Check("-dedicated");
        novideo = CommandLine_Check("-novideo") || isDedicated;

        Library_Init();

        // Determine our basedir and other global paths.
        determineGlobalPaths(&app);

        if(!DD_EarlyInit())
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
        }
        else if(!initDGL())
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing DGL.", 0);
        }
        else if(!loadAllPlugins(&app))
        {
            Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error loading plugins.", 0);
        }
        else
        {   // All initialization complete.
            failed = FALSE;
        }
    }

    // No Windows system keys?
    if(CommandLine_Check("-nowsk"))
    {
        // Disable Alt-Tab, Alt-Esc, Ctrl-Alt-Del.  A bit of a hack...
        SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, TRUE, 0, 0);
        Con_Message("Windows system keys disabled.\n");
    }

    return !failed;
}

#if 0
/**
 * All messages go to the default window message processor.
 */
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static PAINTSTRUCT ps;
    BOOL forwardMsg = true;
    LRESULT result = 0;

    switch(msg)
    {
    case WM_SIZE:
        if(!Sys_IsShuttingDown())
        {
            switch(wParam)
            {
            case SIZE_MAXIMIZED:
                Sys_SetWindow(mainWindowIdx, 0, 0, 0, 0, 0, DDWF_FULLSCREEN, DDSW_NOBPP|DDSW_NOSIZE|DDSW_NOMOVE|DDSW_NOCENTER);
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
        DD_IgnoreInput(true);
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

    case WM_HOTKEY:
        // A hot-key combination we have registered has been used.
        // Used to override alt+return and other easily miss-hit combinations,
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
        // Do not alter high-level engine modes state/properties while busy.
        /// @todo The window manager should not have the authority to make such changes.
        ///       We should simply flag the desire to enter a "suspended mode" which
        ///       will be actioned by the core loop as necessary.
        if(!Sys_IsShuttingDown() && !BusyMode_Active())
        {
            if(LOWORD(wParam) == WA_ACTIVE || (!HIWORD(wParam) && LOWORD(wParam) == WA_CLICKACTIVE))
            {
                SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
                DD_ClearEvents(); // For good measure.
                DD_IgnoreInput(false);
            }
            else
            {
                SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
                DD_IgnoreInput(true);
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

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    DD_ShutdownAll(); // Stop all engine subsystems.
    unloadAllPlugins(&app);
    Library_Shutdown();

#ifdef UNICODE
    free(convBuf); convBuf = 0;
    free(utf8ConvBuf); utf8ConvBuf = 0;
#endif

    // No more use of COM beyond, this point.
    CoUninitialize();

#if 0
    // Unregister our window class.
    UnregisterClass(app.className, app.hInstance);
#endif

    DisplayMode_Shutdown();
}

/**
 * Windows implementation for the *nix strcasestr() function.
 */
const char* strcasestr(const char *text, const char *sub)
{
    int textLen = strlen(text);
    int subLen = strlen(sub);
    int i;

    for(i = 0; i <= textLen - subLen; ++i)
    {
        const char* start = text + i;
        if(!strnicmp(start, sub, subLen)) return start;
    }
    return 0;
}
