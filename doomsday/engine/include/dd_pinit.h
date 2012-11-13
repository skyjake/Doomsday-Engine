/**
 * @file dd_pinit.h
 * Platform independent routines for initializing the engine. @ingroup base
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
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

/// Maximum allowed number of plugins.
#define MAX_PLUGS   32

/**
 * Shuts down all subsystems. This is called from DD_Shutdown().
 */
void DD_ShutdownAll(void);

int DD_CheckArg(char* tag, const char** value);

/**
 * Compose the title for the main window.
 * @param title  Title text for the window.
 */
void DD_ComposeMainWindowTitle(char* title);

/**
 * Called early on during the startup process so that we can get the console
 * online ready for printing ASAP.
 */
void DD_ConsoleInit(void);

void DD_InitAPI(void);

/**
 * Define abbreviations and aliases for command line options.
 */
void DD_InitCommandLine(void);

extern game_import_t gi;
extern game_export_t gx;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_PORTABLE_INIT_H */
