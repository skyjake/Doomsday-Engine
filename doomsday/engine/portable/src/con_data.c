/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 * con_data.c: Console Subsystem
 *
 * Console databases for knownwords, cvars, ccmds and aliases.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int C_DECL wordListSorter(const void *e1, const void *e2);
static int C_DECL knownWordListSorter(const void *e1, const void *e2);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bindclass_t *bindClasses;
extern int numBindClasses;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

cvar_t *cvars = NULL;
int numCVars = 0; // accessed in con_config.c
ccmd_t *ccmds = NULL;
calias_t *caliases = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int maxCVars;
static int numCCmds = 0, maxCCmds;
static int numCAliases = 0;

// The list of known words (for completion).
static knownword_t *knownWords = NULL;
static unsigned int numKnownWords = 0;

// CODE --------------------------------------------------------------------

void Con_SetString(const char *name, char *text, byte override)
{
    cvar_t *cvar = Con_GetVariable(name);
    boolean changed = false;

    if(!cvar)
        return;

    if(cvar->flags & CVF_READ_ONLY && !override)
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", name);
        return;
    }

    if(cvar->type == CVT_CHARPTR)
    {
        if(CV_CHARPTR(cvar) && !stricmp(text, CV_CHARPTR(cvar)))
            changed = true;

        // Free the old string, if one exists.
        if(cvar->flags & CVF_CAN_FREE && CV_CHARPTR(cvar))
            M_Free(CV_CHARPTR(cvar));
        // Allocate a new string.
        cvar->flags |= CVF_CAN_FREE;
        CV_CHARPTR(cvar) = M_Malloc(strlen(text) + 1);
        strcpy(CV_CHARPTR(cvar), text);

        // Make the change notification callback
        if(cvar->notifyChanged != NULL && changed)
            cvar->notifyChanged(cvar);
    }
    else
        Con_Error("Con_SetString: cvar is not of type char*.\n");
}

/**
 * NOTE: Also works with bytes.
 */
void Con_SetInteger(const char *name, int value, byte override)
{
    cvar_t *var = Con_GetVariable(name);
    boolean changed = false;

    if(!var)
        return;

    if(var->flags & CVF_READ_ONLY && !override)
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", name);
        return;
    }

    if(var->type == CVT_INT)
    {
        if(CV_INT(var) != value)
            changed = true;

        CV_INT(var) = value;
    }
    if(var->type == CVT_BYTE)
    {
        if(CV_BYTE(var) != value)
            changed = true;

        CV_BYTE(var) = value;
    }
    if(var->type == CVT_FLOAT)
    {
        if(CV_FLOAT(var) != value)
            changed = true;

        CV_FLOAT(var) = value;
    }

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged(var);
}

void Con_SetFloat(const char *name, float value, byte override)
{
    cvar_t *var = Con_GetVariable(name);
    boolean changed = false;

    if(!var)
        return;

    if(var->flags & CVF_READ_ONLY && !override)
    {
        Con_Printf("%s (cvar) is read-only. It can't be changed "
                   "(not even with force)\n", name);
        return;
    }

    if(var->type == CVT_INT)
    {
        if(CV_INT(var) != (int) value)
            changed = true;

        CV_INT(var) = (int) value;
    }
    if(var->type == CVT_BYTE)
    {
        if(CV_BYTE(var) != (byte) value)
            changed = true;

        CV_BYTE(var) = (byte) value;
    }
    if(var->type == CVT_FLOAT)
    {
        if(CV_FLOAT(var) != value)
            changed = true;

        CV_FLOAT(var) = value;
    }

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged(var);
}

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

char *Con_GetString(const char *name)
{
    cvar_t *var = Con_GetVariable(name);

    if(!var || var->type != CVT_CHARPTR)
        return "";
    return CV_CHARPTR(var);
}

void Con_AddVariableList(cvar_t *varlist)
{
    for(; varlist->name; varlist++)
        Con_AddVariable(varlist);
}

void Con_AddVariable(cvar_t *var)
{
    if(Con_GetVariable(var->name))
        Con_Error("Con_AddVariable: A CVAR by the name \"%s\" "
                  "is already registered", var->name);

    if(++numCVars > maxCVars)
    {
        // Allocate more memory.
        maxCVars *= 2;
        if(maxCVars < numCVars)
            maxCVars = numCVars;
        cvars = M_Realloc(cvars, sizeof(cvar_t) * maxCVars);
    }
    memcpy(cvars + numCVars - 1, var, sizeof(cvar_t));

    // Sort them.
    qsort(cvars, numCVars, sizeof(cvar_t), wordListSorter);
}

cvar_t *Con_GetVariable(const char *name)
{
    int     result;
    int     bottomIdx, topIdx, pivot;
    cvar_t *var;

    bottomIdx = 0;
    topIdx = numCVars-1;

    while(bottomIdx <= topIdx)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;
        var = &cvars[pivot];

        result = stricmp(var->name, name);
        if(result == 0)
            return var;
        else if(result > 0)
            topIdx = pivot - 1;
        else
            bottomIdx = pivot + 1;
    }

    // No match...
    return NULL;
}

