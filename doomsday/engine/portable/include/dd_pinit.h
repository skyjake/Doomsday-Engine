/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * dd_pinit.h: Portable Engine Initialization
 */

#ifndef __DOOMSDAY_PORTABLE_INIT_H__
#define __DOOMSDAY_PORTABLE_INIT_H__

#include "dd_share.h"
#include "dd_api.h"

// Maximum allowed number of plugins.
#define MAX_PLUGS   32

void            DD_ShutdownAll(void);
int             DD_CheckArg(char* tag, const char** value);
void            DD_ErrorBox(boolean error, char* format, ...) PRINTF_F(2,3);
void            DD_ComposeMainWindowTitle(char* title);
boolean         DD_EarlyInit(void);
void            DD_InitAPI(void);
void            DD_InitCommandLine(const char* cmdLine);

extern game_import_t gi;
extern game_export_t gx;

#endif                          // __DOOMSDAY_PORTABLE_INIT_H__
