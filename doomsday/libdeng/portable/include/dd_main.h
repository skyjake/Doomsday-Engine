/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_main.h: Engine Core
 */

#ifndef __DOOMSDAY_MAIN_H__
#define __DOOMSDAY_MAIN_H__

#include "dd_types.h"

// Verbose messages.
#define VERBOSE(code)   { if(verbose >= 1) { code; } }
#define VERBOSE2(code)  { if(verbose >= 2) { code; } }

extern int verbose;
extern int maxZone;
extern int isDedicated;
extern char ddBasePath[];
extern char* defaultWads; // A list of wad names, whitespace in between (in .cfg).
extern directory_t ddRuntimeDir, ddBinDir;
extern filename_t bindingsConfigFileName;

#ifndef WIN32
extern GETGAMEAPI GetGameAPI;
#endif

int             DD_Main(void);
void            DD_UpdateEngineState(void);
void            DD_GameUpdate(int flags);
void            DD_AutoLoad(void);
void            DD_CheckTimeDemo(void);
const char*     value_Str(int val);

// Wrappers.
int             DD_WindowWidth(void);
int             DD_WindowHeight(void);
int             DD_WindowBPP(void);
boolean         GL_Grab(int x, int y, int width, int height, dgltexformat_t format, void *buffer);
boolean         Sys_SetWindowTitle(uint idx, const char *title);
boolean         Sys_GetWindowFullscreen(uint idx, boolean *fullscreen);

#endif
