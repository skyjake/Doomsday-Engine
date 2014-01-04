/** @file con_main.h  Console Subsystem.
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

#ifndef DENG_CONSOLE_MAIN_H
#define DENG_CONSOLE_MAIN_H

#include "dd_share.h"
#include "dd_types.h"
#include "de_system.h"
#include "ui/dd_input.h"
#include "Game"

#include <de/shell/Lexicon> // known words
#include <cstdio>

#define CMDLINE_SIZE 256
#define MAX_ARGS            256

#define OBSOLETE            CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console variable values through the shared data ptr.
#define CV_INT(var)         (*(int *) var->ptr)
#define CV_BYTE(var)        (*(byte *) var->ptr)
#define CV_FLOAT(var)       (*(float *) var->ptr)
#define CV_CHARPTR(var)     (*(char **) var->ptr)
#define CV_URIPTR(var)      (*(uri_s **) var->ptr)

struct cbuffer_s;

typedef struct {
    char cmdLine[2048];
    int argc;
    char *argv[MAX_ARGS];
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
    cvartype_t args[MAX_ARGS];
} ccmd_t;

typedef struct cvar_s {
    /// @ref consoleVariableFlags
    int flags;

    /// Type of this variable.
    cvartype_t type;

    /// Pointer to this variable's node in the directory.
    void *directoryNode;

    /// Pointer to the user data.
    void *ptr;

    /// Minimum and maximum values (for ints and floats).
    float min, max;

    /// On-change notification callback.
    void (*notifyChanged)();
} cvar_t;

ddstring_t const *CVar_TypeName(cvartype_t type);

/// @return  @ref consoleVariableFlags
int CVar_Flags(cvar_t const *var);

/// @return  Type of the variable.
cvartype_t CVar_Type(cvar_t const *var);

/// @return  Symbolic name/path-to the variable.
AutoStr *CVar_ComposePath(cvar_t const *var);

int CVar_Integer(cvar_t const *var);
float CVar_Float(cvar_t const *var);
byte CVar_Byte(cvar_t const *var);
char const *CVar_String(cvar_t const *var);
Uri const *CVar_Uri(cvar_t const *var);

/**
 * Changes the value of an integer variable.
 * @note Also used with @c CVT_BYTE.
 *
 * @param var    Variable.
 * @param value  New integer value for the variable.
 */
void CVar_SetInteger(cvar_t *var, int value);

/**
 * @copydoc CVar_SetInteger()
 * @param svflags  @ref setVariableFlags
 */
void CVar_SetInteger2(cvar_t *var, int value, int svflags);

void CVar_SetFloat2(cvar_t *var, float value, int svflags);
void CVar_SetFloat(cvar_t *var, float value);

void CVar_SetString2(cvar_t *var, char const *text, int svflags);
void CVar_SetString(cvar_t *var, char const *text);

void CVar_SetUri2(cvar_t *var, Uri const *uri, int svflags);
void CVar_SetUri(cvar_t *var, Uri const *uri);

typedef enum {
    WT_ANY = -1,
    KNOWNWORDTYPE_FIRST = 0,
    WT_CCMD = KNOWNWORDTYPE_FIRST,
    WT_CVAR,
    WT_CALIAS,
    WT_GAME,
    KNOWNWORDTYPE_COUNT
} knownwordtype_t;

#define VALID_KNOWNWORDTYPE(t)      ((t) >= KNOWNWORDTYPE_FIRST && (t) < KNOWNWORDTYPE_COUNT)

typedef struct knownword_s {
    knownwordtype_t type;
    void *data;
} knownword_t;

typedef struct calias_s {
    /// Name of this alias.
    char *name;

    /// Aliased command string.
    char *command;
} calias_t;

// Console commands can set this when they need to return a custom value
// e.g. for the game library.
DENG_EXTERN_C int CmdReturnValue;
DENG_EXTERN_C byte consoleDump;

void Con_Register();
void Con_DataRegister();

boolean Con_Init();
void Con_Shutdown();

void Con_InitDatabases();
void Con_ClearDatabases();
void Con_ShutdownDatabases();

void Con_Ticker(timespan_t time);

/**
 * Attempt to change the 'open' state of the console.
 * @note In dedicated mode the console cannot be closed.
 */
void Con_Open(int yes);

void Con_AddCommand(ccmdtemplate_t const *cmd);
void Con_AddCommandList(ccmdtemplate_t const *cmdList);

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

