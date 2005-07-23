/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * con_main.c: Console Subsystem
 *
 * Should be completely redesigned.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>

#include "de_platform.h"

#ifdef WIN32
#  	include <process.h>
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_audio.h"
#include "de_ui.h"
#include "de_misc.h"

#ifdef TextOut
// Windows has its own TextOut.
#  undef TextOut
#endif

// MACROS ------------------------------------------------------------------

#define SC_EMPTY_QUOTE	-1

// Length of the print buffer. Used in Con_Printf. If console messages are
// longer than this, an error will occur.
#define PRBUFF_LEN		8000

#define OBSOLETE		CVF_NO_ARCHIVE|CVF_HIDE

// Macros for accessing the console command values through the void ptr.
#define CV_INT(var)		(*(int*) var->ptr)
#define CV_BYTE(var)	(*(byte*) var->ptr)
#define CV_FLOAT(var)	(*(float*) var->ptr)
#define CV_CHARPTR(var)	(*(char**) var->ptr)

// Operators for the "if" command.
enum {
	IF_EQUAL,
	IF_NOT_EQUAL,
	IF_GREATER,
	IF_LESS,
	IF_GEQUAL,
	IF_LEQUAL
};

// TYPES -------------------------------------------------------------------

typedef struct calias_s {
	char   *name;
	char   *command;
} calias_t;

typedef struct execbuff_s {
	boolean used;				// Is this in use?
	timespan_t when;			// System time when to execute the command.
	char    subCmd[1024];		// A single command w/args.
} execbuff_t;

typedef struct knownword_S {
	char    word[64];			// 64 chars MAX for words.
} knownword_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void registerCommands(void);
static void registerVariables(void);
static int executeSubCmd(const char *subCmd);
void    Con_SplitIntoSubCommands(const char *command, timespan_t markerOffset);
calias_t *Con_GetAlias(const char *name);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean paletted, r_s3tc;	// Use GL_EXT_paletted_texture
extern int freezeRLs;

extern bindclass_t bindClasses[];

//extern cvar_t netCVars[], inputCVars[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

ddfont_t Cfont;					// The console font.

float   CcolYellow[3] = { 1, .85f, .3f };

boolean ConsoleSilent = false;
int     CmdReturnValue = 0;

float   ConsoleOpenY;			// Where the console bottom is when open.

int     consoleTurn;			// The rotation variable.
int     consoleLight = 50, consoleAlpha = 75;
int     conCompMode = 0;		// Completion mode.
int     conSilentCVars = 1;
boolean consoleDump = true;
int     consoleActiveKey = '`';	// Tilde.
boolean consoleShowKeys = false;
boolean consoleShowFPS = false;
boolean consoleShadowText = true;

cvar_t *cvars = NULL;
int     numCVars, maxCVars;

ccmd_t *ccmds = NULL;			// The list of console commands.
int     numCCmds, maxCCmds;		// Number of console commands.

calias_t *caliases = NULL;
int     numCAliases = 0;

knownword_t *knownWords = 0;	// The list of known words (for completion).
int     numKnownWords = 0;

char    trimmedFloatBuffer[32];	// Returned by TrimmedFloat().

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean ConsoleInited;	// Has Con_Init() been called?
static boolean ConsoleActive;	// Is the console active?
static float ConsoleY;			// Where the console bottom is currently?
static float ConsoleDestY;		// Where the console bottom should be?
static timespan_t ConsoleTime;	// How many seconds has the console been open?
static float ConsoleBlink;		// Cursor blink timer (35 Hz tics).

static float funnyAng;
static boolean openingOrClosing = true;

static float fontFx, fontSy;	// Font x factor and y size.
static cbline_t *cbuffer;		// This is the buffer.
int     bufferLines;			// How many lines are there in the buffer?
static int maxBufferLines;		// Maximum number of lines in the buffer.
static int maxLineLen;			// Maximum length of a line.
static int bPos;				// Where the write cursor is? (which line)
static int bFirst;				// The first visible line.
static int bLineOff;			// How many lines from bPos? (+vislines)
static char cmdLine[256];		// The command line.
static int cmdCursor;			// Position of the cursor on the command line.
static cbline_t *oldCmds;		// The old commands buffer.
static int numOldCmds;
static int ocPos;				// Old commands buffer position.
static int complPos;			// Where is the completion cursor?
static int lastCompletion;		// The index of the last completion (in knownWords).

static execbuff_t *exBuff;
static int exBuffSize;
static execbuff_t *curExec;

// CODE --------------------------------------------------------------------

void PrepareCmdArgs(cmdargs_t * cargs, const char *lpCmdLine)
{
	int     i, len = strlen(lpCmdLine);

	// Prepare the data.
	memset(cargs, 0, sizeof(cmdargs_t));
	strcpy(cargs->cmdLine, lpCmdLine);
	// Prepare.
	for(i = 0; i < len; i++)
	{
#define IS_ESC_CHAR(x)	((x) == '"' || (x) == '\\' || (x) == '{' || (x) == '}')
		// Whitespaces are separators.
		if(ISSPACE(cargs->cmdLine[i]))
			cargs->cmdLine[i] = 0;
		if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))	// Escape sequence?
		{
			memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
					sizeof(cargs->cmdLine) - i - 1);
			len--;
			continue;
		}
		if(cargs->cmdLine[i] == '"')	// Find the end.
		{
			int     start = i;

			cargs->cmdLine[i] = 0;
			for(++i; i < len && cargs->cmdLine[i] != '"'; i++)
			{
				if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))	// Escape sequence?
				{
					memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
							sizeof(cargs->cmdLine) - i - 1);
					len--;
					continue;
				}
			}
			// Quote not terminated?
			if(i == len)
				break;
			// An empty set of quotes?
			if(i == start + 1)
				cargs->cmdLine[i] = SC_EMPTY_QUOTE;
			else
				cargs->cmdLine[i] = 0;
		}
		if(cargs->cmdLine[i] == '{')	// Find matching end.
		{
			// Braces are another notation for quotes.
			int     level = 0;
			int     start = i;

			cargs->cmdLine[i] = 0;
			for(++i; i < len; i++)
			{
				if(cargs->cmdLine[i] == '\\' && IS_ESC_CHAR(cargs->cmdLine[i + 1]))	// Escape sequence?
				{
					memmove(cargs->cmdLine + i, cargs->cmdLine + i + 1,
							sizeof(cargs->cmdLine) - i - 1);
					len--;
					i++;
					continue;
				}
				if(cargs->cmdLine[i] == '}')
				{
					if(!level)
						break;
					level--;
				}
				if(cargs->cmdLine[i] == '{')
					level++;
			}
			// Quote not terminated?
			if(i == len)
				break;
			// An empty set of braces?
			if(i == start + 1)
				cargs->cmdLine[i] = SC_EMPTY_QUOTE;
			else
				cargs->cmdLine[i] = 0;
		}
	}
	// Scan through the cmdLine and get the beginning of each token.
	cargs->argc = 0;
	for(i = 0; i < len; i++)
	{
		if(!cargs->cmdLine[i])
			continue;
		// Is this an empty quote?
		if(cargs->cmdLine[i] == SC_EMPTY_QUOTE)
			cargs->cmdLine[i] = 0;	// Just an empty string.
		cargs->argv[cargs->argc++] = cargs->cmdLine + i;
		i += strlen(cargs->cmdLine + i);
	}
}

char   *TrimmedFloat(float val)
{
	char   *ptr = trimmedFloatBuffer;

	sprintf(ptr, "%f", val);
	// Get rid of the extra zeros.
	for(ptr += strlen(ptr) - 1; ptr >= trimmedFloatBuffer; ptr--)
	{
		if(*ptr == '0')
			*ptr = 0;
		else if(*ptr == '.')
		{
			*ptr = 0;
			break;
		}
		else
			break;
	}
	return trimmedFloatBuffer;
}

//--------------------------------------------------------------------------
//
// Console Variable Handling
//
//--------------------------------------------------------------------------

//===========================================================================
// Con_SetString
//===========================================================================
void Con_SetString(const char *name, char *text)
{
	cvar_t *cvar = Con_GetVariable(name);

	if(!cvar)
		return;

	if(cvar->flags & CVF_READ_ONLY)
	{
		Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", name);
		return;
	}

	if(cvar->type == CVT_CHARPTR)
	{
		// Free the old string, if one exists.
		if(cvar->flags & CVF_CAN_FREE && CV_CHARPTR(cvar))
			free(CV_CHARPTR(cvar));
		// Allocate a new string.
		cvar->flags |= CVF_CAN_FREE;
		CV_CHARPTR(cvar) = malloc(strlen(text) + 1);
		strcpy(CV_CHARPTR(cvar), text);
	}
	else
		Con_Error("Con_SetString: cvar is not of type char*.\n");
}

cvar_t *Con_GetVariable(const char *name)
{
	int     i;

	for(i = 0; i < numCVars; i++)
		if(!stricmp(name, cvars[i].name))
			return cvars + i;
	// No match...
	return NULL;
}

//===========================================================================
// Con_SetInteger
//  Also works with bytes.
//===========================================================================
void Con_SetInteger(const char *name, int value)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var)
		return;

	if(var->flags & CVF_READ_ONLY)
	{
		Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", name);
		return;
	}

	if(var->type == CVT_INT)
		CV_INT(var) = value;
	if(var->type == CVT_BYTE)
		CV_BYTE(var) = value;
	if(var->type == CVT_FLOAT)
		CV_FLOAT(var) = value;
}

//===========================================================================
// Con_SetFloat
//===========================================================================
void Con_SetFloat(const char *name, float value)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var)
		return;

	if(var->flags & CVF_READ_ONLY)
	{
		Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", name);
		return;
	}

	if(var->type == CVT_INT)
		CV_INT(var) = (int) value;
	if(var->type == CVT_BYTE)
		CV_BYTE(var) = (byte) value;
	if(var->type == CVT_FLOAT)
		CV_FLOAT(var) = value;
}

//===========================================================================
// Con_GetInteger
//===========================================================================
int Con_GetInteger(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var)
		return 0;
	if(var->type == CVT_BYTE)
		return CV_BYTE(var);
	if(var->type == CVT_FLOAT)
		return CV_FLOAT(var);
	if(var->type == CVT_CHARPTR)
		return strtol(CV_CHARPTR(var), 0, 0);
	return CV_INT(var);
}

//===========================================================================
// Con_GetFloat
//===========================================================================
float Con_GetFloat(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var)
		return 0;
	if(var->type == CVT_INT)
		return CV_INT(var);
	if(var->type == CVT_BYTE)
		return CV_BYTE(var);
	if(var->type == CVT_CHARPTR)
		return strtod(CV_CHARPTR(var), 0);
	return CV_FLOAT(var);
}

//===========================================================================
// Con_GetByte
//===========================================================================
byte Con_GetByte(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var)
		return 0;
	if(var->type == CVT_INT)
		return CV_INT(var);
	if(var->type == CVT_FLOAT)
		return CV_FLOAT(var);
	if(var->type == CVT_CHARPTR)
		return strtol(CV_CHARPTR(var), 0, 0);
	return CV_BYTE(var);
}

