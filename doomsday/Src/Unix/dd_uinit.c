
//**************************************************************************
//**
//** DD_UINIT.C
//**
//** Unix init.
//** Load libraries and set up APIs.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <SDL.h>

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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

GETGAMEAPI GetGameAPI;

/*
HWND hWndMain;			// The window handle to the main window.
HINSTANCE hInstApp;		// Instance handle to the application.
HINSTANCE hInstGame;	// Instance handle to the game DLL.
HINSTANCE hInstPlug[MAX_PLUGS];	// Instances to plugin DLLs.
*/

void *hGame;
void *hPlugin[MAX_PLUGS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
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
	wc.hCursor = NULL; 
	wc.hbrBackground = (HBRUSH) (COLOR_ACTIVEBORDER+1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "DoomsdayMainWClass";
	return RegisterClass(&wc);
}

BOOL InitInstance(HINSTANCE hInst, int cmdShow)
{
	HDC hdc;
	char buf[256];

	DD_MainWindowTitle(buf);

	// Create the main window. It's shown right before GL init.
	hWndMain = CreateWindow("DoomsdayMainWClass", buf,
		WS_CAPTION | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInst,
		NULL);
	if(!hWndMain) return FALSE;

	// Set the font.
	hdc = GetDC(hWndMain);
	SetMapMode(hdc, MM_TEXT);
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

	// Tell DGL of our main window.
	gl.SetInteger(DGL_WINDOW_HANDLE, (int) hWndMain);
	return TRUE;
}
*/

void InitMainWindow(void)
{
	char buf[256];

	DD_MainWindowTitle(buf);
	SDL_WM_SetCaption(buf, NULL);
}

boolean InitGame(void)
{
	char *libName = NULL;

	// First we need to locate the game library name among the command
	// line arguments.
	DD_CheckArg("-game", &libName);

	// Was a game dll specified?
	if(!libName) 
	{
		DD_ErrorBox(true, "InitGame: No game library was specified.\n");
		return false;
	}

	// Now, load the library and get the API/exports.
	if(!(hGame = dlopen(libName, RTLD_NOW)))
	{
		DD_ErrorBox(true, "InitGame: Loading of %s failed (%s).\n", 
					libName, dlerror());
		return false;
	}

	// Get the function.
	if(!(GetGameAPI = (GETGAMEAPI) dlsym(hGame, "GetGameAPI")))
	{
		DD_ErrorBox(true, "InitGame: Failed to get address of "
					"GetGameAPI (%s).\n", dlerror());
		return false;
	}		

	// Do the API transfer.
	DD_InitAPI();
	
	// Everything seems to be working...
	return true;
}

//===========================================================================
// LoadPlugin
//	Loads the given plugin. Returns TRUE iff the plugin was loaded 
//	succesfully.
//===========================================================================
boolean LoadPlugin(const char *filename)
{
/*	int i;

	// Find the first empty plugin instance.
	for(i = 0; hInstPlug[i]; i++);

	// Try to load it.
	if(!(hInstPlug[i] = LoadLibrary(filename))) return FALSE; // Failed!
*/
	// That was all; the plugin registered itself when it was loaded.	
	return true;
}

//===========================================================================
// InitPlugins
//	Loads all the plugins from the startup directory. 
//===========================================================================
boolean InitPlugins(void)
{
/*	long hFile;
	struct _finddata_t fd;
	char plugfn[256];

	sprintf(plugfn, "%sdp*.dll", ddBinDir.path);
	if((hFile = _findfirst(plugfn, &fd)) == -1L) return TRUE;
	do LoadPlugin(fd.name); while(!_findnext(hFile, &fd));*/
	return true;
}

//===========================================================================
// main
//===========================================================================
int main(int argc, char **argv)
{
	char *cmdLine;
	int i, length = 0;

	// Where are we?
//	GetModuleFileName(hInstance, path, 255);
//	Dir_FileDir(path, &ddBinDir);
	
	// Assemble a command line string.
	for(i = 0, length = 0; i < argc; i++)
		length += strlen(argv[i]) + 1;

	// Allocate a large enough string.
	cmdLine = malloc(length);
	
	for(i = 0, length = 0; i < argc; i++)
	{
		strcpy(cmdLine + length, argv[i]);
		if(i < argc - 1) strcat(cmdLine, " ");
		length += strlen(argv[i]) + 1;
	}

	// Prepare the command line arguments.
	DD_InitCommandLine(cmdLine);

	free(cmdLine);
	cmdLine = NULL;

	// Load the rendering DLL.
	if(!DD_InitDGL()) return 1;

	// Load the game DLL.
	if(!InitGame()) return 2;

	// Load all plugins that are found.
	if(!InitPlugins()) return 3; // Fatal error occured?

	// Initialize SDL.
	if(SDL_Init(SDL_INIT_TIMER))
	{
		DD_ErrorBox(true, "SDL Init Failed: %s\n", SDL_GetError());
		return 4;
	}

	// Also initialize the SDL video subsystem, unless we're going to
	// run in dedicated mode.
	if(!ArgExists("-dedicated"))
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO))
		{
			DD_ErrorBox(true, "SDL Init Failed: %s\n", SDL_GetError());
			return 5;
		}
	}

	InitMainWindow();

	// Init memory zone.
	Z_Init();
	
	// Fire up the engine. The game loop will also act as the message pump.
	DD_Main();
    return 0;
}
 
//===========================================================================
// DD_Shutdown
//	Shuts down the engine.
//===========================================================================
void DD_Shutdown(void)
{
	int i;

	// Shutdown all subsystems.
	DD_ShutdownAll();

	SDL_Quit();

	// Close the dynamic libraries.
	dlclose(hGame);
	for(i = 0; hPlugin[i]; i++)
	{
		dlclose(hPlugin[i]);
	}
	hGame = NULL;
	memset(hPlugin, 0, sizeof(hPlugin));
}
