/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * con_main.h: Console Subsystem
 */

#ifndef __DOOMSDAY_CONSOLE_MAIN_H__
#define __DOOMSDAY_CONSOLE_MAIN_H__

#include <stdio.h>
#include "dd_share.h"
#include "de_system.h"
#include "dd_input.h"

#include "con_buffer.h"

#define CMDLINE_SIZE 256
#define MAX_ARGS    256

#define OBSOLETE        CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console command values through the void ptr.
#define CV_INT(var)     (*(int*) var->ptr)
#define CV_BYTE(var)    (*(byte*) var->ptr)
#define CV_FLOAT(var)   (*(float*) var->ptr)
#define CV_CHARPTR(var) (*(char**) var->ptr)

typedef struct {
    char            cmdLine[2048];
    int             argc;
    char           *argv[MAX_ARGS];
} cmdargs_t;

// Doomsday's internal representation of registered ccmds.
typedef struct {
    const char     *name;
    cvartype_t      params[MAX_ARGS];
    int           (*func) (byte src, int argc, char **argv);
    int             flags;
    int             minArgs, maxArgs;
} ddccmd_t;

typedef enum {
    WT_CCMD,
    WT_CVAR,
    WT_CVARREADONLY,
    WT_ALIAS,
    WT_BINDCONTEXT
} knownwordtype_t;

typedef struct knownword_S {
    char    word[64];           // 64 chars MAX for words.
    knownwordtype_t type;
} knownword_t;

typedef struct calias_s {
    char   *name;
    char   *command;
} calias_t;

// Console commands can set this when they need to return a custom value
// e.g. for the game library.
extern int      CmdReturnValue;
extern ddfont_t Cfont;
extern byte     consoleDump;

void            Con_DataRegister(void);
void            Con_DestroyDatabases(void);

ddccmd_t       *Con_GetCommand(cmdargs_t *args);
void            Con_AddCommand(ccmd_t *cmd);
void            Con_AddVariable(cvar_t *var);
void            Con_AddCommandList(ccmd_t *cmdlist);
void            Con_AddVariableList(cvar_t *varlist);
calias_t       *Con_AddAlias(const char *aName, const char *command);
void            Con_DeleteAlias(calias_t *cal);
void            Con_PrintCVar(cvar_t *var, char *prefix);
boolean         Con_IsValidCommand(const char *name);
void            Con_PrintCCmdUsage(ddccmd_t *ccmd, boolean showExtra);
calias_t       *Con_GetAlias(const char *name);
cvar_t         *Con_GetVariable(const char *name);
cvar_t         *Con_GetVariableIDX(unsigned int idx);
unsigned int    Con_CVarCount(void);
int             Con_GetInteger(const char *name);
float           Con_GetFloat(const char *name);
byte            Con_GetByte(const char *name);
char           *Con_GetString(const char *name);

void            Con_SetInteger(const char *name, int value, byte override);
void            Con_SetFloat(const char *name, float value, byte override);
void            Con_SetString(const char *name, char *text, byte override);

boolean         Con_Init(void);
void            Con_Shutdown(void);
void            Con_Register(void);
void            Con_AbnormalShutdown(const char* message);
void            Con_WriteAliasesToFile(FILE * file);
void            Con_SetMaxLineLength(void);
void            Con_Open(int yes);
boolean         Con_IsActive(void);
boolean         Con_IsLocked(void);
boolean         Con_InputMode(void);
uint            Con_CursorPosition(void);
char           *Con_GetCommandLine(void);
cbuffer_t      *Con_GetConsoleBuffer(void);

boolean         Con_IsSpecialChar(int ch);
void            Con_UpdateKnownWords(void);
knownword_t   **Con_CollectKnownWordsMatchingWord(const char *word,
                                                  unsigned int  *count);
void            Con_Ticker(timespan_t time);
boolean         Con_Responder(ddevent_t *event);
void            Con_Printf(const char *format, ...) PRINTF_F(1,2);
void            Con_FPrintf(int flags, const char *format, ...) PRINTF_F(2,3);    // Flagged printf.
int             Con_PrintFileName(const char *fn, filetype_t type, void *dir);
void            Con_SetFont(ddfont_t *cfont);
float           Con_FontScaleY(void);
cbline_t       *Con_GetBufferLine(cbuffer_t *buffer, int num);
int             Con_Execute(byte src, const char *command, int silent,
                            boolean netCmd);
int             Con_Executef(byte src, int silent, const char *command, ...);

void            Con_Message(const char *message, ...) PRINTF_F(1,2);
void            Con_Error(const char *error, ...) PRINTF_F(1,2);

char           *TrimmedFloat(float val);

#endif