//===========================================================================
// Con_GetString
//===========================================================================
char   *Con_GetString(const char *name)
{
	cvar_t *var = Con_GetVariable(name);

	if(!var || var->type != CVT_CHARPTR)
		return "";
	return CV_CHARPTR(var);
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

static int C_DECL wordListSorter(const void *e1, const void *e2)
{
	return stricmp(*(char **) e1, *(char **) e2);
}

static int C_DECL knownWordListSorter(const void *e1, const void *e2)
{
	return stricmp(((knownword_t *) e1)->word, ((knownword_t *) e2)->word);
}

//===========================================================================
// Con_Init
//===========================================================================
void Con_Init()
{
	ConsoleInited = true;

	ConsoleActive = false;
	ConsoleY = 0;
	ConsoleOpenY = 90;
	ConsoleDestY = 0;
	ConsoleTime = 0;

	funnyAng = 0;

	// Font size in VGA coordinates. (Everything is in VGA coords.)
	fontFx = 1;
	fontSy = 9;

	// The buffer.
	cbuffer = 0;
	bufferLines = 0;
	maxBufferLines = 512;		//256;
	maxLineLen = 70;			// Should fit the screen (adjusted later).

	cmdCursor = 0;

	// The old commands buffer.
	oldCmds = 0;
	numOldCmds = 0;
	ocPos = 0;					// No commands yet.

	bPos = 0;
	bFirst = 0;
	bLineOff = 0;

	complPos = 0;
	lastCompletion = -1;

	exBuff = NULL;
	exBuffSize = 0;

	// Register the engine commands and variables.
	registerCommands();
	registerVariables();

    DD_RegisterLoop();
	DD_RegisterInput();
	R_Register();
	Rend_Register();
    GL_Register();
	Net_Register();
	I_Register();
	H_Register();
}

//===========================================================================
// Con_MaxLineLength
//===========================================================================
void Con_MaxLineLength(void)
{
	int     cw = FR_TextWidth("A");

	if(!cw)
	{
		maxLineLen = 70;
		return;
	}
	maxLineLen = screenWidth / cw - 2;
	if(maxLineLen > 250)
		maxLineLen = 250;
}

//===========================================================================
// Con_UpdateKnownWords
//  Variables with CVF_HIDE are not considered known words.
//===========================================================================
void Con_UpdateKnownWords()
{
	int     i, c, known_vars;
	int     len;

	// Count the number of visible console variables.
	for(i = known_vars = 0; i < numCVars; i++)
		if(!(cvars[i].flags & CVF_HIDE))
			known_vars++;

	// Fill the known words table.
	numKnownWords = numCCmds + known_vars + numCAliases + (NUMBINDCLASSES);
	knownWords = realloc(knownWords, len =
						 sizeof(knownword_t) * numKnownWords);
	memset(knownWords, 0, len);

	// Commands, variables, aliases, and bind class names are known words.
	for(i = 0, c = 0; i < numCCmds; i++, c++)
	{
		strncpy(knownWords[c].word, ccmds[i].name, 63);
	}
	for(i = 0; i < numCVars; i++)
	{
		if(!(cvars[i].flags & CVF_HIDE))
			strncpy(knownWords[c++].word, cvars[i].name, 63);
	}
	for(i = 0; i < numCAliases; i++, c++)
	{
		strncpy(knownWords[c].word, caliases[i].name, 63);
	}
	for(i = 0; i < NUMBINDCLASSES; i++, c++)
	{
		strncpy(knownWords[c].word, bindClasses[i].name, 63);
	}

	// Sort it so we get nice alphabetical word completions.
	qsort(knownWords, numKnownWords, sizeof(knownword_t), knownWordListSorter);
}

void Con_AddCommandList(ccmd_t *cmdlist)
{
	for(; cmdlist->name; cmdlist++)
		Con_AddCommand(cmdlist);
}

void Con_AddCommand(ccmd_t *cmd)
{
	if(++numCCmds > maxCCmds)
	{
		maxCCmds *= 2;
		if(maxCCmds < numCCmds)
			maxCCmds = numCCmds;
		ccmds = realloc(ccmds, sizeof(ccmd_t) * maxCCmds);
	}
	memcpy(ccmds + numCCmds - 1, cmd, sizeof(ccmd_t));

	// Sort them.
	qsort(ccmds, numCCmds, sizeof(ccmd_t), wordListSorter);
}

/*
 * Returns a pointer to the ccmd_t with the specified name, or NULL.
 */
ccmd_t *Con_GetCommand(const char *name)
{
	int     i;

	for(i = 0; i < numCCmds; i++)
	{
		if(!stricmp(ccmds[i].name, name))
			return &ccmds[i];
	}
	return NULL;
}

/*
 * Returns true if the given string is a valid command or alias.
 */
boolean Con_IsValidCommand(const char *name)
{
	return Con_GetCommand(name) || Con_GetAlias(name);
}

void Con_AddVariableList(cvar_t *varlist)
{
	for(; varlist->name; varlist++)
		Con_AddVariable(varlist);
}

void Con_AddVariable(cvar_t *var)
{
	if(++numCVars > maxCVars)
	{
		// Allocate more memory.
		maxCVars *= 2;
		if(maxCVars < numCVars)
			maxCVars = numCVars;
		cvars = realloc(cvars, sizeof(cvar_t) * maxCVars);
	}
	memcpy(cvars + numCVars - 1, var, sizeof(cvar_t));

	// Sort them.
	qsort(cvars, numCVars, sizeof(cvar_t), wordListSorter);
}

// Returns NULL if the specified alias can't be found.
calias_t *Con_GetAlias(const char *name)
{
	int     i;

	// Try to find the alias.
	for(i = 0; i < numCAliases; i++)
		if(!stricmp(caliases[i].name, name))
			return caliases + i;
	return NULL;
}

//===========================================================================
// Con_Alias
//  Create an alias.
//===========================================================================
void Con_Alias(char *aName, char *command)
{
	calias_t *cal = Con_GetAlias(aName);
	boolean remove = false;

	// Will we remove this alias?
	if(command == NULL)
		remove = true;
	else if(command[0] == 0)
		remove = true;

	if(cal && remove)
	{
		// This alias will be removed.
		int     idx = cal - caliases;

		free(cal->name);
		free(cal->command);
		if(idx < numCAliases - 1)
			memmove(caliases + idx, caliases + idx + 1,
					sizeof(calias_t) * (numCAliases - idx - 1));
		caliases = realloc(caliases, sizeof(calias_t) * --numCAliases);
		// We're done.
		return;
	}

	// Does the alias already exist?
	if(cal)
	{
		cal->command = realloc(cal->command, strlen(command) + 1);
		strcpy(cal->command, command);
		return;
	}

	// We need to create a new alias.
	caliases = realloc(caliases, sizeof(calias_t) * (++numCAliases));
	cal = caliases + numCAliases - 1;
	// Allocate memory for them.
	cal->name = malloc(strlen(aName) + 1);
	cal->command = malloc(strlen(command) + 1);
	strcpy(cal->name, aName);
	strcpy(cal->command, command);
	//  cal->refcount = 0;

	// Sort them.
	qsort(caliases, numCAliases, sizeof(calias_t), wordListSorter);

	Con_UpdateKnownWords();
}

//===========================================================================
// Con_WriteAliasesToFile
//  Called by the config file writer.
//===========================================================================
void Con_WriteAliasesToFile(FILE * file)
{
	int     i;
	calias_t *cal;

	for(i = 0, cal = caliases; i < numCAliases; i++, cal++)
	{
		fprintf(file, "alias \"");
		M_WriteTextEsc(file, cal->name);
		fprintf(file, "\" \"");
		M_WriteTextEsc(file, cal->command);
		fprintf(file, "\"\n");
	}
}

void Con_ClearBuffer()
{
	int     i;

	// Free the buffer.
	for(i = 0; i < bufferLines; i++)
		free(cbuffer[i].text);
	free(cbuffer);
	cbuffer = 0;
	bufferLines = 0;
	bPos = 0;
	bFirst = 0;
	bLineOff = 0;
}

// Send a console command to the server.
// This shouldn't be called unless we're logged in with the right password.
void Con_Send(const char *command, int silent)
{
	unsigned short len = strlen(command) + 1;

	Msg_Begin(pkt_command);
	// Mark high bit for silent commands.
	Msg_WriteShort(len | (silent ? 0x8000 : 0));
	Msg_Write(command, len);
	// Send it reliably.
	Net_SendBuffer(0, SPF_ORDERED);
}

void Con_ClearExecBuffer()
{
	free(exBuff);
	exBuff = NULL;
	exBuffSize = 0;
}

void Con_QueueCmd(const char *singleCmd, timespan_t atSecond)
{
	execbuff_t *ptr = NULL;
	int     i;

	// Look for an empty spot.
	for(i = 0; i < exBuffSize; i++)
		if(!exBuff[i].used)
		{
			ptr = exBuff + i;
			break;
		}

	if(ptr == NULL)
	{
		// No empty places, allocate a new one.
		exBuff = realloc(exBuff, sizeof(execbuff_t) * ++exBuffSize);
		ptr = exBuff + exBuffSize - 1;
	}
	ptr->used = true;
	strcpy(ptr->subCmd, singleCmd);
	ptr->when = atSecond;
}

void Con_Shutdown()
{
	int     i, k;
	char   *ptr;

	// Free the buffer.
	Con_ClearBuffer();

	// Free the old commands.
	for(i = 0; i < numOldCmds; i++)
		free(oldCmds[i].text);
	free(oldCmds);
	oldCmds = 0;
	numOldCmds = 0;

	free(knownWords);
	knownWords = 0;
	numKnownWords = 0;

	// Free the data of the data cvars.
	for(i = 0; i < numCVars; i++)
		if(cvars[i].flags & CVF_CAN_FREE && cvars[i].type == CVT_CHARPTR)
		{
			ptr = *(char **) cvars[i].ptr;
			// Multiple vars could be using the same pointer,
			// make sure it gets freed only once.
			for(k = i; k < numCVars; k++)
				if(cvars[k].type == CVT_CHARPTR &&
				   ptr == *(char **) cvars[k].ptr)
				{
					cvars[k].flags &= ~CVF_CAN_FREE;
				}
			free(ptr);
		}
	free(cvars);
	cvars = NULL;
	numCVars = 0;

	free(ccmds);
	ccmds = NULL;
	numCCmds = 0;

	// Free the alias data.
	for(i = 0; i < numCAliases; i++)
	{
		free(caliases[i].command);
		free(caliases[i].name);
	}
	free(caliases);
	caliases = NULL;
	numCAliases = 0;

	Con_ClearExecBuffer();
}

/*
 * The execbuffer is used to schedule commands for later.
 * Returns false if an executed command fails.
 */
boolean Con_CheckExecBuffer(void)
{
	boolean allDone;
	boolean ret = true;
	int     i, count = 0;
	char    storage[256];

	do							// We'll keep checking until all is done.
	{
		allDone = true;

		// Execute the commands marked for this or a previous tic.
		for(i = 0; i < exBuffSize; i++)
		{
			execbuff_t *ptr = exBuff + i;

			if(!ptr->used || ptr->when > sysTime)
				continue;
			// We'll now execute this command.
			curExec = ptr;
			ptr->used = false;
			strcpy(storage, ptr->subCmd);
			if(!executeSubCmd(storage))
				ret = false;
			allDone = false;
		}

		if(count++ > 100)
		{
			Con_Message("Console execution buffer overflow! "
						"Everything canceled.\n");
			Con_ClearExecBuffer();
			break;
		}
	}
	while(!allDone);
	return ret;
}

void Con_Ticker(timespan_t time)
{
	float   step = time * 35;

	Con_CheckExecBuffer();

	if(ConsoleY == 0)
		openingOrClosing = true;

	// Move the console to the destination Y.
	if(ConsoleDestY > ConsoleY)
	{
		float   diff = (ConsoleDestY - ConsoleY) / 4;

		if(diff < 1)
			diff = 1;
		ConsoleY += diff * step;
		if(ConsoleY > ConsoleDestY)
			ConsoleY = ConsoleDestY;
	}
	else if(ConsoleDestY < ConsoleY)
	{
		float   diff = (ConsoleY - ConsoleDestY) / 4;

		if(diff < 1)
			diff = 1;
		ConsoleY -= diff * step;
		if(ConsoleY < ConsoleDestY)
			ConsoleY = ConsoleDestY;
	}

	if(ConsoleY == ConsoleOpenY)
		openingOrClosing = false;

	funnyAng += step * consoleTurn / 10000;

	if(!ConsoleActive)
		return;					// We have nothing further to do here.

	ConsoleTime += time;		// Increase the ticker.
	ConsoleBlink += step;		// Cursor blink timer (0 = visible).
}

cbline_t *Con_GetBufferLine(int num)
{
	int     i, newLines;

	if(num < 0)
		return 0;				// This is unacceptable!
	// See if we already have that line.
	if(num < bufferLines)
		return cbuffer + num;
	// Then we'll have to allocate more lines. Usually just one, though.
	newLines = num + 1 - bufferLines;
	bufferLines += newLines;
	cbuffer = realloc(cbuffer, sizeof(cbline_t) * bufferLines);
	for(i = 0; i < newLines; i++)
	{
		cbline_t *line = cbuffer + i + bufferLines - newLines;

		memset(line, 0, sizeof(cbline_t));
	}
	return cbuffer + num;
}

static void addLineText(cbline_t * line, char *txt)
{
	int     newLen = line->len + strlen(txt);

	if(newLen > maxLineLen)
		//Con_Error("addLineText: Too long console line.\n");
		return;					// Can't do anything.

	// Allocate more memory.
	line->text = realloc(line->text, newLen + 1);
	// Copy the new text to the appropriate location.
	strcpy(line->text + line->len, txt);
	// Update the length of the line.
	line->len = newLen;
}

/*
   static void setLineFlags(int num, int fl)
   {
   cbline_t *line = Con_GetBufferLine(num);

   if(!line) return;
   line->flags = fl;
   }
 */

static void addOldCmd(const char *txt)
{
	cbline_t *line;

	if(!strcmp(txt, ""))
		return;					// Don't add empty commands.

	// Allocate more memory.
	oldCmds = realloc(oldCmds, sizeof(cbline_t) * ++numOldCmds);
	line = oldCmds + numOldCmds - 1;
	// Clear the line.
	memset(line, 0, sizeof(cbline_t));
	line->len = strlen(txt);
	line->text = malloc(line->len + 1);	// Also room for the zero in the end.
	strcpy(line->text, txt);
}

static void printcvar(cvar_t *var, char *prefix)
{
	char    equals = '=';

	if(var->flags & CVF_PROTECTED || var->flags & CVF_READ_ONLY)
		equals = ':';

	Con_Printf(prefix);
	switch (var->type)
	{
	case CVT_NULL:
		Con_Printf("%s", var->name);
		break;
	case CVT_BYTE:
		Con_Printf("%s %c %d", var->name, equals, CV_BYTE(var));
		break;
	case CVT_INT:
		Con_Printf("%s %c %d", var->name, equals, CV_INT(var));
		break;
	case CVT_FLOAT:
		Con_Printf("%s %c %g", var->name, equals, CV_FLOAT(var));
		break;
	case CVT_CHARPTR:
		Con_Printf("%s %c %s", var->name, equals, CV_CHARPTR(var));
		break;
	default:
		Con_Printf("%s (bad type!)", var->name);
		break;
	}
	Con_Printf("\n");
}

/*
 * expandWithArguments
 *  expCommand gets reallocated in the expansion process.
 *  This could be bit more clever.
 */
static void expandWithArguments(char **expCommand, cmdargs_t * args)
{
	char   *text = *expCommand;
	int     size = strlen(text) + 1;
	int     i, p, off;

	for(i = 0; text[i]; i++)
	{
		if(text[i] == '%' && (text[i + 1] >= '1' && text[i + 1] <= '9'))
		{
			char   *substr;
			int     aidx = text[i + 1] - '1' + 1;

			// Expand! (or delete)
			if(aidx > args->argc - 1)
				substr = "";
			else
				substr = args->argv[aidx];

			// First get rid of the %n.
			memmove(text + i, text + i + 2, size - i - 2);
			// Reallocate.
			off = strlen(substr);
			text = *expCommand = realloc(*expCommand, size += off - 2);
			if(off)
			{
				// Make room for the insert.
				memmove(text + i + off, text + i, size - i - off);
				memcpy(text + i, substr, off);
			}
			i += off - 1;
		}
		else if(text[i] == '%' && text[i + 1] == '0')
		{
			// First get rid of the %0.
			memmove(text + i, text + i + 2, size - i - 2);
			text = *expCommand = realloc(*expCommand, size -= 2);
			for(p = args->argc - 1; p > 0; p--)
			{
				off = strlen(args->argv[p]) + 1;
				text = *expCommand = realloc(*expCommand, size += off);
				memmove(text + i + off, text + i, size - i - off);
				text[i] = ' ';
				memcpy(text + i + 1, args->argv[p], off - 1);
			}
		}
	}
}

/*
 * executeSubCmd
 *  The command is executed forthwith!!
 */
static int executeSubCmd(const char *subCmd)
{
	int     i;
	char    prefix;
	cmdargs_t args;

	PrepareCmdArgs(&args, subCmd);
	if(!args.argc)
		return true;

	if(args.argc == 1)			// An action?
	{
		prefix = args.argv[0][0];
		if(prefix == '+' || prefix == '-')
		{
			return Con_ActionCommand(args.argv[0], true);
		}
		// What about a prefix-less action?
		if(strlen(args.argv[0]) <= 8 && Con_ActionCommand(args.argv[0], false))
			return true;		// There was one!
	}

	// If logged in, send command to server at this point.
	if(netLoggedIn)
	{
		// We have logged in on the server. Send the command there.
		Con_Send(subCmd, ConsoleSilent);
		return true;
	}

	// Try to find a matching command.
	for(i = 0; i < numCCmds; i++)
		if(!stricmp(ccmds[i].name, args.argv[0]))
		{
			int     cret = ccmds[i].func(args.argc, args.argv);

			if(cret == false)
				Con_Printf("Error: '%s' failed.\n", ccmds[i].name);
			// We're quite done.
			return cret;
		}
	// Then try the cvars?
	for(i = 0; i < numCVars; i++)
		if(!stricmp(cvars[i].name, args.argv[0]))
		{
			cvar_t *var = cvars + i;
			boolean out_of_range = false, setting = false;

			if(args.argc == 2 ||
			   (args.argc == 3 && !stricmp(args.argv[1], "force")))
			{
				char   *argptr = args.argv[args.argc - 1];
				boolean forced = args.argc == 3;

				setting = true;
				if(var->flags & CVF_PROTECTED && !forced)
				{
					Con_Printf
						("%s is protected. You shouldn't change its value.\n",
						 var->name);
					Con_Printf
						("Use the command: '%s force %s' to modify it anyway.\n",
						 var->name, argptr);
				}
				else if(var->flags & CVF_READ_ONLY)
				{
					Con_Printf("%s is read-only. It can't be changed (not even with force)\n", var->name);
				}
				else if(var->type == CVT_BYTE)
				{
					byte    val = (byte) strtol(argptr, NULL, 0);

					if(!forced &&
					   ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_BYTE(var) = val;
				}
				else if(var->type == CVT_INT)
				{
					int     val = strtol(argptr, NULL, 0);

					if(!forced &&
					   ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_INT(var) = val;
				}
				else if(var->type == CVT_FLOAT)
				{
					float   val = strtod(argptr, NULL);

					if(!forced &&
					   ((!(var->flags & CVF_NO_MIN) && val < var->min) ||
						(!(var->flags & CVF_NO_MAX) && val > var->max)))
						out_of_range = true;
					else
						CV_FLOAT(var) = val;
				}
				else if(var->type == CVT_CHARPTR)
				{
					Con_SetString(var->name, argptr);
				}
			}
			if(out_of_range)
			{
				if(!(var->flags & (CVF_NO_MIN | CVF_NO_MAX)))
				{
					char    temp[20];

					strcpy(temp, TrimmedFloat(var->min));
					Con_Printf("Error: %s <= %s <= %s\n", temp, var->name,
							   TrimmedFloat(var->max));
				}
				else if(var->flags & CVF_NO_MAX)
				{
					Con_Printf("Error: %s >= %s\n", var->name,
							   TrimmedFloat(var->min));
				}
				else
				{
					Con_Printf("Error: %s <= %s\n", var->name,
							   TrimmedFloat(var->max));
				}
			}
			else if(!setting || !conSilentCVars)	// Show the value.
			{
				printcvar(var, "");
			}
			return true;
		}

	// How about an alias then?
	for(i = 0; i < numCAliases; i++)
		if(!stricmp(args.argv[0], caliases[i].name))
		{
			calias_t *cal = caliases + i;
			char   *expCommand;

			// Expand the command with arguments.
			expCommand = malloc(strlen(cal->command) + 1);
			strcpy(expCommand, cal->command);
			expandWithArguments(&expCommand, &args);
			// Do it, man!
			Con_SplitIntoSubCommands(expCommand, 0);
			free(expCommand);
			return true;
		}

	// What *is* that?
	Con_Printf("%s: no such command or variable.\n", args.argv[0]);
	return false;
}

/*
 * Splits the command into subcommands and queues them into the 
 * execution buffer.
 */
void Con_SplitIntoSubCommands(const char *command, timespan_t markerOffset)
{
	int     gPos = 0, scPos = 0;
	char    subCmd[2048];
	int     inQuotes = false, escape = false;

	// Is there a command to execute?
	if(!command || command[0] == 0)
		return;

	// Jump over initial semicolons.
	while(command[gPos] == ';' && command[gPos] != 0)
		gPos++;

	// The command may actually contain many commands, separated
	// with semicolons. This isn't a very clear algorithm...
	for(strcpy(subCmd, ""); command[gPos];)
	{
		escape = false;
		if(inQuotes && command[gPos] == '\\')	// Escape sequence?
		{
			subCmd[scPos++] = command[gPos++];
			escape = true;
		}
		if(command[gPos] == '"' && !escape)
			inQuotes = !inQuotes;

		// Collect characters.
		subCmd[scPos++] = command[gPos++];
		if(subCmd[0] == ' ')
			scPos = 0;			// No spaces in the beginning.

		if((command[gPos] == ';' && !inQuotes) || command[gPos] == 0)
		{
			while(command[gPos] == ';' && command[gPos] != 0)
				gPos++;
			// The subcommand ends.
			subCmd[scPos] = 0;
		}
		else
			continue;

		// Queue it.
		Con_QueueCmd(subCmd, sysTime + markerOffset);

		scPos = 0;
	}
}

//===========================================================================
// Con_Execute
//  Returns false if a command fails.
//===========================================================================
int Con_Execute(const char *command, int silent)
{
	int     ret;

	if(silent)
		ConsoleSilent = true;

	Con_SplitIntoSubCommands(command, 0);
	ret = Con_CheckExecBuffer();

	if(silent)
		ConsoleSilent = false;

	return ret;
}

//===========================================================================
// Con_Executef
//===========================================================================
int Con_Executef(int silent, const char *command, ...)
{
	va_list argptr;
	char    buffer[4096];

	va_start(argptr, command);
	vsprintf(buffer, command, argptr);
	va_end(argptr);
	return Con_Execute(buffer, silent);
}

static void processCmd()
{
	DD_ClearKeyRepeaters();

	// Add the command line to the oldCmds buffer.
	addOldCmd(cmdLine);
	ocPos = numOldCmds;

	Con_Execute(cmdLine, false);
}

static void updateCmdLine()
{
	if(ocPos == numOldCmds)
		strcpy(cmdLine, "");
	else
		strcpy(cmdLine, oldCmds[ocPos].text);
	cmdCursor = complPos = strlen(cmdLine);
	lastCompletion = -1;
	if(isDedicated)
		Sys_ConUpdateCmdLine(cmdLine);
}

//===========================================================================
// stramb
//  Ambiguous string check. 'amb' is cut at the first character that 
//  differs when compared to 'str' (case ignored).
//===========================================================================
void stramb(char *amb, const char *str)
{
	while(*str && tolower((unsigned) *amb) == tolower((unsigned) *str))
	{
		amb++;
		str++;
	}
	*amb = 0;
}

//===========================================================================
// completeWord
//  Look at the last word and try to complete it. If there are
//  several possibilities, print them.
//===========================================================================
static void completeWord(void)
{
	int     pass, i, c, cp = strlen(cmdLine) - 1;
	int     numcomp = 0;
	char    word[100], *wordBegin;
	char    unambiguous[256];
	char   *completion = 0;		// Pointer to the completed word.
	cvar_t *cvar;

	if(conCompMode == 1)
		cp = complPos - 1;
	memset(unambiguous, 0, sizeof(unambiguous));

	if(cp < 0)
		return;

	// Skip over any whitespace behind the cursor.
	while(cp > 0 && cmdLine[cp] == ' ')
		cp--;

	// Rewind the word pointer until space or a semicolon is found.
	while(cp > 0 && cmdLine[cp - 1] != ' ' && cmdLine[cp - 1] != ';' &&
		  cmdLine[cp - 1] != '"')
		cp--;

	// Now cp is at the beginning of the word that needs completing.
	strcpy(word, wordBegin = cmdLine + cp);

	if(conCompMode == 1)
		word[complPos - cp] = 0;	// Only check a partial word.

	// The completions we know are the cvars and ccmds.
	for(pass = 1; pass <= 2; pass++)
	{
		if(pass == 2)			// Print the possible completions.
			Con_Printf("Completions:\n");

		// Look through the known words.
		for(c = 0, i = (conCompMode == 0 ? 0 : (lastCompletion + 1));
			c < numKnownWords; c++, i++)
		{
			if(i > numKnownWords - 1)
				i = 0;
			if(!strnicmp(knownWords[i].word, word, strlen(word)))
			{
				// This matches!
				if(!unambiguous[0])
					strcpy(unambiguous, knownWords[i].word);
				else
					stramb(unambiguous, knownWords[i].word);
				// Pass #1: count completions, update completion.
				if(pass == 1)
				{
					numcomp++;
					completion = knownWords[i].word;
					if(conCompMode == 1)
					{
						lastCompletion = i;
						break;
					}
				}
				else			// Pass #2: print it.
				{
					// Print the value of all cvars.
					if((cvar = Con_GetVariable(knownWords[i].word)) != NULL)
						printcvar(cvar, "  ");
					else
						Con_Printf("  %s\n", knownWords[i].word);
				}
			}
		}
		if(numcomp <= 1 || conCompMode == 1)
			break;
	}

	// Was a single completion found?
	if(numcomp == 1)
	{
		strcpy(wordBegin, completion);
		strcat(wordBegin, " ");
		cmdCursor = strlen(cmdLine);
	}
	else if(numcomp > 1)
	{
		// More than one completion; only complete the unambiguous part.
		strcpy(wordBegin, unambiguous);
		cmdCursor = strlen(cmdLine);
	}
}

/*
 * Returns true if the event is eaten.
 */
boolean Con_Responder(event_t *event)
{
	byte    ch;

	if(consoleShowKeys && event->type == ev_keydown)
	{
		Con_Printf("Keydown: ASCII %i (0x%x)\n", event->data1, event->data1);
	}

	// Special console key: Shift-Escape opens the Control Panel.
	if(shiftDown && event->type == ev_keydown && event->data1 == DDKEY_ESCAPE)
	{
		Con_Execute("panel", true);
		return true;
	}

	if(!ConsoleActive)
	{
		// In this case we are only interested in the activation key.
		if(event->type == ev_keydown &&
		   event->data1 == consoleActiveKey /* && !MenuActive */ )
		{
			Con_Open(true);
			return true;
		}
		return false;
	}

	// All keyups are eaten by the console.
	if(event->type == ev_keyup)
		return true;

	// We only want keydown events.
	if(event->type != ev_keydown && event->type != ev_keyrepeat)
		return false;

	// In this case the console is active and operational.
	// Check the shutdown key.
	if(event->data1 == consoleActiveKey)
	{
		if(shiftDown)			// Shift-Tilde to fullscreen and halfscreen.
			ConsoleDestY = ConsoleOpenY = (ConsoleDestY == 200 ? 100 : 200);
		else
			Con_Open(false);
		return true;
	}

	// Hitting Escape in the console closes it.
	if(event->data1 == DDKEY_ESCAPE)
	{
		Con_Open(false);
		return false;			// Let the menu know about this.
	}

	switch (event->data1)
	{
	case DDKEY_UPARROW:
		if(--ocPos < 0)
			ocPos = 0;
		// Update the command line.
		updateCmdLine();
		return true;

	case DDKEY_DOWNARROW:
		if(++ocPos > numOldCmds)
			ocPos = numOldCmds;
		updateCmdLine();
		return true;

	case DDKEY_PGUP:
		bLineOff += 2;
		if(bLineOff > bPos - 1)
			bLineOff = bPos - 1;
		return true;

	case DDKEY_PGDN:
		bLineOff -= 2;
		if(bLineOff < 0)
			bLineOff = 0;
		return true;

	case DDKEY_INS:
		ConsoleOpenY -= fontSy * (shiftDown ? 3 : 1);
		if(ConsoleOpenY < fontSy)
			ConsoleOpenY = fontSy;
		ConsoleDestY = ConsoleOpenY;
		return true;

	case DDKEY_DEL:
		ConsoleOpenY += fontSy * (shiftDown ? 3 : 1);
		if(ConsoleOpenY > 200)
			ConsoleOpenY = 200;
		ConsoleDestY = ConsoleOpenY;
		return true;

	case DDKEY_END:
		bLineOff = 0;
		return true;

	case DDKEY_HOME:
		bLineOff = bPos - 1;
		return true;

	case DDKEY_ENTER:
		// Print the command line with yellow text.
		Con_FPrintf(CBLF_YELLOW, ">%s\n", cmdLine);
		// Process the command line.
		processCmd();
		// Clear it.
		memset(cmdLine, 0, sizeof(cmdLine));
		cmdCursor = 0;
		complPos = 0;
		lastCompletion = -1;
		ConsoleBlink = 0;
		if(isDedicated)
			Sys_ConUpdateCmdLine(cmdLine);
		return true;

	case DDKEY_BACKSPACE:
		if(cmdCursor > 0)
		{
			memmove(cmdLine + cmdCursor - 1, cmdLine + cmdCursor,
					sizeof(cmdLine) - cmdCursor);
			--cmdCursor;
			complPos = cmdCursor;
			lastCompletion = -1;
			ConsoleBlink = 0;
			if(isDedicated)
				Sys_ConUpdateCmdLine(cmdLine);
		}
		return true;

	case DDKEY_TAB:
		completeWord();
		ConsoleBlink = 0;
		if(isDedicated)
			Sys_ConUpdateCmdLine(cmdLine);
		return true;

	case DDKEY_LEFTARROW:

		if(cmdCursor > 0)
		{
			if(shiftDown)
				cmdCursor = 0;
			else
				--cmdCursor;
		}
		complPos = cmdCursor;
		ConsoleBlink = 0;
		break;

	case DDKEY_RIGHTARROW:
		if(cmdLine[cmdCursor] != 0 && cmdCursor < maxLineLen)
		{
			if(shiftDown)
				cmdCursor = strlen(cmdLine);
			else
				++cmdCursor;
		}
		complPos = cmdCursor;
		ConsoleBlink = 0;
		break;

	case DDKEY_F5:
		Con_Execute("clear", true);
		break;

	default:					// Check for a character.
		ch = event->data1;
		ch = DD_ModKey(ch);
		if(ch < 32 || (ch > 127 && ch < DD_HIGHEST_KEYCODE))
			return true;

		if(ch == 'c' && altDown) // if alt + c clear the current cmdline
		{
			memset(cmdLine, 0, sizeof(cmdLine));
			cmdCursor = 0;
			complPos = 0;
			lastCompletion = -1;
			ConsoleBlink = 0;
			if(isDedicated)
				Sys_ConUpdateCmdLine(cmdLine);
			return true;
		}

		if(cmdCursor < maxLineLen)
		{
			// Push the rest of the stuff forwards.
			memmove(cmdLine + cmdCursor + 1, cmdLine + cmdCursor,
					sizeof(cmdLine) - cmdCursor - 1);

			// The last char is always zero, though.
			cmdLine[sizeof(cmdLine) - 1] = 0;
		}
		cmdLine[cmdCursor] = ch;
		if(cmdCursor < maxLineLen)
			++cmdCursor;
		complPos = cmdCursor;	//strlen(cmdLine);
		lastCompletion = -1;
		ConsoleBlink = 0;

		if(isDedicated)
			Sys_ConUpdateCmdLine(cmdLine);
		return true;
	}
	// The console is very hungry for keys...
	return true;
}

static void consoleSetColor(int fl, float alpha)
{
	float   r = 0, g = 0, b = 0;
	int     count = 0;

	// Calculate the average of the given colors.
	if(fl & CBLF_BLACK)
	{
		count++;
	}
	if(fl & CBLF_BLUE)
	{
		b += 1;
		count++;
	}
	if(fl & CBLF_GREEN)
	{
		g += 1;
		count++;
	}
	if(fl & CBLF_CYAN)
	{
		g += 1;
		b += 1;
		count++;
	}
	if(fl & CBLF_RED)
	{
		r += 1;
		count++;
	}
	if(fl & CBLF_MAGENTA)
	{
		r += 1;
		b += 1;
		count++;
	}
	if(fl & CBLF_YELLOW)
	{
		r += CcolYellow[0];
		g += CcolYellow[1];
		b += CcolYellow[2];
		count++;
	}
	if(fl & CBLF_WHITE)
	{
		r += 1;
		g += 1;
		b += 1;
		count++;
	}
	// Calculate the average.
	if(count)
	{
		r /= count;
		g /= count;
		b /= count;
	}
	if(fl & CBLF_LIGHT)
	{
		r += (1 - r) / 2;
		g += (1 - g) / 2;
		b += (1 - b) / 2;
	}
	gl.Color4f(r, g, b, alpha);
}

void Con_SetFont(ddfont_t *cfont)
{
	Cfont = *cfont;
}

//===========================================================================
// Con_DrawRuler2
//===========================================================================
void Con_DrawRuler2(int y, int lineHeight, float alpha, int scrWidth)
{
	int     xoff = 5;
	int     rh = 6;

	UI_GradientEx(xoff, y + (lineHeight - rh) / 2 + 1, scrWidth - 2 * xoff, rh,
				  rh / 2, UI_COL(UIC_SHADOW), UI_COL(UIC_BG_DARK), alpha / 3,
				  alpha);
	UI_DrawRectEx(xoff, y + (lineHeight - rh) / 2 + 1, scrWidth - 2 * xoff, rh, rh / 2, false, UI_COL(UIC_TEXT), NULL,	/*UI_COL(UIC_BG_DARK), UI_COL(UIC_BG_LIGHT), */
				  alpha, -1);
}

//===========================================================================
// Con_DrawRuler
//===========================================================================
void Con_DrawRuler(int y, int lineHeight, float alpha)
{
	Con_DrawRuler2(y, lineHeight, alpha, screenWidth);
}

/*
 * Draw a 'side' text in the console. This is intended for extra
 * information about the current game mode.
 */
void Con_DrawSideText(const char *text, int line, float alpha)
{
	char    buf[300];
	float   gtosMulY = screenHeight / 200.0f;
	float   fontScaledY = Cfont.height * Cfont.sizeY;
	float   y = ConsoleY * gtosMulY - fontScaledY * (1 + line);
	int     ssw;

	if(y > -fontScaledY)
	{
		// The side text is a bit transparent.
		alpha *= .75f;

		// scaled screen width      
		ssw = (int) (screenWidth / Cfont.sizeX);

		// Print the version.
		strncpy(buf, text, sizeof(buf));
		if(Cfont.Filter)
			Cfont.Filter(buf);

		if(consoleShadowText)
		{
			// Draw a shadow.
			gl.Color4f(0, 0, 0, alpha);
			Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 2,
						  y / Cfont.sizeY + 1);
		}
		gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], alpha);
		Cfont.TextOut(buf, ssw - Cfont.Width(buf) - 3, y / Cfont.sizeY);
	}
}

