/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * dd_pinit.c: Portable Engine Initialization
 *
 * Platform independent routines for initializing the engine.
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdarg.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_network.h"
#include "de_misc.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

/*
 * The game imports and exports.
 */
game_import_t __gi;		
game_export_t __gx;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int DD_CheckArg(char *tag, char **value)
{
	int i = ArgCheck(tag);
	char *next = ArgNext();

	if(!i) return 0;
	if(value && next) *value = next;
	return 1;
}

void DD_ErrorBox(boolean error, char *format, ...)
{
	char buff[200];
	va_list args;

	va_start(args, format);
	vsprintf(buff, format, args);
	va_end(args);
#ifdef WIN32
	MessageBox(NULL, buff, "Doomsday "DOOMSDAY_VERSION_TEXT, 
		MB_OK | (error? MB_ICONERROR : MB_ICONWARNING));
#endif
#ifdef UNIX
	fputs(buff, stderr);
#endif
}

/*
 * Compose the title for the main window.
 */
void DD_MainWindowTitle(char *title)
{
	sprintf(title, "Doomsday "DOOMSDAY_VERSION_TEXT" : %s", 
		__gx.Get(DD_GAME_ID));
}

void SetGameImports(game_import_t *imp)
{
	memset(imp, 0, sizeof(*imp));
	imp->apiSize = sizeof(*imp);
	imp->version = DOOMSDAY_VERSION;

	// Data.
	imp->mobjinfo = &mobjinfo;
	imp->states = &states;
	imp->sprnames = &sprnames;
	imp->text = &texts;

	imp->validcount = &validcount;
	imp->topslope = &topslope;
	imp->bottomslope = &bottomslope;
	imp->thinkercap = &thinkercap;

	imp->numvertexes = &numvertexes;
	imp->numsegs = &numsegs;
	imp->numsectors = &numsectors;
	imp->numsubsectors = &numsubsectors;
	imp->numnodes = &numnodes;
	imp->numlines = &numlines;
	imp->numsides = &numsides;

	imp->vertexes = (void**) &vertexes;
	imp->segs = (void**) &segs;
	imp->sectors = (void**) &sectors;
	imp->subsectors = (void**) &subsectors;
	imp->nodes = (void**) &nodes;
	imp->lines = (void**) &lines;
	imp->sides = (void**) &sides;

	imp->blockmaplump = &blockmaplump;
	imp->blockmap = &blockmap;
	imp->bmapwidth = &bmapwidth;
	imp->bmapheight = &bmapheight;
	imp->bmaporgx = &bmaporgx;
	imp->bmaporgy = &bmaporgy;
	imp->rejectmatrix = &rejectmatrix;
	imp->polyblockmap = (void***) &polyblockmap;
	imp->polyobjs = (void**) &polyobjs;
	imp->numpolyobjs = &po_NumPolyobjs;
}

void DD_InitAPI(void)
{
#ifdef WIN32
	extern GETGAMEAPI GetGameAPI;
#endif

	game_export_t *gameExPtr;
	
	// Put the imported stuff into the imports.
	SetGameImports(&__gi);

	memset(&__gx, 0, sizeof(__gx));
	gameExPtr = GetGameAPI(&__gi);
	memcpy(&__gx, gameExPtr, MIN_OF(sizeof(__gx), gameExPtr->apiSize));
}

void DD_InitCommandLine(const char *cmdLine)
{
	ArgInit(cmdLine);

	// Register some abbreviations for command line options.
	ArgAbbreviate("-game", "-g");
	ArgAbbreviate("-gl", "-r"); // As in (R)enderer...
	ArgAbbreviate("-defs", "-d");
	ArgAbbreviate("-width", "-w");
	ArgAbbreviate("-height", "-h");
	ArgAbbreviate("-winsize", "-wh");
	ArgAbbreviate("-bpp", "-b");
	ArgAbbreviate("-window", "-wnd");
	ArgAbbreviate("-nocenter", "-noc");
	ArgAbbreviate("-paltex", "-ptx");
	ArgAbbreviate("-file", "-f");
	ArgAbbreviate("-maxzone", "-mem");
	ArgAbbreviate("-config", "-cfg");
	ArgAbbreviate("-parse", "-p");
	ArgAbbreviate("-cparse", "-cp");
	ArgAbbreviate("-command", "-cmd");
	ArgAbbreviate("-fontdir", "-fd");
	ArgAbbreviate("-modeldir", "-md");
	ArgAbbreviate("-basedir", "-bd");
	ArgAbbreviate("-stdbasedir", "-sbd");
	ArgAbbreviate("-userdir", "-ud");
	ArgAbbreviate("-texdir", "-td");
	ArgAbbreviate("-texdir2", "-td2");
	ArgAbbreviate("-anifilter", "-ani");
	ArgAbbreviate("-verbose", "-v");
}

/*
 * This is called from DD_Shutdown().
 */
void DD_ShutdownAll(void)
{
	extern memzone_t *mainzone;
	int i;

	DD_ShutdownHelp();
	Zip_Shutdown();

	// Kill the message window if it happens to be open.
	SW_Shutdown();

#ifdef WIN32
	// Enables Alt-Tab, Alt-Esc, Ctrl-Alt-Del.
	SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, 0, 0);
#endif

	// Stop all demo recording.
	for(i = 0; i < MAXPLAYERS; i++) Demo_StopRecording(i);

	Sv_Shutdown();
	R_Shutdown();
	Sys_ConShutdown();
	Def_Destroy();
	F_ShutdownDirec();
	FH_Clear();
	ArgShutdown();
	free(mainzone);
	DD_ShutdownDGL();

	// Close the message output file.
	fclose(outFile);
	outFile = NULL;
}
