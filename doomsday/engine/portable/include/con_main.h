/**\file con_main.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Console Subsystem.
 */

#ifndef LIBDENG_CONSOLE_MAIN_H
#define LIBDENG_CONSOLE_MAIN_H

#include <stdio.h>
#include "dd_share.h"
#include "de_system.h"
#include "dd_input.h"

#include "con_buffer.h"

#define CMDLINE_SIZE 256
#define MAX_ARGS            256

#define OBSOLETE            CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console command values through the shared data ptr.
#define CV_INT(var)         (*(int*) var->ptr)
#define CV_BYTE(var)        (*(byte*) var->ptr)
#define CV_FLOAT(var)       (*(float*) var->ptr)
#define CV_CHARPTR(var)     (*(char**) var->ptr)

typedef struct {
    char cmdLine[2048];
    int argc;
    char* argv[MAX_ARGS];
} cmdargs_t;

typedef struct ddccmd_s {
    /// Next command in the global list.
    struct ddccmd_s* next;

    /// Next and previous overloaded versions of this command (if any).
    struct ddccmd_s* nextOverload, *prevOverload;

    /// List of argument types for this command.
    cvartype_t args[MAX_ARGS];

    /// Minimum and maximum number of arguments. Used with commands
    /// that utilize an engine-validated argument list.
    int minArgs, maxArgs;

    /// Publicly visible shared data.
    ccmd_t shared;
} ddccmd_t;

typedef struct cvar_s {
    /// Name of the cvar.
    const char* name;

    /// @see consoleVariableFlags
    int flags;

    /// Type of this variable.
    cvartype_t type;

    /// Pointer to the user data.
    void* ptr;

    /// Minimum and maximum values (for ints and floats).
    float min, max;

    /// On-change notification callback.
    void (*notifyChanged)(void);
} cvar_t;

typedef enum {
    WT_ANY = -1,
    KNOWNWORDTYPE_FIRST = 0,
    WT_CCMD = KNOWNWORDTYPE_FIRST,
    WT_CVAR,
    WT_CALIAS,
    WT_GAMEINFO,
    KNOWNWORDTYPE_COUNT
} knownwordtype_t;

#define VALID_KNOWNWORDTYPE(t)      ((t) >= KNOWNWORDTYPE_FIRST && (t) < KNOWNWORDTYPE_COUNT)

typedef struct knownword_s {
    knownwordtype_t type;
    void* data;
} knownword_t;

typedef struct calias_s {
    /// Name of this alias.
    char* name;

    /// Aliased command string.
    char* command;
} calias_t;

typedef void (*con_textfilter_t) (char* text);

// Console commands can set this when they need to return a custom value
// e.g. for the game library.
extern int CmdReturnValue;
extern byte consoleDump;

void Con_Register(void);
void Con_DataRegister(void);

boolean Con_Init(void);
void Con_Shutdown(void);

void Con_InitDatabases(void);
void Con_ShutdownDatabases(void);

void Con_Ticker(timespan_t time);

/// @return  @c true iff the event is 'eaten'.
boolean Con_Responder(ddevent_t* ev);

/**
 * Attempt to change the 'open' state of the console.
 * \note In dedicated mode the console cannot be closed.
 */
void Con_Open(int yes);

boolean Con_IsActive(void);

boolean Con_IsLocked(void);

boolean Con_InputMode(void);

uint Con_CursorPosition(void);

char* Con_CommandLine(void);

cbuffer_t* Con_ConsoleBuffer(void);

fontid_t Con_Font(void);

void Con_SetFont(fontid_t font);

con_textfilter_t Con_PrintFilter(void);

void Con_SetPrintFilter(con_textfilter_t filter);

void Con_FontScale(float* scaleX, float* scaleY);

void Con_SetFontScale(float scaleX, float scaleY);

void Con_AddCommand(const ccmd_t* cmd);
void Con_AddCommandList(const ccmd_t* cmdList);

/**
 * Search the console database for a named command. If one or more overloaded
 * variants exist then return the variant registered most recently.
 *
 * @param name              Name of the command to search for.
 * @return  Found command else @c 0
 */
ddccmd_t* Con_FindCommand(const char* name);

/**
 * Search the console database for a command. If one or more overloaded variants
 * exist use the argument list to select the required variant.
 *
 * @param args  
 * @return  Found command else @c 0
 */
ddccmd_t* Con_FindCommandMatchArgs(cmdargs_t* args);

void Con_AddVariable(const cvartemplate_t* tpl);
void Con_AddVariableList(const cvartemplate_t* tplList);
cvar_t* Con_FindVariable(const char* name);