//===========================================================================
// Con_Drawer
//  Slightly messy...
//===========================================================================
void Con_Drawer(void)
{
	int     i, k;				// Line count and buffer cursor.
	float   x, y;
	float   closeFade = 1;
	float   gtosMulY = screenHeight / 200.0f;
	char    buff[256], temp[256];
	float   fontScaledY;
	int     bgX = 64, bgY = 64;

	if(ConsoleY == 0)
		return;					// We have nothing to do here.

	// Do we have a font?
	if(Cfont.TextOut == NULL)
	{
		Cfont.flags = DDFONT_WHITE;
		Cfont.height = FR_TextHeight("Con");
		Cfont.sizeX = 1;
		Cfont.sizeY = 1;
		Cfont.TextOut = FR_TextOut;
		Cfont.Width = FR_TextWidth;
		Cfont.Filter = NULL;
	}

	fontScaledY = Cfont.height * Cfont.sizeY;
	fontSy = fontScaledY / gtosMulY;

	// Go into screen projection mode.
	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	BorderNeedRefresh = true;

	if(openingOrClosing)
	{
		closeFade = ConsoleY / (float) ConsoleOpenY;
	}

	// The console is composed of two parts: the main area background and the 
	// border.
	gl.Color4f(consoleLight / 100.0f, consoleLight / 100.0f,
			   consoleLight / 100.0f, closeFade * consoleAlpha / 100);

	// The background.
	if(gx.ConsoleBackground)
		gx.ConsoleBackground(&bgX, &bgY);

	// Let's make it a bit more interesting.
	gl.MatrixMode(DGL_TEXTURE);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Translatef(2 * sin(funnyAng / 4), 2 * cos(funnyAng / 4), 0);
	gl.Rotatef(funnyAng * 3, 0, 0, 1);
	GL_DrawRectTiled(0, (int) ConsoleY * gtosMulY + 4, screenWidth,
					 -screenHeight - 4, bgX, bgY);
	gl.PopMatrix();

	// The border.
	GL_DrawRect(0, (int) ConsoleY * gtosMulY + 3, screenWidth, 2, 0, 0, 0,
				closeFade);

	// Subtle shadow.
	gl.Begin(DGL_QUADS);
	gl.Color4f(.1f, .1f, .1f, closeFade * consoleAlpha / 150);
	gl.Vertex2f(0, (int) ConsoleY * gtosMulY + 5);
	gl.Vertex2f(screenWidth, (int) ConsoleY * gtosMulY + 5);
	gl.Color4f(0, 0, 0, 0);
	gl.Vertex2f(screenWidth, (int) ConsoleY * gtosMulY + 13);
	gl.Vertex2f(0, (int) ConsoleY * gtosMulY + 13);
	gl.End();

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Scalef(Cfont.sizeX, Cfont.sizeY, 1);

	// The game & version number.
	Con_DrawSideText(gx.Get(DD_GAME_ID), 2, closeFade);
	Con_DrawSideText(gx.Get(DD_GAME_MODE), 1, closeFade);

	gl.Color4f(1, 1, 1, closeFade);

	// The text in the console buffer will be drawn from the bottom up (!).
	for(i = bPos - bLineOff - 1, y = ConsoleY * gtosMulY - fontScaledY * 2;
		i >= 0 && i < bufferLines && y > -fontScaledY; i--)
	{
		cbline_t *line = cbuffer + i;

		if(line->flags & CBLF_RULER)
		{
			// Draw a ruler here, and nothing else.
			Con_DrawRuler2(y / Cfont.sizeY, Cfont.height, closeFade,
						   screenWidth / Cfont.sizeX);
		}
		else
		{
			memset(buff, 0, sizeof(buff));
			strncpy(buff, line->text, 255);

			x = line->flags & CBLF_CENTER ? (screenWidth / Cfont.sizeX -
											 Cfont.Width(buff)) / 2 : 2;

			if(Cfont.Filter)
				Cfont.Filter(buff);
			else if(consoleShadowText)
			{
				// Draw a shadow.
				gl.Color3f(0, 0, 0);
				Cfont.TextOut(buff, x + 2, y / Cfont.sizeY + 2);
			}

			// Set the color.
			if(Cfont.flags & DDFONT_WHITE)	// Can it be colored?
				consoleSetColor(line->flags, closeFade);
			Cfont.TextOut(buff, x, y / Cfont.sizeY);
		}
		// Move up.
		y -= fontScaledY;
	}

	// The command line.
	strcpy(buff, ">");
	strcat(buff, cmdLine);

	if(Cfont.Filter)
		Cfont.Filter(buff);
	if(consoleShadowText)
	{
		// Draw a shadow.
		gl.Color3f(0, 0, 0);
		Cfont.TextOut(buff, 4,
					  2 + (ConsoleY * gtosMulY - fontScaledY) / Cfont.sizeY);
	}
	if(Cfont.flags & DDFONT_WHITE)
		gl.Color4f(CcolYellow[0], CcolYellow[1], CcolYellow[2], closeFade);
	else
		gl.Color4f(1, 1, 1, closeFade);
	Cfont.TextOut(buff, 2, (ConsoleY * gtosMulY - fontScaledY) / Cfont.sizeY);

	// Width of the current char.
	temp[0] = cmdLine[cmdCursor];
	temp[1] = 0;
	k = Cfont.Width(temp);
	if(!k)
		k = Cfont.Width(" ");

	// What is the width?
	memset(temp, 0, sizeof(temp));
	strncpy(temp, buff, MIN_OF(250, cmdCursor) + 1);
	i = Cfont.Width(temp);

	// Draw the cursor in the appropriate place.
	gl.Disable(DGL_TEXTURING);
	GL_DrawRect(2 + i, (ConsoleY * gtosMulY - fontScaledY) / Cfont.sizeY, k,
				Cfont.height, CcolYellow[0], CcolYellow[1], CcolYellow[2],
				closeFade * (((int) ConsoleBlink) & 0x10 ? .2f : .5f));
	gl.Enable(DGL_TEXTURING);

	// Restore the original matrices.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
	gl.MatrixMode(DGL_PROJECTION);
	gl.PopMatrix();
}

