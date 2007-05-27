/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * dd_uinit.c: Unix Initialization
 *
 * Load libraries and set up APIs.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <SDL.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

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
lt_dlhandle hPlugins[MAX_PLUGS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void InitMainWindow(void)
{
	char        buf[256];

	DD_MainWindowTitle(buf);
	SDL_WM_SetCaption(buf, NULL);
}

boolean InitGame(void)
{
	char       *gameName = NULL;
	char        libName[PATH_MAX];

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
#ifdef MACOSX
	strcpy(libName, gameName);
#else
	sprintf(libName, "lib%s", gameName);
	strcat(libName, ".so");
#endif
	if(!(hGame = lt_dlopenext(libName)))
	{
		DD_ErrorBox(true, "InitGame: Loading of %s failed (%s).\n", libName,
					lt_dlerror());
		return false;
	}

    if(!(GetGameAPI = (GETGAMEAPI) lt_dlsym(hGame, "GetGameAPI")))
	{
		DD_ErrorBox(true,
					"InitGame: Failed to get address of " "GetGameAPI (%s).\n",
					lt_dlerror());
		return false;
	}

	// Do the API transfer.
	DD_InitAPI();

	// Everything seems to be working...
	return true;
}

static lt_dlhandle *NextPluginHandle(void)
{
	int         i;

	for(i = 0; i < MAX_PLUGS; ++i)
	{
		if(!hPlugins[i])
            return &hPlugins[i];
	}
	return NULL;
}

/*
#if 0
int LoadPlugin(const char *pluginPath, lt_ptr data)
{
	filename_t name;
	lt_dlhandle plugin, *handle;
	void (*initializer)(void);

	// What is the actual file name?
	_splitpath(pluginPath, NULL, NULL, name, NULL);
	
	printf("LP: %s => %s\n", pluginPath, name);

	if(!strncmp(name, "libdp", 5))
	{
#if 0
		filename_t fullPath;
		
#ifdef DENG_LIBRARY_DIR
		sprintf(fullPath, DENG_LIBRARY_DIR "/%s", name);
#else
		strcpy(fullPath, pluginPath);
#endif
		//if(strchr(name, '.'))
			strcpy(name, pluginPath);
		else
		{
			strcpy(name, pluginPath);
			strcat(name, ".dylib");
			}
#endif
		// Try loading this one as a Doomsday plugin.
		if(NULL == (plugin = lt_dlopenext(pluginPath)))
		{
			printf("LoadPlugin: Failed to open %s!\n", pluginPath);
			return 0;
		}

		if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
		   NULL == (handle = NextPluginHandle()))
		{
			printf("LoadPlugin: Failed to load %s!\n", pluginPath);
			lt_dlclose(plugin);
			return 0;
		}

		// This seems to be a Doomsday plugin.
		*handle = plugin;

		printf("LoadPlugin: %s\n", pluginPath);
		initializer();
	}
	
	return 0;
}
#endif
*/

int LoadPlugin(const char *pluginPath, lt_ptr data)
{
#ifndef MACOSX
	filename_t  name;
#endif
	lt_dlhandle plugin, *handle;
	void (*initializer)(void);
	
#ifndef MACOSX
	// What is the actual file name?
	_splitpath(pluginPath, NULL, NULL, name, NULL);
	if(!strncmp(name, "libdp", 5))
#endif
	{
		// Try loading this one as a Doomsday plugin.
		if(NULL == (plugin = lt_dlopenext(pluginPath)))
			return 0;
		
		if(NULL == (initializer = lt_dlsym(plugin, "DP_Initialize")) ||
		   NULL == (handle = NextPluginHandle()))
		{
			printf("LoadPlugin: Failed to load %s!\n", pluginPath);
			lt_dlclose(plugin);
			return 0;
		}
		
		// This seems to be a Doomsday plugin.
		*handle = plugin;
		
		printf("LoadPlugin: %s\n", pluginPath);
		initializer();
	}
	
	return 0;
}

/**
 * Loads all the plugins from the library directory.
 */
boolean InitPlugins(void)
{
	// Try to load all libraries that begin with libdp.
	lt_dlforeachfile(NULL, LoadPlugin, NULL);
	return true;
}

int main(int argc, char **argv)
{
	char       *cmdLine;
	int         i, length = 0;

	// Initialize libtool's dynamic library routines.
	lt_dlinit();

#ifdef DENG_LIBRARY_DIR
	// The default directory is defined in the Makefile.  For
	// instance, "/usr/local/lib".
	lt_dladdsearchdir(DENG_LIBRARY_DIR);
#endif

	// Assemble a command line string.
	for(i = 0, length = 0; i < argc; ++i)
		length += strlen(argv[i]) + 1;

	// Allocate a large enough string.
	cmdLine = malloc(length);

	for(i = 0, length = 0; i < argc; ++i)
	{
		strcpy(cmdLine + length, argv[i]);
		if(i < argc - 1)
			strcat(cmdLine, " ");
		length += strlen(argv[i]) + 1;
	}

	// Prepare the command line arguments.
	DD_InitCommandLine(cmdLine);

	free(cmdLine);
	cmdLine = NULL;
	// Load the rendering DLL.
	if(!DD_InitDGL())
		return 1;
	// Load the game DLL.
	if(!InitGame())
		return 2;
	// Load all plugins that are found.
	if(!InitPlugins())
		return 3;				// Fatal error occured?
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
		if(SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK))
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

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
	int         i;

	// Shutdown all subsystems.
	DD_ShutdownAll();

	SDL_Quit();

	// Close the dynamic libraries.
	lt_dlclose(hGame);
	hGame = NULL;
	for(i = 0; hPlugins[i]; ++i)
		lt_dlclose(hPlugins[i]);
	memset(hPlugins, 0, sizeof(hPlugins));

	lt_dlexit();
}