void Con_PrintCVar(cvar_t *var, char *prefix)
{
    char    equals = '=';

    if(var->flags & CVF_PROTECTED || var->flags & CVF_READ_ONLY)
        equals = ':';

    Con_Printf("%s", prefix);
    switch(var->type)
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

void Con_AddCommandList(ccmd_t *cmdlist)
{
    for(; cmdlist->name; cmdlist++)
        Con_AddCommand(cmdlist);
}

void Con_AddCommand(ccmd_t *cmd)
{
    if(Con_GetCommand(cmd->name))
        Con_Error("Con_AddCommand: A CCmd by the name \"%s\" "
                  "is already registered", cmd->name);

    if(++numCCmds > maxCCmds)
    {
        maxCCmds *= 2;
        if(maxCCmds < numCCmds)
            maxCCmds = numCCmds;
        ccmds = M_Realloc(ccmds, sizeof(ccmd_t) * maxCCmds);
    }
    memcpy(ccmds + numCCmds - 1, cmd, sizeof(ccmd_t));

    // Sort them.
    qsort(ccmds, numCCmds, sizeof(ccmd_t), wordListSorter);
}

/**
 * Returns a pointer to the ccmd_t with the specified name, or NULL.
 */
ccmd_t *Con_GetCommand(const char *name)
{
    int     result;
    int     bottomIdx, topIdx, pivot;
    ccmd_t *cmd;

    bottomIdx = 0;
    topIdx = numCCmds-1;

    while(bottomIdx <= topIdx)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;
        cmd = &ccmds[pivot];

        result = stricmp(cmd->name, name);
        if(result == 0)
            return cmd;
        else if(result > 0)
            topIdx = pivot - 1;
        else
            bottomIdx = pivot + 1;
    }

    // No match...
    return NULL;
}

/**
 * Returns true if the given string is a valid command or alias.
 */
boolean Con_IsValidCommand(const char *name)
{
    return Con_GetCommand(name) || Con_GetAlias(name);
}

/**
 * Returns NULL if the specified alias can't be found.
 */
calias_t *Con_GetAlias(const char *name)
{
    int     result;
    int     bottomIdx, topIdx, pivot;
    calias_t *cal;

    bottomIdx = 0;
    topIdx = numCAliases-1;

    while(bottomIdx <= topIdx)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;
        cal = &caliases[pivot];

        result = stricmp(cal->name, name);
        if(result == 0)
            return cal;
        else if(result > 0)
            topIdx = pivot - 1;
        else
            bottomIdx = pivot + 1;
    }

    // No match...
    return NULL;
}

calias_t *Con_AddAlias(char *aName, char *command)
{
    calias_t *cal;

    caliases = M_Realloc(caliases, sizeof(calias_t) * (++numCAliases));
    cal = caliases + numCAliases - 1;

    // Allocate memory for them.
    cal->name = M_Malloc(strlen(aName) + 1);
    cal->command = M_Malloc(strlen(command) + 1);
    strcpy(cal->name, aName);
    strcpy(cal->command, command);

    // Sort them.
    qsort(caliases, numCAliases, sizeof(calias_t), wordListSorter);
    return cal;
}

void Con_DeleteAlias(calias_t *cal)
{
    int idx = cal - caliases;

    M_Free(cal->name);
    M_Free(cal->command);

    if(idx < numCAliases - 1)
        memmove(caliases + idx, caliases + idx + 1,
                sizeof(calias_t) * (numCAliases - idx - 1));

    caliases = M_Realloc(caliases, sizeof(calias_t) * --numCAliases);
}

/**
 * Called by the config file writer.
 */
void Con_WriteAliasesToFile(FILE * file)
{
    int     i;
    calias_t *cal;

    for(i = 0, cal = caliases; i < numCAliases; ++i, cal++)
    {
        fprintf(file, "alias \"");
        M_WriteTextEsc(file, cal->name);
        fprintf(file, "\" \"");
        M_WriteTextEsc(file, cal->command);
        fprintf(file, "\"\n");
    }
}

static int C_DECL wordListSorter(const void *e1, const void *e2)
{
    return stricmp(*(char **) e1, *(char **) e2);
}

static int C_DECL knownWordListSorter(const void *e1, const void *e2)
{
    return stricmp(((knownword_t *) e1)->word, ((knownword_t *) e2)->word);
}

/**
 * NOTE: Variables with CVF_HIDE are not considered known words.
 */