//===========================================================================
// Con_AddRuler
//  A ruler line will be added into the console. bPos is moved down by 1.
//===========================================================================
void Con_AddRuler(void)
{
	cbline_t *line = Con_GetBufferLine(bPos++);
	int     i;

	line->flags |= CBLF_RULER;
	if(consoleDump)
	{
		// A 70 characters long line.
		for(i = 0; i < 7; i++)
		{
			fprintf(outFile, "----------");
			if(isDedicated)
				Sys_ConPrint(0, "----------");
		}
		fprintf(outFile, "\n");
		if(isDedicated)
			Sys_ConPrint(0, "\n");
	}
}

void conPrintf(int flags, const char *format, va_list args)
{
	unsigned int i;
	int     lbc;				// line buffer cursor
	char   *prbuff, *lbuf = malloc(maxLineLen + 1);
	cbline_t *line;

	if(flags & CBLF_RULER)
	{
		Con_AddRuler();
		flags &= ~CBLF_RULER;
	}

	// Allocate a print buffer that will surely be enough (64Kb).
	// FIXME: No need to allocate on EVERY printf call!
	prbuff = malloc(65536);

	// Format the message to prbuff.
	vsprintf(prbuff, format, args);

	if(consoleDump)
		fprintf(outFile, "%s", prbuff);
	if(SW_IsActive())
		SW_Printf(prbuff);

	// Servers might have to send the text to a number of clients.
	if(isServer)
	{
		if(flags & CBLF_TRANSMIT)
			Sv_SendText(NSP_BROADCAST, flags, prbuff);
		else if(net_remoteuser)	// Is somebody logged in?
			Sv_SendText(net_remoteuser, flags | SV_CONSOLE_FLAGS, prbuff);
	}

	if(isDedicated)
	{
		Sys_ConPrint(flags, prbuff);
		free(lbuf);
		free(prbuff);
		return;
	}

	// We have the text we want to add in the buffer in prbuff.
	line = Con_GetBufferLine(bPos);	// Get a pointer to the current line.
	line->flags = flags;
	memset(lbuf, 0, maxLineLen + 1);
	for(i = 0, lbc = 0; i < strlen(prbuff); i++)
	{
		if(prbuff[i] == '\n' || lbc + line->len >= maxLineLen)	// A new line?
		{
			// Set the line text.
			addLineText(line, lbuf);
			// Clear the line write buffer.
			memset(lbuf, 0, maxLineLen + 1);
			lbc = 0;
			// Get the next line.
			line = Con_GetBufferLine(++bPos);
			line->flags = flags;
			// Newlines won't get in the buffer at all.
			if(prbuff[i] == '\n')
				continue;
		}
		lbuf[lbc++] = prbuff[i];
	}
	// Something still in the write buffer?
	if(lbc)
		addLineText(line, lbuf);

	// Clean up.
	free(lbuf);
	free(prbuff);

	// Now that something new has been printed, it will be shown.
	bLineOff = 0;

	// Check if there are too many lines.
	if(bufferLines > maxBufferLines)
	{
		int     rev = bufferLines - maxBufferLines;

		// The first 'rev' lines get removed.
		for(i = 0; (int) i < rev; i++)
			free(cbuffer[i].text);
		memmove(cbuffer, cbuffer + rev,
				sizeof(cbline_t) * (bufferLines - rev));
		//for(i=0; (int)i<rev; i++) memset(cbuffer+bufferLines-rev+i, 0, sizeof(cbline_t));
		cbuffer = realloc(cbuffer, sizeof(cbline_t) * (bufferLines -= rev));
		// Move the current position.
		bPos -= rev;
	}
}

