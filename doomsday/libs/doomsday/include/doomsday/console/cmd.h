/** @file command.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "var.h"
#include <de/legacy/types.h>

#ifdef __cplusplus
#  include <de/string.h>
#endif

#define DE_MAX_ARGS            256

typedef struct {
    char cmdLine[2048];
    int argc;
    char *argv[DE_MAX_ARGS];
} cmdargs_t;

/**
 * Console command template. Used with Con_AddCommand().
 * @ingroup ccmd
 */
typedef struct ccmdtemplate_s {
    /// Name of the command.
    const char* name;

    /// Argument template.
    const char* argTemplate;

    /// Execute function.
    int (*execFunc) (byte src, int argc, char** argv);

    /// @ref consoleCommandFlags
    unsigned int flags;
} ccmdtemplate_t;

typedef struct ccmd_s {
    /// Next command in the global list.
    struct ccmd_s *next;

    /// Next and previous overloaded versions of this command (if any).
    struct ccmd_s *nextOverload, *prevOverload;

    /// Execute function.
    int (*execFunc) (byte src, int argc, char **argv);

    /// Name of the command.
    const char *name;

    /// @ref consoleCommandFlags
    int flags;

    /// Minimum and maximum number of arguments. Used with commands
    /// that utilize an engine-validated argument list.
    int minArgs, maxArgs;

    /// List of argument types for this command.
    cvartype_t args[DE_MAX_ARGS];
} ccmd_t;

/**
 * @defgroup consoleCommandFlags Console Command Flags
 * @ingroup ccmd apiFlags
 */
///@{
#define CMDF_NO_NULLGAME    0x00000001 ///< Not available unless a game is loaded.
#define CMDF_NO_DEDICATED   0x00000002 ///< Not available in dedicated server mode.
///@}

/**
 * @defgroup consoleUsageFlags Console Command Usage Flags
 * @ingroup ccmd apiFlags
 * The method(s) that @em CANNOT be used to invoke a console command, used with
 * @ref commandSource.
 */
///@{
#define CMDF_DDAY           0x00800000
#define CMDF_GAME           0x01000000
#define CMDF_CONSOLE        0x02000000
#define CMDF_BIND           0x04000000
#define CMDF_CONFIG         0x08000000
#define CMDF_PROFILE        0x10000000
#define CMDF_CMDLINE        0x20000000
#define CMDF_DED            0x40000000
#define CMDF_CLIENT         0x80000000 ///< Sent over the net from a client.
///@}

/**
 * @defgroup commandSource Command Sources
 * @ingroup ccmd
 * Where a console command originated.
 */
///@{
#define CMDS_UNKNOWN        0
#define CMDS_DDAY           1 ///< Sent by the engine.
#define CMDS_GAME           2 ///< Sent by a game library.
#define CMDS_CONSOLE        3 ///< Sent via direct console input.
#define CMDS_BIND           4 ///< Sent from a binding/alias.
#define CMDS_CONFIG         5 ///< Sent via config file.
#define CMDS_PROFILE        6 ///< Sent via player profile.
#define CMDS_CMDLINE        7 ///< Sent via the command line.
#define CMDS_SCRIPT         8 ///< Sent based on a def in a DED file eg (state->execute).
///@}

/// Helper macro for declaring console command functions. @ingroup console
#define D_CMD(x)        int CCmd##x(byte src, int argc, char** argv)

/**
 * Helper macro for registering new console commands.
 * @ingroup ccmd
 */
#define C_CMD(name, argTemplate, fn) \
    { ccmdtemplate_t _template = { name, argTemplate, CCmd##fn, 0 }; Con_AddCommand(&_template); }

/**
 * Helper macro for registering new console commands.
 * @ingroup ccmd
 */
#define C_CMD_FLAGS(name, argTemplate, fn, flags) \
    { ccmdtemplate_t _template = { name, argTemplate, CCmd##fn, flags }; Con_AddCommand(&_template); }

#ifdef __cplusplus

void Con_InitCommands();
void Con_ClearCommands(void);
void Con_AddKnownWordsForCommands();

LIBDOOMSDAY_PUBLIC void Con_AddCommand(const ccmdtemplate_t *cmd);

LIBDOOMSDAY_PUBLIC void Con_AddCommandList(const ccmdtemplate_t *cmdList);

/**
 * Search the console database for a named command. If one or more overloaded
 * variants exist then return the variant registered most recently.
 *
 * @param name  Name of the command to search for.
 * @return  Found command else @c 0
 */
LIBDOOMSDAY_PUBLIC ccmd_t *Con_FindCommand(const char *name);

/**
 * Search the console database for a command. If one or more overloaded variants
 * exist use the argument list to select the required variant.
 *
 * @param args
 * @return  Found command else @c 0
 */
LIBDOOMSDAY_PUBLIC ccmd_t *Con_FindCommandMatchArgs(cmdargs_t *args);

/**
 * @return  @c true iff @a name matches a known command or alias name.
 */
LIBDOOMSDAY_PUBLIC dd_bool Con_IsValidCommand(const char *name);

LIBDOOMSDAY_PUBLIC de::String Con_CmdAsStyledText(ccmd_t *cmd);

LIBDOOMSDAY_PUBLIC void Con_PrintCommandUsage(const ccmd_t *ccmd, bool allOverloads = true);

/**
 * Returns a rich formatted, textual representation of the specified console
 * command's argument list, suitable for logging.
 *
 * @param ccmd  The console command to format usage info for.
 */
LIBDOOMSDAY_PUBLIC de::String Con_CmdUsageAsStyledText(const ccmd_t *ccmd);

/**
 * Defines a console command that behaves like a console variable but accesses
 * the data of a de::Config variable.
 *
 * The purpose of this mechanism is to provide a backwards compatible way to
 * access config variables.
 *
 * @note In the future, when the console uses Doomsday Script for executing commands,
 * this kind of mapping should be much easier since one can just create a reference to
 * the real variable and access it pretty much normally.
 *
 * @param consoleName     Name of the console command ("cvar").
 * @param opts            Type template when setting the value (using the
 *                        ccmdtemplate_t argument template format).
 * @param configVariable  Name of the de::Config variable.
 */
LIBDOOMSDAY_PUBLIC void Con_AddMappedConfigVariable(const char *consoleName,
                                                    const char *opts,
                                                    const de::String &configVariable);

#endif // __cplusplus

#endif // LIBDOOMSDAY_CONSOLE_CMD_H