void Con_UpdateKnownWords(void)
{
    int     i, c, known_vars;
    int     len;

    // Count the number of visible console variables.
    for(i = known_vars = 0; i < numCVars; ++i)
        if(!(cvars[i].flags & CVF_HIDE))
            known_vars++;

    // Fill the known words table.
    numKnownWords = numCCmds + known_vars + numCAliases + numBindClasses;
    knownWords = M_Realloc(knownWords, len =
                         sizeof(knownword_t) * numKnownWords);
    memset(knownWords, 0, len);

    // Commands, variables, aliases, and bind class names are known words.
    for(i = 0, c = 0; i < numCCmds; ++i, ++c)
    {
        strncpy(knownWords[c].word, ccmds[i].name, 63);
        knownWords[c].type = WT_CCMD;
    }
    for(i = 0; i < numCVars; ++i)
    {
        if(!(cvars[i].flags & CVF_HIDE))
            strncpy(knownWords[c++].word, cvars[i].name, 63);
        knownWords[c].type = WT_CVAR;
    }
    for(i = 0; i < numCAliases; ++i, ++c)
    {
        strncpy(knownWords[c].word, caliases[i].name, 63);
        knownWords[c].type = WT_ALIAS;
    }
    for(i = 0; i < numBindClasses; ++i, ++c)
    {
        strncpy(knownWords[c].word, bindClasses[i].name, 63);
        knownWords[c].type = WT_BINDCLASS;
    }

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), knownWordListSorter);
}

/**
 * Collect an array of knownWords which match the given word (at least
 * partially).
 *
 * NOTE: The array must be freed with M_Free.
 *
 * @param word      The word to be matched.
 * @param count     The count of the number of matching words will be
 *                  written back to this location if NOT <code>NULL</code>.
 *
 * @return          A NULL-terminated array of pointers to all the known
 *                  words which match (at least partially) @param word.
 */
knownword_t **Con_CollectKnownWordsMatchingWord(char *word,
                                                unsigned int *count)
{
    unsigned int i, num = 0, num2 = 0;
    unsigned int wordLength;
    knownword_t **array;

    wordLength = strlen(word);

    // Count the number of matching words.
    for(i = 0; i < numKnownWords; ++i)
    {
        if(!strnicmp(knownWords[i].word, word, wordLength))
            num++;
    }

    // Tell the caller how many we found.
    if(count)
        *count = num;

    if(num == 0)     // No matches.
        return NULL;

    // Allocate the array, plus one for the terminator.
    array = M_Malloc(sizeof(knownword_t *) * (num + 1));

    // Collect the pointers.
    for(i = 0, num2 = 0; i < numKnownWords; ++i)
    {
        if(!strnicmp(knownWords[i].word, word, wordLength))
            array[num2++] = &knownWords[i];

        if(num2 == num)
            break;
    }

    // Terminate.
    array[num] = NULL;

    return array;
}

void Con_DestroyDatabases(void)
{
    int     i, k;
    char   *ptr;

    // Free the data of the data cvars.
    for(i = 0; i < numCVars; ++i)
        if(cvars[i].flags & CVF_CAN_FREE && cvars[i].type == CVT_CHARPTR)
        {
            ptr = *(char **) cvars[i].ptr;

            // Multiple vars could be using the same pointer,
            // make sure it gets freed only once.
            for(k = i; k < numCVars; ++k)
                if(cvars[k].type == CVT_CHARPTR &&
                   ptr == *(char **) cvars[k].ptr)
                {
                    cvars[k].flags &= ~CVF_CAN_FREE;
                }
            M_Free(ptr);
        }
    M_Free(cvars);
    cvars = NULL;
    numCVars = 0;

    M_Free(ccmds);
    ccmds = NULL;
    numCCmds = 0;

    M_Free(knownWords);
    knownWords = NULL;
    numKnownWords = 0;

    // Free the alias data.
    for(i = 0; i < numCAliases; ++i)
    {
        M_Free(caliases[i].command);
        M_Free(caliases[i].name);
    }
    M_Free(caliases);
    caliases = NULL;
    numCAliases = 0;
}

D_CMD(ListCmds)
{
    int     i;
    char    *str;
    void *ccmd_help;

    Con_Printf("Console commands:\n");
    for(i = 0; i < numCCmds; ++i)
    {
        if(argc > 1)            // Is there a filter?
            if(strnicmp(ccmds[i].name, argv[1], strlen(argv[1])))
                continue;

        ccmd_help = DH_Find(ccmds[i].name);
        if((str = DH_GetString(ccmd_help, HST_DESCRIPTION)))
            Con_FPrintf( CBLF_LIGHT | CBLF_YELLOW, "  %s (%s)\n", ccmds[i].name, str);
        else
            Con_FPrintf( CBLF_LIGHT | CBLF_YELLOW, "  %s\n", ccmds[i].name);
    }
    return true;
}

D_CMD(ListVars)
{
    int     i;

    Con_Printf("Console variables:\n");
    for(i = 0; i < numCVars; ++i)
    {
        if(cvars[i].flags & CVF_HIDE)
            continue;
        if(argc > 1)            // Is there a filter?
            if(strnicmp(cvars[i].name, argv[1], strlen(argv[1])))
                continue;

        Con_PrintCVar(cvars + i, "  ");
    }
    return true;
}

D_CMD(ListAliases)
{
    int     i;

    Con_Printf("Aliases:\n");
    for(i = 0; i < numCAliases; ++i)
    {
        if(argc > 1)            // Is there a filter?
            if(strnicmp(caliases[i].name, argv[1], strlen(argv[1])))
                continue;

        Con_FPrintf( CBLF_LIGHT | CBLF_YELLOW, "  %s == %s\n", caliases[i].name,
                     caliases[i].command);
    }
    return true;
}