// Print into the buffer.
void Con_Printf(const char *format, ...)
{
	va_list args;

	if(!ConsoleInited || ConsoleSilent)
		return;

	va_start(args, format);
	conPrintf(CBLF_WHITE, format, args);
	va_end(args);
}

void Con_FPrintf(int flags, const char *format, ...)	// Flagged printf
{
	va_list args;

	if(!ConsoleInited || ConsoleSilent)
		return;

	va_start(args, format);
	conPrintf(flags, format, args);
	va_end(args);
}

// As you can see, several commands can be handled inside one command function.
int CCmdConsole(int argc, char **argv)
{
	if(!stricmp(argv[0], "help"))
	{
		if(argc == 2)
		{
			int     i;

			if(!stricmp(argv[1], "(what)"))
			{
				Con_Printf("You've got to be kidding!\n");
				return true;
			}
			// We need to look through the cvars and ccmds to see if there's a match.
			for(i = 0; i < numCCmds; i++)
				if(!stricmp(argv[1], ccmds[i].name))
				{
					Con_Printf("%s\n", ccmds[i].help);
					return true;
				}
			for(i = 0; i < numCVars; i++)
				if(!stricmp(argv[1], cvars[i].name))
				{
					Con_Printf("%s\n", cvars[i].help);
					return true;
				}
			Con_Printf("There's no help about '%s'.\n", argv[1]);
		}
		else
		{
			Con_FPrintf(CBLF_RULER | CBLF_YELLOW | CBLF_CENTER,
						"-=- Doomsday " DOOMSDAY_VERSION_TEXT
						" Console -=-\n");
			Con_Printf("Keys:\n");
			Con_Printf("Tilde         Open/close the console.\n");
			Con_Printf
				("Shift-Tilde   Switch between half and full screen mode.\n");
			Con_Printf("PageUp/Down   Scroll up/down two lines.\n");
			Con_Printf
				("Ins/Del       Move console window up/down one line.\n");
			Con_Printf
				("Shift-Ins/Del Move console window three lines at a time.\n");
			Con_Printf("Home          Jump to the beginning of the buffer.\n");
			Con_Printf("End           Jump to the end of the buffer.\n");
			Con_Printf("F5            Clear the buffer.\n");
			Con_Printf("Alt-C         Clear the command-lne.\n");
			Con_Printf("Shift-left    Move cursor to the start of the command line.\n");
			Con_Printf("Shift-right   Move cursor to the end of the command line.\n");
			Con_Printf("\n");
			Con_Printf
				("Type \"listcmds\" to see a list of available commands.\n");
			Con_Printf
				("Type \"help (what)\" to see information about (what).\n");
			Con_FPrintf(CBLF_RULER, "\n");
		}
	}
	else if(!stricmp(argv[0], "clear"))
	{
		Con_ClearBuffer();
	}
	return true;
}

int CCmdListCmds(int argc, char **argv)
{
	int     i;

	Con_Printf("Console commands:\n");
	for(i = 0; i < numCCmds; i++)
	{
		if(argc > 1)			// Is there a filter?
			if(strnicmp(ccmds[i].name, argv[1], strlen(argv[1])))
				continue;
		Con_Printf("  %s (%s)\n", ccmds[i].name, ccmds[i].help);
	}
	return true;
}

int CCmdListVars(int argc, char **argv)
{
	int     i;

	Con_Printf("Console variables:\n");
	for(i = 0; i < numCVars; i++)
	{
		if(cvars[i].flags & CVF_HIDE)
			continue;
		if(argc > 1)			// Is there a filter?
			if(strnicmp(cvars[i].name, argv[1], strlen(argv[1])))
				continue;
		printcvar(cvars + i, "  ");
	}
	return true;
}

D_CMD(ListAliases)
{
	int     i;

	Con_Printf("Aliases:\n");
	for(i = 0; i < numCAliases; i++)
	{
		if(argc > 1)			// Is there a filter?
			if(strnicmp(caliases[i].name, argv[1], strlen(argv[1])))
				continue;
		Con_Printf("  %s == %s\n", caliases[i].name, caliases[i].command);
	}
	return true;
}

int CCmdVersion(int argc, char **argv)
{
	Con_Printf("Doomsday Engine %s (" __TIME__ ")\n", DOOMSDAY_VERSIONTEXT);
	if(gl.GetString)
		Con_Printf("%s\n", gl.GetString(DGL_VERSION));
	Con_Printf("Game DLL: %s\n", gx.Get(DD_VERSION_LONG));
	Con_Printf("http://sourceforge.net/projects/deng/\n");
	return true;
}

int CCmdQuit(int argc, char **argv)
{
	// No questions asked.
	Sys_Quit();
	return true;
}

void Con_Open(int yes)
{
	// The console cannot be closed in dedicated mode.
	if(isDedicated)
		yes = true;

	// Clear all action keys, keyup events won't go 
	// to bindings processing when the console is open.
	Con_ClearActions();
	openingOrClosing = true;
	if(yes)
	{
		ConsoleActive = true;
		ConsoleDestY = ConsoleOpenY;
		ConsoleTime = 0;
		ConsoleBlink = 0;
	}
	else
	{
		strcpy(cmdLine, "");
		cmdCursor = 0;
		ConsoleActive = false;
		ConsoleDestY = 0;
	}
}

//===========================================================================
// UpdateEngineState
//  What is this kind of a routine doing in Console.c?
//===========================================================================
void UpdateEngineState()
{
	// Update refresh.
	Con_Message("Updating state...\n");

	// Update the dir/WAD translations.
	F_InitDirec();

	gx.UpdateState(DD_PRE);
	R_Update();
	P_ValidateLevel();
	gx.UpdateState(DD_POST);
}

int CCmdLoadFile(int argc, char **argv)
{
	//extern int RegisteredSong;
	int     i, succeeded = false;

	if(argc == 1)
	{
		Con_Printf("Usage: load (file) ...\n");
		return true;
	}
	for(i = 1; i < argc; i++)
	{
		Con_Message("Loading %s...\n", argv[i]);
		if(W_AddFile(argv[i], true))
		{
			Con_Message("OK\n");
			succeeded = true;	// At least one has been loaded.
		}
		else
			Con_Message("Failed!\n");
	}
	// We only need to update if something was actually loaded.
	if(succeeded)
	{
		// Update the lumpcache.
		//W_UpdateCache();
		// The new wad may contain lumps that alter the current ones
		// in use.
		UpdateEngineState();
	}
	return true;
}

int CCmdUnloadFile(int argc, char **argv)
{
	//extern int RegisteredSong;
	int     i, succeeded = false;

	if(argc == 1)
	{
		Con_Printf("Usage: unload (file) ...\n");
		return true;
	}
	for(i = 1; i < argc; i++)
	{
		Con_Message("Unloading %s...\n", argv[i]);
		if(W_RemoveFile(argv[i]))
		{
			Con_Message("OK\n");
			succeeded = true;
		}
		else
			Con_Message("Failed!\n");
	}
	if(succeeded)
		UpdateEngineState();
	return true;
}

int CCmdListFiles(int argc, char **argv)
{
	extern int numrecords;
	extern filerecord_t *records;
	int     i;

	for(i = 0; i < numrecords; i++)
	{
		Con_Printf("%s (%d lump%s%s)", records[i].filename,
				   records[i].numlumps, records[i].numlumps != 1 ? "s" : "",
				   !(records[i].flags & FRF_RUNTIME) ? ", startup" : "");
		if(records[i].iwad)
			Con_Printf(" [%08x]", W_CRCNumberForRecord(i));
		Con_Printf("\n");
	}

	Con_Printf("Total: %d lumps in %d files.\n", numlumps, numrecords);
	return true;
}

int CCmdResetLumps(int argc, char **argv)
{
	GL_SetFilter(0);
	W_Reset();
	Con_Message("Only startup files remain.\n");
	UpdateEngineState();
	return true;
}

int CCmdBackgroundTurn(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf("Usage: bgturn (speed)\n");
		Con_Printf("Negative speeds are allowed. Default: 20.\n");
		Con_Printf("Current bgturn = %d.\n", consoleTurn);
		return true;
	}
	consoleTurn = atoi(argv[1]);
	if(!consoleTurn)
		funnyAng = 0;
	return true;
}

