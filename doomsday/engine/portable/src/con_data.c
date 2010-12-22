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

/**
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

D_CMD(HelpWhat);
D_CMD(ListAliases);
D_CMD(ListCmds);
D_CMD(ListVars);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int C_DECL wordListSorter(const void* e1, const void* e2);
static int C_DECL knownWordListSorter(const void* e1, const void* e2);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

static cvar_t* cvars = NULL;
static ddccmd_t* ccmds = NULL;
static calias_t* caliases = NULL;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint numCVars = 0, maxCVars = 0;
static uint numCCmds = 0, maxCCmds = 0;
static uint numCAliases = 0;

// The list of known words (for completion).
static knownword_t* knownWords = NULL;
static uint numKnownWords = 0;

// CODE --------------------------------------------------------------------

void Con_DataRegister(void)
{
    C_CMD("help",           "s",    HelpWhat);
    C_CMD("listaliases",    NULL,   ListAliases);
    C_CMD("listcmds",       NULL,   ListCmds);
    C_CMD("listvars",       NULL,   ListVars);
}

void Con_SetString(const char* name, char* text, byte overRide)
{
    cvar_t*             cvar = Con_GetVariable(name);
    boolean             changed = false;

    if(!cvar)
        return;

    if(cvar->flags & CVF_READ_ONLY && !overRide)
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
 * \note: Also works with bytes.
 */
void Con_SetInteger(const char* name, int value, byte overRide)
{
    cvar_t*             var = Con_GetVariable(name);
    boolean             changed = false;

    if(!var)
        return;

    if(var->flags & CVF_READ_ONLY && !overRide)
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
        if(CV_BYTE(var) != (byte) value)
            changed = true;

        CV_BYTE(var) = (byte) value;
    }
    if(var->type == CVT_FLOAT)
    {
        if(CV_FLOAT(var) != (float) value)
            changed = true;

        CV_FLOAT(var) = (float) value;
    }

    // Make the change notification callback
    if(var->notifyChanged != NULL && changed)
        var->notifyChanged(var);
}

