/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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

#define MAX_ARG_NAMES   256

// TYPES -------------------------------------------------------------------

enum                            // Parser modes.
{
    PM_NORMAL,
    PM_NORMAL_REC,
    PM_COUNT
};

typedef struct {
    char    long_name[32];
    char    short_name[8];
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

/**
 * Parses the given command line.
 */
int ArgParse(int mode, const char *cmdline)
{
#define MAX_WORDLENGTH 512

    char    word[MAX_WORDLENGTH], *ptr, *response;
    unsigned char *cmd;
    int     i;
    int     count = 0;
    int     ccount;
    FILE   *file;
    boolean quote = false;
    boolean isResponse;
    boolean isDone, copyC;

    // Init normal mode?
    if(mode == PM_NORMAL)
        arg_pos = 0;

    cmd = (unsigned char *) cmdline;
    isDone = false;
    while(*cmd && !isDone)
    {
        // Must allow: -cmd "echo ""this is a command"""
        // But also: @"\Program Files\test.rsp\"
        // And also: @\"Program Files"\test.rsp
        // Hello" My"Friend means "Hello MyFriend"

        // Skip initial whitespace.
        cmd = (unsigned char*) M_SkipWhite((char *) cmd);

        // Check for response files.
        if(*cmd == '@')
        {
            isResponse = true;
            cmd = (unsigned char*) M_SkipWhite((char *)(cmd + 1));
        }
        else
        {
            isResponse = false;
        }

        ccount = 0;
        ptr = word;
        while(*cmd && (quote || !isspace(*cmd)))
        {
            copyC = true;
            if(!quote)
            {
                // We're not inside quotes.
                if(*cmd == '\"')    // Quote begins.
                {
                    quote = true;
                    copyC = false;
                }
            }
            else
            {
                // We're inside quotes.
                if(*cmd == '\"')    // Quote ends.
                {
                    if(cmd[1] == '\"')  // Doubles?
                    {
                        // Normal processing, but output only one quote.
                        cmd++;
                        ccount++;
                    }
                    else
                    {
                        quote = false;
                        copyC = false;
                    }
                }
            }

            if(copyC)
            {
                if(ccount > MAX_WORDLENGTH -1)
                    Con_Error("ArgParse: too many characters in word!");

                *ptr++ = *cmd;
            }

            cmd++;
            ccount++;
        }
        // Append NULL to the word.
        *ptr++ = '\0';

        // Word has been extracted, examine it.
        if(isResponse)         // Response file?
        {
            // Try to open it.
            if((file = fopen(word, "rt")) != NULL)
            {
                // How long is it?
                if(fseek(file, 0, SEEK_END))
                    Con_Error("ArgParse: fseek failed on SEEK_END!");

                i = ftell(file);
                rewind(file);

                if((response = M_Calloc(i + 1)) == NULL)
                    Con_Error("ArgParse: failed on alloc of %d bytes.",
                              i + 1);

                fread(response, 1, i, file);
                fclose(file);
                count +=
                    ArgParse(mode == PM_COUNT ? PM_COUNT : PM_NORMAL_REC,
                             response);
                M_Free(response);
            }
        }
        else if(!strcmp(word, "--"))    // End of arguments.
        {
            isDone = true;
        }
        else if(word[0])        // Make sure there *is* a word.
        {
            // Increase argument count.
            count++;
            if(mode != PM_COUNT)
            {
                // Allocate memory for the argument and copy it to the list.
                args[arg_pos] = M_Malloc(strlen(word) + 1);
                strcpy(args[arg_pos++], word);
            }
        }
    }
    return count;
}

/**
 * Initializes the command line arguments list.
 */
void ArgInit(const char *cmdline)
{
    // Clear the names register.
    num_names = 0;
    memset(names, 0, sizeof(names));

    // First count the number of arguments.
    num_args = ArgParse(PM_COUNT, cmdline);

    // Allocate memory for the list.
    args = M_Calloc(num_args * sizeof(char *));

    // Then create the actual list of arguments.
    ArgParse(PM_NORMAL, cmdline);
}

/**
 * Frees the memory allocated for the command line.
 */
void ArgShutdown(void)
{
    int     i;

    for(i = 0; i < num_args; ++i)
        M_Free(args[i]);
    M_Free(args);
    args = NULL;

    num_args = 0;

    for(i = 0; i < num_names; ++i)
        M_Free(names[i]);
    memset(names, 0, sizeof(names));
    num_names = 0;
}

/**
 * Registers a short name for a long arg name.
 * The short name can then be used on the command line and CheckParm
 * will know to match occurances of the short name with the long name.
 */
void ArgAbbreviate(char *longname, char *shortname)
{
    argname_t *name;

    if(num_names == MAX_ARG_NAMES)
        return;

    if((name = M_Calloc(sizeof(*name))) == NULL)
        Con_Error("ArgAbbreviate: calloc failed on allocation of %lu bytes.",
                  (unsigned long) sizeof(*name));

    names[num_names++] = name;
    strncpy(name->long_name, longname, sizeof(name->long_name) - 1);
    strncpy(name->short_name, shortname, sizeof(name->short_name) - 1);
}

/**
 * @return          The number of arguments on the command line.
 */
int Argc(void)
{
    return num_args;
}

/**
 * @return          Pointer to the i'th argument.
 */
char *Argv(int i)
{
    if(i < 0 || i >= num_args)
        Con_Error("Argv: There is no arg %i.\n", i);
    return args[i];
}

/**
 * @return          Pointer to the i'th argument's pointer.
 */
char **ArgvPtr(int i)
{
    if(i < 0 || i >= num_args)
        Con_Error("ArgvPtr: There is no arg %i.\n", i);
    return &args[i];
}

char *ArgNext(void)
{
    if(!last_match || last_match >= Argc() - 1)
        return NULL;
    return args[++last_match];
}

/**
 * @return          @c true, if the two parameters are equivalent
 *                  according to the abbreviations.
 */
int ArgRecognize(char *first, char *second)
{
    int     k;
    boolean match;

    // A simple match?
    if(!stricmp(first, second))
        return true;

    match = false;
    k = 0;
    while(k < num_names && !match)
    {
        if(!stricmp(first, names[k]->long_name))
        {
            // names[k] now matches the first string.
            if(!stricmp(names[k]->short_name, second))
                match = true;
        }
        k++;
    }

    return match;
}

/**
 * Checks for the given parameter in the program's command line arguments.
 *
 * @return          The argument number (1 to argc-1) or 0 if not present.
 */
int ArgCheck(char *check)
{
    int     i;
    boolean isDone;

    last_match = 0;
    i = 1;
    isDone = false;
    while(i < num_args && !isDone)
    {
        // Check the short names for this arg, too.
        if(ArgRecognize(check, args[i]))
        {
            last_match = i;
            isDone = true;
        }
        else
            i++;
    }

    return last_match;
}

/**
 * Checks for the given parameter in the program's command line arguments
 * and it is followed by N other arguments.
 *
 * @return          The argument number (1 to argc-1) or 0 if not present.
 */
int ArgCheckWith(char *check, int num)
{
    int     i = ArgCheck(check);

    if(!i || i + num >= num_args)
        return 0;
    return i;
}

/**
 * @return          @c true, if the given argument begins with a hyphen.
 */
int ArgIsOption(int i)
{
    if(i < 0 || i >= num_args)
        return false;
    return args[i][0] == '-';
}

/**
 * Determines if an argument exists on the command line.
 *
 * @return  Number of times the argument exists on the command line.
 */
int ArgExists(char *check)
{
    int         i;
    int         count;

    for(i = 1, count = 0; i < Argc(); ++i)
        if(ArgRecognize(check, Argv(i)))
            count++;

    return count;
}