int CCmdDump(int argc, char **argv)
{
	char    fname[100];
	FILE   *file;
	int     lump;
	byte   *lumpPtr;

	if(argc != 2)
	{
		Con_Printf("Usage: dump (name)\n");
		Con_Printf("Writes out the specified lump to (name).dum.\n");
		return true;
	}
	if(W_CheckNumForName(argv[1]) == -1)
	{
		Con_Printf("No such lump.\n");
		return false;
	}
	lump = W_GetNumForName(argv[1]);
	lumpPtr = W_CacheLumpNum(lump, PU_STATIC);

	sprintf(fname, "%s.dum", argv[1]);
	file = fopen(fname, "wb");
	if(!file)
	{
		Con_Printf("Couldn't open %s for writing. %s\n", fname,
				   strerror(errno));
		Z_ChangeTag(lumpPtr, PU_CACHE);
		return false;
	}
	fwrite(lumpPtr, 1, lumpinfo[lump].size, file);
	fclose(file);
	Z_ChangeTag(lumpPtr, PU_CACHE);

	Con_Printf("%s dumped to %s.\n", argv[1], fname);
	return true;
}

D_CMD(Font)
{
	if(argc == 1 || argc > 3)
	{
		Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
		Con_Printf("Commands: default, name, size, xsize, ysize.\n");
		Con_Printf
			("Names: Fixed, Fixed12, System, System12, Large, Small7, Small8, Small10.\n");
		Con_Printf("Size 1.0 is normal.\n");
		return true;
	}
	if(!stricmp(argv[1], "default"))
	{
		FR_DestroyFont(FR_GetCurrent());
		FR_PrepareFont("Fixed");
		Cfont.flags = DDFONT_WHITE;
		Cfont.height = FR_TextHeight("Con");
		Cfont.sizeX = 1;
		Cfont.sizeY = 1;
		Cfont.TextOut = FR_TextOut;
		Cfont.Width = FR_TextWidth;
		Cfont.Filter = NULL;
	}
	else if(!stricmp(argv[1], "name") && argc == 3)
	{
		FR_DestroyFont(FR_GetCurrent());
		if(!FR_PrepareFont(argv[2]))
			FR_PrepareFont("Fixed");
		Cfont.height = FR_TextHeight("Con");
	}
	else if(argc == 3)
	{
		if(!stricmp(argv[1], "xsize") || !stricmp(argv[1], "size"))
			Cfont.sizeX = strtod(argv[2], NULL);
		if(!stricmp(argv[1], "ysize") || !stricmp(argv[1], "size"))
			Cfont.sizeY = strtod(argv[2], NULL);
		// Make sure the sizes are valid.
		if(Cfont.sizeX <= 0)
			Cfont.sizeX = 1;
		if(Cfont.sizeY <= 0)
			Cfont.sizeY = 1;
	}
	return true;
}

//===========================================================================
// CCmdAlias
//  Aliases will be saved to the config file.
//===========================================================================
int CCmdAlias(int argc, char **argv)
{
	if(argc != 3 && argc != 2)
	{
		Con_Printf("Usage: %s (alias) (cmd)\n", argv[0]);
		Con_Printf("Example: alias bigfont \"font size 3\".\n");
		Con_Printf
			("Use %%1-%%9 to pass the alias arguments to the command.\n");
		return true;
	}
	Con_Alias(argv[1], argc == 3 ? argv[2] : NULL);
	if(argc != 3)
		Con_Printf("Alias '%s' deleted.\n", argv[1]);
	return true;
}

D_CMD(SetGamma)
{
	int     newlevel;

	if(argc != 2)
	{
		Con_Printf("Usage: %s (0-4)\n", argv[0]);
		return true;
	}
	newlevel = strtol(argv[1], NULL, 0);
	// Clamp it to the min and max.
	if(newlevel < 0)
		newlevel = 0;
	if(newlevel > 4)
		newlevel = 4;
	// Only reload textures if it's necessary.
	if(newlevel != usegamma)
	{
		usegamma = newlevel;
		GL_UpdateGamma();
		Con_Printf("Gamma correction set to level %d.\n", usegamma);
	}
	else
		Con_Printf("Gamma correction already set to %d.\n", usegamma);
	return true;
}

D_CMD(Parse)
{
	int     i;

	if(argc == 1)
	{
		Con_Printf("Usage: %s (file) ...\n", argv[0]);
		return true;
	}
	for(i = 1; i < argc; i++)
	{
		Con_Printf("Parsing %s.\n", argv[i]);
		Con_ParseCommands(argv[i], false);
	}
	return true;
}

//===========================================================================
// CCmdWait
//===========================================================================
int CCmdWait(int argc, char **argv)
{
	timespan_t offset;

	if(argc != 3)
	{
		Con_Printf("Usage: %s (tics) (cmd)\n", argv[0]);
		Con_Printf("For example, '%s 35 \"echo End\"'.\n", argv[0]);
		return true;
	}
	offset = strtod(argv[1], NULL) / 35;	// Offset in seconds.
	if(offset < 0)
		offset = 0;
	Con_SplitIntoSubCommands(argv[2], offset);
	return true;
}

D_CMD(Repeat)
{
	int     count;
	timespan_t interval, offset;

	if(argc != 4)
	{
		Con_Printf("Usage: %s (count) (interval) (cmd)\n", argv[0]);
		Con_Printf("For example, '%s 10 35 \"screenshot\".\n", argv[0]);
		return true;
	}
	count = atoi(argv[1]);
	interval = strtod(argv[2], NULL) / 35;	// In seconds.
	offset = 0;
	while(count-- > 0)
	{
		offset += interval;
		Con_SplitIntoSubCommands(argv[3], offset);
	}
	return true;
}

D_CMD(Echo)
{
	int     i;

	for(i = 1; i < argc; i++)
		Con_Printf("%s\n", argv[i]);
	return true;
}

// Rather messy, wouldn't you say?
D_CMD(AddSub)
{
	boolean force = false, incdec;
	float   val, mod = 0;
	cvar_t *cvar;

	incdec = !stricmp(argv[0], "inc") || !stricmp(argv[0], "dec");
	if(argc == 1)
	{
		Con_Printf("Usage: %s (cvar) %s(force)\n", argv[0],
				   incdec ? "" : "(val) ");
		Con_Printf("Use force to make cvars go off limits.\n");
		return true;
	}
	if((incdec && argc >= 3) || (!incdec && argc >= 4))
	{
		force = !stricmp(argv[incdec ? 2 : 3], "force");
	}
	cvar = Con_GetVariable(argv[1]);
	if(!cvar)
		return false;

	if(cvar->flags & CVF_READ_ONLY)
	{
		Con_Printf("%s (cvar) is read-only. It can't be changed (not even with force)\n", argv[1]);
		return false;
	}

	val = Con_GetFloat(argv[1]);

	if(!stricmp(argv[0], "inc"))
		mod = 1;
	else if(!stricmp(argv[0], "dec"))
		mod = -1;
	else
		mod = strtod(argv[2], NULL);
	if(!stricmp(argv[0], "sub"))
		mod = -mod;

	val += mod;
	if(!force)
	{
		if(!(cvar->flags & CVF_NO_MAX) && val > cvar->max)
			val = cvar->max;
		if(!(cvar->flags & CVF_NO_MIN) && val < cvar->min)
			val = cvar->min;
	}
	Con_SetFloat(argv[1], val);
	return true;
}

/*
 * Toggle the value of a variable between zero and nonzero.
 */
int CCmdToggle(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf("Usage: %s (cvar)\n", argv[0]);
		return true;
	}

	Con_SetInteger(argv[1], Con_GetInteger(argv[1]) ? 0 : 1);
	return true;
}

/*
 * Execute a command if the condition passes.
 */
int CCmdIf(int argc, char **argv)
{
	struct {
		const char *opstr;
		int     op;
	} operators[] =
	{
		{
		"not", IF_NOT_EQUAL},
		{
		"=", IF_EQUAL},
		{
		">", IF_GREATER},
		{
		"<", IF_LESS},
		{
		">=", IF_GEQUAL},
		{
		"<=", IF_LEQUAL},
		{
		NULL}
	};
	int     i, oper;
	cvar_t *var;
	boolean isTrue = false;

	if(argc != 5 && argc != 6)
	{
		Con_Printf("Usage: %s (cvar) (operator) (value) (cmd) (else-cmd)\n",
				   argv[0]);
		Con_Printf("Operator must be one of: not, =, >, <, >=, <=.\n");
		Con_Printf("The (else-cmd) can be omitted.\n");
		return true;
	}

	var = Con_GetVariable(argv[1]);
	if(!var)
		return false;

	// Which operator?
	for(i = 0; operators[i].opstr; i++)
		if(!stricmp(operators[i].opstr, argv[2]))
		{
			oper = operators[i].op;
			break;
		}
	if(!operators[i].opstr)
		return false;			// Bad operator.

	// Value comparison depends on the type of the variable.
	if(var->type == CVT_BYTE || var->type == CVT_INT)
	{
		int     value = (var->type == CVT_INT ? CV_INT(var) : CV_BYTE(var));
		int     test = strtol(argv[3], 0, 0);

		isTrue =
			(oper == IF_EQUAL ? value == test : oper == IF_NOT_EQUAL ? value !=
			 test : oper == IF_GREATER ? value > test : oper ==
			 IF_LESS ? value < test : oper == IF_GEQUAL ? value >=
			 test : value <= test);
	}
	else if(var->type == CVT_FLOAT)
	{
		float   value = CV_FLOAT(var);
		float   test = strtod(argv[3], 0);

		isTrue =
			(oper == IF_EQUAL ? value == test : oper == IF_NOT_EQUAL ? value !=
			 test : oper == IF_GREATER ? value > test : oper ==
			 IF_LESS ? value < test : oper == IF_GEQUAL ? value >=
			 test : value <= test);
	}
	else if(var->type == CVT_CHARPTR)
	{
		int     comp = stricmp(CV_CHARPTR(var), argv[3]);

		isTrue =
			(oper == IF_EQUAL ? comp == 0 : oper == IF_NOT_EQUAL ? comp !=
			 0 : oper == IF_GREATER ? comp > 0 : oper == IF_LESS ? comp <
			 0 : oper == IF_GEQUAL ? comp >= 0 : comp <= 0);
	}

	// Should the command be executed?
	if(isTrue)
	{
		Con_Execute(argv[4], ConsoleSilent);
	}
	else if(argc == 6)
	{
		Con_Execute(argv[5], ConsoleSilent);
	}
	CmdReturnValue = isTrue;
	return true;
}

/*
 * Prints a file name to the console.
 * This is a f_forall_func_t.
 */
int Con_PrintFileName(const char *fn, filetype_t type, void *dir)
{
	// Exclude the path.
	Con_Printf("  %s\n", fn + strlen(dir));

	// Continue the listing.
	return true;
}

/*
 * Print contents of directories as Doomsday sees them.
 */
int CCmdDir(int argc, char **argv)
{
	char    dir[256], pattern[256];
	int     i;

	if(argc == 1)
	{
		Con_Printf("Usage: %s (dirs)\n", argv[0]);
		Con_Printf("Prints the contents of one or more directories.\n");
		Con_Printf("Virtual files are listed, too.\n");
		Con_Printf("Paths are relative to the base path:\n");
		Con_Printf("  %s\n", ddBasePath);
		return true;
	}

	for(i = 1; i < argc; i++)
	{
		M_PrependBasePath(argv[i], dir);
		Dir_ValidDir(dir);
		Dir_MakeAbsolute(dir);
		Con_Printf("Directory: %s\n", dir);

		// Make the pattern.
		sprintf(pattern, "%s*", dir);
		F_ForAll(pattern, dir, Con_PrintFileName);
	}

	return true;
}

/*
 * Print a 'global' message (to stdout and the console).
 */
void Con_Message(const char *message, ...)
{
	va_list argptr;
	char   *buffer;

	if(message[0])
	{
		buffer = malloc(0x10000);

		va_start(argptr, message);
		vsprintf(buffer, message, argptr);
		va_end(argptr);

#ifdef UNIX
		if(!isDedicated)
		{
			// These messages are supposed to be visible in the real console.
			fprintf(stderr, "%s", buffer);
		}
#endif

		// These messages are always dumped. If consoleDump is set,
		// Con_Printf() will dump the message for us.
		if(!consoleDump)
			printf("%s", buffer);

		// Also print in the console.
		Con_Printf(buffer);

		free(buffer);
	}
	Con_DrawStartupScreen(true);
}

/*
 * Print an error message and quit.
 */