void Con_SetFloat(const char* name, float value, byte overRide)
{
    cvar_t*             var = Con_GetVariable(name);
    boolean             changed = false;

    if(!var)
        return;

    if(var->flags & CVF_READ_ONLY && !overRide)
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

int Con_GetInteger(const char* name)
{
    cvar_t*             var = Con_GetVariable(name);

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

float Con_GetFloat(const char* name)
{
    cvar_t*             var = Con_GetVariable(name);

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

byte Con_GetByte(const char* name)
{
    cvar_t*             var = Con_GetVariable(name);

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

char* Con_GetString(const char* name)
{
    cvar_t*             var = Con_GetVariable(name);

    if(!var || var->type != CVT_CHARPTR)
        return "";
    return CV_CHARPTR(var);
}

void Con_AddVariableList(cvar_t* varlist)
{
    if(!varlist)
        return;

    for(; varlist->name; varlist++)
        Con_AddVariable(varlist);
}

void Con_AddVariable(cvar_t* var)
{
    char* nameCopy = NULL;
    
    if(!var)
        return;

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
    
    // Make a static copy of the variable name in the zone.
    // This allows the source data to change (in case of dynamic registrations).
    nameCopy = Z_Malloc(strlen(var->name) + 1, PU_APPSTATIC, NULL);
    strcpy(nameCopy, var->name);
    cvars[numCVars - 1].name = nameCopy;

    // Sort them.
    qsort(cvars, numCVars, sizeof(cvar_t), wordListSorter);
}

cvar_t* Con_GetVariable(const char* name)
{
    int                 result;
    uint                bottomIdx, topIdx, pivot;
    cvar_t*             var;
    boolean             isDone;

    if(numCVars == 0)
        return NULL;
    if(!name || !name[0])
        return NULL;

    bottomIdx = 0;
    topIdx = numCVars-1;
    var = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = stricmp(cvars[pivot].name, name);
        if(result == 0)
        {   // Found.
            var = &cvars[pivot];
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {   // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return var;
}

cvar_t* Con_GetVariableIDX(uint idx)
{
    if(idx < numCVars)
        return &cvars[idx];

    return NULL;
}

uint Con_CVarCount(void)
{
    return numCVars;
}

void Con_PrintCVar(cvar_t* var, char* prefix)
{
    char                equals = '=';

    if(!var)
        return;

    if(var->flags & CVF_PROTECTED || var->flags & CVF_READ_ONLY)
        equals = ':';

    if(prefix)
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

void Con_AddCommandList(ccmd_t* cmdlist)
{
    if(!cmdlist)
        return;

    for(; cmdlist->name; cmdlist++)
        Con_AddCommand(cmdlist);
}

void Con_AddCommand(ccmd_t* cmd)
{
    uint                i;
    cvartype_t          params[MAX_ARGS];
    int                 minArgs, maxArgs;
    ddccmd_t*           newCmd;

    if(!cmd)
        return;

    if(!cmd->name)
        Con_Error("Con_AddCommand: CCmd missing a name.");

/*#if _DEBUG
Con_Message("Con_AddCommand: \"%s\" \"%s\" (%i).\n", cmd->name,
            cmd->params, cmd->flags);
#endif*/

    // Decode the usage string if present.
    if(cmd->params != NULL)
    {
        char                c;
        size_t              l, len;
        cvartype_t          type = CVT_NULL;
        boolean             unlimitedArgs;

        len = strlen(cmd->params);
        minArgs = 0;
        unlimitedArgs = false;
        for(l = 0; l < len; ++l)
        {
            c = cmd->params[l];
            switch(c)
            {
            // Supported parameter type symbols:
            case 'b': type = CVT_BYTE;     break;
            case 'i': type = CVT_INT;      break;
            case 'f': type = CVT_FLOAT;    break;
            case 's': type = CVT_CHARPTR;  break;

            // Special symbols:
            case '*':
                // Variable arg list.
                if(l != len-1)
                    Con_Error("Con_AddCommand: CCmd '%s': '*' character "
                              "not last in parameter string: \"%s\".",
                              cmd->name, cmd->params);

                unlimitedArgs = true;
                type = CVT_NULL; // not a real parameter.
                break;

            // Erroneous symbol:
            default:
                Con_Error("Con_AddCommand: CCmd '%s': Invalid character "
                          "'%c' in parameter string: \"%s\".", cmd->name, c,
                          cmd->params);
            }

            if(type != CVT_NULL)
            {
                if(minArgs >= MAX_ARGS)
                    Con_Error("Con_AddCommand: CCmd '%s': Too many parameters. "
                              "Limit is %i.", cmd->name, MAX_ARGS);

                // Copy the parameter type into the buffer.
                params[minArgs++] = type;
            }
        }

        // Set the min/max parameter counts for this ccmd.
        if(unlimitedArgs)
        {
            maxArgs = -1;
            if(minArgs == 0)
                minArgs = -1;
        }
        else
            maxArgs = minArgs;

/*#if _DEBUG
{
int i;
Con_Message("Con_AddCommand: CCmd \"%s\": minArgs %i, maxArgs %i: \"",
            cmd->name, minArgs, maxArgs);
for(i = 0; i < minArgs; ++i)
{
    switch(params[i])
    {
    case CVT_BYTE:    c = 'b'; break;
    case CVT_INT:     c = 'i'; break;
    case CVT_FLOAT:   c = 'f'; break;
    case CVT_CHARPTR: c = 's'; break;
    }
    Con_Printf("%c", c);
}
Con_Printf("\".\n");
}
#endif*/
    }
    else // it's usage is NOT validated by Doomsday.
    {
        minArgs = maxArgs = -1;

/*#if _DEBUG
if(cmd->params == NULL)
  Con_Message("Con_AddCommand: CCmd \"%s\" will not have it's usage "
              "validated.\n", cmd->name);
#endif*/
    }

    // Now check that the ccmd to be registered is unique.
    // We allow multiple ccmds with the same name if we can determine by
    // their paramater lists that they are unique (overloading).
    for(i = 0; i < numCCmds; ++i)
    {
        ddccmd_t*           other = &ccmds[i];

        if(!stricmp(other->name, cmd->name))
        {
            boolean unique = true;

            // The ccmd being registered is NOT a deng validated ccmd
            // and there is already an existing ccmd by this name?
            if(minArgs == -1 && maxArgs == -1)
                unique = false;
            // An existing ccmd with no validation?
            else if(other->minArgs == -1 && other->maxArgs == -1)
                unique = false;
            // An existing ccmd with a lower minimum and no maximum?
            else if(other->minArgs < minArgs && other->maxArgs == -1)
                unique = false;
            // An existing ccmd with a larger min and this ccmd has no max?
            else if(other->minArgs > minArgs && maxArgs == -1)
                unique = false;
            // An existing ccmd with the same minimum number of args?
            else if(other->minArgs == minArgs)
            {
                // \todo Implement support for paramater type checking.
                unique = false;
            }

            if(!unique)
                Con_Error("Con_AddCommand: A CCmd by the name '%s' "
                          "is already registered. Their parameter lists "
                          "would be ambiguant.", cmd->name);

            // Sanity check.
            if(other->func == cmd->func)
                Con_Error("Con_AddCommand: A CCmd by the name '%s' is "
                          "already registered and the callback funcs are "
                          "the same, is this really what you wanted?",
                          cmd->name);
        }
    }

    // Have we ran out of available ddcmd_t's?
    if(++numCCmds > maxCCmds)
    {   // Allocate some more.
        maxCCmds *= 2;
        if(maxCCmds < numCCmds)
            maxCCmds = numCCmds;
        ccmds = M_Realloc(ccmds, sizeof(ddccmd_t) * maxCCmds);
    }
    newCmd = &ccmds[numCCmds - 1];

    // Copy the properties.
    newCmd->name   = cmd->name;
    newCmd->func   = cmd->func;
    newCmd->flags  = cmd->flags;
    newCmd->minArgs = minArgs;
    newCmd->maxArgs = maxArgs;
    memcpy(newCmd->params, &params, sizeof(newCmd->params));

    // Sort them.
    qsort(ccmds, numCCmds, sizeof(ddccmd_t), wordListSorter);
}

ddccmd_t* Con_GetCommand(cmdargs_t* args)
{
    uint                i;
    ddccmd_t*           ccmd = NULL;
    ddccmd_t*           matchingNameCmd = NULL;

    if(!args)
        return NULL;

    // \todo Use a faster than O(n) linear search. Note, ccmd->name is not
    //       a unique key (ccmds can share names if params differ).
    for(i = 0; i < numCCmds; ++i)
    {
        ccmd = &ccmds[i];

        if(!stricmp(args->argv[0], ccmd->name))
        {
            boolean             invalidArgs = false;

            // Remember the first one with a matching name.
            if(!matchingNameCmd)
                matchingNameCmd = ccmd;

            // Are we validating the arguments?
            if(!(ccmd->minArgs == -1 && ccmd->maxArgs == -1))
            {
                int                 j;

                // Do we have the right number of arguments?
                if(args->argc-1 < ccmd->minArgs)
                    invalidArgs = true;
                else if(ccmd->maxArgs != -1 && args->argc-1 > ccmd->maxArgs)
                    invalidArgs = true;
                else
                {
                    // We can only validate the minimum number of arguments,
                    // currently. We cannot yet validate non-required args.
                    for(j = 0; j < ccmd->minArgs && !invalidArgs; ++j)
                    {
                        if(ccmd->params[j] == CVT_BYTE)
                            invalidArgs = !M_IsStringValidByte(args->argv[j+1]);
                        else if(ccmd->params[j] == CVT_INT)
                            invalidArgs = !M_IsStringValidInt(args->argv[j+1]);
                        else if(ccmd->params[j] == CVT_FLOAT)
                            invalidArgs = !M_IsStringValidFloat(args->argv[j+1]);
                        // Strings are considered always valid.
                    }
                }
            }

            if(!invalidArgs)
                return ccmd; // This is the one!
        }
    }

    if(matchingNameCmd)
    {
        // We did find a command, perhaps the user needs some help.
        Con_PrintCCmdUsage(matchingNameCmd, false);
    }

    // No command found, or none with matching arguments.
    return NULL;
}

/**
 * @return              @c true, if the given string is a
 *                      valid command or alias name.
 */
boolean Con_IsValidCommand(const char* name)
{
    uint                i = 0;
    boolean             found = false;

    if(!name || !name[0])
        return false;

    // Try the console commands first.
    while(!found && i < numCCmds)
    {
        if(!stricmp(ccmds[i].name, name))
            found = true;
        else
            i++;
    }

    if(found)
    {
        return true;
    }
    else // Try the aliases (aliai?) then.
    {
        return (Con_GetAlias(name) != NULL);
    }
}

/**
 * Outputs the usage information for the given ccmd to the console if the
 * ccmd's usage is validated by Doomsday.
 *
 * @param ccmd          Ptr to the ccmd to print the usage info for.
 * @param showExtra     If @c true, print any additional info we
 *                      have about the ccmd.
 */
void Con_PrintCCmdUsage(ddccmd_t* ccmd, boolean showExtra)
{
    int                 i;
    char*               str;
    void*               ccmd_help;

    if(!ccmd || (ccmd->minArgs == -1 && ccmd->maxArgs == -1))
        return;

    ccmd_help = DH_Find(ccmd->name);

    // Print the expected form for this ccmd.
    Con_Printf("Usage:  %s", ccmd->name);
    for(i = 0; i < ccmd->minArgs; ++i)
    {
        switch(ccmd->params[i])
        {
        case CVT_BYTE:      Con_Printf(" (byte)");      break;
        case CVT_INT:       Con_Printf(" (int)");       break;
        case CVT_FLOAT:     Con_Printf(" (float)");     break;
        case CVT_CHARPTR:   Con_Printf(" (string)");    break;
        default:
            break;
        }
    }
    if(ccmd->maxArgs == -1)
        Con_Printf(" ...");
    Con_Printf("\n");

    if(showExtra)
    {
        // Check for extra info about this ccmd's usage.
        if((str = DH_GetString(ccmd_help, HST_INFO)))
            Con_Printf("%s\n", str);
    }
}

/**
 * Returns NULL if the specified alias can't be found.
 */
calias_t* Con_GetAlias(const char* name)
{
    int                 result;
    uint                bottomIdx, topIdx, pivot;
    calias_t*           cal;
    boolean             isDone;

    if(numCAliases == 0)
        return NULL;
    if(!name || !name[0])
        return NULL;

    bottomIdx = 0;
    topIdx = numCAliases-1;
    cal = NULL;
    isDone = false;
    while(bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = stricmp(caliases[pivot].name, name);
        if(result == 0)
        {
            // Found.
            cal = &caliases[pivot];
            isDone = true;
        }
        else
        {
            if(result > 0)
            {
                if(pivot == 0)
                {
                    // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return cal;
}

calias_t* Con_AddAlias(const char* aName, const char* command)
{
    calias_t*           cal;

    if(!aName || !aName[0] || !command || !command[0])
        return NULL;

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

void Con_DeleteAlias(calias_t* cal)
{
    uint                idx;

    if(!cal)
        return;

    idx = cal - caliases;

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
void Con_WriteAliasesToFile(FILE* file)
{
    uint                i;
    calias_t*           cal;

    if(!file)
        return;

    for(i = 0, cal = caliases; i < numCAliases; ++i, cal++)
    {
        fprintf(file, "alias \"");
        M_WriteTextEsc(file, cal->name);
        fprintf(file, "\" \"");
        M_WriteTextEsc(file, cal->command);
        fprintf(file, "\"\n");
    }
}

static int C_DECL wordListSorter(const void* e1, const void* e2)
{
    return stricmp(*(char**)e1, *(char**)e2);
}

static int C_DECL knownWordListSorter(const void* e1, const void* e2)
{
    return stricmp(((knownword_t*) e1)->word, ((knownword_t*) e2)->word);
}

/**
 * \note: Variables with CVF_HIDE are not considered known words.
 */
void Con_UpdateKnownWords(void)
{
    uint                i, c, knownVars;
    size_t              len;

    // Count the number of visible console variables.
    for(i = knownVars = 0; i < numCVars; ++i)
        if(!(cvars[i].flags & CVF_HIDE))
            knownVars++;

    // Fill the known words table.
    numKnownWords = numCCmds + knownVars + numCAliases /*+ numBindClasses*/;
    len = sizeof(knownword_t) * numKnownWords;
    knownWords = M_Realloc(knownWords, len);
    memset(knownWords, 0, len);

    // Commands, variables, aliases, and bind class names are known words.
    for(i = 0, c = 0; i < numCCmds; ++i, ++c)
    {
        strncpy(knownWords[c].word, ccmds[i].name, 63);
        knownWords[c].type = WT_CCMD;
    }
    for(i = 0; i < knownVars; ++i)
    {
        if(!(cvars[i].flags & CVF_HIDE))
        {
            strncpy(knownWords[c].word, cvars[i].name, 63);
            knownWords[c].type = WT_CVAR;
            c++;
        }
    }
    for(i = 0; i < numCAliases; ++i, ++c)
    {
        strncpy(knownWords[c].word, caliases[i].name, 63);
        knownWords[c].type = WT_ALIAS;
    }
    // TODO: Add bind context names to the known words.
    /*
    for(i = 0; i < numBindContexts; ++i, ++c)
    {
        strncpy(knownWords[c].word, bindContexts[i].name, 63);
        knownWords[c].type = WT_BINDCONTEXT;
    }
     */

    // Sort it so we get nice alphabetical word completions.
    qsort(knownWords, numKnownWords, sizeof(knownword_t), knownWordListSorter);
}

/**
 * Collect an array of knownWords which match the given word (at least
 * partially).
 *
 * \note: The array must be freed with M_Free.
 *
 * @param word          The word to be matched.
 * @param count         The count of the number of matching words will be
 *                      written back to this location if NOT @c NULL.
 *
 * @return              A NULL-terminated array of pointers to all the known
 *                      words which match (at least partially) @param word
 */
knownword_t** Con_CollectKnownWordsMatchingWord(const char* word, uint* count)
{
    uint                i, num = 0, num2 = 0;
    size_t              wordLength;
    knownword_t**       matches;

    if(!word || !word[0])
        return NULL;

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
    matches = M_Malloc(sizeof(knownword_t *) * (num + 1));

    // Collect the pointers.
    i = 0;
    num2 = 0;
    do
    {
        if(!strnicmp(knownWords[i].word, word, wordLength))
            matches[num2++] = &knownWords[i];

        i++;
    }
    while(num2 < num);

    // Terminate.
    matches[num] = NULL;

    return matches;
}

void Con_DestroyDatabases(void)
{
    // Free the data of the data cvars.
    { uint i;
    for(i = 0; i < numCVars; ++i)
    {
        if(cvars[i].type != CVT_CHARPTR)
            continue;

        if(cvars[i].flags & CVF_CAN_FREE)
        {
            char** ptr = (char**) cvars[i].ptr;

            // Multiple vars could be using the same pointer,
            // make sure it gets freed only once.
            { uint k;
            for(k = i; k < numCVars; ++k)
                if(cvars[k].type == CVT_CHARPTR && *ptr == *(char**) cvars[k].ptr)
                {
                    cvars[k].flags &= ~CVF_CAN_FREE;
                }
            }
            M_Free(*ptr);
            *ptr = "";
        }
    }}

    if(cvars)
        M_Free(cvars);
    cvars = NULL;
    numCVars = maxCVars = 0;

    if(ccmds)
        M_Free(ccmds);
    ccmds = NULL;
    numCCmds = maxCCmds = 0;

    if(knownWords)
        M_Free(knownWords);
    knownWords = NULL;
    numKnownWords = 0;

    if(caliases)
    {
        // Free the alias data.
        uint i;
        for(i = 0; i < numCAliases; ++i)
        {
            M_Free(caliases[i].command);
            M_Free(caliases[i].name);
        }
        M_Free(caliases);
    }
    caliases = NULL;
    numCAliases = 0;
}

D_CMD(HelpWhat)
{
    char*               str;
    void*               help;
    ddccmd_t*           ccmd;
    cvar_t*             cvar;
    uint                i, found = 0;

    if(!stricmp(argv[1], "(what)"))
    {
        Con_Printf("You've got to be kidding!\n");
        return true;
    }

    // Try the console commands first.
    for(i = 0; i < numCCmds; ++i)
    {
        ccmd = &ccmds[i];
        if(!stricmp(argv[1], ccmd->name))
        {
            if(found == 0) // only print a description once.
            {
                help = DH_Find(ccmd->name);
                if((str = DH_GetString(help, HST_DESCRIPTION)))
                    Con_Printf("%s\n", str);
            }
            Con_PrintCCmdUsage(ccmd, (found == 0));
            found++; // found one, but there may be more...
        }
    }

    if(found == 0) // Perhaps its a cvar then?
    {
        cvar = Con_GetVariable(argv[1]);
        if(cvar != NULL)
        {
            help = DH_Find(cvar->name);
            if((str = DH_GetString(help, HST_DESCRIPTION)))
            {
                Con_Printf("%s\n", str);
                found = true;
            }
        }
    }

    if(found == 0) // Still not found?
        Con_Printf("There's no help about '%s'.\n", argv[1]);

    return true;
}

D_CMD(ListCmds)
{
    uint                i;
    char*               str;
    void*               ccmd_help;
    size_t              length = 0;

    if(argc > 1)
        length = strlen(argv[1]);

    Con_Printf("Console commands:\n");
    for(i = 0; i < numCCmds; ++i)
    {
        // Is there a filter?
        if(!(argc > 1 && strnicmp(ccmds[i].name, argv[1], length)))
        {
            ccmd_help = DH_Find(ccmds[i].name);
            if((str = DH_GetString(ccmd_help, HST_DESCRIPTION)))
                Con_FPrintf( CBLF_LIGHT | CBLF_YELLOW, "  %s (%s)\n", ccmds[i].name, str);
            else
                Con_FPrintf( CBLF_LIGHT | CBLF_YELLOW, "  %s\n", ccmds[i].name);
        }
    }
    return true;
}

D_CMD(ListVars)
{
    uint                i;
    size_t              length = 0;

    if(argc > 1)
        length = strlen(argv[1]);

    Con_Printf("Console variables:\n");
    for(i = 0; i < numCVars; ++i)
    {
        if(!(cvars[i].flags & CVF_HIDE))
        {
            // Is there a filter?
            if(!(argc > 1 && strnicmp(cvars[i].name, argv[1], length)))
                Con_PrintCVar(cvars + i, "  ");
        }
    }
    return true;
}

D_CMD(ListAliases)
{
    uint                i;
    size_t              length = 0;

    if(argc > 1)
        length = strlen(argv[1]);

    Con_Printf("Aliases:\n");
    for(i = 0; i < numCAliases; ++i)
    {
        // Is there a filter?
        if(!(argc > 1 && strnicmp(caliases[i].name, argv[1], length)))
            Con_FPrintf(CBLF_LIGHT|CBLF_YELLOW, "  %s == %s\n", caliases[i].name,
                         caliases[i].command);
    }
    return true;
}
