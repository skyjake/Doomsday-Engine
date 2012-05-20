/**\file dd_pinit.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * Portable Engine Initialization
 */

#ifndef LIBDENG_PORTABLE_INIT_H
#define LIBDENG_PORTABLE_INIT_H

#include "dd_share.h"
#include "dd_api.h"
#include <de/c_wrapper.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint mainWindowIdx;

// Maximum allowed number of plugins.
#define MAX_PLUGS   32

void DD_ShutdownAll(void);
int DD_CheckArg(char* tag, const char** value);
void DD_ComposeMainWindowTitle(char* title);
void DD_ConsoleInit(void);
void DD_InitAPI(void);
void DD_InitCommandLine(void);

extern game_import_t gi;
extern game_export_t gx;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_PORTABLE_INIT_H */