void Con_Error(const char *error, ...)
{
	static boolean errorInProgress = false;
	extern int bufferLines;
	int     i;
	char    buff[2048], err[256];
	va_list argptr;

	// Already in an error?
	if(!ConsoleInited || errorInProgress)
	{
		fprintf(outFile, "Con_Error: Stack overflow imminent, aborting...\n");

		va_start(argptr, error);
		vsprintf(buff, error, argptr);
		va_end(argptr);
		Sys_MessageBox(buff, true);

		// Exit immediately, lest we go into an infinite loop.
		exit(1);
	}

	// We've experienced a fatal error; program will be shut down.
	errorInProgress = true;

	// Get back to the directory we started from.
	Dir_ChDir(&ddRuntimeDir);

	va_start(argptr, error);
	vsprintf(err, error, argptr);
	va_end(argptr);
	fprintf(outFile, "%s\n", err);

	strcpy(buff, "");
	for(i = 5; i > 1; i--)
	{
		cbline_t *cbl = Con_GetBufferLine(bufferLines - i);

		if(!cbl || !cbl->text)
			continue;
		strcat(buff, cbl->text);
		strcat(buff, "\n");
	}
	strcat(buff, "\n");
	strcat(buff, err);

	Sys_Shutdown();
	B_Shutdown();
	Con_Shutdown();

#ifdef WIN32
	ChangeDisplaySettings(0, 0);	// Restore original mode, just in case.
#endif

	// Be a bit more graphic.
	Sys_ShowCursor(true);
	Sys_ShowCursor(true);
	if(err[0])					// Only show if a message given.
	{
		Sys_MessageBox(buff, true);
	}

	DD_Shutdown();

	// Open Doomsday.out in a text editor.
	fflush(outFile);			// Make sure all the buffered stuff goes into the file.
	Sys_OpenTextEditor("Doomsday.out");

	// Get outta here.
	exit(1);
}

/*
 * Console command to open/close the console prompt.
 */
int CCmdOpenClose(int argc, char **argv)
{
    if(!stricmp(argv[0], "conopen"))
    {
        Con_Open(true);
    }
    else if(!stricmp(argv[0], "conclose"))
    {
        Con_Open(false);
    }
    else
    {
        Con_Open(!ConsoleActive);
    }
    return true; 
}

D_CMD(SkyDetail);
D_CMD(Fog);
D_CMD(Bind);
D_CMD(ListBindings);
D_CMD(ListBindClasses);
D_CMD(EnableBindClass);
D_CMD(SetGamma);
D_CMD(SetRes);
D_CMD(Chat);
D_CMD(DeleteBind);
D_CMD(FlareConfig);
D_CMD(ListActs);
D_CMD(ClearBindings);
D_CMD(Ping);
D_CMD(Login);
D_CMD(Logout);

#ifdef _DEBUG
D_CMD(TranslateFont);
#endif

// Register console commands.  The names should be in LOWER CASE.
static void registerCommands(void)
{
	C_CMD("actions", ListActs, "List all action commands.");
	C_CMD("add", AddSub, "Add something to a cvar.");
	C_CMD("after", Wait, "Execute the specified command after a delay.");
	C_CMD("alias", Alias, "Create aliases for a (set of) console commands.");
	C_CMD("bgturn", BackgroundTurn, "Set console background rotation speed.");
	C_CMD("bind", Bind, "Bind a console command to an event.");
	C_CMD("bindr", Bind,
		  "Bind a console command to an event (keys with repeat).");
	C_CMD("chat", Chat, "Broadcast a chat message.");
	C_CMD("chatnum", Chat, "Send a chat message to the specified player.");
	C_CMD("chatto", Chat, "Send a chat message to the specified player.");
	C_CMD("clear", Console, "Clear the console buffer.");
	C_CMD("clearbinds", ClearBindings, "Deletes all existing bindings.");
    C_CMD("conclose", OpenClose, "Close the console prompt.");
	C_CMD("conlocp", MakeCamera, "Connect a local player.");
	C_CMD("connect", Connect, "Connect to a server using TCP/IP.");
    C_CMD("conopen", OpenClose, "Open the console prompt.");
    C_CMD("contoggle", OpenClose, "Open/close the console prompt.");
	C_CMD("dec", AddSub, "Subtract 1 from a cvar.");
	C_CMD("delbind", DeleteBind,
		  "Deletes all bindings to the given console command.");
	C_CMD("demolump", DemoLump, "Write a reference lump file for a demo.");
	C_CMD("dir", Dir, "Print contents of directories.");
	C_CMD("dump", Dump, "Dump a data lump currently loaded in memory.");
	C_CMD("dumpkeymap", DumpKeyMap, "Write the current keymap to a file.");
	C_CMD("echo", Echo, "Echo the parameters on separate lines.");
	C_CMD("enablebindclass", EnableBindClass, "Enable a binding class.");
	C_CMD("exec", Parse,
		  "Loads and executes a file containing console commands.");
	C_CMD("flareconfig", FlareConfig, "Configure lens flares.");
	C_CMD("fog", Fog, "Modify fog settings.");
	C_CMD("font", Font, "Modify console font settings.");
	C_CMD("help", Console, "Show information about the console.");
	C_CMD("huffman", HuffmanStats,
		  "Print Huffman efficiency and number of bytes sent.");
	C_CMD("if", If, "Execute a command if the condition is true.");
	C_CMD("inc", AddSub, "Add 1 to a cvar.");
	C_CMD("keymap", KeyMap, "Load a DKM keymap file.");
	C_CMD("kick", Kick, "Kick client out of the game.");
	C_CMD("listaliases", ListAliases,
		  "List all aliases and their expanded forms.");
	C_CMD("listbindings", ListBindings, "List all event bindings.");
	C_CMD("listbindclasses", ListBindClasses, "List all event binding classes.");
	C_CMD("listcmds", ListCmds, "List all console commands.");
	C_CMD("listfiles", ListFiles,
		  "List all the loaded data files and show information about them.");
	C_CMD("listmaps", ListMaps, "List all loaded maps.");
	C_CMD("listvars", ListVars,
		  "List all console variables and their values.");
	C_CMD("load", LoadFile, "Load a data file (a WAD or a lump).");
	C_CMD("login", Login, "Log in to server console.");
	C_CMD("logout", Logout, "Terminate remote connection to server console.");
	C_CMD("lowres", LowRes, "Select the poorest rendering quality.");
	C_CMD("ls", Dir, "Print contents of directories.");
	C_CMD("mipmap", MipMap, "Set the mipmapping mode.");
	C_CMD("net", Net, "Network setup and control.");
	C_CMD("panel", OpenPanel, "Open the Doomsday Control Panel.");
	C_CMD("pausedemo", PauseDemo, "Pause/resume demo recording.");
	C_CMD("ping", Ping, "Ping the server (or a player if you're the server).");
	C_CMD("playdemo", PlayDemo, "Play a demo.");
	C_CMD("playext", PlayExt, "Play an external music file.");
	C_CMD("playmusic", PlayMusic,
		  "Play a song, an external music file or a CD track.");
	C_CMD("playsound", PlaySound, "Play a sound effect.");
	C_CMD("quit!", Quit, "Exit the game immediately.");
	C_CMD("recorddemo", RecordDemo, "Start recording a demo.");
	C_CMD("repeat", Repeat, "Repeat a command at given intervals.");
	C_CMD("reset", ResetLumps,
		  "Reset the data files into what they were at startup.");
	C_CMD("safebind", Bind,
		  "Bind a command to an event, unless the event is already bound.");
	C_CMD("safebindr", Bind,
		  "Bind a command to an event, unless the event is already bound.");
	C_CMD("say", Chat, "Broadcast a chat message.");
	C_CMD("saynum", Chat, "Send a chat message to the specified player.");
	C_CMD("sayto", Chat, "Send a chat message to the specified player.");
	C_CMD("setcon", SetConsole, "Set console and viewplayer.");
	C_CMD("setgamma", SetGamma, "Set the gamma correction level.");
	C_CMD("setname", SetName, "Set your name.");
	C_CMD("setres", SetRes, "Change video mode resolution or window size.");
	C_CMD("settics", SetTicks,
		  "Set number of game tics per second (default: 35).");
	C_CMD("setvidramp", UpdateGammaRamp,
		  "Update display's hardware gamma ramp.");
	C_CMD("skydetail", SkyDetail,
		  "Set the number of sky sphere quadrant subdivisions.");
	C_CMD("skyrows", SkyDetail, "Set the number of sky sphere rows.");
	C_CMD("smoothscr", SmoothRaw,
		  "Set the rendering mode of fullscreen images.");
	C_CMD("stopdemo", StopDemo, "Stop currently playing demo.");
	C_CMD("stopmusic", StopMusic, "Stop any currently playing music.");
	C_CMD("sub", AddSub, "Subtract something from a cvar.");
	C_CMD("texreset", ResetTextures, "Force a texture reload.");
	C_CMD("toggle", Toggle,
		  "Toggle the value of a cvar between zero and nonzero.");
	C_CMD("uicolor", UIColor, "Change Doomsday user interface colors.");
	C_CMD("unload", UnloadFile, "Unload a data file from memory.");
	C_CMD("version", Version, "Show detailed version information.");
	C_CMD("write", WriteConsole,
		  "Write variables, bindings and aliases to a file.");

#ifdef _DEBUG
	C_CMD("translatefont", TranslateFont, "Ha ha.");
#endif
}