void Con_AddVariable(cvartemplate_t const *tpl);
void Con_AddVariableList(cvartemplate_t const *tplList);
cvar_t *Con_FindVariable(char const *path);

/// @return  Type of the variable associated with @a path if found else @c CVT_NULL
cvartype_t Con_GetVariableType(char const *path);

int Con_GetInteger(char const *path);
float Con_GetFloat(char const *path);
byte Con_GetByte(char const *path);
char const *Con_GetString(char const *path);
Uri const *Con_GetUri(char const *path);

void Con_SetInteger2(char const *path, int value, int svflags);
void Con_SetInteger(char const *path, int value);

void Con_SetFloat2(char const *path, float value, int svflags);
void Con_SetFloat(char const *path, float value);

void Con_SetString2(char const *path, char const *text, int svflags);
void Con_SetString(char const *path, char const *text);

void Con_SetUri2(char const *path, Uri const *uri, int svflags);
void Con_SetUri(char const *path, Uri const *uri);

calias_t *Con_AddAlias(char const *name, char const *command);

/**
 * @return  @c 0 if the specified alias can't be found.
 */
calias_t *Con_FindAlias(char const *name);

void Con_DeleteAlias(calias_t *cal);

/**
 * @return  @c true iff @a name matches a known command or alias name.
 */
boolean Con_IsValidCommand(char const *name);

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
int Con_Execute(byte src, char const *command, int silent, boolean netCmd);
int Con_Executef(byte src, int silent, char const *command, ...) PRINTF_F(3,4);

/**
 * Print an error message and quit.
 */
void Con_Error(char const *error, ...);
void Con_AbnormalShutdown(char const *error);

/**
 * Iterate over words in the known-word dictionary, making a callback for each.
 * Iteration ends when all selected words have been visited or a callback returns
 * non-zero.
 *
 * @param pattern           If not @c NULL or an empty string, only process those
 *                          words which match this pattern.
 * @param type              If a valid word type, only process words of this type.
 * @param callback          Callback to make for each processed word.
 * @param parameters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Con_IterateKnownWords(char const *pattern, knownwordtype_t type,
    int (*callback)(knownword_t const *word, void *parameters), void *parameters);

enum KnownWordMatchMode {
    KnownWordExactMatch, // case insensitive
    KnownWordStartsWith  // case insensitive
};

int Con_IterateKnownWords(KnownWordMatchMode matchMode, char const *pattern,
    knownwordtype_t type, int (*callback)(knownword_t const *word, void *parameters),
    void *parameters);

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
knownword_t const **Con_CollectKnownWordsMatchingWord(char const *word,
    knownwordtype_t type, uint *count);

AutoStr *Con_KnownWordToString(knownword_t const *word);

/**
 * Print a 'global' message (to stdout and the console) consisting of (at least)
 * one full line of text.
 *
 * @param message  Message with printf() formatting syntax for arguments.
 *                 The terminating line break character may be omitted, however
 *                 the message cannot be continued in a subsequent call.
 */
void Con_Message(char const *message, ...) PRINTF_F(1,2);

/**
 * Print a text fragment (manual/no line breaks) to the console.
 *
 * @param flags   @ref consolePrintFlags
 * @param format  Format for the output using printf() formatting syntax.
 */
void Con_FPrintf(int flags, char const *format, ...) PRINTF_F(2,3);
void Con_Printf(char const *format, ...) PRINTF_F(1,2);

/// Print a ruler into the console.
void Con_PrintRuler();

void Con_PrintCVar(cvar_t *cvar, char const *prefix);

de::String Con_VarAsStyledText(cvar_t *var, char const *prefix);

de::String Con_CmdAsStyledText(ccmd_t *cmd);

/**
 * Returns a rich formatted, textual representation of the specified console
 * command's argument list, suitable for logging.
 *
 * @param ccmd  The console command to format usage info for.
 */
de::String Con_CmdUsageAsStyledText(ccmd_t *ccmd);

de::String Con_AliasAsStyledText(calias_t *alias);

de::String Con_GameAsStyledText(de::Game *game);

de::String Con_AnnotatedConsoleTerms(QStringList terms);

/**
 * Collects all the known words of the console into a Lexicon.
 */
de::shell::Lexicon Con_Lexicon();

#endif // DENG_CONSOLE_MAIN_H
