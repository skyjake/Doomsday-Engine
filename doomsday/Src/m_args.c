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
 * m_args.c: Command Line Arguments
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_ARG_NAMES	256

// TYPES -------------------------------------------------------------------

enum // Parser modes.
{
	PM_NORMAL,
	PM_NORMAL_REC,
	PM_COUNT
};

typedef struct
{
	char long_name[32];
	char short_name[8];
} argname_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int num_args;
static char **args;
static int arg_pos;
static int num_names;
static argname_t *names[MAX_ARG_NAMES];
static int last_match;

// CODE --------------------------------------------------------------------

//==========================================================================
// ArgParse
//	Parses the given command line. 
//==========================================================================
int ArgParse(int mode, const char *cmdline)
{
	char word[512], *ptr, *response, *cmd = (char*) cmdline;
	int i;
	int count = 0;
	FILE *file;
	boolean quote = false;
	boolean is_response;

	// Init normal mode?
	if(mode == PM_NORMAL) arg_pos = 0;

	for(; *cmd; )
	{
		// Must allow: -cmd "echo ""this is a command"""
		// But also: @"\Program Files\test.rsp\"
		// And also: @\"Program Files"\test.rsp
		// Hello" My"Friend means "Hello MyFriend"

		// Skip initial whitespace.
		cmd = M_SkipWhite(cmd);

		// Check for response files.
		if(*cmd == '@')
		{
			is_response = true;
			cmd = M_SkipWhite(cmd + 1);
		}
		else
		{
			is_response = false;
		}

		for(ptr = word; *cmd && (quote || !isspace(*cmd)); cmd++)
		{
			if(!quote)
			{
				// We're not inside quotes.
				if(*cmd == '\"') // Quote begins.
				{
					quote = true;
					continue;
				}
			}
			else
			{
				// We're inside quotes.
				if(*cmd == '\"') // Quote ends.
				{
					if(cmd[1] == '\"') // Doubles?
					{
						// Normal processing, but output only one quote.
						cmd++;
					}
					else
					{
						quote = false;
						continue;
					}
				}
			}
			*ptr++ = *cmd;
		}
		// Append NULL to the word.
		*ptr++ = 0;

		// Word has been extracted, examine it.
		if(is_response) // Response file?
		{
			// Try to open it.
			if((file = fopen(word, "rt")) != NULL)
			{
				// How long is it?
				fseek(file, 0, SEEK_END);
				i = ftell(file);
				rewind(file);
				response = calloc(i + 1, 1);
				fread(response, 1, i, file);
				fclose(file);
				count += ArgParse(mode == PM_COUNT? PM_COUNT
					: PM_NORMAL_REC, response);
				free(response);
			}
		}
		else if(!strcmp(word, "--")) // End of arguments.
		{
			return count;
		}
		else if(word[0]) // Make sure there *is* a word.
		{
			// Increase argument count.
			count++;
			if(mode != PM_COUNT)
			{
				// Allocate memory for the argument and copy it to the list.
				args[arg_pos] = malloc(strlen(word) + 1);
				strcpy(args[arg_pos++], word);
			}
		}
	}
	return count;
}

//==========================================================================
// ArgInit
//	Initializes the command line arguments list.
//==========================================================================
void ArgInit(const char *cmdline)
{
	// Clear the names register.
	num_names = 0;
	memset(names, 0, sizeof(names));

	// First count the number of arguments.
	num_args = ArgParse(PM_COUNT, cmdline);

	// Allocate memory for the list.
	args = calloc(num_args, sizeof(char*));

	// Then create the actual list of arguments.
	ArgParse(PM_NORMAL, cmdline);
}

//==========================================================================
// ArgShutdown
//	Frees the memory allocated for the command line.
//==========================================================================
void ArgShutdown(void)
{
	int i;

	for(i = 0; i < num_args; i++) free(args[i]);
	free(args);
	args = NULL;
	num_args = 0;
	for(i = 0; i < num_names; i++) free(names[i]);
	memset(names, 0, sizeof(names));
	num_names = 0;
}

//==========================================================================
// ArgAbbreviate
//	Registers a short name for a long arg name.
//	The short name can then be used on the command line and CheckParm 
//	will know to match occurances of the short name with the long name.
//==========================================================================
void ArgAbbreviate(char *longname, char *shortname)
{
	argname_t *name;

	if(num_names == MAX_ARG_NAMES) return;
	names[num_names++] = name = calloc(1, sizeof(*name));
	strncpy(name->long_name, longname, sizeof(name->long_name)-1);
	strncpy(name->short_name, shortname, sizeof(name->short_name)-1);
}

//==========================================================================
// Argc
//	Returns the number of arguments on the command line.
//==========================================================================
int Argc(void)
{
	return num_args;
}

//==========================================================================
// Argv
//	Returns a pointer to the i'th argument.
//==========================================================================
char * Argv(int i)
{
	if(i < 0 || i >= num_args) Con_Error("Argv: There is no arg %i.\n", i);
	return args[i];
}

//==========================================================================
// ArgvPtr
//	Returns a pointer to the i'th argument's pointer.
//==========================================================================
char ** ArgvPtr(int i)
{
	if(i < 0 || i >= num_args) Con_Error("ArgvPtr: There is no arg %i.\n", i);
	return &args[i];
}

//==========================================================================
// ArgNext
//==========================================================================
char * ArgNext(void)
{
	if(!last_match || last_match >= Argc()-1) return NULL;
	return args[++last_match];
}

//===========================================================================
// ArgRecognize
//	Returns true if the two parameters are equivalent according to the
//	abbreviations.
//===========================================================================
int ArgRecognize(char *first, char *second)
{
	int k;

	// A simple match?
	if(!stricmp(first, second)) return true;

	for(k = 0; k < num_names; k++)
	{
		if(stricmp(first, names[k]->long_name)) continue;
		// names[k] now matches the first string.
		if(!stricmp(names[k]->short_name, second)) return true;
	}
	return false;
}

//==========================================================================
// ArgCheck
//	Checks for the given parameter in the program's command line arguments.
//	Returns the argument number (1 to argc-1) or 0 if not present.
//==========================================================================
int ArgCheck(char *check)
{
	int i;

	for(i = 1; i < num_args; i++)
	{
		// Check the short names for this arg, too.
		if(ArgRecognize(check, args[i]))
			return last_match = i;
	}
	return last_match = 0;
}

//==========================================================================
// ArgCheckWith
//	Checks for the given parameter in the program's command line arguments
//	and it is followed by N other arguments.
//	Returns the argument number (1 to argc-1) or 0 if not present.
//==========================================================================
int ArgCheckWith(char *check, int num)
{
	int i = ArgCheck(check);

	if(!i || i + num >= num_args) return 0;
	return i;
}

//==========================================================================
// ArgIsOption
//	Returns true if the given argument begins with a hyphen.
//==========================================================================
int ArgIsOption(int i)
{
	if(i < 0 || i >= num_args) return false;
	return args[i][0] == '-';
}

//==========================================================================
// ArgExists
//	Returns true if the given parameter exists in the program's command
//	line arguments, false if not.
//==========================================================================
int ArgExists(char *check)
{
	return ArgCheck(check) != 0;
}

