/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * con_main.h: Console Subsystem
 */

#ifndef __DOOMSDAY_CONSOLE_MAIN_H__
#define __DOOMSDAY_CONSOLE_MAIN_H__

#include <stdio.h>
#include "dd_share.h"

#define MAX_ARGS	256

typedef struct
{
	char cmdLine[2048];
	int argc;
	char *argv[MAX_ARGS];
} cmdargs_t;

// A console buffer line.
typedef struct
{
	int len;					// This is the length of the line (no term).
	char *text;					// This is the text.
	int flags;
} cbline_t;

// Console commands can set this when they need to return a custom value
// e.g. for the game dll.
extern int		CmdReturnValue;

extern int		consoleAlpha, consoleLight;
extern boolean	consoleDump, consoleShowFPS, consoleShadowText;

void Con_Init();
void Con_Shutdown();
void Con_WriteAliasesToFile(FILE *file);
void Con_MaxLineLength(void);
void Con_Open(int yes);
void Con_AddCommand(ccmd_t *cmd);
void Con_AddVariable(cvar_t *var);
void Con_AddCommandList(ccmd_t *cmdlist);
void Con_AddVariableList(cvar_t *varlist);
ccmd_t *Con_GetCommand(const char *name);
boolean Con_IsValidCommand(const char *name);
boolean Con_IsSpecialChar(int ch);
void Con_UpdateKnownWords(void);
void Con_Ticker(timespan_t time);
boolean Con_Responder(event_t *event);
void Con_Drawer(void);
void Con_DrawRuler(int y, int lineHeight, float alpha);
void Con_Printf(const char *format, ...);
void Con_FPrintf(int flags, const char *format, ...); // Flagged printf.
void Con_SetFont(ddfont_t *cfont);
cbline_t *Con_GetBufferLine(int num);
int Con_Execute(const char *command, int silent);
int Con_Executef(int silent, const char *command, ...);

void	Con_Message(const char *message, ...);
void	Con_Error(const char *error, ...);
cvar_t* Con_GetVariable(const char *name);
int		Con_GetInteger(const char *name);
float	Con_GetFloat(const char *name);
byte	Con_GetByte(const char *name);
char*	Con_GetString(const char *name);

void	Con_SetInteger(const char *name, int value);
void	Con_SetFloat(const char *name, float value);
void	Con_SetString(const char *name, char *text);

char *TrimmedFloat(float val);

#endif
