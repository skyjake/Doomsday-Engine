/** @file exec.h  Console executive module.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_CONSOLE_EXEC_H
#define LIBDOOMSDAY_CONSOLE_EXEC_H

#include "../libdoomsday.h"
#include "dd_share.h"
#include "dd_types.h"
#include <de/game/Game>
#include <de/Path>

#include <de/shell/Lexicon> // known words
#include <cstdio>

#define CMDLINE_SIZE        256

#define OBSOLETE            CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console variable values through the shared data ptr.
#define CV_INT(var)         (*(int *) var->ptr)
#define CV_BYTE(var)        (*(byte *) var->ptr)
#define CV_FLOAT(var)       (*(float *) var->ptr)
#define CV_CHARPTR(var)     (*(char **) var->ptr)
#define CV_URIPTR(var)      (*(de::Uri **) var->ptr)

void Con_DataRegister();

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC void Con_Register();

LIBDOOMSDAY_PUBLIC dd_bool Con_Init();
LIBDOOMSDAY_PUBLIC void Con_InitDatabases();
LIBDOOMSDAY_PUBLIC void Con_ClearDatabases();
LIBDOOMSDAY_PUBLIC void Con_Shutdown();

void Con_ShutdownDatabases();

LIBDOOMSDAY_PUBLIC void Con_Ticker(timespan_t time);

/**
 * Attempt to execute a console command.
 *
 * @param src               The source of the command (@ref commandSource)
 * @param command           The command to be executed.
 * @param silent            Non-zero indicates not to log execution of the command.
 * @param netCmd            If @c true, command was sent over the net.
 *
 * @return  Non-zero if successful else @c 0.
 */
LIBDOOMSDAY_PUBLIC int Con_Execute(byte src, char const *command, int silent, dd_bool netCmd);
LIBDOOMSDAY_PUBLIC int Con_Executef(byte src, int silent, char const *command, ...) PRINTF_F(3,4);

LIBDOOMSDAY_PUBLIC dd_bool Con_Parse(de::Path const &fileName, dd_bool silently);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

LIBDOOMSDAY_PUBLIC de::String Con_GameAsStyledText(de::game::Game const *game);

#endif // __cplusplus

#endif // LIBDOOMSDAY_CONSOLE_EXEC_H