static void registerVariables(void)
{
	// Console
	C_VAR_INT("con-alpha", &consoleAlpha, 0, 0, 100,
			  "Console background translucency.");
	C_VAR_INT("con-light", &consoleLight, 0, 0, 100,
			  "Console background light level.");
	C_VAR_INT("con-completion", &conCompMode, 0, 0, 1,
			  "How to complete words when pressing Tab:\n"
			  "0=Show completions, 1=Cycle through them.");
	C_VAR_BYTE("con-dump", &consoleDump, 0, 0, 1,
			   "1=Dump all console messages to Doomsday.out.");
	C_VAR_INT("con-key-activate", &consoleActiveKey, 0, 0, 255,
			  "Key to activate the console (ASCII code, "
			  "default is tilde, 96).");
	C_VAR_BYTE("con-key-show", &consoleShowKeys, 0, 0, 1,
			   "1=Show ASCII codes of pressed keys in the console.");
	C_VAR_BYTE("con-var-silent", &conSilentCVars, 0, 0, 1,
			   "1=Don't show the value of a cvar when setting it.");
	C_VAR_BYTE("con-progress", &progress_enabled, 0, 0, 1,
			   "1=Show progress bar.");
	C_VAR_BYTE("con-fps", &consoleShowFPS, 0, 0, 1,
			   "1=Show FPS counter on screen.");
	C_VAR_BYTE("con-text-shadow", &consoleShadowText, 0, 0, 1,
			   "1=Text in the console has a shadow (might be slow).");

	// User Interface
	C_VAR_BYTE("ui-panel-help", &panel_show_help, 0, 0, 1,
			   "1=Enable help window in Control Panel.");
	C_VAR_BYTE("ui-panel-tips", &panel_show_tips, 0, 0, 1,
			   "1=Show help indicators in Control Panel.");
	C_VAR_INT("ui-cursor-width", &uiMouseWidth, CVF_NO_MAX, 1, 0,
			  "Mouse cursor width.");
	C_VAR_INT("ui-cursor-height", &uiMouseHeight, CVF_NO_MAX, 1, 0,
			  "Mouse cursor height.");

	// Video
	C_VAR_INT("vid-res-x", &defResX, CVF_NO_MAX, 320, 0,
			  "Default resolution (X).");
	C_VAR_INT("vid-res-y", &defResY, CVF_NO_MAX, 240, 0,
			  "Default resolution (Y).");
	C_VAR_FLOAT("vid-gamma", &vid_gamma, 0, 0.1f, 6,
				"Display gamma correction factor: 1=normal.");
	C_VAR_FLOAT("vid-contrast", &vid_contrast, 0, 0, 10,
				"Display contrast: 1=normal.");
	C_VAR_FLOAT("vid-bright", &vid_bright, 0, -2, 2,
				"Display brightness: -1=dark, 0=normal, 1=light.");

	// Render
	C_VAR_INT("rend-dev-wireframe", &renderWireframe, 0, 0, 1,
			  "1=Render player view in wireframe mode.");
	C_VAR_INT("rend-dev-framecount", &framecount,
              CVF_NO_ARCHIVE | CVF_PROTECTED, 0, 0,
			  "Frame counter.");
	// * Render-Info
	C_VAR_BYTE("rend-info-lums", &rend_info_lums, 0, 0, 1,
			   "1=Print lumobj count after rendering a frame.");
	// * Render-Light
	C_VAR_INT("rend-light-ambient", &r_ambient, 0, 0, 255,
			  "Ambient light level.");
	C_VAR_INT("rend-light", &useDynLights, 0, 0, 1,
			  "1=Render dynamic lights.");
	C_VAR_INT("rend-light-blend", &dlBlend, 0, 0, 3,
			  "Dynamic lights color blending mode:\n"
			  "0=normal, 1=additive, 2=no blending.");

	C_VAR_FLOAT("rend-light-bright", &dlFactor, 0, 0, 1,
				"Intensity factor for dynamic lights.");
	C_VAR_INT("rend-light-num", &maxDynLights, 0, 0, 8000,
			  "The maximum number of dynamic lights. 0=no limit.");

	C_VAR_FLOAT("rend-light-radius-scale", &dlRadFactor, 0, 0.1f, 10,
				"A multiplier for dynlight radii (default: 1).");
	C_VAR_INT("rend-light-radius-max", &dlMaxRad, 0, 64, 512,
			  "Maximum radius of dynamic lights (default: 128).");
	C_VAR_FLOAT("rend-light-wall-angle", &rend_light_wall_angle, CVF_NO_MAX, 0,
				0, "Intensity of angle-based wall light.");
	C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1,
			  "1=Use multitexturing when rendering dynamic lights.");
	// * Render-Light-Decor
	C_VAR_BYTE("rend-light-decor", &useDecorations, 0, 0, 1,
			   "1=Enable surface light decorations.");
	C_VAR_FLOAT("rend-light-decor-plane-far", &decorPlaneMaxDist, CVF_NO_MAX,
				0, 0,
				"Maximum distance at which plane light "
				"decorations are visible.");
	C_VAR_FLOAT("rend-light-decor-wall-far", &decorWallMaxDist, CVF_NO_MAX, 0,
				0,
				"Maximum distance at which wall light decorations "
				"are visible.");
	C_VAR_FLOAT("rend-light-decor-plane-bright", &decorPlaneFactor, 0, 0, 10,
				"Brightness of plane light decorations.");
	C_VAR_FLOAT("rend-light-decor-wall-bright", &decorWallFactor, 0, 0, 10,
				"Brightness of wall light decorations.");
	C_VAR_FLOAT("rend-light-decor-angle", &decorFadeAngle, 0, 0, 1,
				"Reduce brightness if surface/view angle too steep.");
	C_VAR_INT("rend-light-sky", &rendSkyLight, 0, 0, 1,
			  "1=Use special light color in sky sectors.");
	// * Render-Glow
	C_VAR_INT("rend-glow", &r_texglow, 0, 0, 1, "1=Enable glowing textures.");
	C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1,
			  "1=Render glow on walls.");
	C_VAR_INT("rend-glow-height", &glowHeight, 0, 0, 1024,
			  "Height of wall glow.");
	C_VAR_FLOAT("rend-glow-fog-bright", &glowFogBright, 0, 0, 1,
				"Brightness of wall glow when fog is enabled.");
	// * Render-Halo
	C_VAR_INT("rend-halo", &haloMode, 0, 0, 5,
			  "Number of flares to draw per light.");
	C_VAR_INT("rend-halo-realistic", &haloRealistic, 0, 0, 1,
			  "1=Use more realistic halo effects.");
	C_VAR_INT("rend-halo-bright", &haloBright, 0, 0, 100,
			  "Halo/flare brightness.");
	C_VAR_INT("rend-halo-occlusion", &haloOccludeSpeed, CVF_NO_MAX, 0, 0,
			  "Rate at which occluded halos fade.");
	C_VAR_INT("rend-halo-size", &haloSize, 0, 0, 100, "Size of halos.");
	C_VAR_FLOAT("rend-halo-secondary-limit", &minHaloSize, CVF_NO_MAX, 0, 0,
				"Minimum halo size.");
	C_VAR_FLOAT("rend-halo-fade-far", &haloFadeMax, CVF_NO_MAX, 0, 0,
				"Distance at which halos are no longer visible.");
	C_VAR_FLOAT("rend-halo-fade-near", &haloFadeMin, CVF_NO_MAX, 0, 0,
				"Distance to begin fading halos.");
	// * Render-Texture
	C_VAR_INT("rend-tex", &renderTextures, CVF_NO_ARCHIVE, 0, 1,
			  "1=Render with textures.");
	C_VAR_INT("rend-tex-gamma", &usegamma, CVF_PROTECTED, 0, 4,
			  "The gamma correction level (0-4).");
	C_VAR_INT("rend-tex-mipmap", &mipmapping, CVF_PROTECTED, 0, 5,
			  "The mipmapping mode for textures.");
	C_VAR_BYTE("rend-tex-paletted", &paletted, CVF_PROTECTED, 0, 1,
			   "1=Use the GL_EXT_shared_texture_palette extension.");
	C_VAR_BYTE("rend-tex-external-always", &loadExtAlways, 0, 0, 1,
			   "1=Always use external texture resources (overrides -pwadtex).");
	C_VAR_INT("rend-tex-quality", &texQuality, 0, 0, 8,
			  "The quality of textures (0-8).");
	C_VAR_INT("rend-tex-filter-sprite", &filterSprites, 0, 0, 1,
			  "1=Render smooth sprites.");
	C_VAR_INT("rend-tex-filter-raw", &linearRaw, CVF_PROTECTED, 0, 1,
			  "1=Fullscreen images (320x200) use linear interpolation.");
	C_VAR_INT("rend-tex-filter-smart", &useSmartFilter, 0, 0, 1,
			  "1=Use hq2x-filtering on all textures.");
    C_VAR_INT("rend-tex-filter-mag", &texMagMode, 0, 0, 1,
              "1=Use bilinear filtering for texture magnification.");
	C_VAR_INT("rend-tex-detail", &r_detail, 0, 0, 1,
			  "1=Render with detail textures.");
	C_VAR_FLOAT("rend-tex-detail-scale", &detailScale, CVF_NO_MIN | CVF_NO_MAX,
				0, 0, "Global detail texture factor.");
	C_VAR_FLOAT("rend-tex-detail-strength", &detailFactor, 0, 0, 10,
				"Global detail texture strength factor.");
	C_VAR_INT("rend-tex-detail-multitex", &useMultiTexDetails, 0, 0, 1,
			  "1=Use multitexturing when rendering detail textures.");
	// * Render-Sky
	C_VAR_INT("rend-sky-detail", &skyDetail, CVF_PROTECTED, 3, 7,
			  "Number of sky sphere quadrant subdivisions.");
	C_VAR_INT("rend-sky-rows", &skyRows, CVF_PROTECTED, 1, 8,
			  "Number of sky sphere rows.");
	C_VAR_FLOAT("rend-sky-distance", &skyDist, CVF_NO_MAX, 1, 0,
				"Sky sphere radius.");
	C_VAR_INT("rend-sky-full", &r_fullsky, 0, 0, 1,
			  "1=Always render the full sky sphere.");
	C_VAR_INT("rend-sky-simple", &simpleSky, 0, 0, 2,
			  "Sky rendering mode: 0=normal, 1=quads.");
	// * Render-Sprite
	C_VAR_FLOAT("rend-sprite-align-angle", &maxSpriteAngle, 0, 0, 90,
				"Maximum angle for slanted sprites (spralign 2).");
	C_VAR_INT("rend-sprite-noz", &r_nospritez, 0, 0, 1,
			  "1=Don't write sprites in the Z buffer.");
	C_VAR_BYTE("rend-sprite-precache", &r_precache_sprites, 0, 0, 1,
			   "1=Precache sprites at level setup (slow).");
	C_VAR_INT("rend-sprite-align", &alwaysAlign, 0, 0, 3,
			  "1=Always align sprites with the view plane.\n"
			  "2=Align to camera, unless slant > r_maxSpriteAngle.");
	C_VAR_INT("rend-sprite-blend", &missileBlend, 0, 0, 1,
			  "1=Use additive blending for explosions.");
	C_VAR_INT("rend-sprite-lit", &litSprites, 0, 0, 1,
			  "1=Sprites lit using dynamic lights.");
	// * Render-Model
	C_VAR_INT("rend-model", &useModels, CVF_NO_MAX, 0, 1,
			  "Render using 3D models when possible.");
	C_VAR_INT("rend-model-lights", &modelLight, 0, 0, 10,
			  "Maximum number of light sources on models.");
	C_VAR_INT("rend-model-inter", &frameInter, 0, 0, 1,
			  "1=Interpolate frames.");
	C_VAR_FLOAT("rend-model-aspect", &rModelAspectMod, CVF_NO_MAX | CVF_NO_MIN,
				0, 0, "Scale for MD2 z-axis when model is loaded.");
	C_VAR_INT("rend-model-distance", &r_maxmodelz, CVF_NO_MAX, 0, 0,
			  "Farther than this models revert back to sprites.");
	C_VAR_BYTE("rend-model-precache", &r_precache_skins, 0, 0, 1,
			   "1=Precache 3D models at level setup (slow).");
	C_VAR_FLOAT("rend-model-lod", &rend_model_lod, CVF_NO_MAX, 0, 0,
				"Custom level of detail factor. 0=LOD disabled, 1=normal.");
	C_VAR_INT("rend-model-mirror-hud", &mirrorHudModels, 0, 0, 1,
			  "1=Mirror HUD weapon models.");
	C_VAR_FLOAT("rend-model-spin-speed", &modelSpinSpeed,
				CVF_NO_MAX | CVF_NO_MIN, 0, 0,
				"Speed of model spinning, 1=normal.");
	C_VAR_INT("rend-model-shiny-multitex", &modelShinyMultitex, 0, 0, 1,
			  "1=Enable multitexturing with shiny model skins.");
	// * Render-HUD
	C_VAR_FLOAT("rend-hud-offset-scale", &weaponOffsetScale, CVF_NO_MAX, 0, 0,
				"Scaling of player weapon (x,y) offset.");
	C_VAR_FLOAT("rend-hud-fov-shift", &weaponFOVShift, CVF_NO_MAX, 0, 1,
				"When FOV > 90 player weapon is shifted downward.");
	// * Render-Mobj
	C_VAR_INT("rend-mobj-smooth-move", &r_use_srvo, 0, 0, 2,
			  "1=Use short-range visual offsets for models.\n"
			  "2=Use SRVO for sprites, too (unjags actor movement).");
	C_VAR_INT("rend-mobj-smooth-turn", &r_use_srvo_angle, 0, 0, 1,
			  "1=Use separate visual angle for mobjs (unjag actors).");
	// * Rend-Particle
	C_VAR_INT("rend-particle", &r_use_particles, 0, 0, 1,
			  "1=Render particle effects.");
	C_VAR_INT("rend-particle-max", &r_max_particles, CVF_NO_MAX, 0, 0,
			  "Maximum number of particles to render. 0=no limit.");
	C_VAR_FLOAT("rend-particle-rate", &r_particle_spawn_rate, 0, 0, 5,
				"Particle spawn rate multiplier (default: 1).");
	C_VAR_FLOAT("rend-particle-diffuse", &rend_particle_diffuse, CVF_NO_MAX, 0,
				0, "Diffuse factor for particles near the camera.");
	C_VAR_INT("rend-particle-visible-near", &rend_particle_nearlimit,
			  CVF_NO_MAX, 0, 0, "Minimum visible distance for a particle.");
	// * Rend-Shadow
	C_VAR_INT("rend-shadow", &useShadows, 0, 0, 1,
			  "1=Render shadows under objects.");
	C_VAR_FLOAT("rend-shadow-darkness", &shadowFactor, 0, 0, 1,
				"Darkness factor for object shadows.");
	C_VAR_INT("rend-shadow-far", &shadowMaxDist, CVF_NO_MAX, 0, 0,
			  "Maximum distance where shadows are visible.");
	C_VAR_INT("rend-shadow-radius-max", &shadowMaxRad, CVF_NO_MAX, 0, 0,
			  "Maximum radius of object shadows.");

	// Server
	C_VAR_CHARPTR("server-name", &serverName, 0, 0, 0,
				  "The name of this computer if it's a server.");
	C_VAR_CHARPTR("server-info", &serverInfo, 0, 0, 0,
				  "The description given of this computer if it's a server.");
	C_VAR_INT("server-public", &masterAware, 0, 0, 1,
			  "1=Send info to master server.");

	// Network
	C_VAR_CHARPTR("net-name", &playerName, 0, 0, 0,
				  "Your name in multiplayer games.");
	C_VAR_CHARPTR("net-master-address", &masterAddress, 0, 0, 0,
				  "Master server IP address / name.");
	C_VAR_INT("net-master-port", &masterPort, 0, 0, 65535,
			  "Master server TCP/IP port.");
	C_VAR_CHARPTR("net-master-path", &masterPath, 0, 0, 0,
				  "Master server path name.");

	// Sound
	C_VAR_INT("sound-volume", &sfx_volume, 0, 0, 255,
			  "Sound effects volume (0-255).");
	C_VAR_INT("sound-info", &sound_info, 0, 0, 1,
			  "1=Show sound debug information.");
	C_VAR_INT("sound-rate", &sound_rate, 0, 11025, 44100,
			  "Sound effects sample rate (11025, 22050, 44100).");
	C_VAR_INT("sound-16bit", &sound_16bit, 0, 0, 1,
			  "1=16-bit sound effects/resampling.");
	C_VAR_INT("sound-3d", &sound_3dmode, 0, 0, 1,
			  "1=Play sound effects in 3D.");
	C_VAR_FLOAT("sound-reverb-volume", &sfx_reverb_strength, 0, 0, 10,
				"Reverb effects general volume (0=disable).");

	// Music 
	C_VAR_INT("music-volume", &mus_volume, 0, 0, 255, "Music volume (0-255).");
	C_VAR_INT("music-source", &mus_preference, 0, 0, 2,
			  "Preferred music source: 0=Original MUS, "
			  "1=External files, 2=CD.");

	// File
	C_VAR_CHARPTR("file-startup", &defaultWads, 0, 0, 0,
				  "The list of WADs to be loaded at startup.");
}
