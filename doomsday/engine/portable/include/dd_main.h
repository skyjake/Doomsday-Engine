/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * dd_main.h: Engine Core
 */

#ifndef __DOOMSDAY_MAIN_H__
#define __DOOMSDAY_MAIN_H__

#include "dd_types.h"

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern int      verbose;
extern int      maxzone;
extern int      shareware;         // true if only episode 1 present
extern boolean  cdrom;             // true if cd-rom mode active ("-cdrom")
extern boolean  debugmode;         // checkparm of -debug
extern boolean  nofullscreen;      // checkparm of -nofullscreen
extern boolean  singletics;        // debug flag to cancel adaptiveness
extern FILE    *outFile;           // Output file for console messages.
extern int      isDedicated;
extern char     ddBasePath[];
extern char    *defaultWads;       // A list of wad names, whitespace in between (in .cfg).
extern directory_t ddRuntimeDir, ddBinDir;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

void            DD_Main();
void            DD_UpdateEngineState(void);
void            DD_GameUpdate(int flags);
void            DD_AddStartupWAD(const char *file);
void            DD_AddIWAD(const char *path);
void            DD_AutoLoad(void);
void            DD_SetConfigFile(char *filename);
void            DD_SetDefsFile(char *filename);
int             DD_GetInteger(int ddvalue);
void            DD_SetInteger(int ddvalue, int parm);
void            DD_SetVariable(int ddvalue, void *ptr);
void           *DD_GetVariable(int ddvalue);
ddplayer_t     *DD_GetPlayer(int number);
void            DD_CheckTimeDemo(void);

#endif
