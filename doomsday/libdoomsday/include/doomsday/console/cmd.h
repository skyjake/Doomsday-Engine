/** @file command.h
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_CONSOLE_CMD_H
#define LIBDOOMSDAY_CONSOLE_CMD_H

#include "../libdoomsday.h"
#include "dd_share.h"
#include <de/String>

#define DENG_MAX_ARGS            256

typedef struct {
    char cmdLine[2048];
    int argc;
    char *argv[DENG_MAX_ARGS];
} cmdargs_t;

typedef struct ccmd_s {
    /// Next command in the global list.
    struct ccmd_s *next;

    /// Next and previous overloaded versions of this command (if any).
    struct ccmd_s *nextOverload, *prevOverload;

    /// Execute function.
    int (*execFunc) (byte src, int argc, char **argv);

    /// Name of the command.
    char const *name;

    /// @ref consoleCommandFlags
    int flags;

    /// Minimum and maximum number of arguments. Used with commands
    /// that utilize an engine-validated argument list.
    int minArgs, maxArgs;

    /// List of argument types for this command.
    cvartype_t args[DENG_MAX_ARGS];
} ccmd_t;

void Con_InitCommands();
void Con_ClearCommands(void);

void Con_AddCommand(ccmdtemplate_t const *cmd);
void Con_AddCommandList(ccmdtemplate_t const *cmdList);

void Con_AddKnownWordsForCommands();

/**
 * Search the console database for a named command. If one or more overloaded
 * variants exist then return the variant registered most recently.
 *
 * @param name  Name of the command to search for.
 * @return  Found command else @c 0
 */
ccmd_t *Con_FindCommand(char const *name);

/**
 * Search the console database for a command. If one or more overloaded variants
 * exist use the argument list to select the required variant.
 *
 * @param args
 * @return  Found command else @c 0
 */
ccmd_t *Con_FindCommandMatchArgs(cmdargs_t *args);

/**
 * @return  @c true iff @a name matches a known command or alias name.
 */
dd_bool Con_IsValidCommand(char const *name);

de::String Con_CmdAsStyledText(ccmd_t *cmd);

LIBDOOMSDAY_PUBLIC void Con_PrintCommandUsage(ccmd_t const *ccmd, bool allOverloads = true);

/**
 * Returns a rich formatted, textual representation of the specified console
 * command's argument list, suitable for logging.
 *
 * @param ccmd  The console command to format usage info for.
 */
de::String Con_CmdUsageAsStyledText(ccmd_t const *ccmd);

#endif // LIBDOOMSDAY_CONSOLE_CMD_H
