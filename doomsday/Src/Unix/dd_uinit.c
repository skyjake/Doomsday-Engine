
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
#include <ltdl.h>
#include <limits.h>
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

lt_dlhandle hGame;
lt_dlhandle hPlugin[MAX_PLUGS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void InitMainWindow(void)
{
	char buf[256];

	DD_MainWindowTitle(buf);
	SDL_WM_SetCaption(buf, NULL);
}

boolean InitGame(void)
{
	char *gameName = NULL;
	char libName[PATH_MAX];

	// First we need to locate the game library name among the command
	// line arguments.
	DD_CheckArg("-game", &gameName);

	// Was a game dll specified?
	if(!gameName) 
	{
		DD_ErrorBox(true, "InitGame: No game library was specified.\n");
		return false;
	}

	// Now, load the library and get the API/exports.
	sprintf(libName, "lib%s", gameName);
	if(!(hGame = lt_dlopenext(libName)))
	{
		DD_ErrorBox(true, "InitGame: Loading of %s failed (%s).\n", 
					libName, lt_dlerror());
		return false;
	}
	if(!(GetGameAPI = (GETGAMEAPI) lt_dlsym(hGame, "GetGameAPI")))
	{
		DD_ErrorBox(true, "InitGame: Failed to get address of "
					"GetGameAPI (%s).\n", lt_dlerror());
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

	// Initialize libtool's dynamic library routines.
	lt_dlinit();
	
#ifdef DENG_LIBRARY_DIR
	// The default directory is defined in the Makefile.  For
	// instance, "/usr/local/lib".
	lt_dladdsearchdir(DENG_LIBRARY_DIR);
#endif	

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
	lt_dlclose(hGame);
	for(i = 0; hPlugin[i]; i++)
	{
		lt_dlclose(hPlugin[i]);
	}
	hGame = NULL;
	memset(hPlugin, 0, sizeof(hPlugin));

	lt_dlexit();
}
