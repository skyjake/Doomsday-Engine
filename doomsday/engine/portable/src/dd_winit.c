/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_winit.h: Win32 Initialization
 *
 * Create windows, load DLLs, setup APIs.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <windows.h>
#include "resource.h"

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "dd_pinit.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam,
                             LPARAM lParam);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HWND    hWndMain;               // The window handle to the main window.
HINSTANCE hInstApp;             // Instance handle to the application.
HINSTANCE hInstGame;            // Instance handle to the game DLL.
HINSTANCE hInstPlug[MAX_PLUGS]; // Instances to plugin DLLs.
GETGAMEAPI GetGameAPI;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

BOOL InitApplication(HINSTANCE hInst)
{
    WNDCLASS wc;

    // We need to register a window class for our window.
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_DOOMSDAY));
    wc.hCursor = NULL;          //LoadCursor(hInst, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_ACTIVEBORDER + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "DoomsdayMainWClass";
    return RegisterClass(&wc);
}

BOOL InitInstance(HINSTANCE hInst, int cmdShow)
{
    HDC     hdc;
    char    buf[256];

    DD_MainWindowTitle(buf);

    // Create the main window. It's shown right before GL init.
    hWndMain =
        CreateWindow("DoomsdayMainWClass", buf,
                     WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                     WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL);
    if(!hWndMain)
        return FALSE;

    // Set the font.
    hdc = GetDC(hWndMain);
    SetMapMode(hdc, MM_TEXT);
    SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

    // Tell DGL of our main window.
    if(gl.SetInteger)
        gl.SetInteger(DGL_WINDOW_HANDLE, (int) hWndMain);
    return TRUE;
}

BOOL InitGameDLL(void)
{
    char   *dllName = NULL;     // Pointer to the filename of the game DLL.

    // First we need to locate the dll name among the command line arguments.
    DD_CheckArg("-game", &dllName);

    // Was a game dll specified?
    if(!dllName)
    {
        DD_ErrorBox(true, "InitGameDLL: No game DLL was specified.\n");
        return FALSE;
    }

    // Now, load the DLL and get the API/exports.
    hInstGame = LoadLibrary(dllName);
    if(!hInstGame)
    {
        DD_ErrorBox(true, "InitGameDLL: Loading of %s failed (error %d).\n",
                    dllName, GetLastError());
        return FALSE;
    }

    // Get the function.
    GetGameAPI = (GETGAMEAPI) GetProcAddress(hInstGame, "GetGameAPI");
    if(!GetGameAPI)
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

//===========================================================================
// LoadPlugin
//  Loads the given plugin. Returns TRUE iff the plugin was loaded
//  succesfully.
//===========================================================================
int LoadPlugin(const char *filename)
{
    int     i;

    // Find the first empty plugin instance.
    for(i = 0; hInstPlug[i]; i++);

    // Try to load it.
    if(!(hInstPlug[i] = LoadLibrary(filename)))
        return FALSE;           // Failed!

    // That was all; the plugin registered itself when it was loaded.
    return TRUE;
}

//===========================================================================
// InitPlugins
//  Loads all the plugins from the startup directory.
//===========================================================================
int InitPlugins(void)
{
    long    hFile;
    struct _finddata_t fd;
    char    plugfn[256];

    sprintf(plugfn, "%sdp*.dll", ddBinDir.path);
    if((hFile = _findfirst(plugfn, &fd)) == -1L)
        return TRUE;
    do
        LoadPlugin(fd.name);
    while(!_findnext(hFile, &fd));
    return TRUE;
}

//===========================================================================
// WinMain
//===========================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    char    path[256];

    // Where are we?
    GetModuleFileName(hInstance, path, 255);
    Dir_FileDir(path, &ddBinDir);

    // Make the instance handle global knowledge.
    hInstApp = hInstance;

    // Prepare the command line arguments.
    DD_InitCommandLine(GetCommandLine());

    // Load the rendering DLL.
    if(!DD_InitDGL())
        return FALSE;

    // Load the game DLL.
    if(!InitGameDLL())
        return FALSE;

    // Load all plugins that are found.
    if(!InitPlugins())
        return FALSE;           // Fatal error occured?

    if(!InitApplication(hInstance))
    {
        DD_ErrorBox(true, "Couldn't initialize application.");
        return FALSE;
    }
    if(!InitInstance(hInstance, nCmdShow))
    {
        DD_ErrorBox(true, "Couldn't initialize instance.");
        return FALSE;
    }

    // Initialize the memory zone.
    Z_Init();

    // Fire up the engine. The game loop will also act as the message pump.
    DD_Main();
    return 0;
}

//===========================================================================
// MainWndProc
//  All messages go to the default window message processor.
//===========================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        // FIXME: Allow closing via the close button.
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

//===========================================================================
// DD_Shutdown
//  Shuts down the engine.
//===========================================================================
void DD_Shutdown(void)
{
    int     i;

    // Shutdown all subsystems.
    DD_ShutdownAll();

    FreeLibrary(hInstGame);
    for(i = 0; hInstPlug[i]; i++)
        FreeLibrary(hInstPlug[i]);
    hInstGame = NULL;
    memset(hInstPlug, 0, sizeof(hInstPlug));
}