/// @return  Type of the variable associated with @a name if found else @c CVT_NULL
cvartype_t Con_GetVariableType(const char* name);

int Con_GetInteger(const char* name);
float Con_GetFloat(const char* name);
byte Con_GetByte(const char* name);
char* Con_GetString(const char* name);

/**
 * \note Also used with @c CVT_BYTE.
 * @param svflags           @see setVariableFlags
 */
void Con_SetInteger2(const char* name, int value, int svflags);
void Con_SetInteger(const char* name, int value);

void Con_SetFloat2(const char* name, float value, int svflags);
void Con_SetFloat(const char* name, float value);

void Con_SetString2(const char* name, char* text, int svflags);
void Con_SetString(const char* name, char* text);

calias_t* Con_AddAlias(const char* name, const char* command);

/**
 * @return  @c 0 if the specified alias can't be found.
 */
calias_t* Con_FindAlias(const char* name);

void Con_DeleteAlias(calias_t* cal);

/**
 * @return  @c true iff @a name matches a known command or alias name.
 */
boolean Con_IsValidCommand(const char* name);

/**
 * Attempt to execute a console command.
 *
 * @param src               The source of the command @see commandSource
 * @param command           The command to be executed.
 * @param silent            Non-zero indicates not to log execution of the command.
 * @param netCmd            If @c true, command was sent over the net.
 *
 * @return  Non-zero if successful else @c 0.
 */
int Con_Execute(byte src, const char* command, int silent, boolean netCmd);
int Con_Executef(byte src, int silent, const char* command, ...) PRINTF_F(3,4);

/**
 * Print an error message and quit.
 */
void Con_Error(const char* error, ...);
void Con_AbnormalShutdown(const char* error);

/**
 * Iterate over words in the known-word dictionary, making a callback for each.
 * Iteration ends when all selected words have been visited or a callback returns
 * non-zero.
 *
 * @param pattern           If not @c NULL or an empty string, only process those
 *                          words which match this pattern.
 * @param type              If a valid word type, only process words of this type.
 * @param callback          Callback to make for each processed word.
 * @param paramaters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Con_IterateKnownWords(const char* pattern, knownwordtype_t type,
    int (*callback)(const knownword_t* word, void* paramaters), void* paramaters);

/**
 * Collect an array of knownWords which match the given word (at least
 * partially).
 *
 * \note: The array must be freed with free()
 *
 * @param word              The word to be matched.
 * @param type              If a valid word type, only collect words of this type.
 * @param count             If not @c NULL the matched word count is written back here.
 *
 * @return  A NULL-terminated array of pointers to all the known words which
 *          match the search criteria.
 */
const knownword_t** Con_CollectKnownWordsMatchingWord(const char* word,
    knownwordtype_t type, unsigned int* count);

/**
 * Print a 'global' message (to stdout and the console).
 */
void Con_Message(const char* message, ...) PRINTF_F(1,2);

/**
 * Print into the console.
 * @param flags             @see consoleBufferLineFlags
 */
void Con_FPrintf(int flags, const char* format, ...) PRINTF_F(2,3);
void Con_Printf(const char* format, ...) PRINTF_F(1,2);

/**
 * @defgroup printPathFlags Print Path Flags
 */
/*{@*/
#define PPF_MULTILINE           0x1 // Use multiple lines.
#define PPF_TRANSFORM_PATH_MAKEPRETTY 0x2 // Make paths 'prettier'.
#define PPF_TRANSFORM_PATH_PRINTINDEX 0x4 // Print an index for each path.
/*}@*/

/**
 * Prints the passed path list to the console.
 *
 * \todo treat paths as URIs (i.e., resolve symbols).
 *
 * @param pathList          A series of textual file/resource names/paths separated
 *                          by semicolons.
 * @param flags             @see printPathFlags.
 */
void Con_PrintPathList3(const char* pathList, const char* seperator, int flags);
void Con_PrintPathList2(const char* pathList, const char* seperator);
void Con_PrintPathList(const char* pathList);

void Con_PrintCVar(cvar_t* cvar, char* prefix);

/**
 * Outputs the usage information for the given ccmd to the console if the
 * ccmd's usage is validated by Doomsday.
 *
 * @param ccmd              Ptr to the ccmd to print the usage info for.
 * @param showExtra         If @c true, print any additional info we have.
 */
void Con_PrintCCmdUsage(ddccmd_t* ccmd, boolean showExtra);

#endif /* LIBDENG_CONSOLE_MAIN_H */
